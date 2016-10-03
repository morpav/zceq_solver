/* Copyright @ 2016 Pavel Moravec */
#include <algorithm>
#include <functional>
#include <chrono>

#include "zceq_solver.h"
#include "zceq_blake2b.h"

namespace zceq_solver {

void ExpandArrayFast(const u8* hash, u8* array);
void ReorderBitsInHash(const u8* __restrict hash,
                              u8* __restrict array);

Solver::Solver() : allocator_(Const::kMaximumStringSetSize,
                              // Manually found minimal values
                              (Const::kExpandHashes
                               ? Const::kMemoryForExpandedProblem
                               : Const::kMemoryForNonExpandedProblem),
                              Const::kReportMemoryAllocation) {
  ResetTimer();
  context_ = new Context();
}

Solver::~Solver() {
  delete context_;
}

void Solver::Reset(Inputs& inputs) {
  Reset(inputs.data, sizeof inputs.data);
}

void Solver::Reset(const u8* data, u64 length) {
  assert(data != nullptr);
  ResetTimer();
  ResetMemoryAllocator();
  ClearSolutions();

  blake.Precompute(data, length);
  initialized_ = true;
};

void Solver::ResetMemoryAllocator() {
  allocator_.Reset();
  space_X1 = allocator_.CreateSpace<GeneratedString>("X1", 0);
  space_X2 = allocator_.CreateSpace<GeneratedString>("X2", 0);

  // One link index for each reduced segment (all - 1).
  // One basic index for initial sort before first collision search.
  link_indices_.resize(Const::kTotalSegmentsCount);
  int name_number = 0;
  for (auto& link_index : link_indices_) {
    std::string index_name = "I";
    index_name += std::to_string(name_number++);
    link_index = allocator_.CreateSpace<PairLink>(index_name, 0);
  }
}

static u32 GetTestSegmentValue(int i, int s, Random& r) {
  if (i % Const::kTestSetExpandMultiplier == 0) {
    i /= Const::kTestSetExpandMultiplier;
    u32 group_size = (1u << s);
    u32 in_group_index = (i % group_size);
    u32 group_shift = (i / 2 / group_size) * group_size;
    return group_shift * (in_group_index == 0);
  } else {
    return r.Next() & Const::kHashSegmentBitMask;
  }
};

void Solver::GenerateOTString(u32 index, OneTimeString& result)
{
  constexpr i32 half_hash_length = Const::N_parameter / 8;
  alignas(32) State blake_result;
  blake.FinalizeInto(blake_result, index / 2);

  auto relevant_part = &blake_result.hash[(index % 2) * half_hash_length];

  // Expand into the given string from segment 0.
  static_assert(OneTimeString::has_expanded_hash, "");
  ExpandArrayFast(relevant_part, result.GetRawData(0));
  result.SetIndex(index);
}

void Solver::GenerateOTStringTest(u32 index, OneTimeString& result)
{
  Random r;
  result.SetIndex(index);
  for (auto segment : range(9)) {
    result.SetSegment(segment, GetTestSegmentValue(index, segment, r));
  }
  // 10th segment is same for all strings
  result.SetSegment(9, 0x77777777 & Const::kHashSegmentBitMask);
}

inline void GenerateHashR(Random& r, unsigned char* hash, size_t hLen) {
  auto target = (u64*)hash;
  while (target < (void*)(hash + hLen)) {
    *target++ = r.Next();
  }
}

static inline u64 ComputeBucketFromHash(void* hash_beginning) {
  return *(u32*)hash_beginning & Const::kBucketNumberMask;
}

Random r;
void Solver::GenerateXStringsTest(
    SpaceAllocator::Space* target_space, BucketIndices* buckets) {
//  r.InitializeState(2312044234393,30498329312);
  GeneratedString* output = target_space->As<GeneratedString>();

  OneTimeString temp;
  for (auto i : range(512 * Const::kTestSetExpandMultiplier)) {
    auto segment_0 = GetTestSegmentValue(i, 0, r);
    auto bucket = segment_0 & Const::kBucketNumberMask;
    auto position = buckets->counter[bucket]++;
    auto row = &output[position];
    row->SetIndex(i);
    row->SetSegment(0, segment_0);
    for (u32 segment = 1; segment < 9; segment++) {
      row->SetSegment(segment, GetTestSegmentValue(i, segment, r));
    }
    // 10th segment is same for all strings
    row->SetSegment(9, 0x77777777 & Const::kHashSegmentBitMask);
    if (i % Const::kTestSetExpandMultiplier == 0) {
      GenerateOTStringTest(i, temp);
      for (u32 segment : range(10u)) {
        assert(row->GetCleanSegment(segment) == temp.GetCleanSegment(segment));
      }
    }
  }
}

void Solver::GenerateXStrings(
    SpaceAllocator::Space* target_space, BucketIndices* buckets) {

  GeneratedString* output = target_space->As<GeneratedString>();

  constexpr i32 half_hash_length = Const::N_parameter / 8;
  alignas(32) State blake_result;
  u32 string_index = 0;
  for (u32 g = 0; g < (Const::kInitialStringSetSize / 2); ++g) {
    // Generate hash data for g-th string pair.
    blake.FinalizeInto(blake_result, g);

    auto bucket = ComputeBucketFromHash(blake_result.hash);
//    auto position = bucket * Const::kItemsInBucket + buckets->counter[bucket]++;
    auto position = buckets->counter[bucket]++;
    auto row = &output[position];
    row->SetIndex(string_index++);
    if (!GeneratedString::has_expanded_hash)
      ReorderBitsInHash(blake_result.hash, row->GetRawData(0));
    else
      ExpandArrayFast(blake_result.hash, row->GetRawData(0));

    bucket = ComputeBucketFromHash(blake_result.hash + half_hash_length);
//    position = bucket * Const::kItemsInBucket + buckets->counter[bucket]++;
    position = buckets->counter[bucket]++;
    row = &output[position];
    row->SetIndex(string_index++);
    if (!GeneratedString::has_expanded_hash)
      ReorderBitsInHash(blake_result.hash + half_hash_length, row->GetRawData(0));
    else
      ExpandArrayFast(blake_result.hash + half_hash_length, row->GetRawData(0));
  }
  assert(string_index == Const::kInitialStringSetSize);
  ReportStep("Generated strings (Blake2b)");
}

template<u32 batch_size>
void Solver::GenerateXStringsBatch(
    SpaceAllocator::Space* target_space, BucketIndices* buckets) {

  GeneratedString* output = target_space->As<GeneratedString>();
  // Double check everything works fine.
  assert(batch_size == blake.GetBatchSize());

  constexpr i32 half_hash_length = Const::N_parameter / 8;
  auto blake_result = blake.GetHashOutputMemory();
  u32 string_index = 0;
  for (u32 g = 0; g < (Const::kInitialStringSetSize / 2); g += batch_size) {

    // Generate hash data for g-th string pair.
    blake.BatchFinalize(g);

    for (auto i : range(batch_size)) {
      u8* hash8 = (u8*)&blake_result[i];

      auto bucket = ComputeBucketFromHash(hash8);
      auto position = buckets->counter[bucket]++;
      auto row = &output[position];
      row->SetIndex(string_index++);
      if (!GeneratedString::has_expanded_hash)
        ReorderBitsInHash(hash8, row->GetRawData(0));
      else
        ExpandArrayFast(hash8, row->GetRawData(0));

      bucket = ComputeBucketFromHash(hash8 + half_hash_length);
      position = buckets->counter[bucket]++;
      row = &output[position];
      row->SetIndex(string_index++);
      if (!GeneratedString::has_expanded_hash)
        ReorderBitsInHash(hash8 + half_hash_length, row->GetRawData(0));
      else
        ExpandArrayFast(hash8 + half_hash_length, row->GetRawData(0));
    }
  }
  assert(string_index == Const::kInitialStringSetSize);
  ReportStep("Generated strings (Blake2b)");
}

template<typename C, typename T>
bool ReductionStep<C,T>::PrepareRTConfiguration() {
  auto space = solver_.link_indices_[segments_reduced];
  target_pair_index_ = space->template As<PairLink>();

  // Set default limit
  out_strings_limit = (u32)solver_.template AvailableStringSlots<OutString>();

  in_strings_ = in_strings->As<InString>();
  out_strings_ = out_strings->As<OutString>();
  reordered_temp_strings_ = out_strings->As<InString>();
  return true;
};

template<typename C, typename S>
bool ReductionStep<C,S>::Execute(Context* context,
                                 BucketIndices* in_buckets,
                                 BucketIndices* out_buckets) noexcept {
  //solver_.allocator().DumpState();
  PrepareRTConfiguration();

  if (Const::kReportCollisions) {
    printf("\nExecuting step %d \n---------------------\n", InString::segments_reduced);
    collisions_.clear();
    collisions_.resize(Const::kTooManyBasicCollisions + 2);
  }

  auto hash = context->hash.data();
  auto count = context->count.data();
  auto cum_sum = context->cum_sum.data();
  auto collisions = context->collisions.data();

  out_buckets->Reset();

  if (Const::kCheckLinksConsistency)
    memset(target_pair_index_, 0xff, Const::kMaximumStringSetSize * sizeof *target_pair_index_);

//for (auto bucket : range(Const::kBucketCount)) {{
  for (u32 outer_partition : range(Const::kPartitionCount)) {
  for (u32 _bucket : range(Const::kBucketsPerPartition)) {
    const auto in_bucket = _bucket + outer_partition * Const::kBucketsPerPartition;
    auto base_index = in_bucket * Const::kItemsInBucket;
    const InString* const in_rows = &in_strings_[base_index];
    PairLink* pair_index = &target_pair_index_[base_index];
    assert(in_buckets->counter[in_bucket] >= base_index);
    assert(in_buckets->counter[in_bucket] - base_index <=
           Const::kItemsInBucket);

    memset(count, 0x00, Const::kHashTableSize * sizeof *count);

    int i = 0;
    int cnt;
    for (u32 inner_partition : range(Const::kPartitionCount)) {
      int actual_items = in_buckets->partition_sizes[in_bucket][inner_partition];
      auto ProcessOneRow = [&]() {
        auto idx = Const::kHashTableMask &
                   (in_rows[i].GetRawHash() >> Const::kBucketCountBits);
        hash[i] = idx;
        count[idx]++;
        if (!C::isFinal && !Const::kStoreIndicesEarly)
          OutputIndex(&pair_index[i], in_rows[i].GetLink());
        i++;
      };
      cnt = actual_items / 4;
      while (LIKELY(cnt--)) {
        ProcessOneRow();
        ProcessOneRow();
        ProcessOneRow();
        ProcessOneRow();
      }
      cnt = 0;
      while (cnt < (actual_items % 4)) {
        ProcessOneRow();
        cnt++;
      }
      // Move to the next input partition
      i += (Const::kItemsInOutPartition - actual_items);
    }
    // Compute cummulative sum, eliminate groups greater then Const::kTooManyBasicCollisions - 1
    // TODO: Start from 1. Value 0 is used for not-used key. Better solution??

    u16 sum = 1;
    i = 0;
    auto ProcessOneHash = [&]() {
      const auto count_i = count[i];
      const auto valid = (count_i >= 2 && count_i < Const::kTooManyBasicCollisions);
      cum_sum[i] = valid ? sum : (u16) 0;
      sum += valid ? count_i : 0;
      i++;
      if (Const::kReportCollisions) {
        if (count_i > Const::kTooManyBasicCollisions) {
          collisions_[Const::kTooManyBasicCollisions]++;
          collisions_[Const::kTooManyBasicCollisions + 1] += count_i;
        } else if (count_i >= 2) {
          collisions_[count_i]++;
        }
      }
    };
    static_assert(Const::kHashTableSize % 8 == 0, "");
    cnt = Const::kHashTableSize / 4;
    // The compiler should probably do the unroll itself, but it sometimes
    // does not.
    while (LIKELY(cnt--)) {
      ProcessOneHash();
      ProcessOneHash();
      ProcessOneHash();
      ProcessOneHash();
    }

    i = 0;
    for (u32 inner_partition : range(Const::kPartitionCount)) {
      int actual_items = in_buckets->partition_sizes[in_bucket][inner_partition];
      auto FillOneItem = [&]() {
        //       const auto idx = Const::kHashTableMask & (in_rows[i].GetHash() >> Const::kBucketCountBits);
        auto idx = hash[i];
        // Use branch-less version of code. We always rewrite collisions[0]
        // when the i-th string is not part of any valid collision
        // (lookup[hash[i]] == 0). But the collisions[0] is therefore always hot
        // (in L1) so it is not an issue and it allows to increment without branch
        // (probably a conditional move is generated by a compiler).
        collisions[cum_sum[idx]] = (u16) i;
        cum_sum[idx] += (cum_sum[idx] > 0);
        i++;
      };
      cnt = actual_items / 4;
      while (LIKELY(cnt--)) {
        FillOneItem();
        FillOneItem();
        FillOneItem();
        FillOneItem();
        //    while (i < (actual_items / 4 * 4)) {
        //    for (auto i : range(actual_items)) {
        //       const auto idx = Const::kHashTableMask & (in_rows[i].GetHash() >> Const::kBucketCountBits);
      }
      //    cnt = 0;
      cnt = actual_items % 4;
      //    while (cnt < (actual_items % 8)) {
      while (LIKELY(cnt--)) {
        FillOneItem();
      }
      // Move to the next input partition
      i += (Const::kItemsInOutPartition - actual_items);
    }
    const InString* collision_group[Const::kTooManyBasicCollisions];

    auto OutputString = [&](const InString* first, const InString* second,
                            u16 first_index, u16 second_index) {
      static_assert((OutString::segments_reduced == InString::segments_reduced + 1) ||
                    (OutString::segments_reduced == InString::segments_reduced + 2),
                    "Invalid string generation");
      static_assert(InString::has_expanded_hash == OutString::has_expanded_hash,"");
      constexpr auto out_segments_reduced = OutString::segments_reduced;
      auto out_hash_xor = first->GetSecondRawHash() ^ second->GetSecondRawHash();
      const auto out_bucket = out_hash_xor & Const::kBucketNumberMask;
//      auto& counter = out_buckets->counter[out_bucket];
//      const auto out_index = counter + Const::kItemsInBucket * out_bucket;
//      counter += int(counter < Const::kItemsInBucket - 20);

//      auto counter = out_buckets->counter[out_bucket]++;
//      const auto out_index = counter + Const::kItemsInBucket * out_bucket;

      if (Const::kCheckBucketOverflow)
        if (UNLIKELY(out_buckets->counter[out_bucket]
                     >= Const::kItemsInBucket * (out_bucket + 1))) {
          return;
        }

      const auto out_index = out_buckets->counter[out_bucket]++;
      OutString& result = out_strings_[out_index];

      // Locate the interesting hash segments in source strings to start XOR there.
      auto a = (u8*)first->GetRawDataConst(out_segments_reduced);
      auto b = (u8*)second->GetRawDataConst(out_segments_reduced);
      auto xor_result = (u8*)result.GetRawData(out_segments_reduced);

      // Make the xor aligned on selected number of bytes so that only
      // exactly allowed instructions can be used.
      // We can reach BEHIND the result object but since we generate strings
      // in increasing order it is not a problem. We allocate space for it.
      XOR(xor_result, a, b,
          (OutString::hash_length + Const::kXORAlignment - 1) / Const::kXORAlignment * Const::kXORAlignment);

      if (Const::kFilterZeroQWordStrings)
//      if (result.GetCleanSegment(out_segments_reduced) == 0 && result.GetCleanSegment(out_segments_reduced + 1) == 0) {
        if (*(u64*)xor_result == 0) {
          out_buckets->counter[out_bucket]--;
          return;
        }

      assert(first_index < second_index);
      auto link = PairLink{second_index, first_index, in_bucket};
      if (!C::isFinal && Const::kStoreIndicesEarly)
        OutputIndex(&target_pair_index_[out_index], link);
      assert(link.Validate(out_index));
      result.SetLink(link);

      if (Const::kValidatePartialSolutions)
        solver_.ValidatePartialSolution(InString::segments_reduced,
                                        first->GetLink(),
                                        second->GetLink());
    };

    u64 last_final_segment = -1ull;
    auto GenerateSolution = [&](const InString* first, const InString* second,
                                u16 first_index, u16 second_index) {
      auto first_final_csegment = first->GetFinalCollisionSegment();
      if (first_final_csegment == second->GetFinalCollisionSegment()) {

        if (Const::kStep8FilterByLastSegment) {
          if (first_final_csegment == last_final_segment)
            return;
          last_final_segment = first_final_csegment;
        }
//        // Put everything to the first bucket
//        const auto out_index = out_buckets->counter[0]++;
//        OutString& result = out_strings_[out_index];
//
//        assert(first_index < second_index);
//        auto link = PairLink{second_index, first_index, in_bucket};
//        assert(link.Validate(out_index));
//        result.SetLink(link);

        solver_.ProcessSolutionCandidate(first->GetLink(), first_index,
                                         second->GetLink(), second_index);
        // out_strings_[first_index].SetLink(first->GetLink());
        // printf("Solution/collision at %ld \n", first->GetFinalCollisionSegment());
      }
    };

#define PREFETCH(...) if (Const::kPrefetchDistance > 0) __builtin_prefetch(__VA_ARGS__)
// #define PREFETCH(...)

#define ProduceOutput(...) (C::isFinal ? GenerateSolution(__VA_ARGS__) : OutputString(__VA_ARGS__))
// #define ProduceOutput OutputString

    for (auto i : range(Const::kHashTableSize)) {
      if (!cum_sum[i]) {
        continue;
      }
      auto cg_indices = &collisions[cum_sum[i] - count[i]];
      auto cnt = count[i];
      u16* prefetch_ptr = cg_indices + Const::kPrefetchDistance;

      switch (cnt) {
        default: {
          collision_group[0] = in_rows + cg_indices[0];
          collision_group[1] = in_rows + cg_indices[1];
          collision_group[2] = in_rows + cg_indices[2];
          collision_group[3] = in_rows + cg_indices[3];
          for (auto ii = 4; ii < cnt; ii++) {
            collision_group[ii] = in_rows + cg_indices[ii];
            // PREFETCH(in_rows + cg_indices[ii + prefetch_distance]);
            PREFETCH(in_rows + *prefetch_ptr++);
            // for (auto ii2 = ii+1; ii2 < cnt; ii2++) {
            for (auto ii2 = 0; ii2 < ii; ii2++) {
              //volatile auto keep = 0;
              ProduceOutput(collision_group[ii2], collision_group[ii], cg_indices[ii2], cg_indices[ii]);
            }
          }
        }
        case 4:
          PREFETCH(in_rows + *prefetch_ptr++);
          ProduceOutput(in_rows + cg_indices[0], in_rows + cg_indices[3], cg_indices[0], cg_indices[3]);
          ProduceOutput(in_rows + cg_indices[1], in_rows + cg_indices[3], cg_indices[1], cg_indices[3]);
          ProduceOutput(in_rows + cg_indices[2], in_rows + cg_indices[3], cg_indices[2], cg_indices[3]);
        case 3:
          PREFETCH(in_rows + *prefetch_ptr++);
          ProduceOutput(in_rows + cg_indices[0], in_rows + cg_indices[2], cg_indices[0], cg_indices[2]);
          ProduceOutput(in_rows + cg_indices[1], in_rows + cg_indices[2], cg_indices[1], cg_indices[2]);
        case 2:
          PREFETCH(in_rows + *prefetch_ptr++);
          ProduceOutput(in_rows + cg_indices[0], in_rows + cg_indices[1], cg_indices[0], cg_indices[1]);
        case 1:
        case 0:
          break;
      }
    }
    out_buckets->CheckCounters();
    }
    out_buckets->ClosePartition(outer_partition);
  }

  // for (auto i : range(Const::kMaximumStringSetSize))
/*
    for (auto i : range(input_size)) {
      _mm_stream_si32((int*) &link_index[i], out_strings_[i].GetHash());
      // link_index[i] = out_strings_[i].GetHash();
    }
*/
  solver_.ReportStep("Performed reduction step");
  if (Const::kReportCollisions)
    ReportCollisionStructure(collisions_, in_buckets->CountUsedPositions());

  if (Const::kReportSteps) {
    // auto out_strings = out_buckets->CountUsedPositions();
    // printf("Generated %d out strings\n", out_strings);
  }
  return true;
}

template<typename C, typename S>
void inline ReductionStep<C,S>::OutputIndex(PairLink* target, PairLink link) {
  if (Const::kUseNonTemporalStoresForIndices)
    target->copy_nt(link);
  else
    *target = link;
}

void Solver::ProcessSolutionCandidate(PairLink l8_link1, u32 link1_position,
                                      PairLink l8_link2, u32 link2_position) {
  // Make space for a new solution.
  if (solution_objects_.size() < valid_solutions_ + 1)
    solution_objects_.emplace_back(512);

  auto& solution = solution_objects_[valid_solutions_];
  auto success = ExtractSolution(l8_link1, link1_position,
                                 l8_link2, link2_position,
                                 solution, 8);
  if (!success) {
    invalid_solutions_++;
    return;
  }

  if (Const::kRecomputeSolution) {
    if (!RecomputeSolution(solution, 8, false)) {
      fprintf(stderr,
              "********** FATAL ERROR: Invalid solution!! **********\n");
      invalid_solutions_++;
      return;
    }
  }
  // Reorder the solution if it is valid
  ReorderSolution(solution);
  valid_solutions_++;
}

void Solver::ValidatePartialSolution(u32 level, PairLink link1,
                                     PairLink link2) {
  // Make space for a new solution.
  if (solution_objects_.size() < valid_solutions_ + 1)
    solution_objects_.emplace_back(512);

  auto& solution = solution_objects_[valid_solutions_];
  auto success = ExtractSolution(link1, 0, link2, 0, solution, level);
  if (!success) {
    if (level == 8) {
      fprintf(stderr, "Cannot extract partial solution\n");
      assert(false);
      return;
    }
  }
  // We don't care about swaps
  // auto swaps = ReorderSolution(solution);

  if (!RecomputeSolution(solution, level, false)) {
    fprintf(stderr, "********** FATAL ERROR: Invalid PARTIAL solution!! **********\n");
    assert(false);
  }
}

i32 Solver::Run() {
  if (!initialized_)
    return -1;

  // Default space selection strategy.
  auto FA = SpaceAllocator::FirstAvailable;

  ReportStep(nullptr, true);

  space_X2->Allocate(FA);
  space_X1->Allocate(FA);
  BucketIndices buckets1;
  BucketIndices buckets2;
  buckets1.Reset();

  if (Const::kGenerateTestSet)
    GenerateXStringsTest(space_X1, &buckets1);
  else {
    // Only when it is allowed and we detected a batch backend,
    // use batch string generation.
    auto batch_size = blake.GetBatchSize();
    if (Const::kAllowBlakeInBatches && batch_size > 0) {
      switch (batch_size) {
        case 4:
          GenerateXStringsBatch<4>(space_X1, &buckets1);
          break;
        case 2:
          GenerateXStringsBatch<2>(space_X1, &buckets1);
          break;
        case 1:
          GenerateXStringsBatch<1>(space_X1, &buckets1);
          break;
        default:
          fprintf(stderr, "Invalid blake batch size %d\n", batch_size);
          abort();
      }
    }
    else
      GenerateXStrings(space_X1, &buckets1);
  }

  buckets1.ClosePartitionsForNewStrings();

  context_->Allocate();

  using Step0 = ReductionStep<ReductionStepConfig<0>, Solver>;
  auto step0 = Step0{*this};
  step0.in_strings = space_X1;
  step0.out_strings = space_X2;
  step0.target_link_index = link_indices_[step0.segments_reduced]->Allocate(
      FA);
  if (!step0.Execute(context_, &buckets1, &buckets2)) {
    return 0;
  }
  using Step1 = ReductionStep<ReductionStepConfig<1>, Solver>;
  auto step1 = Step1{*this};
  space_X2->Resize<typename Step1::InString>();
  space_X1->Reallocate<typename Step1::InString>(FA);
  step1.in_strings = space_X2;
  step1.out_strings = space_X1;
  step1.target_link_index = link_indices_[step1.segments_reduced]
      ->Allocate(FA);
  if (!step1.Execute(context_, &buckets2, &buckets1)) {
    return 0;
  }

  using Step2 = ReductionStep<ReductionStepConfig<2>, Solver>;
  auto step2 = Step2{*this};
  space_X1->Resize<typename Step2::InString>();
  space_X2->Reallocate<typename Step2::InString>(FA);
  step2.in_strings = space_X1;
  step2.out_strings = space_X2;
  step2.target_link_index = link_indices_[step2.segments_reduced]
      ->Allocate(FA);
  if (!step2.Execute(context_, &buckets1, &buckets2)) {
    return 0;
  }

  using Step3 = ReductionStep<ReductionStepConfig<3>, Solver>;
  auto step3 = Step3{*this};
  space_X2->Resize<typename Step3::InString>();
  space_X1->Reallocate<typename Step3::InString>(FA);
  step3.in_strings = space_X2;
  step3.out_strings = space_X1;
  step3.target_link_index = link_indices_[step3.segments_reduced]
      ->Allocate(FA);
  if (!step3.Execute(context_, &buckets2, &buckets1)) {
    return 0;
  }

  using Step4 = ReductionStep<ReductionStepConfig<4>, Solver>;
  auto step4 = Step4{*this};
  space_X1->Resize<typename Step4::InString>();
  space_X2->Reallocate<typename Step4::InString>(FA);
  step4.in_strings = space_X1;
  step4.out_strings = space_X2;
  step4.target_link_index = link_indices_[step4.segments_reduced]
      ->Allocate(FA);
  if (!step4.Execute(context_, &buckets1, &buckets2)) {
    return 0;
  }

  using Step5 = ReductionStep<ReductionStepConfig<5>, Solver>;
  auto step5 = Step5{*this};
  space_X2->Resize<typename Step5::InString>();
  space_X1->Reallocate<typename Step5::InString>(FA);
  step5.in_strings = space_X2;
  step5.out_strings = space_X1;
  step5.target_link_index = link_indices_[step5.segments_reduced]
      ->Allocate(FA);
  if (!step5.Execute(context_, &buckets2, &buckets1)) {
    return 0;
  }

  using Step6 = ReductionStep<ReductionStepConfig<6>, Solver>;
  auto step6 = Step6{*this};
  space_X1->Resize<typename Step6::InString>();
  space_X2->Reallocate<typename Step6::InString>(FA);
  step6.in_strings = space_X1;
  step6.out_strings = space_X2;
  step6.target_link_index = link_indices_[step6.segments_reduced]
      ->Allocate(FA);
  if (!step6.Execute(context_, &buckets1, &buckets2)) {
    return 0;
  }

  using Step7 = ReductionStep<ReductionStepConfig<7>, Solver>;
  auto step7 = Step7{*this};
  // space_S2->Release();
  step7.in_strings = space_X2;
  step7.out_strings = space_X1;
  step7.target_link_index = link_indices_[step7.segments_reduced]->Allocate(FA);
  if (!step7.Execute(context_, &buckets2, &buckets1)) {
    return 0;
  }

  using Step8 = ReductionStep<FinalStepConfig<8>, Solver>;
  auto step8 = Step8{*this};
  space_X1->Resize<typename Step8::InString>();
  space_X2->Reallocate<typename Step8::InString>(FA);
  step8.in_strings = space_X1;
  step8.out_strings = space_X2;
  step8.target_link_index = link_indices_[step8.segments_reduced]
      ->Allocate(FA);
  if (!step8.Execute(context_, &buckets1, &buckets2)) {
    return 0;
  }

  ReportStep(nullptr, true);
  return valid_solutions_;
}

void ExpandArrayFastPrecise(const u8* hash, u8* array) {
  auto hash_end = hash + 25;
  for (; hash < hash_end; hash += 5, array += 6) {
    auto pair = *(u64*)hash;
    auto target = (u64*)array;
    auto tmp = ((pair & 0x0000f0f0f0) >> 4) |
               ((pair & 0x0000000f0f) << 12) |
               ((pair & 0xffff0f0000) << 8);
    // To little endian
    *target = ((tmp & 0xff0000ff0000) >> 16) |
              ((tmp & 0x00ff0000ff00)) |
              ((tmp & 0x0000ff0000ff) << 16);
  }
}

inline void ExpandArrayFast(const u8* __restrict hash,
                            u8* __restrict array) {
  auto hash_end = hash + 25;
  for (; hash < hash_end; hash += 5, array += 6) {
    auto pair = *(u64*)hash;
    auto target = (u64*)array;
    *target = ((pair & 0x00ffff00ffff)) |
              ((pair & 0x000000f00000) >> 4) |
              ((pair & 0x0000000f0000) << 24);
  }
}

bool Solver::ExtractSolution(PairLink l8_link1, u32 link1_position,
                             PairLink l8_link2, u32 link2_position,
                             std::vector<u32>& result,
                             u32 link_level) {
  auto& temporary = temporary_solution_;
  auto solution_size = 2 * (1u << link_level);

  result.clear();
  result.reserve(solution_size);
  temporary.clear();
  temporary.reserve(solution_size);

  if (link_level == 0) {
    result.push_back(l8_link1.GetData());
    result.push_back(l8_link2.GetData());
    temporary = result;
    return result[0] != result[1];
  }

  auto source = &result;
  auto target = &temporary;

  auto l1 = l8_link1.Translate(link1_position);
  auto l2 = l8_link2.Translate(link2_position);

  source->push_back(l1.first);
  source->push_back(l1.second);
  source->push_back(l2.first);
  source->push_back(l2.second);

  for (auto level = link_level - 1; level > 0; --level) {
    target->clear();
    auto index = link_indices_[level]->template As<PairLink>();

    for (auto ref : *source) {
      assert(ref < Const::kMaximumStringSetSize);
      auto link = index[ref];
      if (Const::kCheckLinksConsistency && link.GetData() == 0xffffffff) {
        assert(false);
        return false;
      }
      auto tr = link.Translate(ref);
//      target->push_back(link.orig_first);
//      target->push_back(link.orig_second);
      target->push_back(tr.first);
      target->push_back(tr.second);
      if (Const::kCheckLinksConsistency && !link.Validate(ref)) {
        assert(false);
        return false;
      }
    }
    std::swap(source, target);
  }
  assert(source->size() == solution_size);
  if (link_level % 2 == 1) {
    temporary = result;
    std::swap(source, target);
  }
  // The last translation step will finish in `result` vector.
  assert(target == &result);

  // Translate the expanded indices to indices of the originally
  // generated strings.
  target->clear();
  auto index = link_indices_[0]->template As<PairLink>();
  for (auto& ref : *source) {
    auto original_index = index[ref].GetData();
    target->push_back(original_index);
    // Put the same result into both vectors
    ref = original_index;
  }

  // Check uniqueness of all indices.
  std::sort(temporary.begin(), temporary.end());
  auto last = -1u;
  for (auto idx : temporary) {
    if (idx == last) {
      // Invalid solution
      return false;
    }
    last = idx;
  }
  return true;
}


u32 Solver::ReorderSolution(std::vector<u32>& solution) {
  // Helper function for swapping two branches. Can be optimized
  auto swap = [](u32* p1, u32* p2, int length) {
    for (auto i : range(length)) {
      std::swap(p1[i], p2[i]);
    }
  };
  u32 swap_count = 0;
  auto data = solution.data();
  for (u32 length = 1; length <= 256; length *= 2) {
    u32 step = length * 2;
    for (u32 start = 0; start < 512; start += step) {
      if (data[start] >= data[start+length]) {
        swap(&data[start], &data[start + length], length);
        swap_count++;
      }
    }
  }
  return swap_count;
}


bool Solver::RecomputeSolution(std::vector<u32>& solution,
                               u32 level, bool check_ordering) {
  if (!initialized_) {
    fprintf(stderr, "Solver not initialized");
    return false;
  }

  auto solution_size = 2 * (1u << level);

  if (solution.size() != solution_size)
    return false;

  OneTimeString xstrings[solution_size];
  // Generate all strings from given indices
  for (auto i : range(solution.size())) {
    u32 string_index = solution[i];
    // The index must be in valid bounds
    if (string_index >= Const::kInitialStringSetSize)
      return false;
    if (Const::kGenerateTestSet)
      GenerateOTStringTest(solution[i], xstrings[i]);
    else
      GenerateOTString(string_index, xstrings[i]);
  }

  if (false) {
    // Start with the last
    OneTimeString combined = xstrings[solution_size - 1];
    for (auto i : range(solution_size - 1)) {
      // XOR-in all but the last
      XOR(combined.GetRawData(0),
          combined.GetRawData(0),
          xstrings[i].GetRawData(0),
          OneTimeString::hash_length);
    }
    bool all0 = true;
    auto check_segments = (level == 8 ? 10 : level + 1);
    for (auto segment : range(check_segments)) {
      if (!(combined.GetCleanSegment(segment) == 0)) {
        // printf("Not all segments XOR-ed to 0: %d!\n", segment);
        return false;
      }
    }
  }

  auto indices = solution.data();
  // Combine the stings in a binary tree-like manner.
  for (auto segment : range(level + 1)) {
    u32 pair_distance = 1u << segment;
    u32 next_pair = pair_distance * 2;
    for (u32 first = 0; first < solution_size; first += next_pair) {
      if (check_ordering && indices[first] >= indices[first + pair_distance])
        return false;
      auto& x1 = xstrings[first];
      auto& x2 = xstrings[first + pair_distance];
      // Perform xor operation in place (xor second string into the first,
      // which can stay at the same index then).
      XOR(x1.GetRawData(segment),
          x1.GetRawData(segment),
          x2.GetRawData(segment),
          OneTimeString::hash_length - (Const::kHashSegmentBytes * segment));
      // The i-th segment is supposed to be 0!
      if (x1.GetCleanSegment(segment))
        return false;
    }
  }
  // The last segment of the final string must be 0 as well.
  if (level == 8 && xstrings[0].GetCleanSegment(9) != 0u)
    return false;

  return true;
};

template<typename C, typename S>
void ReductionStep<C,S>::ReportCollisionStructure(std::vector<u32>& collisions, u32 string_count) {
  u64 total_pairs = 0;
  u64 total_collisions = 0;
  for (auto i : range(collisions.size())) {
    if (collisions[i] > 0) {
      u32 combinations = (u32)(i * (i - 1) / 2);
      u32 pairs = collisions[i] * combinations;
      printf("%3ld: %8d = %d collisions * %d (%ld) combinations\n", i, pairs, collisions[i], combinations, i-1);
      total_pairs += pairs;
      total_collisions += collisions[i];
    }
  }
  printf("total pairs: %ld (%ld) from %ld collisions (%d strings)\n",
         total_pairs, (i64)total_pairs - (i64)string_count,
         total_collisions, string_count);
}

static inline u64 now() {
  return (u64)std::chrono::steady_clock::now()
      .time_since_epoch().count() / 1000;
}

void Solver::ResetTimer() {
  timer_start_ = major_start_ = last_report_ = now();
};


void Solver::ReportStep(const char* name, bool major) {
  if (!Const::kReportSteps)
    return;
  auto time = now();
  if (name && print_reports_) {
    printf("[ %7ld %6ld ] %s\n", time - major_start_,time - last_report_,
           name);
  }
  if (major) {
    if (print_reports_)
      printf("[ %7ld        ] # ***\n", time - timer_start_);
    major_start_ = time;
  }
  last_report_ = time;
};

}  // namespace zceq_solver

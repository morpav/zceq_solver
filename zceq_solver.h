/* Copyright @ 2016 Pavel Moravec */
#ifndef ZCEQ_SOLVER_H_
#define ZCEQ_SOLVER_H_

#include <cassert>
#include <cmath>
#include <vector>
#include <cstring>

#include "zceq_config.h"
#include "zceq_blake2b.h"
#include "zceq_misc.h"
#include "zceq_space_allocator.h"


#include "x86intrin.h"


namespace zceq_solver {

using BlakeState = crypto_generichash_blake2b_state;

struct Inputs {
  u8 data[140];
  void SetSimpleNonce(u64 nonce) {
    *((u64*)&data[140 - 8]) = nonce;
  }
};

class PairLink {
  u32 data_;
 public:
#ifdef LINK_ORIGINALS
  u32 orig_first;
  u32 orig_second;
  u32 orig_link_position;
#endif
  struct Translated {
    u32 first;
    u32 second;
  };
 public:
  PairLink() = default;
  PairLink(u32 larger, u32 smaller, u32 bucket) {
    set(larger, smaller, bucket);
  }

  inline void set(u32 larger, u32 smaller, u32 bucket) {
#ifdef LINK_ORIGINALS
    orig_first = smaller + bucket * Const::kItemsInBucket;
    orig_second = larger + bucket * Const::kItemsInBucket;
#endif
    assert(larger > smaller);
    auto indices = ((larger * (larger - 1) / 2) + smaller);
    assert(indices < Const::kMaxCompressedIndexValue);
    data_ = indices | (bucket << Const::kBucketInIndexShift);
    assert(larger == std::round(std::sqrt((double)(2 * (data_ & ((1u << Const::kBucketInIndexShift) - 1)) + 1))));
  }
  inline void SetSingleIndex(u32 single) {
    data_ = single;
#ifdef LINK_ORIGINALS
    orig_first = single;
    orig_second = 0;
#endif
  }
  inline void copy_nt(PairLink value) {
    static_assert(sizeof(i32) == sizeof *this, "Different size of pair link used for nt-store");
    _mm_stream_si32((int*)this, value.data_);
  }
  inline Translated Translate(u64 link_position) {
    auto indices = data_ & ((1u << Const::kBucketInIndexShift) - 1);

    auto larger = (u32)(std::sqrt((float)(2 * indices + 1)));
    auto smaller = indices - (larger * (larger - 1) / 2);
    // Compensate missing rounding. Theese are just a few branch-less intructions
    // instead of calling round.
    int over = smaller >= larger;
    smaller -= larger * over;
    larger += over;

    auto partition = u32((link_position % Const::kItemsInBucket) / Const::kItemsInOutPartition);
    partition &= ((1u << Const::kPartitionCountBits) - 1);
    auto bucket = (u32)(partition << (32u - Const::kBucketInIndexShift) |
                        data_ >> Const::kBucketInIndexShift);

    auto result = Translated{
        u32(Const::kItemsInBucket * bucket + smaller),
        u32(Const::kItemsInBucket * bucket + larger)
    };
#ifdef LINK_ORIGINALS
    assert(orig_first == result.first);
    assert(orig_second == result.second);
#endif
    return result;
  }

  inline bool Validate(u64 link_position) {
    auto tr = Translate(link_position);
    PairLink link {tr.second % Const::kItemsInBucket, tr.first % Const::kItemsInBucket,
                   tr.first / Const::kItemsInBucket};
    return link.GetData() == GetData() &&
        ((tr.first / Const::kItemsInBucket) == (tr.second / Const::kItemsInBucket));
  }

  inline u32 GetData() {
    return data_;
  }
};

// Type alias for standalone representation of hash segment used
// for collision search.
using HSegment = u32;


template<u32 segments_reduced_, bool expanded_hash>
class alignas(Const::kXStringAlignment) XString {
  constexpr static u32 GetHashLength() {
    return (expanded_hash) ?
           (Const::kHashSegmentBytes *
               (Const::kTotalSegmentsCount - segments_reduced))
                           :
           (((Const::kTotalHashBits -
               (segments_reduced * Const::kHashSegmentBits)) + 7) / 8);
  }
 public:
  constexpr static auto segments_reduced = segments_reduced_;
  constexpr static auto has_expanded_hash = expanded_hash;
  constexpr static u32 hash_length = GetHashLength();

 public:
  // Returns raw u32 integer with least significant bits containing the requested
  // segment. Higher bits can contain anything.
  inline u32 GetRawSegment(u32 idx) const {
    assert(ContainsSegment(idx));
    auto raw_data = *(u32*)GetRawDataConst(idx);
    auto shifted = raw_data >> GetSegmentShift(idx);
    return shifted;
  }
  inline HSegment GetCleanSegment(u32 idx) {
    return GetRawSegment(idx) & Const::kHashSegmentBitMask;
  }
  inline HSegment GetRawHash() const {
    static_assert(Const::kHashSegmentBitMask == 0x000fffff, "Not 200,9?");
    return GetRawSegment(segments_reduced_);
  }
  inline HSegment GetSecondRawHash() const {
    return GetRawSegment(segments_reduced_ + 1);
  }
  inline PairLink GetLink() const {
    return link_;
  }
  inline void SetLink(PairLink link) {
    link_ = link;
  }
  inline void SetIndex(u32 index_) {
    link_.SetSingleIndex(index_);
  }
  static constexpr u32 GetSegmentShift(u32 segment) {
    return (!expanded_hash && (segment % 2)) ? 4 : 0;
  }
  inline const u8* GetRawDataConst(u32 from_segment) const {
    return const_cast<XString<segments_reduced_,
        expanded_hash>*>(this)->GetRawData(from_segment);
  }
  // Returns first byte with bits participating in the requested segment.
  // It can start from bit 0 or 4, depending on the segment.
  inline u8* GetRawData(u32 from_segment) {
    assert(ContainsSegment(from_segment));
    if (expanded_hash)
      return hash_bytes_ + (Const::kHashSegmentBytes * (from_segment - segments_reduced));
    else {
      constexpr auto reduced_bytes = (Const::kHashSegmentBits * segments_reduced) / 8;
      return hash_bytes_ + (Const::kHashSegmentBits * from_segment / 8) - reduced_bytes;
    }
  }
  inline void SetSegment(u32 segment, u32 hash_value) {
    assert((~Const::kHashSegmentBitMask & hash_value) == 0);

    auto raw = (u32*)GetRawData(segment);
    auto shift = GetSegmentShift(segment);
    *raw = (*raw & ~(Const::kHashSegmentBitMask << shift)) |
           ((hash_value & Const::kHashSegmentBitMask) << shift);
    assert(GetCleanSegment(segment) == hash_value);
  }
  inline u64 GetFinalCollisionSegment() const {
    assert(segments_reduced == 8);
    // Take only the last two segments
    u64 valid_bits = expanded_hash ? (Const::kHashSegmentBytes * 2 * 8) :
                     Const::kHashSegmentBits * 2;
    return *reinterpret_cast<const u64*>(hash_bytes_)
           & ((1ul << valid_bits) - 1);
  }
 protected:
  static constexpr bool ContainsSegment(u32 segment) {
    return segment >= segments_reduced && segment <= Const::kTotalSegmentsCount;
  }

  static_assert(segments_reduced <= Const::kTotalSegmentsCount, "");

 public:
  PairLink link_;
  u8 hash_bytes_[hash_length];

  template<u32 p, bool exp_h>
  friend class XString;
};

// Last "string" with all segments reduced, holding only a solution candidate.
// It must conform to a part of XString interface to allow generic code to
// compile (but the objects are never used as strings in runtime).
struct SolutionCandidate {
  // The string has all segments reduced.
  static constexpr u32 segments_reduced = Const::kTotalSegmentsCount;
  static constexpr u32 hash_length = 0;
  static constexpr bool has_expanded_hash = Const::kExpandHashes;
  inline u8* GetRawData(u32 segment) {
    return nullptr;
  }
  inline void SetLink(PairLink link) {
    link1 = link;
  }

  PairLink link1;
  PairLink link2;
  u16 link1_position_mod_bucket_size;
  u16 link2_position_mod_bucket_size;
};

template<u32 step_no>
struct ReductionStepConfig {
  static constexpr bool isFinal = false;

  using InString = XString<step_no, Const::kExpandHashes>;
  using OutString = XString<step_no + 1, Const::kExpandHashes>;
};

template<u32 step_no>
struct FinalStepConfig : ReductionStepConfig<step_no> {
  static_assert(step_no == 8, "");
  // We need to select XString of such size that it is big enough to contain
  // SolutionCandidate object. The string is never used directly as a string
  // but we need the space.
  using OutString = XString<8, Const::kExpandHashes>;
  static_assert(sizeof(SolutionCandidate) <= sizeof(OutString), "");
  static constexpr bool isFinal = true;
};

struct BucketIndices {
  u32 counter[Const::kBucketCount];
  // The last partition size is implicitly represented by the `counter`
  u16 partition_sizes[Const::kBucketCount][Const::kPartitionCount];

  void Reset() {
    for (auto i : range(Const::kBucketCount)) {
      counter[i] = (i * Const::kItemsInBucket);
    }
    memset(partition_sizes, 0, sizeof partition_sizes);
  }

  void ResetForFinal() {
    counter[0] = 0;
  }

  u32 CountUsedPositions() {
    u32 sum = 0;
    for (auto i : range(Const::kBucketCount)) {
      for (auto p : range(Const::kPartitionCount))
        sum += partition_sizes[i][p];
    }
    return sum;
  }

  void CheckCounters() {
    for (auto i : range(Const::kBucketCount)) {
      auto diff = counter[i] - (i * Const::kItemsInBucket);
      if (diff > Const::kItemsInBucket)
        assert(false);
    }
  }

  void ClosePartition(u32 partition) {
    u32 shift = partition * Const::kItemsInOutPartition;

    // Record the partition size for each bucket
    for (auto i : range(Const::kBucketCount)) {
      auto size = counter[i] - (i * Const::kItemsInBucket) - shift;
      assert(size <= Const::kItemsInBucket);
      // Ensure that the size is always within bounds.
      partition_sizes[i][partition] = std::min((u16)size, (u16)Const::kItemsInOutPartition);
    }

    // If this was not the last partition, update counter to the next one.
    if (partition != Const::kPartitionCount - 1) {
      // Counter shift within a bucket caused by closing the partition.
      shift = (partition + 1) * Const::kItemsInOutPartition;

      // Update a counter for each bucket. It CAN happen that the move
      // is backwards! but it simply means that the strings could not be
      // properly coded in pair link indices.
      for (auto i : range(Const::kBucketCount)) {
        counter[i] = (i * Const::kItemsInBucket) + shift;
      }
    }
  }

  void ClosePartitionsForNewStrings() {
     // Record the partition size for each bucket
    for (auto i : range(Const::kBucketCount)) {
      auto bucket_start = (i * Const::kItemsInBucket);
      for (auto part : range(Const::kPartitionCount)) {
        auto part_start = bucket_start + part * Const::kItemsInOutPartition;
        if (part_start >= counter[i])
          partition_sizes[i][part] = 0;
        else
          partition_sizes[i][part] = (u16)std::min((counter[i] - part_start), (u32)Const::kItemsInOutPartition);
      }
    }
  }
};

struct Context {
  vector<u16> hash;
  vector<u16> count;
  vector<u16> cum_sum;
  vector<u16> collisions;

  void Allocate() {
    hash.resize(Const::kItemsInBucket);
    count.resize(Const::kHashTableSize);
    cum_sum.resize(Const::kHashTableSize);
    collisions.resize(Const::kItemsInBucket);
  }
};

template<typename Configuration, typename SolverT>
class ReductionStep {
 public:
  // Allow easy access to step configuration's values.
  static constexpr Configuration Cv = {};
  static constexpr u32 segments_reduced = Configuration::InString::segments_reduced;
  static constexpr auto odd_step = bool(segments_reduced % 2);

  using C = Configuration;
  using InString = typename C::InString;
  using OutString = typename C::OutString;

  SpaceAllocator::Space* in_strings = nullptr;
  SpaceAllocator::Space* out_strings = nullptr;
  SpaceAllocator::Space* target_link_index = nullptr;

  ReductionStep(SolverT& solver) : solver_(solver) {};

  bool PrepareRTConfiguration();
  bool Execute(Context* context, BucketIndices* input_buckets, BucketIndices* output_buckets) noexcept;

 protected:
  void OutputIndex(PairLink* target, PairLink);
  void ReportCollisionStructure(std::vector<u32>& collisions, u32 string_count);

  SolverT& solver_;

  InString* in_strings_ = nullptr;
  OutString* out_strings_ = nullptr;
  PairLink* target_pair_index_ = nullptr;

  std::vector<u32> collisions_;
};

class Solver {
 public:
  using Space = SpaceAllocator::Space;

  using OneTimeString = XString<0, true>;
  using GeneratedString = ReductionStepConfig<0>::InString;

  Solver();
  Solver(const Solver&) = delete;
  ~Solver();

  void Reset(const u8* data, u64 length);
  void Reset(Inputs& inputs);
  void GenerateOTString(u32 index, OneTimeString& result);
  void GenerateOTStringTest(u32 index, OneTimeString& result);
  i32 Run();

  const vector<const vector<u32>*>& GetSolutions() {
    solutions_.clear();
    for (auto i : range(valid_solutions_))
      solutions_.push_back(&solution_objects_[i]);
    return solutions_;
  }

  u32 GetInvalidSolutionCount() {
    return invalid_solutions_;
  }
  bool ValidateSolution(std::vector<u32>& solution) {
    return RecomputeSolution(solution, 8, true);
  }
  bool RecomputeSolution(std::vector<u32>& solution) {
    return RecomputeSolution(solution, 8, false);
  }

 protected:
  void ResetMemoryAllocator();
  void GenerateXStrings(SpaceAllocator::Space* target_space, BucketIndices* buckets);
  template<u32 batch_size>
  void GenerateXStringsBatch(SpaceAllocator::Space* target_space, BucketIndices* buckets);
  void GenerateXStringsTest(SpaceAllocator::Space* target_space, BucketIndices* buckets);

  void ClearSolutions() {
    valid_solutions_ = 0;
    invalid_solutions_ = 0;
    solutions_.clear();
  }
  void ProcessSolutionCandidate(PairLink l8_link1, u32 link1_position,
                                PairLink l8_link2, u32 link2_position);
  void ValidatePartialSolution(u32 level, PairLink link1,
                                PairLink link2);
  bool ExtractSolution(PairLink l8_link1, u32 link1_position,
                       PairLink l8_link2, u32 link2_position,
                       std::vector<u32>& result,
                       u32 link_level);
  u32 ReorderSolution(std::vector<u32>& solution);
  bool RecomputeSolution(std::vector<u32>& solution, u32 level, bool check_ordering);
  void ResetTimer();

  void ReportStep(const char* name, bool major = false);

  Blake2b blake;
  Context* context_;
  std::vector<Space*> link_indices_;
  Space* space_X1 = nullptr;
  Space* space_X2 = nullptr;

  u64 timer_start_ = 0;
  u64 major_start_ = 0;
  u64 last_report_ = 0;
  SpaceAllocator allocator_;
  bool print_reports_ = true;
  i32 valid_solutions_ = 0;
  i32 invalid_solutions_ = 0;
  std::vector<std::vector<u32>> solution_objects_;
  std::vector<const std::vector<u32>*> solutions_;
  std::vector<u32> temporary_solution_;
  bool initialized_ = false;

  template<typename Configuration, typename SolverT>
  friend class ReductionStep;
};

}  // namespace zceq_solver

#endif  // ZCEQ_SOLVER_H_

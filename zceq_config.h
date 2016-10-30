/* Copyright @ 2016 Pavel Moravec */
#ifndef ZCEQ_CONFIG_H_
#define ZCEQ_CONFIG_H_

#include "zceq_misc.h"

namespace zceq_solver {

struct Const {
  // Problem setup, some cannot be changed without code changes.
  static constexpr u32 N_parameter = 200;
  static constexpr u32 K_parameter = 9;
  static constexpr u32 kTotalHashBits = N_parameter;
  static constexpr u32 kHashSegmentBits = N_parameter / (K_parameter + 1);
  static constexpr u32 kHashSegmentBitMask = (1u << kHashSegmentBits) - 1;
  static_assert(kHashSegmentBits * (K_parameter + 1) == N_parameter,
                "incompatible problem parameters");
  static constexpr u32 kHashSegmentBytes = (kHashSegmentBits + 7) / 8;
  static constexpr u32 kTotalSegmentsCount = (K_parameter + 1);
  static constexpr u32 kInitialStringSetSize = 1u << (kHashSegmentBits + 1);

  static constexpr u64 kMemoryForExpandedProblem = 76ul;
  static constexpr u64 kMemoryForNonExpandedProblem = 80ul; //68ul;
//  static constexpr u64 kMemoryForExpandedProblem = 376ul;
//  static constexpr u64 kMemoryForNonExpandedProblem = 380ul; //68ul;

  // Algorithm parameters
  static constexpr bool kExpandHashes = false;
  static constexpr bool kRecomputeSolution = false;
  static constexpr bool kFilterZeroQWordStrings = false;
  static constexpr bool kCheckBucketOverflow = true;
  static constexpr bool kStep8FilterByLastSegment = true;
  static constexpr bool kAllowBlakeInBatches = true;
  // If AVX2 present, use asm version?
  static constexpr bool kUseAsmAVX2Code = true;
  // TODO: Not properly supported in this version of the solver, yet.
  // Do not change it.
  static constexpr bool kStoreIndicesEarly = false;
  static constexpr bool kUseNonTemporalStoresForIndices = true;
  static constexpr u64 kXStringAlignment = 4ul;
  static constexpr u64 kXORAlignment = 4ul;
  static constexpr u64 kExtraSpaceMultiplier = 7;
  static constexpr u64 kExtraSpaceDivisor = 5;
  static constexpr u64 kBucketCountBits = 8;
  static constexpr u32 kBucketInIndexShift = 26;
  static constexpr u32 kTooManyBasicCollisions = 14;
  static constexpr u32 kTooManyFinalCollisions = 3;
  static constexpr u64 kPrefetchDistance = 16;

  // Debugging flags - should be always false if you're not debugging
  // a particular sub-system. Some has huge performance effect.
  static constexpr bool kRecomputeHashesByRefImpl = false;
  static constexpr bool kValidatePartialSolutions = false;
  static constexpr bool kCheckLinksConsistency = false;
  static constexpr bool kReportCollisions = false;
  static constexpr bool kReportSteps = false;
  static constexpr bool kReportMemoryAllocation = false;
  static constexpr bool kGenerateTestSet = false;
  static constexpr u32 kTestSetExpandMultiplier = 1;

  // Derived constants
  static constexpr u64 kBucketNumberMask = (1u << kBucketCountBits) - 1;
  static constexpr u16 kBucketCount = (1u << kBucketCountBits);
  static constexpr u32 kItemsInBucket =
      kInitialStringSetSize * kExtraSpaceMultiplier / kExtraSpaceDivisor / kBucketCount;
  static constexpr u64 kMaxCompressedIndexValue =
      kItemsInBucket * (kItemsInBucket - 1) / 2 + kItemsInBucket - 1;
  static_assert((1u << kBucketInIndexShift) > kMaxCompressedIndexValue, "");
  static_assert((1u << (kBucketInIndexShift - 1)) <= kMaxCompressedIndexValue,
                "");

  static constexpr u64 kHashTableSizeBits = kHashSegmentBits - kBucketCountBits;
  static constexpr u64 kHashTableMask = (1u << kHashTableSizeBits) - 1;
  static constexpr u16 kHashTableSize = (1u << kHashTableSizeBits);

  static constexpr u32 kPartitionCountBits =
      kBucketInIndexShift + kBucketCountBits - 32;
  static constexpr u32 kPartitionCount = (1u << kPartitionCountBits);
  static constexpr u32 kBucketsPerPartition = kBucketCount / kPartitionCount;
  static constexpr u32 kItemsInOutPartition = kItemsInBucket / kPartitionCount;
  static_assert(kBucketsPerPartition * kPartitionCount == kBucketCount, "");

  static constexpr u64 kMaximumStringSetSize =
      kInitialStringSetSize * kExtraSpaceMultiplier / kExtraSpaceDivisor;

  static_assert(kPartitionCountBits < 32, "Expression overflowed");
  static_assert(kItemsInOutPartition * kPartitionCount <= kItemsInBucket, "");
  static_assert(
      kItemsInOutPartition * kPartitionCount + kPartitionCount > kItemsInBucket,
      "");
};

}  // namespace zceq_solver


#endif  // ZCEQ_CONFIG_H_

/* Copyright @ 2016 Pavel Moravec */
#ifndef ZCEQ_CONFIG_H_
#define ZCEQ_CONFIG_H_

#include "zceq_misc.h"

namespace zceq_solver {

// By default, all instruction sets specifically supported by the
// software are allowed in run-time.
// A user may want to disable some instruction sets mostly to force
// the solver to use different code paths for profile guided compiler
// optimizations in scenario of binary build distribution.
struct InstructionSet {
  bool AVX2 = true;
  bool AVX1 = true;
  bool SSE41 = true;
  bool SSSE3 = true;
  // Temporarily disabled as not proper SSE2 batch implementation exists.
  bool SSE2 = false;
};

// Configuration structure which can be altered during run-time.
// Mainly intended for command line switches.
struct RTConfig {
  // Specifies which various batch implementations are allowed to run.
  InstructionSet kBatchBlakeAllowed;
  // Specifies which various scalar implementations are allowed to run.
  InstructionSet kScalarBlakeAllowed;
  // Turn off to force the solver to use scalar blake2b
  // implementations.
  bool kAllowBlake2bInBatches = true;
  // Turn off to force the solver to use intrinsics-based blake2b
  // implementations.
  bool kUseAsmBlake2b = true;
};

// Global instance of the CPU configuration.
extern RTConfig RunTimeConfig;

// Compile-time configuration of the solver.
namespace Const {
  // ---
  // Problem setup, these values are directly derived from equihash problem
  // definition (the zcash variant).
  // ---
  //
  // Used equihash parameters. These cannot be changed easily right
  // now.  Some code must be generalized or replaced to support more
  // problem classes. It is not so hard to do but one would have to
  // re-think if the algorithm used is a proper one for the different
  // parameters. If we want a well performing solver, it is NOT the
  // case in general.
  static constexpr u32 N_parameter = 200;
  static constexpr u32 K_parameter = 9;
  // Number of all bits used for collision finding.
  static constexpr u32 kTotalHashBits = N_parameter;
  // Number of bits in one hash segment. The segment is used as a
  // collision domain in one algorithm step.
  static constexpr u32 kHashSegmentBits = N_parameter / (K_parameter + 1);
  // A bitwise mask for reading value of one hash segment, provided
  // that the segment is aligned on byte boundary (otherwise
  // additional shift operation is needed).
  static constexpr u32 kHashSegmentBitMask = (1u << kHashSegmentBits) - 1;
  // Byte length of one hash segment when expanded to whole bytes.
  // See `kExpandHashes` for details.
  static constexpr u32 kHashSegmentBytes = (kHashSegmentBits + 7) / 8;
  // Number of hash segments in initially generated strings.
  static constexpr u32 kTotalSegmentsCount = (K_parameter + 1);
  // Number of initially generated strings.
  static constexpr u32 kInitialStringSetSize = 1u << (kHashSegmentBits + 1);
  // Number of indices in a solution.
  static constexpr u32 kSolutionSize = 1u << (kTotalSegmentsCount - 1);

  // ---
  // Algorithm parameters - they modify behaviour of the solver and
  // shape of used data structures. Try to understand the meaning
  // looking into the code before blindly changing the values. Not all
  // possible values are correct. Some bad options would be catched by
  // static assert statements, but the checks are not bullet-proof.
  // ---
  //
  // When true, generated hashes (strings) are expanded from 20bits
  // into 24bits to occupy 3B (highest 4bits == 0). When false
  // (default), the expansion is not performed and some space is
  // saved.
  static constexpr bool kExpandHashes = false;
  // If set, every found solution by the solver is recomputed from
  // scratch and checked. So 512 blake2b hashes is calculated and the
  // strings are then collided. Maybe, this is more a debugging
  // option.
  static constexpr bool kRecomputeSolution = false;
  // If set, output strings are filtered out if the first 64bits of
  // the produced string is 0. A probability of such a string randomly
  // is small but this can help to detect duplicates early. It turns
  // out that this check can cause more overhead then speedup (it is
  // on a very hot code path). Other filtering options probably do a
  // good job even without this one.
  static constexpr bool kFilterZeroQWordStrings = false;
  // Should be set in production! If not set, the code writes into
  // buckets without checking that the bucket is full so it can
  // overwrite strings at the beginning of the following bucket. It is
  // not really probable but it can happen. `kRecomputeSolution` must
  // be enabled then because some links can link to rewritten (and
  // therefore invalid) strings and not all solutions found must be
  // necessarily valid.
  // Interestingly, this "fail and recover" approach can lead to nice
  // performance. To disable this option for production code some
  // additional testing needs to be done.
  static constexpr bool kCheckBucketOverflow = true;
  // If enabled, candidate solutions found in step 8 are filtered out
  // if they share a value of the last segment with a previous
  // candidate. This optimization is based on fact that a probability
  // of two same string produced by different source strings is really
  // low but it allows to filter a lot of invalid candidate solutions
  // cheaply.
  static constexpr bool kStep8FilterByLastSegment = true;
  // TODO: Not properly supported in this version of the solver.
  // Do not change it.
  static constexpr bool kStoreIndicesEarly = false;
  // If set, use non-temporal store instructions to save indices. The
  // intention is to allow a CPU to NOT load cache lines corresponding
  // to the addresses where we write (we want to bypass cache at
  // all). There are quite strong condition on mem. access patterns
  // for this to work but they should be satisfied in this case.
  static constexpr bool kUseNonTemporalStoresForIndices = true;
  // If set the solver checks solution validity during step8
  // immediately after finding the candidate. If set to false,
  // solution candidates are collected similarly to output strings in
  // earlier steps and then processed together.
  static constexpr bool kProcessSolutionCandidateEarly = false;
  // Algorithm steps 0 .. (kUseTemporaryHashArrayBeforeStep - 1) uses
  // temporary array to store a part of first segment values for
  // second iteration over string. The temporary array helps to keep
  // working set smaller because it needs 2B per string instead of
  // e.g. 16B. It can happen that an overhead with writing this array
  // is greater so that we allow to specify which steps should use
  // this optimization and which not.
  static constexpr u32 kUseTemporaryHashArrayBeforeStep = 8;
  // Defines memory alignment of string objects. Must be a power of 2.
  static constexpr u64 kXStringAlignment = 4ul;
  // Alignment for XOR instructions when producing new strings. The
  // alignment affects which and how many instructions is used and can
  // measurably affect performance. Must be a power of 2.
  static constexpr u64 kXORAlignment = 4ul;
  // Multiplier and divisor for computing maximum number of strings the
  // solver can use in any algorithm step. The coefficients are related to
  // initial number of generated strings.
  static constexpr u64 kExtraSpaceMultiplier = 7;
  static constexpr u64 kExtraSpaceDivisor = 5;
  // Number of bits used for encoding a bucket. Directly defines number
  // of buckets used by the solver.
  static constexpr u64 kBucketCountBits = 8;
  // Number of bits from string's first segment not stored in the strings.
  // The bits are fully dependent on the string's position in some
  // particular bucket so they are redundant. Currently the code support
  // only bits aligned to whole bytes, and of course the parameter must be
  // smaller or equal to number of bits identifying a bucket
  // (`kBucketCountBits`).
  static constexpr u64 kFirstSegmentBitsSkipped = 8;
  // Starting bit position for bucket information within pair link
  // index (u32). The rest bits are used for source strings
  // identification. Not necessarily all bucket bits can be stored
  // into the pair link index (the rest are computed on-the-fly from
  // the link object position).
  static constexpr u32 kBucketInIndexShift = 26;
  // When a collision groups size is equal or greater then this value
  // the group is eliminated as a whole before producing any output
  // strings. This value is derived from a collision group size
  // distribution for random strings. This hugely helps to keep the
  // string set small and "healthy".
  static constexpr u32 kTooManyBasicCollisions = 14;
  static constexpr u32 kTooManyFinalCollisions = 3;
  // This option defines software pre-fetching behaviour during output
  // string generation in each step (collision groups processing). We
  // linearly go thru source string indices in an array and produce
  // output strings. We know which strings are to be needed in the
  // future because we have their indices in the array we iterate
  // thru, so we can try to prefetch them. The following number defines
  // a distance of the prefetch (in number of strings).
  static constexpr u64 kPrefetchDistance = 16;
  // Bytes allocated per one string for the whole algorithm run. This
  // memory is managed by a space allocator which allows to reallocate
  // memory blocks to different data structure quite cheaply. This
  // value is used if `kExpandHashes` == true.
  static constexpr u64 kMemoryForExpandedProblem = 76ul;
  // Bytes allocated per one string for `kExpandHashes` == false.
  static constexpr u64 kMemoryForNonExpandedProblem =
      68ul - (kFirstSegmentBitsSkipped ? 4 : 0);

  // ---
  // Debugging flags - should be always false if you're not debugging
  // a particular sub-system. Some options has huge performance
  // effect.
  // ---
  //
  // When enabled, all hashes are recomputed by reference
  // implementation and compared. Fatal error is raised when the
  // hashes differ.
  static constexpr bool kRecomputeHashesByRefImpl = false;
  // When a new string is produced (and new pair link is produced)
  // solution validation is called for the new pair link. This is VERY
  // time consuming but allows to find linking issues as early as
  // possible.
  static constexpr bool kValidatePartialSolutions = false;
  // Enable additional linking checks to help find bugs.
  static constexpr bool kCheckLinksConsistency = false;
  // Print distribution of collisions (sizes and counts of collision
  // groups) after each step of the algorithm.
  static constexpr bool kReportCollisions = false;
  // Print timings of algorithm steps to help with finding bad
  // performing parts.
  static constexpr bool kReportSteps = false;
  // Dump allocation of memory blocks managed by space allocator
  // whenever some allocation changes.
  static constexpr bool kReportMemoryAllocation = false;
  // Don't generate full set of 2^X strings but generate strings
  // forming exactly one solution (512 strings colliding in perfect
  // order). This helps with bugs identification since the solver can
  // run deterministically to one solution.  Debugging such a run is
  // easy and fast.
  static constexpr bool kGenerateTestSet = false;
  // When `kGenerateTestSet` and this is > 1, then we mix the single
  // prepared solution with random strings. The value specifies how
  // many times 512 strings is generated. Multiplier == 1 means no
  // additional strings, multiplier == 10 means one solution and 9x
  // 512 random strings. When > 1, `kValidatePartialSolutions` doesn't
  // produce correct results and `kRecomputeSolution` can produce
  // false negatives (due to string randomness).
  static constexpr u32 kTestSetExpandMultiplier = 1;

  // ---
  // Derived constants - cannot be changed manually.
  // ---
  //
  // Bitwise mask for reading bucket number from hash segment.
  static constexpr u64 kBucketNumberMask = (1u << kBucketCountBits) - 1;
  // Number of buckets used by the solver.
  static constexpr u16 kBucketCount = (1u << kBucketCountBits);
  // Maximum number of strings stored in one bucket.
  static constexpr u32 kItemsInBucket =
    kInitialStringSetSize * kExtraSpaceMultiplier / kExtraSpaceDivisor
    / kBucketCount;
  // Maximum value for encoding for two strings referenced by a pair
  // link. TODO: Use actual pairing function as used in the code.
  static constexpr u64 kMaxCompressedIndexValue =
      kItemsInBucket * (kItemsInBucket - 1) / 2 + kItemsInBucket - 1;
  // Is there enough space for source string encoding in the pair link
  // object? Or do we waste some space there?
  static_assert((1u << kBucketInIndexShift) > kMaxCompressedIndexValue, "");
  static_assert((1u << (kBucketInIndexShift - 1)) <= kMaxCompressedIndexValue,
		"");
  // A size of a collistion hash table denoted in bits.
  static constexpr u64 kHashTableSizeBits = kHashSegmentBits - kBucketCountBits;
  static constexpr u64 kHashTableMask = (1u << kHashTableSizeBits) - 1;
  static constexpr u16 kHashTableSize = (1u << kHashTableSizeBits);

  // Number of bits we need to reproduce by position of pair link
  // object (amount of information we don't have space for). We expect
  // 32bit pair link index.
  static constexpr u32 kPartitionCountBits =
      kBucketInIndexShift + kBucketCountBits - 32;
  // Number of bucket partitions forced by small pair link object.
  static constexpr u32 kPartitionCount = (1u << kPartitionCountBits);
  // Number of buckets we need to process to close one partition in
  // output strings.
  static constexpr u32 kBucketsPerPartition = kBucketCount / kPartitionCount;
  // Number of strings we can produce into one partition of an output
  // bucket. Other strings will be discarded.
  static constexpr u32 kItemsInOutPartition = kItemsInBucket / kPartitionCount;
  static_assert(kBucketsPerPartition * kPartitionCount == kBucketCount, "");
  // Theoretical maximum number of all strings when all buckets and
  // all partitions are fully used. This number is used mainly for
  // space allocation.
  static constexpr u64 kMaximumStringSetSize =
      kInitialStringSetSize * kExtraSpaceMultiplier / kExtraSpaceDivisor;

  // Sanity checks.
  static_assert(kExtraSpaceMultiplier > kExtraSpaceDivisor,
		"Some additional space is required");
  static_assert(kCheckBucketOverflow || kRecomputeSolution,
		"At least one of the options must be enabled");
  static_assert(kItemsInBucket <= 0xffff,
		"Items in bucket cannot fit into u16");
  static_assert(kPartitionCountBits < 10, "Expression overflowed");
  static_assert(kItemsInOutPartition * kPartitionCount <= kItemsInBucket,
		"Inconsistent partition vs. bucket configuration");
  static_assert((kItemsInOutPartition + 1) * kPartitionCount > kItemsInBucket,
		"Inconsistent partition vs. bucket configuration");
}

}  // namespace zceq_solver


#endif  // ZCEQ_CONFIG_H_

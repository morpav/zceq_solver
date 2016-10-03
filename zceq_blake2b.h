/* Copyright @ 2016 Pavel Moravec */
#ifndef ZCEQ_BLAKE_2_B_H_
#define ZCEQ_BLAKE_2_B_H_

#include <cstring>
#include <cassert>

#include "zceq_config.h"
#include "zceq_misc.h"

extern "C" {
#include "blake2/blake2.h"
#include "blake2/blake2-impl.h"

void Blake2PrepareMidstate4(void *midstate, unsigned char *input);
void Blake2Run4(unsigned char* hashout, void* midstate, uint32_t indexctr);
void Blake2PrepareMidstate2(void *midstate, unsigned char *input);
void Blake2Run2(unsigned char *hashout, void *midstate, uint32_t indexctr);
int blake2b_pick_best_implementation(void);
}

namespace zceq_solver {

// Initial vectors with Blake2b parameters xor-ed over.
static constexpr u64 personalized_state[8] {
    0x6a09e667f3bcc908ull ^ 0x1010032,
    0xbb67ae8584caa73bull,
    0x3c6ef372fe94f82bull,
    0xa54ff53a5f1d36f1ull,
    0x510e527fade682d1ull,
    0x9b05688c2b3e6c1full,
    0x1f83d9abfb41bd6bull ^ 0x576f50687361635aull,
    0x5be0cd19137e2179ull ^ 0x00000009000000c8ull,
};

using BatchHash = u64[8];

struct State {
  union {
    u64 h64[8];
    u8 hash[64];
  };
  u64 t[2];
  u64 f[2];
};

class BlakeBatchBackend {
 public:

  BlakeBatchBackend() {}

  virtual ~BlakeBatchBackend() {
    delete[] raw_memory_;
    raw_memory_ = nullptr;
  }

  // Precomputes internal state
  virtual void Precompute(const u8* header_and_nonce, u64 length,
                          const State* block0_state) = 0;

  // Returns number of hashes computed in one call.
  virtual u32 GetBatchSize() = 0;

  // Returns a memory where the object stores a newly computed hashes.
  // Hashes are a consecutive u64[8] memory blocks.
  virtual BatchHash* GetHashOutputMemory() = 0;

  // Compute the hash(es), starting from index `g_start`.
  virtual void Finalize(u32 g_start) = 0;

 protected:
  u8* AllocateAligned(u64 size) {
    assert(raw_memory_ == nullptr);
    raw_memory_ = new u8[size + 32];
    // Ensure that the memory allocated for batch blake computation is 32B-aligned.
    return (u8*)(((u64)(31 + raw_memory_)) & ~31);
  }
  // Store the allocated memory directly for proper delete[].
  u8* raw_memory_ = nullptr;
};

class alignas(32) Blake2b {
 public:
  inline Blake2b();
  ~Blake2b() {}

  void Precompute(const u8* header_and_nonce, u64 length) {
    if (length != 140) {
      fprintf(stderr, "Invalid block header length %ld (140 expected)\n", length);
      abort();
    }
    memcpy(prepared_state_.hash, personalized_state, 8 * 8);
    prepared_state_.t[0] = 128;
    prepared_state_.t[1] = prepared_state_.f[0] = prepared_state_.f[1] = 0;
    // Compress the first block - the data will never change
    CompressSingle(prepared_state_, header_and_nonce);

    // Update the data structure so that it seems like all data are already
    // in the state, ready to second compression.
    prepared_state_.t[0] = 144;
    prepared_state_.f[0] = -1ull;
    // Copy not already compressed part of the nonce to second block to be
    // compressed later.
    memcpy(second_block_.nonce_end, header_and_nonce + 128, 12);
    // Clear the reset of the second block.
    memset(second_block_.zeros, 0, sizeof(second_block_.zeros));

    // Initialize batch computation if possible
    if (batch_backend_ != nullptr)
      batch_backend_->Precompute(header_and_nonce, length, &prepared_state_);

    prepared_ = true;
  }

  inline void FinalizeInto(State& output, u32 g) {
    output = prepared_state_;
    second_block_.g = g;

    // Use scalar version of compress
    CompressSingle(output, second_block_.all_data);

    if (Const::kRecomputeHashesByRefImpl) {
      State control = prepared_state_;
      blake2b_compress_ref((blake2b_state*) &control, second_block_.all_data);
      for (auto i : range(8)) {
        if (control.h64[i] != output.h64[i]) {
          fprintf(stderr,
                  "Hash produced by vectorized Blake2b is NOT THE SAME! \n");
          abort();
        }
      }
    }
  }

  inline BatchHash* GetHashOutputMemory() {
    assert(batch_backend_ != nullptr);
    return batch_backend_->GetHashOutputMemory();
  }

  inline u32 GetBatchSize() {
    if (batch_backend_ == nullptr)
      return 0;
    return batch_backend_->GetBatchSize();
  }

  inline void BatchFinalize(u32 g_start) {
    assert(batch_backend_ != nullptr);
    batch_backend_->Finalize(g_start);

    if (Const::kRecomputeHashesByRefImpl) {
      BatchHash* hash64 = (BatchHash*)batch_backend_->GetHashOutputMemory();
      for (auto vec : range(batch_backend_->GetBatchSize())) {
        // Copy the prepared state to local variable since it will be demaged
        // by the computation.
        State control_output = prepared_state_;
        second_block_.g = g_start + vec;

        blake2b_compress_ref((blake2b_state*) &control_output, second_block_.all_data);
        // Compress4(&control_output, (u64*)second_block_.all_data);
        for (auto part : range(7)) {
          if (hash64[vec][part] != control_output.h64[part]) {
            fprintf(stderr,
                    "Hash produced by vectorized Blake2b is NOT THE SAME! \n");
            abort();
          }
        }
      }
    }
  }

 protected:
  void CompressSingle(State& state, const u8* data) {
    assert((u64)data % 32 == 0);
    blake2b_compress((blake2b_state*)&state, data);
  }

  alignas(32) union SecondBlock {
    struct {
      u32 nonce_end[3];
      u32 g;
      u32 zeros[28];
    };
    u8 all_data[128];
  } second_block_;

  static_assert(sizeof(SecondBlock) == 128, "sizeof(SecondBlock) != 128");

  State prepared_state_;
  BlakeBatchBackend* batch_backend_;
  bool prepared_ = false;
};


class IntrinsicsAVX2 : public BlakeBatchBackend {
 public:
  using Vectors8x4 = u64[8][4];

  IntrinsicsAVX2() {
    auto len = 4*256 + sizeof(SecondBlockNonZero4);
    auto mem = AllocateAligned(len);

    init_vectors_ = (Vectors8x4*)mem;
    hash_init_vectors_ = (Vectors8x4*)(mem + 256);
    hash_out_vectors_ = (Vectors8x4*)(mem + 512);
    hash_output_ = (BatchHash*)(mem + 768);
    second_block4_ = (SecondBlockNonZero4*)(mem + 1024);
  }

  virtual u32 GetBatchSize() {
    return 4;
  };

  virtual BatchHash* GetHashOutputMemory() {
    return hash_output_;
  };

  virtual void Precompute(const u8* header_and_nonce, u64 length,
                          const State* initial_state);
  virtual void Finalize(u32 g_start);

  union SecondBlockNonZero4 {
    u32 dwords[2][8];
    u64 blocks[2][4];
  };

 protected:
  SecondBlockNonZero4* second_block4_ = nullptr;
  BatchHash* hash_output_ = nullptr;
  Vectors8x4* init_vectors_ = nullptr;
  Vectors8x4* hash_init_vectors_ = nullptr;
  Vectors8x4* hash_out_vectors_ = nullptr;
};

class AsmAVX2 : public BlakeBatchBackend {
 public:
  AsmAVX2() {
    auto mem = AllocateAligned(512);
    prepared_state_ = mem;
    hash_output_ = (BatchHash*)(mem + 256);
  }
  virtual void Precompute(const u8* header_and_nonce, u64 length,
                          const State* block0_state) {
    assert(length == 140);
    Blake2PrepareMidstate4((void*)prepared_state_, (u8*)header_and_nonce);
  }
  virtual void Finalize(u32 g_start) {
    Blake2Run4((u8*)hash_output_, prepared_state_, g_start);
  }
  virtual u32 GetBatchSize() {
    return 4;
  }
  virtual BatchHash* GetHashOutputMemory() {
    return hash_output_;
  }

 protected:
  void* prepared_state_ = nullptr;
  BatchHash* hash_output_ = nullptr;
};


class AsmAVX1 : public BlakeBatchBackend {
 public:
  AsmAVX1() {
    auto mem = AllocateAligned(384);
    prepared_state_ = mem;
    hash_output_ = (BatchHash*)(mem + 256);
  }
  virtual void Precompute(const u8* header_and_nonce, u64 length,
                          const State* block0_state) {
    assert(length == 140);
    Blake2PrepareMidstate2((void*)prepared_state_, (u8*)header_and_nonce);
  }
  virtual void Finalize(u32 g_start) {
    Blake2Run2((u8*)hash_output_, prepared_state_, g_start);
  }
  virtual u32 GetBatchSize() {
    return 2;
  }
  virtual BatchHash* GetHashOutputMemory() {
    return hash_output_;
  }

 protected:
  void* prepared_state_ = nullptr;
  BatchHash* hash_output_ = nullptr;
};

inline Blake2b::Blake2b() {
  // Pick best implementation for scalar blake2b.
  blake2b_pick_best_implementation();

  // Pick best implementation for batch blake2b.
  if (HasAvx2Support()) {
    if (Const::kUseAsmAVX2Code)
      batch_backend_ = new AsmAVX2();
    else
      batch_backend_ = new IntrinsicsAVX2();

  } else if (HasAvx1Support()) {
    batch_backend_ = new AsmAVX1();
  } else
    batch_backend_ = nullptr;
}

}  // namespace zceq_solver

#endif  // ZCEQ_BLAKE_2_B_H_

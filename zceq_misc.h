/* Copyright @ 2016 Pavel Moravec */
#ifndef ZCEQ_MISC_H_
#define ZCEQ_MISC_H_

#include <cstdint>
#include <cstring>
#include <chrono>
#include <functional>
#include <cpuid.h>
#include <x86intrin.h>

#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

void __attribute__((weak)) AsmMarker() {}

namespace zceq_solver {

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using u64 = uint64_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u8 = uint8_t;

class ScopeTimer {
 public:
  ScopeTimer(std::function<void(u64)> callback) : callback_(callback) {
    start_ = std::chrono::high_resolution_clock::now();
  }
  ScopeTimer() { start_ = std::chrono::high_resolution_clock::now(); }

  ~ScopeTimer() {
    if (callback_) {
      auto stop = std::chrono::high_resolution_clock::now();
      callback_((u64)(stop - start_).count());
    }
  }
  void Reset() {
    start_ = std::chrono::high_resolution_clock::now();
  }
  u64 Sample() {
    auto now = std::chrono::high_resolution_clock::now();
    return (u64)(now - start_).count();
  }
  u64 Micro() {
    return Sample() / 1000;
  }

 private:
  std::chrono::high_resolution_clock::time_point start_;
  std::function<void(u64)> callback_;
};


template<typename T>
struct range_ {
  struct iterator {
    T value;
    bool operator <(const iterator& other) {
      return value < other.value;
    }
    bool operator !=(const iterator& other) {
      return value != other.value;
    }
    iterator& operator ++() {
      value++;
      return *this;
    }
    T operator*() {
      return value;
    }
  };
  range_(T end) : range_(0, end) {}
  range_(T begin, T end) : begin_(begin), end_(end) {
    if (end_ < begin_)
      end_ = begin_;
  }
  T begin_;
  T end_;
  iterator begin() {
    return {begin_};
  }
  iterator end() {
    return {end_};
  }
};

template<typename T>
inline range_<T> range(T end) {
  return {end};
}

template<typename T>
inline range_<T> range(T begin, T end) {
  return {begin, end};
}

static inline void XOR(u8* __restrict target,
                       u8* __restrict source1,
                       u8* __restrict source2, u64 length) {
  // Force(help) compiler to generate optimal, branch-less XOR instructions.
  for (auto i = 0; i < (length / 8); i++) {
    ((u64*)target)[i] = ((u64*)source1)[i] ^ ((u64*)source2)[i];
  }
  u64 shift = 0;
  if (length % 8 >= 4) {
    shift = (length / 8 * 8);
    *((u32*) (target + shift)) =
        *((u32*)(source1 + shift)) ^ *((u32*)(source2 + shift));
  }
  if (length % 4 >= 2) {
    shift = (length / 4 * 4);
    *((u16*) (target + shift)) =
        *((u16*)(source1 + shift)) ^ *((u16*)(source2 + shift));
  }
  if (length % 2 >= 1) {
    shift = (length / 2 * 2);
    *((u8*)(target + shift)) = *((u8*)(source1 + shift)) ^ *((u8*)(source2 + shift));
  }
}

// __attribute__((noinline))
static void ReorderBitsInHash(const u8* __restrict hash_,
                              u8* __restrict array_) {
  auto hash = (u64*)hash_;
  auto array = (u64*)array_;

  array[0] = ((hash[0] & 0x00ffffffff00ffff)) |
             ((hash[0] & 0xf000000000f00000) >> 4) |
             ((hash[0] & 0x0f000000000f0000) << 4);

  array[1] = ((hash[1] & 0xffffff00ffffffff)) |
             ((hash[1] & 0x000000f000000000) >> 4) |
             ((hash[1] & 0x0000000f00000000) << 4);

  array[2] = ((hash[2] & 0xff00ffffffff00ff)) |
             ((hash[2] & 0x00f000000000f000) >> 4) |
             ((hash[2] & 0x000f000000000f00) << 4);

  // Handle the last byte.
  // We intentionally copy a whole qword because compiler can vectorize
  // the computation then (32B vs 25B).
  array[3] = hash[3];
  // *(u8*)&array[3] = *(u8*)&hash[3];
}

class Random {
 public:
  Random() : state0_(16041983), state1_(42) {}
  Random(u64 state0) : state0_(state0), state1_(42) {}
  Random(u64 state0, u64 state1) : state0_(state0), state1_(state1) {}

  void InitializeState(u64 state0, u64 state1) {
    state0_ = state0;
    state1_ = state1;
  }

  // clang-format off
  u64 Next() {
    // clang-format on
    u64 s1 = state0_;
    u64 s0 = state1_;
    state0_ = s0;
    s1 ^= s1 << 23;
    s1 ^= s1 >> 17;
    s1 ^= s0;
    s1 ^= s0 >> 26;
    state1_ = s1;
    return state0_ + state1_;
  }

  void GetState(u64* state0, u64* state1) {
    if (state0) *state0 = state0_;
    if (state1) *state1 = state1_;
  }

 protected:
  u64 state0_;
  u64 state1_;
};


template<u64 length>
static inline void memcpy_nt(void* __restrict dest,
                             const void* __restrict source) {
  static_assert(length % 4 == 0, "Cannot copy objects not aligned to 4B.");

  for (auto part = 0; part < (length / 16); ++part) {
    _mm_stream_si128(((__m128i *)dest) + part,
                     *(((__m128i *)source) + part));
  }
  if (length % 16 > 0) {
    int part = (length / 16 * 16) / 8;
    _mm_stream_si64(
        reinterpret_cast<long long int*>(dest) + part,
        *(reinterpret_cast<const long long int*>(source) + part));
  }
  if (length % 8 > 0) {
    int part = (length / 8 * 8) / 4;
    _mm_stream_si32(
        reinterpret_cast<int*>(dest) + part,
        *(reinterpret_cast<const int*>(source) + part));
  }
}

template<u64 length>
static inline void memcpy_t(void* __restrict dest,
                            const void* __restrict source) {
  memcpy(dest, source, length);
}

struct CPUInfo {
  int eax;
  int ebx;
  int ecx;
  int edx;
};

static inline void cpuid(CPUInfo& info, int functionnumber) {
  int a, b, c, d;
  __asm("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(functionnumber), "c"(0) : );
  info.eax = a;
  info.ebx = b;
  info.ecx = c;
  info.edx = d;
}

static inline bool HasAvx2Support() {
  CPUInfo info;
  cpuid(info, 0x00000007);
  return (info.ebx & 0x20) != 0;
}

static inline bool HasAvx1Support() {
  CPUInfo info;
  cpuid(info, 0x00000001);
  return (info.ecx & 0x10000000) != 0;
}

static inline bool HasSSE41Support() {
  CPUInfo info;
  cpuid(info, 0x00000001);
  return (info.ecx & 0x80000) != 0;
}

static inline bool HasSSSE3Support() {
  CPUInfo info;
  cpuid(info, 0x00000001);
  return (info.ecx & 0x200) != 0;
}


}  // namespace zceq_solver

#endif  // ZCEQ_MISC_H_

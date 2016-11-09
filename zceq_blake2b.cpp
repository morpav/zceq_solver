/* Copyright @ 2016 Pavel Moravec */
#include <cassert>
#include <emmintrin.h>
#include <tmmintrin.h>
#include <smmintrin.h>
#include <immintrin.h>

#include "zceq_misc.h"
#include "zceq_blake2b.h"


namespace zceq_solver {

using YWord = __m256i;
using XWord = __m128i;

static constexpr u8 sigma[12][16] =
{
  {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 } ,
  { 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 } ,
  { 11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4 } ,
  {  7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8 } ,
  {  9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13 } ,
  {  2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9 } ,
  { 12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11 } ,
  { 13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10 } ,
  {  6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5 } ,
  { 10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13 , 0 } ,
  {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 } ,
  { 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 }
};

static constexpr u8 a[8] = { 0, 1, 2, 3, 0, 1, 2, 3};
static constexpr u8 b[8] = { 4, 5, 6, 7, 5, 6, 7, 4};
static constexpr u8 c[8] = { 8, 9,10,11,10,11, 8, 9};
static constexpr u8 d[8] = {12,13,14,15,15,12,13,14};


__attribute__((target("avx2")))
__attribute__((always_inline))
static inline void AddMessageAVX2(YWord& output, YWord input, const YWord* messages,
                                  u32 round, u32 g_iter, u32 g_4_7) {
  auto msg_offset = sigma[round][2 * g_iter + g_4_7];
  if (msg_offset < 2)
    output = input + messages[msg_offset];
  else
    output = input;
}

__attribute__((target("avx")))
__attribute__((always_inline))
static inline void AddMessageAVX1(XWord& output, XWord input, const XWord* messages,
                                  u32 round, u32 g_iter, u32 g_4_7) {
  auto msg_offset = sigma[round][2 * g_iter + g_4_7];
  if (msg_offset < 2)
    output = input + messages[msg_offset];
  else
    output = input;
}

__attribute__((target("sse2")))
__attribute__((always_inline))
static inline void AddMessageSSE2(XWord& output, XWord input, const XWord* messages,
                                  u32 round, u32 g_iter, u32 g_4_7) {
  auto msg_offset = sigma[round][2 * g_iter + g_4_7];
  if (msg_offset < 2)
    output = input + messages[msg_offset];
  else
    output = input;
}

__attribute__((target("avx2")))
__attribute__((always_inline))
__attribute__((unused))
static inline YWord Broadcast64(u64 value) {
  return _mm256_broadcastq_epi64(*(XWord*)&value);
}

template<u8 round, int shift>
__attribute__((target("avx2")))
__attribute__((always_inline))
static inline void G_sequence(const YWord* messages, YWord v[16]) {
  const auto rotate16 = _mm256_setr_epi8(2, 3, 4, 5, 6, 7, 0, 1,
                                         10, 11, 12, 13, 14, 15, 8, 9,
                                         2, 3, 4, 5, 6, 7, 0, 1,
                                         10, 11, 12, 13, 14, 15, 8, 9);

  const auto rotate24 = _mm256_setr_epi8(3, 4, 5, 6, 7, 0, 1, 2,
                                         11, 12, 13, 14, 15, 8, 9, 10,
                                         3, 4, 5, 6, 7, 0, 1, 2,
                                         11, 12, 13, 14, 15, 8, 9, 10);

  // a = a + b + m[blake2b_sigma[r][2*i+0]];
  for (auto i : range(shift, shift + 4))
    AddMessageAVX2(v[a[i]], v[a[i]] + v[b[i]], messages, round, i, 0);

  // d = rotr64(d ^ a, 32);
  for (auto i : range(shift, shift + 4))
    v[d[i]] = _mm256_shuffle_epi32(v[d[i]] ^ v[a[i]], _MM_SHUFFLE(2, 3, 0, 1));

  // c = c + d;
  for (auto i : range(shift, shift + 4))
    v[c[i]] = v[c[i]] + v[d[i]];

  // b = rotr64(b ^ c, 24);
  for (auto i : range(shift, shift + 4))
    v[b[i]] = _mm256_shuffle_epi8(v[b[i]] ^ v[c[i]], rotate24);

  // a = a + b + m[blake2b_sigma[r][2*i+1]];
  for (auto i : range(shift, shift + 4))
    AddMessageAVX2(v[a[i]], v[a[i]] + v[b[i]], messages, round, i, 1);

  // d = rotr64(d ^ a, 16);
  for (auto i : range(shift, shift + 4))
    v[d[i]] = _mm256_shuffle_epi8(v[d[i]] ^ v[a[i]], rotate16);

  // c = c + d;
  for (auto i : range(shift, shift + 4))
    v[c[i]] = v[c[i]] + v[d[i]];

  // b = rotr64(b ^ c, 63);
  for (auto i : range(shift, shift + 4)) {
    v[b[i]] = v[b[i]] ^ v[c[i]];
    v[b[i]] = _mm256_or_si256(_mm256_srli_epi64(v[b[i]], 63), v[b[i]] + v[b[i]]);
  }
}

template<u8 round, int shift>
__attribute__((target("avx")))
__attribute__((always_inline))
static inline void G_sequence_AVX1(const XWord* messages, XWord v[16]) {
  const auto rotate16 = _mm_setr_epi8(2, 3, 4, 5, 6, 7, 0, 1,
                                      10, 11, 12, 13, 14, 15, 8, 9);
  const auto rotate24 = _mm_setr_epi8(3, 4, 5, 6, 7, 0, 1, 2,
                                      11, 12, 13, 14, 15, 8, 9, 10 );

  // a = a + b + m[blake2b_sigma[r][2*i+0]];
  for (auto i : range(shift, shift + 4))
    AddMessageAVX1(v[a[i]], v[a[i]] + v[b[i]], messages, round, i, 0);

  // d = rotr64(d ^ a, 32);
  for (auto i : range(shift, shift + 4))
    v[d[i]] = _mm_shuffle_epi32(v[d[i]] ^ v[a[i]], _MM_SHUFFLE(2, 3, 0, 1));

  // c = c + d;
  for (auto i : range(shift, shift + 4))
    v[c[i]] = v[c[i]] + v[d[i]];

  // b = rotr64(b ^ c, 24);
  for (auto i : range(shift, shift + 4))
    v[b[i]] = _mm_shuffle_epi8(v[b[i]] ^ v[c[i]], rotate24);

  // a = a + b + m[blake2b_sigma[r][2*i+1]];
  for (auto i : range(shift, shift + 4))
    AddMessageAVX1(v[a[i]], v[a[i]] + v[b[i]], messages, round, i, 1);

  // d = rotr64(d ^ a, 16);
  for (auto i : range(shift, shift + 4))
    v[d[i]] = _mm_shuffle_epi8(v[d[i]] ^ v[a[i]], rotate16);

  // c = c + d;
  for (auto i : range(shift, shift + 4))
    v[c[i]] = v[c[i]] + v[d[i]];

  // b = rotr64(b ^ c, 63);
  for (auto i : range(shift, shift + 4)) {
    v[b[i]] = v[b[i]] ^ v[c[i]];
    v[b[i]] = _mm_xor_si128(_mm_srli_epi64(v[b[i]], 63), v[b[i]] + v[b[i]]);
  }
}

template<u8 round, int shift>
__attribute__((target("ssse3")))
__attribute__((always_inline))
static inline void G_sequence_SSSE3(const XWord* messages, XWord v[16]) {
  const auto rotate16 = _mm_setr_epi8(2, 3, 4, 5, 6, 7, 0, 1,
                                      10, 11, 12, 13, 14, 15, 8, 9);
  const auto rotate24 = _mm_setr_epi8(3, 4, 5, 6, 7, 0, 1, 2,
                                      11, 12, 13, 14, 15, 8, 9, 10 );

  // a = a + b + m[blake2b_sigma[r][2*i+0]];
  for (auto i : range(shift, shift + 4))
    AddMessageSSE2(v[a[i]], v[a[i]] + v[b[i]], messages, round, i, 0);

  // d = rotr64(d ^ a, 32);
  for (auto i : range(shift, shift + 4))
    v[d[i]] = _mm_shuffle_epi32(v[d[i]] ^ v[a[i]], _MM_SHUFFLE(2, 3, 0, 1));

  // c = c + d;
  for (auto i : range(shift, shift + 4))
    v[c[i]] = v[c[i]] + v[d[i]];

  // b = rotr64(b ^ c, 24);
  for (auto i : range(shift, shift + 4))
    v[b[i]] = _mm_shuffle_epi8(v[b[i]] ^ v[c[i]], rotate24);

  // a = a + b + m[blake2b_sigma[r][2*i+1]];
  for (auto i : range(shift, shift + 4))
    AddMessageSSE2(v[a[i]], v[a[i]] + v[b[i]], messages, round, i, 1);

  // d = rotr64(d ^ a, 16);
  for (auto i : range(shift, shift + 4))
    v[d[i]] = _mm_shuffle_epi8(v[d[i]] ^ v[a[i]], rotate16);

  // c = c + d;
  for (auto i : range(shift, shift + 4))
    v[c[i]] = v[c[i]] + v[d[i]];

  // b = rotr64(b ^ c, 63);
  for (auto i : range(shift, shift + 4)) {
    v[b[i]] = v[b[i]] ^ v[c[i]];
    v[b[i]] = _mm_xor_si128(_mm_srli_epi64(v[b[i]], 63), v[b[i]] + v[b[i]]);
  }
}

template<u8 round, int shift>
__attribute__((target("sse2")))
__attribute__((always_inline))
static inline void G_sequence_SSE2(const XWord* messages, XWord v[16]) {
  const auto rotate16 = _mm_setr_epi8(2, 3, 4, 5, 6, 7, 0, 1,
                                      10, 11, 12, 13, 14, 15, 8, 9);
  const auto rotate24 = _mm_setr_epi8(3, 4, 5, 6, 7, 0, 1, 2,
                                      11, 12, 13, 14, 15, 8, 9, 10 );

  // a = a + b + m[blake2b_sigma[r][2*i+0]];
  for (auto i : range(shift, shift + 4))
    AddMessageSSE2(v[a[i]], v[a[i]] + v[b[i]], messages, round, i, 0);

  // d = rotr64(d ^ a, 32);
  for (auto i : range(shift, shift + 4))
    v[d[i]] = _mm_shuffle_epi32(v[d[i]] ^ v[a[i]], _MM_SHUFFLE(2, 3, 0, 1));

  // c = c + d;
  for (auto i : range(shift, shift + 4))
    v[c[i]] = v[c[i]] + v[d[i]];

  // b = rotr64(b ^ c, 24);
  for (auto i : range(shift, shift + 4))
    v[b[i]] = _mm_shuffle_epi8(v[b[i]] ^ v[c[i]], rotate24);

  // a = a + b + m[blake2b_sigma[r][2*i+1]];
  for (auto i : range(shift, shift + 4))
    AddMessageSSE2(v[a[i]], v[a[i]] + v[b[i]], messages, round, i, 1);

  // d = rotr64(d ^ a, 16);
  for (auto i : range(shift, shift + 4))
    v[d[i]] = _mm_shuffle_epi8(v[d[i]] ^ v[a[i]], rotate16);

  // c = c + d;
  for (auto i : range(shift, shift + 4))
    v[c[i]] = v[c[i]] + v[d[i]];

  // b = rotr64(b ^ c, 63);
  for (auto i : range(shift, shift + 4)) {
    v[b[i]] = v[b[i]] ^ v[c[i]];
    v[b[i]] = _mm_xor_si128(_mm_srli_epi64(v[b[i]], 63), v[b[i]] + v[b[i]]);
  }
}

alignas(64) static const uint64_t blake2b_IV[8] =
{
  0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL,
  0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
  0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
  0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL
};

__attribute__((target("avx2")))
inline void Compress4IntAVX2(const YWord msgs[2], const YWord state_init[8], YWord h[8]) {
  YWord v[16];

  for (auto i : range(8))
    v[i] = h[i];

  for (auto i : range(8))
    v[i + 8] = state_init[i];

  G_sequence<0, 0>(msgs, v);
  G_sequence<0, 4>(msgs, v);
  G_sequence<1, 0>(msgs, v);
  G_sequence<1, 4>(msgs, v);
  G_sequence<2, 0>(msgs, v);
  G_sequence<2, 4>(msgs, v);
  G_sequence<3, 0>(msgs, v);
  G_sequence<3, 4>(msgs, v);
  G_sequence<4, 0>(msgs, v);
  G_sequence<4, 4>(msgs, v);
  G_sequence<5, 0>(msgs, v);
  G_sequence<5, 4>(msgs, v);
  G_sequence<6, 0>(msgs, v);
  G_sequence<6, 4>(msgs, v);
  G_sequence<7, 0>(msgs, v);
  G_sequence<7, 4>(msgs, v);
  G_sequence<8, 0>(msgs, v);
  G_sequence<8, 4>(msgs, v);
  G_sequence<9, 0>(msgs, v);
  G_sequence<9, 4>(msgs, v);
  G_sequence<10, 0>(msgs, v);
  G_sequence<10, 4>(msgs, v);
  G_sequence<11, 0>(msgs, v);
  G_sequence<11, 4>(msgs, v);

  for (auto i : range(8))
    h[i] = h[i] ^ v[i] ^ v[i + 8];
}

__attribute__((target("avx")))
inline void Compress2IntAVX1(const XWord msgs[2], const XWord state_init[8], XWord h[8]) {
  XWord v[16];

  for (auto i : range(8))
    v[i] = h[i];

  for (auto i : range(8))
    v[i + 8] = state_init[i];

  G_sequence_AVX1<0, 0>(msgs, v);
  G_sequence_AVX1<0, 4>(msgs, v);
  G_sequence_AVX1<1, 0>(msgs, v);
  G_sequence_AVX1<1, 4>(msgs, v);
  G_sequence_AVX1<2, 0>(msgs, v);
  G_sequence_AVX1<2, 4>(msgs, v);
  G_sequence_AVX1<3, 0>(msgs, v);
  G_sequence_AVX1<3, 4>(msgs, v);
  G_sequence_AVX1<4, 0>(msgs, v);
  G_sequence_AVX1<4, 4>(msgs, v);
  G_sequence_AVX1<5, 0>(msgs, v);
  G_sequence_AVX1<5, 4>(msgs, v);
  G_sequence_AVX1<6, 0>(msgs, v);
  G_sequence_AVX1<6, 4>(msgs, v);
  G_sequence_AVX1<7, 0>(msgs, v);
  G_sequence_AVX1<7, 4>(msgs, v);
  G_sequence_AVX1<8, 0>(msgs, v);
  G_sequence_AVX1<8, 4>(msgs, v);
  G_sequence_AVX1<9, 0>(msgs, v);
  G_sequence_AVX1<9, 4>(msgs, v);
  G_sequence_AVX1<10, 0>(msgs, v);
  G_sequence_AVX1<10, 4>(msgs, v);
  G_sequence_AVX1<11, 0>(msgs, v);
  G_sequence_AVX1<11, 4>(msgs, v);

  for (auto i : range(8))
    h[i] = h[i] ^ v[i] ^ v[i + 8];
}

__attribute__((target("ssse3")))
inline void Compress2IntSSSE3(const XWord msgs[2], const XWord state_init[8], XWord h[8]) {
  XWord v[16];

  for (auto i : range(8))
    v[i] = h[i];

  for (auto i : range(8))
    v[i + 8] = state_init[i];

  G_sequence_SSSE3<0, 0>(msgs, v);
  G_sequence_SSSE3<0, 4>(msgs, v);
  G_sequence_SSSE3<1, 0>(msgs, v);
  G_sequence_SSSE3<1, 4>(msgs, v);
  G_sequence_SSSE3<2, 0>(msgs, v);
  G_sequence_SSSE3<2, 4>(msgs, v);
  G_sequence_SSSE3<3, 0>(msgs, v);
  G_sequence_SSSE3<3, 4>(msgs, v);
  G_sequence_SSSE3<4, 0>(msgs, v);
  G_sequence_SSSE3<4, 4>(msgs, v);
  G_sequence_SSSE3<5, 0>(msgs, v);
  G_sequence_SSSE3<5, 4>(msgs, v);
  G_sequence_SSSE3<6, 0>(msgs, v);
  G_sequence_SSSE3<6, 4>(msgs, v);
  G_sequence_SSSE3<7, 0>(msgs, v);
  G_sequence_SSSE3<7, 4>(msgs, v);
  G_sequence_SSSE3<8, 0>(msgs, v);
  G_sequence_SSSE3<8, 4>(msgs, v);
  G_sequence_SSSE3<9, 0>(msgs, v);
  G_sequence_SSSE3<9, 4>(msgs, v);
  G_sequence_SSSE3<10, 0>(msgs, v);
  G_sequence_SSSE3<10, 4>(msgs, v);
  G_sequence_SSSE3<11, 0>(msgs, v);
  G_sequence_SSSE3<11, 4>(msgs, v);

  for (auto i : range(8))
    h[i] = h[i] ^ v[i] ^ v[i + 8];
}

__attribute__((target("sse2")))
inline void Compress2IntSSE2(const XWord msgs[2], const XWord state_init[8], XWord h[8]) {
  XWord v[16];

  for (auto i : range(8))
    v[i] = h[i];

  for (auto i : range(8))
    v[i + 8] = state_init[i];

  G_sequence_SSE2<0, 0>(msgs, v);
  G_sequence_SSE2<0, 4>(msgs, v);
  G_sequence_SSE2<1, 0>(msgs, v);
  G_sequence_SSE2<1, 4>(msgs, v);
  G_sequence_SSE2<2, 0>(msgs, v);
  G_sequence_SSE2<2, 4>(msgs, v);
  G_sequence_SSE2<3, 0>(msgs, v);
  G_sequence_SSE2<3, 4>(msgs, v);
  G_sequence_SSE2<4, 0>(msgs, v);
  G_sequence_SSE2<4, 4>(msgs, v);
  G_sequence_SSE2<5, 0>(msgs, v);
  G_sequence_SSE2<5, 4>(msgs, v);
  G_sequence_SSE2<6, 0>(msgs, v);
  G_sequence_SSE2<6, 4>(msgs, v);
  G_sequence_SSE2<7, 0>(msgs, v);
  G_sequence_SSE2<7, 4>(msgs, v);
  G_sequence_SSE2<8, 0>(msgs, v);
  G_sequence_SSE2<8, 4>(msgs, v);
  G_sequence_SSE2<9, 0>(msgs, v);
  G_sequence_SSE2<9, 4>(msgs, v);
  G_sequence_SSE2<10, 0>(msgs, v);
  G_sequence_SSE2<10, 4>(msgs, v);
  G_sequence_SSE2<11, 0>(msgs, v);
  G_sequence_SSE2<11, 4>(msgs, v);

  for (auto i : range(8))
    h[i] = h[i] ^ v[i] ^ v[i + 8];
}

template<u8 batch_size>
void IntrinsicsBackend<batch_size>::Precompute(const u8* header_and_nonce, u64 length,
                                               const State* state) {
  auto second_block_nonce = (u32*) (header_and_nonce + 128);
  // Prepare transposed vectorized version of non-zero parts of the second block.
  // It is used for batch computation of multiple hashes simultaneously.
  for (auto i : range(batch_size)) {
    second_blockN_->dwords[0][2 * i] = second_block_nonce[0];
    second_blockN_->dwords[0][2 * i + 1] = second_block_nonce[1];
    second_blockN_->dwords[1][2 * i] = second_block_nonce[2];
    // Space for g.
    second_blockN_->dwords[1][2 * i + 1] = 0;
  }

  for (auto i : range(batch_size)) {
    (*init_vectors_)[0][i] = blake2b_IV[0];
    (*init_vectors_)[1][i] = blake2b_IV[1];
    (*init_vectors_)[2][i] = blake2b_IV[2];
    (*init_vectors_)[3][i] = blake2b_IV[3];
    (*init_vectors_)[4][i] = (state->t[0] ^ blake2b_IV[4]);
    (*init_vectors_)[5][i] = (state->t[1] ^ blake2b_IV[5]);
    (*init_vectors_)[6][i] = (state->f[0] ^ blake2b_IV[6]);
    (*init_vectors_)[7][i] = (state->f[1] ^ blake2b_IV[7]);
  }

  for (auto vec : range(8)) {
    for (auto i : range(batch_size)) {
      (*hash_init_vectors_)[vec][i] = state->h64[vec];
    }
  }
}

__attribute__((target("avx2")))
void IntrinsicsAVX2::Finalize(u32 g_start) {
  // Fill g indices into the vectorized (transposed) block parts.
  for (auto i : range(kBatchSize))
    second_blockN_->dwords[1][2*i + 1] = g_start + i;

  memcpy(hash_out_vectors_, hash_init_vectors_, sizeof(Vectors8xN));

  // Compute 4 Blake2b hashes simultaneously!
  Compress4IntAVX2((YWord*)second_blockN_, (YWord*)init_vectors_, (YWord*)hash_out_vectors_);

  // Transpose the result hashes
  for (auto vec : range(kBatchSize))
    for (auto part : range(7))
      hash_output_[vec][part] = (*hash_out_vectors_)[part][vec];
}

__attribute__((target("avx")))
void IntrinsicsAVX1::Finalize(u32 g_start) {
  // Fill g indices into the vectorized (transposed) block parts.
  for (auto i : range(kBatchSize))
    second_blockN_->dwords[1][2*i + 1] = g_start + i;

  memcpy(hash_out_vectors_, hash_init_vectors_, sizeof(Vectors8xN));

  // Compute 2 Blake2b hashes simultaneously!
  Compress2IntAVX1((XWord*)second_blockN_, (XWord*)init_vectors_, (XWord*)hash_out_vectors_);

  // Transpose the result hashes
  for (auto vec : range(kBatchSize))
    for (auto part : range(7))
      hash_output_[vec][part] = (*hash_out_vectors_)[part][vec];
}

__attribute__((target("ssse3")))
void IntrinsicsSSSE3::Finalize(u32 g_start) {
  // Fill g indices into the vectorized (transposed) block parts.
  for (auto i : range(kBatchSize))
    second_blockN_->dwords[1][2*i + 1] = g_start + i;

  memcpy(hash_out_vectors_, hash_init_vectors_, sizeof(Vectors8xN));

  // Compute 2 Blake2b hashes simultaneously!
  Compress2IntSSSE3((XWord*)second_blockN_, (XWord*)init_vectors_, (XWord*)hash_out_vectors_);

  // Transpose the result hashes
  for (auto vec : range(kBatchSize))
    for (auto part : range(7))
      hash_output_[vec][part] = (*hash_out_vectors_)[part][vec];
}

__attribute__((target("sse2")))
void IntrinsicsSSE2::Finalize(u32 g_start) {
  // Fill g indices into the vectorized (transposed) block parts.
  for (auto i : range(kBatchSize))
    second_blockN_->dwords[1][2*i + 1] = g_start + i;

  memcpy(hash_out_vectors_, hash_init_vectors_, sizeof(Vectors8xN));

  // Compute 2 Blake2b hashes simultaneously!
  Compress2IntSSE2((XWord*)second_blockN_, (XWord*)init_vectors_, (XWord*)hash_out_vectors_);

  // Transpose the result hashes
  for (auto vec : range(kBatchSize))
    for (auto part : range(7))
      hash_output_[vec][part] = (*hash_out_vectors_)[part][vec];
}


}  // namespace zceq_solver

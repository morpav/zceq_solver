#if IS_ARM_NEON

#include <stdint.h>
#include <string.h>

#include "blake2.h"
#include "blake2-impl.h"
#include "common.h"

__attribute__((aligned(64))) static const uint64_t blake2b_IV[8] =
{
  0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL,
  0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
  0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
  0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL
};

static const uint8_t blake2b_sigma[12][16] =
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


int blake2b_compress_neon32(blake2b_state *S, const uint8_t block[BLAKE2B_BLOCKBYTES])
static void BLAKE2_NEON_Compress32(const byte* input, BLAKE2_State<word32, false>& state)
{
  CRYPTOPP_ALIGN_DATA(16) uint32_t m0[4], m1[4], m2[4], m3[4], m4[4], m5[4], m6[4], m7[4];
  CRYPTOPP_ALIGN_DATA(16) uint32_t m8[4], m9[4], m10[4], m11[4], m12[4], m13[4], m14[4], m15[4];

  GetBlock<word32, LittleEndian, true> get(input);
  get(m0[0])(m1[0])(m2[0])(m3[0])(m4[0])(m5[0])(m6[0])(m7[0])(m8[0])(m9[0])(m10[0])(m11[0])(m12[0])(m13[0])(m14[0])(m15[0]);

  uint32x4_t row1,row2,row3,row4;
  uint32x4_t buf1,buf2,buf3,buf4;
  uint32x4_t ff0,ff1;

  row1 = ff0 = vld1q_u32((const uint32_t*)&state.h[0]);
  row2 = ff1 = vld1q_u32((const uint32_t*)&state.h[4]);
  row3 = vld1q_u32((const uint32_t*)&BLAKE2S_IV(0));
  row4 = veorq_u32(vld1q_u32((const uint32_t*)&BLAKE2S_IV(4)), vld1q_u32((const uint32_t*)&state.t[0]));

  // buf1 = vld1q_u32(m6,m4,m2,m0);
  vld1q_u32_rev(buf1, m6,m4,m2,m0);

  row1 = vaddq_u32(vaddq_u32(row1,buf1),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,16),vshlq_n_u32(row4,16));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,12),vshlq_n_u32(row2,20));

  // buf2 = vld1q_u32(m7,m5,m3,m1);
  vld1q_u32_rev(buf2, m7,m5,m3,m1);

  row1 = vaddq_u32(vaddq_u32(row1,buf2),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,8),vshlq_n_u32(row4,24));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,7),vshlq_n_u32(row2,25));

  row4 = vextq_u32(row4,row4,3);
  row3 = vcombine_u32(vget_high_u32(row3),vget_low_u32(row3));
  row2 = vextq_u32(row2,row2,1);

  // buf3 = vld1q_u32(m14,m12,m10,m8);
  vld1q_u32_rev(buf3, m14,m12,m10,m8);

  row1 = vaddq_u32(vaddq_u32(row1,buf3),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,16),vshlq_n_u32(row4,16));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,12),vshlq_n_u32(row2,20));

  // buf4 = vld1q_u32(m15,m13,m11,m9);
  vld1q_u32_rev(buf4, m15,m13,m11,m9);

  row1 = vaddq_u32(vaddq_u32(row1,buf4),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,8),vshlq_n_u32(row4,24));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,7),vshlq_n_u32(row2,25));

  row4 = vextq_u32(row4,row4,1);
  row3 = vcombine_u32(vget_high_u32(row3),vget_low_u32(row3));
  row2 = vextq_u32(row2,row2,3);

  // buf1 = vld1q_u32(m13,m9,m4,m14);
  vld1q_u32_rev(buf1, m13,m9,m4,m14);

  row1 = vaddq_u32(vaddq_u32(row1,buf1),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,16),vshlq_n_u32(row4,16));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,12),vshlq_n_u32(row2,20));

  // buf2 = vld1q_u32(m6,m15,m8,m10);
  vld1q_u32_rev(buf2, m6,m15,m8,m10);

  row1 = vaddq_u32(vaddq_u32(row1,buf2),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,8),vshlq_n_u32(row4,24));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,7),vshlq_n_u32(row2,25));

  row4 = vextq_u32(row4,row4,3);
  row3 = vcombine_u32(vget_high_u32(row3),vget_low_u32(row3));
  row2 = vextq_u32(row2,row2,1);

  // buf3 = vld1q_u32(m5,m11,m0,m1);
  vld1q_u32_rev(buf3, m5,m11,m0,m1);

  row1 = vaddq_u32(vaddq_u32(row1,buf3),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,16),vshlq_n_u32(row4,16));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,12),vshlq_n_u32(row2,20));

  // buf4 = vld1q_u32(m3,m7,m2,m12);
  vld1q_u32_rev(buf4, m3,m7,m2,m12);

  row1 = vaddq_u32(vaddq_u32(row1,buf4),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,8),vshlq_n_u32(row4,24));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,7),vshlq_n_u32(row2,25));

  row4 = vextq_u32(row4,row4,1);
  row3 = vcombine_u32(vget_high_u32(row3),vget_low_u32(row3));
  row2 = vextq_u32(row2,row2,3);

  // buf1 = vld1q_u32(m15,m5,m12,m11);
  vld1q_u32_rev(buf1, m15,m5,m12,m11);

  row1 = vaddq_u32(vaddq_u32(row1,buf1),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,16),vshlq_n_u32(row4,16));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,12),vshlq_n_u32(row2,20));

  // buf2 = vld1q_u32(m13,m2,m0,m8);
  vld1q_u32_rev(buf2, m13,m2,m0,m8);

  row1 = vaddq_u32(vaddq_u32(row1,buf2),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,8),vshlq_n_u32(row4,24));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,7),vshlq_n_u32(row2,25));

  row4 = vextq_u32(row4,row4,3);
  row3 = vcombine_u32(vget_high_u32(row3),vget_low_u32(row3));
  row2 = vextq_u32(row2,row2,1);

  // buf3 = vld1q_u32(m9,m7,m3,m10);
  vld1q_u32_rev(buf3, m9,m7,m3,m10);

  row1 = vaddq_u32(vaddq_u32(row1,buf3),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,16),vshlq_n_u32(row4,16));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,12),vshlq_n_u32(row2,20));

  // buf4 = vld1q_u32(m4,m1,m6,m14);
  vld1q_u32_rev(buf4, m4,m1,m6,m14);

  row1 = vaddq_u32(vaddq_u32(row1,buf4),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,8),vshlq_n_u32(row4,24));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,7),vshlq_n_u32(row2,25));

  row4 = vextq_u32(row4,row4,1);
  row3 = vcombine_u32(vget_high_u32(row3),vget_low_u32(row3));
  row2 = vextq_u32(row2,row2,3);

  // buf1 = vld1q_u32(m11,m13,m3,m7);
  vld1q_u32_rev(buf1, m11,m13,m3,m7);

  row1 = vaddq_u32(vaddq_u32(row1,buf1),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,16),vshlq_n_u32(row4,16));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,12),vshlq_n_u32(row2,20));

  // buf2 = vld1q_u32(m14,m12,m1,m9);
  vld1q_u32_rev(buf2, m14,m12,m1,m9);

  row1 = vaddq_u32(vaddq_u32(row1,buf2),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,8),vshlq_n_u32(row4,24));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,7),vshlq_n_u32(row2,25));

  row4 = vextq_u32(row4,row4,3);
  row3 = vcombine_u32(vget_high_u32(row3),vget_low_u32(row3));
  row2 = vextq_u32(row2,row2,1);

  // buf3 = vld1q_u32(m15,m4,m5,m2);
  vld1q_u32_rev(buf3, m15,m4,m5,m2);

  row1 = vaddq_u32(vaddq_u32(row1,buf3),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,16),vshlq_n_u32(row4,16));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,12),vshlq_n_u32(row2,20));

  // buf4 = vld1q_u32(m8,m0,m10,m6);
  vld1q_u32_rev(buf4, m8,m0,m10,m6);

  row1 = vaddq_u32(vaddq_u32(row1,buf4),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,8),vshlq_n_u32(row4,24));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,7),vshlq_n_u32(row2,25));

  row4 = vextq_u32(row4,row4,1);
  row3 = vcombine_u32(vget_high_u32(row3),vget_low_u32(row3));
  row2 = vextq_u32(row2,row2,3);

  // buf1 = vld1q_u32(m10,m2,m5,m9);
  vld1q_u32_rev(buf1, m10,m2,m5,m9);

  row1 = vaddq_u32(vaddq_u32(row1,buf1),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,16),vshlq_n_u32(row4,16));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,12),vshlq_n_u32(row2,20));

  // buf2 = vld1q_u32(m15,m4,m7,m0);
  vld1q_u32_rev(buf2, m15,m4,m7,m0);

  row1 = vaddq_u32(vaddq_u32(row1,buf2),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,8),vshlq_n_u32(row4,24));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,7),vshlq_n_u32(row2,25));

  row4 = vextq_u32(row4,row4,3);
  row3 = vcombine_u32(vget_high_u32(row3),vget_low_u32(row3));
  row2 = vextq_u32(row2,row2,1);

  // buf3 = vld1q_u32(m3,m6,m11,m14);
  vld1q_u32_rev(buf3, m3,m6,m11,m14);

  row1 = vaddq_u32(vaddq_u32(row1,buf3),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,16),vshlq_n_u32(row4,16));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,12),vshlq_n_u32(row2,20));

  // buf4 = vld1q_u32(m13,m8,m12,m1);
  vld1q_u32_rev(buf4, m13,m8,m12,m1);

  row1 = vaddq_u32(vaddq_u32(row1,buf4),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,8),vshlq_n_u32(row4,24));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,7),vshlq_n_u32(row2,25));

  row4 = vextq_u32(row4,row4,1);
  row3 = vcombine_u32(vget_high_u32(row3),vget_low_u32(row3));
  row2 = vextq_u32(row2,row2,3);

  // buf1 = vld1q_u32(m8,m0,m6,m2);
  vld1q_u32_rev(buf1, m8,m0,m6,m2);

  row1 = vaddq_u32(vaddq_u32(row1,buf1),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,16),vshlq_n_u32(row4,16));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,12),vshlq_n_u32(row2,20));

  // buf2 = vld1q_u32(m3,m11,m10,m12);
  vld1q_u32_rev(buf2, m3,m11,m10,m12);

  row1 = vaddq_u32(vaddq_u32(row1,buf2),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,8),vshlq_n_u32(row4,24));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,7),vshlq_n_u32(row2,25));

  row4 = vextq_u32(row4,row4,3);
  row3 = vcombine_u32(vget_high_u32(row3),vget_low_u32(row3));
  row2 = vextq_u32(row2,row2,1);

  // buf3 = vld1q_u32(m1,m15,m7,m4);
  vld1q_u32_rev(buf3, m1,m15,m7,m4);

  row1 = vaddq_u32(vaddq_u32(row1,buf3),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,16),vshlq_n_u32(row4,16));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,12),vshlq_n_u32(row2,20));

  // buf4 = vld1q_u32(m9,m14,m5,m13);
  vld1q_u32_rev(buf4, m9,m14,m5,m13);

  row1 = vaddq_u32(vaddq_u32(row1,buf4),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,8),vshlq_n_u32(row4,24));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,7),vshlq_n_u32(row2,25));

  row4 = vextq_u32(row4,row4,1);
  row3 = vcombine_u32(vget_high_u32(row3),vget_low_u32(row3));
  row2 = vextq_u32(row2,row2,3);

  // buf1 = vld1q_u32(m4,m14,m1,m12);
  vld1q_u32_rev(buf1, m4,m14,m1,m12);

  row1 = vaddq_u32(vaddq_u32(row1,buf1),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,16),vshlq_n_u32(row4,16));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,12),vshlq_n_u32(row2,20));

  // buf2 = vld1q_u32(m10,m13,m15,m5);
  vld1q_u32_rev(buf2, m10,m13,m15,m5);

  row1 = vaddq_u32(vaddq_u32(row1,buf2),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,8),vshlq_n_u32(row4,24));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,7),vshlq_n_u32(row2,25));

  row4 = vextq_u32(row4,row4,3);
  row3 = vcombine_u32(vget_high_u32(row3),vget_low_u32(row3));
  row2 = vextq_u32(row2,row2,1);

  // buf3 = vld1q_u32(m8,m9,m6,m0);
  vld1q_u32_rev(buf3, m8,m9,m6,m0);

  row1 = vaddq_u32(vaddq_u32(row1,buf3),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,16),vshlq_n_u32(row4,16));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,12),vshlq_n_u32(row2,20));

  // buf4 = vld1q_u32(m11,m2,m3,m7);
  vld1q_u32_rev(buf4, m11,m2,m3,m7);

  row1 = vaddq_u32(vaddq_u32(row1,buf4),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,8),vshlq_n_u32(row4,24));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,7),vshlq_n_u32(row2,25));

  row4 = vextq_u32(row4,row4,1);
  row3 = vcombine_u32(vget_high_u32(row3),vget_low_u32(row3));
  row2 = vextq_u32(row2,row2,3);

  // buf1 = vld1q_u32(m3,m12,m7,m13);
  vld1q_u32_rev(buf1, m3,m12,m7,m13);

  row1 = vaddq_u32(vaddq_u32(row1,buf1),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,16),vshlq_n_u32(row4,16));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,12),vshlq_n_u32(row2,20));

  // buf2 = vld1q_u32(m9,m1,m14,m11);
  vld1q_u32_rev(buf2, m9,m1,m14,m11);

  row1 = vaddq_u32(vaddq_u32(row1,buf2),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,8),vshlq_n_u32(row4,24));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,7),vshlq_n_u32(row2,25));

  row4 = vextq_u32(row4,row4,3);
  row3 = vcombine_u32(vget_high_u32(row3),vget_low_u32(row3));
  row2 = vextq_u32(row2,row2,1);

  // buf3 = vld1q_u32(m2,m8,m15,m5);
  vld1q_u32_rev(buf3, m2,m8,m15,m5);

  row1 = vaddq_u32(vaddq_u32(row1,buf3),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,16),vshlq_n_u32(row4,16));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,12),vshlq_n_u32(row2,20));

  // buf4 = vld1q_u32(m10,m6,m4,m0);
  vld1q_u32_rev(buf4, m10,m6,m4,m0);

  row1 = vaddq_u32(vaddq_u32(row1,buf4),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,8),vshlq_n_u32(row4,24));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,7),vshlq_n_u32(row2,25));

  row4 = vextq_u32(row4,row4,1);
  row3 = vcombine_u32(vget_high_u32(row3),vget_low_u32(row3));
  row2 = vextq_u32(row2,row2,3);

  // buf1 = vld1q_u32(m0,m11,m14,m6);
  vld1q_u32_rev(buf1, m0,m11,m14,m6);

  row1 = vaddq_u32(vaddq_u32(row1,buf1),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,16),vshlq_n_u32(row4,16));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,12),vshlq_n_u32(row2,20));

  // buf2 = vld1q_u32(m8,m3,m9,m15);
  vld1q_u32_rev(buf2, m8,m3,m9,m15);

  row1 = vaddq_u32(vaddq_u32(row1,buf2),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,8),vshlq_n_u32(row4,24));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,7),vshlq_n_u32(row2,25));

  row4 = vextq_u32(row4,row4,3);
  row3 = vcombine_u32(vget_high_u32(row3),vget_low_u32(row3));
  row2 = vextq_u32(row2,row2,1);

  // buf3 = vld1q_u32(m10,m1,m13,m12);
  vld1q_u32_rev(buf3, m10,m1,m13,m12);

  row1 = vaddq_u32(vaddq_u32(row1,buf3),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,16),vshlq_n_u32(row4,16));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,12),vshlq_n_u32(row2,20));

  // buf4 = vld1q_u32(m5,m4,m7,m2);
  vld1q_u32_rev(buf4, m5,m4,m7,m2);

  row1 = vaddq_u32(vaddq_u32(row1,buf4),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,8),vshlq_n_u32(row4,24));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,7),vshlq_n_u32(row2,25));

  row4 = vextq_u32(row4,row4,1);
  row3 = vcombine_u32(vget_high_u32(row3),vget_low_u32(row3));
  row2 = vextq_u32(row2,row2,3);

  // buf1 = vld1q_u32(m1,m7,m8,m10);
  vld1q_u32_rev(buf1, m1,m7,m8,m10);

  row1 = vaddq_u32(vaddq_u32(row1,buf1),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,16),vshlq_n_u32(row4,16));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,12),vshlq_n_u32(row2,20));

  // buf2 = vld1q_u32(m5,m6,m4,m2);
  vld1q_u32_rev(buf2, m5,m6,m4,m2);

  row1 = vaddq_u32(vaddq_u32(row1,buf2),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,8),vshlq_n_u32(row4,24));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,7),vshlq_n_u32(row2,25));

  row4 = vextq_u32(row4,row4,3);
  row3 = vcombine_u32(vget_high_u32(row3),vget_low_u32(row3));
  row2 = vextq_u32(row2,row2,1);

  // buf3 = vld1q_u32(m13,m3,m9,m15);
  vld1q_u32_rev(buf3, m13,m3,m9,m15);

  row1 = vaddq_u32(vaddq_u32(row1,buf3),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,16),vshlq_n_u32(row4,16));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,12),vshlq_n_u32(row2,20));

  // buf4 = vld1q_u32(m0,m12,m14,m11);
  vld1q_u32_rev(buf4, m0,m12,m14,m11);

  row1 = vaddq_u32(vaddq_u32(row1,buf4),row2);
  row4 = veorq_u32(row4,row1);
  row4 = veorq_u32(vshrq_n_u32(row4,8),vshlq_n_u32(row4,24));
  row3 = vaddq_u32(row3,row4);
  row2 = veorq_u32(row2,row3);
  row2 = veorq_u32(vshrq_n_u32(row2,7),vshlq_n_u32(row2,25));

  row4 = vextq_u32(row4,row4,1);
  row3 = vcombine_u32(vget_high_u32(row3),vget_low_u32(row3));
  row2 = vextq_u32(row2,row2,3);

  vst1q_u32((uint32_t*)&state.h[0],veorq_u32(ff0,veorq_u32(row1,row3)));
  vst1q_u32((uint32_t*)&state.h[4],veorq_u32(ff1,veorq_u32(row2,row4)));
  
  return 0;
}
#endif // IS_ARM_NEON

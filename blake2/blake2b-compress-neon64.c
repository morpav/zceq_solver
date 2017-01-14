#include "../zceq_arch.h"

#if IS_ARM_NEON

#include <arm_neon.h>
#include <stdint.h>
#include <string.h>

#include "blake2.h"
#include "blake2-impl.h"
#include "common.h"

__attribute__((aligned(64))) static const uint64_t iv[8] =
{
  0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL,
  0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
  0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
  0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL
};
#define BLAKE2B_IV(n) iv[n]

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


int blake2b_compress_neon64(blake2b_state *S, const uint8_t block[BLAKE2B_BLOCKBYTES])
{
  // Keep things straight due to swapping. For a 128-bit vector, H64 denotes
  //   the high 64-bit vector, and L64 denotes the low 64-bit vector. The
  //   vectors are the same as returned by vget_high_u64 and vget_low_u64.
#define LANE_H64 1
#define LANE_L64 0

  uint64x2_t m0m1,m2m3,m4m5,m6m7,m8m9,m10m11,m12m13,m14m15;

    m0m1 = vreinterpretq_u64_u8(vld1q_u8(block+  0));
    m2m3 = vreinterpretq_u64_u8(vld1q_u8(block+ 16));
    m4m5 = vreinterpretq_u64_u8(vld1q_u8(block+ 32));
    m6m7 = vreinterpretq_u64_u8(vld1q_u8(block+ 48));
    m8m9 = vreinterpretq_u64_u8(vld1q_u8(block+ 64));
  m10m11 = vreinterpretq_u64_u8(vld1q_u8(block+ 80));
  m12m13 = vreinterpretq_u64_u8(vld1q_u8(block+ 96));
  m14m15 = vreinterpretq_u64_u8(vld1q_u8(block+112));

  uint64x2_t row1l, row1h, row2l, row2h;
  uint64x2_t row3l, row3h, row4l, row4h;
  uint64x2_t b0 = {0,0}, b1 = {0,0}, t0, t1;

  row1l = vld1q_u64((const uint64_t *) &S->h[0]);
  row1h = vld1q_u64((const uint64_t *) &S->h[2]);
  row2l = vld1q_u64((const uint64_t *) &S->h[4]);
  row2h = vld1q_u64((const uint64_t *) &S->h[6]);
  row3l = vld1q_u64((const uint64_t *)&BLAKE2B_IV(0));
  row3h = vld1q_u64((const uint64_t *)&BLAKE2B_IV(2));
  row4l = veorq_u64(vld1q_u64((const uint64_t *)&BLAKE2B_IV(4)), vld1q_u64((const uint64_t*) &S->t[0]));
  row4h = veorq_u64(vld1q_u64((const uint64_t *)&BLAKE2B_IV(6)), vld1q_u64((const uint64_t*) &S->f[0]));

  b0 = vsetq_lane_u64(vgetq_lane_u64(m0m1,LANE_L64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m2m3,LANE_L64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m4m5,LANE_L64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m6m7,LANE_L64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,32),vshlq_n_u64(row4l,32));
  row4h = veorq_u64(vshrq_n_u64(row4h,32),vshlq_n_u64(row4h,32));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,24),vshlq_n_u64(row2l,40));
  row2h = veorq_u64(vshrq_n_u64(row2h,24),vshlq_n_u64(row2h,40));

  b0 = vsetq_lane_u64(vgetq_lane_u64(m0m1,LANE_H64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m2m3,LANE_H64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m4m5,LANE_H64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m6m7,LANE_H64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,16),vshlq_n_u64(row4l,48));
  row4h = veorq_u64(vshrq_n_u64(row4h,16),vshlq_n_u64(row4h,48));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,63),vshlq_n_u64(row2l,1));
  row2h = veorq_u64(vshrq_n_u64(row2h,63),vshlq_n_u64(row2h,1));

  t0 = row4l, t1 = row2l, row4l = row3l, row3l = row3h, row3h = row4l;
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_H64),row4l,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_L64),row4l,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_L64),row4h,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_H64),row4h,LANE_L64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2l,LANE_H64),row2l,LANE_L64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_L64),row2l,LANE_H64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_H64),row2h,LANE_L64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(t1,LANE_L64),row2h,LANE_H64);

  b0 = vsetq_lane_u64(vgetq_lane_u64(m8m9,LANE_L64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m10m11,LANE_L64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m12m13,LANE_L64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m14m15,LANE_L64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,32),vshlq_n_u64(row4l,32));
  row4h = veorq_u64(vshrq_n_u64(row4h,32),vshlq_n_u64(row4h,32));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,24),vshlq_n_u64(row2l,40));
  row2h = veorq_u64(vshrq_n_u64(row2h,24),vshlq_n_u64(row2h,40));

  b0 = vsetq_lane_u64(vgetq_lane_u64(m8m9,LANE_H64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m10m11,LANE_H64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m12m13,LANE_H64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m14m15,LANE_H64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,16),vshlq_n_u64(row4l,48));
  row4h = veorq_u64(vshrq_n_u64(row4h,16),vshlq_n_u64(row4h,48));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,63),vshlq_n_u64(row2l,1));
  row2h = veorq_u64(vshrq_n_u64(row2h,63),vshlq_n_u64(row2h,1));

  t0 = row3l, row3l = row3h, row3h = t0, t0 = row2l, t1 = row4l;
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2l,LANE_L64),row2l,LANE_H64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_H64),row2l,LANE_L64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_L64),row2h,LANE_H64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_H64),row2h,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4l,LANE_H64),row4l,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_L64),row4l,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_H64),row4h,LANE_L64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(t1,LANE_L64),row4h,LANE_H64);

  b0 = vsetq_lane_u64(vgetq_lane_u64(m14m15,LANE_L64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m4m5,LANE_L64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m8m9,LANE_H64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m12m13,LANE_H64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,32),vshlq_n_u64(row4l,32));
  row4h = veorq_u64(vshrq_n_u64(row4h,32),vshlq_n_u64(row4h,32));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,24),vshlq_n_u64(row2l,40));
  row2h = veorq_u64(vshrq_n_u64(row2h,24),vshlq_n_u64(row2h,40));

  b0 = vsetq_lane_u64(vgetq_lane_u64(m10m11,LANE_L64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m8m9,LANE_L64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m14m15,LANE_H64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m6m7,LANE_L64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,16),vshlq_n_u64(row4l,48));
  row4h = veorq_u64(vshrq_n_u64(row4h,16),vshlq_n_u64(row4h,48));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,63),vshlq_n_u64(row2l,1));
  row2h = veorq_u64(vshrq_n_u64(row2h,63),vshlq_n_u64(row2h,1));

  t0 = row4l, t1 = row2l, row4l = row3l, row3l = row3h, row3h = row4l;
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_H64),row4l,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_L64),row4l,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_L64),row4h,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_H64),row4h,LANE_L64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2l,LANE_H64),row2l,LANE_L64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_L64),row2l,LANE_H64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_H64),row2h,LANE_L64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(t1,LANE_L64),row2h,LANE_H64);

  b0 = vsetq_lane_u64(vgetq_lane_u64(m0m1,LANE_H64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m0m1,LANE_L64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m10m11,LANE_H64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m4m5,LANE_H64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,32),vshlq_n_u64(row4l,32));
  row4h = veorq_u64(vshrq_n_u64(row4h,32),vshlq_n_u64(row4h,32));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,24),vshlq_n_u64(row2l,40));
  row2h = veorq_u64(vshrq_n_u64(row2h,24),vshlq_n_u64(row2h,40));

  b0 = vsetq_lane_u64(vgetq_lane_u64(m12m13,LANE_L64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m2m3,LANE_L64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m6m7,LANE_H64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m2m3,LANE_H64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,16),vshlq_n_u64(row4l,48));
  row4h = veorq_u64(vshrq_n_u64(row4h,16),vshlq_n_u64(row4h,48));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,63),vshlq_n_u64(row2l,1));
  row2h = veorq_u64(vshrq_n_u64(row2h,63),vshlq_n_u64(row2h,1));

  t0 = row3l, row3l = row3h, row3h = t0, t0 = row2l, t1 = row4l;
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2l,LANE_L64),row2l,LANE_H64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_H64),row2l,LANE_L64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_L64),row2h,LANE_H64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_H64),row2h,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4l,LANE_H64),row4l,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_L64),row4l,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_H64),row4h,LANE_L64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(t1,LANE_L64),row4h,LANE_H64);

  b0 = vsetq_lane_u64(vgetq_lane_u64(m10m11,LANE_H64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m12m13,LANE_L64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m4m5,LANE_H64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m14m15,LANE_H64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,32),vshlq_n_u64(row4l,32));
  row4h = veorq_u64(vshrq_n_u64(row4h,32),vshlq_n_u64(row4h,32));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,24),vshlq_n_u64(row2l,40));
  row2h = veorq_u64(vshrq_n_u64(row2h,24),vshlq_n_u64(row2h,40));

  b0 = vsetq_lane_u64(vgetq_lane_u64(m8m9,LANE_L64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m0m1,LANE_L64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m2m3,LANE_L64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m12m13,LANE_H64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,16),vshlq_n_u64(row4l,48));
  row4h = veorq_u64(vshrq_n_u64(row4h,16),vshlq_n_u64(row4h,48));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,63),vshlq_n_u64(row2l,1));
  row2h = veorq_u64(vshrq_n_u64(row2h,63),vshlq_n_u64(row2h,1));

  t0 = row4l, t1 = row2l, row4l = row3l, row3l = row3h, row3h = row4l;
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_H64),row4l,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_L64),row4l,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_L64),row4h,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_H64),row4h,LANE_L64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2l,LANE_H64),row2l,LANE_L64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_L64),row2l,LANE_H64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_H64),row2h,LANE_L64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(t1,LANE_L64),row2h,LANE_H64);

  b0 = vsetq_lane_u64(vgetq_lane_u64(m10m11,LANE_L64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m2m3,LANE_H64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m6m7,LANE_H64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m8m9,LANE_H64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,32),vshlq_n_u64(row4l,32));
  row4h = veorq_u64(vshrq_n_u64(row4h,32),vshlq_n_u64(row4h,32));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,24),vshlq_n_u64(row2l,40));
  row2h = veorq_u64(vshrq_n_u64(row2h,24),vshlq_n_u64(row2h,40));

  b0 = vsetq_lane_u64(vgetq_lane_u64(m14m15,LANE_L64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m6m7,LANE_L64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m0m1,LANE_H64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m4m5,LANE_L64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,16),vshlq_n_u64(row4l,48));
  row4h = veorq_u64(vshrq_n_u64(row4h,16),vshlq_n_u64(row4h,48));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,63),vshlq_n_u64(row2l,1));
  row2h = veorq_u64(vshrq_n_u64(row2h,63),vshlq_n_u64(row2h,1));

  t0 = row3l, row3l = row3h, row3h = t0, t0 = row2l, t1 = row4l;
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2l,LANE_L64),row2l,LANE_H64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_H64),row2l,LANE_L64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_L64),row2h,LANE_H64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_H64),row2h,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4l,LANE_H64),row4l,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_L64),row4l,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_H64),row4h,LANE_L64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(t1,LANE_L64),row4h,LANE_H64);

  b0 = vsetq_lane_u64(vgetq_lane_u64(m6m7,LANE_H64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m2m3,LANE_H64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m12m13,LANE_H64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m10m11,LANE_H64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,32),vshlq_n_u64(row4l,32));
  row4h = veorq_u64(vshrq_n_u64(row4h,32),vshlq_n_u64(row4h,32));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,24),vshlq_n_u64(row2l,40));
  row2h = veorq_u64(vshrq_n_u64(row2h,24),vshlq_n_u64(row2h,40));

  b0 = vsetq_lane_u64(vgetq_lane_u64(m8m9,LANE_H64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m0m1,LANE_H64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m12m13,LANE_L64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m14m15,LANE_L64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,16),vshlq_n_u64(row4l,48));
  row4h = veorq_u64(vshrq_n_u64(row4h,16),vshlq_n_u64(row4h,48));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,63),vshlq_n_u64(row2l,1));
  row2h = veorq_u64(vshrq_n_u64(row2h,63),vshlq_n_u64(row2h,1));

  t0 = row4l, t1 = row2l, row4l = row3l, row3l = row3h, row3h = row4l;
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_H64),row4l,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_L64),row4l,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_L64),row4h,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_H64),row4h,LANE_L64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2l,LANE_H64),row2l,LANE_L64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_L64),row2l,LANE_H64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_H64),row2h,LANE_L64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(t1,LANE_L64),row2h,LANE_H64);

  b0 = vsetq_lane_u64(vgetq_lane_u64(m2m3,LANE_L64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m4m5,LANE_H64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m4m5,LANE_L64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m14m15,LANE_H64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,32),vshlq_n_u64(row4l,32));
  row4h = veorq_u64(vshrq_n_u64(row4h,32),vshlq_n_u64(row4h,32));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,24),vshlq_n_u64(row2l,40));
  row2h = veorq_u64(vshrq_n_u64(row2h,24),vshlq_n_u64(row2h,40));

  b0 = vsetq_lane_u64(vgetq_lane_u64(m6m7,LANE_L64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m10m11,LANE_L64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m0m1,LANE_L64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m8m9,LANE_L64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,16),vshlq_n_u64(row4l,48));
  row4h = veorq_u64(vshrq_n_u64(row4h,16),vshlq_n_u64(row4h,48));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,63),vshlq_n_u64(row2l,1));
  row2h = veorq_u64(vshrq_n_u64(row2h,63),vshlq_n_u64(row2h,1));

  t0 = row3l, row3l = row3h, row3h = t0, t0 = row2l, t1 = row4l;
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2l,LANE_L64),row2l,LANE_H64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_H64),row2l,LANE_L64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_L64),row2h,LANE_H64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_H64),row2h,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4l,LANE_H64),row4l,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_L64),row4l,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_H64),row4h,LANE_L64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(t1,LANE_L64),row4h,LANE_H64);

  b0 = vsetq_lane_u64(vgetq_lane_u64(m8m9,LANE_H64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m4m5,LANE_H64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m2m3,LANE_L64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m10m11,LANE_L64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,32),vshlq_n_u64(row4l,32));
  row4h = veorq_u64(vshrq_n_u64(row4h,32),vshlq_n_u64(row4h,32));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,24),vshlq_n_u64(row2l,40));
  row2h = veorq_u64(vshrq_n_u64(row2h,24),vshlq_n_u64(row2h,40));

  b0 = vsetq_lane_u64(vgetq_lane_u64(m0m1,LANE_L64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m6m7,LANE_H64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m4m5,LANE_L64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m14m15,LANE_H64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,16),vshlq_n_u64(row4l,48));
  row4h = veorq_u64(vshrq_n_u64(row4h,16),vshlq_n_u64(row4h,48));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,63),vshlq_n_u64(row2l,1));
  row2h = veorq_u64(vshrq_n_u64(row2h,63),vshlq_n_u64(row2h,1));

  t0 = row4l, t1 = row2l, row4l = row3l, row3l = row3h, row3h = row4l;
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_H64),row4l,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_L64),row4l,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_L64),row4h,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_H64),row4h,LANE_L64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2l,LANE_H64),row2l,LANE_L64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_L64),row2l,LANE_H64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_H64),row2h,LANE_L64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(t1,LANE_L64),row2h,LANE_H64);

  b0 = vsetq_lane_u64(vgetq_lane_u64(m14m15,LANE_L64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m10m11,LANE_H64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m6m7,LANE_L64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m2m3,LANE_H64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,32),vshlq_n_u64(row4l,32));
  row4h = veorq_u64(vshrq_n_u64(row4h,32),vshlq_n_u64(row4h,32));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,24),vshlq_n_u64(row2l,40));
  row2h = veorq_u64(vshrq_n_u64(row2h,24),vshlq_n_u64(row2h,40));

  b0 = vsetq_lane_u64(vgetq_lane_u64(m0m1,LANE_H64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m12m13,LANE_L64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m8m9,LANE_L64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m12m13,LANE_H64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,16),vshlq_n_u64(row4l,48));
  row4h = veorq_u64(vshrq_n_u64(row4h,16),vshlq_n_u64(row4h,48));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,63),vshlq_n_u64(row2l,1));
  row2h = veorq_u64(vshrq_n_u64(row2h,63),vshlq_n_u64(row2h,1));

  t0 = row3l, row3l = row3h, row3h = t0, t0 = row2l, t1 = row4l;
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2l,LANE_L64),row2l,LANE_H64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_H64),row2l,LANE_L64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_L64),row2h,LANE_H64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_H64),row2h,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4l,LANE_H64),row4l,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_L64),row4l,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_H64),row4h,LANE_L64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(t1,LANE_L64),row4h,LANE_H64);

  b0 = vsetq_lane_u64(vgetq_lane_u64(m2m3,LANE_L64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m6m7,LANE_L64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m0m1,LANE_L64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m8m9,LANE_L64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,32),vshlq_n_u64(row4l,32));
  row4h = veorq_u64(vshrq_n_u64(row4h,32),vshlq_n_u64(row4h,32));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,24),vshlq_n_u64(row2l,40));
  row2h = veorq_u64(vshrq_n_u64(row2h,24),vshlq_n_u64(row2h,40));

  b0 = vsetq_lane_u64(vgetq_lane_u64(m12m13,LANE_L64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m10m11,LANE_L64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m10m11,LANE_H64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m2m3,LANE_H64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,16),vshlq_n_u64(row4l,48));
  row4h = veorq_u64(vshrq_n_u64(row4h,16),vshlq_n_u64(row4h,48));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,63),vshlq_n_u64(row2l,1));
  row2h = veorq_u64(vshrq_n_u64(row2h,63),vshlq_n_u64(row2h,1));

  t0 = row4l, t1 = row2l, row4l = row3l, row3l = row3h, row3h = row4l;
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_H64),row4l,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_L64),row4l,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_L64),row4h,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_H64),row4h,LANE_L64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2l,LANE_H64),row2l,LANE_L64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_L64),row2l,LANE_H64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_H64),row2h,LANE_L64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(t1,LANE_L64),row2h,LANE_H64);

  b0 = vsetq_lane_u64(vgetq_lane_u64(m4m5,LANE_L64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m6m7,LANE_H64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m14m15,LANE_H64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m0m1,LANE_H64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,32),vshlq_n_u64(row4l,32));
  row4h = veorq_u64(vshrq_n_u64(row4h,32),vshlq_n_u64(row4h,32));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,24),vshlq_n_u64(row2l,40));
  row2h = veorq_u64(vshrq_n_u64(row2h,24),vshlq_n_u64(row2h,40));

  b0 = vsetq_lane_u64(vgetq_lane_u64(m12m13,LANE_H64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m4m5,LANE_H64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m14m15,LANE_L64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m8m9,LANE_H64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,16),vshlq_n_u64(row4l,48));
  row4h = veorq_u64(vshrq_n_u64(row4h,16),vshlq_n_u64(row4h,48));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,63),vshlq_n_u64(row2l,1));
  row2h = veorq_u64(vshrq_n_u64(row2h,63),vshlq_n_u64(row2h,1));

  t0 = row3l, row3l = row3h, row3h = t0, t0 = row2l, t1 = row4l;
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2l,LANE_L64),row2l,LANE_H64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_H64),row2l,LANE_L64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_L64),row2h,LANE_H64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_H64),row2h,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4l,LANE_H64),row4l,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_L64),row4l,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_H64),row4h,LANE_L64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(t1,LANE_L64),row4h,LANE_H64);

  b0 = vsetq_lane_u64(vgetq_lane_u64(m12m13,LANE_L64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m0m1,LANE_H64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m14m15,LANE_L64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m4m5,LANE_L64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,32),vshlq_n_u64(row4l,32));
  row4h = veorq_u64(vshrq_n_u64(row4h,32),vshlq_n_u64(row4h,32));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,24),vshlq_n_u64(row2l,40));
  row2h = veorq_u64(vshrq_n_u64(row2h,24),vshlq_n_u64(row2h,40));

  b0 = vsetq_lane_u64(vgetq_lane_u64(m4m5,LANE_H64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m14m15,LANE_H64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m12m13,LANE_H64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m10m11,LANE_L64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,16),vshlq_n_u64(row4l,48));
  row4h = veorq_u64(vshrq_n_u64(row4h,16),vshlq_n_u64(row4h,48));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,63),vshlq_n_u64(row2l,1));
  row2h = veorq_u64(vshrq_n_u64(row2h,63),vshlq_n_u64(row2h,1));

  t0 = row4l, t1 = row2l, row4l = row3l, row3l = row3h, row3h = row4l;
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_H64),row4l,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_L64),row4l,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_L64),row4h,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_H64),row4h,LANE_L64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2l,LANE_H64),row2l,LANE_L64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_L64),row2l,LANE_H64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_H64),row2h,LANE_L64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(t1,LANE_L64),row2h,LANE_H64);

  b0 = vsetq_lane_u64(vgetq_lane_u64(m0m1,LANE_L64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m6m7,LANE_L64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m8m9,LANE_H64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m8m9,LANE_L64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,32),vshlq_n_u64(row4l,32));
  row4h = veorq_u64(vshrq_n_u64(row4h,32),vshlq_n_u64(row4h,32));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,24),vshlq_n_u64(row2l,40));
  row2h = veorq_u64(vshrq_n_u64(row2h,24),vshlq_n_u64(row2h,40));

  b0 = vsetq_lane_u64(vgetq_lane_u64(m6m7,LANE_H64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m2m3,LANE_H64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m2m3,LANE_L64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m10m11,LANE_H64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,16),vshlq_n_u64(row4l,48));
  row4h = veorq_u64(vshrq_n_u64(row4h,16),vshlq_n_u64(row4h,48));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,63),vshlq_n_u64(row2l,1));
  row2h = veorq_u64(vshrq_n_u64(row2h,63),vshlq_n_u64(row2h,1));

  t0 = row3l, row3l = row3h, row3h = t0, t0 = row2l, t1 = row4l;
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2l,LANE_L64),row2l,LANE_H64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_H64),row2l,LANE_L64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_L64),row2h,LANE_H64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_H64),row2h,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4l,LANE_H64),row4l,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_L64),row4l,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_H64),row4h,LANE_L64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(t1,LANE_L64),row4h,LANE_H64);

  b0 = vsetq_lane_u64(vgetq_lane_u64(m12m13,LANE_H64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m6m7,LANE_H64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m12m13,LANE_L64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m2m3,LANE_H64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,32),vshlq_n_u64(row4l,32));
  row4h = veorq_u64(vshrq_n_u64(row4h,32),vshlq_n_u64(row4h,32));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,24),vshlq_n_u64(row2l,40));
  row2h = veorq_u64(vshrq_n_u64(row2h,24),vshlq_n_u64(row2h,40));

  b0 = vsetq_lane_u64(vgetq_lane_u64(m10m11,LANE_H64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m14m15,LANE_L64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m0m1,LANE_H64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m8m9,LANE_H64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,16),vshlq_n_u64(row4l,48));
  row4h = veorq_u64(vshrq_n_u64(row4h,16),vshlq_n_u64(row4h,48));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,63),vshlq_n_u64(row2l,1));
  row2h = veorq_u64(vshrq_n_u64(row2h,63),vshlq_n_u64(row2h,1));

  t0 = row4l, t1 = row2l, row4l = row3l, row3l = row3h, row3h = row4l;
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_H64),row4l,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_L64),row4l,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_L64),row4h,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_H64),row4h,LANE_L64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2l,LANE_H64),row2l,LANE_L64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_L64),row2l,LANE_H64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_H64),row2h,LANE_L64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(t1,LANE_L64),row2h,LANE_H64);

  b0 = vsetq_lane_u64(vgetq_lane_u64(m4m5,LANE_H64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m14m15,LANE_H64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m8m9,LANE_L64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m2m3,LANE_L64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,32),vshlq_n_u64(row4l,32));
  row4h = veorq_u64(vshrq_n_u64(row4h,32),vshlq_n_u64(row4h,32));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,24),vshlq_n_u64(row2l,40));
  row2h = veorq_u64(vshrq_n_u64(row2h,24),vshlq_n_u64(row2h,40));

  b0 = vsetq_lane_u64(vgetq_lane_u64(m0m1,LANE_L64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m4m5,LANE_L64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m6m7,LANE_L64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m10m11,LANE_L64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,16),vshlq_n_u64(row4l,48));
  row4h = veorq_u64(vshrq_n_u64(row4h,16),vshlq_n_u64(row4h,48));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,63),vshlq_n_u64(row2l,1));
  row2h = veorq_u64(vshrq_n_u64(row2h,63),vshlq_n_u64(row2h,1));

  t0 = row3l, row3l = row3h, row3h = t0, t0 = row2l, t1 = row4l;
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2l,LANE_L64),row2l,LANE_H64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_H64),row2l,LANE_L64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_L64),row2h,LANE_H64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_H64),row2h,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4l,LANE_H64),row4l,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_L64),row4l,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_H64),row4h,LANE_L64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(t1,LANE_L64),row4h,LANE_H64);

  b0 = vsetq_lane_u64(vgetq_lane_u64(m6m7,LANE_L64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m14m15,LANE_L64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m10m11,LANE_H64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m0m1,LANE_L64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,32),vshlq_n_u64(row4l,32));
  row4h = veorq_u64(vshrq_n_u64(row4h,32),vshlq_n_u64(row4h,32));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,24),vshlq_n_u64(row2l,40));
  row2h = veorq_u64(vshrq_n_u64(row2h,24),vshlq_n_u64(row2h,40));

  b0 = vsetq_lane_u64(vgetq_lane_u64(m14m15,LANE_H64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m8m9,LANE_H64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m2m3,LANE_H64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m8m9,LANE_L64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,16),vshlq_n_u64(row4l,48));
  row4h = veorq_u64(vshrq_n_u64(row4h,16),vshlq_n_u64(row4h,48));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,63),vshlq_n_u64(row2l,1));
  row2h = veorq_u64(vshrq_n_u64(row2h,63),vshlq_n_u64(row2h,1));

  t0 = row4l, t1 = row2l, row4l = row3l, row3l = row3h, row3h = row4l;
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_H64),row4l,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_L64),row4l,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_L64),row4h,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_H64),row4h,LANE_L64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2l,LANE_H64),row2l,LANE_L64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_L64),row2l,LANE_H64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_H64),row2h,LANE_L64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(t1,LANE_L64),row2h,LANE_H64);

  b0 = vsetq_lane_u64(vgetq_lane_u64(m12m13,LANE_L64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m12m13,LANE_H64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m0m1,LANE_H64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m10m11,LANE_L64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,32),vshlq_n_u64(row4l,32));
  row4h = veorq_u64(vshrq_n_u64(row4h,32),vshlq_n_u64(row4h,32));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,24),vshlq_n_u64(row2l,40));
  row2h = veorq_u64(vshrq_n_u64(row2h,24),vshlq_n_u64(row2h,40));

  b0 = vsetq_lane_u64(vgetq_lane_u64(m2m3,LANE_L64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m6m7,LANE_H64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m4m5,LANE_L64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m4m5,LANE_H64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,16),vshlq_n_u64(row4l,48));
  row4h = veorq_u64(vshrq_n_u64(row4h,16),vshlq_n_u64(row4h,48));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,63),vshlq_n_u64(row2l,1));
  row2h = veorq_u64(vshrq_n_u64(row2h,63),vshlq_n_u64(row2h,1));

  t0 = row3l, row3l = row3h, row3h = t0, t0 = row2l, t1 = row4l;
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2l,LANE_L64),row2l,LANE_H64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_H64),row2l,LANE_L64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_L64),row2h,LANE_H64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_H64),row2h,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4l,LANE_H64),row4l,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_L64),row4l,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_H64),row4h,LANE_L64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(t1,LANE_L64),row4h,LANE_H64);

  b0 = vsetq_lane_u64(vgetq_lane_u64(m10m11,LANE_L64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m8m9,LANE_L64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m6m7,LANE_H64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m0m1,LANE_H64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,32),vshlq_n_u64(row4l,32));
  row4h = veorq_u64(vshrq_n_u64(row4h,32),vshlq_n_u64(row4h,32));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,24),vshlq_n_u64(row2l,40));
  row2h = veorq_u64(vshrq_n_u64(row2h,24),vshlq_n_u64(row2h,40));

  b0 = vsetq_lane_u64(vgetq_lane_u64(m2m3,LANE_L64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m4m5,LANE_L64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m6m7,LANE_L64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m4m5,LANE_H64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,16),vshlq_n_u64(row4l,48));
  row4h = veorq_u64(vshrq_n_u64(row4h,16),vshlq_n_u64(row4h,48));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,63),vshlq_n_u64(row2l,1));
  row2h = veorq_u64(vshrq_n_u64(row2h,63),vshlq_n_u64(row2h,1));

  t0 = row4l, t1 = row2l, row4l = row3l, row3l = row3h, row3h = row4l;
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_H64),row4l,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_L64),row4l,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_L64),row4h,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_H64),row4h,LANE_L64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2l,LANE_H64),row2l,LANE_L64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_L64),row2l,LANE_H64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_H64),row2h,LANE_L64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(t1,LANE_L64),row2h,LANE_H64);

  b0 = vsetq_lane_u64(vgetq_lane_u64(m14m15,LANE_H64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m8m9,LANE_H64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m2m3,LANE_H64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m12m13,LANE_H64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,32),vshlq_n_u64(row4l,32));
  row4h = veorq_u64(vshrq_n_u64(row4h,32),vshlq_n_u64(row4h,32));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,24),vshlq_n_u64(row2l,40));
  row2h = veorq_u64(vshrq_n_u64(row2h,24),vshlq_n_u64(row2h,40));

  b0 = vsetq_lane_u64(vgetq_lane_u64(m10m11,LANE_H64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m14m15,LANE_L64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m12m13,LANE_L64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m0m1,LANE_L64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,16),vshlq_n_u64(row4l,48));
  row4h = veorq_u64(vshrq_n_u64(row4h,16),vshlq_n_u64(row4h,48));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,63),vshlq_n_u64(row2l,1));
  row2h = veorq_u64(vshrq_n_u64(row2h,63),vshlq_n_u64(row2h,1));

  t0 = row3l, row3l = row3h, row3h = t0, t0 = row2l, t1 = row4l;
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2l,LANE_L64),row2l,LANE_H64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_H64),row2l,LANE_L64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_L64),row2h,LANE_H64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_H64),row2h,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4l,LANE_H64),row4l,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_L64),row4l,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_H64),row4h,LANE_L64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(t1,LANE_L64),row4h,LANE_H64);

  b0 = vsetq_lane_u64(vgetq_lane_u64(m0m1,LANE_L64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m2m3,LANE_L64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m4m5,LANE_L64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m6m7,LANE_L64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,32),vshlq_n_u64(row4l,32));
  row4h = veorq_u64(vshrq_n_u64(row4h,32),vshlq_n_u64(row4h,32));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,24),vshlq_n_u64(row2l,40));
  row2h = veorq_u64(vshrq_n_u64(row2h,24),vshlq_n_u64(row2h,40));

  b0 = vsetq_lane_u64(vgetq_lane_u64(m0m1,LANE_H64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m2m3,LANE_H64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m4m5,LANE_H64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m6m7,LANE_H64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,16),vshlq_n_u64(row4l,48));
  row4h = veorq_u64(vshrq_n_u64(row4h,16),vshlq_n_u64(row4h,48));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,63),vshlq_n_u64(row2l,1));
  row2h = veorq_u64(vshrq_n_u64(row2h,63),vshlq_n_u64(row2h,1));

  t0 = row4l, t1 = row2l, row4l = row3l, row3l = row3h, row3h = row4l;
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_H64),row4l,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_L64),row4l,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_L64),row4h,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_H64),row4h,LANE_L64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2l,LANE_H64),row2l,LANE_L64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_L64),row2l,LANE_H64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_H64),row2h,LANE_L64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(t1,LANE_L64),row2h,LANE_H64);

  b0 = vsetq_lane_u64(vgetq_lane_u64(m8m9,LANE_L64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m10m11,LANE_L64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m12m13,LANE_L64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m14m15,LANE_L64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,32),vshlq_n_u64(row4l,32));
  row4h = veorq_u64(vshrq_n_u64(row4h,32),vshlq_n_u64(row4h,32));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,24),vshlq_n_u64(row2l,40));
  row2h = veorq_u64(vshrq_n_u64(row2h,24),vshlq_n_u64(row2h,40));

  b0 = vsetq_lane_u64(vgetq_lane_u64(m8m9,LANE_H64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m10m11,LANE_H64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m12m13,LANE_H64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m14m15,LANE_H64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,16),vshlq_n_u64(row4l,48));
  row4h = veorq_u64(vshrq_n_u64(row4h,16),vshlq_n_u64(row4h,48));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,63),vshlq_n_u64(row2l,1));
  row2h = veorq_u64(vshrq_n_u64(row2h,63),vshlq_n_u64(row2h,1));

  t0 = row3l, row3l = row3h, row3h = t0, t0 = row2l, t1 = row4l;
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2l,LANE_L64),row2l,LANE_H64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_H64),row2l,LANE_L64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_L64),row2h,LANE_H64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_H64),row2h,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4l,LANE_H64),row4l,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_L64),row4l,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_H64),row4h,LANE_L64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(t1,LANE_L64),row4h,LANE_H64);

  b0 = vsetq_lane_u64(vgetq_lane_u64(m14m15,LANE_L64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m4m5,LANE_L64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m8m9,LANE_H64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m12m13,LANE_H64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,32),vshlq_n_u64(row4l,32));
  row4h = veorq_u64(vshrq_n_u64(row4h,32),vshlq_n_u64(row4h,32));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,24),vshlq_n_u64(row2l,40));
  row2h = veorq_u64(vshrq_n_u64(row2h,24),vshlq_n_u64(row2h,40));

  b0 = vsetq_lane_u64(vgetq_lane_u64(m10m11,LANE_L64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m8m9,LANE_L64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m14m15,LANE_H64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m6m7,LANE_L64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,16),vshlq_n_u64(row4l,48));
  row4h = veorq_u64(vshrq_n_u64(row4h,16),vshlq_n_u64(row4h,48));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,63),vshlq_n_u64(row2l,1));
  row2h = veorq_u64(vshrq_n_u64(row2h,63),vshlq_n_u64(row2h,1));

  t0 = row4l, t1 = row2l, row4l = row3l, row3l = row3h, row3h = row4l;
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_H64),row4l,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_L64),row4l,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_L64),row4h,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_H64),row4h,LANE_L64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2l,LANE_H64),row2l,LANE_L64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_L64),row2l,LANE_H64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_H64),row2h,LANE_L64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(t1,LANE_L64),row2h,LANE_H64);

  b0 = vsetq_lane_u64(vgetq_lane_u64(m0m1,LANE_H64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m0m1,LANE_L64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m10m11,LANE_H64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m4m5,LANE_H64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,32),vshlq_n_u64(row4l,32));
  row4h = veorq_u64(vshrq_n_u64(row4h,32),vshlq_n_u64(row4h,32));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,24),vshlq_n_u64(row2l,40));
  row2h = veorq_u64(vshrq_n_u64(row2h,24),vshlq_n_u64(row2h,40));

  b0 = vsetq_lane_u64(vgetq_lane_u64(m12m13,LANE_L64),b0,LANE_L64);
  b0 = vsetq_lane_u64(vgetq_lane_u64(m2m3,LANE_L64),b0,LANE_H64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m6m7,LANE_H64),b1,LANE_L64);
  b1 = vsetq_lane_u64(vgetq_lane_u64(m2m3,LANE_H64),b1,LANE_H64);
  row1l = vaddq_u64(vaddq_u64(row1l, b0), row2l);
  row1h = vaddq_u64(vaddq_u64(row1h, b1), row2h);
  row4l = veorq_u64(row4l, row1l);
  row4h = veorq_u64(row4h, row1h);
  row4l = veorq_u64(vshrq_n_u64(row4l,16),vshlq_n_u64(row4l,48));
  row4h = veorq_u64(vshrq_n_u64(row4h,16),vshlq_n_u64(row4h,48));
  row3l = vaddq_u64(row3l, row4l);
  row3h = vaddq_u64(row3h, row4h);
  row2l = veorq_u64(row2l, row3l);
  row2h = veorq_u64(row2h, row3h);
  row2l = veorq_u64(vshrq_n_u64(row2l,63),vshlq_n_u64(row2l,1));
  row2h = veorq_u64(vshrq_n_u64(row2h,63),vshlq_n_u64(row2h,1));

  t0 = row3l, row3l = row3h, row3h = t0, t0 = row2l, t1 = row4l;
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2l,LANE_L64),row2l,LANE_H64);
  row2l = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_H64),row2l,LANE_L64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(row2h,LANE_L64),row2h,LANE_H64);
  row2h = vsetq_lane_u64(vgetq_lane_u64(t0,LANE_H64),row2h,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4l,LANE_H64),row4l,LANE_L64);
  row4l = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_L64),row4l,LANE_H64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(row4h,LANE_H64),row4h,LANE_L64);
  row4h = vsetq_lane_u64(vgetq_lane_u64(t1,LANE_L64),row4h,LANE_H64);

  row1l = veorq_u64(row3l, row1l);
  row1h = veorq_u64(row3h, row1h);
  vst1q_u64((uint64_t*) &S->h[0], veorq_u64(vld1q_u64((const uint64_t*) &S->h[0]), row1l));
  vst1q_u64((uint64_t*) &S->h[2], veorq_u64(vld1q_u64((const uint64_t*) &S->h[2]), row1h));

  row2l = veorq_u64(row4l, row2l);
  row2h = veorq_u64(row4h, row2h);
  vst1q_u64((uint64_t*) &S->h[4], veorq_u64(vld1q_u64((const uint64_t*) &S->h[4]), row2l));
  vst1q_u64((uint64_t*) &S->h[6], veorq_u64(vld1q_u64((const uint64_t*) &S->h[6]), row2h));
  
  return 0;
}
#endif // CRYPTOPP_BOOL_NEON_INTRINSICS_AVAILABLE

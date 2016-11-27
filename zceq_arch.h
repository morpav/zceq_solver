#ifndef ZCEQ_ARCH_H_
#define ZCEQ_ARCH_H_

#if defined(__i386__) && defined(__x86_64__)
  typedef __m128i int128_t;
  typedef __m128u uint128_t;
  #define IS_ARM_NEON 0
  #define IS_X86 1
#elif defined(__ARM_NEON__) || defined(__ARM_NEON) || defined(_M_ARM)
  #define IS_ARM_NEON 1
  #define IS_X86 0
#else
  #define IS_ARM_NEON 0
  #define IS_X86 0
#endif

#endif //ZCEQ_ARCH_H_

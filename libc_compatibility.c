#include <stddef.h>
void *__memcpy_glibc_2_2_5(void *, const void *, size_t);

// Map old variant of mecpy for compatibility reasons. NOTE: it is not
// possible to directly map __wrap_memcpy symbol to a specific mecpy
// version and omit the __wrap_memcpy definition. The symbol will
// always be missing.
asm(".symver __memcpy_glibc_2_2_5, memcpy@GLIBC_2.2.5");

void *__wrap_memcpy(void *dest, const void *src, size_t n)
{
  return __memcpy_glibc_2_2_5(dest, src, n);
}

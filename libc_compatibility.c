
// Force linker to  link against older libc for compatibility reasons.
asm(".symver __wrap_memcpy, memcpy@GLIBC_2.2.5");

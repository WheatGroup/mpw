#define CPUSUPPORT_X86_CPUID 1
#define CPUSUPPORT_X86_SSE2 1
#define CPUSUPPORT_X86_AESNI 1
#ifdef cpusupport_dummy
export CFLAGS_X86_AESNI="-maes"
#endif

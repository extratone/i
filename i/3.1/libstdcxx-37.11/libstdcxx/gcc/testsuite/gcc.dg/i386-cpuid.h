/* Helper file for i386 platform.  Runtime check for MMX/SSE/SSE2 support.
   Used by 20020523-2.c and i386-sse-6.c, and possibly others.  */
/* Plagarized from 20020523-2.c.  */

/* %ecx */
#define bit_SSE3 (1 << 0)

/* %edx */
#define bit_CMOV (1 << 15)
#define bit_MMX (1 << 23)
#define bit_SSE (1 << 25)
#define bit_SSE2 (1 << 26)

#ifndef NOINLINE
#define NOINLINE __attribute__ ((noinline))
#endif

static inline unsigned int
i386_get_cpuid (unsigned int *ecx, unsigned int *edx)
{
  int fl1;

#ifndef __x86_64__
  int fl2;

  /* See if we can use cpuid.  On AMD64 we always can.  */
  __asm__ ("pushfl; pushfl; popl %0; movl %0,%1; xorl %2,%0;"
	   "pushl %0; popfl; pushfl; popl %0; popfl"
	   : "=&r" (fl1), "=&r" (fl2)
	   : "i" (0x00200000));
  if (((fl1 ^ fl2) & 0x00200000) == 0)
    return (0);
#endif

  /* Host supports cpuid.  See if cpuid gives capabilities, try
     CPUID(0).  Preserve %ebx and %ecx; cpuid insn clobbers these, we
     don't need their CPUID values here, and %ebx may be the PIC
     register.  */
#ifdef __x86_64__
  __asm__ ("pushq %%rcx; pushq %%rbx; cpuid; popq %%rbx; popq %%rcx"
	   : "=a" (fl1) : "0" (0) : "rdx", "cc");
#else
  __asm__ ("pushl %%ecx; pushl %%ebx; cpuid; popl %%ebx; popl %%ecx"
	   : "=a" (fl1) : "0" (0) : "edx", "cc");
#endif
  if (fl1 == 0)
    return (0);

  /* Invoke CPUID(1), return %ecx and %edx; caller can examine bits to
     determine what's supported.  */
#ifdef __x86_64__
  __asm__ ("pushq %%rbx; cpuid; popq %%rbx"
	   : "=c" (*ecx), "=d" (*edx), "=a" (fl1) : "2" (1) : "cc");
#else
  __asm__ ("pushl %%ebx; cpuid; popl %%ebx"
	   : "=c" (*ecx), "=d" (*edx), "=a" (fl1) : "2" (1) : "cc");
#endif

  return 1;
}

unsigned int i386_cpuid_ecx (void) NOINLINE;
unsigned int i386_cpuid_edx (void) NOINLINE;

unsigned int NOINLINE
i386_cpuid_ecx (void)
{
  unsigned int ecx, edx;
  if (i386_get_cpuid (&ecx, &edx))
    return ecx;
  else
    return 0;
}

unsigned int NOINLINE
i386_cpuid_edx (void)
{
  unsigned int ecx, edx;
  if (i386_get_cpuid (&ecx, &edx))
    return edx;
  else
    return 0;
}

static inline unsigned int
i386_cpuid (void)
{
  return i386_cpuid_edx ();
}

/* PR rtl-optimization/29841 */
/* Testcase by Khem Raj <raj.khem@gmail.com> */

/* { dg-do compile } */
/* { dg-options "-O2" } */
/* { dg-options "-O2 -mtune=i586" { target i?86-*-* } } */
/* APPLE LOCAL */
/* { dg-require-effective-target ilp32 } */

typedef void (*fp)(void);
extern char* bar(void* a1, int a2);
extern char* mar(int n);
char* cptr;

void foo()
{
  cptr = mar(6);
  ((char *(*)(void *,int (*)(void *,unsigned char **),char**))((fp)bar))(0,0,(void*)(0)); /* { dg-warning "" "non-compatible type" } */
}

/* APPLE LOCAL file constant strings */
/* Test if constant CFStrings get placed in the correct section.  */
/* Contributed by Ziemowit Laski <zlaski@apple.com>  */

/* { dg-options "-fconstant-cfstrings" } */
/* { dg-do compile { target *-*-darwin* } } */
/* { dg-skip-if "" { *-*-darwin* } { "-m64" } { "" } } */

typedef const struct __CFString * CFStringRef;
static CFStringRef appKey = (CFStringRef) @"com.apple.soundpref";

/* { dg-final { scan-assembler ".section __DATA, __cfstring" } } */
/* { dg-final { scan-assembler ".long\t___CFConstantStringClassReference\n\t.long\t1992\n\t.long\t.*\n\t.long\t19\n\t.data" } } */

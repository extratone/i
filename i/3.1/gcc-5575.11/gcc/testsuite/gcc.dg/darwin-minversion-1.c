/* APPLE LOCAL file mainline 2007-02-20 5005743 */
/* Basic test for -mmacosx-version-min switch on Darwin.  */
/* { dg-options "-mmacosx-version-min=10.1" } */
/* { dg-do run { target powerpc*-*-darwin* i?86*-*-darwin* } } */

int main(void)
{
#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ != 1010
  fail me;
#endif
  return 0;
}

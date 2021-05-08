/* Copyright (C) 2004 Free Software Foundation.

   Check that exp10, exp10f, exp10l, exp2, exp2f, exp2l, pow10, pow10f,
   pow10l, expm1, expm1f and expm1l built-in functions compile.

   Written by Uros Bizjak, 13th February 2004.  */

/* { dg-do compile } */
/* { dg-options "-O2 -ffast-math" } */

extern double exp10(double);
extern double exp2(double);
extern double pow10(double);
extern double expm1(double);
extern double ldexp(double, int);
extern float exp10f(float);
extern float exp2f(float);
extern float pow10f(float);
extern float expm1f(float);
extern float ldexpf(float, int);
extern long double exp10l(long double);
extern long double exp2l(long double);
extern long double pow10l(long double);
extern long double expm1l(long double);
extern long double ldexpl(long double, int);


double test1(double x)
{
  return exp10(x);
}

double test2(double x)
{
  return exp2(x);
}

double test3(double x)
{
  return pow10(x);
}

double test4(double x)
{
  return expm1(x);
}

double test5(double x, int exp)
{
  return ldexp(x, exp);
}

float test1f(float x)
{
  return exp10f(x);
}

float test2f(float x)
{
  return exp2f(x);
}

float test3f(float x)
{
  return pow10f(x);
}

float test4f(float x)
{
  return expm1f(x);
}

float test5f(float x, int exp)
{
  return ldexpf(x, exp);
}

long double test1l(long double x)
{
  return exp10l(x);
}

long double test2l(long double x)
{
  return exp2l(x);
}

long double test3l(long double x)
{
  return pow10l(x);
}

long double test4l(long double x)
{
  return expm1l(x);
}

long double test5l(long double x, int exp)
{
  return ldexpl(x, exp);
}

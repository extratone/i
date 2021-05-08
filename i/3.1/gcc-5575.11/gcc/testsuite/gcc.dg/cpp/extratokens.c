/* Copyright (C) 2000 Free Software Foundation, Inc.  */

/* { dg-do preprocess } */
/* APPLE LOCAL -Wextra-tokens */
/* { dg-options "-fno-show-column -Wextra-tokens" } */

/* Tests all directives that do not permit excess tokens at the end of
   the line.  */

/* Source: Neil Booth, 4 Dec 2000.  The combination of separate test
   cases.  */

#ifdef foo bar  /* { dg-error "extra tokens" "tokens after #ifdef" } */
#endif

#ifndef foo bar  /* { dg-error "extra tokens" "tokens after #ifndef" } */
#endif

#if 1 
#if 0
#else foo	/* { dg-warning "extra tokens" "tokens after #else" } */
#endif /	/* { dg-warning "extra tokens" "tokens after #endif" } */
#endif

#undef foo bar  /* { dg-error "extra tokens" "tokens after #undef" } */

#assert foo(bar) bar /* { dg-error "extra tokens" "tokens after #assert" } */

#unassert foo(bar) b /* { dg-error "extra tokens" "tokens after #unassert" } */

#include "mi1c.h" bar /* { dg-error "extra tokens" "tokens after #include" } */

#ident "something" bar /* { dg-error "extra tokens" "tokens after #ident" } */

# 36 "file.c" 3

/* ... but in a system header, it's acceptable.  */
#ifdef KERNEL
#endif KERNEL  /* { dg-bogus "extra tokens" "bad warning" } */

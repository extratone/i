/* Copyright (C) 2002 Free Software Foundation, Inc.  */

/* { dg-do preprocess } */
/* { dg-options "-Wno-endif-labels -pedantic" } */

/* APPLE LOCAL -Wextra-tokens */
/* { dg-options "-Wextra-tokens -Wno-endif-labels -pedantic" { target *-apple-darwin* } } */
/* Tests combinations of -pedantic and -Wno-endif-labels; see extratokens2.c
   for more general tests.  */

/* Source: Phil Edwards, 25 Mar 2002.  Copied from endif-pedantic1.c and
   modified.  */

#if 1 
#if 0
#else foo	/* { dg-error "extra tokens" "tokens after #else" } */
#endif /	/* { dg-error "extra tokens" "tokens after #endif" } */
#endif


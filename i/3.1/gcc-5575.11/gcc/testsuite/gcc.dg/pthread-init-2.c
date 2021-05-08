/* Various Solaris versions have been known to have problems with the
 * pthread initialization macros when __STDC__ is defined.
 *
 * Origin: Kaveh Ghazi (ghazi@caip.rutgers.edu) 9/27/2006.
 */

/* { dg-do compile } */
/* { dg-options "-Wextra -Wall -ansi" } */
/* { dg-options "-Wextra -Wall -ansi -D_POSIX_C_SOURCE=199506L" { target { *-*-hpux* *-*-solaris2.5.1 } } } */

#include "pthread-init-common.h"


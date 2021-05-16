/* Native support for GNU/Linux, for GDB, the GNU debugger.
   Copyright 1999, 2000
   Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* Pick reasonable defaults for the number of real-time signals.  */

#ifndef REALTIME_LO
#define REALTIME_LO 32
#endif
#ifndef REALTIME_HI
#define REALTIME_HI 64
#endif

/* We need this file for the SOLIB_TRAMPOLINE stuff. */

/* If PC is in a shared library trampoline code, return the PC
   where the function itself actually starts.  If not, return 0.  */

#define SKIP_TRAMPOLINE_CODE(pc)  find_solib_trampoline_target (pc)

#include "solib.h"		/* Support for shared libraries. */

/* Definitions to target GDB to GNU/Linux on ia64.

   Copyright 1992, 1993, 2000, 2004, 2005 Free Software Foundation, Inc.

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

#ifndef TM_LINUX_H
#define TM_LINUX_H

#include "config/tm-linux.h"

extern int ia64_linux_in_sigtramp (CORE_ADDR pc, char *func_name);
#define DEPRECATED_IN_SIGTRAMP(pc,func_name) ia64_linux_in_sigtramp (pc, func_name)

#endif /* #ifndef TM_LINUX_H */

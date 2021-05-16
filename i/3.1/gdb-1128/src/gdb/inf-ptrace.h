/* Low level child interface to ptrace.

   Copyright 2004, 2005 Free Software Foundation, Inc.

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

#ifndef INF_PTRACE_H
#define INF_PTRACE_H

/* Create a prototype ptrace target.  The client can override it with
   local methods.  */

extern struct target_ops *inf_ptrace_target (void);

/* Create a "traditional" ptrace target.  REGISTER_U_OFFSET should be
   a function returning the offset within the user area where a
   particular register is stored.  */

extern struct target_ops *
  inf_ptrace_trad_target (CORE_ADDR (*register_u_offset)(int));

#endif

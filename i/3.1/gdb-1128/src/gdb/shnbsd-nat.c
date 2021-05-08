/* Native-dependent code for SuperH running NetBSD, for GDB.

   Copyright 2002, 2003, 2004 Free Software Foundation, Inc.
   Contributed by Wasabi Systems, Inc.

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

#include <sys/types.h>
#include <sys/ptrace.h>
#include <machine/reg.h>

#include "defs.h"
#include "inferior.h"

#include "sh-tdep.h"
#include "shnbsd-tdep.h"

/* Determine if PT_GETREGS fetches this register. */
#define GETREGS_SUPPLIES(regno) \
  (((regno) >= R0_REGNUM && (regno) <= (R0_REGNUM + 15)) \
|| (regno) == PC_REGNUM || (regno) == PR_REGNUM \
|| (regno) == MACH_REGNUM || (regno) == MACL_REGNUM \
|| (regno) == SR_REGNUM)

void
fetch_inferior_registers (int regno)
{
  if (regno == -1 || GETREGS_SUPPLIES (regno))
    {
      struct reg inferior_registers;

      if (ptrace (PT_GETREGS, PIDGET (inferior_ptid),
		  (PTRACE_TYPE_ARG3) &inferior_registers, 0) == -1)
	perror_with_name (_("Couldn't get registers"));

      shnbsd_supply_reg ((char *) &inferior_registers, regno);

      if (regno != -1)
	return;
    }
}

void
store_inferior_registers (int regno)
{
  if (regno == -1 || GETREGS_SUPPLIES (regno))
    {
      struct reg inferior_registers;

      if (ptrace (PT_GETREGS, PIDGET (inferior_ptid),
		  (PTRACE_TYPE_ARG3) &inferior_registers, 0) == -1)
	perror_with_name (_("Couldn't get registers"));

      shnbsd_fill_reg ((char *) &inferior_registers, regno);

      if (ptrace (PT_SETREGS, PIDGET (inferior_ptid),
		  (PTRACE_TYPE_ARG3) &inferior_registers, 0) == -1)
	perror_with_name (_("Couldn't set registers"));

      if (regno != -1)
	return;
    }
}

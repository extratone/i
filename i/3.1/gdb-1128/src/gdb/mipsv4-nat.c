/* Native support for MIPS running SVR4, for GDB.
   Copyright 1994, 1995, 2000, 2001 Free Software Foundation, Inc.

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

#include "defs.h"
#include "inferior.h"
#include "gdbcore.h"
#include "target.h"
#include "regcache.h"

#include <sys/time.h>
#include <sys/procfs.h>
#include <setjmp.h>		/* For JB_XXX.  */

/* Prototypes for supply_gregset etc. */
#include "gregset.h"

/* Size of elements in jmpbuf */

#define JB_ELEMENT_SIZE 4

/*
 * See the comment in m68k-tdep.c regarding the utility of these functions.
 *
 * These definitions are from the MIPS SVR4 ABI, so they may work for
 * any MIPS SVR4 target.
 */

void
supply_gregset (gregset_t *gregsetp)
{
  int regi;
  greg_t *regp = &(*gregsetp)[0];
  char zerobuf[MAX_REGISTER_SIZE];
  memset (zerobuf, 0, MAX_REGISTER_SIZE);

  for (regi = 0; regi <= CXT_RA; regi++)
    regcache_raw_supply (current_regcache, regi, (char *) (regp + regi));

  regcache_raw_supply (current_regcache, mips_regnum (current_gdbarch)->pc,
		       (char *) (regp + CXT_EPC));
  regcache_raw_supply (current_regcache, mips_regnum (current_gdbarch)->hi,
		       (char *) (regp + CXT_MDHI));
  regcache_raw_supply (current_regcache, mips_regnum (current_gdbarch)->lo,
		       (char *) (regp + CXT_MDLO));
  regcache_raw_supply (current_regcache, mips_regnum (current_gdbarch)->cause,
		       (char *) (regp + CXT_CAUSE));

  /* Fill inaccessible registers with zero.  */
  regcache_raw_supply (current_regcache, MIPS_PS_REGNUM, zerobuf);
  regcache_raw_supply (current_regcache,
		       mips_regnum (current_gdbarch)->badvaddr,
		       zerobuf);
  regcache_raw_supply (current_regcache, DEPRECATED_FP_REGNUM, zerobuf);
  regcache_raw_supply (current_regcache, MIPS_UNUSED_REGNUM, zerobuf);
  for (regi = MIPS_FIRST_EMBED_REGNUM; regi <= MIPS_LAST_EMBED_REGNUM; regi++)
    regcache_raw_supply (current_regcache, regi, zerobuf);
}

void
fill_gregset (gregset_t *gregsetp, int regno)
{
  int regi;
  greg_t *regp = &(*gregsetp)[0];

  for (regi = 0; regi <= 32; regi++)
    if ((regno == -1) || (regno == regi))
      *(regp + regi) = *(greg_t *) & deprecated_registers[DEPRECATED_REGISTER_BYTE (regi)];

  if ((regno == -1) || (regno == mips_regnum (current_gdbarch)->pc))
    *(regp + CXT_EPC) = *(greg_t *) & deprecated_registers[DEPRECATED_REGISTER_BYTE (mips_regnum (current_gdbarch)->pc)];

  if ((regno == -1) || (regno == mips_regnum (current_gdbarch)->cause))
    *(regp + CXT_CAUSE) = *(greg_t *) & deprecated_registers[DEPRECATED_REGISTER_BYTE (mips_regnum (current_gdbarch)->cause)];

  if ((regno == -1) || (regno == mips_regnum (current_gdbarch)->hi))
    *(regp + CXT_MDHI) = *(greg_t *) & deprecated_registers[DEPRECATED_REGISTER_BYTE (mips_regnum (current_gdbarch)->hi)];

  if ((regno == -1) || (regno == mips_regnum (current_gdbarch)->lo))
    *(regp + CXT_MDLO) = *(greg_t *) & deprecated_registers[DEPRECATED_REGISTER_BYTE (mips_regnum (current_gdbarch)->lo)];
}

/*
 * Now we do the same thing for floating-point registers.
 * We don't bother to condition on FP0 regnum since any
 * reasonable MIPS configuration has an R3010 in it.
 *
 * Again, see the comments in m68k-tdep.c.
 */

void
supply_fpregset (fpregset_t *fpregsetp)
{
  int regi;
  char zerobuf[MAX_REGISTER_SIZE];
  memset (zerobuf, 0, MAX_REGISTER_SIZE);

  for (regi = 0; regi < 32; regi++)
    regcache_raw_supply (current_regcache,
			 mips_regnum (current_gdbarch)->fp0 + regi,
			 (char *) &fpregsetp->fp_r.fp_regs[regi]);

  regcache_raw_supply (current_regcache,
		       mips_regnum (current_gdbarch)->fp_control_status,
		       (char *) &fpregsetp->fp_csr);

  /* FIXME: how can we supply FCRIR?  The ABI doesn't tell us. */
  regcache_raw_supply (current_regcache,
		       mips_regnum (current_gdbarch)->fp_implementation_revision,
		       zerobuf);
}

void
fill_fpregset (fpregset_t *fpregsetp, int regno)
{
  int regi;
  char *from, *to;

  for (regi = mips_regnum (current_gdbarch)->fp0;
       regi < mips_regnum (current_gdbarch)->fp0 + 32; regi++)
    {
      if ((regno == -1) || (regno == regi))
	{
	  from = (char *) &deprecated_registers[DEPRECATED_REGISTER_BYTE (regi)];
	  to = (char *) &(fpregsetp->fp_r.fp_regs[regi - mips_regnum (current_gdbarch)->fp0]);
	  memcpy (to, from, register_size (current_gdbarch, regi));
	}
    }

  if ((regno == -1)
      || (regno == mips_regnum (current_gdbarch)->fp_control_status))
    fpregsetp->fp_csr = *(unsigned *) &deprecated_registers[DEPRECATED_REGISTER_BYTE (mips_regnum (current_gdbarch)->fp_control_status)];
}


/* Figure out where the longjmp will land.
   We expect the first arg to be a pointer to the jmp_buf structure from which
   we extract the pc (_JB_PC) that we will land at.  The pc is copied into PC.
   This routine returns true on success. */

int
get_longjmp_target (CORE_ADDR *pc)
{
  char *buf;
  CORE_ADDR jb_addr;

  buf = alloca (TARGET_PTR_BIT / TARGET_CHAR_BIT);
  jb_addr = read_register (MIPS_A0_REGNUM);

  if (target_read_memory (jb_addr + _JB_PC * JB_ELEMENT_SIZE, buf,
			  TARGET_PTR_BIT / TARGET_CHAR_BIT))
    return 0;

  *pc = extract_unsigned_integer (buf, TARGET_PTR_BIT / TARGET_CHAR_BIT);

  return 1;
}

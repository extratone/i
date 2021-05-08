/* Mac OS X support for GDB, the GNU debugger.
   Copyright 1997, 1998, 1999, 2000, 2001, 2002
   Free Software Foundation, Inc.

   Contributed by Apple Computer, Inc.

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
#include "frame.h"
#include "inferior.h"
#include "symtab.h"
#include "target.h"
#include "gdbcore.h"
#include "symfile.h"
#include "objfiles.h"
#include "regcache.h"
#include "gdbarch.h"
#include "arch-utils.h"

#include "macosx-nat-mutils.h"

#include "arm-tdep.h"
#include "arm-macosx-thread-status.h"
#include "arm-macosx-tdep.h"

extern enum gdb_osabi arm_set_osabi_from_host_info ();

static inline uint32_t
collect_uint32 (int regnum)
{
  gdb_byte buf[sizeof(uint32_t)];
  regcache_raw_collect (current_regcache, regnum, buf);
  return extract_unsigned_integer (buf, sizeof(uint32_t));
}

static inline uint64_t
collect_uint64 (int regnum)
{
  gdb_byte buf[sizeof(uint64_t)];
  regcache_raw_collect (current_regcache, regnum, buf);
  return extract_unsigned_integer (buf, sizeof(uint64_t));
}

static inline void
supply_uint32 (int regnum, uint32_t val)
{
  gdb_byte buf[sizeof(val)];
  store_unsigned_integer (buf, sizeof(val), val);
  regcache_raw_supply (current_regcache, regnum, buf);
}

static inline void
supply_uint64 (int regnum, uint64_t val)
{
  gdb_byte buf[sizeof(val)];
  store_unsigned_integer (buf, sizeof(val), val);
  regcache_raw_supply (current_regcache, regnum, buf);
}

/* Fetch General Purpose Registers.  */
void
arm_macosx_fetch_gp_registers (struct gdb_arm_thread_state *gp_regs)
{
  int i;
  for (i = 0; i < ARM_MACOSX_NUM_GP_REGS; i++)
    supply_uint32 (ARM_R0_REGNUM + i, gp_regs->r[i]);
  supply_uint32 (ARM_PS_REGNUM, gp_regs->cpsr);
}

void
arm_macosx_fetch_gp_registers_raw (struct gdb_arm_thread_state *gp_regs)
{
  int i;
  for (i = 0; i < ARM_MACOSX_NUM_GP_REGS; i++)
    regcache_raw_supply (current_regcache, ARM_R0_REGNUM + i, &gp_regs->r[i]);
  regcache_raw_supply (current_regcache, ARM_PS_REGNUM, &gp_regs->cpsr);
}


/* Store General Purpose Registers.  */
void
arm_macosx_store_gp_registers (struct gdb_arm_thread_state *gp_regs)
{
  int i;
  for (i = 0; i < ARM_MACOSX_NUM_GP_REGS; i++)
    gp_regs->r[i] = collect_uint32 (ARM_R0_REGNUM + i);
  gp_regs->cpsr = collect_uint32 (ARM_PS_REGNUM);
}

void
arm_macosx_store_gp_registers_raw (struct gdb_arm_thread_state *gp_regs)
{
  int i;
  for (i = 0; i < ARM_MACOSX_NUM_GP_REGS; i++)
    regcache_raw_collect (current_regcache, ARM_R0_REGNUM + i, &gp_regs->r[i]);
  regcache_raw_collect (current_regcache, ARM_PS_REGNUM, &gp_regs->cpsr);
}

/* Fetch VFP Registers.  */

void
arm_macosx_fetch_vfpv1_regs (gdb_arm_thread_vfpv1_state_t *fp_regs)
{
  int i;
  for (i = 0; i < ARM_MACOSX_NUM_VFP_REGS; i++)
    supply_uint32 (ARM_VFP_REGNUM_S0 + i, fp_regs->r[i]);
  supply_uint32 (ARM_VFP_REGNUM_FPSCR, fp_regs->fpscr);
}

void
arm_macosx_fetch_vfpv3_regs (gdb_arm_thread_vfpv3_state_t *fp_regs)
{
  int i;
  for (i = 0; i < ARM_MACOSX_NUM_VFP_REGS; i++)
    supply_uint32 (ARM_VFP_REGNUM_S0 + i, fp_regs->s[i]);
  for (i = 0; i < ARM_MACOSX_NUM_VFPV3_REGS; i++)
    supply_uint64 (ARM_VFPV3_REGNUM_D16 + i, fp_regs->d[i]);
  supply_uint32 (ARM_VFP_REGNUM_FPSCR, fp_regs->fpscr);

}

void
arm_macosx_fetch_vfpv1_regs_raw (gdb_arm_thread_vfpv1_state_t *fp_regs)
{
  int i;
  for (i = 0; i < ARM_MACOSX_NUM_VFP_REGS; i++)
    regcache_raw_supply (current_regcache, ARM_VFP_REGNUM_S0 + i, 
			 &fp_regs->r[i]);
  regcache_raw_supply (current_regcache, ARM_VFP_REGNUM_FPSCR, &fp_regs->fpscr);
}

void
arm_macosx_fetch_vfpv3_regs_raw (gdb_arm_thread_vfpv3_state_t *fp_regs)
{
  int i;
  for (i = 0; i < ARM_MACOSX_NUM_VFP_REGS; i++)
    regcache_raw_supply (current_regcache, ARM_VFP_REGNUM_S0 + i, 
			 &fp_regs->s[i]);
  regcache_raw_supply (current_regcache, ARM_VFP_REGNUM_FPSCR, &fp_regs->fpscr);
  for (i = 0; i < ARM_MACOSX_NUM_VFPV3_REGS; i++)
    regcache_raw_supply (current_regcache, ARM_VFPV3_REGNUM_D16 + i, 
			 &fp_regs->d[i]);
}

/* Store VFP Registers.  */

void
arm_macosx_store_vfpv1_regs (gdb_arm_thread_vfpv1_state_t *fp_regs)
{
  int i;
  for (i = 0; i < ARM_MACOSX_NUM_VFP_REGS; i++)
    fp_regs->r[i] = collect_uint32 (ARM_VFP_REGNUM_S0 + i);
  fp_regs->fpscr = collect_uint32 (ARM_VFP_REGNUM_FPSCR);
}

void
arm_macosx_store_vfpv3_regs (gdb_arm_thread_vfpv3_state_t *fp_regs)
{
  int i;
  for (i = 0; i < ARM_MACOSX_NUM_VFP_REGS; i++)
    fp_regs->s[i] = collect_uint32 (ARM_VFP_REGNUM_S0 + i);
  for (i = 0; i < ARM_MACOSX_NUM_VFPV3_REGS; i++)
    fp_regs->d[i] = collect_uint64 (ARM_VFPV3_REGNUM_D16 + i);
  fp_regs->fpscr = collect_uint32 (ARM_VFP_REGNUM_FPSCR);
}

void
arm_macosx_store_vfpv1_regs_raw (gdb_arm_thread_vfpv1_state_t *fp_regs)
{
  int i;
  for (i = 0; i < ARM_MACOSX_NUM_VFP_REGS; i++)
    regcache_raw_collect (current_regcache, ARM_VFP_REGNUM_S0 + i, 
			  &fp_regs->r[i]);
  regcache_raw_collect (current_regcache, ARM_VFP_REGNUM_FPSCR, &fp_regs->fpscr);
}

void
arm_macosx_store_vfpv3_regs_raw (gdb_arm_thread_vfpv3_state_t *fp_regs)
{
  int i;
  for (i = 0; i < ARM_MACOSX_NUM_VFP_REGS; i++)
    regcache_raw_collect (current_regcache, ARM_VFP_REGNUM_S0 + i, 
			  &fp_regs->s[i]);
  regcache_raw_collect (current_regcache, ARM_VFP_REGNUM_FPSCR, &fp_regs->fpscr);
  for (i = 0; i < ARM_MACOSX_NUM_VFPV3_REGS; i++)
    regcache_raw_collect (current_regcache, ARM_VFPV3_REGNUM_D16 + i, 
			  &fp_regs->d[i]);
}

static void
validate_inferior_registers (int regno)
{
  int i;
  if (regno == -1)
    {
      for (i = 0; i < NUM_GREGS; i++)
        {
          if (!register_cached (i))
            fetch_inferior_registers (i);
        }
      if (!register_cached (ARM_PS_REGNUM))
	fetch_inferior_registers (ARM_PS_REGNUM);
    }
  else if (!register_cached (regno))
    {
      fetch_inferior_registers (regno);
    }
}

/* Read register values from the inferior process.
   If REGNO is -1, do this for all registers.
   Otherwise, REGNO specifies which register (so we can save time).  */

void
fetch_inferior_registers (int regno)
{
  int i;
  thread_t current_thread = ptid_get_tid (inferior_ptid);
  kern_return_t ret = KERN_SUCCESS;

  if (TARGET_OSABI == GDB_OSABI_UNKNOWN)
    arm_set_osabi_from_host_info ();

  if ((regno == -1) || ARM_MACOSX_IS_GP_RELATED_REGNUM (regno))
    {
      struct gdb_arm_thread_state gp_regs;
      unsigned int gp_count = GDB_ARM_THREAD_STATE_COUNT;
      ret = thread_get_state
        (current_thread, GDB_ARM_THREAD_STATE, (thread_state_t) & gp_regs,
         &gp_count);
      if (ret != KERN_SUCCESS)
       {
         printf ("Error calling thread_get_state for GP registers for thread 0x%ulx", 
		  current_thread);
         MACH_CHECK_ERROR (ret);
       }
      MACH_CHECK_ERROR (ret);
      arm_macosx_fetch_gp_registers (&gp_regs);
    }

  if ((regno == -1) || ARM_MACOSX_IS_FP_RELATED_REGNUM (regno))
    {
      /* We don't have F0-F7, though they need to exist in our register
         numbering scheme so we can connect to remote gdbserver's that use
	 FSF register numbers.  */
      for (i = ARM_F0_REGNUM; i <= ARM_F7_REGNUM; i++)
	set_register_cached (i, 1);
      set_register_cached (ARM_FPS_REGNUM, 1);
    }

  if ((regno == -1) || ARM_MACOSX_IS_VFP_RELATED_REGNUM (regno))
    {
      enum arm_vfp_version vfp_version;
      vfp_version = gdbarch_tdep (current_gdbarch)->vfp_version;
      int fp_byte_size = -1;

      switch (vfp_version)
	{
	  case ARM_VFP_UNSUPPORTED:
	    /* No VFP support, so nothing to do.  */
	    fp_byte_size = 0;
	    break;

	  case ARM_VFP_VERSION_1:
	    {
	      gdb_arm_thread_vfpv1_state_t fp_regs;
	      mach_msg_type_number_t fp_count = GDB_ARM_THREAD_FPSTATE_VFPV1_COUNT;
	      ret = thread_get_state (current_thread, GDB_ARM_THREAD_FPSTATE,
				      (thread_state_t) & fp_regs,
				      &fp_count);
	      if (ret != KERN_SUCCESS)
		{
		  printf ("Error calling thread_get_state for VFP registers for thread 0x%ulx", 
			  current_thread);
		  MACH_CHECK_ERROR (ret);
		}
	      arm_macosx_fetch_vfpv1_regs (&fp_regs);
	    }
	    break;

	  case ARM_VFP_VERSION_3:
	    {
	      gdb_arm_thread_vfpv3_state_t fp_regs;
	      mach_msg_type_number_t fp_count = GDB_ARM_THREAD_FPSTATE_VFPV3_COUNT;
	      ret = thread_get_state (current_thread, GDB_ARM_THREAD_FPSTATE,
				      (thread_state_t) & fp_regs,
				      &fp_count);
	      if (ret != KERN_SUCCESS)
		{
		  printf ("Error calling thread_get_state for VFP registers for thread 0x%ulx", 
			  current_thread);
		  MACH_CHECK_ERROR (ret);
		}

	      if (fp_count == GDB_ARM_THREAD_FPSTATE_VFPV1_COUNT)
		{
		  /* We asked for VFPv3 registers but we got VFPv1 registers 
		     back. We may have our VFP settings incorrectly set.  */
		  gdb_arm_thread_vfpv1_state_t *v1 = 
				      (gdb_arm_thread_vfpv1_state_t *)&fp_regs;

		  warning ("\nthread_get_state called with VFPv3 register count "
			   "returned VFPv1 register count.\nUse '--osabi "
			   "DarwinV6' when launching gdb to avoid this issue.");
		  fp_regs.fpscr = v1->fpscr;
		  memset (fp_regs.d, 0, sizeof(fp_regs.d));
		}

	      arm_macosx_fetch_vfpv3_regs (&fp_regs);
	    }
	    break;

	  default:
	    error ("fetch_inferior_registers: unable to fetch ARM_THREAD_FPSTATE: "
		   "unsupported vfp version: %d", (int)vfp_version);
	    break;
	}
    }
}

/* Store our register values back into the inferior.
   If REGNO is -1, do this for all registers.
   Otherwise, REGNO specifies which register (so we can save time).  */

void
store_inferior_registers (int regno)
{
  int current_pid;
  thread_t current_thread;
  kern_return_t ret;

  current_pid = ptid_get_pid (inferior_ptid);
  current_thread = ptid_get_tid (inferior_ptid);

  validate_inferior_registers (regno);

  if ((regno == -1) || ARM_MACOSX_IS_GP_RELATED_REGNUM (regno))
    {
      struct gdb_arm_thread_state gp_regs;
      arm_macosx_store_gp_registers (&gp_regs);
      ret = thread_set_state (current_thread, GDB_ARM_THREAD_STATE,
                              (thread_state_t) & gp_regs,
                              GDB_ARM_THREAD_STATE_COUNT);
      MACH_CHECK_ERROR (ret);
    }

  if ((regno == -1) || ARM_MACOSX_IS_VFP_RELATED_REGNUM (regno))
    {
      enum arm_vfp_version vfp_version;
      vfp_version = gdbarch_tdep (current_gdbarch)->vfp_version;
      int fp_byte_size = -1;

      switch (vfp_version)
	{
	  case ARM_VFP_UNSUPPORTED:
	    /* No VFP support, so nothing to do.  */
	    fp_byte_size = 0;
	    break;

	  case ARM_VFP_VERSION_1:
	    {
	      gdb_arm_thread_vfpv1_state_t fp_regs;
	      arm_macosx_store_vfpv1_regs (&fp_regs);
	      ret = thread_set_state (current_thread, GDB_ARM_THREAD_FPSTATE,
				      (thread_state_t) & fp_regs,
				      GDB_ARM_THREAD_FPSTATE_VFPV1_COUNT);
	      MACH_CHECK_ERROR (ret);
	    }
	    break;
	    
	  case ARM_VFP_VERSION_3:
	    {
	      gdb_arm_thread_vfpv3_state_t fp_regs;
	      arm_macosx_store_vfpv3_regs (&fp_regs);
	      ret = thread_set_state (current_thread, GDB_ARM_THREAD_FPSTATE,
				      (thread_state_t) & fp_regs,
				      GDB_ARM_THREAD_FPSTATE_VFPV3_COUNT);
	      MACH_CHECK_ERROR (ret);
	    }
	    break;

 	  default:
	    error ("store_inferior_registers: unable to store ARM_THREAD_FPSTATE: "
		   "unsupported vfp version: %d", (int)vfp_version);
	    break;
	}
    }
}

void
macosx_complete_child_target (struct target_ops *target)
{
}

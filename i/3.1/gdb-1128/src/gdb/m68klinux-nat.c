/* Motorola m68k native support for GNU/Linux.

   Copyright 1996, 1998, 2000, 2001, 2002 Free Software Foundation,
   Inc.

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
#include "language.h"
#include "gdbcore.h"
#include "gdb_string.h"
#include "regcache.h"

#include "m68k-tdep.h"

#include <sys/param.h>
#include <sys/dir.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/procfs.h>

#ifdef HAVE_SYS_REG_H
#include <sys/reg.h>
#endif

#include <sys/file.h>
#include "gdb_stat.h"

#include "floatformat.h"

#include "target.h"

/* This table must line up with REGISTER_NAME in "m68k-tdep.c".  */
static const int regmap[] =
{
  PT_D0, PT_D1, PT_D2, PT_D3, PT_D4, PT_D5, PT_D6, PT_D7,
  PT_A0, PT_A1, PT_A2, PT_A3, PT_A4, PT_A5, PT_A6, PT_USP,
  PT_SR, PT_PC,
  /* PT_FP0, ..., PT_FP7 */
  21, 24, 27, 30, 33, 36, 39, 42,
  /* PT_FPCR, PT_FPSR, PT_FPIAR */
  45, 46, 47
};

/* Which ptrace request retrieves which registers?
   These apply to the corresponding SET requests as well.  */
#define NUM_GREGS (18)
#define MAX_NUM_REGS (NUM_GREGS + 11)

int
getregs_supplies (int regno)
{
  return 0 <= regno && regno < NUM_GREGS;
}

int
getfpregs_supplies (int regno)
{
  return FP0_REGNUM <= regno && regno <= M68K_FPI_REGNUM;
}

/* Does the current host support the GETREGS request?  */
int have_ptrace_getregs =
#ifdef HAVE_PTRACE_GETREGS
  1
#else
  0
#endif
;



/* BLOCKEND is the value of u.u_ar0, and points to the place where GS
   is stored.  */

int
m68k_linux_register_u_addr (int blockend, int regnum)
{
  return (blockend + 4 * regmap[regnum]);
}


/* Fetching registers directly from the U area, one at a time.  */

/* FIXME: This duplicates code from `inptrace.c'.  The problem is that we
   define FETCH_INFERIOR_REGISTERS since we want to use our own versions
   of {fetch,store}_inferior_registers that use the GETREGS request.  This
   means that the code in `infptrace.c' is #ifdef'd out.  But we need to
   fall back on that code when GDB is running on top of a kernel that
   doesn't support the GETREGS request.  */

#ifndef PT_READ_U
#define PT_READ_U PTRACE_PEEKUSR
#endif
#ifndef PT_WRITE_U
#define PT_WRITE_U PTRACE_POKEUSR
#endif

/* Default the type of the ptrace transfer to int.  */
#ifndef PTRACE_XFER_TYPE
#define PTRACE_XFER_TYPE int
#endif

/* Fetch one register.  */

static void
fetch_register (int regno)
{
  /* This isn't really an address.  But ptrace thinks of it as one.  */
  CORE_ADDR regaddr;
  char mess[128];		/* For messages */
  int i;
  unsigned int offset;		/* Offset of registers within the u area.  */
  char buf[MAX_REGISTER_SIZE];
  int tid;

  if (CANNOT_FETCH_REGISTER (regno))
    {
      memset (buf, '\0', register_size (current_gdbarch, regno));	/* Supply zeroes */
      regcache_raw_supply (current_regcache, regno, buf);
      return;
    }

  /* Overload thread id onto process id */
  tid = TIDGET (inferior_ptid);
  if (tid == 0)
    tid = PIDGET (inferior_ptid);	/* no thread id, just use process id */

  offset = U_REGS_OFFSET;

  regaddr = register_addr (regno, offset);
  for (i = 0; i < register_size (current_gdbarch, regno);
       i += sizeof (PTRACE_XFER_TYPE))
    {
      errno = 0;
      *(PTRACE_XFER_TYPE *) &buf[i] = ptrace (PT_READ_U, tid,
					      (PTRACE_ARG3_TYPE) regaddr, 0);
      regaddr += sizeof (PTRACE_XFER_TYPE);
      if (errno != 0)
	{
	  sprintf (mess, "reading register %s (#%d)", 
		   REGISTER_NAME (regno), regno);
	  perror_with_name (mess);
	}
    }
  regcache_raw_supply (current_regcache, regno, buf);
}

/* Fetch register values from the inferior.
   If REGNO is negative, do this for all registers.
   Otherwise, REGNO specifies which register (so we can save time). */

void
old_fetch_inferior_registers (int regno)
{
  if (regno >= 0)
    {
      fetch_register (regno);
    }
  else
    {
      for (regno = 0; regno < NUM_REGS; regno++)
	{
	  fetch_register (regno);
	}
    }
}

/* Store one register. */

static void
store_register (int regno)
{
  /* This isn't really an address.  But ptrace thinks of it as one.  */
  CORE_ADDR regaddr;
  char mess[128];		/* For messages */
  int i;
  unsigned int offset;		/* Offset of registers within the u area.  */
  int tid;
  char buf[MAX_REGISTER_SIZE];

  if (CANNOT_STORE_REGISTER (regno))
    {
      return;
    }

  /* Overload thread id onto process id */
  tid = TIDGET (inferior_ptid);
  if (tid == 0)
    tid = PIDGET (inferior_ptid);	/* no thread id, just use process id */

  offset = U_REGS_OFFSET;

  regaddr = register_addr (regno, offset);

  /* Put the contents of regno into a local buffer */
  regcache_raw_collect (current_regcache, regno, buf);

  /* Store the local buffer into the inferior a chunk at the time. */
  for (i = 0; i < register_size (current_gdbarch, regno);
       i += sizeof (PTRACE_XFER_TYPE))
    {
      errno = 0;
      ptrace (PT_WRITE_U, tid, (PTRACE_ARG3_TYPE) regaddr,
	      *(PTRACE_XFER_TYPE *) (buf + i));
      regaddr += sizeof (PTRACE_XFER_TYPE);
      if (errno != 0)
	{
	  sprintf (mess, "writing register %s (#%d)", 
		   REGISTER_NAME (regno), regno);
	  perror_with_name (mess);
	}
    }
}

/* Store our register values back into the inferior.
   If REGNO is negative, do this for all registers.
   Otherwise, REGNO specifies which register (so we can save time).  */

void
old_store_inferior_registers (int regno)
{
  if (regno >= 0)
    {
      store_register (regno);
    }
  else
    {
      for (regno = 0; regno < NUM_REGS; regno++)
	{
	  store_register (regno);
	}
    }
}

/*  Given a pointer to a general register set in /proc format
   (elf_gregset_t *), unpack the register contents and supply
   them as gdb's idea of the current register values. */


/* Note both m68k-tdep.c and m68klinux-nat.c contain definitions
   for supply_gregset and supply_fpregset. The definitions
   in m68k-tdep.c are valid if USE_PROC_FS is defined. Otherwise,
   the definitions in m68klinux-nat.c will be used. This is a 
   bit of a hack. The supply_* routines do not belong in 
   *_tdep.c files. But, there are several lynx ports that currently 
   depend on these definitions. */

#ifndef USE_PROC_FS

/* Prototypes for supply_gregset etc. */
#include "gregset.h"

void
supply_gregset (elf_gregset_t *gregsetp)
{
  elf_greg_t *regp = (elf_greg_t *) gregsetp;
  int regi;

  for (regi = M68K_D0_REGNUM; regi <= SP_REGNUM; regi++)
    regcache_raw_supply (current_regcache, regi, (char *) &regp[regmap[regi]]);
  regcache_raw_supply (current_regcache, PS_REGNUM, (char *) &regp[PT_SR]);
  regcache_raw_supply (current_regcache, PC_REGNUM, (char *) &regp[PT_PC]);
}

/* Fill register REGNO (if it is a general-purpose register) in
   *GREGSETPS with the value in GDB's register array.  If REGNO is -1,
   do this for all registers.  */
void
fill_gregset (elf_gregset_t *gregsetp, int regno)
{
  elf_greg_t *regp = (elf_greg_t *) gregsetp;
  int i;

  for (i = 0; i < NUM_GREGS; i++)
    if (regno == -1 || regno == i)
      regcache_raw_collect (current_regcache, i, regp + regmap[i]);
}

#ifdef HAVE_PTRACE_GETREGS

/* Fetch all general-purpose registers from process/thread TID and
   store their values in GDB's register array.  */

static void
fetch_regs (int tid)
{
  elf_gregset_t regs;

  if (ptrace (PTRACE_GETREGS, tid, 0, (int) &regs) < 0)
    {
      if (errno == EIO)
	{
	  /* The kernel we're running on doesn't support the GETREGS
             request.  Reset `have_ptrace_getregs'.  */
	  have_ptrace_getregs = 0;
	  return;
	}

      perror_with_name (_("Couldn't get registers"));
    }

  supply_gregset (&regs);
}

/* Store all valid general-purpose registers in GDB's register array
   into the process/thread specified by TID.  */

static void
store_regs (int tid, int regno)
{
  elf_gregset_t regs;

  if (ptrace (PTRACE_GETREGS, tid, 0, (int) &regs) < 0)
    perror_with_name (_("Couldn't get registers"));

  fill_gregset (&regs, regno);

  if (ptrace (PTRACE_SETREGS, tid, 0, (int) &regs) < 0)
    perror_with_name (_("Couldn't write registers"));
}

#else

static void fetch_regs (int tid) {}
static void store_regs (int tid, int regno) {}

#endif


/* Transfering floating-point registers between GDB, inferiors and cores.  */

/* What is the address of fpN within the floating-point register set F?  */
#define FPREG_ADDR(f, n) ((char *) &(f)->fpregs[(n) * 3])

/* Fill GDB's register array with the floating-point register values in
   *FPREGSETP.  */

void
supply_fpregset (elf_fpregset_t *fpregsetp)
{
  int regi;

  for (regi = FP0_REGNUM; regi < FP0_REGNUM + 8; regi++)
    regcache_raw_supply (current_regcache, regi,
			 FPREG_ADDR (fpregsetp, regi - FP0_REGNUM));
  regcache_raw_supply (current_regcache, M68K_FPC_REGNUM,
		       (char *) &fpregsetp->fpcntl[0]);
  regcache_raw_supply (current_regcache, M68K_FPS_REGNUM,
		       (char *) &fpregsetp->fpcntl[1]);
  regcache_raw_supply (current_regcache, M68K_FPI_REGNUM,
		       (char *) &fpregsetp->fpcntl[2]);
}

/* Fill register REGNO (if it is a floating-point register) in
   *FPREGSETP with the value in GDB's register array.  If REGNO is -1,
   do this for all registers.  */

void
fill_fpregset (elf_fpregset_t *fpregsetp, int regno)
{
  int i;

  /* Fill in the floating-point registers.  */
  for (i = FP0_REGNUM; i < FP0_REGNUM + 8; i++)
    if (regno == -1 || regno == i)
      regcache_raw_collect (current_regcache, i,
			    FPREG_ADDR (fpregsetp, i - FP0_REGNUM));

  /* Fill in the floating-point control registers.  */
  for (i = M68K_FPC_REGNUM; i <= M68K_FPI_REGNUM; i++)
    if (regno == -1 || regno == i)
      regcache_raw_collect (current_regcache, i,
			    (char *) &fpregsetp->fpcntl[i - M68K_FPC_REGNUM]);
}

#ifdef HAVE_PTRACE_GETREGS

/* Fetch all floating-point registers from process/thread TID and store
   thier values in GDB's register array.  */

static void
fetch_fpregs (int tid)
{
  elf_fpregset_t fpregs;

  if (ptrace (PTRACE_GETFPREGS, tid, 0, (int) &fpregs) < 0)
    perror_with_name (_("Couldn't get floating point status"));

  supply_fpregset (&fpregs);
}

/* Store all valid floating-point registers in GDB's register array
   into the process/thread specified by TID.  */

static void
store_fpregs (int tid, int regno)
{
  elf_fpregset_t fpregs;

  if (ptrace (PTRACE_GETFPREGS, tid, 0, (int) &fpregs) < 0)
    perror_with_name (_("Couldn't get floating point status"));

  fill_fpregset (&fpregs, regno);

  if (ptrace (PTRACE_SETFPREGS, tid, 0, (int) &fpregs) < 0)
    perror_with_name (_("Couldn't write floating point status"));
}

#else

static void fetch_fpregs (int tid) {}
static void store_fpregs (int tid, int regno) {}

#endif

#endif

/* Transferring arbitrary registers between GDB and inferior.  */

/* Fetch register REGNO from the child process.  If REGNO is -1, do
   this for all registers (including the floating point and SSE
   registers).  */

void
fetch_inferior_registers (int regno)
{
  int tid;

  /* Use the old method of peeking around in `struct user' if the
     GETREGS request isn't available.  */
  if (! have_ptrace_getregs)
    {
      old_fetch_inferior_registers (regno);
      return;
    }

  /* GNU/Linux LWP ID's are process ID's.  */
  tid = TIDGET (inferior_ptid);
  if (tid == 0)
    tid = PIDGET (inferior_ptid);		/* Not a threaded program.  */

  /* Use the PTRACE_GETFPXREGS request whenever possible, since it
     transfers more registers in one system call, and we'll cache the
     results.  But remember that fetch_fpxregs can fail, and return
     zero.  */
  if (regno == -1)
    {
      fetch_regs (tid);

      /* The call above might reset `have_ptrace_getregs'.  */
      if (! have_ptrace_getregs)
	{
	  old_fetch_inferior_registers (-1);
	  return;
	}

      fetch_fpregs (tid);
      return;
    }

  if (getregs_supplies (regno))
    {
      fetch_regs (tid);
      return;
    }

  if (getfpregs_supplies (regno))
    {
      fetch_fpregs (tid);
      return;
    }

  internal_error (__FILE__, __LINE__,
		  _("Got request for bad register number %d."), regno);
}

/* Store register REGNO back into the child process.  If REGNO is -1,
   do this for all registers (including the floating point and SSE
   registers).  */
void
store_inferior_registers (int regno)
{
  int tid;

  /* Use the old method of poking around in `struct user' if the
     SETREGS request isn't available.  */
  if (! have_ptrace_getregs)
    {
      old_store_inferior_registers (regno);
      return;
    }

  /* GNU/Linux LWP ID's are process ID's.  */
  tid = TIDGET (inferior_ptid);
  if (tid == 0)
    tid = PIDGET (inferior_ptid);	/* Not a threaded program.  */

  /* Use the PTRACE_SETFPREGS requests whenever possible, since it
     transfers more registers in one system call.  But remember that
     store_fpregs can fail, and return zero.  */
  if (regno == -1)
    {
      store_regs (tid, regno);
      store_fpregs (tid, regno);
      return;
    }

  if (getregs_supplies (regno))
    {
      store_regs (tid, regno);
      return;
    }

  if (getfpregs_supplies (regno))
    {
      store_fpregs (tid, regno);
      return;
    }

  internal_error (__FILE__, __LINE__,
		  _("Got request to store bad register number %d."), regno);
}

/* Interpreting register set info found in core files.  */

/* Provide registers to GDB from a core file.

   (We can't use the generic version of this function in
   core-regset.c, because we need to use elf_gregset_t instead of
   gregset_t.)

   CORE_REG_SECT points to an array of bytes, which are the contents
   of a `note' from a core file which BFD thinks might contain
   register contents.  CORE_REG_SIZE is its size.

   WHICH says which register set corelow suspects this is:
     0 --- the general-purpose register set, in elf_gregset_t format
     2 --- the floating-point register set, in elf_fpregset_t format

   REG_ADDR isn't used on GNU/Linux.  */

static void
fetch_core_registers (char *core_reg_sect, unsigned core_reg_size,
		      int which, CORE_ADDR reg_addr)
{
  elf_gregset_t gregset;
  elf_fpregset_t fpregset;

  switch (which)
    {
    case 0:
      if (core_reg_size != sizeof (gregset))
	warning (_("Wrong size gregset in core file."));
      else
	{
	  memcpy (&gregset, core_reg_sect, sizeof (gregset));
	  supply_gregset (&gregset);
	}
      break;

    case 2:
      if (core_reg_size != sizeof (fpregset))
	warning (_("Wrong size fpregset in core file."));
      else
	{
	  memcpy (&fpregset, core_reg_sect, sizeof (fpregset));
	  supply_fpregset (&fpregset);
	}
      break;

    default:
      /* We've covered all the kinds of registers we know about here,
         so this must be something we wouldn't know what to do with
         anyway.  Just ignore it.  */
      break;
    }
}


int
kernel_u_size (void)
{
  return (sizeof (struct user));
}

/* Register that we are able to handle GNU/Linux ELF core file
   formats.  */

static struct core_fns linux_elf_core_fns =
{
  bfd_target_elf_flavour,		/* core_flavour */
  default_check_format,			/* check_format */
  default_core_sniffer,			/* core_sniffer */
  fetch_core_registers,			/* core_read_registers */
  NULL					/* next */
};

void
_initialize_m68k_linux_nat (void)
{
  deprecated_add_core_fns (&linux_elf_core_fns);
}

/* Cache and manage the values of registers for GDB, the GNU debugger.

   Copyright 1986, 1987, 1989, 1991, 1994, 1995, 1996, 1998, 2000,
   2001, 2002, 2004 Free Software Foundation, Inc.

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
#include "target.h"
#include "gdbarch.h"
#include "gdbcmd.h"
#include "regcache.h"
#include "reggroups.h"
#include "gdb_assert.h"
#include "gdb_string.h"
#include "gdbcmd.h"		/* For maintenanceprintlist.  */
#include "observer.h"

/*
 * DATA STRUCTURE
 *
 * Here is the actual register cache.
 */

/* Per-architecture object describing the layout of a register cache.
   Computed once when the architecture is created */

struct gdbarch_data *regcache_descr_handle;

struct regcache_descr
{
  /* The architecture this descriptor belongs to.  */
  struct gdbarch *gdbarch;

  /* The raw register cache.  Each raw (or hard) register is supplied
     by the target interface.  The raw cache should not contain
     redundant information - if the PC is constructed from two
     registers then those registers and not the PC lives in the raw
     cache.  */
  int nr_raw_registers;
  long sizeof_raw_registers;
  long sizeof_raw_register_valid_p;

  /* The cooked register space.  Each cooked register in the range
     [0..NR_RAW_REGISTERS) is direct-mapped onto the corresponding raw
     register.  The remaining [NR_RAW_REGISTERS
     .. NR_COOKED_REGISTERS) (a.k.a. pseudo registers) are mapped onto
     both raw registers and memory by the architecture methods
     gdbarch_pseudo_register_read and gdbarch_pseudo_register_write.  */
  int nr_cooked_registers;
  long sizeof_cooked_registers;
  long sizeof_cooked_register_valid_p;

  /* Offset and size (in 8 bit bytes), of reach register in the
     register cache.  All registers (including those in the range
     [NR_RAW_REGISTERS .. NR_COOKED_REGISTERS) are given an offset.
     Assigning all registers an offset makes it possible to keep
     legacy code, such as that found in read_register_bytes() and
     write_register_bytes() working.  */
  long *register_offset;
  long *sizeof_register;

  /* Cached table containing the type of each register.  */
  struct type **register_type;
};

static void *
init_regcache_descr (struct gdbarch *gdbarch)
{
  int i;
  struct regcache_descr *descr;
  gdb_assert (gdbarch != NULL);

  /* Create an initial, zero filled, table.  */
  descr = GDBARCH_OBSTACK_ZALLOC (gdbarch, struct regcache_descr);
  descr->gdbarch = gdbarch;

  /* Total size of the register space.  The raw registers are mapped
     directly onto the raw register cache while the pseudo's are
     either mapped onto raw-registers or memory.  */
  descr->nr_cooked_registers = NUM_REGS + NUM_PSEUDO_REGS;
  descr->sizeof_cooked_register_valid_p = NUM_REGS + NUM_PSEUDO_REGS;

  /* Fill in a table of register types.  */
  descr->register_type
    = GDBARCH_OBSTACK_CALLOC (gdbarch, descr->nr_cooked_registers, struct type *);
  for (i = 0; i < descr->nr_cooked_registers; i++)
    descr->register_type[i] = gdbarch_register_type (gdbarch, i);

  /* Construct a strictly RAW register cache.  Don't allow pseudo's
     into the register cache.  */
  descr->nr_raw_registers = NUM_REGS;

  /* FIXME: cagney/2002-08-13: Overallocate the register_valid_p
     array.  This pretects GDB from erant code that accesses elements
     of the global register_valid_p[] array in the range [NUM_REGS
     .. NUM_REGS + NUM_PSEUDO_REGS).  */
  descr->sizeof_raw_register_valid_p = descr->sizeof_cooked_register_valid_p;

  /* Lay out the register cache.

     NOTE: cagney/2002-05-22: Only register_type() is used when
     constructing the register cache.  It is assumed that the
     register's raw size, virtual size and type length are all the
     same.  */

  {
    long offset = 0;
    descr->sizeof_register
      = GDBARCH_OBSTACK_CALLOC (gdbarch, descr->nr_cooked_registers, long);
    descr->register_offset
      = GDBARCH_OBSTACK_CALLOC (gdbarch, descr->nr_cooked_registers, long);
    for (i = 0; i < descr->nr_cooked_registers; i++)
      {
	descr->sizeof_register[i] = TYPE_LENGTH (descr->register_type[i]);
	descr->register_offset[i] = offset;
	offset += descr->sizeof_register[i];
	gdb_assert (MAX_REGISTER_SIZE >= descr->sizeof_register[i]);
      }
    /* Set the real size of the register cache buffer.  */
    descr->sizeof_cooked_registers = offset;
  }

  /* FIXME: cagney/2002-05-22: Should only need to allocate space for
     the raw registers.  Unfortunately some code still accesses the
     register array directly using the global registers[].  Until that
     code has been purged, play safe and over allocating the register
     buffer.  Ulgh!  */
  descr->sizeof_raw_registers = descr->sizeof_cooked_registers;

  return descr;
}

static struct regcache_descr *
regcache_descr (struct gdbarch *gdbarch)
{
  return gdbarch_data (gdbarch, regcache_descr_handle);
}

/* Utility functions returning useful register attributes stored in
   the regcache descr.  */

struct type *
register_type (struct gdbarch *gdbarch, int regnum)
{
  struct regcache_descr *descr = regcache_descr (gdbarch);
  gdb_assert (regnum >= 0 && regnum < descr->nr_cooked_registers);
  return descr->register_type[regnum];
}

/* Utility functions returning useful register attributes stored in
   the regcache descr.  */

int
register_size (struct gdbarch *gdbarch, int regnum)
{
  struct regcache_descr *descr = regcache_descr (gdbarch);
  int size;
  gdb_assert (regnum >= 0 && regnum < (NUM_REGS + NUM_PSEUDO_REGS));
  size = descr->sizeof_register[regnum];
  return size;
}

/* The register cache for storing raw register values.  */

struct regcache
{
  struct regcache_descr *descr;
  /* The register buffers.  A read-only register cache can hold the
     full [0 .. NUM_REGS + NUM_PSEUDO_REGS) while a read/write
     register cache can only hold [0 .. NUM_REGS).  */
  gdb_byte *registers;
  gdb_byte *register_valid_p;
  /* Is this a read-only cache?  A read-only cache is used for saving
     the target's register state (e.g, across an inferior function
     call or just before forcing a function return).  A read-only
     cache can only be updated via the methods regcache_dup() and
     regcache_cpy().  The actual contents are determined by the
     reggroup_save and reggroup_restore methods.  */
  int readonly_p;
};

struct regcache *
regcache_xmalloc (struct gdbarch *gdbarch)
{
  struct regcache_descr *descr;
  struct regcache *regcache;
  gdb_assert (gdbarch != NULL);
  descr = regcache_descr (gdbarch);
  regcache = XMALLOC (struct regcache);
  regcache->descr = descr;
  regcache->registers
    = XCALLOC (descr->sizeof_raw_registers, gdb_byte);
  regcache->register_valid_p
    = XCALLOC (descr->sizeof_raw_register_valid_p, gdb_byte);
  regcache->readonly_p = 1;
  return regcache;
}

void
regcache_xfree (struct regcache *regcache)
{
  if (regcache == NULL)
    return;
  xfree (regcache->registers);
  xfree (regcache->register_valid_p);
  xfree (regcache);
}

static void
do_regcache_xfree (void *data)
{
  regcache_xfree (data);
}

struct cleanup *
make_cleanup_regcache_xfree (struct regcache *regcache)
{
  return make_cleanup (do_regcache_xfree, regcache);
}

/* Return REGCACHE's architecture.  */

struct gdbarch *
get_regcache_arch (const struct regcache *regcache)
{
  return regcache->descr->gdbarch;
}

/* Return  a pointer to register REGNUM's buffer cache.  */

static gdb_byte *
register_buffer (const struct regcache *regcache, int regnum)
{
  return regcache->registers + regcache->descr->register_offset[regnum];
}

void
regcache_save (struct regcache *dst, regcache_cooked_read_ftype *cooked_read,
	       void *src)
{
  struct gdbarch *gdbarch = dst->descr->gdbarch;
  gdb_byte buf[MAX_REGISTER_SIZE];
  int regnum;
  /* The DST should be `read-only', if it wasn't then the save would
     end up trying to write the register values back out to the
     target.  */
  gdb_assert (dst->readonly_p);
  /* Clear the dest.  */
  memset (dst->registers, 0, dst->descr->sizeof_cooked_registers);
  memset (dst->register_valid_p, 0, dst->descr->sizeof_cooked_register_valid_p);
  /* Copy over any registers (identified by their membership in the
     save_reggroup) and mark them as valid.  The full [0 .. NUM_REGS +
     NUM_PSEUDO_REGS) range is checked since some architectures need
     to save/restore `cooked' registers that live in memory.  */
  for (regnum = 0; regnum < dst->descr->nr_cooked_registers; regnum++)
    {
      if (gdbarch_register_reggroup_p (gdbarch, regnum, save_reggroup))
	{
	  int valid = cooked_read (src, regnum, buf);
	  if (valid)
	    {
	      memcpy (register_buffer (dst, regnum), buf,
		      register_size (gdbarch, regnum));
	      dst->register_valid_p[regnum] = 1;
	    }
	}
    }
}

void
regcache_restore (struct regcache *dst,
		  regcache_cooked_read_ftype *cooked_read,
		  void *cooked_read_context)
{
  struct gdbarch *gdbarch = dst->descr->gdbarch;
  gdb_byte buf[MAX_REGISTER_SIZE];
  int regnum;
  /* The dst had better not be read-only.  If it is, the `restore'
     doesn't make much sense.  */
  gdb_assert (!dst->readonly_p);
  /* Copy over any registers, being careful to only restore those that
     were both saved and need to be restored.  The full [0 .. NUM_REGS
     + NUM_PSEUDO_REGS) range is checked since some architectures need
     to save/restore `cooked' registers that live in memory.  */
  for (regnum = 0; regnum < dst->descr->nr_cooked_registers; regnum++)
    {
      if (gdbarch_register_reggroup_p (gdbarch, regnum, restore_reggroup))
	{
	  int valid = cooked_read (cooked_read_context, regnum, buf);
	  if (valid)
	    regcache_cooked_write (dst, regnum, buf);
	}
    }
}

static int
do_cooked_read (void *src, int regnum, gdb_byte *buf)
{
  struct regcache *regcache = src;
  if (!regcache->register_valid_p[regnum] && regcache->readonly_p)
    /* Don't even think about fetching a register from a read-only
       cache when the register isn't yet valid.  There isn't a target
       from which the register value can be fetched.  */
    return 0;
  regcache_cooked_read (regcache, regnum, buf);
  return 1;
}


void
regcache_cpy (struct regcache *dst, struct regcache *src)
{
  gdb_assert (src != NULL && dst != NULL);
  gdb_assert (src->descr->gdbarch == dst->descr->gdbarch);
  gdb_assert (src != dst);
  gdb_assert (src->readonly_p || dst->readonly_p);
  if (!src->readonly_p)
    regcache_save (dst, do_cooked_read, src);
  else if (!dst->readonly_p)
    regcache_restore (dst, do_cooked_read, src);
  else
    regcache_cpy_no_passthrough (dst, src);
}

void
regcache_cpy_no_passthrough (struct regcache *dst, struct regcache *src)
{
  gdb_assert (src != NULL && dst != NULL);
  gdb_assert (src->descr->gdbarch == dst->descr->gdbarch);
  /* NOTE: cagney/2002-05-17: Don't let the caller do a no-passthrough
     move of data into the current_regcache().  Doing this would be
     silly - it would mean that valid_p would be completely invalid.  */
  gdb_assert (dst != current_regcache);
  memcpy (dst->registers, src->registers, dst->descr->sizeof_raw_registers);
  memcpy (dst->register_valid_p, src->register_valid_p,
	  dst->descr->sizeof_raw_register_valid_p);
}

struct regcache *
regcache_dup (struct regcache *src)
{
  struct regcache *newbuf;
  gdb_assert (current_regcache != NULL);
  newbuf = regcache_xmalloc (src->descr->gdbarch);
  regcache_cpy (newbuf, src);
  return newbuf;
}

struct regcache *
regcache_dup_no_passthrough (struct regcache *src)
{
  struct regcache *newbuf;
  gdb_assert (current_regcache != NULL);
  newbuf = regcache_xmalloc (src->descr->gdbarch);
  regcache_cpy_no_passthrough (newbuf, src);
  return newbuf;
}

int
regcache_valid_p (struct regcache *regcache, int regnum)
{
  gdb_assert (regcache != NULL);
  gdb_assert (regnum >= 0 && regnum < regcache->descr->nr_raw_registers);
  return regcache->register_valid_p[regnum];
}

gdb_byte *
deprecated_grub_regcache_for_registers (struct regcache *regcache)
{
  return regcache->registers;
}

/* Global structure containing the current regcache.  */
/* FIXME: cagney/2002-05-11: The two global arrays registers[] and
   deprecated_register_valid[] currently point into this structure.  */
struct regcache *current_regcache;

/* NOTE: this is a write-through cache.  There is no "dirty" bit for
   recording if the register values have been changed (eg. by the
   user).  Therefore all registers must be written back to the
   target when appropriate.  */

/* The thread/process associated with the current set of registers. */

static ptid_t registers_ptid;

/*
 * FUNCTIONS:
 */

/* REGISTER_CACHED()

   Returns 0 if the value is not in the cache (needs fetch).
          >0 if the value is in the cache.
	  <0 if the value is permanently unavailable (don't ask again).  */

int
register_cached (int regnum)
{
  return current_regcache->register_valid_p[regnum];
}

/* Record that REGNUM's value is cached if STATE is >0, uncached but
   fetchable if STATE is 0, and uncached and unfetchable if STATE is <0.  */

void
set_register_cached (int regnum, int state)
{
  gdb_assert (regnum >= 0);
  gdb_assert (regnum < current_regcache->descr->nr_raw_registers);
  current_regcache->register_valid_p[regnum] = state;
}

/* Observer for the target_changed event.  */

void
regcache_observer_target_changed (struct target_ops *target)
{
  registers_changed ();
}

/* Low level examining and depositing of registers.

   The caller is responsible for making sure that the inferior is
   stopped before calling the fetching routines, or it will get
   garbage.  (a change from GDB version 3, in which the caller got the
   value from the last stop).  */

/* REGISTERS_CHANGED ()

   Indicate that registers may have changed, so invalidate the cache.  */

void
registers_changed (void)
{
  int i;

  registers_ptid = pid_to_ptid (-1);

  /* Force cleanup of any alloca areas if using C alloca instead of
     a builtin alloca.  This particular call is used to clean up
     areas allocated by low level target code which may build up
     during lengthy interactions between gdb and the target before
     gdb gives control to the user (ie watchpoints).  */
  alloca (0);

  for (i = 0; i < current_regcache->descr->nr_raw_registers; i++)
    set_register_cached (i, 0);

  if (deprecated_registers_changed_hook)
    deprecated_registers_changed_hook ();
}

/* DEPRECATED_REGISTERS_FETCHED ()

   Indicate that all registers have been fetched, so mark them all valid.  */

/* FIXME: cagney/2001-12-04: This function is DEPRECATED.  The target
   code was blatting the registers[] array and then calling this.
   Since targets should only be using regcache_raw_supply() the need for
   this function/hack is eliminated.  */

void
deprecated_registers_fetched (void)
{
  int i;

  for (i = 0; i < NUM_REGS; i++)
    set_register_cached (i, 1);
  /* Do not assume that the pseudo-regs have also been fetched.
     Fetching all real regs NEVER accounts for pseudo-regs.  */
}

/* deprecated_read_register_bytes and deprecated_write_register_bytes
   are generally a *BAD* idea.  They are inefficient because they need
   to check for partial updates, which can only be done by scanning
   through all of the registers and seeing if the bytes that are being
   read/written fall inside of an invalid register.  [The main reason
   this is necessary is that register sizes can vary, so a simple
   index won't suffice.]  It is far better to call read_register_gen
   and write_register_gen if you want to get at the raw register
   contents, as it only takes a regnum as an argument, and therefore
   can't do a partial register update.

   Prior to the recent fixes to check for partial updates, both read
   and deprecated_write_register_bytes always checked to see if any
   registers were stale, and then called target_fetch_registers (-1)
   to update the whole set.  This caused really slowed things down for
   remote targets.  */

/* Copy INLEN bytes of consecutive data from registers
   starting with the INREGBYTE'th byte of register data
   into memory at MYADDR.  */

void
deprecated_read_register_bytes (int in_start, gdb_byte *in_buf, int in_len)
{
  int in_end = in_start + in_len;
  int regnum;
  gdb_byte reg_buf[MAX_REGISTER_SIZE];

  /* See if we are trying to read bytes from out-of-date registers.  If so,
     update just those registers.  */

  for (regnum = 0; regnum < NUM_REGS + NUM_PSEUDO_REGS; regnum++)
    {
      int reg_start;
      int reg_end;
      int reg_len;
      int start;
      int end;
      int byte;

      reg_start = DEPRECATED_REGISTER_BYTE (regnum);
      reg_len = register_size (current_gdbarch, regnum);
      reg_end = reg_start + reg_len;

      if (reg_end <= in_start || in_end <= reg_start)
	/* The range the user wants to read doesn't overlap with regnum.  */
	continue;

      if (REGISTER_NAME (regnum) != NULL && *REGISTER_NAME (regnum) != '\0')
	/* Force the cache to fetch the entire register.  */
	deprecated_read_register_gen (regnum, reg_buf);

      /* Legacy note: This function, for some reason, allows a NULL
         input buffer.  If the buffer is NULL, the registers are still
         fetched, just the final transfer is skipped. */
      if (in_buf == NULL)
	continue;

      /* start = max (reg_start, in_start) */
      if (reg_start > in_start)
	start = reg_start;
      else
	start = in_start;

      /* end = min (reg_end, in_end) */
      if (reg_end < in_end)
	end = reg_end;
      else
	end = in_end;

      /* Transfer just the bytes common to both IN_BUF and REG_BUF */
      for (byte = start; byte < end; byte++)
	{
	  in_buf[byte - in_start] = reg_buf[byte - reg_start];
	}
    }
}

void
regcache_raw_read (struct regcache *regcache, int regnum, gdb_byte *buf)
{
  gdb_assert (regcache != NULL && buf != NULL);
  gdb_assert (regnum >= 0 && regnum < regcache->descr->nr_raw_registers);
  /* Make certain that the register cache is up-to-date with respect
     to the current thread.  This switching shouldn't be necessary
     only there is still only one target side register cache.  Sigh!
     On the bright side, at least there is a regcache object.  */
  if (!regcache->readonly_p)
    {
      gdb_assert (regcache == current_regcache);
      if (! ptid_equal (registers_ptid, inferior_ptid))
	{
	  registers_changed ();
	  registers_ptid = inferior_ptid;
	}
      if (!register_cached (regnum))
	target_fetch_registers (regnum);
#if 0
      /* FIXME: cagney/2004-08-07: At present a number of targets
	 forget (or didn't know that they needed) to set this leading to
	 panics.  Also is the problem that targets need to indicate
	 that a register is in one of the possible states: valid,
	 undefined, unknown.  The last of which isn't yet
	 possible.  */
      gdb_assert (register_cached (regnum));
#endif
    }
  /* Copy the value directly into the register cache.  */
  memcpy (buf, register_buffer (regcache, regnum),
	  regcache->descr->sizeof_register[regnum]);
}

void
regcache_raw_read_signed (struct regcache *regcache, int regnum, LONGEST *val)
{
  gdb_byte *buf;
  gdb_assert (regcache != NULL);
  gdb_assert (regnum >= 0 && regnum < regcache->descr->nr_raw_registers);
  buf = alloca (regcache->descr->sizeof_register[regnum]);
  regcache_raw_read (regcache, regnum, buf);
  (*val) = extract_signed_integer (buf,
				   regcache->descr->sizeof_register[regnum]);
}

void
regcache_raw_read_unsigned (struct regcache *regcache, int regnum,
			    ULONGEST *val)
{
  gdb_byte *buf;
  gdb_assert (regcache != NULL);
  gdb_assert (regnum >= 0 && regnum < regcache->descr->nr_raw_registers);
  buf = alloca (regcache->descr->sizeof_register[regnum]);
  regcache_raw_read (regcache, regnum, buf);
  (*val) = extract_unsigned_integer (buf,
				     regcache->descr->sizeof_register[regnum]);
}

void
regcache_raw_write_signed (struct regcache *regcache, int regnum, LONGEST val)
{
  void *buf;
  gdb_assert (regcache != NULL);
  gdb_assert (regnum >=0 && regnum < regcache->descr->nr_raw_registers);
  buf = alloca (regcache->descr->sizeof_register[regnum]);
  store_signed_integer (buf, regcache->descr->sizeof_register[regnum], val);
  regcache_raw_write (regcache, regnum, buf);
}

void
regcache_raw_write_unsigned (struct regcache *regcache, int regnum,
			     ULONGEST val)
{
  void *buf;
  gdb_assert (regcache != NULL);
  gdb_assert (regnum >=0 && regnum < regcache->descr->nr_raw_registers);
  buf = alloca (regcache->descr->sizeof_register[regnum]);
  store_unsigned_integer (buf, regcache->descr->sizeof_register[regnum], val);
  regcache_raw_write (regcache, regnum, buf);
}

void
deprecated_read_register_gen (int regnum, gdb_byte *buf)
{
  gdb_assert (current_regcache != NULL);
  gdb_assert (current_regcache->descr->gdbarch == current_gdbarch);
  regcache_cooked_read (current_regcache, regnum, buf);
}

void
regcache_cooked_read (struct regcache *regcache, int regnum, gdb_byte *buf)
{
  gdb_assert (regnum >= 0);
  gdb_assert (regnum < regcache->descr->nr_cooked_registers);
  if (regnum < regcache->descr->nr_raw_registers)
    regcache_raw_read (regcache, regnum, buf);
  else if (regcache->readonly_p
	   && regnum < regcache->descr->nr_cooked_registers
	   && regcache->register_valid_p[regnum])
    /* Read-only register cache, perhaps the cooked value was cached?  */
    memcpy (buf, register_buffer (regcache, regnum),
	    regcache->descr->sizeof_register[regnum]);
  else
    gdbarch_pseudo_register_read (regcache->descr->gdbarch, regcache,
				  regnum, buf);
}

void
regcache_cooked_read_signed (struct regcache *regcache, int regnum,
			     LONGEST *val)
{
  gdb_byte *buf;
  gdb_assert (regcache != NULL);
  gdb_assert (regnum >= 0 && regnum < regcache->descr->nr_cooked_registers);
  buf = alloca (regcache->descr->sizeof_register[regnum]);
  regcache_cooked_read (regcache, regnum, buf);
  (*val) = extract_signed_integer (buf,
				   regcache->descr->sizeof_register[regnum]);
}

void
regcache_cooked_read_unsigned (struct regcache *regcache, int regnum,
			       ULONGEST *val)
{
  gdb_byte *buf;
  gdb_assert (regcache != NULL);
  gdb_assert (regnum >= 0 && regnum < regcache->descr->nr_cooked_registers);
  buf = alloca (regcache->descr->sizeof_register[regnum]);
  regcache_cooked_read (regcache, regnum, buf);
  (*val) = extract_unsigned_integer (buf,
				     regcache->descr->sizeof_register[regnum]);
}

void
regcache_cooked_write_signed (struct regcache *regcache, int regnum,
			      LONGEST val)
{
  void *buf;
  gdb_assert (regcache != NULL);
  gdb_assert (regnum >=0 && regnum < regcache->descr->nr_cooked_registers);
  buf = alloca (regcache->descr->sizeof_register[regnum]);
  store_signed_integer (buf, regcache->descr->sizeof_register[regnum], val);
  regcache_cooked_write (regcache, regnum, buf);
}

void
regcache_cooked_write_unsigned (struct regcache *regcache, int regnum,
				ULONGEST val)
{
  void *buf;
  gdb_assert (regcache != NULL);
  gdb_assert (regnum >=0 && regnum < regcache->descr->nr_cooked_registers);
  buf = alloca (regcache->descr->sizeof_register[regnum]);
  store_unsigned_integer (buf, regcache->descr->sizeof_register[regnum], val);
  regcache_cooked_write (regcache, regnum, buf);
}

void
regcache_raw_write (struct regcache *regcache, int regnum,
		    const gdb_byte *buf)
{
  gdb_assert (regcache != NULL && buf != NULL);
  gdb_assert (regnum >= 0 && regnum < regcache->descr->nr_raw_registers);
  gdb_assert (!regcache->readonly_p);

  /* On the sparc, writing %g0 is a no-op, so we don't even want to
     change the registers array if something writes to this register.  */
  if (CANNOT_STORE_REGISTER (regnum))
    return;

  /* Make certain that the correct cache is selected.  */
  gdb_assert (regcache == current_regcache);
  if (! ptid_equal (registers_ptid, inferior_ptid))
    {
      registers_changed ();
      registers_ptid = inferior_ptid;
    }

  /* If we have a valid copy of the register, and new value == old
     value, then don't bother doing the actual store. */
  if (regcache_valid_p (regcache, regnum)
      && (memcmp (register_buffer (regcache, regnum), buf,
		  regcache->descr->sizeof_register[regnum]) == 0))
    return;

  target_prepare_to_store ();
  memcpy (register_buffer (regcache, regnum), buf,
	  regcache->descr->sizeof_register[regnum]);
  regcache->register_valid_p[regnum] = 1;
  target_store_registers (regnum);
}

void
deprecated_write_register_gen (int regnum, gdb_byte *buf)
{
  gdb_assert (current_regcache != NULL);
  gdb_assert (current_regcache->descr->gdbarch == current_gdbarch);
  regcache_cooked_write (current_regcache, regnum, buf);
}

void
regcache_cooked_write (struct regcache *regcache, int regnum,
		       const gdb_byte *buf)
{
  gdb_assert (regnum >= 0);
  gdb_assert (regnum < regcache->descr->nr_cooked_registers);
  if (regnum < regcache->descr->nr_raw_registers)
    regcache_raw_write (regcache, regnum, buf);
  else
    gdbarch_pseudo_register_write (regcache->descr->gdbarch, regcache,
				   regnum, buf);
}

/* Copy INLEN bytes of consecutive data from memory at MYADDR
   into registers starting with the MYREGSTART'th byte of register data.  */

void
deprecated_write_register_bytes (int myregstart, gdb_byte *myaddr, int inlen)
{
  int myregend = myregstart + inlen;
  int regnum;

  target_prepare_to_store ();

  /* Scan through the registers updating any that are covered by the
     range myregstart<=>myregend using write_register_gen, which does
     nice things like handling threads, and avoiding updates when the
     new and old contents are the same.  */

  for (regnum = 0; regnum < NUM_REGS + NUM_PSEUDO_REGS; regnum++)
    {
      int regstart, regend;

      regstart = DEPRECATED_REGISTER_BYTE (regnum);
      regend = regstart + register_size (current_gdbarch, regnum);

      /* Is this register completely outside the range the user is writing?  */
      if (myregend <= regstart || regend <= myregstart)
	/* do nothing */ ;		

      /* Is this register completely within the range the user is writing?  */
      else if (myregstart <= regstart && regend <= myregend)
	deprecated_write_register_gen (regnum, myaddr + (regstart - myregstart));

      /* The register partially overlaps the range being written.  */
      else
	{
	  gdb_byte regbuf[MAX_REGISTER_SIZE];

	  /* We may be doing a partial update of an invalid register.
	     Update it from the target before scribbling on it.  */
	  deprecated_read_register_gen (regnum, regbuf);

	  target_store_registers (regnum);
	}
    }
}

/* Perform a partial register transfer using a read, modify, write
   operation.  */

typedef void (regcache_read_ftype) (struct regcache *regcache, int regnum,
				    void *buf);
typedef void (regcache_write_ftype) (struct regcache *regcache, int regnum,
				     const void *buf);

static void
regcache_xfer_part (struct regcache *regcache, int regnum,
		    int offset, int len, void *in, const void *out,
		    void (*read) (struct regcache *regcache, int regnum,
				  gdb_byte *buf),
		    void (*write) (struct regcache *regcache, int regnum,
				   const gdb_byte *buf))
{
  struct regcache_descr *descr = regcache->descr;
  gdb_byte reg[MAX_REGISTER_SIZE];
  gdb_assert (offset >= 0 && offset <= descr->sizeof_register[regnum]);
  gdb_assert (len >= 0 && offset + len <= descr->sizeof_register[regnum]);
  /* Something to do?  */
  if (offset + len == 0)
    return;
  /* Read (when needed) ... */
  if (in != NULL
      || offset > 0
      || offset + len < descr->sizeof_register[regnum])
    {
      gdb_assert (read != NULL);
      read (regcache, regnum, reg);
    }
  /* ... modify ... */
  if (in != NULL)
    memcpy (in, reg + offset, len);
  if (out != NULL)
    memcpy (reg + offset, out, len);
  /* ... write (when needed).  */
  if (out != NULL)
    {
      gdb_assert (write != NULL);
      write (regcache, regnum, reg);
    }
}

void
regcache_raw_read_part (struct regcache *regcache, int regnum,
			int offset, int len, gdb_byte *buf)
{
  struct regcache_descr *descr = regcache->descr;
  gdb_assert (regnum >= 0 && regnum < descr->nr_raw_registers);
  regcache_xfer_part (regcache, regnum, offset, len, buf, NULL,
		      regcache_raw_read, regcache_raw_write);
}

void
regcache_raw_write_part (struct regcache *regcache, int regnum,
			 int offset, int len, const gdb_byte *buf)
{
  struct regcache_descr *descr = regcache->descr;
  gdb_assert (regnum >= 0 && regnum < descr->nr_raw_registers);
  regcache_xfer_part (regcache, regnum, offset, len, NULL, buf,
		      regcache_raw_read, regcache_raw_write);
}

void
regcache_cooked_read_part (struct regcache *regcache, int regnum,
			   int offset, int len, gdb_byte *buf)
{
  struct regcache_descr *descr = regcache->descr;
  gdb_assert (regnum >= 0 && regnum < descr->nr_cooked_registers);
  regcache_xfer_part (regcache, regnum, offset, len, buf, NULL,
		      regcache_cooked_read, regcache_cooked_write);
}

void
regcache_cooked_write_part (struct regcache *regcache, int regnum,
			    int offset, int len, const gdb_byte *buf)
{
  struct regcache_descr *descr = regcache->descr;
  gdb_assert (regnum >= 0 && regnum < descr->nr_cooked_registers);
  regcache_xfer_part (regcache, regnum, offset, len, NULL, buf,
		      regcache_cooked_read, regcache_cooked_write);
}

/* Hack to keep code that view the register buffer as raw bytes
   working.  */

int
register_offset_hack (struct gdbarch *gdbarch, int regnum)
{
  struct regcache_descr *descr = regcache_descr (gdbarch);
  gdb_assert (regnum >= 0 && regnum < descr->nr_cooked_registers);
  return descr->register_offset[regnum];
}

/* Hack to keep code using register_bytes working.  */

int
deprecated_register_bytes (void)
{
  return current_regcache->descr->sizeof_raw_registers;
}

/* Return the contents of register REGNUM as an unsigned integer.  */

ULONGEST
read_register (int regnum)
{
  gdb_byte *buf = alloca (register_size (current_gdbarch, regnum));
  deprecated_read_register_gen (regnum, buf);
  return (extract_unsigned_integer (buf, register_size (current_gdbarch, regnum)));
}

ULONGEST
read_register_pid (int regnum, ptid_t ptid)
{
  ptid_t save_ptid;
  CORE_ADDR retval;

  if (ptid_equal (ptid, inferior_ptid))
    return read_register (regnum);

  save_ptid = inferior_ptid;

  inferior_ptid = ptid;

  retval = read_register (regnum);

  inferior_ptid = save_ptid;

  return retval;
}

/* Store VALUE into the raw contents of register number REGNUM.  */

void
write_register (int regnum, LONGEST val)
{
  void *buf;
  int size;
  size = register_size (current_gdbarch, regnum);
  buf = alloca (size);
  store_signed_integer (buf, size, (LONGEST) val);
  deprecated_write_register_gen (regnum, buf);
}

void
write_register_pid (int regnum, CORE_ADDR val, ptid_t ptid)
{
  ptid_t save_ptid;

  if (ptid_equal (ptid, inferior_ptid))
    {
      write_register (regnum, val);
      return;
    }

  save_ptid = inferior_ptid;

  inferior_ptid = ptid;

  write_register (regnum, val);

  inferior_ptid = save_ptid;
}

/* Supply register REGNUM, whose contents are stored in BUF, to REGCACHE.  */

void
regcache_raw_supply (struct regcache *regcache, int regnum, const void *buf)
{
  void *regbuf;
  size_t size;

  gdb_assert (regcache != NULL);
  gdb_assert (regnum >= 0 && regnum < regcache->descr->nr_raw_registers);
  gdb_assert (!regcache->readonly_p);

  /* FIXME: kettenis/20030828: It shouldn't be necessary to handle
     CURRENT_REGCACHE specially here.  */
  if (regcache == current_regcache
      && !ptid_equal (registers_ptid, inferior_ptid))
    {
      registers_changed ();
      registers_ptid = inferior_ptid;
    }

  regbuf = register_buffer (regcache, regnum);
  size = regcache->descr->sizeof_register[regnum];

  if (buf)
    memcpy (regbuf, buf, size);
  else
    memset (regbuf, 0, size);

  /* Mark the register as cached.  */
  regcache->register_valid_p[regnum] = 1;
}

/* Collect register REGNUM from REGCACHE and store its contents in BUF.  */

void
regcache_raw_collect (const struct regcache *regcache, int regnum, void *buf)
{
  const void *regbuf;
  size_t size;

  gdb_assert (regcache != NULL && buf != NULL);
  gdb_assert (regnum >= 0 && regnum < regcache->descr->nr_raw_registers);

  regbuf = register_buffer (regcache, regnum);
  size = regcache->descr->sizeof_register[regnum];
  memcpy (buf, regbuf, size);
}


/* read_pc, write_pc, read_sp, etc.  Special handling for registers
   PC, SP, and FP.  */

/* NOTE: cagney/2001-02-18: The functions read_pc_pid(), read_pc() and
   read_sp(), will eventually be replaced by per-frame methods.
   Instead of relying on the global INFERIOR_PTID, they will use the
   contextual information provided by the FRAME.  These functions do
   not belong in the register cache.  */

/* NOTE: cagney/2003-06-07: The functions generic_target_write_pc(),
   write_pc_pid() and write_pc(), all need to be replaced by something
   that does not rely on global state.  But what?  */

CORE_ADDR
read_pc_pid (ptid_t ptid)
{
  ptid_t saved_inferior_ptid;
  CORE_ADDR pc_val;

  /* In case ptid != inferior_ptid. */
  saved_inferior_ptid = inferior_ptid;
  inferior_ptid = ptid;

  if (TARGET_READ_PC_P ())
    pc_val = TARGET_READ_PC (ptid);
  /* Else use per-frame method on get_current_frame.  */
  else if (PC_REGNUM >= 0)
    {
      CORE_ADDR raw_val = read_register_pid (PC_REGNUM, ptid);
      pc_val = ADDR_BITS_REMOVE (raw_val);
    }
  else
    internal_error (__FILE__, __LINE__, _("read_pc_pid: Unable to find PC"));

  inferior_ptid = saved_inferior_ptid;
  return pc_val;
}

CORE_ADDR
read_pc (void)
{
  return read_pc_pid (inferior_ptid);
}

void
generic_target_write_pc (CORE_ADDR pc, ptid_t ptid)
{
  if (PC_REGNUM >= 0)
    write_register_pid (PC_REGNUM, pc, ptid);
  else
    internal_error (__FILE__, __LINE__,
		    _("generic_target_write_pc"));
}

void
write_pc_pid (CORE_ADDR pc, ptid_t ptid)
{
  ptid_t saved_inferior_ptid;

  /* In case ptid != inferior_ptid. */
  saved_inferior_ptid = inferior_ptid;
  inferior_ptid = ptid;

  TARGET_WRITE_PC (pc, ptid);

  inferior_ptid = saved_inferior_ptid;
}

void
write_pc (CORE_ADDR pc)
{
  write_pc_pid (pc, inferior_ptid);
}

/* Cope with strage ways of getting to the stack and frame pointers */

CORE_ADDR
read_sp (void)
{
  if (TARGET_READ_SP_P ())
    return TARGET_READ_SP ();
  else if (gdbarch_unwind_sp_p (current_gdbarch))
    return get_frame_sp (get_current_frame ());
  else if (SP_REGNUM >= 0)
    /* Try SP_REGNUM last: this makes all sorts of [wrong] assumptions
       about the architecture so put it at the end.  */
    return read_register (SP_REGNUM);
  internal_error (__FILE__, __LINE__, _("read_sp: Unable to find SP"));
}

static void
reg_flush_command (char *command, int from_tty)
{
  /* Force-flush the register cache.  */
  registers_changed ();
  if (from_tty)
    printf_filtered (_("Register cache flushed.\n"));
}

static void
build_regcache (void)
{
  current_regcache = regcache_xmalloc (current_gdbarch);
  current_regcache->readonly_p = 0;
}

/* APPLE LOCAL begin checkpoints */
/* Given a pair of regcaches, return 1 if they match.  */
int
regcache_compare (struct regcache *rc1, struct regcache *rc2)
{
  int rslt;

  rslt = memcmp (rc1->registers, rc2->registers, rc1->descr->sizeof_raw_registers);

  if (rslt)
    return 0;

  return 1;
}
/* APPLE LOCAL end checkpoints */

static void
dump_endian_bytes (struct ui_file *file, enum bfd_endian endian,
		   const unsigned char *buf, long len)
{
  int i;
  switch (endian)
    {
    case BFD_ENDIAN_BIG:
      for (i = 0; i < len; i++)
	fprintf_unfiltered (file, "%02x", buf[i]);
      break;
    case BFD_ENDIAN_LITTLE:
      for (i = len - 1; i >= 0; i--)
	fprintf_unfiltered (file, "%02x", buf[i]);
      break;
    default:
      internal_error (__FILE__, __LINE__, _("Bad switch"));
    }
}

enum regcache_dump_what
{
  regcache_dump_none, regcache_dump_raw, regcache_dump_cooked, regcache_dump_groups
};

static void
regcache_dump (struct regcache *regcache, struct ui_file *file,
	       enum regcache_dump_what what_to_dump)
{
  struct cleanup *cleanups = make_cleanup (null_cleanup, NULL);
  struct gdbarch *gdbarch = regcache->descr->gdbarch;
  int regnum;
  int footnote_nr = 0;
  int footnote_register_size = 0;
  int footnote_register_offset = 0;
  int footnote_register_type_name_null = 0;
  long register_offset = 0;
  unsigned char buf[MAX_REGISTER_SIZE];

#if 0
  fprintf_unfiltered (file, "nr_raw_registers %d\n",
		      regcache->descr->nr_raw_registers);
  fprintf_unfiltered (file, "nr_cooked_registers %d\n",
		      regcache->descr->nr_cooked_registers);
  fprintf_unfiltered (file, "sizeof_raw_registers %ld\n",
		      regcache->descr->sizeof_raw_registers);
  fprintf_unfiltered (file, "sizeof_raw_register_valid_p %ld\n",
		      regcache->descr->sizeof_raw_register_valid_p);
  fprintf_unfiltered (file, "NUM_REGS %d\n", NUM_REGS);
  fprintf_unfiltered (file, "NUM_PSEUDO_REGS %d\n", NUM_PSEUDO_REGS);
#endif

  gdb_assert (regcache->descr->nr_cooked_registers
	      == (NUM_REGS + NUM_PSEUDO_REGS));

  for (regnum = -1; regnum < regcache->descr->nr_cooked_registers; regnum++)
    {
      /* Name.  */
      if (regnum < 0)
	fprintf_unfiltered (file, " %-10s", "Name");
      else
	{
	  const char *p = REGISTER_NAME (regnum);
	  if (p == NULL)
	    p = "";
	  else if (p[0] == '\0')
	    p = "''";
	  fprintf_unfiltered (file, " %-10s", p);
	}

      /* Number.  */
      if (regnum < 0)
	fprintf_unfiltered (file, " %4s", "Nr");
      else
	fprintf_unfiltered (file, " %4d", regnum);

      /* Relative number.  */
      if (regnum < 0)
	fprintf_unfiltered (file, " %4s", "Rel");
      else if (regnum < NUM_REGS)
	fprintf_unfiltered (file, " %4d", regnum);
      else
	fprintf_unfiltered (file, " %4d", (regnum - NUM_REGS));

      /* Offset.  */
      if (regnum < 0)
	fprintf_unfiltered (file, " %6s  ", "Offset");
      else
	{
	  fprintf_unfiltered (file, " %6ld",
			      regcache->descr->register_offset[regnum]);
	  if (register_offset != regcache->descr->register_offset[regnum]
	      || register_offset != DEPRECATED_REGISTER_BYTE (regnum)
	      || (regnum > 0
		  && (regcache->descr->register_offset[regnum]
		      != (regcache->descr->register_offset[regnum - 1]
			  + regcache->descr->sizeof_register[regnum - 1])))
	      )
	    {
	      if (!footnote_register_offset)
		footnote_register_offset = ++footnote_nr;
	      fprintf_unfiltered (file, "*%d", footnote_register_offset);
	    }
	  else
	    fprintf_unfiltered (file, "  ");
	  register_offset = (regcache->descr->register_offset[regnum]
			     + regcache->descr->sizeof_register[regnum]);
	}

      /* Size.  */
      if (regnum < 0)
	fprintf_unfiltered (file, " %5s ", "Size");
      else
	fprintf_unfiltered (file, " %5ld",
			    regcache->descr->sizeof_register[regnum]);

      /* Type.  */
      {
	const char *t;
	if (regnum < 0)
	  t = "Type";
	else
	  {
	    static const char blt[] = "builtin_type";
	    t = TYPE_NAME (register_type (regcache->descr->gdbarch, regnum));
	    if (t == NULL)
	      {
		char *n;
		if (!footnote_register_type_name_null)
		  footnote_register_type_name_null = ++footnote_nr;
		n = xstrprintf ("*%d", footnote_register_type_name_null);
		make_cleanup (xfree, n);
		t = n;
	      }
	    /* Chop a leading builtin_type.  */
	    if (strncmp (t, blt, strlen (blt)) == 0)
	      t += strlen (blt);
	  }
	fprintf_unfiltered (file, " %-15s", t);
      }

      /* Leading space always present.  */
      fprintf_unfiltered (file, " ");

      /* Value, raw.  */
      if (what_to_dump == regcache_dump_raw)
	{
	  if (regnum < 0)
	    fprintf_unfiltered (file, "Raw value");
	  else if (regnum >= regcache->descr->nr_raw_registers)
	    fprintf_unfiltered (file, "<cooked>");
	  else if (!regcache_valid_p (regcache, regnum))
	    fprintf_unfiltered (file, "<invalid>");
	  else
	    {
	      regcache_raw_read (regcache, regnum, buf);
	      fprintf_unfiltered (file, "0x");
	      dump_endian_bytes (file, TARGET_BYTE_ORDER, buf,
				 regcache->descr->sizeof_register[regnum]);
	    }
	}

      /* Value, cooked.  */
      if (what_to_dump == regcache_dump_cooked)
	{
	  if (regnum < 0)
	    fprintf_unfiltered (file, "Cooked value");
	  else
	    {
	      regcache_cooked_read (regcache, regnum, buf);
	      fprintf_unfiltered (file, "0x");
	      dump_endian_bytes (file, TARGET_BYTE_ORDER, buf,
				 regcache->descr->sizeof_register[regnum]);
	    }
	}

      /* Group members.  */
      if (what_to_dump == regcache_dump_groups)
	{
	  if (regnum < 0)
	    fprintf_unfiltered (file, "Groups");
	  else
	    {
	      const char *sep = "";
	      struct reggroup *group;
	      for (group = reggroup_next (gdbarch, NULL);
		   group != NULL;
		   group = reggroup_next (gdbarch, group))
		{
		  if (gdbarch_register_reggroup_p (gdbarch, regnum, group))
		    {
		      fprintf_unfiltered (file, "%s%s", sep, reggroup_name (group));
		      sep = ",";
		    }
		}
	    }
	}

      fprintf_unfiltered (file, "\n");
    }

  if (footnote_register_size)
    fprintf_unfiltered (file, "*%d: Inconsistent register sizes.\n",
			footnote_register_size);
  if (footnote_register_offset)
    fprintf_unfiltered (file, "*%d: Inconsistent register offsets.\n",
			footnote_register_offset);
  if (footnote_register_type_name_null)
    fprintf_unfiltered (file, 
			"*%d: Register type's name NULL.\n",
			footnote_register_type_name_null);
  do_cleanups (cleanups);
}

static void
regcache_print (char *args, enum regcache_dump_what what_to_dump)
{
  if (args == NULL)
    regcache_dump (current_regcache, gdb_stdout, what_to_dump);
  else
    {
      struct ui_file *file = gdb_fopen (args, "w");
      if (file == NULL)
	perror_with_name (_("maintenance print architecture"));
      regcache_dump (current_regcache, file, what_to_dump);    
      ui_file_delete (file);
    }
}

static void
maintenance_print_registers (char *args, int from_tty)
{
  regcache_print (args, regcache_dump_none);
}

static void
maintenance_print_raw_registers (char *args, int from_tty)
{
  regcache_print (args, regcache_dump_raw);
}

static void
maintenance_print_cooked_registers (char *args, int from_tty)
{
  regcache_print (args, regcache_dump_cooked);
}

static void
maintenance_print_register_groups (char *args, int from_tty)
{
  regcache_print (args, regcache_dump_groups);
}

extern initialize_file_ftype _initialize_regcache; /* -Wmissing-prototype */

void
_initialize_regcache (void)
{
  regcache_descr_handle = gdbarch_data_register_post_init (init_regcache_descr);
  DEPRECATED_REGISTER_GDBARCH_SWAP (current_regcache);
  deprecated_register_gdbarch_swap (NULL, 0, build_regcache);

  observer_attach_target_changed (regcache_observer_target_changed);

  add_com ("flushregs", class_maintenance, reg_flush_command,
	   _("Force gdb to flush its register cache (maintainer command)"));

   /* Initialize the thread/process associated with the current set of
      registers.  For now, -1 is special, and means `no current process'.  */
  registers_ptid = pid_to_ptid (-1);

  add_cmd ("registers", class_maintenance, maintenance_print_registers, _("\
Print the internal register configuration.\n\
Takes an optional file parameter."), &maintenanceprintlist);
  add_cmd ("raw-registers", class_maintenance,
	   maintenance_print_raw_registers, _("\
Print the internal register configuration including raw values.\n\
Takes an optional file parameter."), &maintenanceprintlist);
  add_cmd ("cooked-registers", class_maintenance,
	   maintenance_print_cooked_registers, _("\
Print the internal register configuration including cooked values.\n\
Takes an optional file parameter."), &maintenanceprintlist);
  add_cmd ("register-groups", class_maintenance,
	   maintenance_print_register_groups, _("\
Print the internal register configuration including each register's group.\n\
Takes an optional file parameter."),
	   &maintenanceprintlist);

}

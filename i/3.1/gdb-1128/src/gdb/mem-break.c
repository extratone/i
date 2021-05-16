/* Simulate breakpoints by patching locations in the target system, for GDB.

   Copyright 1990, 1991, 1992, 1993, 1995, 1997, 1998, 1999, 2000,
   2002 Free Software Foundation, Inc.

   Contributed by Cygnus Support.  Written by John Gilmore.

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

/* This file is only useful if BREAKPOINT_FROM_PC is set.  If not, we
   punt.  */

#include "symtab.h"
#include "breakpoint.h"
#include "inferior.h"
#include "target.h"


/* Insert a breakpoint on targets that don't have any better breakpoint
   support.  We read the contents of the target location and stash it,
   then overwrite it with a breakpoint instruction.  ADDR is the target
   location in the target machine.  CONTENTS_CACHE is a pointer to 
   memory allocated for saving the target contents.  It is guaranteed
   by the caller to be long enough to save BREAKPOINT_LEN bytes (this
   is accomplished via BREAKPOINT_MAX).  */

int
default_memory_insert_breakpoint (CORE_ADDR addr, bfd_byte *contents_cache)
{
  int val;
  const unsigned char *bp;
  int bplen;
  /* APPLE LOCAL: Override trust-readonly-sections.  */
  int old_readonly;
  struct cleanup *reset_trust_readonly;
  /* END APPLE LOCAL */

  /* Determine appropriate breakpoint contents and size for this address.  */
  bp = BREAKPOINT_FROM_PC (&addr, &bplen);
  if (bp == NULL)
    error (_("Software breakpoints not implemented for this target."));

  /* APPLE LOCAL: For breakpoints we should override the trust_readonly setting.  */
  old_readonly = set_trust_readonly (0);
  reset_trust_readonly = make_cleanup (set_trust_readonly_cleanup, (void *) old_readonly);
  /* END APPLE LOCAL */
  /* Save the memory contents.  */
  val = target_read_memory (addr, contents_cache, bplen);

  /* Write the breakpoint.  */
  if (val == 0)
    val = target_write_memory (addr, bp, bplen);

  do_cleanups (reset_trust_readonly);
  return val;
}


int
default_memory_remove_breakpoint (CORE_ADDR addr, bfd_byte *contents_cache)
{
  const bfd_byte *bp;
  int bplen;
  int val;
  unsigned char cur_contents[BREAKPOINT_MAX];
  /* APPLE LOCAL: Override trust-readonly-sections.  */
  int old_readonly;
  struct cleanup *reset_trust_readonly;
  /* END APPLE LOCAL */

  /* Determine appropriate breakpoint contents and size for this address.  */
  bp = BREAKPOINT_FROM_PC (&addr, &bplen);
  if (bp == NULL)
    error (_("Software breakpoints not implemented for this target."));

  /* APPLE LOCAL: If a program has self modifying code, it might have
     overwritten our trap between the time we last inserted it and
     now.  So if the current contents is not our trap, let's use what
     got written there as the contents_cache..  */
  /* ALSO, we have to unset trust_readonly if it's set, because for 
     this we're counting on getting the value we wrote there.  */

  old_readonly = set_trust_readonly (0);
  reset_trust_readonly = make_cleanup (set_trust_readonly_cleanup, (void *) old_readonly);
  val = target_read_memory (addr, cur_contents, bplen);
  
  /* I don't know why we wouldn't be able to read the memory where we
     plan to re-insert to old code, but if we can't we aren't going
     to write it either, most likely...  */
  
  if (val != 0)
    goto cleanup;
  
  if (memcmp (cur_contents, bp, bplen) != 0)
    {
      memcpy (contents_cache, cur_contents, bplen);
      val = 0;
    }
  else
    val = target_write_memory (addr, contents_cache, bplen);
  
 cleanup:
  do_cleanups(reset_trust_readonly);
  return val;
  /* END APPLE LOCAL */
}


int
memory_insert_breakpoint (CORE_ADDR addr, bfd_byte *contents_cache)
{
  return MEMORY_INSERT_BREAKPOINT(addr, contents_cache);
}

int
memory_remove_breakpoint (CORE_ADDR addr, bfd_byte *contents_cache)
{
  return MEMORY_REMOVE_BREAKPOINT(addr, contents_cache);
}

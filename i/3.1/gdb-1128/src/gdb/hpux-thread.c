/* Low level interface for debugging HPUX/DCE threads for GDB, the GNU
   debugger.

   Copyright 1996, 1998, 1999, 2000, 2001, 2004 Free Software
   Foundation, Inc.

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

/* This module implements a sort of half target that sits between the
   machine-independent parts of GDB and the ptrace interface (infptrace.c) to
   provide access to the HPUX user-mode thread implementation.

   HPUX threads are true user-mode threads, which are invoked via the cma_*
   and pthread_* (DCE and Posix respectivly) interfaces.  These are mostly
   implemented in user-space, with all thread context kept in various
   structures that live in the user's heap.  For the most part, the kernel has
   no knowlege of these threads.

 */

#include "defs.h"

#define _CMA_NOWRAPPERS_

#include <cma_tcb_defs.h>
#include <cma_deb_core.h>
#include "gdbthread.h"
#include "target.h"
#include "inferior.h"
#include "regcache.h"
#include <fcntl.h>
#include <string.h>
#include "gdb_stat.h"
#include "gdbcore.h"
#include "gdbcmd.h"
#include "hppa-tdep.h"

extern int child_suppress_run;

extern void _initialize_hpux_thread (void);

struct string_map
  {
    int num;
    char *str;
  };

static int hpux_thread_active = 0;

static ptid_t main_ptid;		/* Real process ID */

static struct cleanup *save_inferior_pid (void);

static void hpux_thread_resume (ptid_t ptid, int step,
                                enum target_signal signo);

static void init_hpux_thread_ops (void);

static struct target_ops hpux_thread_ops;

static ptid_t find_active_thread (void);

static int cached_thread;
static cma__t_int_tcb cached_tcb;

static ptid_t
find_active_thread (void)
{
  static cma__t_int_tcb tcb;
  CORE_ADDR tcb_ptr;

  assert (P_cma__g_current_thread != 0);
  read_memory ((CORE_ADDR) P_cma__g_current_thread,
	       (char *) &tcb_ptr,
	       sizeof tcb_ptr);

  read_memory (tcb_ptr, (char *) &tcb, sizeof tcb);

  return (ptid_build (PIDGET (main_ptid), 0,
                      cma_thread_get_unique (&tcb.prolog.client_thread)));
}

static cma__t_int_tcb *find_tcb (ptid_t ptid);

static cma__t_int_tcb *
find_tcb (ptid_t ptid)
{
  cma__t_known_object queue_header;
  cma__t_queue *queue_ptr;
  int thread = ptid_get_tid (ptid);

  if (thread == cached_thread)
    return &cached_tcb;

  assert (P_cma__g_known_threads != 0);
  read_memory (P_cma__g_known_threads, (char *) &queue_header, sizeof queue_header);

  for (queue_ptr = queue_header.queue.flink;
       queue_ptr != (cma__t_queue *) P_cma__g_known_threads;
       queue_ptr = cached_tcb.threads.flink)
    {
      cma__t_int_tcb *tcb_ptr;

      tcb_ptr = cma__base (queue_ptr, threads, cma__t_int_tcb);

      QUIT;

      read_memory ((CORE_ADDR) tcb_ptr, (char *) &cached_tcb, sizeof cached_tcb);

      if ((thread >> 16) == 0)
	{
	  cached_thread = thread;
	  return &cached_tcb;
	}

      if (cached_tcb.header.type == cma__c_obj_tcb)
	if (cma_thread_get_unique (&cached_tcb.prolog.client_thread) == thread)
	  {
	    cached_thread = thread;
	    return &cached_tcb;
	  }
    }

  error (_("Can't find TCB %d"), thread);
  return NULL;
}

static void
check_new_threads ()
{
  cma__t_int_tcb tcb;
  cma__t_known_object queue_header;
  cma__t_queue *queue_ptr = NULL;

  init_thread_list ();

  assert (P_cma__g_known_threads != 0);
  read_memory (P_cma__g_known_threads, (char *) &queue_header, sizeof queue_header);

  for (queue_ptr = queue_header.queue.flink;
       queue_ptr != (cma__t_queue *) P_cma__g_known_threads;
       queue_ptr = tcb.threads.flink)
    {
      cma__t_int_tcb *tcb_ptr;

      tcb_ptr = cma__base (queue_ptr, threads, cma__t_int_tcb);

      QUIT;

      read_memory ((CORE_ADDR) tcb_ptr, (char *) &tcb, sizeof tcb);

      if (tcb.header.type == cma__c_obj_tcb)
	{
	  unsigned int tpid = (cma_thread_get_unique (&tcb.prolog.client_thread) << 16) | main_pid;
	  if (!in_thread_list (tpid))
	    {
	      add_thread (tpid);
	    }
	}
    }
}

/* Most target vector functions from here on actually just pass through to
   inftarg.c, as they don't need to do anything specific for threads.  */

static void
hpux_thread_open (char *arg, int from_tty)
{
  deprecated_child_ops.to_open (arg, from_tty);
}

/* Attach to process PID, then initialize for debugging it
   and wait for the trace-trap that results from attaching.  */

static void
hpux_thread_attach (char *args, int from_tty)
{
  deprecated_child_ops.to_attach (args, from_tty);

  /* XXX - might want to iterate over all the threads and register them. */
}

/* Take a program previously attached to and detaches it.
   The program resumes execution and will no longer stop
   on signals, etc.  We'd better not have left any breakpoints
   in the program or it'll die when it hits one.  For this
   to work, it may be necessary for the process to have been
   previously attached.  It *might* work if the program was
   started via the normal ptrace (PTRACE_TRACEME).  */

static void
hpux_thread_detach (char *args, int from_tty)
{
  deprecated_child_ops.to_detach (args, from_tty);
}

/* Resume execution of process PID.  If STEP is nozero, then
   just single step it.  If SIGNAL is nonzero, restart it with that
   signal activated.  We may have to convert pid from a thread-id to an LWP id
   for procfs.  */

static void
hpux_thread_resume (ptid_t ptid, int step, enum target_signal signo)
{
  struct cleanup *old_chain;

  old_chain = save_inferior_ptid ();

  ptid = main_ptid;
  inferior_ptid = main_ptid;

#if 0
  if (pid != -1)
    {
      pid = thread_to_lwp (pid, -2);
      if (pid == -2)		/* Inactive thread */
	error (_("This version of Solaris can't start inactive threads."));
    }
#endif

  deprecated_child_ops.to_resume (ptid, step, signo);

  cached_thread = 0;

  do_cleanups (old_chain);
}

/* Wait for any threads to stop.  We may have to convert PID from a thread id
   to a LWP id, and vice versa on the way out.  */

static ptid_t
hpux_thread_wait (ptid_t ptid, struct target_waitstatus *ourstatus)
{
  ptid_t rtnval;
  struct cleanup *old_chain;

  old_chain = save_inferior_pid ();

  inferior_pid = main_pid;

  if (!ptid_equal (ptid, minus_one_ptid))
    ptid = main_ptid;

  rtnval = child_ops.to_wait (ptid, ourstatus);

  rtnval = deprecated_child_ops.to_wait (ptid, ourstatus);

  rtnval = find_active_thread ();

  do_cleanups (old_chain);

  return rtnval;
}

static char regmap[] =
{
  -2, -1, -1, 0, 4, 8, 12, 16, 20, 24,	/* flags, r1 -> r9 */
  28, 32, 36, 40, 44, 48, 52, 56, 60, -1,	/* r10 -> r19 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,	/* r20 -> r29 */

  /* r30, r31, sar, pcoqh, pcsqh, pcoqt, pcsqt, eiem, iir, isr */
  -2, -1, -1, -2, -1, -1, -1, -1, -1, -1,

  /* ior, ipsw, goto, sr4, sr0, sr1, sr2, sr3, sr5, sr6 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,

  /* sr7, cr0, cr8, cr9, ccr, cr12, cr13, cr24, cr25, cr26 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,

  -1, -1, -1, -1,		/* mpsfu_high, mpsfu_low, mpsfu_ovflo, pad */
  144, -1, -1, -1, -1, -1, -1, -1,	/* fpsr, fpe1 -> fpe7 */
  -1, -1, -1, -1, -1, -1, -1, -1,	/* fr4 -> fr7 */
  -1, -1, -1, -1, -1, -1, -1, -1,	/* fr8 -> fr11 */
  136, -1, 128, -1, 120, -1, 112, -1,	/* fr12 -> fr15 */
  104, -1, 96, -1, 88, -1, 80, -1,	/* fr16 -> fr19 */
  72, -1, 64, -1, -1, -1, -1, -1,	/* fr20 -> fr23 */
  -1, -1, -1, -1, -1, -1, -1, -1,	/* fr24 -> fr27 */
  -1, -1, -1, -1, -1, -1, -1, -1,	/* fr28 -> fr31 */
};

static void
hpux_thread_fetch_registers (int regno)
{
  cma__t_int_tcb tcb, *tcb_ptr;
  struct cleanup *old_chain;
  int i;
  int first_regno, last_regno;

  tcb_ptr = find_tcb (inferior_ptid);

  old_chain = save_inferior_ptid ();

  inferior_ptid = main_ptid;

  if (tcb_ptr->state == cma__c_state_running)
    {
      deprecated_child_ops.to_fetch_registers (regno);

      do_cleanups (old_chain);

      return;
    }

  if (regno == -1)
    {
      first_regno = 0;
      last_regno = NUM_REGS - 1;
    }
  else
    {
      first_regno = regno;
      last_regno = regno;
    }

  for (regno = first_regno; regno <= last_regno; regno++)
    {
      if (regmap[regno] == -1)
	deprecated_child_ops.to_fetch_registers (regno);
      else
	{
	  unsigned char buf[MAX_REGISTER_SIZE];
	  CORE_ADDR sp;

	  sp = (CORE_ADDR) tcb_ptr->static_ctx.sp - 160;

	  if (regno == HPPA_FLAGS_REGNUM)
	    /* Flags must be 0 to avoid bogus value for SS_INSYSCALL */
	    memset (buf, '\000', register_size (current_gdbarch, regno));
	  else if (regno == HPPA_SP_REGNUM)
	    store_unsigned_integer (buf, sizeof sp, sp);
	  else if (regno == HPPA_PCOQ_HEAD_REGNUM)
	    read_memory (sp - 20, buf, register_size (current_gdbarch, regno));
	  else
	    read_memory (sp + regmap[regno], buf, register_size (current_gdbarch, regno));

	  regcache_raw_supply (current_regcache, regno, buf);
	}
    }

  do_cleanups (old_chain);
}

static void
hpux_thread_store_registers (int regno)
{
  cma__t_int_tcb tcb, *tcb_ptr;
  struct cleanup *old_chain;
  int i;
  int first_regno, last_regno;

  tcb_ptr = find_tcb (inferior_ptid);

  old_chain = save_inferior_ptid ();

  inferior_ptid = main_ptid;

  if (tcb_ptr->state == cma__c_state_running)
    {
      deprecated_child_ops.to_store_registers (regno);

      do_cleanups (old_chain);

      return;
    }

  if (regno == -1)
    {
      first_regno = 0;
      last_regno = NUM_REGS - 1;
    }
  else
    {
      first_regno = regno;
      last_regno = regno;
    }

  for (regno = first_regno; regno <= last_regno; regno++)
    {
      if (regmap[regno] == -1)
	deprecated_child_ops.to_store_registers (regno);
      else
	{
	  unsigned char buf[MAX_REGISTER_SIZE];
	  CORE_ADDR sp;

	  sp = (CORE_ADDR) tcb_ptr->static_ctx.sp - 160;

	  if (regno == HPPA_FLAGS_REGNUM)
	    deprecated_child_ops.to_store_registers (regno);	/* Let lower layer handle this... */
	  else if (regno == HPPA_SP_REGNUM)
	    {
	      regcache_raw_read (current_regcache, regno, buf);
	      write_memory ((CORE_ADDR) &tcb_ptr->static_ctx.sp, buf,
			    register_size (current_gdbarch, regno));
	      tcb_ptr->static_ctx.sp
		= (cma__t_hppa_regs *) ((CORE_ADDR) buf + 160);
	    }
	  else if (regno == HPPA_PCOQ_HEAD_REGNUM)
	    {
	      regcache_raw_read (current_regcache, regno, buf);
	      write_memory (sp - 20, buf,
			    register_size (current_gdbarch, regno));
	    }
	  else
	    {
	      regcache_raw_read (current_regcache, regno, buf);
	      write_memory (sp + regmap[regno], buf,
			    register_size (current_gdbarch, regno));
	    }
	}
    }

  do_cleanups (old_chain);
}

/* Get ready to modify the registers array.  On machines which store
   individual registers, this doesn't need to do anything.  On machines
   which store all the registers in one fell swoop, this makes sure
   that registers contains all the registers from the program being
   debugged.  */

static void
hpux_thread_prepare_to_store (void)
{
  deprecated_child_ops.to_prepare_to_store ();
}

static int
hpux_thread_xfer_memory (CORE_ADDR memaddr, char *myaddr, int len,
			 int dowrite, struct mem_attrib *attribs,
			 struct target_ops *target)
{
  int retval;
  struct cleanup *old_chain;

  old_chain = save_inferior_ptid ();

  inferior_ptid = main_ptid;

  retval = 
    deprecated_child_ops.deprecated_xfer_memory (memaddr, myaddr, len, dowrite, attribs, target);

  do_cleanups (old_chain);

  return retval;
}

/* Print status information about what we're accessing.  */

static void
hpux_thread_files_info (struct target_ops *ignore)
{
  deprecated_child_ops.to_files_info (ignore);
}

static void
hpux_thread_kill_inferior (void)
{
  deprecated_child_ops.to_kill ();
}

static void
hpux_thread_notice_signals (ptid_t ptid)
{
  deprecated_child_ops.to_notice_signals (ptid);
}

/* Fork an inferior process, and start debugging it with /proc.  */

static void
hpux_thread_create_inferior (char *exec_file, char *allargs, char **env,
			     int from_tty)
{
  deprecated_child_ops.to_create_inferior (exec_file, allargs, env, from_tty);

  if (hpux_thread_active)
    {
      main_ptid = inferior_ptid;

      push_target (&hpux_thread_ops);

      inferior_ptid = find_active_thread ();

      add_thread (inferior_ptid);
    }
}

/* This routine is called whenever a new symbol table is read in, or when all
   symbol tables are removed.  libthread_db can only be initialized when it
   finds the right variables in libthread.so.  Since it's a shared library,
   those variables don't show up until the library gets mapped and the symbol
   table is read in.  */

/* This new_objfile event is now managed by a chained function pointer. 
 * It is the callee's responsability to call the next client on the chain.
 */

/* Saved pointer to previous owner of the new_objfile event. */
static void (*target_new_objfile_chain) (struct objfile *);

void
hpux_thread_new_objfile (struct objfile *objfile)
{
  struct minimal_symbol *ms;

  if (!objfile)
    {
      hpux_thread_active = 0;
      goto quit;
    }

  ms = lookup_minimal_symbol ("cma__g_known_threads", NULL, objfile);

  if (!ms)
    goto quit;

  P_cma__g_known_threads = SYMBOL_VALUE_ADDRESS (ms);

  ms = lookup_minimal_symbol ("cma__g_current_thread", NULL, objfile);

  if (!ms)
    goto quit;

  P_cma__g_current_thread = SYMBOL_VALUE_ADDRESS (ms);

  hpux_thread_active = 1;
quit:
  /* Call predecessor on chain, if any. */
  if (target_new_objfile_chain)
    target_new_objfile_chain (objfile);
}

/* Clean up after the inferior dies.  */

static void
hpux_thread_mourn_inferior (void)
{
  deprecated_child_ops.to_mourn_inferior ();
}

/* Mark our target-struct as eligible for stray "run" and "attach" commands.  */

static int
hpux_thread_can_run (void)
{
  return child_suppress_run;
}

static int
hpux_thread_alive (ptid_t ptid)
{
  return (find_tcb (pid) != NULL);
}

static void
hpux_thread_stop (void)
{
  deprecated_child_ops.to_stop ();
}

/* Convert a pid to printable form. */

char *
hpux_pid_to_str (ptid_t ptid)
{
  static char buf[100];
  int pid = PIDGET (ptid);

  sprintf (buf, "Thread %ld", ptid_get_tid (ptid));

  return buf;
}

static void
init_hpux_thread_ops (void)
{
  hpux_thread_ops.to_shortname = "hpux-threads";
  hpux_thread_ops.to_longname = "HPUX threads and pthread.";
  hpux_thread_ops.to_doc = "HPUX threads and pthread support.";
  hpux_thread_ops.to_open = hpux_thread_open;
  hpux_thread_ops.to_attach = hpux_thread_attach;
  hpux_thread_ops.to_detach = hpux_thread_detach;
  hpux_thread_ops.to_resume = hpux_thread_resume;
  hpux_thread_ops.to_wait = hpux_thread_wait;
  hpux_thread_ops.to_fetch_registers = hpux_thread_fetch_registers;
  hpux_thread_ops.to_store_registers = hpux_thread_store_registers;
  hpux_thread_ops.to_prepare_to_store = hpux_thread_prepare_to_store;
  hpux_thread_ops.deprecated_xfer_memory = hpux_thread_xfer_memory;
  hpux_thread_ops.to_files_info = hpux_thread_files_info;
  hpux_thread_ops.to_insert_breakpoint = memory_insert_breakpoint;
  hpux_thread_ops.to_remove_breakpoint = memory_remove_breakpoint;
  hpux_thread_ops.to_terminal_init = terminal_init_inferior;
  hpux_thread_ops.to_terminal_inferior = terminal_inferior;
  hpux_thread_ops.to_terminal_ours_for_output = terminal_ours_for_output;
  hpux_thread_ops.to_terminal_save_ours = terminal_save_ours;
  hpux_thread_ops.to_terminal_ours = terminal_ours;
  hpux_thread_ops.to_terminal_info = child_terminal_info;
  hpux_thread_ops.to_kill = hpux_thread_kill_inferior;
  hpux_thread_ops.to_create_inferior = hpux_thread_create_inferior;
  hpux_thread_ops.to_mourn_inferior = hpux_thread_mourn_inferior;
  hpux_thread_ops.to_can_run = hpux_thread_can_run;
  hpux_thread_ops.to_notice_signals = hpux_thread_notice_signals;
  hpux_thread_ops.to_thread_alive = hpux_thread_alive;
  hpux_thread_ops.to_stop = hpux_thread_stop;
  hpux_thread_ops.to_stratum = process_stratum;
  hpux_thread_ops.to_has_all_memory = 1;
  hpux_thread_ops.to_has_memory = 1;
  hpux_thread_ops.to_has_stack = 1;
  hpux_thread_ops.to_has_registers = 1;
  hpux_thread_ops.to_has_execution = 1;
  hpux_thread_ops.to_magic = OPS_MAGIC;
}

void
_initialize_hpux_thread (void)
{
  struct cmd_list_element *cmd;

  init_hpux_thread_ops ();
  add_target (&hpux_thread_ops);

  child_suppress_run = 0;

  child_suppress_run = 1;
  /* Hook into new_objfile notification.  */
  target_new_objfile_chain = deprecated_target_new_objfile_hook;
  deprecated_target_new_objfile_hook  = hpux_thread_new_objfile;
}

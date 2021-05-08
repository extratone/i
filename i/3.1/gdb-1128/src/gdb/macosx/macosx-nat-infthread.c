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
#include "inferior.h"
#include "target.h"
#include "symfile.h"
#include "symtab.h"
#include "objfiles.h"
#include "gdbcmd.h"
#include "gdbthread.h"

#include <stdio.h>
#include <sys/param.h>
#include <sys/dir.h>

#include "macosx-nat-inferior.h"
#include "macosx-nat-inferior-util.h"
#include "macosx-nat-inferior-debug.h"
#include "macosx-nat-mutils.h"

extern macosx_inferior_status *macosx_status;

#define set_trace_bit(thread) modify_trace_bit (thread, 1)
#define clear_trace_bit(thread) modify_trace_bit (thread, 0)

/* We use this structure in iterate_over_threads to prune the
   thread list.  */
struct ptid_list
{
  unsigned int nthreads;
  ptid_t *ptids;
};

void
macosx_setup_registers_before_hand_call ()
{
  thread_t current_thread = ptid_get_tid (inferior_ptid);

  thread_abort_safely (current_thread);
}

#if defined (TARGET_I386)

/* Set/clear bit 8 (Trap Flag) of the EFLAGS processor control
   register to enable/disable single-step mode.  Handle new-style 
   32-bit, 64-bit, and old-style 32-bit interfaces in this function.  
   VALUE is a boolean, indicating whether to set (1) the Trap Flag 
   or clear it (0).  */

kern_return_t
modify_trace_bit (thread_t thread, int value)
{
  gdb_x86_thread_state_t state;
  unsigned int state_count = GDB_x86_THREAD_STATE_COUNT;
  kern_return_t kret;

  kret = thread_get_state (thread, GDB_x86_THREAD_STATE, 
                           (thread_state_t) &state, &state_count);
  if (kret == KERN_SUCCESS &&
      (state.tsh.flavor == GDB_x86_THREAD_STATE32 ||
       state.tsh.flavor == GDB_x86_THREAD_STATE64))
    {
      if (state.tsh.flavor == GDB_x86_THREAD_STATE32 
          && (state.uts.ts32.eflags & 0x100UL) != (value ? 1 : 0))
        {
          state.uts.ts32.eflags = 
                    (state.uts.ts32.eflags & ~0x100UL) | (value ? 0x100UL : 0);
          kret = thread_set_state (thread, GDB_x86_THREAD_STATE32, 
                                   (thread_state_t) & state.uts.ts32,
                                   GDB_x86_THREAD_STATE32_COUNT);
          MACH_PROPAGATE_ERROR (kret);
        }
      else if (state.tsh.flavor == GDB_x86_THREAD_STATE64 
               && (state.uts.ts64.rflags & 0x100UL) != (value ? 1 : 0))
        {
          state.uts.ts64.rflags = 
                     (state.uts.ts64.rflags & ~0x100UL) | (value ? 0x100UL : 0);
          kret = thread_set_state (thread, GDB_x86_THREAD_STATE, 
                                   (thread_state_t) &state, state_count);
          MACH_PROPAGATE_ERROR (kret);
        }
    }
  else
    {
      gdb_i386_thread_state_t state;
      
      state_count = GDB_i386_THREAD_STATE_COUNT;
      kret = thread_get_state (thread, GDB_i386_THREAD_STATE, 
                               (thread_state_t) &state, &state_count);
      MACH_PROPAGATE_ERROR (kret);

      if ((state.eflags & 0x100UL) != (value ? 1 : 0))
        {
          state.eflags = (state.eflags & ~0x100UL) | (value ? 0x100UL : 0);
          kret = thread_set_state (thread, GDB_i386_THREAD_STATE, 
                                   (thread_state_t) &state, state_count);
          MACH_PROPAGATE_ERROR (kret);
        }
    }

  return KERN_SUCCESS;
}

#elif defined (TARGET_POWERPC)

#include "ppc-macosx-thread-status.h"

kern_return_t
modify_trace_bit (thread_t thread, int value)
{
  gdb_ppc_thread_state_64_t state;
  unsigned int state_count = GDB_PPC_THREAD_STATE_64_COUNT;
  kern_return_t kret;

  kret =
    thread_get_state (thread, GDB_PPC_THREAD_STATE_64,
                      (thread_state_t) & state, &state_count);
  MACH_PROPAGATE_ERROR (kret);

  if ((state.srr1 & 0x400UL) != (value ? 1 : 0))
    {
      state.srr1 = (state.srr1 & ~0x400UL) | (value ? 0x400UL : 0);
      kret =
        thread_set_state (thread, GDB_PPC_THREAD_STATE_64,
                          (thread_state_t) & state, state_count);
      MACH_PROPAGATE_ERROR (kret);
    }

  return KERN_SUCCESS;

}

#elif defined (TARGET_ARM)

#include "arm-macosx-thread-status.h"
#include "arm-tdep.h"

// BCR address match type
#define BCR_M_IMVA_MATCH        ((uint32_t)(0u << 21))
#define BCR_M_CONTEXT_ID_MATCH  ((uint32_t)(1u << 21))
#define BCR_M_IMVA_MISMATCH     ((uint32_t)(2u << 21))
#define BCR_M_RESERVED          ((uint32_t)(3u << 21))

// Link a BVR/BCR or WVR/WCR pair to another
#define E_ENABLE_LINKING	((uint32_t)(1u << 20))

// Byte Address Select
#define BAS_IMVA_PLUS_0		((uint32_t)(1u << 5))
#define BAS_IMVA_PLUS_1		((uint32_t)(1u << 6))
#define BAS_IMVA_PLUS_2		((uint32_t)(1u << 7))
#define BAS_IMVA_PLUS_3		((uint32_t)(1u << 8))
#define BAS_IMVA_0_1		((uint32_t)(3u << 5))
#define BAS_IMVA_2_3		((uint32_t)(3u << 7))
#define BAS_IMVA_ALL		((uint32_t)(0xfu << 5))

// Break only in priveleged or user mode
#define S_RSVD			((uint32_t)(0u << 1))
#define S_PRIV			((uint32_t)(1u << 1))
#define S_USER			((uint32_t)(2u << 1))
#define S_PRIV_USER		((S_PRIV) | (S_USER))

#define BCR_ENABLE		((uint32_t)(1u))
#define WCR_ENABLE		((uint32_t)(1u))

// Watchpoint load/store
#define WCR_LOAD		((uint32_t)(1u << 3))
#define WCR_STORE		((uint32_t)(1u << 4))

ULONGEST read_memory_unsigned_integer (CORE_ADDR memaddr, int len);

extern arm_macosx_tdep_inf_status_t arm_macosx_tdep_inf_status;

kern_return_t
modify_trace_bit (thread_t thread, int value)
{
  kern_return_t kret;
  gdb_arm_thread_debug_state_t dbg;
  unsigned int state_count;
  const uint32_t hw_idx = 0;
  int update_dregs = 0;
  /* Set the debug state to all zeros and only fill it in if we are enabling
     a hardware breakpoint.  */
  state_count = GDB_ARM_THREAD_DEBUG_STATE_COUNT;
  kret = thread_get_state (thread, GDB_ARM_THREAD_DEBUG_STATE,
			   (thread_state_t) &dbg, &state_count);
  inferior_debug(3, "modify_trace_bit(%4.4x, %i): "
		 "thread_get_state(%4.4x, %i, ***, %i) => kret = %8.8x, "
		 "BRP%u = 0x%8.8x 0x%8.8x, state_count = %i\n", 
		 thread, value, thread, GDB_ARM_THREAD_DEBUG_STATE, 
		 GDB_ARM_THREAD_DEBUG_STATE_COUNT, kret, hw_idx, 
		 dbg.bvr[hw_idx], dbg.bcr[hw_idx], state_count);
  MACH_PROPAGATE_ERROR (kret);

  if (value)
    {
      /* Enable trace bit.  */
      arm_macosx_tdep_inf_status.macosx_half_step_pc = (CORE_ADDR)-1;

      /* Read the general regs first so we can get the PC.  */
      gdb_arm_thread_state_t gpr;
      state_count = GDB_ARM_THREAD_STATE_COUNT;
      kret = thread_get_state (thread, GDB_ARM_THREAD_STATE,
			       (thread_state_t) &gpr, &state_count);
      MACH_PROPAGATE_ERROR (kret);

      inferior_debug(3, "modify_trace_bit(%4.4x, %i): pc = 0x%8.8x\n", 
		     gpr.r[15]);
      
      /* Set a hardware breakpoint that will stop when the PC changes.  */
      
      /* Set the current PC as the breakpoint address with bits 1:0 removed.  */
      dbg.bvr[hw_idx] = gpr.r[15] & 0xFFFFFFFCu;
      /* Stop on address mismatch (BCR_M_IMVA_MISMATCH), when any address bits 
         change (BAS_IMVA_ALL), stop only in user mode only (S_USER), and 
	 enable this hardware breakpoint (BCR_ENABLE).  */
      dbg.bcr[hw_idx] = BCR_M_IMVA_MISMATCH | S_USER | BCR_ENABLE;
      
      if (gpr.cpsr & FLAG_T)
	{
	  // Thumb breakpoint
	  if (gpr.r[15] & 2)
	    dbg.bcr[hw_idx] |= BAS_IMVA_2_3;
	  else
	    dbg.bcr[hw_idx] |= BAS_IMVA_0_1;

	  uint32_t insn = read_memory_unsigned_integer (gpr.r[15], 2);
	  if (((insn & 0xE000) == 0xE000) && insn & 0x1800)
	    {
	      // 32 bit thumb opcode...
	      if (gpr.r[15] & 2)
		{
		  // We can't take care of a 32 bit thumb instruction that lies
		  // on a 2 byte boundary with a single IVA mismatch. We will 
		  // need to chain an extra hardware single step in order to 
		  // complete this single step since some architectures 
		  // (pre ARMv6T2) treat 32 bit thumb instructions as two fixed
		  // length opcodes, later cores treat 32 bit thumb instructions
		  // as a single opcode (variable length). Store the address of
		  // this 32 bit opcode along with the ptid, so we can complete
		  // the single step without stopping half way through this
		  // opcode.
		  arm_macosx_tdep_inf_status.macosx_half_step_pc = gpr.r[15] + 2;
		}
	      else
		{
		  // This 32 bit thumb instruction starts on a four byte 
		  // boundary so we can use single IVA mismtach to complete
		  // the step.
		  dbg.bcr[hw_idx] |= BAS_IMVA_ALL;
		}
	    }
	}
      else
	{
	  // ARM breakpoint, stop when any address bits change
	  dbg.bcr[hw_idx] |= BAS_IMVA_ALL;
	}
      update_dregs = 1;
    }
  else
    {
      /* Disable trace bit.  */
      if (dbg.bcr[hw_idx] & BCR_ENABLE)
	{
	  dbg.bvr[hw_idx] = 0;
	  dbg.bcr[hw_idx] = 0;
	  update_dregs = 1;
	}
    }
    
  if (update_dregs)
    {
      state_count = GDB_ARM_THREAD_DEBUG_STATE_COUNT;
      kret = thread_set_state (thread, GDB_ARM_THREAD_DEBUG_STATE,
			       (thread_state_t) &dbg, 
			       GDB_ARM_THREAD_DEBUG_STATE_COUNT);
      inferior_debug(3, "modify_trace_bit(%4.4x, %i): thread_set_state(%4.4x, "
		     "%i, ***, %i) => kret = %8.8x, BRP%u = %8.8x %8.8x\n", 
		     thread, value, thread, GDB_ARM_THREAD_DEBUG_STATE, 
		     state_count, kret, hw_idx, dbg.bvr[hw_idx], 
		     dbg.bcr[hw_idx]);
      MACH_PROPAGATE_ERROR (kret);
    }

  return KERN_SUCCESS;
}
#else
#error "unknown architecture"
#endif

void
prepare_threads_after_stop (struct macosx_inferior_status *inferior)
{
  thread_array_t thread_list = NULL;
  unsigned int nthreads = 0;
  kern_return_t kret;
  unsigned int i;

  if (inferior->exception_status.saved_exceptions_stepping)
    {
      kret = macosx_restore_exception_ports (inferior->task,
                                             &inferior->exception_status.
                                             saved_exceptions_step);
      MACH_CHECK_ERROR (kret);
      inferior->exception_status.saved_exceptions_stepping = 0;
    }

  kret = task_threads (inferior->task, &thread_list, &nthreads);
  MACH_CHECK_ERROR (kret);

  macosx_check_new_threads (thread_list, nthreads);

  for (i = 0; i < nthreads; i++)
    {

      ptid_t ptid;
      struct thread_info *tp = NULL;

      ptid = ptid_build (inferior->pid, 0, thread_list[i]);
      tp = find_thread_pid (ptid);
      CHECK_FATAL (tp != NULL);
      if (inferior_debug_flag >= 2)
        {
          struct thread_basic_info info;
          unsigned int info_count = THREAD_BASIC_INFO_COUNT;
          kern_return_t kret;

          kret =
            thread_info (thread_list[i], THREAD_BASIC_INFO,
                         (thread_info_t) & info, &info_count);
          MACH_CHECK_ERROR (kret);

          if (tp->gdb_suspend_count > 0)
            inferior_debug (3, "**  Resuming thread 0x%x, gdb suspend count: "
                            "%d, real suspend count: %d\n",
                            thread_list[i], tp->gdb_suspend_count,
                            info.suspend_count);
	  else if (tp->gdb_suspend_count < 0)
	    inferior_debug (3, "**  Re-suspending thread 0x%x, original suspend count: "
                            "%d\n",
                            thread_list[i], tp->gdb_suspend_count);
          else
            inferior_debug (3, "**  Thread 0x%x was not suspended from gdb, "
                            "real suspend count: %d\n",
                            thread_list[i], info.suspend_count);
        }
      if (tp->gdb_suspend_count > 0)
	{
	  while (tp->gdb_suspend_count > 0)
	    {
	      thread_resume (thread_list[i]);
	      tp->gdb_suspend_count--;
	    }
	}
      else if (tp->gdb_suspend_count < 0)
	{
	  while (tp->gdb_suspend_count < 0)
	    {
	      thread_suspend (thread_list[i]);
	      tp->gdb_suspend_count++;
	    }
	}


      kret = clear_trace_bit (thread_list[i]);
      MACH_WARN_ERROR (kret);
    }

  kret =
    vm_deallocate (mach_task_self (), (vm_address_t) thread_list,
                   (nthreads * sizeof (int)));
  MACH_WARN_ERROR (kret);
}

void prepare_threads_before_run
  (struct macosx_inferior_status *inferior, int step, thread_t current,
   int stop_others)
{
  thread_array_t thread_list = NULL;
  unsigned int nthreads = 0;
  kern_return_t kret;
  unsigned int i;

  prepare_threads_after_stop (inferior);

  if (step || stop_others)
    {
      struct thread_basic_info info;
      unsigned int info_count = THREAD_BASIC_INFO_COUNT;
      kern_return_t kret;

      kret =
        thread_info (current, THREAD_BASIC_INFO, (thread_info_t) & info,
                     &info_count);
      MACH_CHECK_ERROR (kret);

      if (info.suspend_count != 0)
        {
          if (step)
	    {
	      ptid_t ptid;
	      struct thread_info *tp = NULL;

	      ptid = ptid_build (inferior->pid, 0, current);
	      tp = find_thread_pid (ptid);
	      CHECK_FATAL (tp != NULL);
	      inferior_debug (3, "Resuming to single-step thread 0x%x "
			      "(thread is already suspended from outside of GDB)",
			      current);
	      while (info.suspend_count > 0)
		{
		  tp->gdb_suspend_count--;
		  info.suspend_count--;
		  thread_resume (current);
		}
	    }
          else
            error ("Unable to run only thread 0x%x "
                   "(thread is already suspended from outside of GDB)",
                   current);
        }
    }

  kret = task_threads (inferior->task, &thread_list, &nthreads);
  MACH_CHECK_ERROR (kret);

  if (step)
    inferior_debug (3, "*** Suspending threads to step: 0x%x\n", current);

  for (i = 0; i < nthreads; i++)
    {

      /* Don't need to do this either, since it is already done in prepare_threads_after_stop
         kret = clear_trace_bit (thread_list[i]);

         MACH_CHECK_ERROR (kret);
       */

      if ((stop_others) && (thread_list[i] != current))
        {

          ptid_t ptid;
          struct thread_info *tp = NULL;

          ptid = ptid_build (inferior->pid, 0, thread_list[i]);
          tp = find_thread_pid (ptid);
          CHECK_FATAL (tp != NULL);
          kret = thread_suspend (thread_list[i]);
          MACH_CHECK_ERROR (kret);
          tp->gdb_suspend_count++;
          inferior_debug (3, "*** Suspending thread 0x%x, suspend count %d\n",
                          thread_list[i], tp->gdb_suspend_count);
        }
      else if (stop_others)
        inferior_debug (3, "*** Allowing thread 0x%x to run from gdb\n",
                        thread_list[i]);
    }

  kret =
    vm_deallocate (mach_task_self (), (vm_address_t) thread_list,
                   (nthreads * sizeof (int)));
  MACH_CHECK_ERROR (kret);

  if (step)
    {
      set_trace_bit (current);
    }

  if (step)
    {
      kret =
        macosx_save_exception_ports (inferior->task,
                                     &inferior->exception_status.
                                     saved_exceptions_step);
      MACH_CHECK_ERROR (kret);
      inferior->exception_status.saved_exceptions_stepping = 1;
    }
}

char *
unparse_run_state (int run_state)
{
  switch (run_state)
    {
    case TH_STATE_RUNNING:
      return "RUNNING";
    case TH_STATE_STOPPED:
      return "STOPPED";
    case TH_STATE_WAITING:
      return "WAITING";
    case TH_STATE_UNINTERRUPTIBLE:
      return "UNINTERRUPTIBLE";
    case TH_STATE_HALTED:
      return "HALTED";
    default:
      return "[UNKNOWN]";
    }
}

/* get_application_thread_port returns the thread port number in the
   application's port namespace.  We get this so that we can present
   the user with the same port number they would see if they store
   away the thread id in their program.  It is for display purposes 
   only.  */

thread_t
get_application_thread_port (thread_t our_name)
{
  mach_msg_type_number_t i;
  mach_port_name_array_t names;
  mach_msg_type_number_t names_count;
  mach_port_type_array_t types;
  mach_msg_type_number_t types_count;
  mach_port_t match = 0;
  kern_return_t ret;

  /* To get the application name, we have to iterate over all the ports
     in the application and extract a right for them.  The right will include
     the port name in our port namespace, so we can use that to find the
     thread we are looking for.  Of course, we don't actually need another
     right to each of these ports, so we deallocate it when we are done.  */

  ret = mach_port_names (macosx_status->task, &names, &names_count, &types,
                   &types_count);
  if (ret != KERN_SUCCESS)
    {
      warning ("Error %d getting port names from mach_port_names", ret);
      return (thread_t) 0x0;
    }

  for (i = 0; i < names_count; i++)
    {
      mach_port_t local_name;
      mach_msg_type_name_t local_type;

      ret = mach_port_extract_right (macosx_status->task,
                                     names[i],
                                     MACH_MSG_TYPE_COPY_SEND,
                                     &local_name, &local_type);
      if (ret == KERN_SUCCESS)
        {
          mach_port_deallocate (mach_task_self (), local_name);
          if (local_name == our_name)
            {
              match = names[i];
              break;
            }
        }
    }

  vm_deallocate (mach_task_self (), (vm_address_t) names,
                 names_count * sizeof (mach_port_t));
  vm_deallocate (mach_task_self (), (vm_address_t) types,
                 types_count * sizeof (mach_port_t));

  return (thread_t) match;
}

void
print_thread_info (thread_t tid)
{
  struct thread_basic_info info;
  unsigned int info_count = THREAD_BASIC_INFO_COUNT;
  kern_return_t kret;
  thread_t app_thread_name;

  kret =
    thread_info (tid, THREAD_BASIC_INFO, (thread_info_t) & info, &info_count);
  MACH_CHECK_ERROR (kret);

  /* FIXME: We store the application port name in the private structure
     in the thread_info.  Can we use that here rather than having to
     call get_application_thread_port? */

  app_thread_name = get_application_thread_port (tid);

  printf_filtered ("Thread 0x%lx (local 0x%lx) has current state \"%s\"\n",
                   (unsigned long) app_thread_name,
                   (unsigned long) tid, unparse_run_state (info.run_state));
  printf_filtered ("Thread 0x%lx has a suspend count of %d",
                   (unsigned long) app_thread_name, info.suspend_count);
  if (info.sleep_time == 0)
    {
      printf_filtered (".\n");
    }
  else
    {
      printf_filtered (", and has been sleeping for %d seconds.\n",
                       info.sleep_time);
    }
}

void
info_task_command (char *args, int from_tty)
{
  struct task_basic_info info;
  unsigned int info_count = TASK_BASIC_INFO_COUNT;

  thread_array_t thread_list = NULL;
  unsigned int nthreads = 0;

  kern_return_t kret;
  unsigned int i;

  kret =
    task_info (macosx_status->task, TASK_BASIC_INFO, (task_info_t) & info,
               &info_count);
  MACH_CHECK_ERROR (kret);

  printf_filtered ("Inferior task 0x%lx has a suspend count of %d.\n",
                   (unsigned long) macosx_status->task, info.suspend_count);

  kret = task_threads (macosx_status->task, &thread_list, &nthreads);
  MACH_CHECK_ERROR (kret);

  printf_filtered ("The task has %lu threads:\n", (unsigned long) nthreads);
  for (i = 0; i < nthreads; i++)
    {
      print_thread_info (thread_list[i]);
    }

  kret =
    vm_deallocate (mach_task_self (), (vm_address_t) thread_list,
                   (nthreads * sizeof (int)));
  MACH_CHECK_ERROR (kret);
}

static thread_t
parse_thread (char *tidstr)
{
  ptid_t ptid;

  if (ptid_equal (inferior_ptid, null_ptid))
    {
      error ("The program being debugged is not being run.");
    }

  if (tidstr != NULL)
    {

      int num = atoi (tidstr);
      ptid = thread_id_to_pid (num);

      if (ptid_equal (ptid, minus_one_ptid))
        {
          error
            ("Thread ID %d not known.  Use the \"info threads\" command to\n"
             "see the IDs of currently known threads.", num);
        }
    }
  else
    {
      ptid = inferior_ptid;
    }

  if (!target_thread_alive (ptid))
    {
      error ("Thread ID %s does not exist.\n", target_pid_to_str (ptid));
    }

  return ptid_get_tid (ptid);
}

void
info_thread_command (char *tidstr, int from_tty)
{
  thread_t thread = parse_thread (tidstr);
  print_thread_info (thread);
}

static void
thread_suspend_command (char *tidstr, int from_tty)
{
  kern_return_t kret;
  thread_t thread;

  thread = parse_thread (tidstr);
  kret = thread_suspend (thread);

  MACH_CHECK_ERROR (kret);
}

static void
thread_resume_command (char *tidstr, int from_tty)
{
  kern_return_t kret;
  thread_t tid;
  struct thread_basic_info info;
  unsigned int info_count = THREAD_BASIC_INFO_COUNT;

  tid = parse_thread (tidstr);

  kret =
    thread_info (tid, THREAD_BASIC_INFO, (thread_info_t) & info, &info_count);
  MACH_CHECK_ERROR (kret);
  if (info.suspend_count == 0)
    error ("Attempt to resume a thread with suspend count of 0");

  kret = thread_resume (tid);

  MACH_CHECK_ERROR (kret);
}

static int
mark_dead_if_thread_is_gone (struct thread_info *tp, void *data)
{
  struct ptid_list *ptids = (struct ptid_list *) data;
  int i;
  int found_it = 0;

  for (i = 0; i < ptids->nthreads; i++)
    {
      if (ptid_equal(tp->ptid, ptids->ptids[i]))
	{
	  found_it = 1;
	  break;
	}      
    }
  if (!found_it)
    {
      tp->ptid = pid_to_ptid (-1);
    }
  return 0;
}

/* This compares the list of threads from task_threads, and gdb's
   current thread list, and removes the ones that are inactive.  It
   works much the same as "prune_threads" except that that function
   ends up calling task_threads for each thread in the thread list,
   which isn't very efficient.  */

void
macosx_prune_threads (thread_array_t thread_list, unsigned int nthreads)
{
  struct ptid_list ptid_list;
  kern_return_t kret;
  unsigned int i;
  int dealloc_thread_list = (thread_list == NULL);

  if (thread_list == NULL)
    {
      kret = task_threads (macosx_status->task, &thread_list, &nthreads);
      MACH_CHECK_ERROR (kret);
    }

  ptid_list.nthreads = nthreads;
  ptid_list.ptids = (ptid_t *) xmalloc (nthreads * sizeof (ptid_t));
 
  for (i = 0; i < nthreads; i++)
    {
      ptid_t ptid = ptid_build (macosx_status->pid, 0, thread_list[i]);
      ptid_list.ptids[i] = ptid;
    }
  if (dealloc_thread_list)
    {
      kret =
	vm_deallocate (mach_task_self (), (vm_address_t) thread_list,
		       (nthreads * sizeof (int)));
      MACH_CHECK_ERROR (kret);
    }

  iterate_over_threads (mark_dead_if_thread_is_gone, &ptid_list);
  prune_threads ();
}

void
_initialize_threads ()
{
  arm_macosx_tdep_inf_status.macosx_half_step_pc = (CORE_ADDR)-1;

  add_cmd ("suspend", class_run, thread_suspend_command,
           "Increment the suspend count of a thread.", &thread_cmd_list);

  add_cmd ("resume", class_run, thread_resume_command,
           "Decrement the suspend count of a thread.", &thread_cmd_list);

  add_info ("thread", info_thread_command, "Get information on thread.");

  add_info ("task", info_task_command, "Get information on task.");
}

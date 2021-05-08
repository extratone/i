/* Mac OS X support for GDB, the GNU debugger.
   Copyright 1997, 1998, 1999, 2000, 2001, 2002, 2004
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
#include "top.h"
#include "inferior.h"
#include "target.h"
#include "symfile.h"
#include "symtab.h"
#include "objfiles.h"
#include "gdb.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "gdbthread.h"
#include "regcache.h"
#include "environ.h"
#include "event-top.h"
#include "event-loop.h"
#include "inf-loop.h"
#include "gdb_stat.h"
#include "exceptions.h"
#include "checkpoint.h"
#include "value.h"
#include "gdb_regex.h"
#include "osabi.h"
#include "gdb_assert.h"

#include "bfd.h"

#include <sys/ptrace.h>
#include <sys/signal.h>
#include <machine/setjmp.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <mach/mach_error.h>
#include <spawn.h>

#include "macosx-nat-dyld.h"
#include "macosx-nat-inferior.h"
#include "macosx-nat-infthread.h"
#include "macosx-nat-inferior-debug.h"
#include "macosx-nat-mutils.h"
#include "macosx-nat-excthread.h"
#include "macosx-nat-sigthread.h"
#include "macosx-nat-threads.h"
#include "macosx-xdep.h"
/* classic-inferior-support */
#include "macosx-nat.h"
#include "macosx-nat-inferior-util.h"
#include "macosx-nat-dyld-process.h"

#if WITH_CFM
#include "macosx-nat-cfm.h"
#endif

#include <mach/mach_vm.h>

#ifndef EXC_SOFT_SIGNAL
#define EXC_SOFT_SIGNAL 0
#endif

/* The code values for single step vrs. breakpoint
   trap aren't defined in ppc header files.  There is a 
   def'n in the i386 exception.h, but it is a i386 specific
   define.  */

#if defined (TARGET_I386)
#define SINGLE_STEP EXC_I386_SGL
#elif defined (TARGET_POWERPC)
#define SINGLE_STEP 5
#elif defined (TARGET_ARM)
#define SINGLE_STEP 5
#include "arm-macosx-tdep.h"
#else
#error "unknown architecture"
#endif

#define _dyld_debug_make_runnable(a, b) DYLD_FAILURE
#define _dyld_debug_restore_runnable(a, b) DYLD_FAILURE
#define _dyld_debug_module_name(a, b, c, d, e) DYLD_FAILURE
#define _dyld_debug_set_error_func(a) DYLD_FAILURE
#define _dyld_debug_add_event_subscriber(a, b, c, d, e) DYLD_FAILURE

extern int standard_is_async_p (void);
extern int standard_can_async_p (void);

extern bfd *exec_bfd;

extern struct target_ops deprecated_child_ops;
extern struct target_ops macosx_child_ops;

extern struct target_ops exec_ops;
extern struct target_ops macosx_exec_ops;

macosx_inferior_status *macosx_status = NULL;
extern macosx_dyld_thread_status macosx_dyld_status;

int inferior_ptrace_flag = 1;
int inferior_ptrace_on_attach_flag = 1;
int inferior_bind_exception_port_flag = 1;

struct target_ops macosx_child_ops;
struct target_ops macosx_exec_ops;

/* From inftarg.c, needed by classic-inferior-support */
extern void init_child_ops (void);
extern void init_exec_ops (void);

extern int inferior_auto_start_dyld_flag;

int macosx_fake_resume = 0;

static int announce_attach = 1;

enum macosx_source_type
{
  NEXT_SOURCE_NONE = 0x0,
  NEXT_SOURCE_EXCEPTION = 0x1,
  NEXT_SOURCE_SIGNAL = 0x2,
  NEXT_SOURCE_CFM = 0x4,
  NEXT_SOURCE_ERROR = 0x8,
  NEXT_SOURCE_ALL = 0xf,
};

struct macosx_pending_event
{
  enum macosx_source_type type;
  unsigned char *buf;
  struct macosx_pending_event *next;
  struct macosx_pending_event *prev;
};

/* A list of processes already running at gdb-startup with the same
   name.  Used for the "-waitfor" command line option so we can ignore
   existing zombies/running copies of the process/etc and detect a newly
   launched version.  */

struct pid_list
{
  int count;
  pid_t *pids;
};

struct macosx_pending_event *pending_event_chain, *pending_event_tail;

static void (*async_client_callback) (enum inferior_event_type event_type,
                                      void *context);
static void *async_client_context;

static enum macosx_source_type macosx_fetch_event (struct
                                                   macosx_inferior_status
                                                   *inferior,
                                                   unsigned char *buf,
                                                   size_t len,
                                                   unsigned int flags,
                                                   int timeout);

static int macosx_service_event (enum macosx_source_type source,
                                 unsigned char *buf,
                                 struct target_waitstatus *status);

static void macosx_handle_signal (macosx_signal_thread_message *msg,
                                  struct target_waitstatus *status);

static void macosx_handle_exception (macosx_exception_thread_message *msg,
                                     struct target_waitstatus *status);

static int macosx_process_events (struct macosx_inferior_status *ns,
                                  struct target_waitstatus *status,
                                  int timeout, int service_first_event);

static struct macosx_pending_event * macosx_add_to_pending_events (enum macosx_source_type,
                                          unsigned char *buf);

static int macosx_post_pending_event (void);
static void macosx_pending_event_handler (void *data);
static ptid_t macosx_process_pending_event (struct macosx_inferior_status *ns,
                                            struct target_waitstatus *status,
                                            gdb_client_data client_data);

static void macosx_clear_pending_events ();

static void macosx_child_stop (void);

static void macosx_child_resume (ptid_t ptid, int step,
                                 enum target_signal signal);

static ptid_t macosx_child_wait (ptid_t ptid,
                                 struct target_waitstatus *status,
                                 gdb_client_data client_data);

static void macosx_mourn_inferior ();

static int macosx_lookup_task (char *args, task_t * ptask, int *ppid);

static void macosx_child_attach (char *args, int from_tty);

static void macosx_child_detach (char *args, int from_tty);

static int macosx_kill_inferior (void *);
static void macosx_kill_inferior_safe ();

static void macosx_ptrace_me ();

static void macosx_ptrace_him (int pid);

static void macosx_child_create_inferior (char *exec_file, char *allargs,
                                          char **env, int from_tty);

static void macosx_child_files_info (struct target_ops *ops);

static char *macosx_pid_to_str (ptid_t tpid);

static int macosx_child_thread_alive (ptid_t tpid);

static struct pid_list *find_existing_processes_by_name (const char *procname);
static int pid_present_on_pidlist (pid_t pid, struct pid_list *proclist);

static void
macosx_handle_signal (macosx_signal_thread_message *msg,
                      struct target_waitstatus *status)
{
  kern_return_t kret;

  CHECK_FATAL (macosx_status != NULL);

  CHECK_FATAL (macosx_status->attached_in_ptrace);
  CHECK_FATAL (!macosx_status->stopped_in_ptrace);
  /* CHECK_FATAL (! macosx_status->stopped_in_softexc); */

  if (inferior_debug_flag)
    {
      macosx_signal_thread_debug_status (stderr, msg->status);
    }

  if (msg->pid != macosx_status->pid)
    {
      warning ("macosx_handle_signal: signal message was for pid %d, "
               "not for inferior process (pid %d)\n",
               msg->pid, macosx_status->pid);
      return;
    }

  if (WIFEXITED (msg->status))
    {
      status->kind = TARGET_WAITKIND_EXITED;
      status->value.integer = WEXITSTATUS (msg->status);
      return;
    }

  if (!WIFSTOPPED (msg->status))
    {
      status->kind = TARGET_WAITKIND_SIGNALLED;
      status->value.sig = target_signal_from_host (WTERMSIG (msg->status));
      return;
    }

  macosx_status->stopped_in_ptrace = 1;

  kret = macosx_inferior_suspend_mach (macosx_status);
  MACH_CHECK_ERROR (kret);

  prepare_threads_after_stop (macosx_status);

  status->kind = TARGET_WAITKIND_STOPPED;
  status->value.sig = target_signal_from_host (WSTOPSIG (msg->status));
}

static void
macosx_handle_exception (macosx_exception_thread_message *msg,
                         struct target_waitstatus *status)
{
  kern_return_t kret;

  CHECK_FATAL (status != NULL);
  CHECK_FATAL (macosx_status != NULL);

  if (inferior_ptrace_flag)
    {
      CHECK_FATAL (macosx_status->attached_in_ptrace);
      CHECK_FATAL (!macosx_status->stopped_in_ptrace);
    }

  inferior_debug (2, "macosx_handle_exception: received exception message\n");

  if (msg->task_port != macosx_status->task)
    {
      /* If the exception was for a child other than the process being
         debugged, reset the exception ports for the child back to
         default, and resume.  Ideally the exception ports would never
         have been set to the one as modified by GDB in the first
         place, but this should work in most cases. */

      inferior_debug (2,
                      "macosx_handle_exception: exception was for child of process being debugged\n");

      kret =
        macosx_restore_exception_ports (msg->task_port,
                                        &macosx_status->exception_status.
                                        saved_exceptions);
      MACH_WARN_ERROR (kret);

      macosx_inferior_resume_mach (macosx_status, 0);

      status->kind = TARGET_WAITKIND_SPURIOUS;
      return;
    }

  macosx_status->last_thread = msg->thread_port;

  kret = macosx_inferior_suspend_mach (macosx_status);
  MACH_CHECK_ERROR (kret);

  prepare_threads_after_stop (macosx_status);

  status->kind = TARGET_WAITKIND_STOPPED;

  switch (msg->exception_type)
    {
    case EXC_BAD_ACCESS:
      status->value.sig = TARGET_EXC_BAD_ACCESS;
      /* Preserve the exception data so we can print it later.  */
      status->code = msg->exception_data[0];
      /* When gcc casts a signed int to an unsigned long long, it first
         casts it to a long long (sign extending it in the process) then
         it uselessly casts it to unsigned.  So we first have to cast it
         to an unsigned of its size.  I can't figure out a way to do that
         automatically, but i know the exception_data is either going to
         be an int, or a long long, so I'll just handle those cases.  */

      if (sizeof (msg->exception_data[1]) < sizeof (CORE_ADDR))
        status->address = (CORE_ADDR) ((unsigned int) msg->exception_data[1]);
      else
        status->address = (CORE_ADDR) msg->exception_data[1];
      break;
    case EXC_BAD_INSTRUCTION:
      status->value.sig = TARGET_EXC_BAD_INSTRUCTION;
      break;
    case EXC_ARITHMETIC:
      status->value.sig = TARGET_EXC_ARITHMETIC;
      break;
    case EXC_EMULATION:
      status->value.sig = TARGET_EXC_EMULATION;
      break;
    case EXC_SOFTWARE:
      {
        switch (msg->exception_data[0])
          {
          case EXC_SOFT_SIGNAL:
            status->value.sig = 
	      target_signal_from_host ((unsigned int) msg->exception_data[1]);
            macosx_status->stopped_in_softexc = 1;
            break;
          default:
            status->value.sig = TARGET_EXC_SOFTWARE;
            break;
          }
      }
      break;
    case EXC_BREAKPOINT:
      /* Many internal GDB routines expect breakpoints to be reported
         as TARGET_SIGNAL_TRAP, and will report TARGET_EXC_BREAKPOINT
         as a spurious signal. */
      status->value.sig = TARGET_SIGNAL_TRAP;
      break;
    default:
      status->value.sig = TARGET_SIGNAL_UNKNOWN;
      break;
    }
}

static void
macosx_add_to_port_set (struct macosx_inferior_status *inferior,
                        fd_set * fds, int flags)
{
  FD_ZERO (fds);

  if ((flags & NEXT_SOURCE_EXCEPTION)
      && inferior->exception_status.receive_from_fd > 0)
    {
      FD_SET (inferior->exception_status.receive_from_fd, fds);
    }

  if ((flags & NEXT_SOURCE_ERROR)
      && inferior->exception_status.error_receive_fd > 0)
    {
      FD_SET (inferior->exception_status.error_receive_fd, fds);
    }

  if ((flags & NEXT_SOURCE_SIGNAL)
      && inferior->signal_status.receive_fd > 0)
    {
      FD_SET (inferior->signal_status.receive_fd, fds);
    }
}

/* TIMEOUT is either -1, 0, or greater than 0.
   For 0, check if there is anything to read, but don't block.
   For -1, block until there is something to read.
   For >0, block at least the specified number of microseconds, or until there
   is something to read.
   The kernel doesn't give better than ~1HZ (0.01 sec) resolution, so
   don't use this as a high accuracy timer. */

static enum macosx_source_type
macosx_fetch_event (struct macosx_inferior_status *inferior,
                    unsigned char *buf, size_t len,
                    unsigned int flags, int timeout)
{
  fd_set fds;
  int fd, ret;
  struct timeval tv;

  CHECK_FATAL (len >= sizeof (macosx_exception_thread_message));
  CHECK_FATAL (len >= sizeof (macosx_signal_thread_message));

  tv.tv_sec = 0;
  tv.tv_usec = timeout;

  macosx_add_to_port_set (inferior, &fds, flags);

  for (;;)
    {
      if (timeout == -1)
        {
          ret = select (FD_SETSIZE, &fds, NULL, NULL, NULL);
        }
      else
        {
          ret = select (FD_SETSIZE, &fds, NULL, NULL, &tv);
        }
      if ((ret < 0) && (errno == EINTR))
        {
          continue;
        }
      if (ret < 0)
        {
          internal_error (__FILE__, __LINE__, "unable to select: %s",
                          strerror (errno));
        }
      if (ret == 0)
        {
          return NEXT_SOURCE_NONE;
        }
      break;
    }

  fd = inferior->exception_status.error_receive_fd;
  if (fd > 0 && FD_ISSET (fd, &fds))
    {
      read (fd, buf, 1);
      return NEXT_SOURCE_ERROR;
    }

  fd = inferior->exception_status.receive_from_fd;
  if (fd > 0 && FD_ISSET (fd, &fds))
    {
      read (fd, buf, sizeof (macosx_exception_thread_message));
      return NEXT_SOURCE_EXCEPTION;
    }

  fd = inferior->signal_status.receive_fd;
  if (fd > 0 && FD_ISSET (fd, &fds))
    {
      read (fd, buf, sizeof (macosx_signal_thread_message));
      return NEXT_SOURCE_SIGNAL;
    }

  return NEXT_SOURCE_NONE;
}

/* This takes the data from an event and puts it on the tail of the
   "pending event" chain. */

static struct macosx_pending_event *
macosx_add_to_pending_events (enum macosx_source_type type,
                              unsigned char *buf)
{
  struct macosx_pending_event *new_event;

  new_event = (struct macosx_pending_event *)
    xmalloc (sizeof (struct macosx_pending_event));

  new_event->type = type;

  if (type == NEXT_SOURCE_SIGNAL)
    {
      macosx_signal_thread_message *mssg;
      mssg = (macosx_signal_thread_message *)
        xmalloc (sizeof (macosx_signal_thread_message));
      memcpy (mssg, buf, sizeof (macosx_signal_thread_message));
      inferior_debug (1,
                      "macosx_add_to_pending_events: adding a signal event "
		      "to the pending events.\n");
      new_event->buf = (void *) mssg;
    }
  else if (type == NEXT_SOURCE_EXCEPTION)
    {
      macosx_exception_thread_message *mssg;
      mssg = (macosx_exception_thread_message *)
        xmalloc (sizeof (macosx_exception_thread_message));
      memcpy (mssg, buf, sizeof (macosx_exception_thread_message));
      inferior_debug (1,
                      "macosx_add_to_pending_events: adding an exception event "
		      "to the pending events.\n");
      new_event->buf = (void *) mssg;
    }

  new_event->next = NULL;

  if (pending_event_chain == NULL)
    {
      pending_event_chain = new_event;
      pending_event_tail = new_event;
      new_event->prev = NULL;
    }
  else
    {
      new_event->prev = pending_event_tail;
      pending_event_tail->next = new_event;
      pending_event_tail = new_event;
    }
  return new_event;
}

static void
macosx_free_pending_event (struct macosx_pending_event *event_ptr)
{
  xfree (event_ptr->buf);
  xfree (event_ptr);
}

static int 
macosx_count_pending_events ()
{
  int counter = 0;
  struct macosx_pending_event *event_ptr;

  for (event_ptr = pending_event_chain; event_ptr != NULL; event_ptr = event_ptr->next)
    counter++;

  return counter;
}
      
static void
macosx_remove_pending_event (struct macosx_pending_event *event_ptr, int delete)
{
  if (event_ptr == pending_event_chain)
    pending_event_chain = event_ptr->next;
  if (event_ptr == pending_event_tail)
    pending_event_tail = event_ptr->prev;

  if (event_ptr->prev != NULL)
    event_ptr->prev->next = event_ptr->next;
  if (event_ptr->next != NULL)
    event_ptr->next->prev = event_ptr->prev;
  
  if (delete)
    macosx_free_pending_event (event_ptr);
}

static void
macosx_clear_pending_events ()
{
  struct macosx_pending_event *event_ptr = pending_event_chain;

  while (event_ptr != NULL)
    {
      pending_event_chain = event_ptr->next;
      macosx_free_pending_event (event_ptr);
      event_ptr = pending_event_chain;
    }
}

/* This extracts the top of the pending event chain and posts a gdb event
   with its content to the gdb event queue.  Returns 0 if there were no
   pending events to be posted, 1 otherwise. */

static int
macosx_post_pending_event (void)
{
  struct macosx_pending_event *event;

  if (pending_event_chain == NULL)
    {
      inferior_debug (1, "macosx_post_pending_event: no events to post\n");
      return 0;
    }
  else
    {
      event = pending_event_chain;
      macosx_remove_pending_event (event, 0);
      inferior_debug (1,
                      "macosx_post_pending_event: consuming event off queue\n");
      gdb_queue_event (macosx_pending_event_handler, (void *) event, HEAD);

      return 1;
    }
}

static void
macosx_pending_event_handler (void *data)
{
  inferior_debug (4, "Called in macosx_pending_event_handler\n");
  async_client_callback (INF_REG_EVENT, data);
}

static int
macosx_service_event (enum macosx_source_type source,
                      unsigned char *buf, struct target_waitstatus *status)
{
  /* We are using code not equal to -1 to signal we have extra goodies that
     the upper level code might want to print.  */
  status->code = -1;

  if (source == NEXT_SOURCE_EXCEPTION)
    {
      macosx_exception_thread_message *msg =
        (macosx_exception_thread_message *) buf;
      inferior_debug (1,
                      "macosx_service_events: got exception message 0x%lx\n",
                      msg->exception_type);
      CHECK_FATAL (inferior_bind_exception_port_flag);
      macosx_handle_exception ((macosx_exception_thread_message *) buf,
                               status);
      if (status->kind != TARGET_WAITKIND_SPURIOUS)
          return 1;
    }
  else if (source == NEXT_SOURCE_SIGNAL)
    {
      inferior_debug (1, "macosx_service_events: got signal message\n");
      macosx_handle_signal ((macosx_signal_thread_message *) buf, status);
      CHECK_FATAL (status->kind != TARGET_WAITKIND_SPURIOUS);
      return 1;
    }
  else if (source == NEXT_SOURCE_ERROR)
    {
      inferior_debug (1, "macosx_service_events: got an error\n");
      target_mourn_inferior ();
      return 0;
    }
  else
    {
      error ("got message from unknown source: 0x%08x\n", source);
      return 0;
    }
  return 1;
}

/* We treat single step events, and breakpoint events
   specially - though only if we get more than one event
   at a time.  This enum and the get_event_type function
   are helpers for the code that does this.  */

enum bp_ss_or_other {
  bp_event = 0,
  ss_event,
  sig_event,
  other_event
};

static enum bp_ss_or_other 
get_event_type (struct macosx_exception_thread_message *msg)
{
  if (msg->exception_type == EXC_BREAKPOINT)
    {
      if (msg->data_count == 2
	  && msg->exception_data[0] == SINGLE_STEP)
	return ss_event;
      else
	return bp_event;
    }
  else if (msg->exception_type == EXC_SOFTWARE
      && (msg->data_count == 2)
      && (msg->exception_data[0] == EXC_SOFT_SIGNAL))
    return sig_event;
  else
    return other_event;
}

/* This function services the first of the non-breakpoint type events.  
   It pushes all the other "other" type events back on the pending events chain. 
   It deletes all the others.  
   FIXME: This is a bit of a hack, but I don't know how to REALLY push the
   signal events back onto the target.  So I have to fake it by leaving them
   around on the pending event queue, and that will mean the next time you
   try to run, you'll hit the next event without actually running...  
   Fortunately, it looks like this is academic, because the system
   seems to serialize all the other events for the debugger.  */

int
macosx_service_one_other_event (struct target_waitstatus *status)
{
  struct macosx_pending_event *event;
  int count = 0;

  event = pending_event_chain;

  while (event != NULL) 
    {
      struct macosx_pending_event *next_event = event->next;
      macosx_exception_thread_message *msg = 
	(macosx_exception_thread_message *) event->buf;
      if (event->type != NEXT_SOURCE_EXCEPTION 
	  || get_event_type (msg) == other_event
	  || get_event_type (msg) == sig_event)
	{
	  count++;
	  if (count == 1)
	    {
	      macosx_service_event (event->type, event->buf, status);
	      macosx_remove_pending_event (event, 1);
	    }
	}
      else
	{
	  macosx_remove_pending_event (event, 1);
	}
      event = next_event;
    } 
  return count;
}

/* Backs up the pc to before the breakpoint (if necessary) for all
   the pending breakpoint events - except for breakpoint event
   IGNORE, if IGNORE is not < 0.  Returns the event ignored, or
   NULL if there is no such event.  */

struct macosx_pending_event *
macosx_backup_before_break (int ignore)
{
  int count = 0;
  struct macosx_pending_event *ret_event = NULL, *event;

  for (event = pending_event_chain; event != NULL ; event = event->next) {
    if (event->type == NEXT_SOURCE_EXCEPTION)
      {
	macosx_exception_thread_message *msg = 
	  (macosx_exception_thread_message *) event->buf;
	ptid_t ptid = ptid_build (macosx_status->pid, 0, msg->thread_port);

	if (get_event_type(msg) == bp_event
	    && breakpoint_here_p (read_pc_pid (ptid) -
				     DECR_PC_AFTER_BREAK))
	  {
	    if (count == ignore)
	      {
		ret_event = event;
	      }
	    else
	      {
		/* Back up the PC if necessary.  */
		if (DECR_PC_AFTER_BREAK)
		  {
		    CORE_ADDR new_pc = read_pc_pid (ptid) - DECR_PC_AFTER_BREAK; 
		    write_pc_pid (new_pc, ptid);
		    inferior_debug (6, "backup_before_break: setting PID for thread: 0x%lx to %s\n", 
				    msg->thread_port, paddr_nz (new_pc));
		  }
	      }
	  }
      }
    count++;
  }
  
  if (DECR_PC_AFTER_BREAK)
    {
      if (ret_event)
	inferior_debug (6, "Backing up all breakpoint hits except thread 0x%lx\n",
			((macosx_exception_thread_message *) ret_event->buf)->thread_port);
      else
	inferior_debug (6, "Backing up all breakpoint hits\n");
    }
  return ret_event;
}

/* This drains the event sources.  The first event found is directly
   handled.  The rest are "pushed back" to the target as best we can.

   The priority is:

   1) If there is a single step event pending, we report that.
   2) Otherwise, if there are any non-breakpoint events, we report
   them.
   3) Otherwise we pick one breakpoint, and report that.
       a) If the scheduler is locked, and one of the breakpoints is
          on the locked thread, then we report that.
       b) Otherwise we pick a breakpoint at random from the list.

   All the other events are pushed back if we know how to do this.

   Caveats:
   1) At present, I don't know how to "push back" a signal.  So if there
   is more than one SOFTEXC event we just send them all.  Not sure what
   gdb will do with this.  I haven't been able get the system to send
   more than one at a time.
   2) Ditto for other traps.  Dunno what I would do with two EXC_BAD_ACCESS
   messages, for instance.

   Returns: The number of events found. */

static int
macosx_process_events (struct macosx_inferior_status *inferior,
                       struct target_waitstatus *status,
                       int timeout, int service_first_event)
{
  enum macosx_source_type source;
  unsigned char buf[1024];
  int event_count = 0;
  int breakpoint_count = 0;
  int other_count = 0;
  int scheduler_bp = -1;
  enum bp_ss_or_other event_type;
  int hand_call_function_bp = -1;
  struct macosx_pending_event *event = NULL, *single_step = NULL,
    *hand_call_function_thread_event = NULL,
    *signal_event = NULL;

  int first_time_through = 1;

  CHECK_FATAL (status->kind == TARGET_WAITKIND_SPURIOUS);

  event_count = macosx_count_pending_events ();
  if (event_count != 0)
    return event_count;

  /* Fetch events from the exc & signal threads.  First time through,
     we use TIMEOUT and wait, then we poll to drain the rest of the 
     events the exc & signal threads got.  */
  for (;;)
    {
      source = macosx_fetch_event (inferior, buf, sizeof (buf),
                                   NEXT_SOURCE_ALL, timeout);
      /* After we wake up from select, get the write lock.  This
         insures that we don't start reading the exception data when
         the exception thread is only part way through writing it.  Be
         sure to release this before exiting the function.  Since each
         event is written in a single shot, we don't have to worry
         about getting a partial first event, it's only the subsequent
         events we need to sync with.  */
      if (first_time_through)
        {
          first_time_through = 0;
          macosx_exception_get_write_lock (&inferior->exception_status);
        }

      if (source == NEXT_SOURCE_NONE)
        {
          break;
        }

      event_count++;
      if (source == NEXT_SOURCE_ERROR)
	{
	  /* We couldn't read from the inferior exception port.  Dunno why,
	     but we aren't going to get much further.  So tell ourselves that 
	     the target exited, cons up some bogus status, and get out
	     of here.  */
	  inferior_debug (2, "Got NEXT_SOURCE_ERROR from macosx_fetch_event\n");
	  status->kind = TARGET_WAITKIND_EXITED;
	  status->value.integer = 0;
	  return 0;
	}
      else if (source == NEXT_SOURCE_SIGNAL)
	{
	  event = macosx_add_to_pending_events (source, buf);
	  signal_event = event;
	  other_count++;
	}
      else
        {

	  struct ptid this_ptid;
	  this_ptid = ptid_build (macosx_status->pid, 0, 
				  ((macosx_exception_thread_message *) buf)->thread_port);

	  event = macosx_add_to_pending_events (source, buf);
	  event_type = get_event_type ((macosx_exception_thread_message *) buf);

	  /* If this event is for the thread we were calling a function
	     on, prefer that event.  Also record the bp number if it is a
	     breakpoint event, so we can rewind the right breakpoints.  */

	  extern ptid_t get_hand_call_ptid (void);
	  if (ptid_equal (this_ptid, get_hand_call_ptid ()))
	    {
	      hand_call_function_thread_event = event;
	      if (event_type == bp_event)
		hand_call_function_bp = breakpoint_count;
	    }

	  if (event_type == ss_event)
	    single_step = event;
	  else if (event_type == bp_event)
	    {
	      if (scheduler_lock_on_p ()) 
		{
		  if (ptid_equal (get_scheduler_lock_ptid (), this_ptid))
		    scheduler_bp = breakpoint_count;
		}
	      breakpoint_count += 1;
	    }
	  else if (event_type == sig_event)
	    {
	      signal_event = event;
	      other_count += 1;
	    }
	  else
	    other_count += 1;
        }
      timeout = 0;
    }

  macosx_exception_release_write_lock (&inferior->exception_status);

  if (event_count == 0)
    return 0;

  inferior_debug (2,
          "macosx_process_events: returning with (status->kind == %d)\n",
                  status->kind);

  /* Okay, now that we've gotten the events what should we do?  
     If we only got one, let's service it and exit. */
  if (event_count == 1)
    {
      int retval;

      if (macosx_service_event (event->type, 
				event->buf, status) == 0)
	retval = 0;
      else
	retval = 1;

      macosx_clear_pending_events ();
      return retval;
    }
  else
    {
      /* If we have more than one, look through them to figure 
	 out what to do.  */
      
      /* We have more than one exception event, but the upper layers of gdb can 
	 only deal with one at a time.  We currently use the following
         Heuristics pick which one we are going to report:
	 
	 * First priority is single-step events.  This is so we can finish
	   stepping over a breakpoint without interruption.
	 * Next, if there is a signal event, we handle it.  That's because
	   I don't know how to push back a signal event.
	 * Next is if we are calling a function by hand and that stops, we
           should handle that immediately.
	 * If there is any other non-breakpoint exception in the set, we
	   handle that next.
	 * Next if we hit a breakpoint on the thread we were trying to run
	   the scheduler on, we prefer that.  This is an issue because we
           might have pushed back many breakpoint hits, then tried to 
	   run something on one of the threads.  We don't want to service
	   the left-over events from the other threads till we are done with
	   what we are doing on this thread.
	 * Finally, we randomly pick one of the breakpoint events, and
	   handle that.
      */

      if (single_step != NULL)
	{
	  inferior_debug (2, "macosx_process_events: found a single step "
			  "event, and %d breakpoint events\n", breakpoint_count);
	  macosx_backup_before_break (-1);
	  event_count = macosx_service_event (single_step->type, single_step->buf,
				status);
	}
      else if (signal_event != NULL)
	{
	  inferior_debug (2, "macosx_process_events: forwarding signal event");
	  macosx_backup_before_break (-1);
	  event_count = macosx_service_event (signal_event->type,
					      signal_event->buf,
					      status);
	}
      else if (hand_call_function_thread_event != NULL)
	{
	  inferior_debug (2, "macosx_process_events: found an event for "
			  "the hand-call function thread "
			  ", and %d breakpoint events\n", breakpoint_count);
	  macosx_backup_before_break (hand_call_function_bp);
	  event_count = macosx_service_event (hand_call_function_thread_event->type, 
				hand_call_function_thread_event->buf,
				status);
	}
      else if (breakpoint_count != event_count)
	{
	  /* This is the case where we have maybe some breakpoints, and one or more
	     other exceptions.  */
	  inferior_debug (2, "macosx_process_events: found %d breakpoint events out of "
			  "%d total events\n", breakpoint_count, event_count);
	  macosx_backup_before_break (-1);
	  event_count = macosx_service_one_other_event (status);
	}
      else
	{
	  /* This is the case where we just have a bunch of breakpoint hits.  */
	  struct macosx_pending_event *chosen_break;
	  int random_selector;
	  if (scheduler_bp != -1)
	    {
	      /* If we are trying to run a single thread, let's favor
		 that thread over another that might have just gotten
		 created and hit a breakpoint.  */
	      inferior_debug (6, "macosx_process_events: Choosing scheduler "
			      "breakpoint thread index: %d\n", scheduler_bp);
	      random_selector = scheduler_bp;
	    }
	  else
	    {
	      /* Otherwise pick one of the threads randomly, and report
		 the breakpoint hit for that one.  */
	      random_selector = (int)
		((breakpoint_count * (double) rand ()) / (RAND_MAX + 1.0));
	      inferior_debug (6, "macosx_process_events: Choosing random "
			      "selector index %d\n", random_selector);
	    }
	  chosen_break = macosx_backup_before_break (random_selector);
	  event_count = macosx_service_event (chosen_break->type, chosen_break->buf,
				    status);
	}
      macosx_clear_pending_events ();
    }
  return event_count;
}

void
macosx_check_new_threads (thread_array_t thread_list, unsigned int nthreads)
{
  kern_return_t kret;
  unsigned int i;
  int dealloc_thread_list = (thread_list == NULL);

  if (thread_list == NULL)
    {
      kret = task_threads (macosx_status->task, &thread_list, &nthreads);
      MACH_CHECK_ERROR (kret);
    }

  for (i = 0; i < nthreads; i++)
    {
      ptid_t ptid = ptid_build (macosx_status->pid, 0, thread_list[i]);

      if (!in_thread_list (ptid))
        {
          struct thread_info *tp;
          tp = add_thread (ptid);
          if (create_private_thread_info (tp))
            tp->private->app_thread_port =
              get_application_thread_port (thread_list[i]);
        }
    }

  if (dealloc_thread_list)
    {
      kret =
	vm_deallocate (mach_task_self (), (vm_address_t) thread_list,
		       (nthreads * sizeof (int)));
      MACH_CHECK_ERROR (kret);
    }
}

/* This differs from child_stop in that we don't send
   "-inferior_process_group" to kill when we are attached to the
   process, we just send inferior_process_group.  Even this is kind
   of a lie, since inferior_process_group really isn't, it is just
   the pid of the child process, look at "terminal_init_inferior"
   in inflow.c, which sets inferior_process_group.  This just passes
   in the pid of the child process!  I think all the job control
   stuff in inflow.c looks bogus to me, we ought to use MacOS X
   specific versions everywhere we can, and avoid that mess...
*/

static void
macosx_child_stop (void)
{
  pid_t pid = PIDGET (inferior_ptid);
  int ret;

  ret = kill (pid, SIGINT);
}

static void
macosx_child_resume (ptid_t ptid, int step, enum target_signal signal)
{
  int nsignal = target_signal_to_host (signal);
  struct target_waitstatus status;
  int stop_others = 1;
  int pid;
  thread_t thread;

  status.code = -1;

  if (ptid_equal (ptid, minus_one_ptid))
    {
      ptid = inferior_ptid;
      stop_others = 0;
    }

  pid = ptid_get_pid (ptid);
  thread = ptid_get_tid (ptid);

  CHECK_FATAL (macosx_status != NULL);

  macosx_inferior_check_stopped (macosx_status);
  if (!macosx_inferior_valid (macosx_status))
    return;

  /* Check for pending events.  If we find any, then we won't really resume,
     but rather we will extract the first event from the pending events
     queue, and post it to the gdb event queue, and then "pretend" that we
     have in fact resumed. */

  inferior_debug (2, "macosx_child_resume: checking for pending events\n");
  status.kind = TARGET_WAITKIND_SPURIOUS;
  macosx_process_events (macosx_status, &status, 0, 0);

  inferior_debug (1, "macosx_child_resume: %s process with signal %d\n",
                  step ? "stepping" : "continuing", nsignal);

  if (macosx_post_pending_event ())
    {
      /* QUESTION: Do I need to lie about target_executing here? */
      macosx_fake_resume = 1;
      if (target_is_async_p ())
        target_executing = 1;
      return;
    }

  if (macosx_status->stopped_in_ptrace || macosx_status->stopped_in_softexc)
    {
      macosx_inferior_resume_ptrace (macosx_status, thread, nsignal,
                                     PTRACE_CONT);
    }

  if (!macosx_inferior_valid (macosx_status))
    return;

  if (step)
    prepare_threads_before_run (macosx_status, step, thread, 1);
  else
    prepare_threads_before_run (macosx_status, 0, thread, stop_others);

  macosx_inferior_resume_mach (macosx_status, -1);

  if (target_can_async_p ())
    target_async (inferior_event_handler, 0);

  if (target_is_async_p ())
    target_executing = 1;
}

static ptid_t
macosx_process_pending_event (struct macosx_inferior_status *ns,
                              struct target_waitstatus *status,
                              gdb_client_data client_data)
{
  struct macosx_pending_event *event
    = (struct macosx_pending_event *) client_data;

  inferior_debug (1, "Processing pending event type: %d\n", event->type);
  macosx_service_event (event->type, (unsigned char *) event->buf, status);

  return ptid_build (macosx_status->pid, 0, macosx_status->last_thread);
}

/*
 * This fetches & processes events from the exception & signal file
 * descriptors.  If client_data is not NULL, then the client data must
 * hold a macosx_pending_event structure, and in that case, this just
 * processes that event.
 */

ptid_t
macosx_wait (struct macosx_inferior_status *ns,
             struct target_waitstatus * status, gdb_client_data client_data)
{
  CHECK_FATAL (ns != NULL);

  if (client_data != NULL)
    return macosx_process_pending_event (ns, status, client_data);

  set_sigint_trap ();
  set_sigio_trap ();

  status->kind = TARGET_WAITKIND_SPURIOUS;
  while (status->kind == TARGET_WAITKIND_SPURIOUS)
    macosx_process_events (ns, status, -1, 1);

  clear_sigio_trap ();
  clear_sigint_trap ();

  if ((status->kind == TARGET_WAITKIND_EXITED)
      || (status->kind == TARGET_WAITKIND_SIGNALLED))
    return null_ptid;

  macosx_check_new_threads (NULL, 0);

  if (!macosx_thread_valid (macosx_status->task, macosx_status->last_thread))
    {
      if (macosx_task_valid (macosx_status->task))
        {
          warning ("Currently selected thread no longer alive; "
                   "selecting initial thread");
          macosx_status->last_thread =
            macosx_primary_thread_of_task (macosx_status->task);
        }
    }

  return ptid_build (macosx_status->pid, 0, macosx_status->last_thread);
}

static ptid_t
macosx_child_wait (ptid_t pid, struct target_waitstatus *status,
                   gdb_client_data client_data)
{
  CHECK_FATAL (macosx_status != NULL);
  return macosx_wait (macosx_status, status, client_data);
}

static void
macosx_mourn_inferior ()
{
  unpush_target (&macosx_child_ops);
  deprecated_child_ops.to_mourn_inferior ();
  macosx_inferior_destroy (macosx_status);

  inferior_ptid = null_ptid;
  attach_flag = 0;

  macosx_dyld_mourn_inferior ();

  macosx_clear_pending_events ();
  remove_thread_event_breakpoints ();
}

void
macosx_fetch_task_info (struct kinfo_proc **info, size_t * count)
{
  struct kinfo_proc *proc;
  int control[4] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };
  size_t length;

  CHECK_FATAL (info != NULL);
  CHECK_FATAL (count != NULL);

  sysctl (control, 3, NULL, &length, NULL, 0);
  proc = (struct kinfo_proc *) xmalloc (length);
  sysctl (control, 3, proc, &length, NULL, 0);

  *count = length / sizeof (struct kinfo_proc);
  *info = proc;
}

char **
macosx_process_completer_quoted (char *text, char *word, int quote, 
                                 struct pid_list *ignorepids)
{
  struct kinfo_proc *proc = NULL;
  size_t count, i, found = 0;

  char **procnames = NULL;
  char **ret = NULL;
  int quoted = 0;

  if (text[0] == '"')
    {
      quoted = 1;
    }

  macosx_fetch_task_info (&proc, &count);

  procnames = (char **) xmalloc ((count + 1) * sizeof (char *));

  for (i = 0; i < count; i++)
    {
      /* classic-inferior-support */
      if (!can_attach (proc[i].kp_proc.p_pid))
        continue;
      if (pid_present_on_pidlist (proc[i].kp_proc.p_pid, ignorepids))
        continue;
      /* Skip zombie processes */
      if (proc[i].kp_proc.p_stat == SZOMB || proc[i].kp_proc.p_stat == 0)
        continue;
      char *temp =
        (char *) xmalloc (strlen (proc[i].kp_proc.p_comm) + 1 + 16);
      sprintf (temp, "%s.%d", proc[i].kp_proc.p_comm, proc[i].kp_proc.p_pid);
      procnames[found] = (char *) xmalloc (strlen (temp) * 2 + 2 + 1);
      if (quote)
        {
          if (quoted)
            {
              sprintf (procnames[found], "\"%s\"", temp);
            }
          else
            {
              char *s = temp;
              char *t = procnames[found];
              while (*s != '\0')
                {
                  if (strchr ("\" ", *s) != NULL)
                    {
                      *t++ = '\\';
                      *t++ = *s++;
                    }
                  else
                    {
                      *t++ = *s++;
                    }
                }
              *t++ = '\0';
            }
        }
      else
        {
          sprintf (procnames[found], "%s", temp);
        }
      found++;
    }
  procnames[found] = NULL;

  ret = complete_on_enum ((const char **) procnames, text, word);

  xfree (proc);
  return ret;
}

char **
macosx_process_completer (char *text, char *word)
{
  return macosx_process_completer_quoted (text, word, 1, NULL);
}

static void
macosx_lookup_task_local (char *pid_str, int pid, task_t * ptask, int *ppid,
                          struct pid_list *ignorepids)
{
  CHECK_FATAL (ptask != NULL);
  CHECK_FATAL (ppid != NULL);

  if (pid_str == NULL)
    {

      task_t itask;
      kern_return_t kret;

      kret = task_for_pid (mach_task_self (), pid, &itask);
      if (kret != KERN_SUCCESS)
        {
          error ("Unable to access task for process-id %d: %s.", pid,
                 MACH_ERROR_STRING (kret));
        }
      *ptask = itask;
      *ppid = pid;

    }
  else
    {
      struct cleanup *cleanups = NULL;
      char **ret = macosx_process_completer_quoted (pid_str, pid_str, 0, 
                                                    ignorepids);
      char *tmp = NULL;
      char *tmp2 = NULL;
      unsigned long lpid = 0;

      task_t itask;
      kern_return_t kret;

      cleanups = make_cleanup (free, ret);

      if ((ret == NULL) || (ret[0] == NULL))
        {
          error ("Unable to locate process named \"%s\".", pid_str);
        }
      if (ret[1] != NULL)
        {
          error ("Multiple processes exist with the name \"%s\".", pid_str);
        }

      tmp = strrchr (ret[0], '.');
      if (tmp == NULL)
        {
          error
            ("Unable to parse process-specifier \"%s\" (does not contain process-id)",
             ret[0]);
        }
      tmp++;
      lpid = strtoul (tmp, &tmp2, 10);
      if (!isdigit (*tmp) || (*tmp2 != '\0'))
        {
          error
            ("Unable to parse process-specifier \"%s\" (does not contain process-id)",
             ret[0]);
        }
      if ((lpid > INT_MAX) || ((lpid == ULONG_MAX) && (errno == ERANGE)))
        {
          error ("Unable to parse process-id \"%s\" (integer overflow).",
                 ret[0]);
        }
      pid = lpid;

      kret = task_for_pid (mach_task_self (), pid, &itask);
      if (kret != KERN_SUCCESS)
        {
          error ("Unable to locate task for process-id %d: %s.", pid,
                 MACH_ERROR_STRING (kret));
        }

      *ptask = itask;
      *ppid = pid;

      do_cleanups (cleanups);
    }
}

/* Execute a busywait loop and poll to see if a process of a given
   name has been started.  procname is case sensitive and does not
   include the pathname of the process.  */

static void
wait_for_process_by_name (const char *procname, struct pid_list *ignorepids)
{
  struct kinfo_proc *proc = NULL;
  size_t count, i;

  if (procname == NULL || procname[0] == '\0')
    return;

  printf_filtered ("Waiting for process '%s' to launch.\n", procname);

  while (1)
    {
      QUIT;
      macosx_fetch_task_info (&proc, &count);
      for (i = 0; i < count; i++)
        {
          if (pid_present_on_pidlist (proc[i].kp_proc.p_pid, ignorepids))
            continue;
          if (strncmp (proc[i].kp_proc.p_comm, procname, MAXCOMLEN) == 0)
            {
              xfree (proc);
              return;
            }
        }
      usleep (400);
      xfree (proc);
    }
}

/* -waitfor should ignore any processes by name that are already
   up & running -- we want to attach to the first newly-launched
   process.  So we begin by creating a list of all processes
   with that name that are executing/zombied/etc.  
   This function returns an xmalloc'ed array - the caller is 
   responsible for freeing it.  
   NULL is returned if there are no matching processes. */

static struct pid_list *
find_existing_processes_by_name (const char *procname)
{
  struct kinfo_proc *proc = NULL;
  struct pid_list *pidlist;
  size_t count, i;
  int matching_processes, j;

  macosx_fetch_task_info (&proc, &count);
  for (i = 0, matching_processes = 0; i < count; i++)
    if (strncmp (proc[i].kp_proc.p_comm, procname, MAXCOMLEN) == 0)
      matching_processes++;
  if (matching_processes == 0)
    {
      xfree (proc);
      return NULL;
    }

  pidlist = (struct pid_list *) xmalloc (sizeof (struct pid_list));
  pidlist->count = matching_processes;
  pidlist->pids = (pid_t *) xmalloc (sizeof (pid_t) * matching_processes);
  
  for (i = 0, j = 0; i < count; i++)
    if (strncmp (proc[i].kp_proc.p_comm, procname, MAXCOMLEN) == 0)
      pidlist->pids[j++] = proc[i].kp_proc.p_pid;

  xfree (proc);
  return pidlist;
}

/* Returns 1 if PID is present on PROCLIST. 
   0 if PID is not present or PROCLIST is empty.  */

static int
pid_present_on_pidlist (pid_t pid, struct pid_list *proclist)
{
  int i;
  if (proclist == NULL)
    return 0;
  for (i = 0; i < proclist->count ; i++)
    if (proclist->pids[i] == pid)
      return 1;
  return 0;
}

static int
macosx_lookup_task (char *args, task_t *ptask, int *ppid)
{
  char *pid_str = NULL;
  char *tmp = NULL;
  struct pid_list *ignorepids = NULL; /* processes to ignore */
  struct cleanup *cleanups = NULL;
  char **argv = NULL;
  unsigned int argc;

  unsigned long lpid = 0;
  int pid = 0;

  CHECK_FATAL (ptask != NULL);
  CHECK_FATAL (ppid != NULL);

  *ptask = TASK_NULL;
  *ppid = 0;

  if (args == NULL)
    {
      return 0;
    }

  argv = buildargv (args);
  if (argv == NULL)
    {
      nomem (0);
    }

  cleanups = make_cleanup_freeargv (argv);

  for (argc = 0; argv[argc] != NULL; argc++);

  switch (argc)
    {
    case 1:
      pid_str = argv[0];
      break;
    case 2:
      if (strcmp ("-waitfor", argv[0]) == 0 
          || strcmp ("--waitfor", argv[0]) == 0)
        {
           /* If the user-provided process name string is long than the
              proc name string provided by the kernel we'll need to truncate
              the user's string if we're going to find a match.  */
           pid_str = argv[1];
           if (strlen (pid_str) > MAXCOMLEN)
             pid_str[MAXCOMLEN] = '\0';
           ignorepids = find_existing_processes_by_name (pid_str);
           if (ignorepids)
             {
               make_cleanup (xfree, ignorepids->pids);
               make_cleanup (xfree, ignorepids);
             }
           wait_for_process_by_name (pid_str, ignorepids);
	   break;
        }
    default:
      error ("Usage: attach <-waitfor procname>|<pid>|<procname>.");
      break;
    }

  CHECK_FATAL (pid_str != NULL);
  lpid = strtoul (pid_str, &tmp, 10);
  if (isdigit (*pid_str) && (*tmp == '\0'))
    {
      if ((lpid > INT_MAX) || ((lpid == ULONG_MAX) && (errno == ERANGE)))
        {
          error ("Unable to locate pid \"%s\" (integer overflow).", pid_str);
        }
      pid_str = NULL;
      pid = lpid;
    }

  macosx_lookup_task_local (pid_str, pid, ptask, ppid, ignorepids);

  do_cleanups (cleanups);
  return 0;
}


static void
macosx_child_attach (char *args, int from_tty)
{
  task_t itask;
  int pid;
  int ret;
  kern_return_t kret;
  char *exec_file = NULL;

  if (args == NULL)
    {
      error_no_arg ("process-id to attach");
    }

  macosx_lookup_task (args, &itask, &pid);
  if (itask == TASK_NULL)
    {
      error ("unable to locate task");
    }

  if (itask == mach_task_self ())
    {
      error ("unable to debug self");
    }

  CHECK_FATAL (macosx_status != NULL);
  macosx_inferior_destroy (macosx_status);

  exec_file = get_exec_file (0);
  if (announce_attach)
    {
      if (exec_file)
	printf_filtered ("Attaching to program: `%s', %s.\n",
			 exec_file, target_pid_to_str (pid_to_ptid (pid)));
      else
	printf_filtered ("Attaching to %s.\n",
			 target_pid_to_str (pid_to_ptid (pid)));
    }

  /* classic-inferior-support
     A bit of a hack:  Despite being in the middle of macosx_child_attach(), 
     if we're about to attach to a classic process we're going to use an
     entirely different attach procedure and skip out of here.  */

  if (attaching_to_classic_process_p (pid))
    {
      attach_to_classic_process (pid);
      return;
    }
  
  /* A native (i386) gdb trying to attach to a translated (ppc) app will
     result in a gdb crash.  Let's flag it as an error instead.  */
  if (is_pid_classic (getpid ()) == 0 && is_pid_classic (pid) == 1)
    warning ("Attempting to attach to a PPC process with an i386 "
             "native gdb - attach will not succeed.");

  macosx_create_inferior_for_task (macosx_status, itask, pid);

  macosx_exception_thread_create (&macosx_status->exception_status,
                                  macosx_status->task);

  if (inferior_ptrace_on_attach_flag)
    {

      ret = call_ptrace (PTRACE_ATTACHEXC, pid, 0, 0);
      if (ret != 0)
        {
          macosx_inferior_destroy (macosx_status);
          if (errno == EPERM)
            {
              error ("Unable to attach to process-id %d: %s (%d).\n"
                     "This request requires that the target process be neither setuid nor "
                     "setgid and have the same real userid as the debugger, or that the "
                     "debugger be running with administrator privileges.",
                     pid, strerror (errno), errno);
            }
          else
            {
              error ("Unable to attach to process-id %d: %s (%d)",
                     pid, strerror (errno), errno);
            }
        }

      macosx_status->attached_in_ptrace = 1;
      macosx_status->stopped_in_ptrace = 0;
      macosx_status->stopped_in_softexc = 0;

      macosx_status->suspend_count = 0;

    }
  else if (inferior_bind_exception_port_flag)
    {

      kret = macosx_inferior_suspend_mach (macosx_status);
      if (kret != KERN_SUCCESS)
        {
          macosx_inferior_destroy (macosx_status);
          MACH_CHECK_ERROR (kret);
        }
    }

  macosx_check_new_threads (NULL, 0);

  inferior_ptid = ptid_build (pid, 0, macosx_status->last_thread);
  attach_flag = 1;

  push_target (&macosx_child_ops);

  if (macosx_status->attached_in_ptrace)
    {
      enum macosx_source_type source;
      unsigned char buf[1024];

      /* read attach notification.  This is a little gross, as it
         turns out.  We call ptrace with PT_ATTACHEXC and we get the
         EXC_SOFT_SIGNAL for the SIGSTOP, but for some reason I don't
         quite understand, we ALSO get a SIGSTOP from the waitpid in
         the signal thread.  This happens even though we haven't
         replied to the exception yet, or restarted the target.

         HOWEVER, when we attach to a program that's been stopped
         under CrashReporter, we don't get the waitpid SIGSTOP.  So we
         can't do a blocking wait for it or we will never return in
         the CrashReporter case.  Instead, we just poll the event
         threads and see if anything has come in.  If it's the
         SIGSTOP, we silently discard it.  

	 Finally...  We have to make sure the signal thread has a
         chance to run through the first waitpid call or we will pick
         up the SIGSTOP next time we look for events (when we go to
         continue.)  To do that we call macosx_fetch_event for the 
	 signal thread with a fairly long (.1 sec) timeout.
      */

      /* So this wait - which has only the exception thread running,
	 will get us the EXC_SOFT_SIGNAL,SIGSTOP.  */
      stop_soon = STOP_QUIETLY;
      wait_for_inferior ();

      /* Now we need to start up the signal thread, wait to give it a chance to 
	 start up, then we will snag the signal and discard it.  */

      macosx_signal_thread_create (&macosx_status->signal_status,
                                   macosx_status->pid);

      source = macosx_fetch_event (macosx_status, buf, sizeof (buf),
				   NEXT_SOURCE_SIGNAL, 100000);

      if (source == NEXT_SOURCE_SIGNAL)
	{
	  unsigned int signo;

	  macosx_signal_thread_message *msg 
	    = (macosx_signal_thread_message *) buf;
	  signo = target_signal_from_host (WSTOPSIG (msg->status));
	  if (signo != SIGSTOP)
	    {
	      /* If we are attaching to a crashed program - this
		 happens when Xcode catches the crash, for instance -
		 we'll pick up the crash signal rather than a SIGSTOP.  */
	      inferior_debug (2,"Attach returned signal: %d\n", signo);
	    }
	  else 
	    inferior_debug (2, "Got SIGSTOP from signal thread.");
	}
      else if (source == NEXT_SOURCE_NONE)
	{
	  inferior_debug (2, "No signal event on stop after attach.");
	}
    }

  if (inferior_auto_start_dyld_flag)
    {
      macosx_solib_add (NULL, 0, NULL, 0);
    }

  /* I don't have any good way to know whether the malloc library
     has been initialized yet.  But I'm going to guess that we are
     unlikely to be able to attach BEFORE then...  */
  /* BUT sometimes we get a process that has been stopped at the
     first instruction when launched so we can attach to it.  In
     that case, we know that malloc hasn't been inited.  We had
     better not set malloc inited in that case, or somebody will
     try to call a function that does malloc, and we will corrupt
     the target.

     Note, we don't know what the "first instruction is" we are just
     relying on the fact that it's currently _dyld_start.  Yecch...
     But I can't think of anything better to do.  */
  {
    extern char *dyld_symbols_prefix;
    int result;
    char *name;
    CORE_ADDR addr;

    result = find_pc_partial_function (stop_pc, &name, &addr, NULL);
    if (result != 0)
      {
	char *decorated_dyld_start;
	decorated_dyld_start = xmalloc ( strlen ("_dyld_start") 
				   + strlen (dyld_symbols_prefix) + 1);
	sprintf (decorated_dyld_start, "%s_dyld_start", dyld_symbols_prefix);
	/* I also check to make sure we're not too far away from
	   _dyld_start, in case dyld gets stripped and there are a
	   bunch of functions after dyld_start that don't have
	   symbols.  */
	if (strcmp (name, decorated_dyld_start) != 0
	    || stop_pc - addr > 30 )
	  {
	    macosx_set_malloc_inited (1);
	  }
	xfree (decorated_dyld_start);
      }
    else
      macosx_set_malloc_inited (1);

  }  
}

static void
macosx_child_detach (char *args, int from_tty)
{
  kern_return_t kret;

  CHECK_FATAL (macosx_status != NULL);

  if (ptid_equal (inferior_ptid, null_ptid))
    {
      return;
    }

  if (from_tty)
    {
      char *exec_file = get_exec_file (0);
      if (exec_file)
        printf_filtered ("Detaching from program: `%s', %s.\n",
                         exec_file, target_pid_to_str (inferior_ptid));
      else
        printf_filtered ("Detaching from %s.\n",
                         target_pid_to_str (inferior_ptid));
    }

  if (!macosx_inferior_valid (macosx_status))
    {
      target_mourn_inferior ();
      return;
    }

  macosx_inferior_check_stopped (macosx_status);
  CHECK (macosx_inferior_valid (macosx_status));

  if (macosx_status->attached_in_ptrace
      && (!macosx_status->stopped_in_ptrace)
      && (!macosx_status->stopped_in_softexc))
    {
      macosx_inferior_suspend_ptrace (macosx_status);
      CHECK_FATAL (macosx_status->stopped_in_ptrace
                   || macosx_status->stopped_in_softexc);
    }

  if (inferior_bind_exception_port_flag)
    {
      kret =
        macosx_restore_exception_ports (macosx_status->task,
                                        &macosx_status->exception_status.
                                        saved_exceptions);
      MACH_CHECK_ERROR (kret);
    }

  if (macosx_status->attached_in_ptrace)
    {
      macosx_inferior_resume_ptrace (macosx_status, 0, 0, PTRACE_DETACH);
    }

  if (!macosx_inferior_valid (macosx_status))
    {
      target_mourn_inferior ();
      return;
    }

  macosx_inferior_suspend_mach (macosx_status);

  if (!macosx_inferior_valid (macosx_status))
    {
      target_mourn_inferior ();
      return;
    }

  prepare_threads_before_run (macosx_status, 0, THREAD_NULL, 0);
  macosx_inferior_resume_mach (macosx_status, -1);

  target_mourn_inferior ();
  return;
}

static int
macosx_kill_inferior (void *arg)
{
  kern_return_t *errval = (kern_return_t *) arg;
  int status;

  CHECK_FATAL (macosx_status != NULL);
  *errval = KERN_SUCCESS;

  if (ptid_equal (inferior_ptid, null_ptid))
    {
      return 1;
    }

  if (!macosx_inferior_valid (macosx_status))
    {
      target_mourn_inferior ();
      return 1;
    }

  macosx_inferior_check_stopped (macosx_status);
  CHECK (macosx_inferior_valid (macosx_status));

  if (macosx_status->attached_in_ptrace
      && (!macosx_status->stopped_in_ptrace)
      && (!macosx_status->stopped_in_softexc))
    {
      macosx_inferior_suspend_ptrace (macosx_status);
      CHECK_FATAL ((macosx_status->stopped_in_ptrace
                    || macosx_status->stopped_in_softexc));
    }

  macosx_inferior_suspend_mach (macosx_status);
  prepare_threads_before_run (macosx_status, 0, THREAD_NULL, 0);

  if (macosx_status->attached_in_ptrace)
    {
      CHECK_FATAL (macosx_status->stopped_in_ptrace
                   || macosx_status->stopped_in_softexc);
      if (call_ptrace (PTRACE_KILL, macosx_status->pid, 0, 0) != 0)
        {
          error ("macosx_child_detach: ptrace (%d, %d, %d, %d): %s",
                 PTRACE_KILL, macosx_status->pid, 0, 0, strerror (errno));
        }
      macosx_status->stopped_in_ptrace = 0;
      macosx_status->stopped_in_softexc = 0;
    }

  if (!macosx_inferior_valid (macosx_status))
    {
      target_mourn_inferior ();
      return 1;
    }

  macosx_inferior_resume_mach (macosx_status, -1);
  sched_yield ();

  wait (&status);

  target_mourn_inferior ();

  return 1;
}

static void
macosx_kill_inferior_safe ()
{
  kern_return_t kret;
  int ret;

  ret = catch_errors (macosx_kill_inferior, &kret,
                      "error while killing target (killing anyway): ",
                      RETURN_MASK_ALL);

  if (ret == 0)
    {
      kret = task_terminate (macosx_status->task);
      MACH_WARN_ERROR (kret);
      sched_yield ();
      target_mourn_inferior ();
    }
}

static void
macosx_ptrace_me ()
{
  call_ptrace (PTRACE_TRACEME, 0, 0, 0);
  call_ptrace (PTRACE_SIGEXC, 0, 0, 0);
}

static void
macosx_ptrace_him (int pid)
{
  task_t itask;
  kern_return_t kret;
  int traps_expected;

  CHECK_FATAL (!macosx_status->attached_in_ptrace);
  CHECK_FATAL (!macosx_status->stopped_in_ptrace);
  CHECK_FATAL (!macosx_status->stopped_in_softexc);
  CHECK_FATAL (macosx_status->suspend_count == 0);

  kret = task_for_pid (mach_task_self (), pid, &itask);
  {
    char buf[64];
    sprintf (buf, "%s=%d", "TASK", itask);
    putenv (buf);
  }
  if (kret != KERN_SUCCESS)
    {
      error ("Unable to find Mach task port for process-id %d: %s (0x%lx).",
             pid, MACH_ERROR_STRING (kret), (unsigned long) kret);
    }

  inferior_debug (2, "inferior task: 0x%08x, pid: %d\n", itask, pid);

  push_target (&macosx_child_ops);
  macosx_create_inferior_for_task (macosx_status, itask, pid);

  macosx_signal_thread_create (&macosx_status->signal_status,
                               macosx_status->pid);
  macosx_exception_thread_create (&macosx_status->exception_status,
                                  macosx_status->task);

  macosx_status->attached_in_ptrace = 1;
  macosx_status->stopped_in_ptrace = 0;
  macosx_status->stopped_in_softexc = 0;

  macosx_status->suspend_count = 0;

  /* This set how many traps we expect before we get to the
     PT_TRACEME trap we really care about.  When using just
     "exec" in the shell, we'll get one for the shell exec'ing,
     and one for the exec command.  For "arch" there's one more
     for the "arch" command running.  */

#ifdef USE_ARCH_FOR_EXEC
  traps_expected = (start_with_shell_flag ? 3 : 1);
#else
  traps_expected = (start_with_shell_flag ? 2 : 1);
#endif
  startup_inferior (traps_expected);

  if (ptid_equal (inferior_ptid, null_ptid))
    {
      return;
    }

  if (!macosx_task_valid (macosx_status->task))
    {
      target_mourn_inferior ();
      return;
    }

  macosx_inferior_check_stopped (macosx_status);
  CHECK (macosx_inferior_valid (macosx_status));

  if (inferior_ptrace_flag)
    {
      CHECK_FATAL (macosx_status->attached_in_ptrace);
      CHECK_FATAL (macosx_status->stopped_in_ptrace
                   || macosx_status->stopped_in_softexc);
    }
  else
    {
      macosx_inferior_resume_ptrace (macosx_status, 0, 0, PTRACE_DETACH);
      CHECK_FATAL (!macosx_status->attached_in_ptrace);
      CHECK_FATAL (!macosx_status->stopped_in_ptrace);
      CHECK_FATAL (!macosx_status->stopped_in_softexc);
    }
}

#if defined (TARGET_ARM)
/* This version of macosx_child_create_inferior is needed to work
   around the fact that the task port doesn't persist across a
   fork/exec.  It would be nice if we could just switch to the new
   task after the exec, but that is currently buggy, and the
   PT_THUPDATE stops working.  So instead, this code just lets
   posix_spawn launch the app (with the POSIX_SPAWN_START_SUSPENDED
   argument telling the target to stop on the first instruction.)
   Then we attach, and continue.
   This version silently ignores the "start-with-shell" setting.  */

static void
macosx_child_create_inferior (char *exec_file, char *allargs, char **env,
			      int from_tty)
{  
  pid_t new_pid;
  char pid_str[32];
  static int debug_setpgrp = 657473;
  unsigned int i;
  char **argt = NULL;
  static char **argv;
  char *fileptr;
  posix_spawnattr_t attr;
  int retval;
  cpu_type_t cpu = 0;
  const char *osabi_name = gdbarch_osabi_name (gdbarch_osabi (current_gdbarch));
  struct gdb_exception e;

  /* These comes from fork_child.c:  */
  extern char *exec_pathname;
  extern char *exec_argv0;

  /* If no exec file handed to us, get it from the exec-file command
     -- with a good, common error message if none is specified.  */
  if (exec_file == 0)
    exec_file = get_exec_file (1);
  
  if (exec_pathname[0] != '\0')
    fileptr = exec_pathname;
  else
    fileptr = exec_file;
  
  argt = buildargv (allargs);
  if (argt == NULL)
    {
      error ("unable to build argument vector for inferior process (out of memory)");
    }

  if ((allargs == NULL) || (allargs[0] == '\0'))
    argt[0] = NULL;
  for (i = 0; argt[i] != NULL; i++);
  argv = (char **) xmalloc ((i + 1 + 1) * (sizeof *argt));
  argv[0] = exec_file;
  if (exec_argv0[0] != '\0')
    argv[0] = exec_argv0;
  memcpy (&argv[1], argt, (i + 1) * sizeof (*argt));
  /* freeargv (argt); */
  
#if defined (TARGET_POWERPC)
  if (strcmp (osabi_name, "Darwin") == 0)
    {
      cpu = CPU_TYPE_POWERPC;
    }
  else if (strcmp (osabi_name, "Darwin64") == 0)
    {
      cpu = CPU_TYPE_POWERPC64;
    }
#elif defined (TARGET_I386)
  if (strcmp (osabi_name, "Darwin") == 0)
    {
      cpu = CPU_TYPE_I386;
    }
  else if (strcmp (osabi_name, "Darwin64") == 0)
    {
      cpu = CPU_TYPE_X86_64;
    }
#elif defined (TARGET_ARM)

#if 0
  /* Don't set the CPU type for ARM as we currently can't reliably set
     the CPU type with posix_spawnattr_setbinpref_np as it will pick the 
     first CPU type that matches CPU_TYPE_ARM which isn't necessarily the
     one that we want to run. If we ever want to use 
     posix_spawnattr_setbinpref_np for ARM, we will also need a way to set
     the CPU subtype.  */
  if (strcmp (osabi_name, "Darwin") == 0)
    {
      cpu = CPU_TYPE_ARM;
    }
  else if (strcmp (osabi_name, "DarwinV6") == 0)
    {
      cpu = CPU_TYPE_ARM;
    }
  else if (strcmp (osabi_name, "DarwinV7") == 0)
    {
      cpu = CPU_TYPE_ARM;
    }
#endif
#endif

  retval = posix_spawnattr_init (&attr);
  if (retval != 0)
    error ("Couldn't initialize attributes for posix_spawn, error: %d", retval);

  retval = posix_spawnattr_setflags(&attr, POSIX_SPAWN_START_SUSPENDED);
  if (retval != 0)
    error ("Couldn't add POSIX_SPAWN_SETEXEC to attributes, error: %d", retval);

  if (cpu != 0)
    {
      size_t copied = 0;	
      retval = posix_spawnattr_setbinpref_np(&attr, 1, &cpu, &copied);
      if (retval != 0 || copied != 1)
	  error ("Couldn't set the binary preferences, error: %d", retval);
    }
  retval = posix_spawnattr_setpgroup (&attr, debug_setpgrp);
  if (retval != 0)
    error ("Couldn't set the process group, error: %d", retval);

  retval = posix_spawnp (&new_pid, fileptr, NULL,  &attr, argv, env);

  snprintf (pid_str, 31, "%d", new_pid);

  announce_attach = 0;
  TRY_CATCH (e, RETURN_MASK_ERROR)
    {
      macosx_child_attach (pid_str, from_tty);
    }
  announce_attach = 1;
  /* Clear the attach flag that was set by calling macosx_child_attach() so
     if/when gdb is quit it won't keep the child process around and 
     running.  */
  attach_flag = 0;

  if (e.reason != NO_ERROR)
    throw_exception (e);

  if (target_can_async_p ())
    target_async (inferior_event_handler, 0);

  clear_proceed_status ();
  proceed ((CORE_ADDR) - 1, TARGET_SIGNAL_0, 0);

}
#else
static void
macosx_child_create_inferior (char *exec_file, char *allargs, char **env,
			      int from_tty)
{  
  if ((exec_bfd != NULL) &&
      (exec_bfd->xvec->flavour == bfd_target_pef_flavour
       || exec_bfd->xvec->flavour == bfd_target_pef_xlib_flavour))
    {
      error
        ("Can't run a PEF binary - use LaunchCFMApp as the executable file.");
    }

  /* It's not safe to call functions in the target until we've initialized
     the libsystem malloc package.  So for now, mark it unsafe.  */
  macosx_set_malloc_inited (0);

  fork_inferior (exec_file, allargs, env, macosx_ptrace_me, macosx_ptrace_him,
                 NULL, NULL);
  if (ptid_equal (inferior_ptid, null_ptid))
    return;

  macosx_dyld_create_inferior_hook ((CORE_ADDR) - 1);

  attach_flag = 0;

  if (target_can_async_p ())
    target_async (inferior_event_handler, 0);

  clear_proceed_status ();
  proceed ((CORE_ADDR) - 1, TARGET_SIGNAL_0, 0);
}
#endif

static void
macosx_child_files_info (struct target_ops *ops)
{
  CHECK_FATAL (macosx_status != NULL);
  macosx_debug_inferior_status (macosx_status);
}

static char *
macosx_pid_to_str (ptid_t ptid)
{
  static char buf[128];
  int pid = ptid_get_pid (ptid);
  struct thread_info *tp;

  tp = find_thread_pid (ptid);
  if (tp->private == NULL || tp->private->app_thread_port == 0)
    {
      thread_t thread = ptid_get_tid (ptid);
      sprintf (buf, "process %d local thread 0x%lx", pid,
               (unsigned long) thread);
    }
  else
    sprintf (buf, "process %d thread 0x%lx", pid,
             (unsigned long) tp->private->app_thread_port);

  return buf;
}

static int
macosx_child_thread_alive (ptid_t ptid)
{
  return macosx_thread_valid (macosx_status->task, ptid_get_tid (ptid));
}

void
macosx_create_inferior_for_task (struct macosx_inferior_status *inferior, 
                                 task_t task, int pid)
{
  CHECK_FATAL (inferior != NULL);

  macosx_inferior_destroy (inferior);
  macosx_inferior_reset (inferior);

  inferior->task = task;
  inferior->pid = pid;

  inferior->attached_in_ptrace = 0;
  inferior->stopped_in_ptrace = 0;
  inferior->stopped_in_softexc = 0;

  inferior->suspend_count = 0;

  inferior->last_thread = macosx_primary_thread_of_task (inferior->task);
}

static int remote_async_terminal_ours_p = 1;
static void (*ofunc) (int);
static PTR sigint_remote_twice_token;
static PTR sigint_remote_token;

static void remote_interrupt_twice (int signo);
static void remote_interrupt (int signo);
static void handle_remote_sigint_twice (int sig);
static void handle_remote_sigint (int sig);
static void async_remote_interrupt_twice (gdb_client_data arg);
static void async_remote_interrupt (gdb_client_data arg);

static void
interrupt_query (void)
{
  target_terminal_ours ();

  if (query ("Interrupted while waiting for the program.\n\
Give up (and stop debugging it)? "))
    {
      target_mourn_inferior ();
      deprecated_throw_reason (RETURN_QUIT);
    }

  target_terminal_inferior ();
}

static void
remote_interrupt_twice (int signo)
{
  signal (signo, ofunc);
  interrupt_query ();
  signal (signo, remote_interrupt);
}

static void
remote_interrupt (int signo)
{
  signal (signo, remote_interrupt_twice);
  target_stop ();
}

static void
handle_remote_sigint_twice (int sig)
{
  signal (sig, handle_sigint);
  sigint_remote_twice_token =
    create_async_signal_handler (inferior_event_handler_wrapper, NULL);
  mark_async_signal_handler_wrapper (sigint_remote_twice_token);
}

static void
handle_remote_sigint (int sig)
{
  signal (sig, handle_remote_sigint_twice);
  sigint_remote_twice_token =
    create_async_signal_handler (async_remote_interrupt_twice, NULL);
  mark_async_signal_handler_wrapper (sigint_remote_token);
}

static void
async_remote_interrupt_twice (gdb_client_data arg)
{
  if (target_executing)
    {
      interrupt_query ();
      signal (SIGINT, handle_remote_sigint);
    }
}

static void
async_remote_interrupt (gdb_client_data arg)
{
  target_stop ();
}

static void
cleanup_sigint_signal_handler (void *dummy)
{
  signal (SIGINT, handle_sigint);
  if (sigint_remote_twice_token)
    delete_async_signal_handler ((struct async_signal_handler **)
                                 &sigint_remote_twice_token);
  if (sigint_remote_token)
    delete_async_signal_handler ((struct async_signal_handler **)
                                 &sigint_remote_token);
}

static void
initialize_sigint_signal_handler (void)
{
  sigint_remote_token =
    create_async_signal_handler (async_remote_interrupt, NULL);
  signal (SIGINT, handle_remote_sigint);
}

static void
macosx_terminal_inferior (void)
{
  terminal_inferior ();

  if (!sync_execution)
    return;
  if (!remote_async_terminal_ours_p)
    return;
  CHECK_FATAL (sync_execution);
  CHECK_FATAL (remote_async_terminal_ours_p);
  delete_file_handler (input_fd);
  remote_async_terminal_ours_p = 0;
  initialize_sigint_signal_handler ();
}

static void
macosx_terminal_ours (void)
{
  terminal_ours ();

  if (!sync_execution)
    return;
  if (remote_async_terminal_ours_p)
    return;
  CHECK_FATAL (sync_execution);
  CHECK_FATAL (!remote_async_terminal_ours_p);
  cleanup_sigint_signal_handler (NULL);

  add_file_handler (input_fd, stdin_event_handler, 0);

  remote_async_terminal_ours_p = 1;
}

static void
macosx_file_handler (int error, gdb_client_data client_data)
{
  async_client_callback (INF_REG_EVENT, async_client_context);
}

static void
macosx_async (void (*callback) (enum inferior_event_type event_type,
                                void *context), void *context)
{
  if (current_target.to_async_mask_value == 0)
    internal_error (__FILE__, __LINE__,
                    "Calling remote_async when async is masked");

  if (callback != NULL)
    {
      async_client_callback = callback;
      async_client_context = context;
      if (macosx_status->exception_status.error_receive_fd > 0)
        add_file_handler (macosx_status->exception_status.error_receive_fd,
                          macosx_file_handler, NULL);
      if (macosx_status->exception_status.receive_from_fd > 0)
        add_file_handler (macosx_status->exception_status.receive_from_fd,
                          macosx_file_handler, NULL);
      if (macosx_status->signal_status.receive_fd > 0)
        add_file_handler (macosx_status->signal_status.receive_fd,
                          macosx_file_handler, NULL);
    }
  else
    {
      if (macosx_status->exception_status.error_receive_fd > 0)
        delete_file_handler (macosx_status->exception_status.error_receive_fd);
      if (macosx_status->exception_status.receive_from_fd > 0)
        delete_file_handler (macosx_status->exception_status.receive_from_fd);
      if (macosx_status->signal_status.receive_fd > 0)
        delete_file_handler (macosx_status->signal_status.receive_fd);
    }
}

static char *macosx_unsafe_functions[] = {
  "malloc",
  "free",
  "szone_malloc",
  "szone_free",
  "_class_lookupMethodAndLoadCache",
  "look_up_class"
};

static regex_t *macosx_unsafe_patterns = NULL;
static int num_unsafe_patterns;
 
int
macosx_check_safe_call (void)
{
  /* If we haven't initialized the malloc library yet, don't even
     try to call functions.  It is very unlikely to succeed...  */
  if (macosx_get_malloc_inited () == 0)
      return 0;

  if (macosx_unsafe_patterns == NULL)
    {
      int i;
      
      num_unsafe_patterns = sizeof (macosx_unsafe_functions) / sizeof (char *);
      macosx_unsafe_patterns = (regex_t *) 
	xcalloc (num_unsafe_patterns, sizeof (regex_t));

      for (i = 0; i < num_unsafe_patterns; i++)
	{
	  int err_code;
	  err_code = regcomp (macosx_unsafe_patterns + i, 
			      macosx_unsafe_functions[i],
			      REG_EXTENDED|REG_NOSUB);
	  if (err_code != 0)
	    {
	      char err_str[512];
	      regerror (err_code, macosx_unsafe_patterns + i,
				  err_str, 512);
	      internal_error (__FILE__, __LINE__,
			      "Couldn't compile unsafe call pattern %s, error %s", 
			      macosx_unsafe_functions[i], err_str);
	    }
	}
    }
  return check_safe_call (macosx_unsafe_patterns, num_unsafe_patterns, 5, 
			  CHECK_SCHEDULER_VALUE);
}

/* Info for a random fork (actually a random process). When fork-based checkpoints
   are fully functional, this can go away.  */

/* static */ void
cpfork_info (char *args, int from_tty)
{
  task_t itask;
  int pid;
  int total = 0;

  if (args == NULL)
    {
      pid = ptid_get_pid (inferior_ptid);
    }

  macosx_lookup_task (args, &itask, &pid);
  if (itask == TASK_NULL)
    {
      error ("unable to locate task with pid %d", pid);
      return;
    }

 {
    vm_address_t        address = 0;
    vm_size_t           size = 0;
    kern_return_t       err = 0;

    while (1) {
      mach_msg_type_number_t  count;
      struct vm_region_submap_info_64 info;
      natural_t nestingDepth;

      count = VM_REGION_SUBMAP_INFO_COUNT_64;
      err = vm_region_recurse_64(itask, &address, &size, &nestingDepth,
				 (vm_region_info_64_t)&info,&count);
      if (err == KERN_INVALID_ADDRESS) {
	break;
      } else if (err) {
	mach_error("vm_region",err);
	break; // reached last region
      }

      ++total;

      if (info.protection & VM_PROT_WRITE)
	{
	  printf("addr 0x%s size 0x%s (", paddr_nz (address), paddr_nz (size));
	  {
	    int rslt;
	    vm_offset_t mempointer;       /* local copy of inferior's memory */
	    mach_msg_type_number_t memcopied;     /* for vm_read to use */

	    rslt = mach_vm_read (itask, address, 16, &mempointer, &memcopied);

	    if (rslt == KERN_SUCCESS)
	      {
		int i;

		for (i = 0; i < 16; ++i)
		  printf("%x ", ((unsigned char *) mempointer)[i]);
	      }
	  }
	  printf(")\n");
	}

      if (info.is_submap) { // is it a submap?
	nestingDepth++;
      } else {
	address = address+size;
      }
    }
 }
  printf ("%d regions total.\n", total);
}

/* Checkpoint support.  */

/* Given a checkpoint, collect blocks of memory from the inferior and
   save them in GDB. This is very inefficient, and should only be used
   as a fallback.  */

void
direct_memcache_get (struct checkpoint *cp)
{
  task_t itask;
  int kret;
  int pid = cp->pid;
  vm_address_t        address = 0;
  vm_size_t           size = 0;
  kern_return_t       err = 0;

  kret = task_for_pid (mach_task_self (), pid, &itask);
  if (kret != KERN_SUCCESS)
    {
      error ("Unable to locate task for process-id %d: %s.", pid,
	     MACH_ERROR_STRING (kret));
    }
  if (itask == TASK_NULL)
    {
      error ("unable to locate task");
      return;
    }

  while (1)
    {
      mach_msg_type_number_t  count;
      struct vm_region_submap_info_64 info;
      natural_t nesting_depth;

      count = VM_REGION_SUBMAP_INFO_COUNT_64;
      err = vm_region_recurse_64 (itask, &address, &size, &nesting_depth,
				  (vm_region_info_64_t) &info, &count);
      if (err == KERN_INVALID_ADDRESS)
	{
	  break;
	}
      else if (err)
	{
	  mach_error ("vm_region",err);
	  break; // reached last region
	}

      if (info.protection & VM_PROT_WRITE)
	{
	  memcache_get (cp, address, size);
	}
      
      if (info.is_submap)
	nesting_depth++;
      else
	address += size;
    }
}

/* Given a checkpoint, collect blocks of memory from the fork that is serving
   as its "backing store", and install them into the current inferior.  */

void
fork_memcache_put (struct checkpoint *cp)
{
  task_t itask;
  int kret;
  int pid = cp->pid;
  vm_address_t        address = 0;
  vm_size_t           size = 0;
  kern_return_t       err = 0;

  kret = task_for_pid (mach_task_self (), pid, &itask);
  if (kret != KERN_SUCCESS)
    {
      error ("Unable to locate task for process-id %d: %s.", pid,
	     MACH_ERROR_STRING (kret));
    }
  if (itask == TASK_NULL)
    {
      error ("unable to locate task");
      return;
    }

  while (1)
    {
      mach_msg_type_number_t  count;
      struct vm_region_submap_info_64 info;
      natural_t nesting_depth;

      count = VM_REGION_SUBMAP_INFO_COUNT_64;
      err = vm_region_recurse_64 (itask, &address, &size, &nesting_depth,
				  (vm_region_info_64_t) &info, &count);
      if (err == KERN_INVALID_ADDRESS)
	{
	  break;
	}
      else if (err)
	{
	  mach_error ("vm_region",err);
	  break; // reached last region
	}

      if (info.protection & VM_PROT_WRITE)
	{
	  int rslt;
	  vm_offset_t mempointer;       /* local copy of inferior's memory */
	  mach_msg_type_number_t memcopied;     /* for vm_read to use */

	  if (0)
	    printf("count now %d addr 0x%s size 0x%s\n", count, 
                   paddr_nz (address), paddr_nz (size));
	  
	  rslt = mach_vm_read (itask, address, size, &mempointer, &memcopied);

	  if (0)
	    printf("rslt is %d, copied %d\n", rslt, memcopied);

	  if (rslt == KERN_SUCCESS)
	    {
	      target_write (&current_target, TARGET_OBJECT_MEMORY, NULL,
			    (bfd_byte *) mempointer, address, memcopied);
	    }
	}
      
      if (info.is_submap)
	nesting_depth++;
      else
	address += size;
    }
}

void
_initialize_macosx_inferior ()
{
  CHECK_FATAL (macosx_status == NULL);
  macosx_status = (struct macosx_inferior_status *)
    xmalloc (sizeof (struct macosx_inferior_status));

  macosx_inferior_reset (macosx_status);

  dyld_init_paths (&macosx_dyld_status.path_info);
  dyld_objfile_info_init (&macosx_dyld_status.current_info);

  init_child_ops ();
  macosx_child_ops = deprecated_child_ops;
  deprecated_child_ops.to_can_run = NULL;

  init_exec_ops ();
  macosx_exec_ops = exec_ops;
  exec_ops.to_can_run = NULL;
  
  macosx_exec_ops.to_shortname = "macos-exec";
  macosx_exec_ops.to_longname = "Mac OS X executable";
  macosx_exec_ops.to_doc = "Mac OS X executable";
  macosx_exec_ops.to_can_async_p = standard_can_async_p;
  macosx_exec_ops.to_is_async_p = standard_is_async_p;

  macosx_exec_ops.to_has_thread_control = tc_schedlock | tc_switch;

  macosx_exec_ops.to_find_exception_catchpoints
    = macosx_find_exception_catchpoints;
  macosx_exec_ops.to_enable_exception_callback
    = macosx_enable_exception_callback;
  macosx_exec_ops.to_get_current_exception_event
    = macosx_get_current_exception_event;

  macosx_complete_child_target (&macosx_exec_ops);

  /* We don't currently ever use the "macos-exec" target ops.
     Instead, just make them be the default exec_ops.  */

  /* FIXME: The original intent was that we have a macosx_exec_ops
     struct, that inherits from exec_ops, and provides some extra
     functions.  But the problem is that the exec target gets pushed
     in generic code in exec.c, and that code has exec_ops hard-coded
     into it.  We should most likely make the exec-target be
     determined along with the architecture by the ABI recognizer.  */

  exec_ops = macosx_exec_ops;
  macosx_exec_ops.to_can_run = NULL;

  macosx_child_ops.to_shortname = "macos-child";
  macosx_child_ops.to_longname = "Mac OS X child process";
  macosx_child_ops.to_doc =
    "Mac OS X child process (started by the \"run\" command).";
  macosx_child_ops.to_attach = macosx_child_attach;
  macosx_child_ops.to_detach = macosx_child_detach;
  macosx_child_ops.to_create_inferior = macosx_child_create_inferior;
  macosx_child_ops.to_files_info = macosx_child_files_info;
  macosx_child_ops.to_wait = macosx_child_wait;
  macosx_child_ops.to_mourn_inferior = macosx_mourn_inferior;
  macosx_child_ops.to_kill = macosx_kill_inferior_safe;
  macosx_child_ops.to_stop = macosx_child_stop;
  macosx_child_ops.to_resume = macosx_child_resume;
  macosx_child_ops.to_thread_alive = macosx_child_thread_alive;
  macosx_child_ops.to_pid_to_str = macosx_pid_to_str;
  macosx_child_ops.to_load = NULL;
  macosx_child_ops.deprecated_xfer_memory = mach_xfer_memory;
  macosx_child_ops.to_xfer_partial = mach_xfer_partial;
  macosx_child_ops.to_can_async_p = standard_can_async_p;
  macosx_child_ops.to_is_async_p = standard_is_async_p;
  macosx_child_ops.to_terminal_inferior = macosx_terminal_inferior;
  macosx_child_ops.to_terminal_ours = macosx_terminal_ours;
  macosx_child_ops.to_async = macosx_async;
  macosx_child_ops.to_async_mask_value = 1;
  macosx_child_ops.to_bind_function = dyld_lookup_and_bind_function;
  macosx_child_ops.to_check_safe_call = macosx_check_safe_call;
  macosx_child_ops.to_allocate_memory = macosx_allocate_space_in_inferior;
  macosx_child_ops.to_check_is_objfile_loaded = dyld_is_objfile_loaded;
#if defined (TARGET_ARM)
  macosx_child_ops.to_keep_going = arm_macosx_keep_going;
  macosx_child_ops.to_save_thread_inferior_status = arm_macosx_save_thread_inferior_status;
  macosx_child_ops.to_restore_thread_inferior_status = arm_macosx_restore_thread_inferior_status;
  macosx_child_ops.to_free_thread_inferior_status = arm_macosx_free_thread_inferior_status;
#endif
  macosx_child_ops.to_has_thread_control = tc_schedlock | tc_switch;

  macosx_child_ops.to_find_exception_catchpoints
    = macosx_find_exception_catchpoints;
  macosx_child_ops.to_enable_exception_callback
    = macosx_enable_exception_callback;
  macosx_child_ops.to_get_current_exception_event
    = macosx_get_current_exception_event;

  macosx_child_ops.to_find_exception_catchpoints
    = macosx_find_exception_catchpoints;
  macosx_child_ops.to_enable_exception_callback
    = macosx_enable_exception_callback;
  macosx_child_ops.to_get_current_exception_event
    = macosx_get_current_exception_event;

  macosx_complete_child_target (&macosx_child_ops);

  add_target (&macosx_exec_ops);
  add_target (&macosx_child_ops);

  inferior_stderr = fdopen (fileno (stderr), "w");
  inferior_debug (2, "GDB task: 0x%lx, pid: %d\n", mach_task_self (),
                  getpid ());

  add_setshow_boolean_cmd ("inferior-bind-exception-port", class_obscure,
			   &inferior_bind_exception_port_flag, _("\
Set if GDB should bind the task exception port."), _("\
Show if GDB should bind the task exception port."), NULL,
			   NULL, NULL,
			   &setlist, &showlist);

  add_setshow_boolean_cmd ("inferior-ptrace", class_obscure,
			   &inferior_ptrace_flag, _("\
Set if GDB should attach to the subprocess using ptrace ()."), _("\
Show if GDB should attach to the subprocess using ptrace ()."), NULL,
			   NULL, NULL,
			   &setlist, &showlist);

  add_setshow_boolean_cmd ("inferior-ptrace-on-attach", class_obscure,
			   &inferior_ptrace_on_attach_flag, _("\
Set if GDB should attach to the subprocess using ptrace ()."), _("\
Show if GDB should attach to the subprocess using ptrace ()."), NULL,
			   NULL, NULL,
			   &setlist, &showlist);

  add_info ("fork", cpfork_info, "help");
}

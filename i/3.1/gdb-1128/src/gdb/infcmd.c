/* Memory-access and commands for "inferior" process, for GDB.

   Copyright 1986, 1987, 1988, 1989, 1990, 1991, 1992, 1993, 1994,
   1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005
   Free Software Foundation, Inc.

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
#include <signal.h>
#include "gdb_string.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "frame.h"
#include "inferior.h"
#include "environ.h"
#include "value.h"
#include "gdbcmd.h"
#include "symfile.h"
#include "gdbcore.h"
#include "target.h"
#include "language.h"
/* APPLE LOCAL - subroutine inlining  */
#include "annotate.h"
#include "symfile.h"
#include "objfiles.h"
#include "completer.h"
#include "ui-out.h"
#include "event-top.h"
#include "parser-defs.h"
/* APPLE LOCAL top.h */
#include "top.h"
#include "regcache.h"
#include "reggroups.h"
#include "block.h"
#include "solib.h"
#include <ctype.h>
#include "gdb_assert.h"
/* APPLE LOCAL - subroutine inlining  */
#include "inlining.h"
#include "macosx/macosx-nat-dyld.h"

/* APPLE LOCAL checkpoints */
#include "checkpoint.h"

/* APPLE LOCAL begin process completer */
#ifndef PROCESS_COMPLETER
#define PROCESS_COMPLETER noop_completer
#endif

extern char **process_completer (char *, char *);
/* APPLE LOCAL end process completer */

/* Functions exported for general use, in inferior.h: */

void all_registers_info (char *, int);

void registers_info (char *, int);

void nexti_command (char *, int);

void stepi_command (char *, int);

void continue_command (char *, int);

void interrupt_target_command (char *args, int from_tty);

/* Local functions: */

static void nofp_registers_info (char *, int);

static void print_return_value (int struct_return, struct type *value_type);

static void finish_command_continuation (struct continuation_arg *);

static void until_next_command (int);

static void until_command (char *, int);

static void path_info (char *, int);

static void path_command (char *, int);

static void unset_command (char *, int);

static void float_info (char *, int);

static void detach_command (char *, int);

static void disconnect_command (char *, int);

static void unset_environment_command (char *, int);

static void set_environment_command (char *, int);

static void environment_info (char *, int);

static void program_info (char *, int);

/* APPLE LOCAL pid info */
void pid_info (char *, int);

static void finish_command (char *, int);

static void signal_command (char *, int);

static void jump_command (char *, int);

static void step_1 (int, int, char *);
static void step_1_inlining (int, int, char *);
static void step_1_no_inlining (int, int, char *);
/* APPLE LOCAL make step_once globally visible */
static void step_1_continuation (struct continuation_arg *arg);

/* APPLE LOCAL begin checkpoints */
void re_exec_1 ();
void re_exec_1_continuation (struct continuation_arg *arg);
void re_exec_once (int count);
/* APPLE LOCAL end checkpoints */

static void next_command (char *, int);

static void step_command (char *, int);

static void run_command (char *, int);

static void run_no_args_command (char *args, int from_tty);

static void go_command (char *line_no, int from_tty);

int strip_bg_char (char **);

void _initialize_infcmd (void);

#define GO_USAGE   "Usage: go <location>\n"

#define ERROR_NO_INFERIOR \
   if (!target_has_execution) error (_("The program is not being run."));

/* String containing arguments to give to the program, separated by spaces.
   Empty string (pointer to '\0') means no args.  */

static char *inferior_args;

/* The inferior arguments as a vector.  If INFERIOR_ARGC is nonzero,
   then we must compute INFERIOR_ARGS from this (via the target).  */

static int inferior_argc;
static char **inferior_argv;

/* APPLE LOCAL begin start with shell */
/* Use this to tell whether we go to start the inferior using a shell or
   not.  This has to be here, rather than in fork-child.c, because it is
   used in construct_inferior_arguments, which is also used here, but not
   everyone uses fork-child... */

int start_with_shell_flag = STARTUP_WITH_SHELL;
/* APPLE LOCAL end start with shell */

/* File name for default use for standard in/out in the inferior.  */

static char *inferior_io_terminal;

/* Pid of our debugged inferior, or 0 if no inferior now.
   Since various parts of infrun.c test this to see whether there is a program
   being debugged it should be nonzero (currently 3 is used) for remote
   debugging.  */

ptid_t inferior_ptid;

/* Last signal that the inferior received (why it stopped).  */

enum target_signal stop_signal;

/* Address at which inferior stopped.  */

CORE_ADDR stop_pc;

/* Chain containing status of breakpoint(s) that we have stopped at.  */

bpstat stop_bpstat;

/* Flag indicating that a command has proceeded the inferior past the
   current breakpoint.  */

int breakpoint_proceeded;

/* Nonzero if stopped due to a step command.  */

int stop_step;

/* Nonzero if stopped due to completion of a stack dummy routine.  */

int stop_stack_dummy;

/* Nonzero if stopped due to a random (unexpected) signal in inferior
   process.  */

int stopped_by_random_signal;

/* APPLE LOCAL begin subroutine inlining  */

/* Nonzero if user requested stepping over an inlined subroutine.  */

int stepping_over_inlined_subroutine;

/* Nonzero if user requested finishing out of an inlined subroutine.  */

int finishing_inlined_subroutine;
/* APPLE LOCAL end subroutine inlining  */

/* Range to single step within.
   If this is nonzero, respond to a single-step signal
   by continuing to step if the pc is in this range.  */

CORE_ADDR step_range_start;	/* Inclusive */
CORE_ADDR step_range_end;	/* Exclusive */

/* APPLE LOCAL begin step ranges  */
/* Same as step_range_start and step_range_end, except for use with
   functions (inlined) that have multiple non-contiguous ranges of
   addresses.  */

struct address_range_list *stepping_ranges;
/* APPLE LOCAL end step ranges  */

/* Stack frame address as of when stepping command was issued.
   This is how we know when we step into a subroutine call,
   and how to set the frame for the breakpoint used to step out.  */

struct frame_id step_frame_id;

enum step_over_calls_kind step_over_calls;

/* If stepping, nonzero means step count is > 1
   so don't print frame next time inferior stops
   if it stops due to stepping.  */

int step_multi;

/* Environment to use for running inferior,
   in format described in environ.h.  */

struct gdb_environ *inferior_environ;

/* Accessor routines. */

void 
set_inferior_io_terminal (const char *terminal_name)
{
  if (inferior_io_terminal)
    xfree (inferior_io_terminal);

  if (!terminal_name)
    inferior_io_terminal = NULL;
  else
    inferior_io_terminal = savestring (terminal_name, strlen (terminal_name));
}

const char *
get_inferior_io_terminal (void)
{
  return inferior_io_terminal;
}

char *
get_inferior_args (void)
{
  if (inferior_argc != 0)
    {
      char *n, *old;

      n = gdbarch_construct_inferior_arguments (current_gdbarch,
						inferior_argc, inferior_argv);
      old = set_inferior_args (n);
      xfree (old);
    }

  if (inferior_args == NULL)
    inferior_args = xstrdup ("");

  return inferior_args;
}

char *
set_inferior_args (char *newargs)
{
  char *saved_args = inferior_args;

  inferior_args = newargs;
  inferior_argc = 0;
  inferior_argv = 0;

  return saved_args;
}

void
set_inferior_args_vector (int argc, char **argv)
{
  inferior_argc = argc;
  inferior_argv = argv;
}

/* Notice when `set args' is run.  */
static void
notice_args_set (char *args, int from_tty, struct cmd_list_element *c)
{
  inferior_argc = 0;
  inferior_argv = 0;
}

/* Notice when `show args' is run.  */
static void
notice_args_read (struct ui_file *file, int from_tty,
		  struct cmd_list_element *c, const char *value)
{
  deprecated_show_value_hack (file, from_tty, c, value);
  /* Might compute the value.  */
  get_inferior_args ();
}


/* Compute command-line string given argument vector.  This does the
   same shell processing as fork_inferior.  */
char *
construct_inferior_arguments (struct gdbarch *gdbarch, int argc, char **argv)
{
  char *result;

  /* APPLE LOCAL start with shell */
  if (start_with_shell_flag)
    {
      /* This holds all the characters considered special to the
	 typical Unix shells.  We include `^' because the SunOS
	 /bin/sh treats it as a synonym for `|'.  */
      char *special = "\"!#$&*()\\|[]{}<>?'\"`~^; \t\n";
      int i;
      int length = 0;
      char *out, *cp;

      /* We over-compute the size.  It shouldn't matter.  */
      for (i = 0; i < argc; ++i)
	length += 2 * strlen (argv[i]) + 1 + 2 * (argv[i][0] == '\0');

      result = (char *) xmalloc (length);
      out = result;

      for (i = 0; i < argc; ++i)
	{
	  if (i > 0)
	    *out++ = ' ';

	  /* Need to handle empty arguments specially.  */
	  if (argv[i][0] == '\0')
	    {
	      *out++ = '\'';
	      *out++ = '\'';
	    }
	  else
	    {
	      for (cp = argv[i]; *cp; ++cp)
		{
		  if (strchr (special, *cp) != NULL)
		    *out++ = '\\';
		  *out++ = *cp;
		}
	    }
	}
      *out = '\0';
    }
  else
    {
      /* In this case we can't handle arguments that contain spaces,
	 tabs, or newlines -- see breakup_args().  */
      int i;
      int length = 0;

      for (i = 0; i < argc; ++i)
	{
	  char *cp = strchr (argv[i], ' ');
	  if (cp == NULL)
	    cp = strchr (argv[i], '\t');
	  if (cp == NULL)
	    cp = strchr (argv[i], '\n');
	  if (cp != NULL)
	    error (_("can't handle command-line argument containing whitespace"));
	  length += strlen (argv[i]) + 1;
	}

      result = (char *) xmalloc (length);
      result[0] = '\0';
      for (i = 0; i < argc; ++i)
	{
	  if (i > 0)
	    strcat (result, " ");
	  strcat (result, argv[i]);
	}
    }

  return result;
}


/* This function detects whether or not a '&' character (indicating
   background execution) has been added as *the last* of the arguments ARGS
   of a command. If it has, it removes it and returns 1. Otherwise it
   does nothing and returns 0. */
/* APPLE LOCAL make globally visible */
int
strip_bg_char (char **args)
{
  char *p = NULL;

  p = strchr (*args, '&');

  if (p)
    {
      if (p == (*args + strlen (*args) - 1))
	{
	  if (strlen (*args) > 1)
	    {
	      do
		p--;
	      while (*p == ' ' || *p == '\t');
	      *(p + 1) = '\0';
	    }
	  else
	    *args = 0;
	  return 1;
	}
    }
  return 0;
}

void
tty_command (char *file, int from_tty)
{
  if (file == 0)
    error_no_arg (_("terminal name for running target process"));

  set_inferior_io_terminal (file);
}

/* Kill the inferior if already running.  This function is designed
   to be called when we are about to start the execution of the program
   from the beginning.  Ask the user to confirm that he wants to restart
   the program being debugged when FROM_TTY is non-null.  */

void
kill_if_already_running (int from_tty)
{
  if (! ptid_equal (inferior_ptid, null_ptid) && target_has_execution)
    {
      if (from_tty
	  && !query ("The program being debugged has been started already.\n\
Start it from the beginning? "))
	error (_("Program not restarted."));
      target_kill ();
#if defined(SOLIB_RESTART)
      SOLIB_RESTART ();
#endif
      /* APPLE LOCAL checkpoints */
      clear_all_checkpoints ();

      init_wait_for_inferior ();
    }
}

/* Implement the "run" command. If TBREAK_AT_MAIN is set, then insert
   a temporary breakpoint at the begining of the main program before
   running the program.  */

static void
run_command_1 (char *args, int from_tty, int tbreak_at_main)
{
  char *exec_file;

  dont_repeat ();

  kill_if_already_running (from_tty);
  clear_breakpoint_hit_counts ();

  /* APPLE LOCAL checkpoints */
  clear_all_checkpoints ();

  /* Purge old solib objfiles. */
  objfile_purge_solibs ();

  do_run_cleanups (NULL);

  /* APPLE LOCAL begin subroutine inlining  */
  /* On our first 'run' of the program, set up the data for keeping
     track of inlined function calls.  On subsequent runs, blank out
     the data but re-use the space.  */
  if (inlined_function_call_stack_initialized_p())
    inlined_function_reinitialize_call_stack ();
  else
    inlined_function_initialize_call_stack ();
  /* APPLE LOCAL end subroutine inlining  */

  /* The comment here used to read, "The exec file is re-read every
     time we do a generic_mourn_inferior, so we just have to worry
     about the symbol file."  The `generic_mourn_inferior' function
     gets called whenever the program exits.  However, suppose the
     program exits, and *then* the executable file changes?  We need
     to check again here.  Since reopen_exec_file doesn't do anything
     if the timestamp hasn't changed, I don't see the harm.  */
  reopen_exec_file ();
  reread_symbols ();

  /* Insert the temporary breakpoint if a location was specified.  */
  if (tbreak_at_main)
    tbreak_command (main_name (), 0);

  exec_file = (char *) get_exec_file (0);

  /* We keep symbols from add-symbol-file, on the grounds that the
     user might want to add some symbols before running the program
     (right?).  But sometimes (dynamic loading where the user manually
     introduces the new symbols with add-symbol-file), the code which
     the symbols describe does not persist between runs.  Currently
     the user has to manually nuke all symbols between runs if they
     want them to go away (PR 2207).  This is probably reasonable.  */

  if (!args)
    {
      if (target_can_async_p ())
	async_disable_stdin ();
    }
  else
    {
      int async_exec = strip_bg_char (&args);

      /* If we get a request for running in the bg but the target
         doesn't support it, error out. */
      if (async_exec && !target_can_async_p ())
	error (_("Asynchronous execution not supported on this target."));

      /* If we don't get a request of running in the bg, then we need
         to simulate synchronous (fg) execution. */
      if (!async_exec && target_can_async_p ())
	{
	  /* Simulate synchronous execution */
	  async_disable_stdin ();
	}

      /* If there were other args, beside '&', process them. */
      if (args)
	{
          char *old_args = set_inferior_args (xstrdup (args));
          xfree (old_args);
	}
    }

  if (from_tty)
    {
      ui_out_field_string (uiout, NULL, "Starting program");
      ui_out_text (uiout, ": ");
      if (exec_file)
	ui_out_field_string (uiout, "execfile", exec_file);
      ui_out_spaces (uiout, 1);
      /* We call get_inferior_args() because we might need to compute
	 the value now.  */
      ui_out_field_string (uiout, "infargs", get_inferior_args ());
      ui_out_text (uiout, "\n");
      ui_out_flush (uiout);
    }

  /* APPLE LOCAL begin */
  if (state_change_hook)
    {
      state_change_hook (STATE_INFERIOR_LOGICALLY_RUNNING);
    }
  /* APPLE LOCAL end */
  /* APPLE LOCAL begin run command hook */
  if (run_command_hook)
    {
      if (run_command_hook() == 0)
        return;
    }
  /* APPLE LOCAL end run command hook */

  /* We call get_inferior_args() because we might need to compute
     the value now.  */
  target_create_inferior (exec_file, get_inferior_args (),
			  environ_vector (inferior_environ), from_tty);

  /* APPLE LOCAL begin async */
  if (ptid_equal (inferior_ptid, null_ptid))
    {
      /* The target didn't manage to start for some reason.  Just in case
	 we didn't clean up, let's set target_executing to 0 here. */
      target_executing = 0;
      
      if (target_can_async_p ())
        {
          /* Simulate synchronous execution */
          async_enable_stdin (0);
        }
    }
  /* APPLE LOCAL end async */
}

static void
run_command (char *args, int from_tty)
{
  run_command_1 (args, from_tty, 0);
}

static void
run_no_args_command (char *args, int from_tty)
{
  char *old_args = set_inferior_args (xstrdup (""));
  xfree (old_args);
}


/* Start the execution of the program up until the beginning of the main
   program.  */

static void
start_command (char *args, int from_tty)
{
  /* Some languages such as Ada need to search inside the program
     minimal symbols for the location where to put the temporary
     breakpoint before starting.  */
  if (!have_minimal_symbols ())
    error (_("No symbol table loaded.  Use the \"file\" command."));

  /* Run the program until reaching the main procedure...  */
  run_command_1 (args, from_tty, 1);
} 

void
continue_command (char *proc_count_exp, int from_tty)
{
  int async_exec = 0;
  ERROR_NO_INFERIOR;

  /* Find out whether we must run in the background. */
  if (proc_count_exp != NULL)
    async_exec = strip_bg_char (&proc_count_exp);

  /* If we must run in the background, but the target can't do it,
     error out. */
  if (async_exec && !target_can_async_p ())
    error (_("Asynchronous execution not supported on this target."));

  /* If we are not asked to run in the bg, then prepare to run in the
     foreground, synchronously. */
  if (!async_exec && target_can_async_p ())
    {
      /* Simulate synchronous execution */
      async_disable_stdin ();
    }

  /* If have argument (besides '&'), set proceed count of breakpoint
     we stopped at.  */
  if (proc_count_exp != NULL)
    {
      bpstat bs = stop_bpstat;
      int num = bpstat_num (&bs);
      /* APPLE LOCAL: Used to be "num == 0", but bpstat_num returns -1
	 if the breakpoint_at for the bpstat is NULL.  We should
	 ignore that as well.  */

      if (num <= 0 && from_tty)
	{
	  printf_filtered
	    ("Not stopped at any breakpoint; argument ignored.\n");
	}
      while (num != 0)
	{
	  set_ignore_count (num,
			    parse_and_eval_long (proc_count_exp) - 1,
			    from_tty);
	  /* set_ignore_count prints a message ending with a period.
	     So print two spaces before "Continuing.".  */
	  if (from_tty)
	    printf_filtered ("  ");
	  num = bpstat_num (&bs);
	}
    }

  if (from_tty)
    printf_filtered (_("Continuing.\n"));

  clear_proceed_status ();

  /* APPLE LOCAL begin */
  if (state_change_hook)
      state_change_hook (STATE_INFERIOR_LOGICALLY_RUNNING);
  /* APPLE LOCAL end */
      
  if (continue_command_hook) 
    continue_command_hook();

  proceed ((CORE_ADDR) -1, TARGET_SIGNAL_DEFAULT, 0);
}

/* Step until outside of current statement.  */

static void
step_command (char *count_string, int from_tty)
{
  step_1 (0, 0, count_string);
}

/* Likewise, but skip over subroutine calls as if single instructions.  */

static void
next_command (char *count_string, int from_tty)
{
  step_1 (1, 0, count_string);
}

/* Likewise, but step only one instruction.  */

void
stepi_command (char *count_string, int from_tty)
{
  step_1 (0, 1, count_string);
}

void
nexti_command (char *count_string, int from_tty)
{
  step_1 (1, 1, count_string);
}

static void
disable_longjmp_breakpoint_cleanup (void *ignore)
{
  disable_longjmp_breakpoint ();
}

/* APPLE LOCAL begin stepping through inlined subroutines  */

/* The following function calls either the original step_1 function
   (now called step_1_no_inlining), to not attempt to properly
   maneuver through inlined code on a synchronous target; or it will
   call step_1_inlining, to attempt to properly handle inlined code.
   Which function it calls depends on dwarf2_allow_inlined_stepping,
   which the user can control with 'set inlined-stepping <on/off>'.  */

static void
step_1 (int skip_subroutines, int single_inst, char *count_string)
{
  if (dwarf2_allow_inlined_stepping)
    step_1_inlining (skip_subroutines, single_inst, count_string);
  else
    step_1_no_inlining (skip_subroutines, single_inst, count_string);
}

/* This is the original step_1 routine, which will be called if the
   inlined-stepping option is turned off.  */

static void
step_1_no_inlining (int skip_subroutines, int single_inst, char *count_string)
/* APPLE LOCAL end stepping through inlined subroutines  */
{
  int count = 1;
  struct frame_info *frame;
  struct cleanup *cleanups = 0;
  int async_exec = 0;
  /* APPLE LOCAL begin subroutine inlining  */
  char *file_name = NULL;
  int line_num = 0;
  int column = 0;
  /* APPLE LOCAL end subroutine inlining  */

  ERROR_NO_INFERIOR;

  if (count_string)
    async_exec = strip_bg_char (&count_string);

  /* If we get a request for running in the bg but the target
     doesn't support it, error out. */
  if (async_exec && !target_can_async_p ())
    error (_("Asynchronous execution not supported on this target."));

  /* If we don't get a request of running in the bg, then we need
     to simulate synchronous (fg) execution. */
  /* APPLE LOCAL begin subroutine inlining  */
  if (!async_exec && target_can_async_p ()
      && !single_inst
      && (skip_subroutines 
	  || !at_inlined_call_site_p (&file_name, &line_num, &column)))
  /* APPLE LOCAL end subroutine inlining  */
    {
      /* Simulate synchronous execution */
      async_disable_stdin ();
    }

  count = count_string ? parse_and_eval_long (count_string) : 1;

  if (!single_inst || skip_subroutines)		/* leave si command alone */
    {
      enable_longjmp_breakpoint ();
      if (!target_can_async_p ())
	cleanups = make_cleanup (disable_longjmp_breakpoint_cleanup, 0 /*ignore*/);
      else
        make_exec_cleanup (disable_longjmp_breakpoint_cleanup, 0 /*ignore*/);
    }

  /* In synchronous case, all is well, just use the regular for loop. */
  if (!target_can_async_p ())
    {
      /* APPLE LOCAL: Stepping command hook here.  */
      if (stepping_command_hook)
	stepping_command_hook ();

      for (; count > 0; count--)
	{
	  clear_proceed_status ();

	  frame = get_current_frame ();
	  if (!frame)		/* Avoid coredump here.  Why tho? */
	    error (_("No current frame"));
	  step_frame_id = get_frame_id (frame);

	  if (!single_inst)
	    {
	      find_pc_line_pc_range (stop_pc, &step_range_start, &step_range_end);
	      if (step_range_end == 0)
		{
		  char *name;
		  if (find_pc_partial_function (stop_pc, &name, &step_range_start,
						&step_range_end) == 0)
		    error (_("Cannot find bounds of current function"));

		  target_terminal_ours ();
		  printf_filtered (_("\
Single stepping until exit from function %s, \n\
which has no line number information.\n"), name);
		}
	    }
	  else
	    {
	      /* Say we are stepping, but stop after one insn whatever it does.  */
	      step_range_start = step_range_end = 1;
	      if (!skip_subroutines)
		/* It is stepi.
		   Don't step over function calls, not even to functions lacking
		   line numbers.  */
		step_over_calls = STEP_OVER_NONE;
	    }

	  if (skip_subroutines)
	    step_over_calls = STEP_OVER_ALL;

	  step_multi = (count > 1);
	  proceed ((CORE_ADDR) -1, TARGET_SIGNAL_DEFAULT, 1);

	  if (!stop_step)
	    break;
	}

      if (!single_inst || skip_subroutines)
	do_cleanups (cleanups);
      return;
    }
  /* In case of asynchronous target things get complicated, do only
     one step for now, before returning control to the event loop. Let
     the continuation figure out how many other steps we need to do,
     and handle them one at the time, through step_once(). */
  else
    {
      if (target_can_async_p ())
	/* APPLE LOCAL begin step command hook */
	{
	  if (stepping_command_hook)
            stepping_command_hook ();
	  step_once (skip_subroutines, single_inst, count);
	}
	/* APPLE LOCAL end step command hook */
    }
}

/* APPLE LOCAL begin stepping through inlined subroutines  */

/* The following function is a modified version of the step_1
   function, to allow for properly maneuvering through inlined subroutines.
   The modifications in this function are being copied from
   step_once.  */

static void
step_1_inlining (int skip_subroutines, int single_inst, char *count_string)
{
  int count = 1;
  struct frame_info *frame;
  struct cleanup *cleanups = 0;
  int async_exec = 0;
  /* APPLE LOCAL begin subroutine inlining  */
  char *file_name = NULL;
  int line_num = 0;
  int column = 0;
  int stack_pos = 0;
  int do_proceed = 1;
  CORE_ADDR end_of_line = (CORE_ADDR) 0;
  CORE_ADDR inline_start_pc = (CORE_ADDR) 0;
  CORE_ADDR tmp_end = (CORE_ADDR) 0;
  /* APPLE LOCAL end subroutine inlining  */

  ERROR_NO_INFERIOR;

  if (count_string)
    async_exec = strip_bg_char (&count_string);

  /* If we get a request for running in the bg but the target
     doesn't support it, error out. */
  if (async_exec && !target_can_async_p ())
    error (_("Asynchronous execution not supported on this target."));

  /* If we don't get a request of running in the bg, then we need
     to simulate synchronous (fg) execution. */
  /* APPLE LOCAL begin subroutine inlining  */
  if (!async_exec && target_can_async_p ()
      && !single_inst
      && (skip_subroutines 
	  || !at_inlined_call_site_p (&file_name, &line_num, &column)))
  /* APPLE LOCAL end subroutine inlining  */
    {
      /* Simulate synchronous execution */
      async_disable_stdin ();
    }

  count = count_string ? parse_and_eval_long (count_string) : 1;

  if (!single_inst || skip_subroutines)		/* leave si command alone */
    {
      enable_longjmp_breakpoint ();
      if (!target_can_async_p ())
	cleanups = make_cleanup (disable_longjmp_breakpoint_cleanup, 
				 0 /*ignore*/);
      else
        make_exec_cleanup (disable_longjmp_breakpoint_cleanup, 0 /*ignore*/);
    }

  /* In synchronous case, all is well, just use the regular for loop. */
  if (!target_can_async_p ())
    {
      /* APPLE LOCAL: Stepping command hook here.  */
      if (stepping_command_hook)
	stepping_command_hook ();

      for (; count > 0; count--)
	{
	  if (!single_inst
	      && ((stack_pos = at_inlined_call_site_p (&file_name, &line_num,
						       &column))
		  || rest_of_line_contains_inlined_subroutine (&end_of_line))
	      && skip_subroutines)
	    {
	      /* We're trying to 'next' over an inlined funcion call.  If so,
		 find the pc for the end of the 'current' inlined subroutine
		 (we may be nested  several inlinings deep), and set the
		 step_range_end to that address.  */

	      struct symtab_and_line sal;
	      struct symtab_and_line *cur = NULL;

	      /* Find end PC for current inlined subroutine.  */

	      frame = get_current_frame ();

	      if (line_num != 0)
		{
		  /* The inlined subroutine is the first bit of code for the
		     line;  it might not be ALL the code for the line
		     however.  First find the sal for the inlined 
		     subroutine.  */
		  CORE_ADDR start_pc;

		  start_pc = 
		         global_inlined_call_stack.records[stack_pos].start_pc;
		  sal = find_pc_line (start_pc, 0);
		  while (sal.entry_type != INLINED_CALL_SITE_LT_ENTRY
			 || sal.line != line_num)
		    {
		      cur = sal.next;
		      if (!cur)
			break;
		      sal = *cur;
		    }

		  if (sal.entry_type == INLINED_CALL_SITE_LT_ENTRY
		      && sal.line == line_num)
		    sal.pc = sal.end;

		  /* If there is more code to the line than the inlined
		     subroutine, make the 'next' command step over the rest of
		     the line as well.  */

		  if (end_of_line != 0
		      && sal.pc != end_of_line)
		    sal.pc = end_of_line;
		  else
		    {
		      /* Check to see if there is any more to this line... */
		      struct symtab_and_line tmp_sal;

		      tmp_sal = find_pc_line (sal.end, 0);
		      if (tmp_sal.symtab == sal.symtab
			  && tmp_sal.line == sal.line
			  && tmp_sal.entry_type == NORMAL_LT_ENTRY
			  && tmp_sal.end > sal.end)
			{
			  sal.pc = tmp_sal.end;
			  sal.end = tmp_sal.end;
			}
		    }
		}
	      else
		{
		  /* There is code for this line before we get to the
		     inlined subroutine; there may be code for this line
		     after the inlined subroutine as well.  We want the
		     'next' command to step over ALL the code for the
		     line.  */

		  gdb_assert (end_of_line != 0);
		  
		  if (in_inlined_function_call_p (&tmp_end))
		    {
		      if (tmp_end && tmp_end < end_of_line)
			end_of_line = tmp_end;
		    }

		  sal = find_pc_line (stop_pc, 0);
		  sal.pc = end_of_line;
		  sal.end = end_of_line;
		}

	      /* Set the global values for the 'next'. */

	      clear_proceed_status ();

	      /* Tell the user what's going on.  */

	      if (dwarf2_debug_inlined_stepping)
		ui_out_text (uiout, 
			     "** Stepping over inlined function code.  **\n");
	      
	      /* Set up various necessary variables to make sure we actually
		 stop at the right time.  */

	      if (!frame)
		error (_("No current frame"));
	      step_frame_id = get_frame_id (frame);
	      step_range_start = stop_pc;
	      step_range_end = sal.pc;
	      step_over_calls = STEP_OVER_ALL;
	      stepping_over_inlined_subroutine = 1;
	      if (current_inlined_subroutine_stack_size() > 0
		  && stack_pos > 0
		  && stack_pos < current_inlined_subroutine_stack_size ())
		stepping_ranges = 
		           global_inlined_call_stack.records[stack_pos].ranges;
	      else
		stepping_ranges = 0;

	      do_proceed = 1;

	    }
	  else if (!single_inst
		   && at_inlined_call_site_p (&file_name, &line_num, & column))
	    {
	      struct symtab_and_line sal;
	      struct symtab_and_line *cur = NULL;
	      int func_first_line = 0;
	      int found = 0;

	      stepping_into_inlined_subroutine = 1;

	      if (current_inlined_subroutine_stack_position ()
		  < current_inlined_subroutine_stack_size ())
		{
		  /* Set up hte SAL for printing out the current source
		     location.  */

		  step_into_current_inlined_subroutine ();

		  sal = find_pc_line 
		            (current_inlined_subroutine_call_stack_start_pc (),
			     0);
		  
		  for (cur = &sal; cur; cur = cur->next)
		    if (cur->entry_type == INLINED_SUBROUTINE_LT_ENTRY
			&& cur->pc == 
			      current_inlined_subroutine_call_stack_start_pc ()
			&& cur->end == 
			       current_inlined_subroutine_call_stack_end_pc ())
		      {
			sal.symtab = cur->symtab;
			sal.pc = cur->pc;
			sal.end = cur->end;
			sal.line = cur->line;
			sal.entry_type = cur->entry_type;
			break;
		      }

		  /* Flush the existing frames.  This is necessary
		     because we will need to create a new
		     INLINED_FRAME at level zero for the inlined
		     subroutine we are "stepping" into.  */

		  flush_cached_frames ();
		}
	      else
		{
		  /* We're stepping from the call site of our
		     innermost inlined subroutine into the innermost
		     inlined subroutine.  We don't actually run the
		     inferior, because the PC is already AT the
		     beginning of the inlined subroutine.  This is
		     purely a context change from the user's
		     perspective to make maneuvering through inlined
		     subroutines less confusion: We've forced a stop
		     at the call site, where there is not executable
		     code, before diving into the subroutine.  */
		 
		  /* Find the INLINED_SUBROUTINE line table entry for
		     the current PC, with information matching the
		     current record in the
		     global_inlined_call_stack.  */

		  sal = find_pc_line (stop_pc, 0);
		  
		  while (!found)
		    {
		      if (sal.entry_type == INLINED_SUBROUTINE_LT_ENTRY
			  && sal.pc == 
                              current_inlined_subroutine_call_stack_start_pc ()
			  && sal.end == 
			       current_inlined_subroutine_call_stack_end_pc ())
			found = 1;
		      else
			{
			  cur = sal.next;
			  if (!cur)
			    break;
			  sal = *cur;
			}
		    }
		  
		  /* Stepping into an inlined function requires
		     creating a new INLINED_FRAME, at level 0, so we
		     need to flush the current set of frames.  */

		  flush_cached_frames ();
		  
		  /* Update the global_inlined_call_stack data
		     appropriately.  */

		  step_into_current_inlined_subroutine ();
		}

	      /* Tell the user what we are doing.  */
	      if (dwarf2_debug_inlined_stepping)
		ui_out_text (uiout,
		     "** Simulating stepping into inlined subroutines.  **\n");

	      /* Tell emacs (or anything else using annotations) to update
		 the location.  */

	      if (at_inlined_call_site_p (&file_name, &line_num, &column))
		func_first_line = line_num;
	      else
		func_first_line = inlined_function_find_first_line (sal);

	      annotate_starting ();
	      annotate_frames_invalid ();
	      breakpoints_changed ();
	      annotate_frames_invalid ();
	      if (annotation_level == 0)
		print_source_lines (sal.symtab, func_first_line, 1, 0);
	      else
		identify_source_line (sal.symtab, func_first_line, 0, sal.pc);
	      annotate_frame_end ();
	      annotate_stopped ();

	      /* Make sure the mi interpreter updates the current location
		 appropriately (including fooling it into believing the
		 inferior has run, so it can properly finish its 'step'
		 command).  */

	      target_executing = 0;
	      do_proceed = 0;
	    }
	  else
	    {
	      /* "Normal" synchronous stepping case (not stepping into or
		 over an inlined subroutine.  */

	      if (!single_inst
		  && rest_of_line_contains_inlined_subroutine (&end_of_line))
		find_next_inlined_subroutine (stop_pc, &inline_start_pc, 
					      end_of_line);

	      clear_proceed_status ();

	      frame = get_current_frame ();

	      while (frame
		     && get_frame_type (frame) == INLINED_FRAME)
		frame = get_prev_frame (frame);

	      if (!frame)		/* Avoid coredump here.  Why tho? */
		error (_("No current frame"));
	      step_frame_id = get_frame_id (frame);

	      if (!single_inst)
		{
		  find_pc_line_pc_range (stop_pc, &step_range_start, 
					 &step_range_end);

		  if (inline_start_pc)
		    {
		      step_range_end = inline_start_pc;
		      stepping_into_inlined_subroutine = 1;
		      if (dwarf2_debug_inlined_stepping)
			ui_out_text (uiout,
		      "** Stepping to beginning of inlined subroutine.  **\n");
		    }
		  
		  if (step_range_end == 0)
		    {
		      char *name;
		      if (find_pc_partial_function (stop_pc, &name, 
						    &step_range_start,
						&step_range_end) == 0)
			error (_("Cannot find bounds of current function"));

		      target_terminal_ours ();
		      printf_filtered (_("\
Single stepping until exit from function %s, \n\
which has no line number information.\n"), name);
		    }
		}
	      else
		{
		  /* Say we are stepping, but stop after one insn
		     whatever it does.  */
		  step_range_start = step_range_end = 1;
		  if (!skip_subroutines)
		    /* It is stepi. Don't step over function calls,
		       not even to functions lacking line numbers.  */
		    step_over_calls = STEP_OVER_NONE;
		}
	      
	      if (skip_subroutines)
		step_over_calls = STEP_OVER_ALL;

	      step_multi = (count > 1);
	    }

	  if (do_proceed)
	    proceed ((CORE_ADDR) -1, TARGET_SIGNAL_DEFAULT, 1);

	  if (!stop_step)
	    break;
	}

      if (!single_inst || skip_subroutines)
	do_cleanups (cleanups);
      return;
    }
  /* In case of asynchronous target things get complicated, do only
     one step for now, before returning control to the event loop. Let
     the continuation figure out how many other steps we need to do,
     and handle them one at the time, through step_once(). */
  else
    {
      if (target_can_async_p ())
	/* APPLE LOCAL begin step command hook */
	{
	  if (stepping_command_hook)
            stepping_command_hook ();
	  step_once (skip_subroutines, single_inst, count);
	}
	/* APPLE LOCAL end step command hook */
    }
}
/* APPLE LOCAL end stepping through inlined subroutines  */

/* Called after we are done with one step operation, to check whether
   we need to step again, before we print the prompt and return control
   to the user. If count is > 1, we will need to do one more call to
   proceed(), via step_once(). Basically it is like step_once and
   step_1_continuation are co-recursive. */
static void
step_1_continuation (struct continuation_arg *arg)
{
  int count;
  int skip_subroutines;
  int single_inst;

  skip_subroutines = arg->data.integer;
  single_inst      = arg->next->data.integer;
  count            = arg->next->next->data.integer;

  if (stop_step)
    step_once (skip_subroutines, single_inst, count - 1);
  else
    if (!single_inst || skip_subroutines)
      do_exec_cleanups (ALL_CLEANUPS);
}

/* Do just one step operation. If count >1 we will have to set up a
   continuation to be done after the target stops (after this one
   step). This is useful to implement the 'step n' kind of commands, in
   case of asynchronous targets. We had to split step_1 into two parts,
   one to be done before proceed() and one afterwards. This function is
   called in case of step n with n>1, after the first step operation has
   been completed.*/
/* APPLE LOCAL make step_once globally visible */
void 
step_once (int skip_subroutines, int single_inst, int count)
{ 
  struct continuation_arg *arg1; 
  struct continuation_arg *arg2;
  struct continuation_arg *arg3; 
  struct frame_info *frame;
  /* APPLE LOCAL begin subroutine inlining  */
  char *file_name = NULL;
  int line_num = 0;
  int column = 0;
  int stack_pos;
  CORE_ADDR end_of_line = (CORE_ADDR) 0;
  CORE_ADDR inline_start_pc = (CORE_ADDR) 0;
  CORE_ADDR tmp_end = (CORE_ADDR) 0;

  if (!single_inst
      && (stack_pos = at_inlined_call_site_p (&file_name, &line_num, &column)
	  || rest_of_line_contains_inlined_subroutine (&end_of_line)) 
      && skip_subroutines)
    {
      /* We're trying to 'next' over an inlined function call.  If so,
	 find the pc for the end of the 'current' inlined subroutine
	 (we may be nested several inlinings deep), and set the
	 step-resume breakpoint there.  */

      struct symtab_and_line sal;
      struct symtab_and_line *cur = NULL;

      /* Find end PC for current inlined subroutine.  */

      frame = get_current_frame ();

      if (line_num != 0)
	{
	  /* The inlined subroutine is the first bit of code for the line;
	     it might not be ALL the code for the line, however.  First find
	     the sal for the inlined subroutine.  */
	  CORE_ADDR start_pc;

	  start_pc = global_inlined_call_stack.records[stack_pos].start_pc;

	  sal = find_pc_line (start_pc, 0);
	  while (sal.entry_type != INLINED_CALL_SITE_LT_ENTRY
		 ||  sal.line != line_num)
	    {
	      cur = sal.next;
	      if (!cur)
		break;
	      sal = *cur;
	    }

	  if (sal.entry_type == INLINED_CALL_SITE_LT_ENTRY
	      && sal.line == line_num)
	    sal.pc = sal.end;
	  
	  /* If there is more code to the line than the inlined
	     subroutine, make the 'next' command step over the rest of
	     the line as well.  */
	  
	  if (end_of_line != 0
	      && sal.pc != end_of_line)
	    sal.pc = end_of_line;
	  else
	    {
	      /* Check to see if there is any more to this line... */
	      struct symtab_and_line tmp_sal;

	      tmp_sal = find_pc_line (sal.end, 0);
	      if (tmp_sal.symtab == sal.symtab
		  && tmp_sal.line == sal.line
		  && tmp_sal.entry_type == NORMAL_LT_ENTRY
		  && tmp_sal.end > sal.end)
		{
		  sal.pc = tmp_sal.end;
		  sal.end = tmp_sal.end;
		}
	    }

	  
	}
      else
	{
	  /*  There is code for this line before we get to the inlined
	      subroutine; there may be code for this line after the
	      inlined subroutine as well.  We want the 'next' command
	      to step over ALL the code for the line.  */
	  
	  gdb_assert (end_of_line != 0);
	 
	  if (in_inlined_function_call_p (&tmp_end))
	    {
	      if (tmp_end && tmp_end < end_of_line)
		end_of_line = tmp_end;
	    }
 
	  sal = find_pc_line (stop_pc, 0);
	  sal.pc = end_of_line;
	  sal.end = end_of_line;
	}

      /* Set the breakpoint for the 'next'.  */

      clear_proceed_status ();

      /* Tell the user what's going on.  */

      if (dwarf2_debug_inlined_stepping)
	ui_out_text  (uiout, "** Stepping over inlined function code. **\n");
	  
      /* Set up various necessary variables to make sure we actually stop when
	 we get to the breakpoint.  */

      if (!frame)		/* Avoid coredump here.  Why tho? */
	error (_("No current frame"));
      step_frame_id = get_frame_id (frame);
      step_range_start = stop_pc;
      step_range_end = sal.pc;
      step_over_calls = STEP_OVER_ALL;
      stepping_over_inlined_subroutine = 1;
      if (current_inlined_subroutine_stack_size() > 0
	  && stack_pos > 0
	  && stack_pos < current_inlined_subroutine_stack_size ())
	stepping_ranges = global_inlined_call_stack.records[stack_pos].ranges;
      else
	stepping_ranges = 0;

      proceed ((CORE_ADDR) -1, TARGET_SIGNAL_DEFAULT, 1);
    }
  else if (!single_inst 
	   && at_inlined_call_site_p (&file_name, &line_num, &column))
    {
      struct symtab_and_line sal;
      struct symtab_and_line *cur = NULL;
      int func_first_line = 0;
      int found = 0;

      stepping_into_inlined_subroutine = 1;

      if (current_inlined_subroutine_stack_position ()
	   < current_inlined_subroutine_stack_size ())
	{
	  /* Set up the SAL for printing out the current source location.  */

	  step_into_current_inlined_subroutine ();

	  sal = find_pc_line (current_inlined_subroutine_call_stack_start_pc (),
			      0);

	  for (cur = &sal; cur; cur = cur->next)
	    if (cur->entry_type == INLINED_SUBROUTINE_LT_ENTRY
		&& cur->pc == current_inlined_subroutine_call_stack_start_pc ()
		&& cur->end == current_inlined_subroutine_call_stack_end_pc ())
	      {
		sal.symtab = cur->symtab;
		sal.pc = cur->pc;
		sal.end = cur->end;
		sal.line = cur->line;
		sal.entry_type = cur->entry_type;
		break;
	      }

	  /* Flush the existing frames.  This is necessary because we will need
	     to create a new INLINED_FRAME at level zero for the inlined 
	     subroutine we are "stepping" into.  */
	  
	  flush_cached_frames ();

	}
      else
	{
	  /* We're stepping from the call site of our innermost
	     inlined subroutine into the innermost inlined subroutine.
	     We don't actually run the inferior, because the PC is
	     already AT the beginning of the inlined subroutine.  This
	     is purely a context change from the user's perspective to
	     make maneuvering through inlined subroutines less
	     confusing: We've forced a stop at the call site, where
	     there is no executable code, before diving into the
	     subroutine.  */
	  
	  
	  /* Find the INLINED_SUBROUTINE line table entry for the current PC,
	     with information matching the current record in the 
	     global_inlined_call_stack.  */
	  
	  sal = find_pc_line (stop_pc, 0);
	  
	  while (!found)
	    {
	      if (sal.entry_type == INLINED_SUBROUTINE_LT_ENTRY
		  && sal.pc == current_inlined_subroutine_call_stack_start_pc ()
		  && sal.end == current_inlined_subroutine_call_stack_end_pc ())
		found = 1;
	      else
		{
		  cur = sal.next;
		  if (!cur)
		    break;
		  sal = *cur;
		}
	    }
	  
	  /* Stepping into an inlined function requires creating a new
	     INLINED_FRAME, at level 0, so we need to flush the current
	     set of frames.  */
	  
	  flush_cached_frames ();

	  /* Update the global_inlined_call_stack data appropriately.  */
	  
	  step_into_current_inlined_subroutine ();
	  
	}

      /* Tell the user what we are doing.  */
      if (dwarf2_debug_inlined_stepping)
	ui_out_text (uiout, 
		     "** Simulating stepping into inlined subroutine.  **\n");
	  
      /* Tell emacs (or anything else using annotations) to update the 
	 location.  */

      if (at_inlined_call_site_p (&file_name, &line_num, &column))
	  func_first_line = line_num;
      else
	func_first_line = inlined_function_find_first_line (sal);

      annotate_starting ();
      annotate_frames_invalid ();
      breakpoints_changed ();
      annotate_frames_invalid ();
      if (annotation_level == 0)
	print_source_lines (sal.symtab, func_first_line, 1, 0);
      else
	identify_source_line (sal.symtab, func_first_line, 0, sal.pc);
      annotate_frame_end ();
      annotate_stopped ();
	  
      /* Make sure the mi interpreter updates the current location
	 appropriately (including fooling it into believing the
	 inferior has run, so it can properly finish its 'step'
	 command).  */
	  
      target_executing = 0;
    }
  else
    {
  /* APPLE LOCAL end subroutine inlining  */
      if (count > 0)
	{
	  /* APPLE LOCAL begin subroutine inlining  */
	  if (!single_inst
	      && rest_of_line_contains_inlined_subroutine (&end_of_line))
	    find_next_inlined_subroutine (stop_pc, &inline_start_pc, 
					  end_of_line);
	  /* APPLE LOCAL end subroutine inlining  */
	      
	  clear_proceed_status ();

	  frame = get_current_frame ();

	  while (frame 
		 && get_frame_type (frame) == INLINED_FRAME)
	    frame = get_prev_frame (frame);

	  if (!frame)		/* Avoid coredump here.  Why tho? */
	    error (_("No current frame"));
	  step_frame_id = get_frame_id (frame);

	  if (!single_inst)
	    {
	      find_pc_line_pc_range (stop_pc, &step_range_start, &step_range_end);

	      /* APPLE LOCAL begin subroutine inlining  */
	      if (inline_start_pc)
		{
		  step_range_end = inline_start_pc;
		  stepping_into_inlined_subroutine = 1;
		  if (dwarf2_debug_inlined_stepping)
		    ui_out_text (uiout, 
		        "** Stepping to beginning of inlined subroutine.  **\n");
		}
	      /* APPLE LOCAL end subroutine inlining  */


	      /* If we have no line info, switch to stepi mode.  */
	      if (step_range_end == 0 && step_stop_if_no_debug)
		{
		  step_range_start = step_range_end = 1;
		}
	      else if (step_range_end == 0)
		{
		  char *name;
		  if (find_pc_partial_function (stop_pc, &name, &step_range_start,
						&step_range_end) == 0)
		    error (_("Cannot find bounds of current function"));

		  target_terminal_ours ();
		  printf_filtered (_("\
Single stepping until exit from function %s, \n\
which has no line number information.\n"), name);
		}
	    }
	  else
	    {
	      /* Say we are stepping, but stop after one insn whatever it does. */
	      step_range_start = step_range_end = 1;
	      if (!skip_subroutines)
		/* It is stepi.
		   Don't step over function calls, not even to functions lacking
		   line numbers.  */
		step_over_calls = STEP_OVER_NONE;
	    }

	  if (skip_subroutines)
	    step_over_calls = STEP_OVER_ALL;

	  step_multi = (count > 1);
	  arg1 =
	   (struct continuation_arg *) xmalloc (sizeof (struct continuation_arg));
	  arg2 =
	   (struct continuation_arg *) xmalloc (sizeof (struct continuation_arg));
	  arg3 =
	   (struct continuation_arg *) xmalloc (sizeof (struct continuation_arg));
	  arg1->next = arg2;
	  arg1->data.integer = skip_subroutines;
	  arg2->next = arg3;
	  arg2->data.integer = single_inst;
	  arg3->next = NULL;
	  arg3->data.integer = count;
	  add_intermediate_continuation (step_1_continuation, arg1);
	  proceed ((CORE_ADDR) -1, TARGET_SIGNAL_DEFAULT, 1);
	}
    /* APPLE LOCAL begin subroutine inlining  */  
    }
    /* APPLE LOCAL end subroutine inlining  */
}

/* APPLE LOCAL begin checkpoints */
/* Experimental for re-execution from a checkpoint, used to reach
   points in a program between checkpoints.  */

struct checkpoint *rx_cp;
struct checkpoint *active_checkpoint;
int magic_flag = 0;

void
re_execute_command (char *args, int from_tty)
{
  int cpn = (args ? parse_and_eval_long (args) : 1);

  rx_cp = find_checkpoint (cpn);

  if (rx_cp == NULL)
    {
      printf ("Checkpoint not found\n");
      return;
    }

  printf ("Re-executing to ");
  print_checkpoint_info (rx_cp);

  re_exec_1 ();
}

void
re_exec_1 ()
{
  int count = 1;
  struct frame_info *frame;
  int async_exec = 0;

  magic_flag = 0;

  ERROR_NO_INFERIOR;

  /* If we get a request for running in the bg but the target
     doesn't support it, error out. */
  if (async_exec && !target_can_async_p ())
    error (_("Asynchronous execution not supported on this target."));

  /* If we don't get a request of running in the bg, then we need
     to simulate synchronous (fg) execution. */
  if (!async_exec && target_can_async_p ())
    {
      /* Simulate synchronous execution */
      async_disable_stdin ();
    }

  count = 100;

  /* In synchronous case, all is well, just use the regular for loop. */
  if (!target_can_async_p ())
    {
      for (; count > 0; count--)
	{
	  clear_proceed_status ();

	  frame = get_current_frame ();
	  if (!frame)		/* Avoid coredump here.  Why tho? */
	    error (_("No current frame"));
	  step_frame_id = get_frame_id (frame);

	  /* Say we are stepping, but stop after one insn whatever it does.  */
	  step_range_start = step_range_end = 1;
	  step_over_calls = STEP_OVER_NONE;

	  step_multi = (count > 1);
	  proceed ((CORE_ADDR) -1, TARGET_SIGNAL_DEFAULT, 1);

	  if (!stop_step)
	    break;
	}

      return;
    }
  /* In case of asynchronous target things get complicated, do only
     one step for now, before returning control to the event loop. Let
     the continuation figure out how many other steps we need to do,
     and handle them one at the time, through step_once(). */
  else
    {
      if (target_can_async_p ())
	/* APPLE LOCAL begin step command hook */
	{
	  if (stepping_command_hook)
            stepping_command_hook ();
	  re_exec_once (count);
	}
	/* APPLE LOCAL end step command hook */
    }
}

void
re_exec_1_continuation (struct continuation_arg *arg)
{
  int count;

  count            = arg->data.integer;

  if (stop_step)
    re_exec_once (count - 1);
}

void 
re_exec_once (int count)
{ 
  struct continuation_arg *arg1; 
  struct frame_info *frame;

  if (count <= 0)
    {
      printf ("Re-execution failed\n");
      rx_cp = NULL;
      return;
    }

  if (magic_flag)
    {
      magic_flag = 0;
      rx_cp = NULL;
      return;
    }


  clear_proceed_status ();

  frame = get_current_frame ();
  if (!frame)		/* Avoid coredump here.  Why tho? */
    error (_("No current frame"));
  step_frame_id = get_frame_id (frame);

  /* Say we are stepping, but stop after one insn whatever it does.  */
  step_range_start = step_range_end = 1;
  step_over_calls = STEP_OVER_NONE;

  step_multi = (count > 1);

  printf("Comparing against ");
  print_checkpoint_info (active_checkpoint);
  if (checkpoint_compare (rx_cp, active_checkpoint))
    {
      step_multi = 0;
      magic_flag = 1;
      rx_cp = NULL;
    }

  arg1 = (struct continuation_arg *) xmalloc (sizeof (struct continuation_arg));
  arg1->next = NULL;
  arg1->data.integer = count;
  add_intermediate_continuation (re_exec_1_continuation, arg1);

  proceed ((CORE_ADDR) -1, TARGET_SIGNAL_DEFAULT, 1);
}
/* APPLE LOCAL end checkpoints */


/* Continue program at specified address.  */

static void
jump_command (char *arg, int from_tty)
{
  CORE_ADDR addr;
  struct symtabs_and_lines sals;
  struct symtab_and_line sal;
  struct symbol *fn;
  struct symbol *sfn;
  int async_exec = 0;

  ERROR_NO_INFERIOR;

  /* Find out whether we must run in the background. */
  if (arg != NULL)
    async_exec = strip_bg_char (&arg);

  /* If we must run in the background, but the target can't do it,
     error out. */
  if (async_exec && !target_can_async_p ())
    error (_("Asynchronous execution not supported on this target."));

  /* APPLE LOCAL move async bit down */

  if (!arg)
    error_no_arg (_("starting address"));

  sals = decode_line_spec_1 (arg, 1);
  if (sals.nelts != 1)
    {
      error (_("Unreasonable jump request"));
    }

  sal = sals.sals[0];
  xfree (sals.sals);

  if (sal.symtab == 0 && sal.pc == 0)
    error (_("No source file has been specified."));

  resolve_sal_pc (&sal);	/* May error out */

  /* See if we are trying to jump to another function. */
  fn = get_frame_function (get_current_frame ());
  sfn = find_pc_function (sal.pc);
  if (fn != NULL && sfn != fn)
    {
      if (!query ("Line %d is not in `%s'.  Jump anyway? ", sal.line,
		  SYMBOL_PRINT_NAME (fn)))
	{
	  error (_("Not confirmed."));
	  /* NOTREACHED */
	}
    }

  if (sfn != NULL)
    {
      fixup_symbol_section (sfn, 0);
      if (section_is_overlay (SYMBOL_BFD_SECTION (sfn)) &&
	  !section_is_mapped (SYMBOL_BFD_SECTION (sfn)))
	{
	  if (!query ("WARNING!!!  Destination is in unmapped overlay!  Jump anyway? "))
	    {
	      error (_("Not confirmed."));
	      /* NOTREACHED */
	    }
	}
    }

  addr = sal.pc;

  if (from_tty)
    {
      printf_filtered (_("Continuing at "));
      deprecated_print_address_numeric (addr, 1, gdb_stdout);
      printf_filtered (".\n");
    }

  /* APPLE LOCAL end async */
  /* If we are not asked to run in the bg, then prepare to run in the
     foreground, synchronously. */
  if (!async_exec && target_can_async_p ())
    {
      /* Simulate synchronous execution */
      async_disable_stdin ();
    }
  /* APPLE LOCAL end async */

  clear_proceed_status ();
  proceed (addr, TARGET_SIGNAL_0, 0);
}


/* Go to line or address in current procedure */
static void
go_command (char *line_no, int from_tty)
{
  if (line_no == (char *) NULL || !*line_no)
    printf_filtered (GO_USAGE);
  else
    {
      tbreak_command (line_no, from_tty);
      jump_command (line_no, from_tty);
    }
}


/* Continue program giving it specified signal.  */

static void
signal_command (char *signum_exp, int from_tty)
{
  enum target_signal oursig;

  dont_repeat ();		/* Too dangerous.  */
  ERROR_NO_INFERIOR;

  if (!signum_exp)
    error_no_arg (_("signal number"));

  /* It would be even slicker to make signal names be valid expressions,
     (the type could be "enum $signal" or some such), then the user could
     assign them to convenience variables.  */
  oursig = target_signal_from_name (signum_exp);

  if (oursig == TARGET_SIGNAL_UNKNOWN)
    {
      /* No, try numeric.  */
      int num = parse_and_eval_long (signum_exp);

      if (num == 0)
	oursig = TARGET_SIGNAL_0;
      else
	oursig = target_signal_from_command (num);
    }

  if (from_tty)
    {
      if (oursig == TARGET_SIGNAL_0)
	printf_filtered (_("Continuing with no signal.\n"));
      else
	printf_filtered (_("Continuing with signal %s.\n"),
			 target_signal_to_name (oursig));
    }

  clear_proceed_status ();

  /* APPLE LOCAL begin */
  if (state_change_hook)
    {
      state_change_hook (STATE_INFERIOR_LOGICALLY_RUNNING);
    }
  /* APPLE LOCAL end */

  /* "signal 0" should not get stuck if we are stopped at a breakpoint.
     FIXME: Neither should "signal foo" but when I tried passing
     (CORE_ADDR)-1 unconditionally I got a testsuite failure which I haven't
     tried to track down yet.  */
  proceed (oursig == TARGET_SIGNAL_0 ? (CORE_ADDR) -1 : stop_pc, oursig, 0);
}

/* Proceed until we reach a different source line with pc greater than
   our current one or exit the function.  We skip calls in both cases.

   Note that eventually this command should probably be changed so
   that only source lines are printed out when we hit the breakpoint
   we set.  This may involve changes to wait_for_inferior and the
   proceed status code.  */

static void
until_next_command (int from_tty)
{
  struct frame_info *frame;
  CORE_ADDR pc;
  struct symbol *func;
  struct symtab_and_line sal;

  clear_proceed_status ();

  frame = get_current_frame ();

  /* Step until either exited from this function or greater
     than the current line (if in symbolic section) or pc (if
     not). */

  pc = read_pc ();
  func = find_pc_function (pc);

  if (!func)
    {
      struct minimal_symbol *msymbol = lookup_minimal_symbol_by_pc (pc);

      if (msymbol == NULL)
	error (_("Execution is not within a known function."));

      step_range_start = SYMBOL_VALUE_ADDRESS (msymbol);
      step_range_end = pc;
    }
  else
    {
      sal = find_pc_line (pc, 0);

      /* APPLE LOCAL begin address range; FIXME: just changing the following line
	 from BLOCK_START to BLOCK_LOWEST_PC may not be enough to make
	 sure this function really does the right thing for functions with
	 non-contiguous ranges of addresses. -- ctice  */
      step_range_start = BLOCK_LOWEST_PC (SYMBOL_BLOCK_VALUE (func));
      /* APPLE LOCAL end address range  */
      step_range_end = sal.end;
    }

  step_over_calls = STEP_OVER_ALL;
  step_frame_id = get_frame_id (frame);

  step_multi = 0;		/* Only one call to proceed */

  proceed ((CORE_ADDR) -1, TARGET_SIGNAL_DEFAULT, 1);
}

static void
until_command (char *arg, int from_tty)
{
  int async_exec = 0;

  if (!target_has_execution)
    error (_("The program is not running."));

  /* Find out whether we must run in the background. */
  if (arg != NULL)
    async_exec = strip_bg_char (&arg);

  /* If we must run in the background, but the target can't do it,
     error out. */
  if (async_exec && !target_can_async_p ())
    error (_("Asynchronous execution not supported on this target."));

  /* If we are not asked to run in the bg, then prepare to run in the
     foreground, synchronously. */
  if (!async_exec && target_can_async_p ())
    {
      /* Simulate synchronous execution */
      async_disable_stdin ();
    }

  if (arg)
    until_break_command (arg, from_tty, 0);
  else
    until_next_command (from_tty);
}

static void
advance_command (char *arg, int from_tty)
{
  int async_exec = 0;

  if (!target_has_execution)
    error (_("The program is not running."));

  if (arg == NULL)
    error_no_arg (_("a location"));

  /* Find out whether we must run in the background.  */
  if (arg != NULL)
    async_exec = strip_bg_char (&arg);

  /* If we must run in the background, but the target can't do it,
     error out.  */
  if (async_exec && !target_can_async_p ())
    error (_("Asynchronous execution not supported on this target."));

  /* If we are not asked to run in the bg, then prepare to run in the
     foreground, synchronously.  */
  if (!async_exec && target_can_async_p ())
    {
      /* Simulate synchronous execution.  */
      async_disable_stdin ();
    }

  until_break_command (arg, from_tty, 1);
}

/* Print the result of a function at the end of a 'finish' command.  */

static void
print_return_value (int struct_return, struct type *value_type)
{
  struct gdbarch *gdbarch = current_gdbarch;
  struct cleanup *old_chain;
  struct ui_stream *stb;
  struct value *value;

  gdb_assert (TYPE_CODE (value_type) != TYPE_CODE_VOID);

  /* FIXME: 2003-09-27: When returning from a nested inferior function
     call, it's possible (with no help from the architecture vector)
     to locate and return/print a "struct return" value.  This is just
     a more complicated case of what is already being done in in the
     inferior function call code.  In fact, when inferior function
     calls are made async, this will likely be made the norm.  */

  switch (gdbarch_return_value (gdbarch, value_type, NULL, NULL, NULL))
    {
    case RETURN_VALUE_REGISTER_CONVENTION:
    case RETURN_VALUE_ABI_RETURNS_ADDRESS:
      value = allocate_value (value_type);
      CHECK_TYPEDEF (value_type);
      gdbarch_return_value (current_gdbarch, value_type, stop_registers,
			    value_contents_raw (value), NULL);
      break;
    case RETURN_VALUE_STRUCT_CONVENTION:
      value = NULL;
      break;
    default:
      internal_error (__FILE__, __LINE__, _("bad switch"));
    }

  if (value)
    {
      /* Print it.  */
      stb = ui_out_stream_new (uiout);
      old_chain = make_cleanup_ui_out_stream_delete (stb);
      ui_out_text (uiout, "Value returned is ");
      ui_out_field_fmt (uiout, "gdb-result-var", "$%d",
			record_latest_value (value));
      ui_out_text (uiout, " = ");
      value_print (value, stb->stream, 0, Val_no_prettyprint);
      ui_out_field_stream (uiout, "return-value", stb);
      ui_out_text (uiout, "\n");
      do_cleanups (old_chain);
    }
  else
    {
      ui_out_text (uiout, "Value returned has type: ");
      ui_out_field_string (uiout, "return-type", TYPE_NAME (value_type));
      ui_out_text (uiout, ".");
      ui_out_text (uiout, " Cannot determine contents\n");
    }
}

/* Stuff that needs to be done by the finish command after the target
   has stopped.  In asynchronous mode, we wait for the target to stop
   in the call to poll or select in the event loop, so it is
   impossible to do all the stuff as part of the finish_command
   function itself.  The only chance we have to complete this command
   is in fetch_inferior_event, which is called by the event loop as
   soon as it detects that the target has stopped. This function is
   called via the cmd_continuation pointer.  */

static void
finish_command_continuation (struct continuation_arg *arg)
{
  struct symbol *function;
  struct breakpoint *breakpoint;
  struct cleanup *cleanups;

  breakpoint = (struct breakpoint *) arg->data.pointer;
  function = (struct symbol *) arg->next->data.pointer;
  cleanups = (struct cleanup *) arg->next->next->data.pointer;

  if (bpstat_find_breakpoint (stop_bpstat, breakpoint) != NULL
      && function != NULL)
    {
      struct type *value_type;
      int struct_return;
      int gcc_compiled;

      value_type = TYPE_TARGET_TYPE (SYMBOL_TYPE (function));
      if (!value_type)
	internal_error (__FILE__, __LINE__,
			_("finish_command: function has no target type"));

      if (TYPE_CODE (value_type) == TYPE_CODE_VOID)
	{
	  do_exec_cleanups (cleanups);
	  return;
	}

      CHECK_TYPEDEF (value_type);
      gcc_compiled = BLOCK_GCC_COMPILED (SYMBOL_BLOCK_VALUE (function));
      struct_return = using_struct_return (value_type, gcc_compiled);

      print_return_value (struct_return, value_type); 
    }

  do_exec_cleanups (cleanups);
}

/* APPLE LOCAL begin subroutine inlining  */

/* Stuff that needs to be done by the finish command, when performed on
   an inlined subroutine, after the target has stopped.  Very similar to
   finish_command_continuation (see that function for further comments). */

static void
finish_inlined_subroutine_command_continuation (struct continuation_arg *arg)
{
  struct cleanup *cleanups;
  struct symbol *function;

  function = (struct symbol *) arg->data.pointer;
  cleanups = (struct cleanup *) arg->next->data.pointer;

  if (function != NULL)
    {
      struct type *value_type;
      int struct_return;
      int gcc_compiled;

      value_type = TYPE_TARGET_TYPE (SYMBOL_TYPE (function));
      if (!value_type)
	internal_error (__FILE__, __LINE__,
			_("finish_inlined_subroutine_command: function has no target type"));
      
      if (TYPE_CODE (value_type) == TYPE_CODE_VOID)
	{
	  do_exec_cleanups (cleanups);
	  return;
	}

      CHECK_TYPEDEF (value_type);
      gcc_compiled = BLOCK_GCC_COMPILED (SYMBOL_BLOCK_VALUE (function));
      struct_return = using_struct_return (value_type, gcc_compiled);

      print_return_value (struct_return, value_type);
    }

  do_exec_cleanups (cleanups);
}

/* Finish out of an inlined subroutine.  This is acutally done by setting
   things up as if the user requested a 'next' at the inlined call site
   (temporary breakpoints cannot be used because, due to the way the
   compiler lays out inlined code, the requested breakpoint address is often
   never hit).  Also, call finish_inlined_suborutine_command_continuation
   to make sure return values are printed out, etc.  */

static void
finish_inlined_subroutine_command (CORE_ADDR inline_end_pc)
{
  int i;  /* Position, in inlined call stack, of subroutine to be 
	     'finish'ed  */
  char *file_name;
  int line_num;
  int column;
  struct symtab_and_line sal;
  struct symtab_and_line *cur = NULL;
  struct frame_info *frame;
  CORE_ADDR start_pc;
  struct symbol *function;
  struct cleanup *old_chain = NULL;
  struct continuation_arg *arg1;
  struct continuation_arg *arg2;

  /* Find the record corresponding to the inlined subroutine we wish
     to 'finish'.  */

  i = current_inlined_subroutine_stack_position ();
  if (i > 0
      && !global_inlined_call_stack.records[i].stepped_into)
    i--;

  /* If we aren't at an active record on the inlining stack, then we
     have gotten here by mistake.  */

  gdb_assert (i >= 1);
  gdb_assert (global_inlined_call_stack.records[i].stepped_into);

  /* Collect the call site information.  */

  file_name = global_inlined_call_stack.records[i].call_site_filename;
  line_num  = global_inlined_call_stack.records[i].call_site_line;
  column    = global_inlined_call_stack.records[i].call_site_column;
  start_pc = global_inlined_call_stack.records[i].start_pc;
  
  /* Get the correct sal.  */

  frame = get_current_frame ();
  sal = find_pc_line (start_pc, 0);
  while (sal.entry_type != INLINED_CALL_SITE_LT_ENTRY
	 || sal.line != line_num)
    {
      cur = sal.next;
      if (!cur)
	break;
      sal = *cur;
    }

  if (sal.entry_type == INLINED_CALL_SITE_LT_ENTRY
      && sal.line == line_num)
    sal.pc = sal.end;

  /* Proceed as if we are doing 'next' over the inlined subroutine, but 
     make sure to use prev_frame.  */
  
  clear_proceed_status ();
  if (!frame)
    error (_("No current frame"));
  step_frame_id = get_frame_id (get_prev_frame (frame));
  step_range_start = start_pc;
  step_range_end = sal.pc;
  step_over_calls = STEP_OVER_ALL;
  stepping_over_inlined_subroutine = 1;
  stepping_ranges = global_inlined_call_stack.records[i].ranges;
  
  /* Clena up global inlined call stack to refect no longer being in
     the subroutien.  */

  global_inlined_call_stack.records[i].stepped_into = 0;
  adjust_current_inlined_subroutine_stack_position (-1);

  frame = get_prev_frame (deprecated_selected_frame);

  function = find_pc_function (get_frame_pc (frame));

  /* Take care of printing out the return value, if any, of the function
     we are finishing.  */

  if (target_can_async_p ())
    {
      arg1 = 
	(struct continuation_arg *) xmalloc (sizeof (struct continuation_arg));
      arg2 = 
	(struct continuation_arg *) xmalloc (sizeof (struct continuation_arg));
      arg1->next = arg2;
      arg2->next = NULL;
      arg1->data.pointer = function;
      arg2->data.pointer = old_chain;
      add_continuation (finish_inlined_subroutine_command_continuation, arg1);
    }

  finishing_inlined_subroutine = 1;
  proceed_to_finish = 1;
  proceed ((CORE_ADDR) -1, TARGET_SIGNAL_DEFAULT, 1);
  
  if (!target_can_async_p ())
    {
      if (function != NULL)
	{
	  struct type *value_type;
	  int struct_return;
	  int gcc_compiled;
	  
	  value_type = TYPE_TARGET_TYPE (SYMBOL_TYPE (function));
	  if (!value_type)
	    internal_error (__FILE__, __LINE__,
	  _("finish_inlined_subroutine_command: function has no target type"));
	  
	  if (TYPE_CODE (value_type) == TYPE_CODE_VOID)
	    {
	      do_exec_cleanups (old_chain);
	      return;
	    }
	  
	  CHECK_TYPEDEF (value_type);
	  gcc_compiled = BLOCK_GCC_COMPILED (SYMBOL_BLOCK_VALUE (function));
	  struct_return = using_struct_return (value_type, gcc_compiled);
	  
	  print_return_value (struct_return, value_type);
	}
      
      do_exec_cleanups (old_chain);
    }
}
/* APPLE LOCAL end subroutine inlining  */

/* "finish": Set a temporary breakpoint at the place the selected
   frame will return to, then continue.  */

static void
finish_command (char *arg, int from_tty)
{
  struct symtab_and_line sal;
  struct frame_info *frame;
  struct frame_info *selected_frame;
  struct symbol *function;
  struct breakpoint *breakpoint;
  struct cleanup *old_chain;
  struct continuation_arg *arg1, *arg2, *arg3;
  /* APPLE LOCAL begin subroutine inlining  */
  CORE_ADDR inline_end_pc = 0;
  /* APPLE LOCAL end subroutine inlining  */

  int async_exec = 0;

  /* APPLE LOCAL begin subroutine inlining  */
  /* Check to see if we're trying to finish out of an inlined subroutine
     call.  If so, we need to use a completely different mechanism (much
     more similar to stepping over a function).  */
  if (in_inlined_function_call_p (&inline_end_pc))
    finish_inlined_subroutine_command (inline_end_pc);
  else /* Proceed with normal finish.  */
    {
      /* APPLE LOCAL end subroutine inlining  */

      /* Find out whether we must run in the background.  */
      if (arg != NULL)
	async_exec = strip_bg_char (&arg);

      /* If we must run in the background, but the target can't do it,
	 error out.  */
      if (async_exec && !target_can_async_p ())
	error (_("Asynchronous execution not supported on this target."));

      /* If we are not asked to run in the bg, then prepare to run in the
	 foreground, synchronously.  */
      if (!async_exec && target_can_async_p ())
	{
	  /* Simulate synchronous execution.  */
	  async_disable_stdin ();
	}

      if (arg)
	error (_("The \"finish\" command does not take any arguments."));
      if (!target_has_execution)
	error (_("The program is not running."));

      /* APPLE LOCAL: If there's no selected frame, default to the
	 current frame.  */
      selected_frame = get_selected_frame ("No selected frame.");

      frame = get_prev_frame (selected_frame);
      if (frame == 0)
	error (_("\"finish\" not meaningful in the outermost frame."));

      clear_proceed_status ();

      sal = find_pc_line (get_frame_pc (frame), 0);
      sal.pc = get_frame_pc (frame);

      breakpoint = set_momentary_breakpoint (sal, get_frame_id (frame), 
					     bp_finish);


      if (!target_can_async_p ())
	old_chain = make_cleanup_delete_breakpoint (breakpoint);
      else
	old_chain = make_exec_cleanup_delete_breakpoint (breakpoint);

      /* Find the function we will return from.  */
      
      function = find_pc_function (get_frame_pc (selected_frame));
      
      /* Print info on the selected frame, including level number but not
	 source.  */
      if (from_tty)
	{
	  printf_filtered (_("Run till exit from "));
	  print_stack_frame (selected_frame, 1, LOCATION);
	}
  
      /* If running asynchronously and the target support asynchronous
	 execution, set things up for the rest of the finish command to be
	 completed later on, when gdb has detected that the target has
	 stopped, in fetch_inferior_event.  */
      if (target_can_async_p ())
	{
	  arg1 =
	    (struct continuation_arg *) xmalloc (sizeof (struct continuation_arg));
	  arg2 =
	    (struct continuation_arg *) xmalloc (sizeof (struct continuation_arg));
	  arg3 =
	    (struct continuation_arg *) xmalloc (sizeof (struct continuation_arg));
	  arg1->next = arg2;
	  arg2->next = arg3;
	  arg3->next = NULL;
	  arg1->data.pointer = breakpoint;
	  arg2->data.pointer = function;
	  arg3->data.pointer = old_chain;
	  add_continuation (finish_command_continuation, arg1);
	}
      
      proceed_to_finish = 1;	/* We want stop_registers, please...  */
      proceed ((CORE_ADDR) -1, TARGET_SIGNAL_DEFAULT, 0);
      
      /* Do this only if not running asynchronously or if the target
	 cannot do async execution.  Otherwise, complete this command when
	 the target actually stops, in fetch_inferior_event.  */
      if (!target_can_async_p ())
	{
	  /* Did we stop at our breakpoint?  */
	  if (bpstat_find_breakpoint (stop_bpstat, breakpoint) != NULL
	      && function != NULL)
	    {
	      struct type *value_type;
	      int struct_return;
	      int gcc_compiled;
	      
	      value_type = TYPE_TARGET_TYPE (SYMBOL_TYPE (function));
	      if (!value_type)
		internal_error (__FILE__, __LINE__,
				_("finish_command: function has no target type"));
	      
	      /* FIXME: Shouldn't we do the cleanups before returning?  */
	      if (TYPE_CODE (value_type) == TYPE_CODE_VOID)
		return;
	      
	      CHECK_TYPEDEF (value_type);
	      gcc_compiled = BLOCK_GCC_COMPILED (SYMBOL_BLOCK_VALUE (function));
	      struct_return = using_struct_return (value_type, gcc_compiled);
	      
	      print_return_value (struct_return, value_type); 
	    }
	  
	  do_cleanups (old_chain);
	}
    /* APPLE LOCAL begin subroutine inlining  */
    }
    /* APPLE LOCAL end subroutine inlining  */
}


static void
program_info (char *args, int from_tty)
{
  bpstat bs = stop_bpstat;
  int num = bpstat_num (&bs);

  if (!target_has_execution)
    {
      printf_filtered (_("The program being debugged is not being run.\n"));
      return;
    }

  target_files_info ();
  printf_filtered (_("Program stopped at %s.\n"),
		   hex_string ((unsigned long) stop_pc));
  if (stop_step)
    printf_filtered (_("It stopped after being stepped.\n"));
  else if (num != 0)
    {
      /* There may be several breakpoints in the same place, so this
         isn't as strange as it seems.  */
      while (num != 0)
	{
	  if (num < 0)
	    {
	      printf_filtered (_("\
It stopped at a breakpoint that has since been deleted.\n"));
	    }
	  else
	    printf_filtered (_("It stopped at breakpoint %d.\n"), num);
	  num = bpstat_num (&bs);
	}
    }
  else if (stop_signal != TARGET_SIGNAL_0)
    {
      printf_filtered (_("It stopped with signal %s, %s.\n"),
		       target_signal_to_name (stop_signal),
		       target_signal_to_string (stop_signal));
    }

  if (!from_tty)
    {
      printf_filtered (_("\
Type \"info stack\" or \"info registers\" for more information.\n"));
    }
}

/* APPLE LOCAL: A command to get the inferior's process ID, useful for
   an IDE in some circumstances.  So pid_info() was added.  */
void
pid_info (char *args, int from_tty)
{
  if (!target_has_execution)
    {
      error ("The program being debugged is not being run.");
    }

  ui_out_text (uiout, "Inferior has process ID ");
  ui_out_field_int (uiout, "process-id", PIDGET (inferior_ptid));
  ui_out_text (uiout, ".\n");
  ui_out_flush (uiout);
}

static void
environment_info (char *var, int from_tty)
{
  if (var)
    {
      char *val = get_in_environ (inferior_environ, var);
      if (val)
	{
	  puts_filtered (var);
	  puts_filtered (" = ");
	  puts_filtered (val);
	  puts_filtered ("\n");
	}
      else
	{
	  puts_filtered ("Environment variable \"");
	  puts_filtered (var);
	  puts_filtered ("\" not defined.\n");
	}
    }
  else
    {
      char **vector = environ_vector (inferior_environ);
      while (*vector)
	{
	  puts_filtered (*vector++);
	  puts_filtered ("\n");
	}
    }
}

static void
set_environment_command (char *arg, int from_tty)
{
  char *p, *val, *var;
  int nullset = 0;

  if (arg == 0)
    error_no_arg (_("environment variable and value"));

  /* Find seperation between variable name and value */
  p = (char *) strchr (arg, '=');
  val = (char *) strchr (arg, ' ');

  if (p != 0 && val != 0)
    {
      /* We have both a space and an equals.  If the space is before the
         equals, walk forward over the spaces til we see a nonspace 
         (possibly the equals). */
      if (p > val)
	while (*val == ' ')
	  val++;

      /* Now if the = is after the char following the spaces,
         take the char following the spaces.  */
      if (p > val)
	p = val - 1;
    }
  else if (val != 0 && p == 0)
    p = val;

  if (p == arg)
    error_no_arg (_("environment variable to set"));

  if (p == 0 || p[1] == 0)
    {
      nullset = 1;
      if (p == 0)
	p = arg + strlen (arg);	/* So that savestring below will work */
    }
  else
    {
      /* Not setting variable value to null */
      val = p + 1;
      while (*val == ' ' || *val == '\t')
	val++;
    }

  while (p != arg && (p[-1] == ' ' || p[-1] == '\t'))
    p--;

  var = savestring (arg, p - arg);
  if (nullset)
    {
      printf_filtered (_("\
Setting environment variable \"%s\" to null value.\n"),
		       var);
      set_in_environ (inferior_environ, var, "");
    }
  else
    set_in_environ (inferior_environ, var, val);
  xfree (var);
}

static void
unset_environment_command (char *var, int from_tty)
{
  if (var == 0)
    {
      /* If there is no argument, delete all environment variables.
         Ask for confirmation if reading from the terminal.  */
      if (!from_tty || query (_("Delete all environment variables? ")))
	{
	  free_environ (inferior_environ);
	  inferior_environ = make_environ ();
	}
    }
  else
    unset_in_environ (inferior_environ, var);
}

/* Handle the execution path (PATH variable) */

static const char path_var_name[] = "PATH";

static void
path_info (char *args, int from_tty)
{
  puts_filtered ("Executable and object file path: ");
  puts_filtered (get_in_environ (inferior_environ, path_var_name));
  puts_filtered ("\n");
}

/* Add zero or more directories to the front of the execution path.  */

static void
path_command (char *dirname, int from_tty)
{
  char *exec_path;
  char *env;
  dont_repeat ();
  env = get_in_environ (inferior_environ, path_var_name);
  /* Can be null if path is not set */
  if (!env)
    env = "";
  exec_path = xstrdup (env);
  mod_path (dirname, &exec_path);
  set_in_environ (inferior_environ, path_var_name, exec_path);
  xfree (exec_path);
  if (from_tty)
    path_info ((char *) NULL, from_tty);
}


/* Print out the machine register regnum. If regnum is -1, print all
   registers (print_all == 1) or all non-float and non-vector
   registers (print_all == 0).

   For most machines, having all_registers_info() print the
   register(s) one per line is good enough.  If a different format is
   required, (eg, for MIPS or Pyramid 90x, which both have lots of
   regs), or there is an existing convention for showing all the
   registers, define the architecture method PRINT_REGISTERS_INFO to
   provide that format.  */

void
default_print_registers_info (struct gdbarch *gdbarch,
			      struct ui_file *file,
			      struct frame_info *frame,
			      int regnum, int print_all)
{
  int i;
  const int numregs = NUM_REGS + NUM_PSEUDO_REGS;
  gdb_byte buffer[MAX_REGISTER_SIZE];

  for (i = 0; i < numregs; i++)
    {
      /* Decide between printing all regs, non-float / vector regs, or
         specific reg.  */
      if (regnum == -1)
	{
	  if (print_all)
	    {
	      if (!gdbarch_register_reggroup_p (gdbarch, i, all_reggroup))
		continue;
	    }
	  else
	    {
	      if (!gdbarch_register_reggroup_p (gdbarch, i, general_reggroup))
		continue;
	    }
	}
      else
	{
	  if (i != regnum)
	    continue;
	}

      /* If the register name is empty, it is undefined for this
         processor, so don't display anything.  */
      if (REGISTER_NAME (i) == NULL || *(REGISTER_NAME (i)) == '\0')
	continue;

      fputs_filtered (REGISTER_NAME (i), file);
      print_spaces_filtered (15 - strlen (REGISTER_NAME (i)), file);

      /* Get the data in raw format.  */
      if (! frame_register_read (frame, i, buffer))
	{
	  fprintf_filtered (file, "*value not available*\n");
	  continue;
	}

      /* APPLE LOCAL begin */
      /* For floating and vector registers, print the value in natural
	 format, followed by the contents of the register in hex.  For
	 all other registers, pring the contents of the register in
	 hex, followed by the natural (integer) value. */

      if ((TYPE_CODE (register_type (current_gdbarch, i)) == TYPE_CODE_FLT)
	  || TYPE_VECTOR (register_type (current_gdbarch, i)))
	/* APPLE LOCAL end */
	{
	  int j;

	  val_print (register_type (current_gdbarch, i), buffer, 0, 0,
		     file, 0, 1, 0, Val_pretty_default);

	  fprintf_filtered (file, "\t(raw 0x");
	  for (j = 0; j < register_size (current_gdbarch, i); j++)
	    {
	      int idx;
	      if (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG)
		idx = j;
	      else
		idx = register_size (current_gdbarch, i) - 1 - j;
	      fprintf_filtered (file, "%02x", (unsigned char) buffer[idx]);
	    }
	  fprintf_filtered (file, ")");
	}
      else
	{
	  /* Print the register in hex.  */
	  val_print (register_type (current_gdbarch, i), buffer, 0, 0,
		     file, 'x', 1, 0, Val_pretty_default);
          /* If not a vector register, print it also according to its
             natural format.  */
	  if (TYPE_VECTOR (register_type (current_gdbarch, i)) == 0)
	    {
	      fprintf_filtered (file, "\t");
	      val_print (register_type (current_gdbarch, i), buffer, 0, 0,
			 file, 0, 1, 0, Val_pretty_default);
	    }
	}

      fprintf_filtered (file, "\n");
    }
}

void
registers_info (char *addr_exp, int fpregs)
{
  /* APPLE LOCAL begin eliminate unused variable warnings  */
  /* int regnum, numregs;  */
  /* char *end; */
  /* APPLE LOCAL end eliminate unused variable warnings  */

  if (!target_has_registers)
    error (_("The program has no registers now."));
  if (deprecated_selected_frame == NULL)
    error (_("No selected frame."));

  if (!addr_exp)
    {
      gdbarch_print_registers_info (current_gdbarch, gdb_stdout,
				    deprecated_selected_frame, -1, fpregs);
      return;
    }

  while (*addr_exp != '\0')
    {
      char *start;
      const char *end;

      /* Keep skipping leading white space.  */
      if (isspace ((*addr_exp)))
	{
	  addr_exp++;
	  continue;
	}

      /* Discard any leading ``$''.  Check that there is something
         resembling a register following it.  */
      if (addr_exp[0] == '$')
	addr_exp++;
      if (isspace ((*addr_exp)) || (*addr_exp) == '\0')
	error (_("Missing register name"));

      /* Find the start/end of this register name/num/group.  */
      start = addr_exp;
      while ((*addr_exp) != '\0' && !isspace ((*addr_exp)))
	addr_exp++;
      end = addr_exp;
      
      /* Figure out what we've found and display it.  */

      /* A register name?  */
      {
	int regnum = frame_map_name_to_regnum (deprecated_selected_frame,
					       start, end - start);
	if (regnum >= 0)
	  {
	    gdbarch_print_registers_info (current_gdbarch, gdb_stdout,
					  deprecated_selected_frame, regnum, fpregs);
	    continue;
	  }
      }
	
      /* A register number?  (how portable is this one?).  */
      {
	char *endptr;
	int regnum = strtol (start, &endptr, 0);
	if (endptr == end
	    && regnum >= 0
	    && regnum < NUM_REGS + NUM_PSEUDO_REGS)
	  {
	    gdbarch_print_registers_info (current_gdbarch, gdb_stdout,
					  deprecated_selected_frame, regnum, fpregs);
	    continue;
	  }
      }

      /* A register group?  */
      {
	struct reggroup *group;
	for (group = reggroup_next (current_gdbarch, NULL);
	     group != NULL;
	     group = reggroup_next (current_gdbarch, group))
	  {
	    /* Don't bother with a length check.  Should the user
	       enter a short register group name, go with the first
	       group that matches.  */
	    if (strncmp (start, reggroup_name (group), end - start) == 0)
	      break;
	  }
	if (group != NULL)
	  {
	    int regnum;
	    for (regnum = 0; regnum < NUM_REGS + NUM_PSEUDO_REGS; regnum++)
	      {
		if (gdbarch_register_reggroup_p (current_gdbarch, regnum,
						 group))
		  gdbarch_print_registers_info (current_gdbarch,
						gdb_stdout, deprecated_selected_frame,
						regnum, fpregs);
	      }
	    continue;
	  }
      }

      /* Nothing matched.  */
      error (_("Invalid register `%.*s'"), (int) (end - start), start);
    }
}

void
all_registers_info (char *addr_exp, int from_tty)
{
  registers_info (addr_exp, 1);
}

static void
nofp_registers_info (char *addr_exp, int from_tty)
{
  registers_info (addr_exp, 0);
}

static void
print_vector_info (struct gdbarch *gdbarch, struct ui_file *file,
		   struct frame_info *frame, const char *args)
{
  if (!target_has_registers)
    error (_("The program has no registers now."));
  if (deprecated_selected_frame == NULL)
    error (_("No selected frame."));

  if (gdbarch_print_vector_info_p (gdbarch))
    gdbarch_print_vector_info (gdbarch, file, frame, args);
  else
    {
      int regnum;
      int printed_something = 0;

      for (regnum = 0; regnum < NUM_REGS + NUM_PSEUDO_REGS; regnum++)
	{
	  if (gdbarch_register_reggroup_p (gdbarch, regnum, vector_reggroup))
	    {
	      printed_something = 1;
	      gdbarch_print_registers_info (gdbarch, file, frame, regnum, 1);
	    }
	}
      if (!printed_something)
	fprintf_filtered (file, "No vector information\n");
    }
}

static void
vector_info (char *args, int from_tty)
{
  print_vector_info (current_gdbarch, gdb_stdout, deprecated_selected_frame, args);
}


/*
 * TODO:
 * Should save/restore the tty state since it might be that the
 * program to be debugged was started on this tty and it wants
 * the tty in some state other than what we want.  If it's running
 * on another terminal or without a terminal, then saving and
 * restoring the tty state is a harmless no-op.
 * This only needs to be done if we are attaching to a process.
 */

/*
   attach_command --
   takes a program started up outside of gdb and ``attaches'' to it.
   This stops it cold in its tracks and allows us to start debugging it.
   and wait for the trace-trap that results from attaching.  */

void
attach_command (char *args, int from_tty)
{
  char *exec_file;
  char *full_exec_path = NULL;

  dont_repeat ();		/* Not for the faint of heart */

  if (target_has_execution)
    {
      if (query ("A program is being debugged already.  Kill it? "))
	target_kill ();
      else
	error (_("Not killed."));
    }

  /* Clear out solib state. Otherwise the solib state of the previous
     inferior might have survived and is entirely wrong for the new
     target.  This has been observed on Linux using glibc 2.3. How to
     reproduce:

     bash$ ./foo&
     [1] 4711
     bash$ ./foo&
     [1] 4712
     bash$ gdb ./foo
     [...]
     (gdb) attach 4711
     (gdb) detach
     (gdb) attach 4712
     Cannot access memory at address 0xdeadbeef
  */
#ifdef CLEAR_SOLIB
      CLEAR_SOLIB ();
#else
      clear_solib ();
#endif

  /* APPLE LOCAL checkpoints */
  clear_all_checkpoints ();

  target_attach (args, from_tty);

  /* Set up the "saved terminal modes" of the inferior
     based on what modes we are starting it with.  */
  target_terminal_init ();

  /* Set up execution context to know that we should return from
     wait_for_inferior as soon as the target reports a stop.  */
  init_wait_for_inferior ();
  clear_proceed_status ();

  /* No traps are generated when attaching to inferior under Mach 3
     or GNU hurd.  */
#ifndef ATTACH_NO_WAIT
  /* Careful here. See comments in inferior.h.  Basically some OSes
     don't ignore SIGSTOPs on continue requests anymore.  We need a
     way for handle_inferior_event to reset the stop_signal variable
     after an attach, and this is what STOP_QUIETLY_NO_SIGSTOP is for.  */
  stop_soon = STOP_QUIETLY_NO_SIGSTOP;
  wait_for_inferior ();
  stop_soon = NO_STOP_QUIETLY;
#endif

  /*
   * If no exec file is yet known, try to determine it from the
   * process itself.
   */
  exec_file = (char *) get_exec_file (0);
  if (!exec_file)
    {
      exec_file = target_pid_to_exec_file (PIDGET (inferior_ptid));
      if (exec_file)
	{
	  /* It's possible we don't have a full path, but rather just a
	     filename.  Some targets, such as HP-UX, don't provide the
	     full path, sigh.

	     Attempt to qualify the filename against the source path.
	     (If that fails, we'll just fall back on the original
	     filename.  Not much more we can do...)
	   */
	  if (!source_full_path_of (exec_file, &full_exec_path))
	    full_exec_path = savestring (exec_file, strlen (exec_file));

	  exec_file_attach (full_exec_path, from_tty);
	  symbol_file_add_main (full_exec_path, from_tty);
	}
    }
  else
    {
      reopen_exec_file ();
      reread_symbols ();
    }

#ifdef SOLIB_ADD
  /* Add shared library symbols from the newly attached process, if any.  */
  SOLIB_ADD ((char *) 0, from_tty, &current_target, auto_solib_add);
#else
  solib_add (NULL, from_tty, &current_target, auto_solib_add);
#endif
  /* APPLE LOCAL control breakpoint re-enable messages */
  re_enable_breakpoints_in_shlibs (0);

  /* Take any necessary post-attaching actions for this platform.
   */
  target_post_attach (PIDGET (inferior_ptid));

  /* Install inferior's terminal modes.  */
  target_terminal_inferior ();

  normal_stop ();

  if (deprecated_attach_hook)
    deprecated_attach_hook ();
}

/*
 * detach_command --
 * takes a program previously attached to and detaches it.
 * The program resumes execution and will no longer stop
 * on signals, etc.  We better not have left any breakpoints
 * in the program or it'll die when it hits one.  For this
 * to work, it may be necessary for the process to have been
 * previously attached.  It *might* work if the program was
 * started via the normal ptrace (PTRACE_TRACEME).
 */

static void
detach_command (char *args, int from_tty)
{
  dont_repeat ();		/* Not for the faint of heart.  */
  target_detach (args, from_tty);
#if defined(SOLIB_RESTART)
  SOLIB_RESTART ();
#endif
  if (deprecated_detach_hook)
    deprecated_detach_hook ();
}

/* Disconnect from the current target without resuming it (leaving it
   waiting for a debugger).

   We'd better not have left any breakpoints in the program or the
   next debugger will get confused.  Currently only supported for some
   remote targets, since the normal attach mechanisms don't work on
   stopped processes on some native platforms (e.g. GNU/Linux).  */

static void
disconnect_command (char *args, int from_tty)
{
  dont_repeat ();		/* Not for the faint of heart */
  target_disconnect (args, from_tty);
#if defined(SOLIB_RESTART)
  SOLIB_RESTART ();
#endif
  if (deprecated_detach_hook)
    deprecated_detach_hook ();
}

/* Stop the execution of the target while running in async mode, in
   the backgound. */
void
interrupt_target_command (char *args, int from_tty)
{
  if (target_can_async_p ())
    {
      dont_repeat ();		/* Not for the faint of heart */
      target_stop ();
    }
}

static void
print_float_info (struct gdbarch *gdbarch, struct ui_file *file,
		  struct frame_info *frame, const char *args)
{
  if (!target_has_registers)
    error (_("The program has no registers now."));
  if (deprecated_selected_frame == NULL)
    error (_("No selected frame."));

  if (gdbarch_print_float_info_p (gdbarch))
    gdbarch_print_float_info (gdbarch, file, frame, args);
  else
    {
      int regnum;
      int printed_something = 0;

      for (regnum = 0; regnum < NUM_REGS + NUM_PSEUDO_REGS; regnum++)
	{
	  if (gdbarch_register_reggroup_p (gdbarch, regnum, float_reggroup))
	    {
	      printed_something = 1;
	      gdbarch_print_registers_info (gdbarch, file, frame, regnum, 1);
	    }
	}
      if (!printed_something)
	fprintf_filtered (file, "\
No floating-point info available for this processor.\n");
    }
}

static void
float_info (char *args, int from_tty)
{
  print_float_info (current_gdbarch, gdb_stdout, 
		    deprecated_selected_frame, args);
}

static void
unset_command (char *args, int from_tty)
{
  printf_filtered (_("\
\"unset\" must be followed by the name of an unset subcommand.\n"));
  help_list (unsetlist, "unset ", -1, gdb_stdout);
}

void
_initialize_infcmd (void)
{
  struct cmd_list_element *c = NULL;

  /* add the filename of the terminal connected to inferior I/O */
  add_setshow_filename_cmd ("inferior-tty", class_run,
			    &inferior_io_terminal, _("\
Set terminal for future runs of program being debugged."), _("\
Show terminal for future runs of program being debugged."), _("\
Usage: set inferior-tty /dev/pts/1"), NULL, NULL, &setlist, &showlist);
  add_com_alias ("tty", "set inferior-tty", class_alias, 0);

  /* c->completer_word_break_characters = gdb_completer_filename_word_break_characters; */ /* FIXME */

  add_setshow_optional_filename_cmd ("args", class_run,
				     &inferior_args, _("\
Set argument list to give program being debugged when it is started."), _("\
Show argument list to give program being debugged when it is started."), _("\
Follow this command with any number of args, to be passed to the program."),
				     notice_args_set,
				     notice_args_read,
				     &setlist, &showlist);

  c = add_cmd ("environment", no_class, environment_info, _("\
The environment to give the program, or one variable's value.\n\
With an argument VAR, prints the value of environment variable VAR to\n\
give the program being debugged.  With no arguments, prints the entire\n\
environment to be given to the program."), &showlist);
  set_cmd_completer (c, noop_completer);

  add_prefix_cmd ("unset", no_class, unset_command,
		  _("Complement to certain \"set\" commands."),
		  &unsetlist, "unset ", 0, &cmdlist);

  c = add_cmd ("environment", class_run, unset_environment_command, _("\
Cancel environment variable VAR for the program.\n\
This does not affect the program until the next \"run\" command."),
	       &unsetlist);
  set_cmd_completer (c, noop_completer);

  c = add_cmd ("environment", class_run, set_environment_command, _("\
Set environment variable value to give the program.\n\
Arguments are VAR VALUE where VAR is variable name and VALUE is value.\n\
VALUES of environment variables are uninterpreted strings.\n\
This does not affect the program until the next \"run\" command."),
	       &setlist);
  set_cmd_completer (c, noop_completer);

  c = add_com ("path", class_files, path_command, _("\
Add directory DIR(s) to beginning of search path for object files.\n\
$cwd in the path means the current working directory.\n\
This path is equivalent to the $PATH shell variable.  It is a list of\n\
directories, separated by colons.  These directories are searched to find\n\
fully linked executable files and separately compiled object files as needed."));
  set_cmd_completer (c, filename_completer);

  c = add_cmd ("paths", no_class, path_info, _("\
Current search path for finding object files.\n\
$cwd in the path means the current working directory.\n\
This path is equivalent to the $PATH shell variable.  It is a list of\n\
directories, separated by colons.  These directories are searched to find\n\
fully linked executable files and separately compiled object files as needed."),
	       &showlist);
  set_cmd_completer (c, noop_completer);

  /* APPLE LOCAL process completer */
  c = add_com ("attach", class_run, attach_command, _("\
Attach to a process or file outside of GDB.\n\
This command attaches to another target, of the same type as your last\n\
\"target\" command (\"info files\" will show your target stack).\n\
The command may take as argument a process id, a process name\n\
(with an optional process-id as a suffix), or a device file.\n\
For a process id, you must have permission to send the process a signal,\n\
and it must have the same effective uid as the debugger.\n\
When using \"attach\" to an existing process, the debugger finds the\n\
program running in the process, looking first in the current working\n\
directory, or (if not found there) using the source file search path\n\
(see the \"directory\" command).  You can also use the \"file\" command\n\
to specify the program, and to load its symbol table.\n\
As an Apple extension, the attach command can be given the name of a process.\n\
You may use tab completion.  The process name is case sensitive.\n\
You may also use the '-waitfor' argument followed by a process name -- gdb\n\
will poll and loop waiting for the process by that name to launch."));

  /* APPLE LOCAL process completer */
  set_cmd_completer (c, PROCESS_COMPLETER);
#ifdef PROCESS_COMPLETER_WORD_BREAK_CHARACTERS
  /* c->completer_word_break_characters = PROCESS_COMPLETER_WORD_BREAK_CHARACTERS; */ /* FIXME */
#endif

  add_com ("detach", class_run, detach_command, _("\
Detach a process or file previously attached.\n\
If a process, it is no longer traced, and it continues its execution.  If\n\
you were debugging a file, the file is closed and gdb no longer accesses it."));

  add_com ("disconnect", class_run, disconnect_command, _("\
Disconnect from a target.\n\
The target will wait for another debugger to connect.  Not available for\n\
all targets."));

  add_com ("signal", class_run, signal_command, _("\
Continue program giving it signal specified by the argument.\n\
An argument of \"0\" means continue program without giving it a signal."));

  add_com ("stepi", class_run, stepi_command, _("\
Step one instruction exactly.\n\
Argument N means do this N times (or till program stops for another reason)."));
  add_com_alias ("si", "stepi", class_alias, 0);

  add_com ("nexti", class_run, nexti_command, _("\
Step one instruction, but proceed through subroutine calls.\n\
Argument N means do this N times (or till program stops for another reason)."));
  add_com_alias ("ni", "nexti", class_alias, 0);

  add_com ("finish", class_run, finish_command, _("\
Execute until selected stack frame returns.\n\
Upon return, the value returned is printed and put in the value history."));

  add_com ("next", class_run, next_command, _("\
Step program, proceeding through subroutine calls.\n\
Like the \"step\" command as long as subroutine calls do not happen;\n\
when they do, the call is treated as one instruction.\n\
Argument N means do this N times (or till program stops for another reason)."));
  add_com_alias ("n", "next", class_run, 1);
  if (xdb_commands)
    add_com_alias ("S", "next", class_run, 1);

  add_com ("step", class_run, step_command, _("\
Step program until it reaches a different source line.\n\
Argument N means do this N times (or till program stops for another reason)."));
  add_com_alias ("s", "step", class_run, 1);

  c = add_com ("until", class_run, until_command, _("\
Execute until the program reaches a source line greater than the current\n\
or a specified location (same args as break command) within the current frame."));
  set_cmd_completer (c, location_completer);
  add_com_alias ("u", "until", class_run, 1);

  c = add_com ("advance", class_run, advance_command, _("\
Continue the program up to the given location (same form as args for break command).\n\
Execution will also stop upon exit from the current stack frame."));
  set_cmd_completer (c, location_completer);

  c = add_com ("jump", class_run, jump_command, _("\
Continue program being debugged at specified line or address.\n\
Give as argument either LINENUM or *ADDR, where ADDR is an expression\n\
for an address to start at."));
  set_cmd_completer (c, location_completer);

  if (xdb_commands)
    {
      c = add_com ("go", class_run, go_command, _("\
Usage: go <location>\n\
Continue program being debugged, stopping at specified line or \n\
address.\n\
Give as argument either LINENUM or *ADDR, where ADDR is an \n\
expression for an address to start at.\n\
This command is a combination of tbreak and jump."));
      set_cmd_completer (c, location_completer);
    }

  if (xdb_commands)
    add_com_alias ("g", "go", class_run, 1);

  add_com ("continue", class_run, continue_command, _("\
Continue program being debugged, after signal or breakpoint.\n\
If proceeding from breakpoint, a number N may be used as an argument,\n\
which means to set the ignore count of that breakpoint to N - 1 (so that\n\
the breakpoint won't break until the Nth time it is reached)."));
  add_com_alias ("c", "cont", class_run, 1);
  add_com_alias ("fg", "cont", class_run, 1);

  c = add_com ("run", class_run, run_command, _("\
Start debugged program.  You may specify arguments to give it.\n\
If 'start-with-shell' is set to 1, args may include \"*\", or \"[...]\";\n\
they will be expanded using \"sh\".  Input and output redirection with\n\
\">\", \"<\", or \">>\" will also be allowed.\n\n\
With no arguments, uses arguments last specified (with \"run\" or \"set args\").\n\
To cancel previous arguments and run with no arguments,\n\
use \"set args\" without arguments."));
  set_cmd_completer (c, filename_completer);
  /* c->completer_word_break_characters = gdb_completer_filename_word_break_characters; */ /* FIXME */
  add_com_alias ("r", "run", class_run, 1);
  if (xdb_commands)
    add_com ("R", class_run, run_no_args_command,
	     _("Start debugged program with no arguments."));

  c = add_com ("start", class_run, start_command, _("\
Run the debugged program until the beginning of the main procedure.\n\
You may specify arguments to give to your program, just as with the\n\
\"run\" command."));
  set_cmd_completer (c, filename_completer);

  add_com ("interrupt", class_run, interrupt_target_command,
	   _("Interrupt the execution of the debugged program."));

  add_info ("registers", nofp_registers_info, _("\
List of integer registers and their contents, for selected stack frame.\n\
Register name as argument means describe only that register."));
  add_info_alias ("r", "registers", 1);

  if (xdb_commands)
    add_com ("lr", class_info, nofp_registers_info, _("\
List of integer registers and their contents, for selected stack frame.\n\
Register name as argument means describe only that register."));
  add_info ("all-registers", all_registers_info, _("\
List of all registers and their contents, for selected stack frame.\n\
Register name as argument means describe only that register."));

  add_info ("program", program_info,
	    _("Execution status of the program."));

  /* APPLE LOCAL begin info pid */
  /* "info pid" & MI's "-pid-info" are Apple local cmds for
     use by Xcode.  */
  add_info ("pid", pid_info,
	    "Process ID of the program.");
  /* APPLE LOCAL end info pid */

  add_info ("float", float_info,
	    _("Print the status of the floating point unit\n"));

  add_info ("vector", vector_info,
	    _("Print the status of the vector unit\n"));

  inferior_environ = make_environ ();
  init_environ (inferior_environ);

  /* APPLE LOCAL */
  smuggle_dyld_settings (inferior_environ);
}

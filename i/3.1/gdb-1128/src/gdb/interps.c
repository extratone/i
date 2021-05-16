/* Manages interpreters for GDB, the GNU debugger.

   Copyright 2000, 2002, 2003 Free Software Foundation, Inc.

   Written by Jim Ingham <jingham@apple.com> of Apple Computer, Inc.

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
   Boston, MA 02111-1307, USA. */

/* This is just a first cut at separating out the "interpreter"
   functions of gdb into self-contained modules.  There are a couple
   of open areas that need to be sorted out:

   1) The interpreter explicitly contains a UI_OUT, and can insert itself
   into the event loop, but it doesn't explicitly contain hooks for readline.
   I did this because it seems to me many interpreters won't want to use
   the readline command interface, and it is probably simpler to just let
   them take over the input in their resume proc.  */

#include "defs.h"
#include "gdbcmd.h"
#include "ui-out.h"
#include "event-loop.h"
#include "event-top.h"
#include "interps.h"
#include "completer.h"
#include "gdb_string.h"
#include "gdb-events.h"
#include "gdb_assert.h"
#include "top.h"		/* For command_loop.  */
#include "exceptions.h"

struct interp
{
  /* This is the name in "-i=" and set interpreter. */
  const char *name;

  /* Interpreters are stored in a linked list, this is the next
     one...  */
  struct interp *next;

  /* This is a cookie that an instance of the interpreter can use.
     This is a bit confused right now as the exact initialization
     sequence for it, and how it relates to the interpreter's uiout
     object is a bit confused.  */

  /* APPLE LOCAL: I changed my mind about how this was going to get
     initialized midway, and didn't fix the API's.  This is set by
     the init_proc for the interpreter.  The "data" that is passed
     into gdb_new_interpreter is ignored.  We should just take it
     out...  

     Other than that, it is just some data to use.  The mi
     uses it to store a copy to the mi_interp structure, which happens
     to hold the interpreter's various uiout's.  
     The cli doesn't need this. 

  */

  void *data;

  /* Has the init_proc been run? */
  int inited;

  /* This is the ui_out used to collect results for this interpreter.
     It can be a formatter for stdout, as is the case for the console
     & mi outputs, or it might be a result formatter.  */
  struct ui_out *interpreter_out;

  const struct interp_procs *procs;
  int quiet_p;
};

/* Functions local to this file. */
static void initialize_interps (void);
static void set_interpreter_cmd (char *args, int from_tty, 
				 struct cmd_list_element *c);
static void list_interpreter_cmd (char *args, int from_tty);

static char **interpreter_completer (char *text, char *word);

/* The magic initialization routine for this module. */

void _initialize_interpreter (void);

/* Variables local to this file: */

static struct interp *interp_list = NULL;
static struct interp *current_interpreter = NULL;

static int interpreter_initialized = 0;

/* interp_new - This allocates space for a new interpreter,
   fills the fields from the inputs, and returns a pointer to the
   interpreter. */
struct interp *
interp_new (const char *name, void *data, struct ui_out *uiout,
	    const struct interp_procs *procs)
{
  struct interp *new_interp;

  new_interp = XMALLOC (struct interp);

  new_interp->name = xstrdup (name);
  new_interp->data = data;
  new_interp->interpreter_out = uiout;
  new_interp->quiet_p = 0;
  new_interp->procs = procs;
  new_interp->inited = 0;

  return new_interp;
}

/* Add interpreter INTERP to the gdb interpreter list.  The
   interpreter must not have previously been added.  */
void
interp_add (struct interp *interp)
{
  if (!interpreter_initialized)
    initialize_interps ();

  gdb_assert (interp_lookup (interp->name) == NULL);

  interp->next = interp_list;
  interp_list = interp;
}

/* This sets the current interpreter to be INTERP.  If INTERP has not
   been initialized, then this will also run the init proc.  If the
   init proc is successful, return a pointer to the old interp.  If we
   can't restore the old interpreter, then raise an internal error,
   since we are in pretty bad shape at this point. */

struct interp *
interp_set (struct interp *interp)
{
  struct interp *old_interp = current_interpreter;
  int first_time = 0;


  char buffer[64];

  if (current_interpreter != NULL)
    {
      /* APPLE LOCAL: Don't do this, you can't be sure there are no
	 continuations from the enclosing interpreter which should
	 really be run when that interpreter is in force. */
#if 0
      do_all_continuations ();
#endif
      ui_out_flush (uiout);
      if (current_interpreter->procs->suspend_proc
	  && !current_interpreter->procs->suspend_proc (current_interpreter->
							data))
	{
	  error (_("Could not suspend interpreter \"%s\"."),
		 current_interpreter->name);
	}
    }
  else
    {
      first_time = 1;
    }

  current_interpreter = interp;

  /* We use interpreter_p for the "set interpreter" variable, so we need
     to make sure we have a malloc'ed copy for the set command to free. */
  if (interpreter_p != NULL
      && strcmp (current_interpreter->name, interpreter_p) != 0)
    {
      xfree (interpreter_p);

      interpreter_p = xstrdup (current_interpreter->name);
    }

  uiout = interp->interpreter_out;

  /* Run the init proc.  If it fails, try to restore the old interp. */

  /* APPLE LOCAL: FIXME: Keith cut restoring the old interp, but didn't
     change the comment to reflect this.  */

  if (!interp->inited)
    {
      if (interp->procs->init_proc != NULL)
	{
	  interp->data = interp->procs->init_proc ();
	}
      interp->inited = 1;
    }

  /* APPLE LOCAL: I don't think we want to clear the parent interpreter's
     The parent interpreter may want to be able to snoop on the child
     interpreter through them.  */

#if 0
  /* Clear out any installed interpreter hooks/event handlers.  */
  clear_interpreter_hooks ();
#endif

  if (interp->procs->resume_proc != NULL
      && (!interp->procs->resume_proc (interp->data)))
    {
      if (old_interp == NULL || !interp_set (old_interp))
	internal_error (__FILE__, __LINE__,
			_("Failed to initialize new interp \"%s\" %s"),
			interp->name, "and could not restore old interp!\n");
      return NULL;
    }

  /* Finally, put up the new prompt to show that we are indeed here. 
     Also, display_gdb_prompt for the console does some readline magic
     which is needed for the console interpreter, at least... */

  if (!first_time)
    {
      if (!interp_quiet_p (interp))
	{
	  sprintf (buffer, "Switching to interpreter \"%.24s\".\n",
		   interp->name);
	  ui_out_text (uiout, buffer);
	}
      display_gdb_prompt (NULL);
    }

  /* If there wasn't any interp before, return the current interp.  
     That way if somebody is grabbing the return value and using
     it, it will actually work first time through.  */
  if (old_interp == NULL)
    return current_interpreter;
  else
    return old_interp;
}

/* interp_lookup - Looks up the interpreter for NAME.  If no such
   interpreter exists, return NULL, otherwise return a pointer to the
   interpreter.  */
struct interp *
interp_lookup (const char *name)
{
  struct interp *interp;

  if (name == NULL || strlen (name) == 0)
    return NULL;

  for (interp = interp_list; interp != NULL; interp = interp->next)
    {
      if (strcmp (interp->name, name) == 0)
	return interp;
    }

  return NULL;
}

/* Returns the current interpreter. */

struct interp *
current_interp (void)
{
  return current_interpreter;
}

struct ui_out *
interp_ui_out (struct interp *interp)
{
  if (interp != NULL)
    return interp->interpreter_out;

  return current_interpreter->interpreter_out;
}

/* APPLE LOCAL: Ira uses this for the MacsBug plugin, where he wants to
   run the standard CLI interpreter, but he wants to also monkey with 
   it's output so he can divert it to various places in the display.
   It's arguable that creating a whole new interpreter (like the TUI 
   does) is cleaner.  But if all you need to do is reformat output
   that seems like overkill.
   Given an input INTERP and NEW_UIOUT, sets the uiout of INTERP
   to NEW_UIOUT, and returns INTERP's previous INTERP.  
   If INTERP == NULL, uses the current interpreter.  */

struct ui_out *
interp_set_ui_out (struct interp *interp, struct ui_out *new_uiout)
{
  struct ui_out *prev_uiout;
  
  if (new_uiout == NULL)
    error ("Must provide a new uiout.");
  
  if (interp != NULL)
    {
      prev_uiout = interp->interpreter_out;
      interp->interpreter_out = new_uiout;
    }
  else
    {
      prev_uiout = current_interp()->interpreter_out;
      current_interp()->interpreter_out = new_uiout; /* use current_interpreter */
    }
  
  return prev_uiout;
}

/* Returns true if the current interp is the passed in name. */
int
current_interp_named_p (const char *interp_name)
{
  if (current_interpreter)
    return (strcmp (current_interpreter->name, interp_name) == 0);

  return 0;
}

/* This is called in display_gdb_prompt.  If the proc returns a zero
   value, display_gdb_prompt will return without displaying the
   prompt.  */
int
current_interp_display_prompt_p (void)
{
  if (current_interpreter == NULL
      || current_interpreter->procs->prompt_proc_p == NULL)
    return 0;
  else
    return current_interpreter->procs->prompt_proc_p (current_interpreter->
						      data);
}

/* Run the current command interpreter's main loop.  */
void
current_interp_command_loop (void)
{
  /* Somewhat messy.  For the moment prop up all the old ways of
     selecting the command loop.  `deprecated_command_loop_hook'
     should be deprecated.  */
  if (deprecated_command_loop_hook != NULL)
    deprecated_command_loop_hook ();
  else if (current_interpreter != NULL
	   && current_interpreter->procs->command_loop_proc != NULL)
    current_interpreter->procs->command_loop_proc (current_interpreter->data);
  else
    cli_command_loop (NULL);
}

int
interp_quiet_p (struct interp *interp)
{
  if (interp != NULL)
    return interp->quiet_p;
  else
    return current_interpreter->quiet_p;
}

/* APPLE LOCAL: I need this in the mi interpreter.
   In the FSF it is static.  */

int
interp_set_quiet (struct interp *interp, int quiet)
{
  int old_val = interp->quiet_p;
  interp->quiet_p = quiet;
  return old_val;
}

/* interp_exec - This executes COMMAND_STR in the current 
   interpreter. */
int
interp_exec_p (struct interp *interp)
{
  return interp->procs->exec_proc != NULL;
}

struct gdb_exception
interp_exec (struct interp *interp, const char *command_str)
{
  if (interp->procs->exec_proc != NULL)
    {
      return interp->procs->exec_proc (interp->data, command_str);
    }
  return exception_none;
}


int
interp_complete (struct interp *interp, 
		 char *word, char *command_buffer, int cursor, int limit)
{
  if (interp->procs->complete_proc != NULL)
    {
      return interp->procs->complete_proc (interp->data, word, command_buffer, 
                                           cursor, limit);
    }
  
  return 0;
}


/* A convenience routine that nulls out all the common command hooks.
   Use it when removing your interpreter in its suspend proc.  */
void
clear_interpreter_hooks (void)
{
  deprecated_init_ui_hook = 0;
  deprecated_print_frame_info_listing_hook = 0;
  /*print_frame_more_info_hook = 0; */
  deprecated_query_hook = 0;
  deprecated_warning_hook = 0;
  deprecated_create_breakpoint_hook = 0;
  deprecated_delete_breakpoint_hook = 0;
  deprecated_modify_breakpoint_hook = 0;
  deprecated_interactive_hook = 0;
  deprecated_registers_changed_hook = 0;
  deprecated_readline_begin_hook = 0;
  deprecated_readline_hook = 0;
  deprecated_readline_end_hook = 0;
  deprecated_register_changed_hook = 0;
  deprecated_memory_changed_hook = 0;
  deprecated_context_hook = 0;
  deprecated_target_wait_hook = 0;
  deprecated_call_command_hook = 0;
  deprecated_error_hook = 0;
  deprecated_error_begin_hook = 0;
  deprecated_command_loop_hook = 0;
  clear_gdb_event_hooks ();
}

/* This is a lazy init routine, called the first time the interpreter
   module is used.  I put it here just in case, but I haven't thought
   of a use for it yet.  I will probably bag it soon, since I don't
   think it will be necessary.  */
static void
initialize_interps (void)
{
  interpreter_initialized = 1;
  /* Don't know if anything needs to be done here... */
}

static void
interpreter_exec_cmd (char *args, int from_tty)
{
  struct interp *old_interp, *interp_to_use;
  char **prules = NULL;
  char **trule = NULL;
  unsigned int nrules;
  unsigned int i;
  int old_quiet, use_quiet;

  prules = buildargv (args);
  if (prules == NULL)
    {
      error (_("unable to parse arguments"));
    }

  nrules = 0;
  if (prules != NULL)
    {
      for (trule = prules; *trule != NULL; trule++)
	{
	  nrules++;
	}
    }

  if (nrules < 2)
    error (_("usage: interpreter-exec <interpreter> [ <command> ... ]"));

  interp_to_use = interp_lookup (prules[0]);
  if (interp_to_use == NULL)
    error (_("Could not find interpreter \"%s\"."), prules[0]);

  /* Temporarily set interpreters quiet */
  use_quiet = interp_set_quiet (interp_to_use, 1);

  /* APPLE LOCAL get old quiet */
  old_interp = interp_set (interp_to_use);
  if (old_interp == NULL)
    error (_("Could not switch to interpreter \"%s\"."), prules[0]);
  old_quiet = interp_set_quiet (old_interp, 1);

  for (i = 1; i < nrules; i++)
    {
      struct gdb_exception e = interp_exec (interp_to_use, prules[i]);
      if (e.reason < 0)
	{
	  interp_set (old_interp);
	  interp_set_quiet (interp_to_use, old_quiet);
	  error (_("error in command: \"%s\"."), prules[i]);
	  break;
	}
    }

  interp_set (old_interp);
  interp_set_quiet (interp_to_use, use_quiet);
  interp_set_quiet (old_interp, old_quiet);
}

/* List the possible interpreters which could complete the given text. */
static char **
interpreter_completer (char *text, char *word)
{
  int alloced = 0;
  int textlen;
  int num_matches;
  char **matches;
  struct interp *interp;

  /* We expect only a very limited number of interpreters, so just
     allocate room for all of them. */
  for (interp = interp_list; interp != NULL; interp = interp->next)
    ++alloced;
  matches = (char **) xmalloc (alloced * sizeof (char *));

  num_matches = 0;
  textlen = strlen (text);
  for (interp = interp_list; interp != NULL; interp = interp->next)
    {
      if (strncmp (interp->name, text, textlen) == 0)
	{
	  matches[num_matches] =
	    (char *) xmalloc (strlen (word) + strlen (interp->name) + 1);
	  if (word == text)
	    strcpy (matches[num_matches], interp->name);
	  else if (word > text)
	    {
	      /* Return some portion of interp->name */
	      strcpy (matches[num_matches], interp->name + (word - text));
	    }
	  else
	    {
	      /* Return some of text plus interp->name */
	      strncpy (matches[num_matches], word, text - word);
	      matches[num_matches][text - word] = '\0';
	      strcat (matches[num_matches], interp->name);
	    }
	  ++num_matches;
	}
    }

  if (num_matches == 0)
    {
      xfree (matches);
      matches = NULL;
    }
  else if (num_matches < alloced)
    {
      matches = (char **) xrealloc ((char *) matches, ((num_matches + 1)
						       * sizeof (char *)));
      matches[num_matches] = NULL;
    }

  return matches;
}

/* APPLE LOCAL: I guess Keith eliminated this because they added
 * an interpreter completer?  Don't see why, though...
 * list_interpreter_cmd - This implements "info interpreters".
 */

void
list_interpreter_cmd (char *args, int from_tty)
{
  struct interp *interp_ptr;
  struct cleanup *list_cleanup;

  list_cleanup = make_cleanup_ui_out_list_begin_end (uiout, "interpreters");
  for (interp_ptr = interp_list; interp_ptr != NULL; 
       interp_ptr = interp_ptr->next)
    {
      ui_out_text (uiout, "  * ");
      ui_out_field_string (uiout, "interpreter", interp_ptr->name);
      ui_out_text (uiout, "\n");
    }
  do_cleanups (list_cleanup);
}

/* APPLE LOCAL: Keith cut this command out, but I like it... 
    set_interpreter_cmd - This implements "set interpreter foo". */

static void 
set_interpreter_cmd (char *args, int from_tty, struct cmd_list_element * c)
{
  struct interp *interp_ptr;

  dont_repeat ();

  if (cmd_type (c) != set_cmd)
    return;

  interp_ptr = interp_lookup (interpreter_p);
  if (interp_ptr != NULL)
    {
      /* If we are already using this interpreter,
	 just return.  */
      if (interp_ptr == current_interpreter)
	return;

      if (!interp_set (interp_ptr))
	error ("\nCould not switch to interpreter \"%s\", %s%s\".\n", 
	       interp_ptr->name, "reverting to interpreter \"",
	       current_interpreter->name);
    }
  else
    {
      char *bad_name = interpreter_p;
      interpreter_p = xstrdup (current_interpreter->name);
      error ("Could not find interpreter \"%s\".", bad_name);
    }
}


/* This just adds the "interpreter-exec" command.  */
void
_initialize_interpreter (void)
{
  struct cmd_list_element *c;

  add_setshow_string_cmd ("interpreter", class_support,
			  &interpreter_p, _("\
Set the interpreter for gdb."), _("\
Show the interpreter for gdb."), NULL,
			  set_interpreter_cmd, NULL,
			  &setlist, &showlist);
  /* APPLE MERGE 
  set_cmd_completer (c, interpreter_completer);
  */

  add_cmd ("interpreters", class_support,
	       list_interpreter_cmd,
	   "List the interpreters currently available in gdb.",
	       &infolist);

  c = add_cmd ("interpreter-exec", class_support,
	       interpreter_exec_cmd, _("\
Execute a command in an interpreter.  It takes two arguments:\n\
The first argument is the name of the interpreter to use.\n\
The second argument is the command to execute.\n"), &cmdlist);
  set_cmd_completer (c, interpreter_completer);
}

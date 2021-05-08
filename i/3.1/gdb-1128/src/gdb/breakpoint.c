/* Everything about breakpoints, for GDB.

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
/* APPLE LOCAL: we use strftime.  */
#include <time.h>
#include <ctype.h>
#include "symtab.h"
#include "frame.h"
#include "breakpoint.h"
#include "gdbtypes.h"
#include "expression.h"
#include "gdbcore.h"
#include "gdbcmd.h"
#include "value.h"
#include "command.h"
#include "inferior.h"
#include "gdbthread.h"
#include "target.h"
#include "language.h"
#include "gdb_string.h"
#include "demangle.h"
#include "annotate.h"
#include "symfile.h"
#include "objfiles.h"
#include "source.h"
#include "linespec.h"
#include "completer.h"
#include "gdb.h"
#include "ui-out.h"
#include "cli/cli-script.h"
/* APPLE LOCAL: we use cli_out_new.  */
#include "cli-out.h"
#include "gdb_assert.h"
#include "block.h"
/* APPLE LOCAL: for wrappers */
#include "wrapper.h"
#include "solib.h"
#include "solist.h"
#include "observer.h"
#include "exceptions.h"
/* APPLE LOCAL: for exception catching regex */
#include "gdb_regex.h"

#include "gdb-events.h"
/* APPLE LOCAL: we use tilde_expand from readline.  */
#include "readline/tilde.h"
#include "mi/mi-common.h"
/* APPLE LOCAL - subroutine inlining */
#include "inlining.h"

/* Prototypes for local functions. */

static void until_break_command_continuation (struct continuation_arg *arg);

static void catch_command_1 (char *, int, int);

static void enable_delete_command (char *, int);

static void enable_delete_breakpoint (struct breakpoint *);

static void enable_once_command (char *, int);

static void enable_once_breakpoint (struct breakpoint *);

static void disable_command (char *, int);

static void enable_command (char *, int);

static void map_breakpoint_numbers (char *, void (*)(struct breakpoint *));

static void ignore_command (char *, int);

static int breakpoint_re_set_one (void *);

/* APPLE LOCAL breakpoints */
static void breakpoint_re_set_all ();

static void clear_command (char *, int);

static void catch_command (char *, int);

static void watch_command (char *, int);

static int can_use_hardware_watchpoint (struct value *);

/* APPLE LOCAL begin breakpoint lists */
/* To use break_command_1 in gdb_breakpoint, we need
   to capture the breakpoints that break_command_1 made (so we can
   apply the ignore count & conditions to them.  This list will
   be initialized every time break_command_1 is run, and then filled
   by create_breakpoints.  */
struct breakpoint_list
{
  struct breakpoint_list *next;
  struct breakpoint *bp;
};

static int breakpoint_address_is_meaningful (struct breakpoint *);
/* APPLE LOCAL end breakpoint lists */

static int break_command_1 (char *, int, int, struct breakpoint *);

/* APPLE LOCAL: We want to call most of break_command_1 from gdb_breakpoints so we
   don't have lots of duplicated code.  That's what break_command_2 gives us.  */
static int break_command_2 (char *, int, int, struct breakpoint *,
			    char *, int *, struct breakpoint_list **);

static void mention (struct breakpoint *);

/* APPLE LOCAL: Added pending_p.  */
struct breakpoint *set_raw_breakpoint (struct symtab_and_line, enum bptype, 
				       int pending_p);

static void check_duplicates (struct breakpoint *);

static void breakpoint_adjustment_warning (CORE_ADDR, CORE_ADDR, int, int);

static CORE_ADDR adjust_breakpoint_address (CORE_ADDR bpaddr,
                                            enum bptype bptype);

static void describe_other_breakpoints (CORE_ADDR, asection *);

static void breakpoints_info (char *, int);

static void breakpoint_1 (int, int);

static bpstat bpstat_alloc (struct breakpoint *, bpstat);

static int breakpoint_cond_eval (void *);

/* APPLE LOCAL begin exception throw/catch types */
/* These variables contain the regexp's used in current_exception_should_stop
   to determine whether this is an object throw or catch we are interested
   in */

static char *exception_throw_type_regexp;
static char *exception_catch_type_regexp;

static int exception_catchpoint_catch_enabled;
static int exception_catchpoint_throw_enabled;

static int current_exception_should_stop(void);
/* APPLE LOCAL end exception throw/catch types */

static void cleanup_executing_breakpoints (void *);

static void commands_command (char *, int);

static void condition_command (char *, int);

/* APPLE LOCAL begin refactor condition_command */
/* We want to use condition_command to set the condition from 
   gdb_breakpoints, but we already have the breakpoint.  So condition_command_1
   is the functional part of condition_command for use to use.  */
static void condition_command_1 (struct breakpoint *b, char *condition, int from_tty);
/* APPLE LOCAL end refactor condition_command */

static int get_number_trailer (char **, int);

static void do_captured_parse_breakpoint (struct ui_out *, void *);

static enum print_stop_action
print_exception_catchpoint (struct breakpoint *b);

static void
print_one_exception_catchpoint (struct breakpoint *b, CORE_ADDR *last_addr);

static void
print_mention_exception_catchpoint (struct breakpoint *b);

/* APPLE LOCAL: Used for breakpoints that run the target under the mi.  */
static void
async_breakpoint_command_continuation (struct continuation_arg *arg);

void set_breakpoint_count (int);

typedef enum
  {
    mark_inserted,
    mark_uninserted
  }
insertion_state_t;

static int remove_breakpoint (struct bp_location *, insertion_state_t);

static enum print_stop_action print_it_typical (bpstat);

static enum print_stop_action print_bp_stop_message (bpstat bs);

typedef struct
  {
    enum exception_event_kind kind;
    int enable_p;
  }
args_for_catchpoint_enable;

static int watchpoint_check (void *);

static int cover_target_enable_exception_callback (void *);

static void maintenance_info_breakpoints (char *, int);

static void create_longjmp_breakpoint (char *);

static void create_overlay_event_breakpoint (char *);

static int hw_breakpoint_used_count (void);

static int hw_watchpoint_used_count (enum bptype, int *);

static void hbreak_command (char *, int);

static void thbreak_command (char *, int);

/* APPLE LOCAL: Added the by_location arg.  */
static void watch_command_1 (char *, int, int, int);

static void rwatch_command (char *, int);

static void awatch_command (char *, int);

static void do_enable_breakpoint (struct breakpoint *, enum bpdisp);

static void solib_load_unload_1 (char *hookname,
				 int tempflag,
				 char *dll_pathname,
				 char *cond_string, enum bptype bp_kind);

static void create_fork_vfork_event_catchpoint (int tempflag,
						char *cond_string,
						enum bptype bp_kind);

static void stop_command (char *arg, int from_tty);

static void stopin_command (char *arg, int from_tty);

static void stopat_command (char *arg, int from_tty);

static char *ep_find_event_name_end (char *arg);

static char *ep_parse_optional_if_clause (char **arg);

static char *ep_parse_optional_filename (char **arg);

/* APPLE LOCAL return a value */
static struct breakpoint *create_exception_catchpoint (int tempflag, char *cond_string,
					 /* APPLE LOCAL gnu v3 */
					 int gnu_v3_p,
					 enum exception_event_kind ex_event,
					 struct symtab_and_line *sal);

static void catch_exception_command_1 (enum exception_event_kind ex_event, 
				       char *arg, int tempflag, int from_tty);

static void tcatch_command (char *arg, int from_tty);

static void ep_skip_leading_whitespace (char **s);

/* APPLE LOCAL - We may have more than one breakpoint implementing the
   exception catch & exception throw breaks.  So it is convenient to keep
   a separate variable that tracks whether the feature is turned on or not. */

static int exception_catchpoint_catch_enabled;
static int exception_catchpoint_throw_enabled;

/* APPLE LOCAL - for future-break & -break-insert -f.  */
static void restore_saved_pending_break_support (void * val);

/* APPLE LOCAL: Can't let breakpoint_sals_to_pc error when resolving
   pending breakpoints, since that might happen when we are also 
   handling the shared library event.  In any case, it's stupid
   to let one unresolvable breakpoint scotch setting all the rest.  */

static void breakpoint_sals_to_pc (struct symtabs_and_lines *sals, char *address);
/* APPLE LOCAL:  */
static int safe_breakpoint_sals_to_pc (struct symtabs_and_lines *sals, 
				       char *address);

/* APPLE LOCAL: These two track the relative ages of symbols &
   breakpoints.  Increment the symbol generation if you add new
   symbols, and decrement the breakpoint generation if you want to
   force breakpoints to be reset. */
int symbol_generation = 1;
int breakpoint_generation = 0;

/* APPLE LOCAL: Use this variable to quiet the breakpoint setting code
   when RE-SETTING breakpoints.  */
static int dont_mention = 0;
/* APPLE LOCAL: Set the objfile associated with a given breakpoint.  */
static void set_bp_objfile (struct breakpoint *b, struct symtab_and_line *sal);

static void print_catch_info (struct breakpoint *b);

/* Prototypes for exported functions. */

/* If FALSE, gdb will not use hardware support for watchpoints, even
   if such is available. */
static int can_use_hw_watchpoints;

static void
show_can_use_hw_watchpoints (struct ui_file *file, int from_tty,
			     struct cmd_list_element *c,
			     const char *value)
{
  fprintf_filtered (file, _("\
Debugger's willingness to use watchpoint hardware is %s.\n"),
		    value);
}

/* If AUTO_BOOLEAN_FALSE, gdb will not attempt to create pending breakpoints.
   If AUTO_BOOLEAN_TRUE, gdb will automatically create pending breakpoints
   for unrecognized breakpoint locations.  
   If AUTO_BOOLEAN_AUTO, gdb will query when breakpoints are unrecognized.  */
static enum auto_boolean pending_break_support;
static void
show_pending_break_support (struct ui_file *file, int from_tty,
			    struct cmd_list_element *c,
			    const char *value)
{
  fprintf_filtered (file, _("\
Debugger's behavior regarding pending breakpoints is %s.\n"),
		    value);
}

void _initialize_breakpoint (void);

extern int addressprint;	/* Print machine addresses? */

/* Are we executing breakpoint commands?  */
static int executing_breakpoint_commands;

/* Are overlay event breakpoints enabled? */
static int overlay_events_enabled;

/* Walk the following statement or block through all breakpoints.
   ALL_BREAKPOINTS_SAFE does so even if the statment deletes the current
   breakpoint.  */

#define ALL_BREAKPOINTS(B)  for (B = breakpoint_chain; B; B = B->next)

#define ALL_BREAKPOINTS_SAFE(B,TMP)	\
	for (B = breakpoint_chain;	\
	     B ? (TMP=B->next, 1): 0;	\
	     B = TMP)

/* Similar iterators for the low-level breakpoints.  */

#define ALL_BP_LOCATIONS(B)  for (B = bp_location_chain; B; B = B->next)

#define ALL_BP_LOCATIONS_SAFE(B,TMP)	\
	for (B = bp_location_chain;	\
	     B ? (TMP=B->next, 1): 0;	\
	     B = TMP)

/* True if breakpoint hit counts should be displayed in breakpoint info.  */

int show_breakpoint_hit_counts = 1;

/* Chains of all breakpoints defined.  */

struct breakpoint *breakpoint_chain;

struct bp_location *bp_location_chain;

/* Number of last breakpoint made.  */

int breakpoint_count;

/* APPLE LOCAL: Add an breakpoint to the new_breakpoints list */

static void
add_breakpoint_to_new_list (struct breakpoint_list **new_breakpoints,
			    struct breakpoint *bp)
{
  struct breakpoint_list *new_bp = (struct breakpoint_list *)
    xmalloc (sizeof (struct breakpoint_list));
  new_bp->next = *new_breakpoints;
  new_bp->bp = bp;
  *new_breakpoints = new_bp;
}

/* APPLE LOCAL: Clear the restricted breakpoint search list.  */

static void
clear_new_breakpoint_list (struct breakpoint_list *new_breakpoints)
{
  while (new_breakpoints != NULL)
    {
      struct breakpoint_list *list_ptr;
      list_ptr = new_breakpoints;
      new_breakpoints = list_ptr->next;
      xfree (list_ptr);
    }
}

/* END APPLE LOCAL */

/* Pointer to current exception event record */
static struct exception_event_record *current_exception_event;

/* Indicator of whether exception catchpoints should be nuked between
   runs of a program.  */
int deprecated_exception_catchpoints_are_fragile = 0;

/* Indicator of when exception catchpoints set-up should be
   reinitialized -- e.g. when program is re-run.  */
int deprecated_exception_support_initialized = 0;

/* This function returns a pointer to the string representation of the
   pathname of the dynamically-linked library that has just been
   loaded.

   This function must be used only when SOLIB_HAVE_LOAD_EVENT is TRUE,
   or undefined results are guaranteed.

   This string's contents are only valid immediately after the
   inferior has stopped in the dynamic linker hook, and becomes
   invalid as soon as the inferior is continued.  Clients should make
   a copy of this string if they wish to continue the inferior and
   then access the string.  */

#ifndef SOLIB_LOADED_LIBRARY_PATHNAME
#define SOLIB_LOADED_LIBRARY_PATHNAME(pid) ""
#endif

/* This function returns a pointer to the string representation of the
   pathname of the dynamically-linked library that has just been
   unloaded.

   This function must be used only when SOLIB_HAVE_UNLOAD_EVENT is
   TRUE, or undefined results are guaranteed.

   This string's contents are only valid immediately after the
   inferior has stopped in the dynamic linker hook, and becomes
   invalid as soon as the inferior is continued.  Clients should make
   a copy of this string if they wish to continue the inferior and
   then access the string.  */

#ifndef SOLIB_UNLOADED_LIBRARY_PATHNAME
#define SOLIB_UNLOADED_LIBRARY_PATHNAME(pid) ""
#endif

/* This function is called by the "catch load" command.  It allows the
   debugger to be notified by the dynamic linker when a specified
   library file (or any library file, if filename is NULL) is loaded.  */

#ifndef SOLIB_CREATE_CATCH_LOAD_HOOK
#define SOLIB_CREATE_CATCH_LOAD_HOOK(pid,tempflag,filename,cond_string) \
   error (_("catch of library loads not yet implemented on this platform"))
#endif

/* This function is called by the "catch unload" command.  It allows
   the debugger to be notified by the dynamic linker when a specified
   library file (or any library file, if filename is NULL) is
   unloaded.  */

#ifndef SOLIB_CREATE_CATCH_UNLOAD_HOOK
#define SOLIB_CREATE_CATCH_UNLOAD_HOOK(pid, tempflag, filename, cond_string) \
   error (_("catch of library unloads not yet implemented on this platform"))
#endif

/* Return whether a breakpoint is an active enabled breakpoint.  */
static int
breakpoint_enabled (struct breakpoint *b)
{
  return (b->enable_state == bp_enabled && !b->pending);
}

/* Set breakpoint count to NUM.  */

void
set_breakpoint_count (int num)
{
  breakpoint_count = num;
  set_internalvar (lookup_internalvar ("bpnum"),
		   value_from_longest (builtin_type_int, (LONGEST) num));
}

/* Used in run_command to zero the hit count when a new run starts. */

void
clear_breakpoint_hit_counts (void)
{
  struct breakpoint *b;

  ALL_BREAKPOINTS (b)
    b->hit_count = 0;
}

/* Default address, symtab and line to put a breakpoint at
   for "break" command with no arg.
   if default_breakpoint_valid is zero, the other three are
   not valid, and "break" with no arg is an error.

   This set by print_stack_frame, which calls set_default_breakpoint.  */

int default_breakpoint_valid;
CORE_ADDR default_breakpoint_address;
struct symtab *default_breakpoint_symtab;
int default_breakpoint_line;

/* *PP is a string denoting a breakpoint.  Get the number of the breakpoint.
   Advance *PP after the string and any trailing whitespace.

   Currently the string can either be a number or "$" followed by the name
   of a convenience variable.  Making it an expression wouldn't work well
   for map_breakpoint_numbers (e.g. "4 + 5 + 6").
   
   TRAILER is a character which can be found after the number; most
   commonly this is `-'.  If you don't want a trailer, use \0.  */ 
static int
get_number_trailer (char **pp, int trailer)
{
  int retval = 0;	/* default */
  char *p = *pp;

  if (p == NULL)
    /* Empty line means refer to the last breakpoint.  */
    return breakpoint_count;
  else if (*p == '$')
    {
      /* Make a copy of the name, so we can null-terminate it
         to pass to lookup_internalvar().  */
      char *varname;
      char *start = ++p;
      struct value *val;

      while (isalnum (*p) || *p == '_')
	p++;
      varname = (char *) alloca (p - start + 1);
      strncpy (varname, start, p - start);
      varname[p - start] = '\0';
      val = value_of_internalvar (lookup_internalvar (varname));
      if (TYPE_CODE (value_type (val)) == TYPE_CODE_INT)
	retval = (int) value_as_long (val);
      else
	{
	  printf_filtered (_("Convenience variable must have integer value.\n"));
	  retval = 0;
	}
    }
  else
    {
      if (*p == '-')
	++p;
      while (*p >= '0' && *p <= '9')
	++p;
      if (p == *pp)
	/* There is no number here.  (e.g. "cond a == b").  */
	{
	  /* Skip non-numeric token */
	  while (*p && !isspace((int) *p))
	    ++p;
	  /* Return zero, which caller must interpret as error. */
	  retval = 0;
	}
      else
	retval = atoi (*pp);
    }
  if (!(isspace (*p) || *p == '\0' || *p == trailer))
    {
      /* Trailing junk: return 0 and let caller print error msg. */
      while (!(isspace (*p) || *p == '\0' || *p == trailer))
	++p;
      retval = 0;
    }
  while (isspace (*p))
    p++;
  *pp = p;
  return retval;
}


/* Like get_number_trailer, but don't allow a trailer.  */
int
get_number (char **pp)
{
  return get_number_trailer (pp, '\0');
}

/* Parse a number or a range.
 * A number will be of the form handled by get_number.
 * A range will be of the form <number1> - <number2>, and 
 * will represent all the integers between number1 and number2,
 * inclusive.
 *
 * While processing a range, this fuction is called iteratively;
 * At each call it will return the next value in the range.
 *
 * At the beginning of parsing a range, the char pointer PP will
 * be advanced past <number1> and left pointing at the '-' token.
 * Subsequent calls will not advance the pointer until the range
 * is completed.  The call that completes the range will advance
 * pointer PP past <number2>.
 */

int 
get_number_or_range (char **pp)
{
  static int last_retval, end_value;
  static char *end_ptr;
  static int in_range = 0;

  if (**pp != '-')
    {
      /* Default case: pp is pointing either to a solo number, 
	 or to the first number of a range.  */
      last_retval = get_number_trailer (pp, '-');
      if (**pp == '-')
	{
	  char **temp;

	  /* This is the start of a range (<number1> - <number2>).
	     Skip the '-', parse and remember the second number,
	     and also remember the end of the final token.  */

	  temp = &end_ptr; 
	  end_ptr = *pp + 1; 
	  while (isspace ((int) *end_ptr))
	    end_ptr++;	/* skip white space */
	  end_value = get_number (temp);
	  if (end_value < last_retval) 
	    {
	      error (_("inverted range"));
	    }
	  else if (end_value == last_retval)
	    {
	      /* degenerate range (number1 == number2).  Advance the
		 token pointer so that the range will be treated as a
		 single number.  */ 
	      *pp = end_ptr;
	    }
	  else
	    in_range = 1;
	}
    }
  else if (! in_range)
    error (_("negative value"));
  else
    {
      /* pp points to the '-' that betokens a range.  All
	 number-parsing has already been done.  Return the next
	 integer value (one greater than the saved previous value).
	 Do not advance the token pointer 'pp' until the end of range
	 is reached.  */

      if (++last_retval == end_value)
	{
	  /* End of range reached; advance token pointer.  */
	  *pp = end_ptr;
	  in_range = 0;
	}
    }
  return last_retval;
}



/* condition N EXP -- set break condition of breakpoint N to EXP.  */

static void
condition_command (char *arg, int from_tty)
{
  struct breakpoint *b;
  char *p;
  int bnum;

  if (arg == 0)
    error_no_arg (_("breakpoint number"));

  p = arg;
  bnum = get_number (&p);
  if (bnum == 0)
    error (_("Bad breakpoint argument: '%s'"), arg);

  ALL_BREAKPOINTS (b)
    if (b->number == bnum)
    {
      /* APPLE LOCAL refactor condition_command */
      condition_command_1 (b, p, from_tty);
      breakpoints_changed ();
      breakpoint_modify_event (b->number);
      return;
    }

  error (_("No breakpoint number %d."), bnum);
}

/* APPLE LOCAL begin refactor condition_command */
static void
condition_command_1 (struct breakpoint *b, char *condition, int from_tty)
{

  if (b->cond)
    {
      xfree (b->cond);
      b->cond = 0;
    }
  if (b->cond_string != NULL)
    xfree (b->cond_string);
  
  if (condition == 0 || *condition == 0)
    {
      b->cond = 0;
      b->cond_string = NULL;
      if (from_tty)
	printf_filtered ("Breakpoint %d now unconditional.\n", b->number);
    }
  else
    {
      /* APPLE LOCAL: There are a couple of reasons besides a bad
	 expression why the condition might not work here.  For instance,
	 it might require a call into the inferior (ObjC lookup does) when
	 the target is not running...  We don't want to nuke the expression
	 here just for that.  */
      int parse_successful;
      
      /* I don't know if it matters whether this is the string the user
	 typed in or the decompiled expression.  */
      b->cond_string = savestring (condition, strlen (condition));
      if (!b->pending)
	{
	  parse_successful = gdb_parse_exp_1 (&condition, 
					      block_for_pc (b->loc->address), 
					      0, &(b->cond));
	  if (!parse_successful)
		error ("Error parsing breakpoint condition.  "
		       "Will try again when we hit the breakpoint.");
	  else if (*condition)
	    error ("Junk at end of expression.");
	}
    }
  return;
}
/* APPLE LOCAL end refactor condition_command */

static void
commands_command (char *arg, int from_tty)
{
  struct breakpoint *b;
  char *p;
  int bnum;
  struct command_line *l;

  /* If we allowed this, we would have problems with when to
     free the storage, if we change the commands currently
     being read from.  */

  if (executing_breakpoint_commands)
    error (_("Can't use the \"commands\" command among a breakpoint's commands."));

  p = arg;
  bnum = get_number (&p);

  if (p && *p)
    error (_("Unexpected extra arguments following breakpoint number."));

  ALL_BREAKPOINTS (b)
    if (b->number == bnum)
      {
	char *tmpbuf = xstrprintf ("Type commands for when breakpoint %d is hit, one per line.", 
				 bnum);
	struct cleanup *cleanups = make_cleanup (xfree, tmpbuf);
	l = read_command_lines (tmpbuf, from_tty);
	do_cleanups (cleanups);
	free_command_lines (&b->commands);
	b->commands = l;
	breakpoints_changed ();
	breakpoint_modify_event (b->number);
	return;
    }
  error (_("No breakpoint number %d."), bnum);
}

/* APPLE LOCAL begin breakpoint MI */
/* find_breakpoint, breakpoint_add_commands and
   breakpoint_print_commands are helper functions for the MI command
   break-commands. */

struct breakpoint *
find_breakpoint (int bpnum) 
{ 

  struct breakpoint *b;

  ALL_BREAKPOINTS (b)
    {
      if (b->number == bpnum)
	return b; 
    }
  return NULL;
}

void
breakpoint_add_commands (struct breakpoint *b, struct command_line *commands)
{
  free_command_lines (&b->commands);
  b->commands = commands;
  breakpoints_changed ();
  breakpoint_modify_event (b->number);
}

void
breakpoint_print_commands (struct ui_out *uiout, struct breakpoint *b)
{
  struct cleanup *list_cleanup;

  list_cleanup = make_cleanup_ui_out_list_begin_end (uiout, "commands");
  if (b->commands)
    print_command_lines (uiout, b->commands, 0);
  do_cleanups (list_cleanup);
}
/* APPLE LOCAL end breakpoint MI */


/* Like target_read_memory() but if breakpoints are inserted, return
   the shadow contents instead of the breakpoints themselves.

   Read "memory data" from whatever target or inferior we have. 
   Returns zero if successful, errno value if not.  EIO is used
   for address out of bounds.  If breakpoints are inserted, returns
   shadow contents, not the breakpoints themselves.  From breakpoint.c.  */

int
deprecated_read_memory_nobpt (CORE_ADDR memaddr, gdb_byte *myaddr,
			      unsigned len)
{
  int status;
  struct bp_location *b;
  CORE_ADDR bp_addr = 0;
  int bp_size = 0;

  if (BREAKPOINT_FROM_PC (&bp_addr, &bp_size) == NULL)
    /* No breakpoints on this machine. */
    return target_read_memory (memaddr, myaddr, len);

  ALL_BP_LOCATIONS (b)
  {
    if (b->owner->type == bp_none)
      warning (_("reading through apparently deleted breakpoint #%d?"),
              b->owner->number);

    if (b->loc_type != bp_loc_software_breakpoint)
      continue;
    if (!b->inserted)
      continue;
    /* Addresses and length of the part of the breakpoint that
       we need to copy.  */
    /* XXXX The m68k, sh and h8300 have different local and remote
       breakpoint values.  BREAKPOINT_FROM_PC still manages to
       correctly determine the breakpoints memory address and size
       for these targets. */
    bp_addr = b->address;
    bp_size = 0;
    if (BREAKPOINT_FROM_PC (&bp_addr, &bp_size) == NULL)
      continue;
    if (bp_size == 0)
      /* bp isn't valid */
      continue;
    if (bp_addr + bp_size <= memaddr)
      /* The breakpoint is entirely before the chunk of memory we
         are reading.  */
      continue;
    if (bp_addr >= memaddr + len)
      /* The breakpoint is entirely after the chunk of memory we are
         reading. */
      continue;
    /* Copy the breakpoint from the shadow contents, and recurse for
       the things before and after.  */
    {
      /* Offset within shadow_contents.  */
      int bptoffset = 0;

      if (bp_addr < memaddr)
	{
	  /* Only copy the second part of the breakpoint.  */
	  bp_size -= memaddr - bp_addr;
	  bptoffset = memaddr - bp_addr;
	  bp_addr = memaddr;
	}

      if (bp_addr + bp_size > memaddr + len)
	{
	  /* Only copy the first part of the breakpoint.  */
	  bp_size -= (bp_addr + bp_size) - (memaddr + len);
	}

      memcpy (myaddr + bp_addr - memaddr,
	      b->shadow_contents + bptoffset, bp_size);

      if (bp_addr > memaddr)
	{
	  /* Copy the section of memory before the breakpoint.  */
	  status = deprecated_read_memory_nobpt (memaddr, myaddr, bp_addr - memaddr);
	  if (status != 0)
	    return status;
	}

      if (bp_addr + bp_size < memaddr + len)
	{
	  /* Copy the section of memory after the breakpoint.  */
	  status = deprecated_read_memory_nobpt (bp_addr + bp_size,
				      myaddr + bp_addr + bp_size - memaddr,
				      memaddr + len - (bp_addr + bp_size));
	  if (status != 0)
	    return status;
	}
      return 0;
    }
  }
  /* Nothing overlaps.  Just call read_memory_noerr.  */
  return target_read_memory (memaddr, myaddr, len);
}


/* A wrapper function for inserting catchpoints.  */
static void
insert_catchpoint (struct ui_out *uo, void *args)
{
  struct breakpoint *b = (struct breakpoint *) args;
  /* APPLE LOCAL remove unused local */

  switch (b->type)
    {
    case bp_catch_fork:
      target_insert_fork_catchpoint (PIDGET (inferior_ptid));
      break;
    case bp_catch_vfork:
      target_insert_vfork_catchpoint (PIDGET (inferior_ptid));
      break;
    case bp_catch_exec:
      target_insert_exec_catchpoint (PIDGET (inferior_ptid));
      break;
    default:
      internal_error (__FILE__, __LINE__, _("unknown breakpoint type"));
      break;
    }
}

/* Helper routine: free the value chain for a breakpoint (watchpoint).  */

static void free_valchain (struct bp_location *b)
{
  struct value *v;
  struct value *n;

  /* Free the saved value chain.  We will construct a new one
     the next time the watchpoint is inserted.  */
  for (v = b->owner->val_chain; v; v = n)
    {
      n = value_next (v);
      value_free (v);
    }
  b->owner->val_chain = NULL;
}

/* Insert a low-level "breakpoint" of some type.  BPT is the breakpoint.
   Any error messages are printed to TMP_ERROR_STREAM; and DISABLED_BREAKS,
   PROCESS_WARNING, and HW_BREAKPOINT_ERROR are used to report problems.

   NOTE drow/2003-09-09: This routine could be broken down to an object-style
   method for each breakpoint or catchpoint type.  */
static int
insert_bp_location (struct bp_location *bpt,
		    struct ui_file *tmp_error_stream,
		    int *disabled_breaks, int *process_warning,
		    int *hw_breakpoint_error)
{
  int val = 0;

  /* Permanent breakpoints cannot be inserted or removed.  Disabled
     breakpoints should not be inserted.  */
  if (!breakpoint_enabled (bpt->owner))
    return 0;

  /* APPLE LOCAL: If the objfile is known, but not really resident
     (e.g. we picked it up from a load command, or the inferior has
      been mourned and is no longer in memory), don't try to insert
     a breakpoint.  
     FIXME: Obviously the bp_objfile should be in the bp_location, not
     the breakpoint but it's hardly the only mistake in the separation
     of these two structures. 
     NB: Watchpoints have a null bp_objfile because their expression may
     not resolve to a single address -- handling all the setting and removing
     of watchpoints is not easy, so I'm punting for now and inserting them
     like they've always been inserted.  jsm/2005-01-04 

     One more sophistication, if we have NO objfile, but we do have an
     address, that means that the breakpoint was set as a *0x?????
     type breakpoint, and in that case we should just go ahead and set
     it.  The is mostly for CodeWarrior, where they pass address
     breakpoints, but turn off gdb's parsing of CFM load info.  */

  if (bpt->loc_type != bp_loc_hardware_watchpoint
      && !(target_check_is_objfile_loaded (bpt->owner->bp_objfile)
	  || (bpt->address != 0x0 && bpt->owner->bp_objfile == NULL)))
    {
      bpt->inserted = 0;
      return 0;
    }

  if (bpt->inserted || bpt->duplicate)
    return 0;

  if (bpt->loc_type == bp_loc_software_breakpoint
      || bpt->loc_type == bp_loc_hardware_breakpoint)
    {
      /* First check to see if we have to handle an overlay.  */
      if (overlay_debugging == ovly_off
	  || bpt->section == NULL
	  || !(section_is_overlay (bpt->section)))
	{
	  /* No overlay handling: just set the breakpoint.  */

	  if (bpt->loc_type == bp_loc_hardware_breakpoint)
	    val = target_insert_hw_breakpoint (bpt->address, 
					       bpt->shadow_contents);
	  else
	    val = target_insert_breakpoint (bpt->address,
					    bpt->shadow_contents);
	}
      else
	{
	  /* This breakpoint is in an overlay section.  
	     Shall we set a breakpoint at the LMA?  */
	  if (!overlay_events_enabled)
	    {
	      /* Yes -- overlay event support is not active, 
		 so we must try to set a breakpoint at the LMA.
		 This will not work for a hardware breakpoint.  */
	      if (bpt->loc_type == bp_loc_hardware_breakpoint)
		warning (_("hardware breakpoint %d not supported in overlay!"),
			 bpt->owner->number);
	      else
		{
		  CORE_ADDR addr = overlay_unmapped_address (bpt->address,
							     bpt->section);
		  /* Set a software (trap) breakpoint at the LMA.  */
		  val = target_insert_breakpoint (addr, bpt->shadow_contents);
		  if (val != 0)
		    fprintf_unfiltered (tmp_error_stream, 
					"Overlay breakpoint %d failed: in ROM?", 
					bpt->owner->number);
		}
	    }
	  /* Shall we set a breakpoint at the VMA? */
	  if (section_is_mapped (bpt->section))
	    {
	      /* Yes.  This overlay section is mapped into memory.  */
	      if (bpt->loc_type == bp_loc_hardware_breakpoint)
		val = target_insert_hw_breakpoint (bpt->address, 
						   bpt->shadow_contents);
	      else
		val = target_insert_breakpoint (bpt->address,
						bpt->shadow_contents);
	    }
	  else
	    {
	      /* No.  This breakpoint will not be inserted.  
		 No error, but do not mark the bp as 'inserted'.  */
	      return 0;
	    }
	}

      if (val)
	{
	  /* Can't set the breakpoint.  */
	  if (
#if defined (DISABLE_UNSETTABLE_BREAK)
	      DISABLE_UNSETTABLE_BREAK (bpt->address)
#else
	      solib_address (bpt->address)
#endif
	      )
	    {
	      /* See also: disable_breakpoints_in_shlibs. */
	      val = 0;
	      bpt->owner->enable_state = bp_shlib_disabled;
	      if (!*disabled_breaks)
		{
		  /* APPLE LOCAL begin breakpoint notices on one line */
		  /* Don't emit the breakpoint notices one per line.
		     We sometimes end up with lots of them, and the
		     spam is annoying.  */
		  /* APPLE LOCAL end breakpoint notices on one line */
		  fprintf_unfiltered (tmp_error_stream, 
				      "Temporarily disabling shared library breakpoints:\n");
		}
	      *disabled_breaks = 1;
	      fprintf_unfiltered (tmp_error_stream,
				  /* APPLE LOCAL breakpoint notices on one line */
				  " %d", bpt->owner->number);
	    }
	  else
	    {
#ifdef ONE_PROCESS_WRITETEXT
	      *process_warning = 1;
#endif
	      if (bpt->loc_type == bp_loc_hardware_breakpoint)
		{
		  *hw_breakpoint_error = 1;
		  fprintf_unfiltered (tmp_error_stream, 
				      "Cannot insert hardware breakpoint %d.\n",
				      bpt->owner->number);
		}
	      else
		{
		  fprintf_unfiltered (tmp_error_stream, 
				      "Cannot insert breakpoint %d.\n", 
				      bpt->owner->number);
		  fprintf_filtered (tmp_error_stream, 
				    "Error accessing memory address ");
		  deprecated_print_address_numeric (bpt->address, 1, tmp_error_stream);
		  fprintf_filtered (tmp_error_stream, ": %s.\n",
				    safe_strerror (val));
		}

	    }
	}
      else
	bpt->inserted = 1;

      return val;
    }

  else if (bpt->loc_type == bp_loc_hardware_watchpoint
	   /* NOTE drow/2003-09-08: This state only exists for removing
	      watchpoints.  It's not clear that it's necessary... */
	   && bpt->owner->disposition != disp_del_at_next_stop)
    {
      /* FIXME drow/2003-09-08: This code sets multiple hardware watchpoints
	 based on the expression.  Ideally this should happen at a higher level,
	 and there should be one bp_location for each computed address we
	 must watch.  As soon as a many-to-one mapping is available I'll
	 convert this.  */

      int within_current_scope;
      struct value *mark = value_mark ();
      struct value *v;
      struct frame_id saved_frame_id;

      /* Save the current frame's ID so we can restore it after
	 evaluating the watchpoint expression on its own frame.  */
      /* FIXME drow/2003-09-09: It would be nice if evaluate_expression
	 took a frame parameter, so that we didn't have to change the
	 selected frame.  */
      saved_frame_id = get_frame_id (deprecated_selected_frame);

      /* Determine if the watchpoint is within scope.  */
      if (bpt->owner->exp_valid_block == NULL)
	within_current_scope = 1;
      else
	{
	  struct frame_info *fi;
	  fi = frame_find_by_id (bpt->owner->watchpoint_frame);
	  within_current_scope = (fi != NULL);
	  if (within_current_scope)
	    select_frame (fi);
	}

      if (within_current_scope)
	{
	  free_valchain (bpt);

	  /* Evaluate the expression and cut the chain of values
	     produced off from the value chain.

	     Make sure the value returned isn't lazy; we use
	     laziness to determine what memory GDB actually needed
	     in order to compute the value of the expression.  */
	  v = evaluate_expression (bpt->owner->exp);
	  value_contents (v);
	  value_release_to_mark (mark);

	  bpt->owner->val_chain = v;
	  bpt->inserted = 1;

	  /* Look at each value on the value chain.  */
	  for (; v; v = value_next (v))
	    {
	      /* If it's a memory location, and GDB actually needed
		 its contents to evaluate the expression, then we
		 must watch it.  */
	      if (VALUE_LVAL (v) == lval_memory
		  && ! value_lazy (v))
		{
		  struct type *vtype = check_typedef (value_type (v));

		  /* We only watch structs and arrays if user asked
		     for it explicitly, never if they just happen to
		     appear in the middle of some value chain.  */
		  if (v == bpt->owner->val_chain
		      || (TYPE_CODE (vtype) != TYPE_CODE_STRUCT
			  && TYPE_CODE (vtype) != TYPE_CODE_ARRAY))
		    {
		      CORE_ADDR addr;
		      int len, type;

		      addr = VALUE_ADDRESS (v) + value_offset (v);
		      len = TYPE_LENGTH (value_type (v));
		      type = hw_write;
		      if (bpt->owner->type == bp_read_watchpoint)
			type = hw_read;
		      else if (bpt->owner->type == bp_access_watchpoint)
			type = hw_access;

		      val = target_insert_watchpoint (addr, len, type);
		      if (val == -1)
			{
			  /* Don't exit the loop, try to insert
			     every value on the value chain.  That's
			     because we will be removing all the
			     watches below, and removing a
			     watchpoint we didn't insert could have
			     adverse effects.  */
			  bpt->inserted = 0;
			}
		      val = 0;
		    }
		}
	    }
	  /* Failure to insert a watchpoint on any memory value in the
	     value chain brings us here.  */
	  if (!bpt->inserted)
	    {
	      remove_breakpoint (bpt, mark_uninserted);
	      *hw_breakpoint_error = 1;
	      fprintf_unfiltered (tmp_error_stream,
				  "Could not insert hardware watchpoint %d.\n", 
				  bpt->owner->number);
	      val = -1;
	    }               
	}
      else
	{
	  printf_filtered (_("\
Hardware watchpoint %d deleted because the program has left the block \n\
in which its expression is valid.\n"),
			   bpt->owner->number);
	  if (bpt->owner->related_breakpoint)
	    bpt->owner->related_breakpoint->disposition = disp_del_at_next_stop;
	  bpt->owner->disposition = disp_del_at_next_stop;
	}

      /* Restore the selected frame.  */
      select_frame (frame_find_by_id (saved_frame_id));

      return val;
    }

  else if (ep_is_exception_catchpoint (bpt->owner))
    {
      /* FIXME drow/2003-09-09: This code sets both a catchpoint and a
	 breakpoint.  Once again, it would be better if this was represented
	 as two bp_locations.  */

      /* If we get here, we must have a callback mechanism for exception
	 events -- with g++ style embedded label support, we insert
	 ordinary breakpoints and not catchpoints. */
      val = target_insert_breakpoint (bpt->address, bpt->shadow_contents);
      if (val)
	{
	  /* Couldn't set breakpoint for some reason */
	  fprintf_unfiltered (tmp_error_stream, 
			      "Cannot insert catchpoint %d; disabling it.\n",
			      bpt->owner->number);
	  fprintf_filtered (tmp_error_stream, 
			    "Error accessing memory address ");
	  deprecated_print_address_numeric (bpt->address, 1, tmp_error_stream);
	  fprintf_filtered (tmp_error_stream, ": %s.\n",
			    safe_strerror (val));
	  bpt->owner->enable_state = bp_disabled;
	}
      else
	{
	  /* Bp set, now make sure callbacks are enabled */
	  /* Format possible error msg */
	  char *message = xstrprintf ("Error inserting catchpoint %d:\n",
				      bpt->owner->number);
	  struct cleanup *cleanups = make_cleanup (xfree, message);
	  int val;
	  args_for_catchpoint_enable args;
	  args.kind = bpt->owner->type == bp_catch_catch ? 
	    EX_EVENT_CATCH : EX_EVENT_THROW;
	  args.enable_p = 1;
	  val = catch_errors (cover_target_enable_exception_callback,
			      &args, message, RETURN_MASK_ALL);
	  do_cleanups (cleanups);
	  if (val != 0 && val != -1)
	    bpt->inserted = 1;

	  /* Check if something went wrong; val == 0 can be ignored */
	  if (val == -1)
	    {
	      /* something went wrong */
	      fprintf_unfiltered (tmp_error_stream, 
				  "Cannot insert catchpoint %d; disabling it.\n",
				  bpt->owner->number);
	      bpt->owner->enable_state = bp_disabled;
	    }
	}

      return val;
    }

  else if (bpt->owner->type == bp_catch_fork
	   || bpt->owner->type == bp_catch_vfork
	   || bpt->owner->type == bp_catch_exec)
    {
      struct gdb_exception e = catch_exception (uiout, insert_catchpoint,
						bpt->owner, RETURN_MASK_ERROR);
      exception_fprintf (gdb_stderr, e, "warning: inserting catchpoint %d: ",
			 bpt->owner->number);
      if (e.reason < 0)
	bpt->owner->enable_state = bp_disabled;
      else
	bpt->inserted = 1;

      /* We've already printed an error message if there was a problem
	 inserting this catchpoint, and we've disabled the catchpoint,
	 so just return success.  */
      return 0;
    }

  return 0;
}

/* insert_breakpoints is used when starting or continuing the program.
   remove_breakpoints is used when the program stops.
   Both return zero if successful,
   or an `errno' value if could not write the inferior.  */

int
insert_breakpoints (void)
{
  struct bp_location *b, *temp;
  int return_val = 0;	/* return success code. */
  int val = 0;
  int disabled_breaks = 0;
  int hw_breakpoint_error = 0;
  int process_warning = 0;
  /* APPLE LOCAL: This is to check watchpoint setting returns.  */
  int retval;

  struct ui_file *tmp_error_stream = mem_fileopen ();
  make_cleanup_ui_file_delete (tmp_error_stream);

  /* Explicitly mark the warning -- this will only be printed if
     there was an error.  */
  fprintf_unfiltered (tmp_error_stream, "Warning:\n");

  /* APPLE LOCAL: We update the breakpoints here, just before we insert
     them.  Of course, if nothing in the symbol world has changed, this
     does no work.  */
  breakpoint_update ();
  /* END APPLE LOCAL */

  ALL_BP_LOCATIONS_SAFE (b, temp)
    {
      /* Permanent breakpoints cannot be inserted or removed.  Disabled
	 breakpoints should not be inserted.  */
      if (!breakpoint_enabled (b->owner))
	continue;
      
      /* APPLE LOCAL: The watchpoint code will set this to 0 if there's an error.  */
      retval = 1;

      /* FIXME drow/2003-10-07: This code should be pushed elsewhere when
	 hardware watchpoints are split into multiple loc breakpoints.  */
      if ((b->loc_type == bp_loc_hardware_watchpoint
	   || b->owner->type == bp_watchpoint) && !b->owner->val)
	{
	  struct value *val;

	  val = evaluate_expression (b->owner->exp);
	  release_value (val);
	  /* APPLE LOCAL: Don't throw an error here if we can't read
	     memory at the breakpoint site.  If we do this when starting
	     up the target, we just stall.  */
	  if (value_lazy (val))
	    {
	      retval = gdb_value_fetch_lazy (val);
	      if (retval == 0)
		warning ("Could not set watchpoint %d", b->owner->number);
	      else
		b->owner->val = val;
	    }
	}

      if (retval)
	{
	  val = insert_bp_location (b, tmp_error_stream,
				    &disabled_breaks, &process_warning,
				    &hw_breakpoint_error);
	  if (val)
	    return_val = val;
	}
    }
  /* APPLE LOCAL begin breakpoint notices on one line */
  /* We changed the printing code to emit all the disabled breakpoints
     on one line, so we have to wrap it here.  */
  if (disabled_breaks)
    fprintf_unfiltered (tmp_error_stream, "\n");
  /* APPLE LOCAL end breakpoint notices on one line */

  if (return_val)
    {
      /* If a hardware breakpoint or watchpoint was inserted, add a
         message about possibly exhausted resources.  */
      if (hw_breakpoint_error)
	{
	  fprintf_unfiltered (tmp_error_stream, 
			      "Could not insert hardware breakpoints:\n\
You may have requested too many hardware breakpoints/watchpoints.\n");
	}
#ifdef ONE_PROCESS_WRITETEXT
      if (process_warning)
	fprintf_unfiltered (tmp_error_stream,
			    "The same program may be running in another process.");
#endif
      target_terminal_ours_for_output ();
      /* APPLE LOCAL begin breakpoints */
      /* The code in the FSF version was (I think errantly) changed to
         throw an error here. Pretty much none of the code that calls
         insert_breakpoints is prepared to handle the errors, however.
         I am reverting to the old behavior of just dumping the error,
         and then returning the error code upstream.  This seems to
         work much better.

	 The cast here is stupid, but no more stupid than the
         "do_write" function that error_stream uses in utils.c...
      */
      ui_file_put (tmp_error_stream, (ui_file_put_method_ftype *) ui_file_write, 
		   gdb_stderr);
      /* APPLE LOCAL end breakpoints */
    }
  return return_val;
}

int
remove_breakpoints (void)
{
  struct bp_location *b;
  int retval = 0;

  ALL_BP_LOCATIONS (b)
  {
    if (b->inserted)
      {
	int val;
	val = remove_breakpoint (b, mark_uninserted);
	/* APPLE LOCAL: Just because you can't remove
	   one breakpoint, don't fail to remove all the
	   others...  If you do this while single-stepping
	   over a trap, you'll just end up hitting the trap
	   over and over...  */
	if (val != 0)
	  {
	    warning ("Could not remove breakpoint at \"0x%s\".", paddr_nz (b->address));
	    retval = val;
	  }
      }
  }
  return retval;
}

int
remove_hw_watchpoints (void)
{
  struct bp_location *b;
  int val;

  ALL_BP_LOCATIONS (b)
  {
    if (b->inserted && b->loc_type == bp_loc_hardware_watchpoint)
      {
	val = remove_breakpoint (b, mark_uninserted);
	if (val != 0)
	  return val;
      }
  }
  return 0;
}

int
reattach_breakpoints (int pid)
{
  struct bp_location *b;
  int val;
  struct cleanup *old_chain = save_inferior_ptid ();

  /* Set inferior_ptid; remove_breakpoint uses this global.  */
  inferior_ptid = pid_to_ptid (pid);
  ALL_BP_LOCATIONS (b)
  {
    if (b->inserted)
      {
	remove_breakpoint (b, mark_inserted);
	if (b->loc_type == bp_loc_hardware_breakpoint)
	  val = target_insert_hw_breakpoint (b->address, b->shadow_contents);
	else
	  val = target_insert_breakpoint (b->address, b->shadow_contents);
	/* FIXME drow/2003-10-07: This doesn't handle any other kinds of
	   breakpoints.  It's wrong for watchpoints, for example.  */
	if (val != 0)
	  {
	    do_cleanups (old_chain);
	    return val;
	  }
      }
  }
  do_cleanups (old_chain);
  return 0;
}

void
update_breakpoints_after_exec (void)
{
  struct breakpoint *b;
  struct breakpoint *temp;

  /* Doing this first prevents the badness of having delete_breakpoint()
     write a breakpoint's current "shadow contents" to lift the bp.  That
     shadow is NOT valid after an exec()! */
  mark_breakpoints_out ();

  ALL_BREAKPOINTS_SAFE (b, temp)
  {
    /* Solib breakpoints must be explicitly reset after an exec(). */
    if (b->type == bp_shlib_event)
      {
	delete_breakpoint (b);
	continue;
      }

    /* Thread event breakpoints must be set anew after an exec(),
       as must overlay event breakpoints.  */
    if (b->type == bp_thread_event || b->type == bp_overlay_event)
      {
	delete_breakpoint (b);
	continue;
      }

    /* Step-resume breakpoints are meaningless after an exec(). */
    if (b->type == bp_step_resume)
      {
	delete_breakpoint (b);
	continue;
      }

    /* Ditto the sigtramp handler breakpoints. */
    if (b->type == bp_through_sigtramp)
      {
	delete_breakpoint (b);
	continue;
      }

    /* Ditto the exception-handling catchpoints. */
    if ((b->type == bp_catch_catch) || (b->type == bp_catch_throw))
      {
	delete_breakpoint (b);
	continue;
      }

    /* Don't delete an exec catchpoint, because else the inferior
       won't stop when it ought!

       Similarly, we probably ought to keep vfork catchpoints, 'cause
       on this target, we may not be able to stop when the vfork is
       seen, but only when the subsequent exec is seen.  (And because
       deleting fork catchpoints here but not vfork catchpoints will
       seem mysterious to users, keep those too.)

       ??rehrauer: Let's hope that merely clearing out this catchpoint's
       target address field, if any, is sufficient to have it be reset
       automagically.  Certainly on HP-UX that's true.

       Jim Blandy <jimb@redhat.com>: Actually, zero is a perfectly
       valid code address on some platforms (like the mn10300
       simulators).  We shouldn't assign any special interpretation to
       a breakpoint with a zero address.  And in fact, GDB doesn't ---
       I can't see what that comment above is talking about.  As far
       as I can tell, setting the address of a
       bp_catch_exec/bp_catch_vfork/bp_catch_fork breakpoint to zero
       is meaningless, since those are implemented with HP-UX kernel
       hackery, not by storing breakpoint instructions somewhere.  */
    if ((b->type == bp_catch_exec) ||
	(b->type == bp_catch_vfork) ||
	(b->type == bp_catch_fork))
      {
	b->loc->address = (CORE_ADDR) NULL;
	continue;
      }

    /* bp_finish is a special case.  The only way we ought to be able
       to see one of these when an exec() has happened, is if the user
       caught a vfork, and then said "finish".  Ordinarily a finish just
       carries them to the call-site of the current callee, by setting
       a temporary bp there and resuming.  But in this case, the finish
       will carry them entirely through the vfork & exec.

       We don't want to allow a bp_finish to remain inserted now.  But
       we can't safely delete it, 'cause finish_command has a handle to
       the bp on a bpstat, and will later want to delete it.  There's a
       chance (and I've seen it happen) that if we delete the bp_finish
       here, that its storage will get reused by the time finish_command
       gets 'round to deleting the "use to be a bp_finish" breakpoint.
       We really must allow finish_command to delete a bp_finish.

       In the absense of a general solution for the "how do we know
       it's safe to delete something others may have handles to?"
       problem, what we'll do here is just uninsert the bp_finish, and
       let finish_command delete it.

       (We know the bp_finish is "doomed" in the sense that it's
       momentary, and will be deleted as soon as finish_command sees
       the inferior stopped.  So it doesn't matter that the bp's
       address is probably bogus in the new a.out, unlike e.g., the
       solib breakpoints.)  */

    if (b->type == bp_finish)
      {
	continue;
      }

    /* Without a symbolic address, we have little hope of the
       pre-exec() address meaning the same thing in the post-exec()
       a.out. */
    if (b->addr_string == NULL)
      {
	delete_breakpoint (b);
	continue;
      }

    /* If this breakpoint has survived the above battery of checks, then
       it must have a symbolic address.  Be sure that it gets reevaluated
       to a target address, rather than reusing the old evaluation.

       Jim Blandy <jimb@redhat.com>: As explained above in the comment
       for bp_catch_exec and friends, I'm pretty sure this is entirely
       unnecessary.  A call to breakpoint_re_set_one always recomputes
       the breakpoint's address from scratch, or deletes it if it can't.
       So I think this assignment could be deleted without effect.  */
    b->loc->address = (CORE_ADDR) NULL;
  }
  /* FIXME what about longjmp breakpoints?  Re-create them here?  */
  create_overlay_event_breakpoint ("_ovly_debug_event");
}

int
detach_breakpoints (int pid)
{
  struct bp_location *b;
  int val;
  struct cleanup *old_chain = save_inferior_ptid ();

  if (pid == PIDGET (inferior_ptid))
    error (_("Cannot detach breakpoints of inferior_ptid"));

  /* Set inferior_ptid; remove_breakpoint uses this global.  */
  inferior_ptid = pid_to_ptid (pid);
  ALL_BP_LOCATIONS (b)
  {
    if (b->inserted)
      {
	val = remove_breakpoint (b, mark_inserted);
	if (val != 0)
	  {
	    do_cleanups (old_chain);
	    return val;
	  }
      }
  }
  do_cleanups (old_chain);
  return 0;
}

static int
remove_breakpoint (struct bp_location *b, insertion_state_t is)
{
  int val;

  if (b->owner->enable_state == bp_permanent)
    /* Permanent breakpoints cannot be inserted or removed.  */
    return 0;

  if (b->owner->type == bp_none)
    warning (_("attempted to remove apparently deleted breakpoint #%d?"), 
	     b->owner->number);

  if (b->loc_type == bp_loc_software_breakpoint
      || b->loc_type == bp_loc_hardware_breakpoint)
    {
      /* "Normal" instruction breakpoint: either the standard
	 trap-instruction bp (bp_breakpoint), or a
	 bp_hardware_breakpoint.  */

      /* First check to see if we have to handle an overlay.  */
      if (overlay_debugging == ovly_off
	  || b->section == NULL
	  || !(section_is_overlay (b->section)))
	{
	  /* No overlay handling: just remove the breakpoint.  */

	  if (b->loc_type == bp_loc_hardware_breakpoint)
	    val = target_remove_hw_breakpoint (b->address, 
					       b->shadow_contents);
	  else
	    val = target_remove_breakpoint (b->address, b->shadow_contents);
	}
      else
	{
	  /* This breakpoint is in an overlay section.  
	     Did we set a breakpoint at the LMA?  */
	  if (!overlay_events_enabled)
	      {
		/* Yes -- overlay event support is not active, so we
		   should have set a breakpoint at the LMA.  Remove it.  
		*/
		CORE_ADDR addr = overlay_unmapped_address (b->address, 
							   b->section);
		/* Ignore any failures: if the LMA is in ROM, we will
		   have already warned when we failed to insert it.  */
		if (b->loc_type == bp_loc_hardware_breakpoint)
		  target_remove_hw_breakpoint (addr, b->shadow_contents);
		else
		  target_remove_breakpoint (addr, b->shadow_contents);
	      }
	  /* Did we set a breakpoint at the VMA? 
	     If so, we will have marked the breakpoint 'inserted'.  */
	  if (b->inserted)
	    {
	      /* Yes -- remove it.  Previously we did not bother to
		 remove the breakpoint if the section had been
		 unmapped, but let's not rely on that being safe.  We
		 don't know what the overlay manager might do.  */
	      if (b->loc_type == bp_loc_hardware_breakpoint)
		val = target_remove_hw_breakpoint (b->address, 
						   b->shadow_contents);
	      else
		val = target_remove_breakpoint (b->address,
						b->shadow_contents);
	    }
	  else
	    {
	      /* No -- not inserted, so no need to remove.  No error.  */
	      val = 0;
	    }
	}
      if (val)
	return val;
      b->inserted = (is == mark_inserted);
    }
  else if (b->loc_type == bp_loc_hardware_watchpoint
	   && breakpoint_enabled (b->owner)
	   && !b->duplicate)
    {
      struct value *v;
      /* APPLE LOCAL remove unused local */

      b->inserted = (is == mark_inserted);
      /* Walk down the saved value chain.  */
      for (v = b->owner->val_chain; v; v = value_next (v))
	{
	  /* For each memory reference remove the watchpoint
	     at that address.  */
	  if (VALUE_LVAL (v) == lval_memory
	      && ! value_lazy (v))
	    {
	      struct type *vtype = check_typedef (value_type (v));

	      if (v == b->owner->val_chain
		  || (TYPE_CODE (vtype) != TYPE_CODE_STRUCT
		      && TYPE_CODE (vtype) != TYPE_CODE_ARRAY))
		{
		  CORE_ADDR addr;
		  int len, type;

		  addr = VALUE_ADDRESS (v) + value_offset (v);
		  len = TYPE_LENGTH (value_type (v));
		  type   = hw_write;
		  if (b->owner->type == bp_read_watchpoint)
		    type = hw_read;
		  else if (b->owner->type == bp_access_watchpoint)
		    type = hw_access;

		  val = target_remove_watchpoint (addr, len, type);
		  if (val == -1)
		    b->inserted = 1;
		  val = 0;
		}
	    }
	}
      /* Failure to remove any of the hardware watchpoints comes here.  */
      if ((is == mark_uninserted) && (b->inserted))
	warning (_("Could not remove hardware watchpoint %d."),
		 b->owner->number);
    }
  else if ((b->owner->type == bp_catch_fork ||
	    b->owner->type == bp_catch_vfork ||
	    b->owner->type == bp_catch_exec)
	   && breakpoint_enabled (b->owner)
	   && !b->duplicate)
    {
      val = -1;
      switch (b->owner->type)
	{
	case bp_catch_fork:
	  val = target_remove_fork_catchpoint (PIDGET (inferior_ptid));
	  break;
	case bp_catch_vfork:
	  val = target_remove_vfork_catchpoint (PIDGET (inferior_ptid));
	  break;
	case bp_catch_exec:
	  val = target_remove_exec_catchpoint (PIDGET (inferior_ptid));
	  break;
	default:
	  warning (_("Internal error, %s line %d."), __FILE__, __LINE__);
	  break;
	}
      if (val)
	return val;
      b->inserted = (is == mark_inserted);
    }
  else if ((b->owner->type == bp_catch_catch ||
	    b->owner->type == bp_catch_throw)
	   && breakpoint_enabled (b->owner)
	   && !b->duplicate)
    {

      val = target_remove_breakpoint (b->address, b->shadow_contents);
      if (val)
	return val;
      b->inserted = (is == mark_inserted);
    }
  else if (ep_is_exception_catchpoint (b->owner)
	   && b->inserted	/* sometimes previous insert doesn't happen */
	   && breakpoint_enabled (b->owner)
	   && !b->duplicate)
    {

      val = target_remove_breakpoint (b->address, b->shadow_contents);
      if (val)
	return val;

      b->inserted = (is == mark_inserted);
    }

  return 0;
}

/* Clear the "inserted" flag in all breakpoints.  */

void
mark_breakpoints_out (void)
{
  struct bp_location *bpt;

  ALL_BP_LOCATIONS (bpt)
    bpt->inserted = 0;
}

/* Clear the "inserted" flag in all breakpoints and delete any
   breakpoints which should go away between runs of the program.

   Plus other such housekeeping that has to be done for breakpoints
   between runs.

   Note: this function gets called at the end of a run (by
   generic_mourn_inferior) and when a run begins (by
   init_wait_for_inferior). */



void
breakpoint_init_inferior (enum inf_context context)
{
  struct breakpoint *b, *temp;
  struct bp_location *bpt;
  static int warning_needed = 0;

  ALL_BP_LOCATIONS (bpt)
    bpt->inserted = 0;

  ALL_BREAKPOINTS_SAFE (b, temp)
  {
    switch (b->type)
      {
      case bp_call_dummy:
      case bp_watchpoint_scope:

	/* If the call dummy breakpoint is at the entry point it will
	   cause problems when the inferior is rerun, so we better
	   get rid of it. 

	   Also get rid of scope breakpoints.  */
	delete_breakpoint (b);
	break;

      case bp_watchpoint:
      case bp_hardware_watchpoint:
      case bp_read_watchpoint:
      case bp_access_watchpoint:

	/* Likewise for watchpoints on local expressions.  */
	if (b->exp_valid_block != NULL)
	  delete_breakpoint (b);
	if (context == inf_starting) 
	  {
	    /* Reset val field to force reread of starting value
	       in insert_breakpoints.  */
	    if (b->val)
	      value_free (b->val);
	    b->val = NULL;
	  }
	break;
      default:
	/* Likewise for exception catchpoints in dynamic-linked
	   executables where required */
	if (ep_is_exception_catchpoint (b)
	    && deprecated_exception_catchpoints_are_fragile)
	  {
	    warning_needed = 1;
	    delete_breakpoint (b);
	  }
	break;
      }
  }

  if (deprecated_exception_catchpoints_are_fragile)
    deprecated_exception_support_initialized = 0;

  /* Don't issue the warning unless it's really needed... */
  if (warning_needed && (context != inf_exited))
    {
      warning (_("Exception catchpoints from last run were deleted.\n"
		 "You must reinsert them explicitly."));
      warning_needed = 0;
    }
}

/* breakpoint_here_p (PC) returns non-zero if an enabled breakpoint
   exists at PC.  It returns ordinary_breakpoint_here if it's an
   ordinary breakpoint, or permanent_breakpoint_here if it's a
   permanent breakpoint.
   - When continuing from a location with an ordinary breakpoint, we
     actually single step once before calling insert_breakpoints.
   - When continuing from a localion with a permanent breakpoint, we
     need to use the `SKIP_PERMANENT_BREAKPOINT' macro, provided by
     the target, to advance the PC past the breakpoint.  */

enum breakpoint_here
breakpoint_here_p (CORE_ADDR pc)
{
  struct bp_location *bpt;
  int any_breakpoint_here = 0;

  ALL_BP_LOCATIONS (bpt)
    {
      if (bpt->loc_type != bp_loc_software_breakpoint
	  && bpt->loc_type != bp_loc_hardware_breakpoint)
	continue;

      if ((breakpoint_enabled (bpt->owner)
	   || bpt->owner->enable_state == bp_permanent)
	  && bpt->address == pc)	/* bp is enabled and matches pc */
	{
	  if (overlay_debugging 
	      && section_is_overlay (bpt->section) 
	      && !section_is_mapped (bpt->section))
	    continue;		/* unmapped overlay -- can't be a match */
	  else if (bpt->owner->enable_state == bp_permanent)
	    return permanent_breakpoint_here;
	  else
	    any_breakpoint_here = 1;
	}
    }

  return any_breakpoint_here ? ordinary_breakpoint_here : 0;
}


/* breakpoint_inserted_here_p (PC) is just like breakpoint_here_p(),
   but it only returns true if there is actually a breakpoint inserted
   at PC.  */

int
breakpoint_inserted_here_p (CORE_ADDR pc)
{
  struct bp_location *bpt;

  ALL_BP_LOCATIONS (bpt)
    {
      if (bpt->loc_type != bp_loc_software_breakpoint
	  && bpt->loc_type != bp_loc_hardware_breakpoint)
	continue;

      if (bpt->inserted
	  && bpt->address == pc)	/* bp is inserted and matches pc */
	{
	  if (overlay_debugging 
	      && section_is_overlay (bpt->section) 
	      && !section_is_mapped (bpt->section))
	    continue;		/* unmapped overlay -- can't be a match */
	  else
	    return 1;
	}
    }

  return 0;
}

/* This function returns non-zero iff there is a software breakpoint
   inserted at PC.  */

int
software_breakpoint_inserted_here_p (CORE_ADDR pc)
{
  struct bp_location *bpt;
  /* APPLE LOCAL remove unused local var */

  ALL_BP_LOCATIONS (bpt)
    {
      if (bpt->loc_type != bp_loc_software_breakpoint)
	continue;

      if ((breakpoint_enabled (bpt->owner)
	   || bpt->owner->enable_state == bp_permanent)
	  && bpt->inserted
	  && bpt->address == pc)	/* bp is enabled and matches pc */
	{
	  if (overlay_debugging 
	      && section_is_overlay (bpt->section) 
	      && !section_is_mapped (bpt->section))
	    continue;		/* unmapped overlay -- can't be a match */
	  else
	    return 1;
	}
    }

  return 0;
}

/* breakpoint_thread_match (PC, PTID) returns true if the breakpoint at
   PC is valid for process/thread PTID.  */

int
breakpoint_thread_match (CORE_ADDR pc, ptid_t ptid)
{
  struct bp_location *bpt;
  int thread;

  thread = pid_to_thread_id (ptid);

  ALL_BP_LOCATIONS (bpt)
    {
      if (bpt->loc_type != bp_loc_software_breakpoint
	  && bpt->loc_type != bp_loc_hardware_breakpoint)
	continue;

      if ((breakpoint_enabled (bpt->owner)
	   || bpt->owner->enable_state == bp_permanent)
	  && bpt->address == pc
	  && (bpt->owner->thread == -1 || bpt->owner->thread == thread))
	{
	  if (overlay_debugging 
	      && section_is_overlay (bpt->section) 
	      && !section_is_mapped (bpt->section))
	    continue;		/* unmapped overlay -- can't be a match */
	  else
	    return 1;
	}
    }

  return 0;
}


/* bpstat stuff.  External routines' interfaces are documented
   in breakpoint.h.  */

int
ep_is_catchpoint (struct breakpoint *ep)
{
  return
    (ep->type == bp_catch_load)
    || (ep->type == bp_catch_unload)
    || (ep->type == bp_catch_fork)
    || (ep->type == bp_catch_vfork)
    || (ep->type == bp_catch_exec)
    || (ep->type == bp_catch_catch)
    || (ep->type == bp_catch_throw)
    /* APPLE LOCAL: Add the gnu_v3 exception type.  */
    || (ep->type == bp_gnu_v3_catch_catch)
    || (ep->type == bp_gnu_v3_catch_throw);

  /* ??rehrauer: Add more kinds here, as are implemented... */
}

int
ep_is_shlib_catchpoint (struct breakpoint *ep)
{
  return
    (ep->type == bp_catch_load)
    || (ep->type == bp_catch_unload);
}

int
ep_is_exception_catchpoint (struct breakpoint *ep)
{
  return
    (ep->type == bp_catch_catch)
    /* APPLE LOCAL begin gnu v3 */
    || (ep->type == bp_catch_throw)
    || (ep->type == bp_gnu_v3_catch_catch)
    || (ep->type == bp_gnu_v3_catch_throw);
    /* APPLE LOCAL end gnu v3 */
}

/* Clear a bpstat so that it says we are not at any breakpoint.
   Also free any storage that is part of a bpstat.  */

void
bpstat_clear (bpstat *bsp)
{
  bpstat p;
  bpstat q;

  if (bsp == 0)
    return;
  p = *bsp;
  while (p != NULL)
    {
      q = p->next;
      if (p->old_val != NULL)
	value_free (p->old_val);
      free_command_lines (&p->commands);
      xfree (p);
      p = q;
    }
  *bsp = NULL;
}

/* Return a copy of a bpstat.  Like "bs1 = bs2" but all storage that
   is part of the bpstat is copied as well.  */

bpstat
bpstat_copy (bpstat bs)
{
  bpstat p = NULL;
  bpstat tmp;
  bpstat retval = NULL;

  if (bs == NULL)
    return bs;

  for (; bs != NULL; bs = bs->next)
    {
      tmp = (bpstat) xmalloc (sizeof (*tmp));
      memcpy (tmp, bs, sizeof (*tmp));
      if (bs->commands != NULL)
	tmp->commands = copy_command_lines (bs->commands);
      if (bs->old_val != NULL)
	tmp->old_val = value_copy (bs->old_val);
      /* APPLE LOCAL: We want to release the value here, or it might
	 get freed out from underneath us by a value_free_to_mark, or
	 similar. */
      if (tmp->old_val != NULL)
	release_value (tmp->old_val);
      if (p == NULL)
	/* This is the first thing in the chain.  */
	retval = tmp;
      else
	p->next = tmp;
      p = tmp;
    }
  p->next = NULL;
  return retval;
}

/* Find the bpstat associated with this breakpoint */

bpstat
bpstat_find_breakpoint (bpstat bsp, struct breakpoint *breakpoint)
{
  if (bsp == NULL)
    return NULL;

  for (; bsp != NULL; bsp = bsp->next)
    {
      if (bsp->breakpoint_at == breakpoint)
	return bsp;
    }
  return NULL;
}

/* Find a step_resume breakpoint associated with this bpstat.
   (If there are multiple step_resume bp's on the list, this function
   will arbitrarily pick one.)

   It is an error to use this function if BPSTAT doesn't contain a
   step_resume breakpoint.

   See wait_for_inferior's use of this function.  */
struct breakpoint *
bpstat_find_step_resume_breakpoint (bpstat bsp)
{
  int current_thread;

  gdb_assert (bsp != NULL);

  current_thread = pid_to_thread_id (inferior_ptid);

  for (; bsp != NULL; bsp = bsp->next)
    {
      if ((bsp->breakpoint_at != NULL) &&
	  (bsp->breakpoint_at->type == bp_step_resume) &&
	  (bsp->breakpoint_at->thread == current_thread || 
	   bsp->breakpoint_at->thread == -1))
	return bsp->breakpoint_at;
    }

  internal_error (__FILE__, __LINE__, _("No step_resume breakpoint found."));
}


/* Return the breakpoint number of the first breakpoint we are stopped
   at.  *BSP upon return is a bpstat which points to the remaining
   breakpoints stopped at (but which is not guaranteed to be good for
   anything but further calls to bpstat_num).
   Return 0 if passed a bpstat which does not indicate any breakpoints.  */

int
bpstat_num (bpstat *bsp)
{
  struct breakpoint *b;

  if ((*bsp) == NULL)
    return 0;			/* No more breakpoint values */
  else
    {
      b = (*bsp)->breakpoint_at;
      *bsp = (*bsp)->next;
      if (b == NULL)
	return -1;		/* breakpoint that's been deleted since */
      else
	return b->number;	/* We have its number */
    }
}

/* Modify BS so that the actions will not be performed.  */

void
bpstat_clear_actions (bpstat bs)
{
  for (; bs != NULL; bs = bs->next)
    {
      free_command_lines (&bs->commands);
      if (bs->old_val != NULL)
	{
	  value_free (bs->old_val);
	  bs->old_val = NULL;
	}
    }
}

/* Stub for cleaning up our state if we error-out of a breakpoint command */
static void
cleanup_executing_breakpoints (void *ignore)
{
  executing_breakpoint_commands = 0;
}

/* Execute all the commands associated with all the breakpoints at this
   location.  Any of these commands could cause the process to proceed
   beyond this point, etc.  We look out for such changes by checking
   the global "breakpoint_proceeded" after each command.  */

void
bpstat_do_actions (bpstat *bsp)
{
  bpstat bs;
  struct cleanup *old_chain;
  /* APPLE LOCAL: Keep track of whether we ran any commands
     so we can tell the MI that something happened...  */
  int first_command = 0;
  bpstat bs_copy;
  struct cleanup *bpstat_cleanup_chain;


  /* Avoid endless recursion if a `source' command is contained
     in bs->commands.  */
  if (executing_breakpoint_commands)
    return;

  executing_breakpoint_commands = 1;
  old_chain = make_cleanup (cleanup_executing_breakpoints, 0);

top:
  /* Note that (as of this writing), our callers all appear to
     be passing us the address of global stop_bpstat.  And, if
     our calls to execute_control_command cause the inferior to
     proceed, that global (and hence, *bsp) will change.

     We must be careful to not touch *bsp unless the inferior
     has not proceeded. */

  /* APPLE LOCAL: There is code below that is #if 0'ed from the FSF
     version of gdb that attempts to work around the problem of the
     stop_bpstat's commands getting free'd out from under us.  They
     solve this by saving away the just the currently executing
     command tree.  

     But I have seen cases that look very much like the whole bsp gets
     free'd while we are in the loop, and then we crash doing
     bs->next.  Since I don't think copying the whole bpstat is likely
     to be very costly, I do that instead to be safer.  

     FIXME: Should really make sure this isn't over-paranoid at 
     some point.

  */

  if (bsp == NULL)
    {
      do_cleanups (old_chain);
      return;
    }

  bs_copy = bpstat_copy (*bsp);
  bpstat_cleanup_chain = make_cleanup ((void (*) (void *)) bpstat_clear, 
				       &bs_copy);
  bpstat_clear_actions (*bsp);
  /* END APPLE LOCAL */

  breakpoint_proceeded = 0;
  for (bs = bs_copy; bs != NULL; bs = bs->next)
    {
      struct command_line *cmd;

      /* APPLE LOCAL remove dubious ownership code */
      cmd = bs->commands;
      /* APPLE LOCAL remove dubious ownership code */

      while (cmd != NULL)
	{
	  first_command++;

	  execute_control_command (cmd);

	  if (breakpoint_proceeded)
	    break;
	  else
	    cmd = cmd->next;
	}

      /* APPLE LOCAL remove dubious cleanup */

      if (breakpoint_proceeded)
	/* The inferior is proceeded by the command; bomb out now.
	   The bpstat chain has been blown away by wait_for_inferior.
	   But since execution has stopped again, there is a new bpstat
	   to look at, so start over.  */

	/* APPLE LOCAL:
	  If the target is asynchronous, and the event loop is running,
           then we haven't fetched any new events yet, so the inferior
           is just running away.  Let it go, and we will hit a breakpoint
           sometime later.

           One more (unfortunate) bit here is that the mi interpreter
           registers a continuation for this event through the mi_cmd_interpreter_exec
           so if we do it here, then we will get the same basic action registered
           twice.  FIXME: Maybe better would be to tell the invocation in
           execute_control_command that this is a breakpoint command, and so
           should not register its own handler.
        */
        {
          if (target_can_async_p() && target_executing)
            {
              if (!ui_out_is_mi_like_p (uiout))
                add_continuation (async_breakpoint_command_continuation, NULL);
              break;
            }
          else
            {
              /* We are going back to the top, and making a copy of
                 the new stop_bpstat, so we have to clear our old copy
                 first.  */
              do_cleanups (bpstat_cleanup_chain);
              goto top;
            }
        }

      /* END APPLE LOCAL */
    }

  /* APPLE LOCAL: We should tell the UI that we are done with breakpoint
     commands so that it knows it is now safe to display the stopped state.  */
  if (!breakpoint_proceeded && first_command && ui_out_is_mi_like_p (uiout))
    {
      struct cleanup *notify_cleanup =
	make_cleanup_ui_out_notify_begin_end (uiout, "breakpoint-command-completed");
      do_cleanups (notify_cleanup);
    }
  /* END APPLE LOCAL */

  do_cleanups (old_chain);
}

/* APPLE LOCAL: This continuation is necessary for breakpoint commands
   that run the target when the target is asynchronous, since we have to
   leave bpstat_do_actions to get back to the event loop and notice that
   we have stopped again.  */

static void
async_breakpoint_command_continuation (struct continuation_arg *arg)
{
  bpstat_do_actions (&stop_bpstat);
}

/* This is the normal print function for a bpstat.  In the future,
   much of this logic could (should?) be moved to bpstat_stop_status,
   by having it set different print_it values.

   Current scheme: When we stop, bpstat_print() is called.  It loops
   through the bpstat list of things causing this stop, calling the
   print_bp_stop_message function on each one. The behavior of the
   print_bp_stop_message function depends on the print_it field of
   bpstat. If such field so indicates, call this function here.

   Return values from this routine (ultimately used by bpstat_print()
   and normal_stop() to decide what to do): 
   PRINT_NOTHING: Means we already printed all we needed to print,
   don't print anything else.
   PRINT_SRC_ONLY: Means we printed something, and we do *not* desire
   that something to be followed by a location.
   PRINT_SCR_AND_LOC: Means we printed something, and we *do* desire
   that something to be followed by a location.
   PRINT_UNKNOWN: Means we printed nothing or we need to do some more
   analysis.  */

static enum print_stop_action
print_it_typical (bpstat bs)
{
  struct cleanup *old_chain, *ui_out_chain;
  struct ui_stream *stb;
  stb = ui_out_stream_new (uiout);
  old_chain = make_cleanup_ui_out_stream_delete (stb);
  /* bs->breakpoint_at can be NULL if it was a momentary breakpoint
     which has since been deleted.  */
  if (bs->breakpoint_at == NULL)
    return PRINT_UNKNOWN;

  switch (bs->breakpoint_at->type)
    {
    case bp_breakpoint:
    /* APPLE LOCAL begin subroutine inlining  */
    case bp_inlined_breakpoint:
    /* APPLE LOCAL end subroutine inlining  */
    case bp_hardware_breakpoint:
      if (bs->breakpoint_at->loc->address != bs->breakpoint_at->loc->requested_address)
	breakpoint_adjustment_warning (bs->breakpoint_at->loc->requested_address,
	                               bs->breakpoint_at->loc->address,
				       bs->breakpoint_at->number, 1);
      annotate_breakpoint (bs->breakpoint_at->number);
      ui_out_text (uiout, "\nBreakpoint ");
	  
      ui_out_print_annotation_string (uiout, 0, "reason", async_reason_lookup (EXEC_ASYNC_BREAKPOINT_HIT));
	  
      if (ui_out_is_mi_like_p (uiout))
	{
	  bpstat bpstat_ptr;
	  int any_commands = 0;
	  /* APPLE LOCAL: Print out whether the breakpoint has any
	     commands, so the UI will know whether to expect something
	     funny to happen.  */
	  for (bpstat_ptr = bs; 
               bpstat_ptr != NULL; 
               bpstat_ptr = bpstat_ptr->next)
	    {
	      if (bpstat_ptr->commands != NULL)
		{
		  any_commands = 1;
		  break;
		}
	    }
	  if (any_commands)
	    ui_out_field_string (uiout, "commands", "yes");
	  else
	    ui_out_field_string (uiout, "commands", "no");

	}
      /* APPLE LOCAL: Xcode wants to know how many times this breakpoint 
             has been hit. */
      ui_out_print_annotation_int (uiout, 0, "times", bs->breakpoint_at->hit_count);
      ui_out_print_annotation_int (uiout, 1, "bkptno", bs->breakpoint_at->number);
      ui_out_text (uiout, ", ");
      return PRINT_SRC_AND_LOC;
      break;

    case bp_shlib_event:
      /* Did we stop because the user set the stop_on_solib_events
	 variable?  (If so, we report this as a generic, "Stopped due
	 to shlib event" message.) */

      /* APPLE LOCAL begin breakpoint MI */
      ui_out_print_annotation_string (uiout, 0, "reason", "shlib-event");
      /* APPLE LOCAL end breakpoint MI */
      ui_out_text (uiout, "Stopped due to shared library event\n");
      return PRINT_NOTHING;
      break;

    case bp_thread_event:
      /* Not sure how we will get here. 
	 GDB should not stop for these breakpoints.  */
      printf_filtered (_("Thread Event Breakpoint: gdb should not stop!\n"));
      return PRINT_NOTHING;
      break;

    case bp_overlay_event:
      /* By analogy with the thread event, GDB should not stop for these. */
      printf_filtered (_("Overlay Event Breakpoint: gdb should not stop!\n"));
      return PRINT_NOTHING;
      break;

    case bp_catch_load:
      annotate_catchpoint (bs->breakpoint_at->number);
      printf_filtered (_("\nCatchpoint %d (loaded %s), "),
		       bs->breakpoint_at->number,
		       bs->breakpoint_at->triggered_dll_pathname);
      return PRINT_SRC_AND_LOC;
      break;

    case bp_catch_unload:
      annotate_catchpoint (bs->breakpoint_at->number);
      printf_filtered (_("\nCatchpoint %d (unloaded %s), "),
		       bs->breakpoint_at->number,
		       bs->breakpoint_at->triggered_dll_pathname);
      return PRINT_SRC_AND_LOC;
      break;

    case bp_catch_fork:
      annotate_catchpoint (bs->breakpoint_at->number);
      printf_filtered (_("\nCatchpoint %d (forked process %d), "),
		       bs->breakpoint_at->number, 
		       bs->breakpoint_at->forked_inferior_pid);
      return PRINT_SRC_AND_LOC;
      break;

    case bp_catch_vfork:
      annotate_catchpoint (bs->breakpoint_at->number);
      printf_filtered (_("\nCatchpoint %d (vforked process %d), "),
		       bs->breakpoint_at->number, 
		       bs->breakpoint_at->forked_inferior_pid);
      return PRINT_SRC_AND_LOC;
      break;

    case bp_catch_exec:
      annotate_catchpoint (bs->breakpoint_at->number);
      printf_filtered (_("\nCatchpoint %d (exec'd %s), "),
		       bs->breakpoint_at->number,
		       bs->breakpoint_at->exec_pathname);
      return PRINT_SRC_AND_LOC;
      break;

    case bp_catch_catch:
    case bp_catch_throw:
      /* APPLE LOCAL begin gnu_v3 */
    case bp_gnu_v3_catch_catch:
    case bp_gnu_v3_catch_throw:
      /* APPLE LOCAL end gnu_v3 */
      return print_exception_catchpoint (bs->breakpoint_at);

    case bp_watchpoint:
    case bp_hardware_watchpoint:
      if (bs->old_val != NULL)
	{
	  annotate_watchpoint (bs->breakpoint_at->number);
	  ui_out_print_annotation_string (uiout, 0, "reason",
	       async_reason_lookup (EXEC_ASYNC_WATCHPOINT_TRIGGER));
	  mention (bs->breakpoint_at);
	  ui_out_chain = make_cleanup_ui_out_tuple_begin_end (uiout, "value");
	  ui_out_text (uiout, "\nOld value = ");
	  value_print (bs->old_val, stb->stream, 0, Val_pretty_default);
	  ui_out_field_stream (uiout, "old", stb);
	  ui_out_text (uiout, "\nNew value = ");
	  value_print (bs->breakpoint_at->val, stb->stream, 0, Val_pretty_default);
	  ui_out_field_stream (uiout, "new", stb);
	  do_cleanups (ui_out_chain);
	  ui_out_text (uiout, "\n");
	  value_free (bs->old_val);
	  bs->old_val = NULL;
	}
      /* More than one watchpoint may have been triggered.  */
      return PRINT_UNKNOWN;
      break;

    case bp_read_watchpoint:
      ui_out_print_annotation_string (uiout, 0, "reason",
	   async_reason_lookup (EXEC_ASYNC_READ_WATCHPOINT_TRIGGER));
      mention (bs->breakpoint_at);
      ui_out_chain = make_cleanup_ui_out_tuple_begin_end (uiout, "value");
      ui_out_text (uiout, "\nValue = ");
      value_print (bs->breakpoint_at->val, stb->stream, 0, Val_pretty_default);
      ui_out_field_stream (uiout, "value", stb);
      do_cleanups (ui_out_chain);
      ui_out_text (uiout, "\n");
      return PRINT_UNKNOWN;
      break;

    case bp_access_watchpoint:
      if (bs->old_val != NULL)     
	{
	  annotate_watchpoint (bs->breakpoint_at->number);
	  ui_out_print_annotation_string (uiout, 0, "reason",
	       async_reason_lookup (EXEC_ASYNC_ACCESS_WATCHPOINT_TRIGGER));
	  mention (bs->breakpoint_at);
	  ui_out_chain = make_cleanup_ui_out_tuple_begin_end (uiout, "value");
	  ui_out_text (uiout, "\nOld value = ");
	  value_print (bs->old_val, stb->stream, 0, Val_pretty_default);
	  ui_out_field_stream (uiout, "old", stb);
	  value_free (bs->old_val);
	  bs->old_val = NULL;
	  ui_out_text (uiout, "\nNew value = ");
	}
      else 
	{
	  mention (bs->breakpoint_at);
	  ui_out_print_annotation_string (uiout, 0, "reason",
	       async_reason_lookup (EXEC_ASYNC_ACCESS_WATCHPOINT_TRIGGER));
	  ui_out_chain = make_cleanup_ui_out_tuple_begin_end (uiout, "value");
	  ui_out_text (uiout, "\nValue = ");
	}
      value_print (bs->breakpoint_at->val, stb->stream, 0,Val_pretty_default);
      ui_out_field_stream (uiout, "new", stb);
      do_cleanups (ui_out_chain);
      ui_out_text (uiout, "\n");
      return PRINT_UNKNOWN;
      break;

    /* Fall through, we don't deal with these types of breakpoints
       here. */

    case bp_finish:
      ui_out_print_annotation_string (uiout, 0, "reason",
	   async_reason_lookup (EXEC_ASYNC_FUNCTION_FINISHED));
      return PRINT_UNKNOWN;
      break;

    case bp_until:
      ui_out_print_annotation_string (uiout, 0, "reason", "location-reached");
      return PRINT_UNKNOWN;
      break;

    case bp_none:
    case bp_longjmp:
    case bp_longjmp_resume:
    case bp_step_resume:
    case bp_through_sigtramp:
    case bp_watchpoint_scope:
    case bp_call_dummy:
    default:
      return PRINT_UNKNOWN;
    }
}

/* APPLE LOCAL: This prints catch exception information, using the current 
   exception global. */

static void
print_catch_info (struct breakpoint *b)
{
  char * event_type;

  if (CURRENT_EXCEPTION_KIND == EX_EVENT_THROW)
    event_type = "throw";
  else
    event_type = "catch";

  ui_out_text (uiout, "\nCatchpoint ");
  ui_out_field_int (uiout, "breakpoint", 
		    b->number);
  ui_out_text (uiout, " (exception caught), throw location ");

  if (CURRENT_EXCEPTION_THROW_PC && CURRENT_EXCEPTION_THROW_LINE)
    {
      ui_out_field_string (uiout, "throw-file", CURRENT_EXCEPTION_THROW_FILE);
      ui_out_text (uiout, ":");
      ui_out_field_int (uiout, "throw-line", CURRENT_EXCEPTION_THROW_LINE);
    }  
  else
    {
      ui_out_field_skip (uiout, "throw-file");
      ui_out_text (uiout, "unknown");
      ui_out_field_skip (uiout, "throw-line");
    }

  ui_out_text (uiout, ", catch location ");
  if (CURRENT_EXCEPTION_CATCH_PC && CURRENT_EXCEPTION_CATCH_LINE)
    {
      ui_out_field_string (uiout, "catch-file", CURRENT_EXCEPTION_CATCH_FILE);
      ui_out_text (uiout, ":");
      ui_out_field_int (uiout, "catch-line", CURRENT_EXCEPTION_CATCH_LINE);
    }  
  else
    {
      ui_out_field_skip (uiout, "catch-file");
      ui_out_text (uiout, "unknown");
      ui_out_field_skip (uiout, "catch-line");
    }

  
  ui_out_text (uiout, ", exception type ");
  if (CURRENT_EXCEPTION_TYPE)
    ui_out_field_string (uiout, "exception-type",
			 CURRENT_EXCEPTION_TYPE);
  else
    {
      ui_out_field_skip (uiout, "exception-type");
      ui_out_text (uiout, "unknown");
    }
  
  ui_out_text (uiout, "\n");
}

/* Generic routine for printing messages indicating why we
   stopped. The behavior of this function depends on the value
   'print_it' in the bpstat structure.  Under some circumstances we
   may decide not to print anything here and delegate the task to
   normal_stop(). */

static enum print_stop_action
print_bp_stop_message (bpstat bs)
{
  switch (bs->print_it)
    {
    case print_it_noop:
      /* Nothing should be printed for this bpstat entry. */
      return PRINT_UNKNOWN;
      break;

    case print_it_done:
      /* We still want to print the frame, but we already printed the
         relevant messages. */
      return PRINT_SRC_AND_LOC;
      break;

    case print_it_normal:
      /* Normal case.  Call the breakpoint's print_it method, or
	 print_it_typical.  */
      if (bs->breakpoint_at != NULL && bs->breakpoint_at->ops != NULL
	  && bs->breakpoint_at->ops->print_it != NULL)
	return bs->breakpoint_at->ops->print_it (bs->breakpoint_at);
      else
	return print_it_typical (bs);
      break;

    default:
      internal_error (__FILE__, __LINE__,
		      _("print_bp_stop_message: unrecognized enum value"));
      break;
    }
}

/* Print a message indicating what happened.  This is called from
   normal_stop().  The input to this routine is the head of the bpstat
   list - a list of the eventpoints that caused this stop.  This
   routine calls the generic print routine for printing a message
   about reasons for stopping.  This will print (for example) the
   "Breakpoint n," part of the output.  The return value of this
   routine is one of:

   PRINT_UNKNOWN: Means we printed nothing
   PRINT_SRC_AND_LOC: Means we printed something, and expect subsequent
   code to print the location. An example is 
   "Breakpoint 1, " which should be followed by
   the location.
   PRINT_SRC_ONLY: Means we printed something, but there is no need
   to also print the location part of the message.
   An example is the catch/throw messages, which
   don't require a location appended to the end.  
   PRINT_NOTHING: We have done some printing and we don't need any 
   further info to be printed.*/

enum print_stop_action
bpstat_print (bpstat bs)
{
  int val;

  /* Maybe another breakpoint in the chain caused us to stop.
     (Currently all watchpoints go on the bpstat whether hit or not.
     That probably could (should) be changed, provided care is taken
     with respect to bpstat_explains_signal).  */
  for (; bs; bs = bs->next)
    {
      val = print_bp_stop_message (bs);
      if (val == PRINT_SRC_ONLY 
	  || val == PRINT_SRC_AND_LOC 
	  || val == PRINT_NOTHING)
	return val;
    }

  /* We reached the end of the chain, or we got a null BS to start
     with and nothing was printed. */
  return PRINT_UNKNOWN;
}

/* Evaluate the expression EXP and return 1 if value is zero.
   This is used inside a catch_errors to evaluate the breakpoint condition. 
   The argument is a "struct expression *" that has been cast to char * to 
   make it pass through catch_errors.  */

static int
breakpoint_cond_eval (void *exp)
{
  struct value *mark = value_mark ();
  int i = !value_true (evaluate_expression ((struct expression *) exp));
  value_free_to_mark (mark);
  return i;
}

/* Allocate a new bpstat and chain it to the current one.  */

static bpstat
bpstat_alloc (struct breakpoint *b, bpstat cbs /* Current "bs" value */ )
{
  bpstat bs;

  bs = (bpstat) xmalloc (sizeof (*bs));
  cbs->next = bs;
  bs->breakpoint_at = b;
  /* If the condition is false, etc., don't do the commands.  */
  bs->commands = NULL;
  bs->old_val = NULL;
  bs->print_it = print_it_normal;
  return bs;
}

/* APPLE LOCAL begin watchpoint comparison */
/* Like value_equal, but two arrays are only considered equal if their
   contents are equal. */

static int
watchpoint_equal (struct value *arg1, struct value *arg2)
{
  const gdb_byte *p1, *p2;

  if ((TYPE_CODE (value_type (arg1)) == TYPE_CODE_ARRAY)
      && (TYPE_CODE (value_type (arg2)) == TYPE_CODE_ARRAY))
    {
      int len = TYPE_LENGTH (value_type (arg1));
      if (TYPE_LENGTH (value_type (arg1)) != TYPE_LENGTH (value_type (arg2)))
        return 0;
      p1 = value_contents (arg1);
      p2 = value_contents (arg2);
      while (--len >= 0)
        {
          if (*p1++ != *p2++)
            break;
        }
      return len < 0;
    }

  return value_equal (arg1, arg2);
}
/* APPLE LOCAL end watchpoint comparison */

/* Possible return values for watchpoint_check (this can't be an enum
   because of check_errors).  */
/* The watchpoint has been deleted.  */
#define WP_DELETED 1
/* The value has changed.  */
#define WP_VALUE_CHANGED 2
/* The value has not changed.  */
#define WP_VALUE_NOT_CHANGED 3

#define BP_TEMPFLAG 1
#define BP_HARDWAREFLAG 2
/* APPLE LOCAL future breakpoints */
#define BP_FUTUREFLAG 4

/* Check watchpoint condition.  */

static int
watchpoint_check (void *p)
{
  bpstat bs = (bpstat) p;
  struct breakpoint *b;
  struct frame_info *fr;
  int within_current_scope;

  b = bs->breakpoint_at;

  if (b->exp_valid_block == NULL)
    within_current_scope = 1;
  else
    {
      /* There is no current frame at this moment.  If we're going to have
         any chance of handling watchpoints on local variables, we'll need
         the frame chain (so we can determine if we're in scope).  */
      reinit_frame_cache ();
      fr = frame_find_by_id (b->watchpoint_frame);
      within_current_scope = (fr != NULL);
      /* in_function_epilogue_p() returns a non-zero value if we're still
	 in the function but the stack frame has already been invalidated.
	 Since we can't rely on the values of local variables after the
	 stack has been destroyed, we are treating the watchpoint in that
	 state as `not changed' without further checking.
	 
	 vinschen/2003-09-04: The former implementation left out the case
	 that the watchpoint frame couldn't be found by frame_find_by_id()
	 because the current PC is currently in an epilogue.  Calling
	 gdbarch_in_function_epilogue_p() also when fr == NULL fixes that. */
      if ((!within_current_scope || fr == get_current_frame ())
          && gdbarch_in_function_epilogue_p (current_gdbarch, read_pc ()))
	return WP_VALUE_NOT_CHANGED;
      if (fr && within_current_scope)
	/* If we end up stopping, the current frame will get selected
	   in normal_stop.  So this call to select_frame won't affect
	   the user.  */
	select_frame (fr);
    }

  if (within_current_scope)
    {
      /* We use value_{,free_to_}mark because it could be a
         *long* time before we return to the command level and
         call free_all_values.  We can't call free_all_values because
         we might be in the middle of evaluating a function call.  */

      /* APPLE LOCAL: Don't do anything if the val is NULL.  This means
	 we haven't successfully inserted the watchpoint yet.  */
      if (b->val)
	{
	  struct value *mark = value_mark ();
	  struct value *new_val = evaluate_expression (bs->breakpoint_at->exp);
	  /* APPLE LOCAL watchpoint comparison */
	  
	  if (!watchpoint_equal (b->val, new_val))
	    {
	      release_value (new_val);
	      value_free_to_mark (mark);
	      bs->old_val = b->val;
	      b->val = new_val;
	      /* We will stop here */
	      return WP_VALUE_CHANGED;
	    }
	  else
	    {
	      /* Nothing changed, don't do anything.  */
	      value_free_to_mark (mark);
	      /* We won't stop here */
	      return WP_VALUE_NOT_CHANGED;
	    }
	}
      else
	return WP_VALUE_NOT_CHANGED;
    }
  else
    {
      /* This seems like the only logical thing to do because
         if we temporarily ignored the watchpoint, then when
         we reenter the block in which it is valid it contains
         garbage (in the case of a function, it may have two
         garbage values, one before and one after the prologue).
         So we can't even detect the first assignment to it and
         watch after that (since the garbage may or may not equal
         the first value assigned).  */
      /* We print all the stop information in print_it_typical(), but
	 in this case, by the time we call print_it_typical() this bp
	 will be deleted already. So we have no choice but print the
	 information here. */
      ui_out_print_annotation_string (uiout, 0, "reason", 
			   async_reason_lookup (EXEC_ASYNC_WATCHPOINT_SCOPE));
      ui_out_text (uiout, "\nWatchpoint ");
      ui_out_field_int (uiout, "wpnum", bs->breakpoint_at->number);
      ui_out_text (uiout, " deleted because the program has left the block in\n\
which its expression is valid.\n");     

      if (b->related_breakpoint)
	b->related_breakpoint->disposition = disp_del_at_next_stop;
      b->disposition = disp_del_at_next_stop;

      return WP_DELETED;
    }
}

/* Get a bpstat associated with having just stopped at address
   BP_ADDR in thread PTID.  STOPPED_BY_WATCHPOINT is 1 if the
   target thinks we stopped due to a hardware watchpoint, 0 if we
   know we did not trigger a hardware watchpoint, and -1 if we do not know.  */

/* Determine whether we stopped at a breakpoint, etc, or whether we
   don't understand this stop.  Result is a chain of bpstat's such that:

   if we don't understand the stop, the result is a null pointer.

   if we understand why we stopped, the result is not null.

   Each element of the chain refers to a particular breakpoint or
   watchpoint at which we have stopped.  (We may have stopped for
   several reasons concurrently.)

   Each element of the chain has valid next, breakpoint_at,
   commands, FIXME??? fields.  */

bpstat
bpstat_stop_status (CORE_ADDR bp_addr, ptid_t ptid, int stopped_by_watchpoint)
{
  struct breakpoint *b, *temp;
  /* True if we've hit a breakpoint (as opposed to a watchpoint).  */
  int real_breakpoint = 0;
  /* Root of the chain of bpstat's */
  struct bpstats root_bs[1];
  /* Pointer to the last thing in the chain currently.  */
  bpstat bs = root_bs;
  int thread_id = pid_to_thread_id (ptid);

  ALL_BREAKPOINTS_SAFE (b, temp)
  {
    if (!breakpoint_enabled (b) && b->enable_state != bp_permanent)
      continue;

    if (b->type != bp_watchpoint
	&& b->type != bp_hardware_watchpoint
	&& b->type != bp_read_watchpoint
	&& b->type != bp_access_watchpoint
	&& b->type != bp_hardware_breakpoint
	&& b->type != bp_catch_fork
	&& b->type != bp_catch_vfork
	&& b->type != bp_catch_exec
	&& b->type != bp_catch_catch
	&& b->type != bp_catch_throw
      /* APPLE LOCAL: Handle gnu_v3_catch types.  */
	&& b->type != bp_gnu_v3_catch_catch
	&& b->type != bp_gnu_v3_catch_throw)	/* a non-watchpoint bp */
      {
	if (b->loc->address != bp_addr) 	/* address doesn't match */
	  continue;
	if (overlay_debugging		/* unmapped overlay section */
	    && section_is_overlay (b->loc->section) 
	    && !section_is_mapped (b->loc->section))
	  continue;
      }

    /* Continuable hardware watchpoints are treated as non-existent if the 
       reason we stopped wasn't a hardware watchpoint (we didn't stop on 
       some data address).  Otherwise gdb won't stop on a break instruction 
       in the code (not from a breakpoint) when a hardware watchpoint has 
       been defined.  */

    if ((b->type == bp_hardware_watchpoint
	 || b->type == bp_read_watchpoint
	 || b->type == bp_access_watchpoint)
	&& !stopped_by_watchpoint)
      continue;

    if (b->type == bp_hardware_breakpoint)
      {
	if (b->loc->address != bp_addr)
	  continue;
	if (overlay_debugging		/* unmapped overlay section */
	    && section_is_overlay (b->loc->section) 
	    && !section_is_mapped (b->loc->section))
	  continue;
      }

    /* Is this a catchpoint of a load or unload?  If so, did we
       get a load or unload of the specified library?  If not,
       ignore it. */
    if ((b->type == bp_catch_load)
#if defined(SOLIB_HAVE_LOAD_EVENT)
	&& (!SOLIB_HAVE_LOAD_EVENT (PIDGET (inferior_ptid))
	    || ((b->dll_pathname != NULL)
		&& (strcmp (b->dll_pathname, 
			    SOLIB_LOADED_LIBRARY_PATHNAME (
			      PIDGET (inferior_ptid)))
		    != 0)))
#endif
      )
      continue;

    if ((b->type == bp_catch_unload)
#if defined(SOLIB_HAVE_UNLOAD_EVENT)
	&& (!SOLIB_HAVE_UNLOAD_EVENT (PIDGET (inferior_ptid))
	    || ((b->dll_pathname != NULL)
		&& (strcmp (b->dll_pathname, 
			    SOLIB_UNLOADED_LIBRARY_PATHNAME (
			      PIDGET (inferior_ptid)))
		    != 0)))
#endif
      )
      continue;

    if ((b->type == bp_catch_fork)
	&& !inferior_has_forked (PIDGET (inferior_ptid),
				 &b->forked_inferior_pid))
      continue;

    if ((b->type == bp_catch_vfork)
	&& !inferior_has_vforked (PIDGET (inferior_ptid),
				  &b->forked_inferior_pid))
      continue;

    if ((b->type == bp_catch_exec)
	&& !inferior_has_execd (PIDGET (inferior_ptid), &b->exec_pathname))
      continue;

    /* APPLE LOCAL begin gnu_v3 */
    if (ep_is_exception_catchpoint (b))
      {
        /* APPLE LOCAL: There may be more than one exception
           catchpoint, but we only need to call
           target_get_current_exception_event for the one that was
           hit.  */
        if (b->loc->address != bp_addr)
          continue;
        if (!(current_exception_event = target_get_current_exception_event ()))
	  continue;
      }
    /* APPLE LOCAL end gnu_v3 */

    /* Come here if it's a watchpoint, or if the break address matches */

    bs = bpstat_alloc (b, bs);	/* Alloc a bpstat to explain stop */

    /* Watchpoints may change this, if not found to have triggered.
       Also if a catchpoint, but we aren't catching this object,
       we will change this.  */
    bs->stop = 1;
    bs->print = 1;

    if (ep_is_exception_catchpoint (b))
      {
	if (!current_exception_should_stop()) {
	  // remove_breakpoints ();
	  bs->stop = 0;
	  bs->print_it = print_it_noop;
	  continue;
	}
      }
    else if (b->type == bp_watchpoint ||
	b->type == bp_hardware_watchpoint)
      {
	char *message = xstrprintf ("Error evaluating expression for watchpoint %d\n",
				    b->number);
	struct cleanup *cleanups = make_cleanup (xfree, message);
	int e = catch_errors (watchpoint_check, bs, message, 
			      RETURN_MASK_ALL);
	do_cleanups (cleanups);
	switch (e)
	  {
	  case WP_DELETED:
	    /* We've already printed what needs to be printed.  */
	    /* Actually this is superfluous, because by the time we
               call print_it_typical() the wp will be already deleted,
               and the function will return immediately. */
	    bs->print_it = print_it_done;
	    /* Stop.  */
	    break;
	  case WP_VALUE_CHANGED:
	    /* Stop.  */
	    ++(b->hit_count);
	    break;
	  case WP_VALUE_NOT_CHANGED:
	    /* Don't stop.  */
	    bs->print_it = print_it_noop;
	    bs->stop = 0;
	    continue;
	  default:
	    /* Can't happen.  */
	    /* FALLTHROUGH */
	  case 0:
	    /* Error from catch_errors.  */
	    printf_filtered (_("Watchpoint %d deleted.\n"), b->number);
	    if (b->related_breakpoint)
	      b->related_breakpoint->disposition = disp_del_at_next_stop;
	    b->disposition = disp_del_at_next_stop;
	    /* We've already printed what needs to be printed.  */
	    bs->print_it = print_it_done;

	    /* Stop.  */
	    break;
	  }
      }
    else if (b->type == bp_read_watchpoint || 
	     b->type == bp_access_watchpoint)
      {
	CORE_ADDR addr;
	struct value *v;
	int found = 0;

	if (!target_stopped_data_address (&current_target, &addr))
	  continue;
	for (v = b->val_chain; v; v = value_next (v))
	  {
	    if (VALUE_LVAL (v) == lval_memory
		&& ! value_lazy (v))
	      {
		struct type *vtype = check_typedef (value_type (v));

		if (v == b->val_chain
		    || (TYPE_CODE (vtype) != TYPE_CODE_STRUCT
			&& TYPE_CODE (vtype) != TYPE_CODE_ARRAY))
		  {
		    CORE_ADDR vaddr;

		    vaddr = VALUE_ADDRESS (v) + value_offset (v);
		    /* Exact match not required.  Within range is
                       sufficient.  */
		    if (addr >= vaddr &&
			addr < vaddr + TYPE_LENGTH (value_type (v)))
		      found = 1;
		  }
	      }
	  }
	if (found)
	  {
	    char *message = xstrprintf ("Error evaluating expression for watchpoint %d\n",
					b->number);
	    struct cleanup *cleanups = make_cleanup (xfree, message);
	    int e = catch_errors (watchpoint_check, bs, message,
				  RETURN_MASK_ALL);
	    do_cleanups (cleanups);
	    switch (e)
	      {
	      case WP_DELETED:
		/* We've already printed what needs to be printed.  */
		bs->print_it = print_it_done;
		/* Stop.  */
		break;
	      case WP_VALUE_CHANGED:
		if (b->type == bp_read_watchpoint)
		  {
		    /* Don't stop: read watchpoints shouldn't fire if
		       the value has changed.  This is for targets
		       which cannot set read-only watchpoints.  */
		    bs->print_it = print_it_noop;
		    bs->stop = 0;
		    continue;
		  }
		++(b->hit_count);
		break;
	      case WP_VALUE_NOT_CHANGED:
		/* Stop.  */
		++(b->hit_count);
		break;
	      default:
		/* Can't happen.  */
	      case 0:
		/* Error from catch_errors.  */
		printf_filtered (_("Watchpoint %d deleted.\n"), b->number);
		if (b->related_breakpoint)
		  b->related_breakpoint->disposition = disp_del_at_next_stop;
		b->disposition = disp_del_at_next_stop;
		/* We've already printed what needs to be printed.  */
		bs->print_it = print_it_done;
		break;
	      }
	  }
	else	/* found == 0 */
	  {
	    /* This is a case where some watchpoint(s) triggered,
	       but not at the address of this watchpoint (FOUND
	       was left zero).  So don't print anything for this
	       watchpoint.  */
	    bs->print_it = print_it_noop;
	    bs->stop = 0;
            continue;
	  }
      }
    else
      {
	/* By definition, an encountered breakpoint is a triggered
	   breakpoint. */
	++(b->hit_count);

	real_breakpoint = 1;
      }

    /* APPLE LOCAL begin subroutine inlining  */
    if (frame_id_p (b->frame_id)
	&& !frame_id_eq (b->frame_id, get_frame_id (get_current_frame ()))
	&& (get_frame_type (get_current_frame ()) != INLINED_FRAME
	    || !frame_id_eq (step_frame_id, b->frame_id)))
    /* APPLE LOCAL end subroutine inlining  */
      bs->stop = 0;
    else
      {
	int value_is_zero = 0;

        /* APPLE LOCAL begin subroutine inlining  */
        if (b->type == bp_inlined_breakpoint)
          inlined_subroutine_adjust_position_for_breakpoint (b);
        /* APPLE LOCAL end subroutine inlining  */

	/* APPLE LOCAL: We allow a breakpoint condition to be set if it
	   couldn't be parsed at the time of the condition command, since
	   there are legit reasons why that might happen.  So now we should
	   work from cond_string.  Note, also it is important to catch errors here,
	   since if we error out we will miss inserting the real instruction
	   in place of the breakpoint trap.  */

	if (b->cond_string)
	  {
	    int parse_succeeded = 1;
	    /* Need to select the frame, with all that implies
	       so that the conditions will have the right context.  */
	    select_frame (get_current_frame ());
	    
            /* The condition might have been zero'ed out when
               we did breakpoint_re_set_one, and so we need to
               rebuild it here. */
            if (b->cond == NULL)
              {
                /* APPLE LOCAL: It is important to catch errors here.  If we
                   get an error parsing the expression we will miss inserting
                   the instruction under the breakpoint.  */
                char *s = b->cond_string;
		parse_succeeded = gdb_parse_exp_1 (&s, get_selected_block (NULL), 
						   0, &(b->cond));
              }
	    
            if (parse_succeeded)
              {
		value_is_zero
		  = catch_errors (breakpoint_cond_eval, (b->cond),
				  "Error in testing breakpoint condition:\n",
				  RETURN_MASK_ALL);
		/* FIXME-someday, should give breakpoint # */
		free_all_values ();
	      }
	  }
	if (b->cond && value_is_zero)
	  {
	    bs->stop = 0;
	    /* Don't consider this a hit.  */
	    --(b->hit_count);
	  }
	else if (b->thread != -1 && b->thread != thread_id)
	  {
	    bs->stop = 0;
	    /* Don't consider this a hit.  */
	    --(b->hit_count);
	  }
	else if (b->ignore_count > 0)
	  {
	    b->ignore_count--;
	    annotate_ignore_count_change ();
	    bs->stop = 0;
	  }
	else
	  {
	    /* We will stop here */
	    if (b->disposition == disp_disable)
	      b->enable_state = bp_disabled;
	    if (b->silent)
	      bs->print = 0;
	    bs->commands = b->commands;
	    if (bs->commands &&
		(strcmp ("silent", bs->commands->line) == 0
		 || (xdb_commands && strcmp ("Q", bs->commands->line) == 0)))
	      {
		bs->commands = bs->commands->next;
		bs->print = 0;
	      }
	    bs->commands = copy_command_lines (bs->commands);
	  }
      }
    /* Print nothing for this entry if we dont stop or if we dont print.  */
    if (bs->stop == 0 || bs->print == 0)
      bs->print_it = print_it_noop;
  }

  bs->next = NULL;		/* Terminate the chain */
  bs = root_bs->next;		/* Re-grab the head of the chain */

  /* The value of a hardware watchpoint hasn't changed, but the
     intermediate memory locations we are watching may have.  */
  if (bs && !bs->stop)
    {
      switch (bs->breakpoint_at->type)
	{
	case bp_hardware_watchpoint:
	case bp_read_watchpoint:
	case bp_access_watchpoint:
	  remove_breakpoints ();
	  insert_breakpoints ();
	  break;
	default:
	  break;
	}
    }

  return bs;
}

/* Tell what to do about this bpstat.  */
struct bpstat_what
bpstat_what (bpstat bs)
{
  /* Classify each bpstat as one of the following.  */
  enum class
    {
      /* This bpstat element has no effect on the main_action.  */
      no_effect = 0,

      /* There was a watchpoint, stop but don't print.  */
      wp_silent,

      /* There was a watchpoint, stop and print.  */
      wp_noisy,

      /* There was a breakpoint but we're not stopping.  */
      bp_nostop,

      /* There was a breakpoint, stop but don't print.  */
      bp_silent,

      /* There was a breakpoint, stop and print.  */
      bp_noisy,

      /* We hit the longjmp breakpoint.  */
      long_jump,

      /* We hit the longjmp_resume breakpoint.  */
      long_resume,

      /* We hit the step_resume breakpoint.  */
      step_resume,

      /* We hit the through_sigtramp breakpoint.  */
      through_sig,

      /* We hit the shared library event breakpoint.  */
      shlib_event,

      /* We caught a shared library event.  */
      catch_shlib_event,

      /* This is just used to count how many enums there are.  */
      class_last
    };

  /* Here is the table which drives this routine.  So that we can
     format it pretty, we define some abbreviations for the
     enum bpstat_what codes.  */
#define kc BPSTAT_WHAT_KEEP_CHECKING
#define ss BPSTAT_WHAT_STOP_SILENT
#define sn BPSTAT_WHAT_STOP_NOISY
#define sgl BPSTAT_WHAT_SINGLE
#define slr BPSTAT_WHAT_SET_LONGJMP_RESUME
#define clr BPSTAT_WHAT_CLEAR_LONGJMP_RESUME
#define clrs BPSTAT_WHAT_CLEAR_LONGJMP_RESUME_SINGLE
#define sr BPSTAT_WHAT_STEP_RESUME
#define ts BPSTAT_WHAT_THROUGH_SIGTRAMP
#define shl BPSTAT_WHAT_CHECK_SHLIBS
#define shlr BPSTAT_WHAT_CHECK_SHLIBS_RESUME_FROM_HOOK

/* "Can't happen."  Might want to print an error message.
   abort() is not out of the question, but chances are GDB is just
   a bit confused, not unusable.  */
#define err BPSTAT_WHAT_STOP_NOISY

  /* Given an old action and a class, come up with a new action.  */
  /* One interesting property of this table is that wp_silent is the same
     as bp_silent and wp_noisy is the same as bp_noisy.  That is because
     after stopping, the check for whether to step over a breakpoint
     (BPSTAT_WHAT_SINGLE type stuff) is handled in proceed() without
     reference to how we stopped.  We retain separate wp_silent and
     bp_silent codes in case we want to change that someday. 

     Another possibly interesting property of this table is that
     there's a partial ordering, priority-like, of the actions.  Once
     you've decided that some action is appropriate, you'll never go
     back and decide something of a lower priority is better.  The
     ordering is:

     kc   < clr sgl shl shlr slr sn sr ss ts
     sgl  < clrs shl shlr slr sn sr ss ts
     slr  < err shl shlr sn sr ss ts
     clr  < clrs err shl shlr sn sr ss ts
     clrs < err shl shlr sn sr ss ts
     ss   < shl shlr sn sr ts
     sn   < shl shlr sr ts
     sr   < shl shlr ts
     shl  < shlr
     ts   < 
     shlr <

     What I think this means is that we don't need a damned table
     here.  If you just put the rows and columns in the right order,
     it'd look awfully regular.  We could simply walk the bpstat list
     and choose the highest priority action we find, with a little
     logic to handle the 'err' cases, and the CLEAR_LONGJMP_RESUME/
     CLEAR_LONGJMP_RESUME_SINGLE distinction (which breakpoint.h says
     is messy anyway).  */

  /* step_resume entries: a step resume breakpoint overrides another
     breakpoint of signal handling (see comment in wait_for_inferior
     at where we set the step_resume breakpoint).  */
  /* We handle the through_sigtramp_breakpoint the same way; having both
     one of those and a step_resume_breakpoint is probably very rare (?).  */

  static const enum bpstat_what_main_action
    table[(int) class_last][(int) BPSTAT_WHAT_LAST] =
  {
  /*                              old action */
  /*       kc    ss    sn    sgl    slr   clr    clrs   sr    ts   shl   shlr
   */
/*no_effect */
    {kc, ss, sn, sgl, slr, clr, clrs, sr, ts, shl, shlr},
/*wp_silent */
    {ss, ss, sn, ss, ss, ss, ss, sr, ts, shl, shlr},
/*wp_noisy */
    {sn, sn, sn, sn, sn, sn, sn, sr, ts, shl, shlr},
/*bp_nostop */
    {sgl, ss, sn, sgl, slr, clrs, clrs, sr, ts, shl, shlr},
/*bp_silent */
    {ss, ss, sn, ss, ss, ss, ss, sr, ts, shl, shlr},
/*bp_noisy */
    {sn, sn, sn, sn, sn, sn, sn, sr, ts, shl, shlr},
/*long_jump */
    {slr, ss, sn, slr, slr, err, err, sr, ts, shl, shlr},
/*long_resume */
    {clr, ss, sn, clrs, err, err, err, sr, ts, shl, shlr},
/*step_resume */
    {sr, sr, sr, sr, sr, sr, sr, sr, ts, shl, shlr},
/*through_sig */
    {ts, ts, ts, ts, ts, ts, ts, ts, ts, shl, shlr},
/*shlib */
    {shl, shl, shl, shl, shl, shl, shl, shl, ts, shl, shlr},
/*catch_shlib */
    {shlr, shlr, shlr, shlr, shlr, shlr, shlr, shlr, ts, shlr, shlr}
  };

#undef kc
#undef ss
#undef sn
#undef sgl
#undef slr
#undef clr
#undef clrs
#undef err
#undef sr
#undef ts
#undef shl
#undef shlr
  enum bpstat_what_main_action current_action = BPSTAT_WHAT_KEEP_CHECKING;
  struct bpstat_what retval;

  retval.call_dummy = 0;
  for (; bs != NULL; bs = bs->next)
    {
      enum class bs_class = no_effect;
      if (bs->breakpoint_at == NULL)
	/* I suspect this can happen if it was a momentary breakpoint
	   which has since been deleted.  */
	continue;
      switch (bs->breakpoint_at->type)
	{
	case bp_none:
	  continue;

	case bp_breakpoint:
	/* APPLE LOCAL begin subroutine inlining  */
        case bp_inlined_breakpoint:
	/* APPLE LOCAL end subroutine inlining  */
	case bp_hardware_breakpoint:
	case bp_until:
	case bp_finish:
	  if (bs->stop)
	    {
	      if (bs->print)
		bs_class = bp_noisy;
	      else
		bs_class = bp_silent;
	    }
	  else
	    bs_class = bp_nostop;
	  break;
	case bp_watchpoint:
	case bp_hardware_watchpoint:
	case bp_read_watchpoint:
	case bp_access_watchpoint:
	  if (bs->stop)
	    {
	      if (bs->print)
		bs_class = wp_noisy;
	      else
		bs_class = wp_silent;
	    }
	  else
	    /* There was a watchpoint, but we're not stopping. 
	       This requires no further action.  */
	    bs_class = no_effect;
	  break;
	case bp_longjmp:
	  bs_class = long_jump;
	  break;
	case bp_longjmp_resume:
	  bs_class = long_resume;
	  break;
	case bp_step_resume:
	  if (bs->stop)
	    {
	      bs_class = step_resume;
	    }
	  else
	    /* It is for the wrong frame.  */
	    bs_class = bp_nostop;
	  break;
	case bp_through_sigtramp:
	  bs_class = through_sig;
	  break;
	case bp_watchpoint_scope:
	  bs_class = bp_nostop;
	  break;
	case bp_shlib_event:
	  bs_class = shlib_event;
	  break;
	case bp_thread_event:
	case bp_overlay_event:
	  bs_class = bp_nostop;
	  break;
	case bp_catch_load:
	case bp_catch_unload:
	  /* Only if this catchpoint triggered should we cause the
	     step-out-of-dld behaviour.  Otherwise, we ignore this
	     catchpoint.  */
	  if (bs->stop)
	    bs_class = catch_shlib_event;
	  else
	    bs_class = no_effect;
	  break;
	case bp_catch_fork:
	case bp_catch_vfork:
	case bp_catch_exec:
	  if (bs->stop)
	    {
	      if (bs->print)
		bs_class = bp_noisy;
	      else
		bs_class = bp_silent;
	    }
	  else
	    /* There was a catchpoint, but we're not stopping.  
	       This requires no further action.  */
	    bs_class = no_effect;
	  break;
	case bp_catch_catch:
	  if (!bs->stop || CURRENT_EXCEPTION_KIND != EX_EVENT_CATCH)
	    bs_class = bp_nostop;
	  else if (bs->stop)
	    bs_class = bs->print ? bp_noisy : bp_silent;
	  break;
	case bp_catch_throw:
	  if (!bs->stop || CURRENT_EXCEPTION_KIND != EX_EVENT_THROW)
	    bs_class = bp_nostop;
	  else if (bs->stop)
	    bs_class = bs->print ? bp_noisy : bp_silent;
	  break;
	  /* APPLE LOCAL begin gnu_v3 */
	case bp_gnu_v3_catch_catch:
	case bp_gnu_v3_catch_throw:
	  if (bs->stop)
	    bs_class = bs->print ? bp_noisy : bp_silent;
	  break;
	  /* APPLE LOCAL end gnu_v3 */
	case bp_call_dummy:
	  /* Make sure the action is stop (silent or noisy),
	     so infrun.c pops the dummy frame.  */
	  bs_class = bp_silent;
	  retval.call_dummy = 1;
	  break;
	  /* APPLE LOCAL begin switch default */
        default:
          internal_error (__FILE__, __LINE__, "unhandled switch case");
          break;
	  /* APPLE LOCAL end switch default */
	}
      current_action = table[(int) bs_class][(int) current_action];
    }
  retval.main_action = current_action;
  return retval;
}

/* Nonzero if we should step constantly (e.g. watchpoints on machines
   without hardware support).  This isn't related to a specific bpstat,
   just to things like whether watchpoints are set.  */

int
bpstat_should_step (void)
{
  struct breakpoint *b;
  ALL_BREAKPOINTS (b)
    if (breakpoint_enabled (b) && b->type == bp_watchpoint)
      return 1;
  return 0;
}

/* Nonzero if there are enabled hardware watchpoints. */
int
bpstat_have_active_hw_watchpoints (void)
{
  struct bp_location *bpt;
  ALL_BP_LOCATIONS (bpt)
    if (breakpoint_enabled (bpt->owner)
	&& bpt->inserted
	&& bpt->loc_type == bp_loc_hardware_watchpoint)
      return 1;
  return 0;
}


/* Given a bpstat that records zero or more triggered eventpoints, this
   function returns another bpstat which contains only the catchpoints
   on that first list, if any. */
void
bpstat_get_triggered_catchpoints (bpstat ep_list, bpstat *cp_list)
{
  struct bpstats root_bs[1];
  bpstat bs = root_bs;
  struct breakpoint *ep;
  char *dll_pathname;

  bpstat_clear (cp_list);
  root_bs->next = NULL;

  for (; ep_list != NULL; ep_list = ep_list->next)
    {
      /* Is this eventpoint a catchpoint?  If not, ignore it. */
      ep = ep_list->breakpoint_at;
      if (ep == NULL)
	break;
      if ((ep->type != bp_catch_load) &&
	  (ep->type != bp_catch_unload) &&
	  (ep->type != bp_catch_catch) &&
	  (ep->type != bp_catch_throw))		
	/* pai: (temp) ADD fork/vfork here!!  */
	continue;

      /* Yes; add it to the list. */
      bs = bpstat_alloc (ep, bs);
      *bs = *ep_list;
      bs->next = NULL;
      bs = root_bs->next;

#if defined(SOLIB_ADD)
      /* Also, for each triggered catchpoint, tag it with the name of
         the library that caused this trigger.  (We copy the name now,
         because it's only guaranteed to be available NOW, when the
         catchpoint triggers.  Clients who may wish to know the name
         later must get it from the catchpoint itself.) */
      if (ep->triggered_dll_pathname != NULL)
	xfree (ep->triggered_dll_pathname);
      if (ep->type == bp_catch_load)
	dll_pathname = SOLIB_LOADED_LIBRARY_PATHNAME (
	                 PIDGET (inferior_ptid));
      else
	dll_pathname = SOLIB_UNLOADED_LIBRARY_PATHNAME (
	                 PIDGET (inferior_ptid));
#else
      dll_pathname = NULL;
#endif
      if (dll_pathname)
	{
	  ep->triggered_dll_pathname = (char *) 
	    xmalloc (strlen (dll_pathname) + 1);
	  strcpy (ep->triggered_dll_pathname, dll_pathname);
	}
      else
	ep->triggered_dll_pathname = NULL;
    }

  *cp_list = bs;
}

/* Print B to gdb_stdout. */
static void
print_one_breakpoint (struct breakpoint *b,
		      CORE_ADDR *last_addr)
{
  struct command_line *l;
  struct symbol *sym;
  struct ep_type_description
    {
      enum bptype type;
      char *description;
    };
  static struct ep_type_description bptypes[] =
  {
    {bp_none, "?deleted?"},
    {bp_breakpoint, "breakpoint"},
    /* APPLE LOCAL begin subroutine inlining  */
    {bp_inlined_breakpoint, "inlined subroutine breakpoint"},
    /* APPLE LOCAL end subroutine inlining  */
    {bp_hardware_breakpoint, "hw breakpoint"},
    {bp_until, "until"},
    {bp_finish, "finish"},
    {bp_watchpoint, "watchpoint"},
    {bp_hardware_watchpoint, "hw watchpoint"},
    {bp_read_watchpoint, "read watchpoint"},
    {bp_access_watchpoint, "acc watchpoint"},
    {bp_longjmp, "longjmp"},
    {bp_longjmp_resume, "longjmp resume"},
    {bp_step_resume, "step resume"},
    {bp_through_sigtramp, "sigtramp"},
    {bp_watchpoint_scope, "watchpoint scope"},
    {bp_call_dummy, "call dummy"},
    {bp_shlib_event, "shlib events"},
    {bp_thread_event, "thread events"},
    {bp_overlay_event, "overlay events"},
    {bp_catch_load, "catch load"},
    {bp_catch_unload, "catch unload"},
    {bp_catch_fork, "catch fork"},
    {bp_catch_vfork, "catch vfork"},
    {bp_catch_exec, "catch exec"},
    {bp_catch_catch, "catch catch"},
    {bp_catch_throw, "catch throw"}
    /* APPLE LOCAL begin gnu_v3 */
    , {bp_gnu_v3_catch_catch, "catch catch"}
    , {bp_gnu_v3_catch_throw, "catch throw"}
    /* APPLE LOCAL end gnu_v3 */
  };
  
  static char *bpdisps[] =
  {"del", "dstp", "dis", "keep"};
  /* APPLE LOCAL: bpenables is "nynny" in the FSF code.  We want
     them all to be distinct.  */
  static char bpenables[] = "nyscp";
  char wrap_indent[80];
  struct ui_stream *stb = ui_out_stream_new (uiout);
  struct cleanup *old_chain = make_cleanup_ui_out_stream_delete (stb);
  struct cleanup *bkpt_chain;

  annotate_record ();
  bkpt_chain = make_cleanup_ui_out_tuple_begin_end (uiout, "bkpt");

  /* 1 */
  annotate_field (0);
  ui_out_field_int (uiout, "number", b->number);

  /* 2 */
  annotate_field (1);
  if (((int) b->type > (sizeof (bptypes) / sizeof (bptypes[0])))
      || ((int) b->type != bptypes[(int) b->type].type))
    internal_error (__FILE__, __LINE__,
		    _("bptypes table does not describe type #%d."),
		    (int) b->type);
  ui_out_field_string (uiout, "type", bptypes[(int) b->type].description);

  /* 3 */
  annotate_field (2);
  ui_out_field_string (uiout, "disp", bpdisps[(int) b->disposition]);

  /* 4 */
  annotate_field (3);
  ui_out_field_fmt (uiout, "enabled", "%c", bpenables[(int) b->enable_state]);
  ui_out_spaces (uiout, 2);
  
  /* 5 and 6 */
  strcpy (wrap_indent, "                           ");
  if (addressprint)
    {
      if (TARGET_ADDR_BIT <= 32)
	strcat (wrap_indent, "           ");
      else
	strcat (wrap_indent, "                   ");
    }

  if (b->ops != NULL && b->ops->print_one != NULL)
    b->ops->print_one (b, last_addr);
  else
    switch (b->type)
      {
      case bp_none:
	internal_error (__FILE__, __LINE__,
			_("print_one_breakpoint: bp_none encountered\n"));
	break;

      case bp_watchpoint:
      case bp_hardware_watchpoint:
      case bp_read_watchpoint:
      case bp_access_watchpoint:
	/* Field 4, the address, is omitted (which makes the columns
	   not line up too nicely with the headers, but the effect
	   is relatively readable).  */
	if (addressprint)
	  ui_out_field_skip (uiout, "addr");
	annotate_field (5);
	/* APPLE LOCAL: Replace call to print_expression with 
	   gdb_print_expression so we don't error out here.  */
	gdb_print_expression (b->exp, stb->stream);
	ui_out_field_stream (uiout, "what", stb);
	break;

      case bp_catch_load:
      case bp_catch_unload:
	/* Field 4, the address, is omitted (which makes the columns
	   not line up too nicely with the headers, but the effect
	   is relatively readable).  */
	if (addressprint)
	  ui_out_field_skip (uiout, "addr");
	annotate_field (5);
	if (b->dll_pathname == NULL)
	  {
	    ui_out_field_string (uiout, "what", "<any library>");
	    ui_out_spaces (uiout, 1);
	  }
	else
	  {
	    ui_out_text (uiout, "library \"");
	    ui_out_field_string (uiout, "what", b->dll_pathname);
	    ui_out_text (uiout, "\" ");
	  }
	break;

      case bp_catch_fork:
      case bp_catch_vfork:
	/* Field 4, the address, is omitted (which makes the columns
	   not line up too nicely with the headers, but the effect
	   is relatively readable).  */
	if (addressprint)
	  ui_out_field_skip (uiout, "addr");
	annotate_field (5);
	if (b->forked_inferior_pid != 0)
	  {
	    ui_out_text (uiout, "process ");
	    ui_out_field_int (uiout, "what", b->forked_inferior_pid);
	    ui_out_spaces (uiout, 1);
	  }

      case bp_catch_exec:
	/* Field 4, the address, is omitted (which makes the columns
	   not line up too nicely with the headers, but the effect
	   is relatively readable).  */
	if (addressprint)
	  ui_out_field_skip (uiout, "addr");
	annotate_field (5);
	if (b->exec_pathname != NULL)
	  {
	    ui_out_text (uiout, "program \"");
	    ui_out_field_string (uiout, "what", b->exec_pathname);
	    ui_out_text (uiout, "\" ");
	  }
	break;

      case bp_catch_catch:
      case bp_gnu_v3_catch_catch:
      case bp_catch_throw:
      case bp_gnu_v3_catch_throw:
	print_one_exception_catchpoint (b, 0);

      case bp_breakpoint:
      /* APPLE LOCAL begin subroutine inlining  */
      case bp_inlined_breakpoint:
      /* APPLE LOCAL end subroutine inlining  */
      case bp_hardware_breakpoint:
      case bp_until:
      case bp_finish:
      case bp_longjmp:
      case bp_longjmp_resume:
      case bp_step_resume:
      case bp_through_sigtramp:
      case bp_watchpoint_scope:
      case bp_call_dummy:
      case bp_shlib_event:
      case bp_thread_event:
      case bp_overlay_event:
	if (addressprint)
	  {
	    annotate_field (4);
	    if (b->pending)
	      ui_out_field_string (uiout, "addr", "<PENDING>");
	    else
	      ui_out_field_core_addr (uiout, "addr", b->loc->address);
	  }
	annotate_field (5);
	*last_addr = b->loc->address;
	if (b->source_file)
	  {
	    sym = find_pc_sect_function (b->loc->address, b->loc->section);
	    if (sym)
	      {
		ui_out_text (uiout, "in ");
		ui_out_field_string (uiout, "func",
				     SYMBOL_PRINT_NAME (sym));
		ui_out_wrap_hint (uiout, wrap_indent);
		ui_out_text (uiout, " at ");
	      }
	    ui_out_field_string (uiout, "file", b->source_file);
	    ui_out_text (uiout, ":");
	    ui_out_field_int (uiout, "line", b->line_number);
	  }
	else if (b->pending)
	  {
	    ui_out_field_string (uiout, "pending", b->addr_string);
	  }
	else
	  {
	    print_address_symbolic (b->loc->address, stb->stream, demangle, "");
	    ui_out_field_stream (uiout, "at", stb);
	  }
	break;
	/* APPLE LOCAL: Add a default case...  */
      default:
	internal_error (__FILE__, __LINE__, "unhandled switch case");
      }

  if (b->thread != -1)
    {
      /* FIXME: This seems to be redundant and lost here; see the
	 "stop only in" line a little further down. */
      ui_out_text (uiout, " thread ");
      ui_out_field_int (uiout, "thread", b->thread);
    }
  
  /* APPLE LOCAL: Output the shared library this breakpoint was
     set in.  */
  if (ui_out_is_mi_like_p (uiout) && b->bp_objfile != NULL)
    {
      if (b->bp_objfile->name != NULL)
	ui_out_field_string (uiout, "shlib", b->bp_objfile->name);
      else
	ui_out_field_skip (uiout, "shlib");
    }
  /* END APPLE LOCAL */
  ui_out_text (uiout, "\n");
  
  if (frame_id_p (b->frame_id))
    {
      annotate_field (6);
      ui_out_text (uiout, "\tstop only in stack frame at ");
      /* FIXME: cagney/2002-12-01: Shouldn't be poeking around inside
         the frame ID.  */
      ui_out_field_core_addr (uiout, "frame", b->frame_id.stack_addr);
      ui_out_text (uiout, "\n");
    }
  
  /* APPLE LOCAL breakpoints */
  if (b->cond || b->cond_string)
    {
      /* APPLE LOCAL begin breakpoint printing */
      /* There might be an error printing the expression.  Not sure
         what the best thing to do here is.  The expression was
         parseable, so this is clearly a bug in the printer.  Should
         we tell the user the expression printer is lame, or should we
         just return what they input?  For now do the latter, if a
         cond_string is available.  Note, we have to redirect stderr
         or the error will go to the console and mess up the
         output.  */
      struct ui_file *prev_stderr = gdb_stderr;
      /* APPLE LOCAL end breakpoint printing */
      annotate_field (7);
      /* APPLE LOCAL begin breakpoint printing */
      gdb_stderr = gdb_null;
      if (b->cond && gdb_print_expression (b->cond, stb->stream))
	{
	  ui_out_text (uiout, "\tstop only if ");
	  ui_out_field_stream (uiout, "cond", stb);
	}
      else
        {
          if (b->cond_string)
	    {
	      ui_out_text (uiout, "\tcondition not yet parsed: ");
	      ui_out_field_string (uiout, "cond", b->cond_string);
	    }
          else
            ui_out_field_string (uiout, "cond", "Error printing condition string");
        }
      ui_out_text (uiout, "\n");
      gdb_stderr = prev_stderr;
      /* APPLE LOCAL end breakpoint printing */
    }

  if (b->pending && b->cond_string)
    {
      annotate_field (7);
      ui_out_text (uiout, "\tstop only if ");
      ui_out_field_string (uiout, "cond", b->cond_string);
      ui_out_text (uiout, "\n");
    }

  if (b->thread != -1)
    {
      /* FIXME should make an annotation for this */
      ui_out_text (uiout, "\tstop only in thread ");
      ui_out_field_int (uiout, "thread", b->thread);
      ui_out_text (uiout, "\n");
    }
  
  if (show_breakpoint_hit_counts && b->hit_count)
    {
      /* FIXME should make an annotation for this */
      if (ep_is_catchpoint (b))
	ui_out_text (uiout, "\tcatchpoint");
      else
	ui_out_text (uiout, "\tbreakpoint");
      ui_out_text (uiout, " already hit ");
      ui_out_field_int (uiout, "times", b->hit_count);
      if (b->hit_count == 1)
	ui_out_text (uiout, " time\n");
      else
	ui_out_text (uiout, " times\n");
    }
  
  /* Output the count also if it is zero, but only if this is
     mi. FIXME: Should have a better test for this. */
  if (ui_out_is_mi_like_p (uiout))
    if (show_breakpoint_hit_counts && b->hit_count == 0)
      ui_out_field_int (uiout, "times", b->hit_count);

  if (b->ignore_count)
    {
      annotate_field (8);
      ui_out_text (uiout, "\tignore next ");
      ui_out_field_int (uiout, "ignore", b->ignore_count);
      ui_out_text (uiout, " hits\n");
    }
  
  if ((l = b->commands))
    {
      struct cleanup *script_chain;

      annotate_field (9);
      /* APPLE LOCAL: We changed "script" to "commands" here.  This was to
	 be consistent with the rest of the MI reporting of the commands,
	 where we use command not script.  */
      script_chain = make_cleanup_ui_out_tuple_begin_end (uiout, "commands");
      print_command_lines (uiout, l, 4);
      do_cleanups (script_chain);
    }
  do_cleanups (bkpt_chain);
  do_cleanups (old_chain);
}

struct captured_breakpoint_query_args
  {
    int bnum;
  };

static int
do_captured_breakpoint_query (struct ui_out *uiout, void *data)
{
  struct captured_breakpoint_query_args *args = data;
  struct breakpoint *b;
  CORE_ADDR dummy_addr = 0;
  ALL_BREAKPOINTS (b)
    {
      if (args->bnum == b->number)
	{
	  print_one_breakpoint (b, &dummy_addr);
	  return GDB_RC_OK;
	}
    }
  return GDB_RC_NONE;
}

enum gdb_rc
gdb_breakpoint_query (struct ui_out *uiout, int bnum, char **error_message)
{
  struct captured_breakpoint_query_args args;
  args.bnum = bnum;
  /* For the moment we don't trust print_one_breakpoint() to not throw
     an error. */
  return catch_exceptions_with_msg (uiout, do_captured_breakpoint_query, &args,
				    error_message, RETURN_MASK_ALL);
}

/* Return non-zero if B is user settable (breakpoints, watchpoints,
   catchpoints, et.al.). */

static int
user_settable_breakpoint (const struct breakpoint *b)
{
  return (b->type == bp_breakpoint
          /* APPLE LOCAL begin subroutine inlining  */
          || b->type == bp_inlined_breakpoint
          /* APPLE LOCAL end subroutine inlining  */
	  || b->type == bp_catch_load
	  || b->type == bp_catch_unload
	  || b->type == bp_catch_fork
	  || b->type == bp_catch_vfork
	  || b->type == bp_catch_exec
	  || b->type == bp_catch_catch
	  || b->type == bp_catch_throw
	  /* APPLE LOCAL begin gnu_v3 */
	  || b->type == bp_gnu_v3_catch_catch
	  || b->type == bp_gnu_v3_catch_throw
	  /* APPLE LOCAL end gnu_v3 */
	  || b->type == bp_hardware_breakpoint
	  || b->type == bp_watchpoint
	  || b->type == bp_read_watchpoint
	  || b->type == bp_access_watchpoint
	  || b->type == bp_hardware_watchpoint);
}
	
/* Print information on user settable breakpoint (watchpoint, etc)
   number BNUM.  If BNUM is -1 print all user settable breakpoints.
   If ALLFLAG is non-zero, include non- user settable breakpoints. */

static void
breakpoint_1 (int bnum, int allflag)
{
  struct breakpoint *b;
  CORE_ADDR last_addr = (CORE_ADDR) -1;
  int nr_printable_breakpoints;
  struct cleanup *bkpttbl_chain;
  
  /* Compute the number of rows in the table. */
  nr_printable_breakpoints = 0;
  ALL_BREAKPOINTS (b)
    if (bnum == -1
	|| bnum == b->number)
      {
	if (allflag || user_settable_breakpoint (b))
	  nr_printable_breakpoints++;
      }

  if (addressprint)
    bkpttbl_chain 
      = make_cleanup_ui_out_table_begin_end (uiout, 6, nr_printable_breakpoints,
                                             "BreakpointTable");
  else
    bkpttbl_chain 
      = make_cleanup_ui_out_table_begin_end (uiout, 5, nr_printable_breakpoints,
                                             "BreakpointTable");

  if (nr_printable_breakpoints > 0)
    annotate_breakpoints_headers ();
  if (nr_printable_breakpoints > 0)
    annotate_field (0);
  ui_out_table_header (uiout, 3, ui_left, "number", "Num");		/* 1 */
  if (nr_printable_breakpoints > 0)
    annotate_field (1);
  ui_out_table_header (uiout, 14, ui_left, "type", "Type");		/* 2 */
  if (nr_printable_breakpoints > 0)
    annotate_field (2);
  ui_out_table_header (uiout, 4, ui_left, "disp", "Disp");		/* 3 */
  if (nr_printable_breakpoints > 0)
    annotate_field (3);
  ui_out_table_header (uiout, 3, ui_left, "enabled", "Enb");	/* 4 */
  if (addressprint)
	{
	  if (nr_printable_breakpoints > 0)
	    annotate_field (4);
	  if (TARGET_ADDR_BIT <= 32)
	    ui_out_table_header (uiout, 10, ui_left, "addr", "Address");/* 5 */
	  else
	    ui_out_table_header (uiout, 18, ui_left, "addr", "Address");/* 5 */
	}
  if (nr_printable_breakpoints > 0)
    annotate_field (5);
  ui_out_table_header (uiout, 40, ui_noalign, "what", "What");	/* 6 */
  ui_out_table_body (uiout);
  if (nr_printable_breakpoints > 0)
    annotate_breakpoints_table ();

  ALL_BREAKPOINTS (b)
    if (bnum == -1
	|| bnum == b->number)
      {
	/* We only print out user settable breakpoints unless the
	   allflag is set. */
	if (allflag || user_settable_breakpoint (b))
	  print_one_breakpoint (b, &last_addr);
      }
  
  do_cleanups (bkpttbl_chain);

  if (nr_printable_breakpoints == 0)
    {
      if (bnum == -1)
	ui_out_message (uiout, 0, "No breakpoints or watchpoints.\n");
      else
	ui_out_message (uiout, 0, "No breakpoint or watchpoint number %d.\n",
			bnum);
    }
  else
    {
      /* Compare against (CORE_ADDR)-1 in case some compiler decides
	 that a comparison of an unsigned with -1 is always false.  */
      if (last_addr != (CORE_ADDR) -1)
	set_next_address (last_addr);
    }

  /* FIXME? Should this be moved up so that it is only called when
     there have been breakpoints? */
  annotate_breakpoints_table_end ();
}

static void
breakpoints_info (char *bnum_exp, int from_tty)
{
  int bnum = -1;

  if (bnum_exp)
    bnum = parse_and_eval_long (bnum_exp);

  breakpoint_1 (bnum, 0);
}

static void
maintenance_info_breakpoints (char *bnum_exp, int from_tty)
{
  int bnum = -1;

  if (bnum_exp)
    bnum = parse_and_eval_long (bnum_exp);

  breakpoint_1 (bnum, 1);
}

/* Print a message describing any breakpoints set at PC.  */

static void
describe_other_breakpoints (CORE_ADDR pc, asection *section)
{
  int others = 0;
  struct breakpoint *b;

  ALL_BREAKPOINTS (b)
    if (b->loc->address == pc)	/* address match / overlay match */
      /* APPLE LOCAL begin */
      if (!b->pending && (!overlay_debugging || b->loc->section == section)
	  && breakpoint_address_is_meaningful (b))
	/* APPLE LOCAL end */
	others++;
  if (others > 0)
    {
      if (others == 1)
	printf_filtered (_("Note: breakpoint "));
      else /* if (others == ???) */
	printf_filtered (_("Note: breakpoints "));
      ALL_BREAKPOINTS (b)
	if (b->loc->address == pc)	/* address match / overlay match */
	  /* APPLE LOCAL begin */
	  if (!b->pending && (!overlay_debugging || b->loc->section == section)
	      && breakpoint_address_is_meaningful (b))
	    /* APPLE LOCAL end */
	    {
	      others--;
	      printf_filtered ("%d%s%s ",
			       b->number,
			       ((b->enable_state == bp_disabled || 
				 b->enable_state == bp_shlib_disabled || 
				 b->enable_state == bp_call_disabled) 
				? " (disabled)" 
				: b->enable_state == bp_permanent 
				? " (permanent)"
				: ""),
			       (others > 1) ? "," 
			       : ((others == 1) ? " and" : ""));
	    }
      printf_filtered (_("also set at pc "));
      deprecated_print_address_numeric (pc, 1, gdb_stdout);
      printf_filtered (".\n");
    }
}

/* Set the default place to put a breakpoint
   for the `break' command with no arguments.  */

void
set_default_breakpoint (int valid, CORE_ADDR addr, struct symtab *symtab,
			int line)
{
  default_breakpoint_valid = valid;
  default_breakpoint_address = addr;
  default_breakpoint_symtab = symtab;
  default_breakpoint_line = line;
}

/* Return true iff it is meaningful to use the address member of
   BPT.  For some breakpoint types, the address member is irrelevant
   and it makes no sense to attempt to compare it to other addresses
   (or use it for any other purpose either).

   More specifically, each of the following breakpoint types will always
   have a zero valued address and we don't want check_duplicates() to mark
   breakpoints of any of these types to be a duplicate of an actual
   breakpoint at address zero:

      bp_watchpoint
      bp_hardware_watchpoint
      bp_read_watchpoint
      bp_access_watchpoint
      bp_catch_exec
      bp_longjmp_resume
      bp_catch_fork
      bp_catch_vork */

static int
breakpoint_address_is_meaningful (struct breakpoint *bpt)
{
  enum bptype type = bpt->type;

  return (type != bp_watchpoint
	  && type != bp_hardware_watchpoint
	  && type != bp_read_watchpoint
	  && type != bp_access_watchpoint
	  && type != bp_catch_exec
	  && type != bp_longjmp_resume
	  && type != bp_catch_fork
	  && type != bp_catch_vfork);
}

/* Rescan breakpoints at the same address and section as BPT,
   marking the first one as "first" and any others as "duplicates".
   This is so that the bpt instruction is only inserted once.
   If we have a permanent breakpoint at the same place as BPT, make
   that one the official one, and the rest as duplicates.  */

static void
check_duplicates (struct breakpoint *bpt)
{
  struct bp_location *b;
  int count = 0;
  struct bp_location *perm_bp = 0;
  CORE_ADDR address = bpt->loc->address;
  asection *section = bpt->loc->section;

  if (! breakpoint_address_is_meaningful (bpt))
    return;

  ALL_BP_LOCATIONS (b)
    if (b->owner->enable_state != bp_disabled
	&& b->owner->enable_state != bp_shlib_disabled
	&& !b->owner->pending
	&& b->owner->enable_state != bp_call_disabled
	&& b->address == address	/* address / overlay match */
	&& (!overlay_debugging || b->section == section)
	&& breakpoint_address_is_meaningful (b->owner))
    {
      /* Have we found a permanent breakpoint?  */
      if (b->owner->enable_state == bp_permanent)
	{
	  perm_bp = b;
	  break;
	}
	
      count++;
      b->duplicate = count > 1;
    }

  /* If we found a permanent breakpoint at this address, go over the
     list again and declare all the other breakpoints there to be the
     duplicates.  */
  if (perm_bp)
    {
      perm_bp->duplicate = 0;

      /* Permanent breakpoint should always be inserted.  */
      if (! perm_bp->inserted)
	internal_error (__FILE__, __LINE__,
			_("allegedly permanent breakpoint is not "
			"actually inserted"));

      ALL_BP_LOCATIONS (b)
	if (b != perm_bp)
	  {
	    if (b->owner->enable_state != bp_disabled
		&& b->owner->enable_state != bp_shlib_disabled
		&& !b->owner->pending
		&& b->owner->enable_state != bp_call_disabled
		&& b->address == address	/* address / overlay match */
		&& (!overlay_debugging || b->section == section)
		&& breakpoint_address_is_meaningful (b->owner))
	      {
		if (b->inserted)
		  internal_error (__FILE__, __LINE__,
				  _("another breakpoint was inserted on top of "
				  "a permanent breakpoint"));

		b->duplicate = 1;
	      }
	  }
    }
}

static void
breakpoint_adjustment_warning (CORE_ADDR from_addr, CORE_ADDR to_addr,
                               int bnum, int have_bnum)
{
  char astr1[40];
  char astr2[40];

  strcpy (astr1, hex_string_custom ((unsigned long) from_addr, 8));
  strcpy (astr2, hex_string_custom ((unsigned long) to_addr, 8));
  if (have_bnum)
    warning (_("Breakpoint %d address previously adjusted from %s to %s."),
             bnum, astr1, astr2);
  else
    warning (_("Breakpoint address adjusted from %s to %s."), astr1, astr2);
}

/* Adjust a breakpoint's address to account for architectural constraints
   on breakpoint placement.  Return the adjusted address.  Note: Very
   few targets require this kind of adjustment.  For most targets,
   this function is simply the identity function.  */

static CORE_ADDR
adjust_breakpoint_address (CORE_ADDR bpaddr, enum bptype bptype)
{
  if (!gdbarch_adjust_breakpoint_address_p (current_gdbarch))
    {
      /* Very few targets need any kind of breakpoint adjustment.  */
      return bpaddr;
    }
  else if (bptype == bp_watchpoint
           || bptype == bp_hardware_watchpoint
           || bptype == bp_read_watchpoint
           || bptype == bp_access_watchpoint
           || bptype == bp_catch_fork
           || bptype == bp_catch_vfork
           || bptype == bp_catch_exec)
    {
      /* Watchpoints and the various bp_catch_* eventpoints should not
         have their addresses modified.  */
      return bpaddr;
    }
  else
    {
      CORE_ADDR adjusted_bpaddr;

      /* Some targets have architectural constraints on the placement
         of breakpoint instructions.  Obtain the adjusted address.  */
      adjusted_bpaddr = gdbarch_adjust_breakpoint_address (current_gdbarch,
                                                           bpaddr);

      /* An adjusted breakpoint address can significantly alter
         a user's expectations.  Print a warning if an adjustment
	 is required.  */
      if (adjusted_bpaddr != bpaddr)
	breakpoint_adjustment_warning (bpaddr, adjusted_bpaddr, 0, 0);

      return adjusted_bpaddr;
    }
}

/* Allocate a struct bp_location.  */

static struct bp_location *
allocate_bp_location (struct breakpoint *bpt, enum bptype bp_type)
{
  struct bp_location *loc, *loc_p;

  loc = xmalloc (sizeof (struct bp_location));
  memset (loc, 0, sizeof (*loc));

  loc->owner = bpt;

  switch (bp_type)
    {
    case bp_breakpoint:
    /* APPLE LOCAL begin subroutine inlining  */
    case bp_inlined_breakpoint:
    /* APPLE LOCAL end subroutine inlining  */
    case bp_until:
    case bp_finish:
    case bp_longjmp:
    case bp_longjmp_resume:
    case bp_step_resume:
    case bp_through_sigtramp:
    case bp_watchpoint_scope:
    case bp_call_dummy:
    case bp_shlib_event:
    case bp_thread_event:
    case bp_overlay_event:
    case bp_catch_load:
    case bp_catch_unload:
    /* APPLE LOCAL begin gnu_v3 */
      /* Handle gnu_v3_catch types as ordinary breakpoints.  */
    case bp_gnu_v3_catch_catch:
    case bp_gnu_v3_catch_throw:
      /* APPLE LOCAL end gnu_v3 */
      loc->loc_type = bp_loc_software_breakpoint;
      break;
    case bp_hardware_breakpoint:
      loc->loc_type = bp_loc_hardware_breakpoint;
      break;
    case bp_hardware_watchpoint:
    case bp_read_watchpoint:
    case bp_access_watchpoint:
      loc->loc_type = bp_loc_hardware_watchpoint;
      break;
    case bp_watchpoint:
    case bp_catch_fork:
    case bp_catch_vfork:
    case bp_catch_exec:
    case bp_catch_catch:
    case bp_catch_throw:
      loc->loc_type = bp_loc_other;
      break;
    default:
      internal_error (__FILE__, __LINE__, _("unknown breakpoint type"));
    }

  /* Add this breakpoint to the end of the chain.  */

  loc_p = bp_location_chain;
  if (loc_p == 0)
    bp_location_chain = loc;
  else
    {
      while (loc_p->next)
	loc_p = loc_p->next;
      loc_p->next = loc;
    }

  return loc;
}

/* APPLE LOCAL: Sets the bp_objfile for breakpoint B based
   on the sal SAL used to set the breakpoint.  */

static
void set_bp_objfile (struct breakpoint *b, struct symtab_and_line *sal)
{
  if (sal->symtab != NULL)
    b->bp_objfile = sal->symtab->objfile;
  else if (sal->section != NULL)
    {
      struct obj_section *osect;
      osect = find_pc_sect_section (sal->pc, sal->section);
      if (osect)
        b->bp_objfile = osect->objfile;
    }
  else
    {
      struct obj_section *osect;
      osect = find_pc_section (sal->pc);
      if (osect)
        b->bp_objfile = osect->objfile;
    }
  /* If the breakpoint's objfile is set in a separate debug objfile, then report
     the breakpoint as being set in the original objfile.  */
  if (b->bp_objfile && b->bp_objfile->separate_debug_objfile_backlink)
    b->bp_objfile = b->bp_objfile->separate_debug_objfile_backlink;
}
/* END APPLE LOCAL  */

/* set_raw_breakpoint() is a low level routine for allocating and
   partially initializing a breakpoint of type BPTYPE.  The newly
   created breakpoint's address, section, source file name, and line
   number are provided by SAL.  The newly created and partially
   initialized breakpoint is added to the breakpoint chain and
   is also returned as the value of this function.

   It is expected that the caller will complete the initialization of
   the newly created breakpoint struct as well as output any status
   information regarding the creation of a new breakpoint.  In
   particular, set_raw_breakpoint() does NOT set the breakpoint
   number!  Care should be taken to not allow an error() to occur
   prior to completing the initialization of the breakpoint.  If this
   should happen, a bogus breakpoint will be left on the chain.  */

/* APPLE LOCAL: Added pending_p so we can not set the breakpoint shlib if 
   we are pending.  */

struct breakpoint *
set_raw_breakpoint (struct symtab_and_line sal, enum bptype bptype, int pending_p)
{
  struct breakpoint *b, *b1;

  b = (struct breakpoint *) xmalloc (sizeof (struct breakpoint));
  memset (b, 0, sizeof (*b));
  b->loc = allocate_bp_location (b, bptype);
  b->loc->requested_address = sal.pc;
  b->loc->address = adjust_breakpoint_address (b->loc->requested_address,
                                               bptype);
  if (sal.symtab == NULL)
    b->source_file = NULL;
  else
    b->source_file = savestring (sal.symtab->filename,
				 strlen (sal.symtab->filename));
  b->loc->section = sal.section;
  b->type = bptype;
  b->language = current_language->la_language;
  b->input_radix = input_radix;
  b->thread = -1;
  b->line_number = sal.line;
  b->enable_state = bp_enabled;
  b->next = 0;
  b->silent = 0;
  b->ignore_count = 0;
  b->commands = NULL;
  b->frame_id = null_frame_id;
  b->dll_pathname = NULL;
  b->triggered_dll_pathname = NULL;
  b->forked_inferior_pid = 0;
  b->exec_pathname = NULL;
  b->ops = NULL;
  b->pending = pending_p;
  /* APPLE LOCAL: NULL out the fields that support setting the 
     breakpoint in a requested_shlib, and for tracking which objfile
     a breakpoint gets set in.  */
  b->requested_shlib = NULL;
  b->bp_objfile = NULL;
  /* APPLE LOCAL radar 5273932  */
  b->bp_objfile_name = NULL;

  /* APPLE LOCAL: Set the bp_objfile to the objfile that contains
     this breakpoint.  Before inserting it, we'll run try
     target_check_is_objfile_loaded() and if the objfile pointer is
     0x0 or the objfile hasn't been loaded, the breakpoint won't be
     inserted.  */
  if (!b->pending)
    set_bp_objfile (b, &sal);

  /* Add this breakpoint to the end of the chain
     so that a list of breakpoints will come out in order
     of increasing numbers.  */

  b1 = breakpoint_chain;
  if (b1 == 0)
    breakpoint_chain = b;
  else
    {
      while (b1->next)
	b1 = b1->next;
      b1->next = b;
    }

  check_duplicates (b);
  breakpoints_changed ();

  return b;
}


/* Note that the breakpoint object B describes a permanent breakpoint
   instruction, hard-wired into the inferior's code.  */
void
make_breakpoint_permanent (struct breakpoint *b)
{
  b->enable_state = bp_permanent;

  /* By definition, permanent breakpoints are already present in the code.  */
  b->loc->inserted = 1;
}

static struct breakpoint *
create_internal_breakpoint (CORE_ADDR address, enum bptype type)
{
  static int internal_breakpoint_number = -1;
  struct symtab_and_line sal;
  struct breakpoint *b;

  init_sal (&sal);		/* initialize to zeroes */

  sal.pc = address;
  sal.section = find_pc_overlay (sal.pc);

  b = set_raw_breakpoint (sal, type, 0);
  b->number = internal_breakpoint_number--;
  b->disposition = disp_donttouch;
  /* APPLE_LOCAL: We added the bp_set_state flag.  */
  b->bp_set_state = bp_state_set;

  return b;
}


static void
create_longjmp_breakpoint (char *func_name)
{
  struct breakpoint *b;
  struct minimal_symbol *m;

  if (func_name == NULL)
    b = create_internal_breakpoint (0, bp_longjmp_resume);
  else
    {
      if ((m = lookup_minimal_symbol_text (func_name, NULL)) == NULL)
	return;
 
      b = create_internal_breakpoint (SYMBOL_VALUE_ADDRESS (m), bp_longjmp);
    }

  b->enable_state = bp_disabled;
  b->silent = 1;
  if (func_name)
    b->addr_string = xstrdup (func_name);
}

/* Call this routine when stepping and nexting to enable a breakpoint
   if we do a longjmp().  When we hit that breakpoint, call
   set_longjmp_resume_breakpoint() to figure out where we are going. */

void
enable_longjmp_breakpoint (void)
{
  struct breakpoint *b;

  ALL_BREAKPOINTS (b)
    if (b->type == bp_longjmp)
    {
      b->enable_state = bp_enabled;
      check_duplicates (b);
    }
}

void
disable_longjmp_breakpoint (void)
{
  struct breakpoint *b;

  ALL_BREAKPOINTS (b)
    if (b->type == bp_longjmp
	|| b->type == bp_longjmp_resume)
    {
      b->enable_state = bp_disabled;
      check_duplicates (b);
    }
}

static void
create_overlay_event_breakpoint (char *func_name)
{
  struct breakpoint *b;
  struct minimal_symbol *m;

  if ((m = lookup_minimal_symbol_text (func_name, NULL)) == NULL)
    return;
 
  b = create_internal_breakpoint (SYMBOL_VALUE_ADDRESS (m), 
				  bp_overlay_event);
  b->addr_string = xstrdup (func_name);

  if (overlay_debugging == ovly_auto)
    {
      b->enable_state = bp_enabled;
      overlay_events_enabled = 1;
    }
  else 
    {
      b->enable_state = bp_disabled;
      overlay_events_enabled = 0;
    }
}

void
enable_overlay_breakpoints (void)
{
  struct breakpoint *b;

  ALL_BREAKPOINTS (b)
    if (b->type == bp_overlay_event)
    {
      b->enable_state = bp_enabled;
      check_duplicates (b);
      overlay_events_enabled = 1;
    }
}

void
disable_overlay_breakpoints (void)
{
  struct breakpoint *b;

  ALL_BREAKPOINTS (b)
    if (b->type == bp_overlay_event)
    {
      b->enable_state = bp_disabled;
      check_duplicates (b);
      overlay_events_enabled = 0;
    }
}

struct breakpoint *
create_thread_event_breakpoint (CORE_ADDR address)
{
  struct breakpoint *b;

  b = create_internal_breakpoint (address, bp_thread_event);
  
  b->enable_state = bp_enabled;
  /* addr_string has to be used or breakpoint_re_set will delete me.  */
  b->addr_string = xstrprintf ("*0x%s", paddr (b->loc->address));

  return b;
}

void
remove_thread_event_breakpoints (void)
{
  struct breakpoint *b, *temp;

  ALL_BREAKPOINTS_SAFE (b, temp)
    if (b->type == bp_thread_event)
      delete_breakpoint (b);
}

struct captured_parse_breakpoint_args
  {
    char **arg_p;
    struct symtabs_and_lines *sals_p;
    char ***addr_string_p;
    /* APPLE LOCAL requested shlib */
    char *requested_shlib;
    int *not_found_ptr;
  };

struct lang_and_radix
  {
    enum language lang;
    int radix;
  };

/* Cleanup helper routine to restore the current language and
   input radix.  */
static void
do_restore_lang_radix_cleanup (void *old)
{
  struct lang_and_radix *p = old;
  set_language (p->lang);
  input_radix = p->radix;
}

/* Try and resolve a pending breakpoint.  */
/* APPLE LOCAL begin comment */
/* If the breakpoint is successfully resolved, then the original
   pended breakpoint will be deleted, and one or more new breakpoints
   will be created.  */
/* APPLE LOCAL end comment */
static int
resolve_pending_breakpoint (struct breakpoint *b)
{
  /* Try and reparse the breakpoint in case the shared library
     is now loaded.  */
  /* APPLE LOCAL break_command_2 */
  int rc;
  /* APPLE LOCAL break_command_2 */
  struct lang_and_radix old_lr;
  struct cleanup *old_chain;
  /* APPLE LOCAL break_command_2 */
  int choices[1] = {-1};

  /* Set language, input-radix, then reissue breakpoint command. 
     Ensure the language and input-radix are restored afterwards.  */
  old_lr.lang = current_language->la_language;
  old_lr.radix = input_radix;
  old_chain = make_cleanup (do_restore_lang_radix_cleanup, &old_lr);
  
  set_language (b->language);
  input_radix = b->input_radix;
  /* APPLE LOCAL: Changed this to break_command_2, and now pass
     a "choices" array to choose ALL matches if there are multiple
     possibilities.  */

  rc = break_command_2 (b->addr_string, b->flag, b->from_tty, b,
			b->requested_shlib, choices, NULL);
  
  if (rc == GDB_RC_OK)
    /* APPLE LOCAL begin breakpoints */
    {
      /* Pending breakpoint has been resolved.  */
      printf_filtered ("Pending breakpoint %d - \"%s\" resolved\n", b->number, b->addr_string);
      delete_breakpoint (b);
    }
  /* APPLE LOCAL end breakpoints */
  do_cleanups (old_chain);
  return rc;
}

void
remove_solib_event_breakpoints (void)
{
  struct breakpoint *b, *temp;

  ALL_BREAKPOINTS_SAFE (b, temp)
    if (b->type == bp_shlib_event)
      delete_breakpoint (b);
}

struct breakpoint *
create_solib_event_breakpoint (CORE_ADDR address)
{
  struct breakpoint *b;

  b = create_internal_breakpoint (address, bp_shlib_event);
  return b;
}

/* Disable any breakpoints that are on code in shared libraries.  Only
   apply to enabled breakpoints, disabled ones can just stay disabled.  */

void
disable_breakpoints_in_shlibs (int silent)
{
  struct breakpoint *b;
  int disabled_shlib_breaks = 0;

  /* See also: insert_breakpoints, under DISABLE_UNSETTABLE_BREAK. */
  ALL_BREAKPOINTS (b)
  {
    /* APPLE LOCAL breakpoints */
#if defined (PC_SOLIB)
    /* APPLE LOCAL begin subroutine inlining  */
    if (((b->type == bp_breakpoint) || (b->type == bp_hardware_breakpoint)
	 || (b->type == bp_inlined_breakpoint))
    /* APPLE LOCAL end subroutine inlining  */
      /* APPLE LOCAL: I removed the "&& PC_SOLIB (b->loc->address)"
	 from the condition, I am pretty sure it is wrong.
	 b->loc->address could come from a pre-running address, which
	 by the time we get to here could no longer have a valid
	 library under it.  In that case we CERTAINLY would want to
	 disable the breakpoint... */
	&& breakpoint_enabled (b) && !b->loc->duplicate)
      {
	b->enable_state = bp_shlib_disabled;
	if (!silent)
	  {
	    if (!disabled_shlib_breaks)
	      {
		target_terminal_ours_for_output ();
		warning (_("Temporarily disabling shared library breakpoints:"));
	      }
	    disabled_shlib_breaks = 1;
	    warning (_("breakpoint #%d "), b->number);
	  }
      }
    /* APPLE LOCAL breakpoints */
#endif
  }
  /* APPLE LOCAL begin breakpoints */
  if (!silent && disabled_shlib_breaks)
    printf_filtered ("\n");
  /* APPLE LOCAL end breakpoints */
}

/* Disable any breakpoints that are in in an unloaded shared library.  Only
   apply to enabled breakpoints, disabled ones can just stay disabled.  */

void
disable_breakpoints_in_unloaded_shlib (struct so_list *solib)
{
  struct breakpoint *b;
  int disabled_shlib_breaks = 0;

  /* See also: insert_breakpoints, under DISABLE_UNSETTABLE_BREAK.  */
  ALL_BREAKPOINTS (b)
  {
    if ((b->loc->loc_type == bp_loc_hardware_breakpoint
	|| b->loc->loc_type == bp_loc_software_breakpoint)
	&& breakpoint_enabled (b) && !b->loc->duplicate)
      {
#ifdef PC_SOLIB
	char *so_name = PC_SOLIB (b->loc->address);
#else
	char *so_name = solib_address (b->loc->address);
#endif
	if (so_name && !strcmp (so_name, solib->so_name))
          {
	    b->enable_state = bp_shlib_disabled;
	    /* At this point, we cannot rely on remove_breakpoint
	       succeeding so we must mark the breakpoint as not inserted
	       to prevent future errors occurring in remove_breakpoints.  */
	    b->loc->inserted = 0;
	    if (!disabled_shlib_breaks)
	      {
		target_terminal_ours_for_output ();
		warning (_("Temporarily disabling breakpoints for unloaded shared library \"%s\""),
			  so_name);
	      }
	    disabled_shlib_breaks = 1;
	  }
      }
  }
}

/* Try to reenable any breakpoints in shared libraries.  */
void
/* APPLE LOCAL control silencing of breakpoint re-enable */
re_enable_breakpoints_in_shlibs (int silent)
{
  struct breakpoint *b, *tmp;

  ALL_BREAKPOINTS_SAFE (b, tmp)
  {
    if (b->enable_state == bp_shlib_disabled)
      {
	gdb_byte buf[1];
	/* APPLE LOCAL we don't use lib */
	
	/* Do not reenable the breakpoint if the shared library is
	   still not mapped in.  */
	/* APPLE LOCAL begin don't use PC_SOLIB */
	/* We can't use the PC_SOLIB test here, because the Mac OS X
	   version currently just returns NULL...  */
	if (target_read_memory (b->loc->address, buf, 1) == 0)
	  /* APPLE LOCAL end don't use PC_SOLIB */
	  {
	    b->enable_state = bp_enabled;
	    /* APPLE LOCAL begin control silencing of breakpoint re-enable */
	    if (!silent)
	      {
		target_terminal_ours_for_output ();
		printf_filtered ("Re-enabling shared library breakpoint %d\n", 
				 b->number);
	      }
	    /* APPLE LOCAL end control silencing of breakpoint re-enable */
	  }
      }
    else if (b->pending && (b->enable_state == bp_enabled))
      {
	/* APPLE LOCAL: We do this while we are parsing shlib load
	   messages, so we better not let any uncaught errors
	   propagate.  */
	char *message = xstrprintf ("Error resolving pending breakpoint %d: ",
					b->number);
	struct cleanup *cleanups = make_cleanup (xfree, message);
	catch_errors ((catch_errors_ftype *) resolve_pending_breakpoint, (void *) b, 
		    message, RETURN_MASK_ALL);
	do_cleanups (cleanups);
      }
  }
}

static void
solib_load_unload_1 (char *hookname, int tempflag, char *dll_pathname,
		     char *cond_string, enum bptype bp_kind)
{
  struct breakpoint *b;
  struct symtabs_and_lines sals;
  struct cleanup *old_chain;
  struct cleanup *canonical_strings_chain = NULL;
  char *addr_start = hookname;
  char *addr_end = NULL;
  char **canonical = (char **) NULL;
  int thread = -1;		/* All threads. */

  /* Set a breakpoint on the specified hook.  */
  /* APPLE LOCAL begin return multiple symbols */
  sals = decode_line_1 (&hookname, 1, (struct symtab *) NULL, 
			0, &canonical, NULL, 0);
  /* APPLE LOCAL end return multiple symbols  */

  addr_end = hookname;

  if (sals.nelts == 0)
    {
      warning (_("Unable to set a breakpoint on dynamic linker callback.\n"
		 "Suggest linking with /opt/langtools/lib/end.o.\n"
		 "GDB will be unable to track shl_load/shl_unload calls."));
      return;
    }
  if (sals.nelts != 1)
    {
      warning (_("Unable to set unique breakpoint on dynamic linker callback.\n"
		 "GDB will be unable to track shl_load/shl_unload calls."));
      return;
    }

  /* Make sure that all storage allocated in decode_line_1 gets freed
     in case the following errors out.  */
  old_chain = make_cleanup (xfree, sals.sals);
  if (canonical != (char **) NULL)
    {
      make_cleanup (xfree, canonical);
      canonical_strings_chain = make_cleanup (null_cleanup, 0);
      if (canonical[0] != NULL)
	make_cleanup (xfree, canonical[0]);
    }

  resolve_sal_pc (&sals.sals[0]);

  /* Remove the canonical strings from the cleanup, they are needed below.  */
  if (canonical != (char **) NULL)
    discard_cleanups (canonical_strings_chain);

  b = set_raw_breakpoint (sals.sals[0], bp_kind, 0);
  set_breakpoint_count (breakpoint_count + 1);
  b->number = breakpoint_count;
  b->cond = NULL;
  b->cond_string = (cond_string == NULL) ? 
    NULL : savestring (cond_string, strlen (cond_string));
  b->thread = thread;

  if (canonical != (char **) NULL && canonical[0] != NULL)
    b->addr_string = canonical[0];
  else if (addr_start)
    b->addr_string = savestring (addr_start, addr_end - addr_start);

  b->enable_state = bp_enabled;
  b->disposition = tempflag ? disp_del : disp_donttouch;

  if (dll_pathname == NULL)
    b->dll_pathname = NULL;
  else
    {
      b->dll_pathname = (char *) xmalloc (strlen (dll_pathname) + 1);
      strcpy (b->dll_pathname, dll_pathname);
    }

  mention (b);
  do_cleanups (old_chain);
}

void
create_solib_load_event_breakpoint (char *hookname, int tempflag,
				    char *dll_pathname, char *cond_string)
{
  solib_load_unload_1 (hookname, tempflag, dll_pathname, 
		       cond_string, bp_catch_load);
}

void
create_solib_unload_event_breakpoint (char *hookname, int tempflag,
				      char *dll_pathname, char *cond_string)
{
  solib_load_unload_1 (hookname, tempflag, dll_pathname, 
		       cond_string, bp_catch_unload);
}

static void
create_fork_vfork_event_catchpoint (int tempflag, char *cond_string,
				    enum bptype bp_kind)
{
  struct symtab_and_line sal;
  struct breakpoint *b;
  int thread = -1;		/* All threads. */

  init_sal (&sal);
  sal.pc = 0;
  sal.symtab = NULL;
  sal.line = 0;

  b = set_raw_breakpoint (sal, bp_kind, 0);
  set_breakpoint_count (breakpoint_count + 1);
  b->number = breakpoint_count;
  b->cond = NULL;
  b->cond_string = (cond_string == NULL) ? 
    NULL : savestring (cond_string, strlen (cond_string));
  b->thread = thread;
  b->addr_string = NULL;
  b->enable_state = bp_enabled;
  b->disposition = tempflag ? disp_del : disp_donttouch;
  b->forked_inferior_pid = 0;

  mention (b);
}

void
create_fork_event_catchpoint (int tempflag, char *cond_string)
{
  create_fork_vfork_event_catchpoint (tempflag, cond_string, bp_catch_fork);
}

void
create_vfork_event_catchpoint (int tempflag, char *cond_string)
{
  create_fork_vfork_event_catchpoint (tempflag, cond_string, bp_catch_vfork);
}

void
create_exec_event_catchpoint (int tempflag, char *cond_string)
{
  struct symtab_and_line sal;
  struct breakpoint *b;
  int thread = -1;		/* All threads. */

  init_sal (&sal);
  sal.pc = 0;
  sal.symtab = NULL;
  sal.line = 0;

  b = set_raw_breakpoint (sal, bp_catch_exec, 0);
  set_breakpoint_count (breakpoint_count + 1);
  b->number = breakpoint_count;
  b->cond = NULL;
  b->cond_string = (cond_string == NULL) ?
    NULL : savestring (cond_string, strlen (cond_string));
  b->thread = thread;
  b->addr_string = NULL;
  b->enable_state = bp_enabled;
  b->disposition = tempflag ? disp_del : disp_donttouch;

  mention (b);
}

static int
hw_breakpoint_used_count (void)
{
  struct breakpoint *b;
  int i = 0;

  ALL_BREAKPOINTS (b)
  {
    if (b->type == bp_hardware_breakpoint && b->enable_state == bp_enabled)
      i++;
  }

  return i;
}

static int
hw_watchpoint_used_count (enum bptype type, int *other_type_used)
{
  struct breakpoint *b;
  int i = 0;

  *other_type_used = 0;
  ALL_BREAKPOINTS (b)
  {
    if (breakpoint_enabled (b))
      {
	if (b->type == type)
	  i++;
	else if ((b->type == bp_hardware_watchpoint ||
		  b->type == bp_read_watchpoint ||
		  b->type == bp_access_watchpoint))
	  *other_type_used = 1;
      }
  }
  return i;
}

/* Call this after hitting the longjmp() breakpoint.  Use this to set
   a new breakpoint at the target of the jmp_buf.

   FIXME - This ought to be done by setting a temporary breakpoint
   that gets deleted automatically... */

void
set_longjmp_resume_breakpoint (CORE_ADDR pc, struct frame_id frame_id)
{
  struct breakpoint *b;

  ALL_BREAKPOINTS (b)
    if (b->type == bp_longjmp_resume)
    {
      b->loc->requested_address = pc;
      b->loc->address = adjust_breakpoint_address (b->loc->requested_address,
                                                   b->type);
      b->enable_state = bp_enabled;
      b->frame_id = frame_id;
      check_duplicates (b);
      return;
    }
}

void
disable_watchpoints_before_interactive_call_start (void)
{
  struct breakpoint *b;

  ALL_BREAKPOINTS (b)
  {
    if (((b->type == bp_watchpoint)
	 || (b->type == bp_hardware_watchpoint)
	 || (b->type == bp_read_watchpoint)
	 || (b->type == bp_access_watchpoint)
	 || ep_is_exception_catchpoint (b))
	&& breakpoint_enabled (b))
      {
	b->enable_state = bp_call_disabled;
	check_duplicates (b);
      }
  }
}

void
enable_watchpoints_after_interactive_call_stop (void)
{
  struct breakpoint *b;

  ALL_BREAKPOINTS (b)
  {
    if (((b->type == bp_watchpoint)
	 || (b->type == bp_hardware_watchpoint)
	 || (b->type == bp_read_watchpoint)
	 || (b->type == bp_access_watchpoint)
	 || ep_is_exception_catchpoint (b))
	&& (b->enable_state == bp_call_disabled))
      {
	b->enable_state = bp_enabled;
	check_duplicates (b);
      }
  }
}


/* Set a breakpoint that will evaporate an end of command
   at address specified by SAL.
   Restrict it to frame FRAME if FRAME is nonzero.  */

struct breakpoint *
set_momentary_breakpoint (struct symtab_and_line sal, struct frame_id frame_id,
			  enum bptype type)
{
  struct breakpoint *b;
  b = set_raw_breakpoint (sal, type, 0);
  b->enable_state = bp_enabled;
  b->disposition = disp_donttouch;
  b->frame_id = frame_id;

  /* If we're debugging a multi-threaded program, then we
     want momentary breakpoints to be active in only a 
     single thread of control.  */
  if (in_thread_list (inferior_ptid))
    b->thread = pid_to_thread_id (inferior_ptid);

  return b;
}


/* Tell the user we have just set a breakpoint B.  */

static void
mention (struct breakpoint *b)
{
  int say_where = 0;
  struct cleanup *old_chain, *ui_out_chain;
  struct ui_stream *stb;

  /* APPLE LOCAL: We set this when we are doing breakpoint resetting to cut down
     on the spam when we are resetting breakpoints.  */
  if (dont_mention)
    return;

  stb = ui_out_stream_new (uiout);
  old_chain = make_cleanup_ui_out_stream_delete (stb);

  /* FIXME: This is misplaced; mention() is called by things (like
     hitting a watchpoint) other than breakpoint creation.  It should
     be possible to clean this up and at the same time replace the
     random calls to breakpoint_changed with this hook, as has already
     been done for deprecated_delete_breakpoint_hook and so on.  */
  if (deprecated_create_breakpoint_hook)
    deprecated_create_breakpoint_hook (b);
  breakpoint_create_event (b->number);

  if (b->ops != NULL && b->ops->print_mention != NULL)
    b->ops->print_mention (b);
  else
    switch (b->type)
      {
      case bp_none:
	printf_filtered (_("(apparently deleted?) Eventpoint %d: "), b->number);
	break;
      case bp_watchpoint:
	ui_out_text (uiout, "Watchpoint ");
	ui_out_chain = make_cleanup_ui_out_tuple_begin_end (uiout, "wpt");
	ui_out_field_int (uiout, "number", b->number);
	ui_out_text (uiout, ": ");
	/* APPLE LOCAL gdb_print_expression */
	gdb_print_expression (b->exp, stb->stream);
	ui_out_field_stream (uiout, "exp", stb);
	do_cleanups (ui_out_chain);
	break;
      case bp_hardware_watchpoint:
	ui_out_text (uiout, "Hardware watchpoint ");
	ui_out_chain = make_cleanup_ui_out_tuple_begin_end (uiout, "wpt");
	ui_out_field_int (uiout, "number", b->number);
	ui_out_text (uiout, ": ");
	/* APPLE LOCAL gdb_print_expression */
	gdb_print_expression (b->exp, stb->stream);
	ui_out_field_stream (uiout, "exp", stb);
	do_cleanups (ui_out_chain);
	break;
      case bp_read_watchpoint:
	ui_out_text (uiout, "Hardware read watchpoint ");
	ui_out_chain = make_cleanup_ui_out_tuple_begin_end (uiout, "hw-rwpt");
	ui_out_field_int (uiout, "number", b->number);
	ui_out_text (uiout, ": ");
	/* APPLE LOCAL gdb_print_expression */
	gdb_print_expression (b->exp, stb->stream);
	ui_out_field_stream (uiout, "exp", stb);
	do_cleanups (ui_out_chain);
	break;
      case bp_access_watchpoint:
	ui_out_text (uiout, "Hardware access (read/write) watchpoint ");
	ui_out_chain = make_cleanup_ui_out_tuple_begin_end (uiout, "hw-awpt");
	ui_out_field_int (uiout, "number", b->number);
	ui_out_text (uiout, ": ");
	/* APPLE LOCAL gdb_print_expression */
	gdb_print_expression (b->exp, stb->stream);
	ui_out_field_stream (uiout, "exp", stb);
	do_cleanups (ui_out_chain);
	break;
      case bp_breakpoint:
	if (ui_out_is_mi_like_p (uiout))
	  {
	    say_where = 0;
	    break;
	  }
	printf_filtered (_("Breakpoint %d"), b->number);
	say_where = 1;
	break;
      /* APPLE LOCAL begin subroutine inlining  */
      case bp_inlined_breakpoint:
        if (ui_out_is_mi_like_p (uiout))
          {
            say_where = 0;
            break;
          }
        printf_filtered (_("Breakpoint %d (inlined %s)"), b->number,
                         b->addr_string);
        say_where = 1;
        break;
      /* APPLE LOCAL end subroutine inlining  */
      case bp_hardware_breakpoint:
	if (ui_out_is_mi_like_p (uiout))
	  {
	    say_where = 0;
	    break;
	  }
	printf_filtered (_("Hardware assisted breakpoint %d"), b->number);
	say_where = 1;
	break;
      case bp_catch_load:
      case bp_catch_unload:
	printf_filtered (_("Catchpoint %d (%s %s)"),
			 b->number,
			 (b->type == bp_catch_load) ? "load" : "unload",
			 (b->dll_pathname != NULL) ? 
			 b->dll_pathname : "<any library>");
	break;
      case bp_catch_fork:
      case bp_catch_vfork:
	printf_filtered (_("Catchpoint %d (%s)"),
			 b->number,
			 (b->type == bp_catch_fork) ? "fork" : "vfork");
	break;
      case bp_catch_exec:
	printf_filtered (_("Catchpoint %d (exec)"),
			 b->number);
	break;
      case bp_catch_catch:
      case bp_catch_throw:
      /* APPLE LOCAL begin gnu_v3 */
      case bp_gnu_v3_catch_catch:
      case bp_gnu_v3_catch_throw:
      /* APPLE LOCAL end gnu_v3 */
        ui_out_text (uiout, "Catchpoint ");
        ui_out_chain = make_cleanup_ui_out_tuple_begin_end (uiout, 
                                                            "catchpoint");
        ui_out_field_int (uiout, "catchno", b->number);
        ui_out_text (uiout, " (");
        ui_out_field_string (uiout, "type", 
                             (b->type == bp_catch_catch) ? "catch" : "throw");
        do_cleanups (ui_out_chain);
        ui_out_text (uiout, ")");
	break;

      case bp_until:
      case bp_finish:
      case bp_longjmp:
      case bp_longjmp_resume:
      case bp_step_resume:
      case bp_through_sigtramp:
      case bp_call_dummy:
      case bp_watchpoint_scope:
      case bp_shlib_event:
      case bp_thread_event:
      case bp_overlay_event:
	break;
	/* APPLE LOCAL begin default case */
      default:
	internal_error (__FILE__, __LINE__, "unhandled switch case");
	break;
	/* APPLE LOCAL end default case */
      }

  if (say_where)
    {
      /* i18n: cagney/2005-02-11: Below needs to be merged into a
	 single string.  */
      if (b->pending)
	{
	  printf_filtered (_(" (%s) pending."), b->addr_string);
	}
      else
	{
	  if (addressprint || b->source_file == NULL)
	    {
	      printf_filtered (" at ");
	      deprecated_print_address_numeric (b->loc->address, 1, gdb_stdout);
	    }
	  if (b->source_file)
	    printf_filtered (": file %s, line %d.",
			     b->source_file, b->line_number);
	}
    }
  do_cleanups (old_chain);
  if (ui_out_is_mi_like_p (uiout))
    return;
  printf_filtered ("\n");
}


/* Add SALS.nelts breakpoints to the breakpoint table.  For each
   SALS.sal[i] breakpoint, include the corresponding ADDR_STRING[i],
   COND[i] and COND_STRING[i] values.

   The parameter PENDING_BP points to a pending breakpoint that is
   the basis of the breakpoints currently being created.  The pending
   breakpoint may contain a separate condition string or commands
   that were added after the initial pending breakpoint was created.

   NOTE: If the function succeeds, the caller is expected to cleanup
   the arrays ADDR_STRING, COND_STRING, COND and SALS (but not the
   array contents).  If the function fails (error() is called), the
   caller is expected to cleanups both the ADDR_STRING, COND_STRING,
   COND and SALS arrays and each of those arrays contents. */

/* APPLE LOCAL: Pass the requested_shlib into create_breakpoints.
   Also take an optional breakpoint_list, and record the new
   breakpoints in it.  This allows the callers to operate on the
   breakpoints you just made.  */

static void
create_breakpoints (struct symtabs_and_lines sals, char **addr_string,
		    struct expression **cond, char **cond_string,
		    /* APPLE LOCAL requested shlib */
		    char *requested_shlib,
		    enum bptype type, enum bpdisp disposition,
		    int thread, int ignore_count, int from_tty,
		    /* APPLE LOCAL breakpoint lists */
		    struct breakpoint *pending_bp,
		    struct breakpoint_list **new_breakpoints)
{
  if (type == bp_hardware_breakpoint)
    {
      int i = hw_breakpoint_used_count ();
      int target_resources_ok = 
	TARGET_CAN_USE_HARDWARE_WATCHPOINT (bp_hardware_breakpoint, 
					    i + sals.nelts, 0);
      if (target_resources_ok == 0)
	error (_("No hardware breakpoint support in the target."));
      else if (target_resources_ok < 0)
	error (_("Hardware breakpoints used exceeds limit."));
    }

  /* Now set all the breakpoints.  */
  {
    int i;
    for (i = 0; i < sals.nelts; i++)
      {
	struct breakpoint *b;
	struct symtab_and_line sal = sals.sals[i];

	if (from_tty)
	  describe_other_breakpoints (sal.pc, sal.section);
	
	b = set_raw_breakpoint (sal, type, 0);
	/* APPLE LOCAL breakpoints */
	/* If we only made one breakpoint from a pending breakpoint,
	   don't change its number.  That's just annoying.  */
	if (sals.nelts == 1 && pending_bp != NULL)
	  {
	    struct breakpoint *bpt;

	    /* Move the new breakpoint back into the position the pending
	       breakpoint originally was so the list stays ordered.  */

	    /* First remove the new breakpoint (set_raw_breakpoint sticks it
	       at the end).  */
	    ALL_BREAKPOINTS (bpt)
	      {
		if (bpt->next == b)
		  {
		    bpt->next = b->next;
		    break;
		  }
	      }

	    /* Now replace pending_bp with it.  */

	    if (pending_bp == breakpoint_chain)
	      {
		b->next = pending_bp->next;
		breakpoint_chain = b;
	      }
	    else
	      {
		ALL_BREAKPOINTS (bpt)
		  {
		    if (bpt->next == pending_bp) 
		      {
			bpt->next = b;
			b->next = pending_bp->next;
			break;
		      }
		  }
	      }
	    b->number = pending_bp->number;
	  }
	else
	  {
	    /* APPLE LOCAL end breakpoints */
	set_breakpoint_count (breakpoint_count + 1);
	b->number = breakpoint_count;
	/* APPLE LOCAL breakpoints */
	  }
	b->cond = cond[i];
	b->thread = thread;
	if (addr_string[i])
	  b->addr_string = addr_string[i];
	else
	  /* addr_string has to be used or breakpoint_re_set will delete
	     me.  */
	  b->addr_string = xstrprintf ("*0x%s", paddr (b->loc->address));
	b->cond_string = cond_string[i];
	b->ignore_count = ignore_count;
	b->enable_state = bp_enabled;
	b->disposition = disposition;
	/* If resolving a pending breakpoint, a check must be made to see if
	   the user has specified a new condition or commands for the 
	   breakpoint.  A new condition will override any condition that was 
	   initially specified with the initial breakpoint command.  */
	if (pending_bp)
	  {
	    char *arg;
	    if (pending_bp->cond_string)
	      {
		/* APPLE LOCAL: I am turning the error's from parsing the expression,
		   and the "junk at end of expression" case into warnings.  That
		   will mean that we will allow the breakpoint and just ignore
		   the condition.  If we error when resolving a pending breakpoint
		   we aren't in a good state to figure out what happened above us,
		   and for instance sometimes we think the breakpoint resolution
		   failed, and shlib_disable the breakpoint, which is undesirable.  */
		int parse_succeeded;
		arg = pending_bp->cond_string;
		b->cond_string = savestring (arg, strlen (arg));
		parse_succeeded = gdb_parse_exp_1 (&arg, block_for_pc (b->loc->address), 0, &b->cond);
		if (!parse_succeeded || *arg)
		  {
		    /* If the error handlers did their job they should
		       have freed the cond before throwing an error, but
		       just in case somebody messed up below us, we will 
		       clean up here.  */
		    if (b->cond != NULL)
		      {
			xfree (b->cond);
			b->cond = NULL;
		      }
		    if (!parse_succeeded)
		      warning ("Error parsing breakpoint condition expression");
		    else if (*arg)
		      warning ("Junk at end of pending breakpoint condition expression");
		  }
		/* END APPLE LOCAL */
	      }
	    /* If there are commands associated with the breakpoint, they should 
	       be copied too.  */
	    if (pending_bp->commands)
	      b->commands = copy_command_lines (pending_bp->commands);
	    
	    /* We have to copy over the ignore_count and thread as well.  */
	    b->ignore_count = pending_bp->ignore_count;
	    b->thread = pending_bp->thread;
	  }

	/* APPLE LOCAL: Tell listeners that a pending breakpoint has been resolved.  */
	if (pending_bp != NULL)
	  breakpoint_resolve_event (pending_bp->number, b->number);

	/* APPLE LOCAL: Set the requested_shlib if it has been passed
	   into this function.  */
	if (requested_shlib)
	  b->requested_shlib = xstrdup (requested_shlib);

	/* APPLE LOCAL: Set the bp_objfile to the objfile that we found
	   this breakpoint in.  This way we will only shlib_disable this
	   breakpoint if the objfile it is in was changed (useful in the
	   case that more than one objfile shares the same memory address.)  */
	set_bp_objfile (b, &sal);

	/* APPLE LOCAL: Mark the breakpoint as set so we don't try to reset
	   it unless the objfile it was set in gets reread.  */
	b->bp_set_state = bp_state_set;
	/* END APPLE LOCAL */

	/* APPLE LOCAL: Add the breakpoint to the new breakpoints list.  */
	if (new_breakpoints != NULL)
	  add_breakpoint_to_new_list (new_breakpoints, b);
	mention (b);
      }

    /* APPLE LOCAL begin subroutine inlining  */
    if (type != bp_inlined_breakpoint)
      {
        struct symtabs_and_lines new_sals;
        char **new_addr_string;
        struct expression **new_cond;
        char **new_cond_string;
	
        new_sals = check_for_additional_inlined_breakpoint_locations (sals,
								addr_string,
								cond,
								cond_string,
								&new_addr_string,
								&new_cond,
								&new_cond_string);

      if (new_sals.nelts > 0)
        create_breakpoints (new_sals, new_addr_string, new_cond, new_cond_string,
                            requested_shlib, bp_inlined_breakpoint, disposition,
                            thread, ignore_count, from_tty, pending_bp,
                            new_breakpoints);
      }
    /* APPLE LOCAL end subroutine inlining */
  }    
}

/* Parse ARG which is assumed to be a SAL specification possibly
   followed by conditionals.  On return, SALS contains an array of SAL
   addresses found. ADDR_STRING contains a vector of (canonical)
   address strings. ARG points to the end of the SAL. */

static void
parse_breakpoint_sals (char **address,
		       struct symtabs_and_lines *sals,
		       char ***addr_string,
		       /* APPLE LOCAL requested shlib */
		       char *requested_shlib,
		       int *not_found_ptr)
{
  char *addr_start = *address;
  /* APPLE LOCAL begin requested shlib */
  struct cleanup *restrict_cleanup;

  *addr_string = NULL;
  /* If no arg given, or if first arg is 'if ', use the default
     breakpoint. */
  if ((*address) == NULL
      || (strncmp ((*address), "if", 2) == 0 && isspace ((*address)[2])))
    {
      if (default_breakpoint_valid)
	{
	  struct symtab_and_line sal;
	  init_sal (&sal);		/* initialize to zeroes */
	  sals->sals = (struct symtab_and_line *)
	    xmalloc (sizeof (struct symtab_and_line));
	  sal.pc = default_breakpoint_address;
	  sal.line = default_breakpoint_line;
	  sal.symtab = default_breakpoint_symtab;
	  sal.section = find_pc_overlay (sal.pc);
	  sals->sals[0] = sal;
	  sals->nelts = 1;
	  /* APPLE LOCAL: Supply a "*ADDR" for default case (ADDR is pc value).  */
	  {
	    char *s;
	    s = paddr_u (sal.pc);
	    *addr_string = (char **) xmalloc (sizeof (char **));
	    **addr_string = (char *) xmalloc (strlen (s) + 2);
	    sprintf (**addr_string, "*%s", s);
	  }
	}
      else
	error (_("No default breakpoint address now."));
    }
  else
    {
      /* Force almost all breakpoints to be in terms of the
         current_source_symtab (which is decode_line_1's default).  This
         should produce the results we want almost all of the time while
         leaving default_breakpoint_* alone.  
         ObjC: However, don't match an Objective-C method name which
         may have a '+' or '-' succeeded by a '[' */
	 
      struct symtab_and_line cursal = get_current_source_symtab_and_line ();

      /* APPLE LOCAL begin requested shlib */
      if (requested_shlib != NULL)
	{
	  struct objfile *obj, *objnext;
	  restrict_cleanup = make_cleanup_restrict_to_shlib (requested_shlib);
	  if (restrict_cleanup == (void *) -1)
	    {
	      *not_found_ptr = 1;
              /* If this is a pending breakpoint, just bail on trying to set it;
                 don't issue an error message about not finding the objfile. */
              if (pending_break_support == AUTO_BOOLEAN_TRUE)
		throw_error (NOT_FOUND_ERROR, "");
              else
		throw_error (GENERIC_ERROR, 
                    _("Couldn't locate shared library \"%s\" for breakpoint."), 
			     requested_shlib);
	    }

          /* Okay, we've restricted the search to the requested shlib.  
	     Since we are trying to set breakpoints here, let's
	     raise the load level of all the objfiles to OBJF_SYM_ALL.
	     For now there is only one objfile, but I use the
	     ALL_OBJFILES iterator in case we pass in a set some day.  */

	  ALL_OBJFILES_SAFE (obj, objnext)
	    objfile_set_load_state (obj, OBJF_SYM_ALL, 0);
	}
      else
	restrict_cleanup = make_cleanup (null_cleanup, NULL);
      /* APPLE LOCAL end requested shlib */

      if (default_breakpoint_valid
	  && (!cursal.symtab
 	      || ((strchr ("+-", (*address)[0]) != NULL)
 		  && ((*address)[1] != '['))))
	/* APPLE LOCAL begin return multiple symbols */
	*sals = decode_line_1 (address, 1, default_breakpoint_symtab,
			       default_breakpoint_line, addr_string, 
			       not_found_ptr, 0);
      /* APPLE LOCAL end return multiple symbols */
      else
	/* APPLE LOCAL begin return multiple symbols */
	*sals = decode_line_1 (address, 1, (struct symtab *) NULL, 0,
		               addr_string, not_found_ptr, 1);
      /* APPLE LOCAL end return multiple symbols */
      /* APPLE LOCAL requested shlib */
      do_cleanups (restrict_cleanup);
    }
  /* For any SAL that didn't have a canonical string, fill one in. */
  if (sals->nelts > 0 && *addr_string == NULL)
    *addr_string = xcalloc (sals->nelts, sizeof (char **));
  if (addr_start != (*address))
    {
      int i;
      for (i = 0; i < sals->nelts; i++)
	{
	  /* Add the string if not present. */
	  if ((*addr_string)[i] == NULL)
	    (*addr_string)[i] = savestring (addr_start, (*address) - addr_start);
	}
    }
}


/* Convert each SAL into a real PC.  Verify that the PC can be
   inserted as a breakpoint.  If it can't throw an error. */

static void
breakpoint_sals_to_pc (struct symtabs_and_lines *sals,
		       char *address)
{    
  int i;
  for (i = 0; i < sals->nelts; i++)
    {
      resolve_sal_pc (&sals->sals[i]);

      /* It's possible for the PC to be nonzero, but still an illegal
         value on some targets.

         For example, on HP-UX if you start gdb, and before running the
         inferior you try to set a breakpoint on a shared library function
         "foo" where the inferior doesn't call "foo" directly but does
         pass its address to another function call, then we do find a
         minimal symbol for the "foo", but it's address is invalid.
         (Appears to be an index into a table that the loader sets up
         when the inferior is run.)

         Give the target a chance to bless sals.sals[i].pc before we
         try to make a breakpoint for it. */
#ifdef DEPRECATED_PC_REQUIRES_RUN_BEFORE_USE
      if (DEPRECATED_PC_REQUIRES_RUN_BEFORE_USE (sals->sals[i].pc))
	{
	  if (address == NULL)
	    error (_("Cannot break without a running program."));
	  else
	    error (_("Cannot break on %s without a running program."), 
		   address);
	}
#endif
    }
}

/* APPLE LOCAL: introducing a safe_breakpoint_sals_to_pc */

struct safe_sals_to_pc_args
{
  struct symtabs_and_lines *sals;
  char *address;
};

static int 
wrap_breakpoint_sals_to_pc (char *a)
{
  struct safe_sals_to_pc_args *args = (struct safe_sals_to_pc_args *) a;

  breakpoint_sals_to_pc (args->sals, args->address);

  return 1;
  
}

static int
safe_breakpoint_sals_to_pc (struct symtabs_and_lines *sals, char *address)
{
  struct safe_sals_to_pc_args args;

  args.sals = sals;
  args.address = address;

  if (!catch_errors ((catch_errors_ftype *) wrap_breakpoint_sals_to_pc, &args,
		     "", RETURN_MASK_ALL))
    {
      /* An error occurred */
      return 0;
    }

  return 1;
}

/* END APPLE LOCAL */

static void
do_captured_parse_breakpoint (struct ui_out *ui, void *data)
{
  struct captured_parse_breakpoint_args *args = data;
  
  parse_breakpoint_sals (args->arg_p, args->sals_p, args->addr_string_p, 
		         args->requested_shlib, args->not_found_ptr);
}

/* Set a breakpoint according to ARG (function, linenum or *address)
   flag: first bit  : 0 non-temporary, 1 temporary.
   second bit : 0 normal breakpoint, 1 hardware breakpoint. 

   PENDING_BP is non-NULL when this function is being called to resolve
   a pending breakpoint.  */

static int
break_command_1 (char *arg, int flag, int from_tty, struct breakpoint *pending_bp)
{
  char *requested_shlib;

  /* APPLE LOCAL: Kind of hacky, but look for requested_shlib and strip it off if
     found.  */

  if (!arg)
    requested_shlib = NULL;
  else
    {
      while (isspace (*arg))
	arg++;
      if (*arg == '-' && *(arg+1) == 's' && strstr(arg, "-shlib") == arg)
	{
	  char *begin;
	  int len;
	  
	  arg += 6;
	  while (isspace (*arg))
	    arg++;
	  if (*arg == '\0')
	    error ("No value for \"-shlib\"");
	  if (*arg == '"')
	    {
	      int bs = 0;
	      arg++;
	      begin = arg;
	      while (*arg != '"' || bs)
		{
		  if (*arg == '\0')
		    error ("No close quote in \"-shlib\" argument.");
		  if (*arg == '\\')
		    {
		      if (bs)
			bs = 0;
		      else
			bs = 1;
		    }
		  else
		    bs = 0;
		  
		  arg++;
		}
	      
	      len = arg - begin;
	      /* Step past the quote.  */
	      arg++;
	    }
	  else
	    {
	      begin = arg;
	      while (!isspace (*arg) && *arg != '\0')
		arg++;
	      len = arg - begin;
	    }

	  requested_shlib = malloc (len + 1);
	  strncpy (requested_shlib, begin, len);
	  requested_shlib[len] = '\0';
	  /* Now step past any other spaces, the code below requires that.  */
	  while (isspace (*arg))
	    arg++;
	}
      else
	requested_shlib = NULL;
    }
  return break_command_2 (arg, flag, from_tty, pending_bp, requested_shlib, NULL, NULL);

}

/* Helper function for break_command_1 and gdb_breakpoint.  This
   arguments have the same meaning as break_command_1.  In addition,
   it takes two optional arguments.  

   If INDICES is non-null, then the elements of the array will be used
   to pick the breakpoint hits if there are multiple hits.  The list
   indices is terminated by an element with value -1, but if the first
   element of the list is -1, then ALL matches will be picked.

   If NEW_BREAKPOINTS is non-null, then it will be filled with a 
   list of the newly created breakpoints.  */

static int
break_command_2 (char *arg, int flag, int from_tty, 
		 struct breakpoint *pending_bp, 
		 char *requested_shlib, int *indices, 
		 struct breakpoint_list **new_breakpoints)
{
  struct gdb_exception e;
  int tempflag, hardwareflag;
  struct symtabs_and_lines sals;
  struct expression **cond = 0;
  struct symtab_and_line pending_sal;
  char **cond_string = (char **) NULL;
  char *copy_arg;
  char *addr_start = arg;
  char **addr_string;
  struct cleanup *old_chain;
  struct cleanup *breakpoint_chain = NULL;
  struct captured_parse_breakpoint_args parse_args;
  int i;
  int pending = 0;
  int thread = -1;
  int ignore_count = 0;
  int not_found = 0;

  hardwareflag = flag & BP_HARDWAREFLAG;
  tempflag = flag & BP_TEMPFLAG;

  sals.sals = NULL;
  sals.nelts = 0;
  addr_string = NULL;

  parse_args.arg_p = &arg;
  parse_args.sals_p = &sals;
  parse_args.addr_string_p = &addr_string;
  parse_args.requested_shlib = requested_shlib;
  parse_args.not_found_ptr = &not_found;

  e = catch_exception (uiout, do_captured_parse_breakpoint, 
		       &parse_args, RETURN_MASK_ALL);

  /* APPLE_LOCAL: if the interpreter is the mi, and we have gotten more
     than one breakpoint hit, then don't set the breakpoint, but just
     output the hits, and return.  The MI client should then query the
     user for their choice, and reset the breakpoints based on the canonical
     form returned. */
  
  if (e.reason == 0 && sals.nelts > 1)
    {
      /* If we got an indices list, it's because we reported the
	 list to the UI, which made a choice, and came back to us
	 with the pick of choices.  We can't really do much to make 
	 sure the index pick matches what we sent before, we just
	 have to assume the UI did the right thing.  */
      if (indices != NULL)
	{
	  if (indices[0] != -1)
	    {
	      int nsals;
	      int i;
	      struct symtab_and_line *new_sals;
	      for (nsals = 0; indices[nsals] != -1; nsals++) { ; }
	      
	      new_sals = (struct symtab_and_line *) 
		xmalloc (nsals * sizeof (struct symtab_and_line));
	      
	      for (i = 0; i < nsals; i++)
		{
		  if (0 <= indices[i] && indices[i] < sals.nelts)
		    new_sals[i] = sals.sals[indices[i]];
		  else
		    {
		      xfree (new_sals);
		      error ("Index %d out of range: %d", i, indices[i]);
		    }
		}
	      
	      xfree (sals.sals);
	      sals.nelts = nsals;
	      sals.sals = new_sals;
	    }
	}
      else if (ui_out_is_mi_like_p (uiout))
	{
	  struct cleanup *old_chain, *list_cleanup;
	  int retval;

	  /* Always have a addr_string array, even if it is empty. */
	  old_chain = make_cleanup (xfree, addr_string);
	  
	  /* Make sure that all storage allocated to SALS gets freed.  */
	  make_cleanup (xfree, sals.sals);
	  
	  /* APPLE LOCAL: Have to use safe_breakpoint_sals_to_pc here,
	     because this could get called to resolve a pending
	     breakpoint and we don't want a failure in one pending
	     breakpoint to scotch setting all the others, or worse
	     to cause us not to finish handling a shared library event. */
	  retval = safe_breakpoint_sals_to_pc (&sals, addr_start);
	  if (!retval)
	    {
	      do_cleanups (old_chain);
	      return GDB_RC_FAIL;
	    }
	  /* END APPLE LOCAL */
	  make_cleanup_ui_out_list_begin_end (uiout, "matches");
	  
	  for (i = 0; i < sals.nelts; i++)
	    {
	      struct symtab_and_line sal = sals.sals[i];
	      list_cleanup = make_cleanup_ui_out_list_begin_end (uiout, "b");
	      
	      /* Output an index so that the MI client can come back and
		 make choices without having to ask by name (which might
		 end up being circular.  Increment by two so we can pass
		 the results back to select_symbol_args...  */
	      
	      ui_out_field_int (uiout, "index", i);
	      
	      if (addr_string[i] != NULL)
		ui_out_field_string (uiout, "canonical", addr_string[i]);
	      else
		ui_out_field_skip (uiout, "canonical");
	      
	      if (sal.symtab && sal.symtab->filename)
		ui_out_field_string (uiout, "file", sal.symtab->filename);
	      
	      if (sal.symtab && sal.symtab->objfile && sal.symtab->objfile->name)
		ui_out_field_string (uiout, "binary", sal.symtab->objfile->name);
	      else if (sal.section && sal.section->owner && sal.section->owner->filename)
		ui_out_field_string (uiout, "binary", sal.section->owner->filename);
	      
	      ui_out_field_int (uiout, "line", sal.line);
	      ui_out_field_core_addr (uiout, "addr", sal.pc);
	      
	      
	      do_cleanups (list_cleanup);
	    }
	  do_cleanups (old_chain);
	  return GDB_RC_NONE;
	}
    }

  /* If caller is interested in rc value from parse, set value.  */
  switch (e.reason)
    {
    case RETURN_QUIT:
      exception_print (gdb_stderr, e);
      return e.reason;
    case RETURN_ERROR:
      switch (e.error)
	{
	  /* APPLE LOCAL: In this code, if not_found_ptr got set, that
	     should override the error type returned.  Really, all of 
	     the routines we call into should return the right kind of
	     error, but I put this check in here just in case somebody
	     makes a mistake.  */
	default:
	  if (!not_found)
	    {
	      exception_print (gdb_stderr, e);
	      return e.reason;
	    }
	case NOT_FOUND_ERROR:
	  /* If called to resolve pending breakpoint, just return
	     error code.  */
	  if (pending_bp)
	    return e.reason;

	  /* APPLE LOCAL begin */
	  /* If this is a pending breakpoint, and we were given a 
             shlib to set it in, and we can't set the breakpoint right now, 
             don't issue any error message - we'll figure it out eventually.  */
          if (pending_break_support != AUTO_BOOLEAN_TRUE 
              || requested_shlib == NULL)
	  /* APPLE LOCAL end */
	  exception_print (gdb_stderr, e);

	  /* If pending breakpoint support is turned off, throw
	     error.  */

	  if (pending_break_support == AUTO_BOOLEAN_FALSE)
	    throw_exception (e);
	  
          /* If pending breakpoint support is auto query and the user
	     selects no, then simply return the error code.  */
	  if (pending_break_support == AUTO_BOOLEAN_AUTO && 
	      !nquery ("Make breakpoint pending on future shared library load? "))
	    return e.reason;

	  /* At this point, either the user was queried about setting
	     a pending breakpoint and selected yes, or pending
	     breakpoint behavior is on and thus a pending breakpoint
	     is defaulted on behalf of the user.  */
	  copy_arg = xstrdup (addr_start);
	  addr_string = &copy_arg;
	  sals.nelts = 1;
	  sals.sals = &pending_sal;
	  pending_sal.pc = 0;
	  pending = 1;
	  break;
	}
    default:
      if (!sals.nelts)
	return GDB_RC_FAIL;
    }

  /* Create a chain of things that always need to be cleaned up. */
  old_chain = make_cleanup (null_cleanup, 0);

  if (!pending)
    {
      /* Make sure that all storage allocated to SALS gets freed.  */
      make_cleanup (xfree, sals.sals);
      
      /* Cleanup the addr_string array but not its contents. */
      make_cleanup (xfree, addr_string);
    }

  /* Allocate space for all the cond expressions. */
  cond = xcalloc (sals.nelts, sizeof (struct expression *));
  make_cleanup (xfree, cond);

  /* Allocate space for all the cond strings. */
  cond_string = xcalloc (sals.nelts, sizeof (char **));
  make_cleanup (xfree, cond_string);

  /* ----------------------------- SNIP -----------------------------
     Anything added to the cleanup chain beyond this point is assumed
     to be part of a breakpoint.  If the breakpoint create succeeds
     then the memory is not reclaimed. */
  breakpoint_chain = make_cleanup (null_cleanup, 0);

  /* Mark the contents of the addr_string for cleanup.  These go on
     the breakpoint_chain and only occure if the breakpoint create
     fails. */
  for (i = 0; i < sals.nelts; i++)
    {
      if (addr_string[i] != NULL)
	make_cleanup (xfree, addr_string[i]);
    }

  /* Resolve all line numbers to PC's and verify that the addresses
     are ok for the target.  */
  if (!pending)
    {
      int retval;
      /* APPLE LOCAL: See comment in previous use of
	 safe_breakpoint_sals_to_pc above. */
      retval = safe_breakpoint_sals_to_pc (&sals, addr_start);
      if (!retval)
	{
	  do_cleanups (old_chain);
	  return GDB_RC_FAIL;
	}
      /* END APPLE LOCAL */
    }

  /* Verify that condition can be parsed, before setting any
     breakpoints.  Allocate a separate condition expression for each
     breakpoint. */
  thread = -1;			/* No specific thread yet */
  if (!pending)
    {
      for (i = 0; i < sals.nelts; i++)
	{
	  char *tok = arg;
	  while (tok && *tok)
	    {
	      char *end_tok;
	      int toklen;
	      char *cond_start = NULL;
	      char *cond_end = NULL;
	      while (*tok == ' ' || *tok == '\t')
		tok++;
	      
	      end_tok = tok;
	      
	      while (*end_tok != ' ' && *end_tok != '\t' && *end_tok != '\000')
		end_tok++;
	      
	      toklen = end_tok - tok;
	      
	      if (toklen >= 1 && strncmp (tok, "if", toklen) == 0)
		{
		  tok = cond_start = end_tok + 1;
		  cond[i] = parse_exp_1 (&tok, block_for_pc (sals.sals[i].pc), 
				         0);
		  make_cleanup (xfree, cond[i]);
		  cond_end = tok;
		  cond_string[i] = savestring (cond_start, 
				               cond_end - cond_start);
		  make_cleanup (xfree, cond_string[i]);
		}
	      else if (toklen >= 1 && strncmp (tok, "thread", toklen) == 0)
		{
		  char *tmptok;
		  
		  tok = end_tok + 1;
		  tmptok = tok;
		  thread = strtol (tok, &tok, 0);
		  if (tok == tmptok)
		    error (_("Junk after thread keyword."));
		  if (!valid_thread_id (thread))
		    error (_("Unknown thread %d."), thread);
		}
	      else
		error (_("Junk at end of arguments."));
	    }
	}
      create_breakpoints (sals, addr_string, cond, cond_string,
			  /* APPLE LOCAL requested shlib */
			  requested_shlib,
			  hardwareflag ? bp_hardware_breakpoint 
			  : bp_breakpoint,
			  tempflag ? disp_del : disp_donttouch,
			  thread, ignore_count, from_tty,
			  /* APPLE breakpoint lists */
			  pending_bp, new_breakpoints);
    }
  else
    {
      struct symtab_and_line sal;
      struct breakpoint *b;

      sal.symtab = NULL;
      sal.pc = 0;

      make_cleanup (xfree, copy_arg);

      b = set_raw_breakpoint (sal, hardwareflag ? bp_hardware_breakpoint 
		              : bp_breakpoint, 1);
      set_breakpoint_count (breakpoint_count + 1);
      b->number = breakpoint_count;
      b->cond = *cond;
      b->thread = thread;
      b->addr_string = *addr_string;
      b->cond_string = *cond_string;
      b->ignore_count = ignore_count;
      b->pending = 1;
      b->disposition = tempflag ? disp_del : disp_donttouch;
      b->from_tty = from_tty;
      b->flag = flag;
      /* APPLE LOCAL: Record requested_shlib */
      if (requested_shlib != NULL)
	b->requested_shlib = xstrdup (requested_shlib);
      mention (b);
    }
  
  /* APPLE LOCAL: don't output this warning if the mi is setting
     the breakpoints.  That will just confuse the user.  */
  if (sals.nelts > 1 && !ui_out_is_mi_like_p (uiout))
    warning (_("Multiple breakpoints were set.\n"
	       "Use the \"delete\" command to delete unwanted breakpoints."));
  /* That's it. Discard the cleanups for data inserted into the
     breakpoint. */
  discard_cleanups (breakpoint_chain);
  /* But cleanup everything else. */
  do_cleanups (old_chain);

  return GDB_RC_OK;
}

/* Set a breakpoint of TYPE/DISPOSITION according to ARG (function,
   linenum or *address) with COND and IGNORE_COUNT. */

struct captured_breakpoint_args
  {
    char *address;
    char *condition;
    int hardwareflag;
    int tempflag;
    int thread;
    int ignore_count;
  };

static int
do_captured_breakpoint (struct ui_out *uiout, void *data)
{
  struct captured_breakpoint_args *args = data;
  struct symtabs_and_lines sals;
  struct expression **cond;
  struct cleanup *old_chain;
  struct cleanup *breakpoint_chain = NULL;
  int i;
  char **addr_string;
  char **cond_string;

  char *address_end;

  /* Parse the source and lines spec.  Delay check that the expression
     didn't contain trailing garbage until after cleanups are in
     place. */
  sals.sals = NULL;
  sals.nelts = 0;
  address_end = args->address;
  addr_string = NULL;
  /* APPLE LOCAL requested shlib */
  parse_breakpoint_sals (&address_end, &sals, &addr_string, NULL, 0);

  if (!sals.nelts)
    return GDB_RC_NONE;

  /* Create a chain of things at always need to be cleaned up. */
  old_chain = make_cleanup (null_cleanup, 0);

  /* Always have a addr_string array, even if it is empty. */
  make_cleanup (xfree, addr_string);

  /* Make sure that all storage allocated to SALS gets freed.  */
  make_cleanup (xfree, sals.sals);

  /* Allocate space for all the cond expressions. */
  cond = xcalloc (sals.nelts, sizeof (struct expression *));
  make_cleanup (xfree, cond);

  /* Allocate space for all the cond strings. */
  cond_string = xcalloc (sals.nelts, sizeof (char **));
  make_cleanup (xfree, cond_string);

  /* ----------------------------- SNIP -----------------------------
     Anything added to the cleanup chain beyond this point is assumed
     to be part of a breakpoint.  If the breakpoint create goes
     through then that memory is not cleaned up. */
  breakpoint_chain = make_cleanup (null_cleanup, 0);

  /* Mark the contents of the addr_string for cleanup.  These go on
     the breakpoint_chain and only occure if the breakpoint create
     fails. */
  for (i = 0; i < sals.nelts; i++)
    {
      if (addr_string[i] != NULL)
	make_cleanup (xfree, addr_string[i]);
    }

  /* Wait until now before checking for garbage at the end of the
     address. That way cleanups can take care of freeing any
     memory. */
  if (*address_end != '\0')
    error (_("Garbage %s following breakpoint address"), address_end);

  /* Resolve all line numbers to PC's.  */
  breakpoint_sals_to_pc (&sals, args->address);

  /* Verify that conditions can be parsed, before setting any
     breakpoints.  */
  for (i = 0; i < sals.nelts; i++)
    {
      if (args->condition != NULL)
	{
	  char *tok = args->condition;
	  cond[i] = parse_exp_1 (&tok, block_for_pc (sals.sals[i].pc), 0);
	  if (*tok != '\0')
	    error (_("Garbage %s follows condition"), tok);
	  make_cleanup (xfree, cond[i]);
	  cond_string[i] = xstrdup (args->condition);
	}
    }

  create_breakpoints (sals, addr_string, cond, cond_string,
		      /* APPLE LOCAL requested shlib */
		      NULL,
		      args->hardwareflag ? bp_hardware_breakpoint : bp_breakpoint,
		      args->tempflag ? disp_del : disp_donttouch,
		      args->thread, args->ignore_count, 0/*from-tty*/, 
		      /* APPLE LOCAL breakpoint lists */
		      NULL/*pending_bp*/, NULL/*new_breakpoints*/);

  /* That's it. Discard the cleanups for data inserted into the
     breakpoint. */
  discard_cleanups (breakpoint_chain);
  /* But cleanup everything else. */
  do_cleanups (old_chain);
  return GDB_RC_OK;
}

enum gdb_rc
fsf_gdb_breakpoint (char *address, char *condition,
		int hardwareflag, int tempflag,
		int thread, int ignore_count,
		char **error_message)
{
  struct captured_breakpoint_args args;
  args.address = address;
  args.condition = condition;
  args.hardwareflag = hardwareflag;
  args.tempflag = tempflag;
  args.thread = thread;
  args.ignore_count = ignore_count;
  return catch_exceptions_with_msg (uiout, do_captured_breakpoint, &args,
				    error_message, RETURN_MASK_ALL);
}

/* APPLE LOCAL breakpoints */
enum gdb_rc
gdb_breakpoint (char *address, char *condition,
		int hardwareflag, int tempflag,
		/* APPLE LOCAL future breakpoints */
		int futureflag,
		int thread, int ignore_count,
		/* APPLE LOCAL breakpoints */
		int *indices, char *requested_shlib,
		char **error_message)
{
  struct cleanup *wipe;
  int retval = GDB_RC_OK;
  int flag;
  struct breakpoint_list *new_breakpoints = NULL, *new_bp;

  /* APPLE LOCAL - if a future break is requested, turn on
     automatic pending support.  Otherwise, turn it off.
     We don't want to be asking questions in gdb_breakpoint.  */

  wipe = make_cleanup (restore_saved_pending_break_support, 
		       (void *) pending_break_support);
  if (futureflag)
    pending_break_support = AUTO_BOOLEAN_TRUE;
  else
    pending_break_support = AUTO_BOOLEAN_FALSE;
  
  flag = 0;
  if (hardwareflag)
    flag |= BP_HARDWAREFLAG;
  if (tempflag)
    flag |= BP_TEMPFLAG;
  
  break_command_2 (address, flag, 0, NULL, requested_shlib, indices, &new_breakpoints);
  
  /* Now that we have set the breakpoints, record the condition & ignore_counts.  */
  for (new_bp = new_breakpoints; new_bp != NULL; new_bp = new_bp->next)
    {
      new_bp->bp->ignore_count = ignore_count;
      if (condition != NULL)
	condition_command_1 (new_bp->bp, condition, 0);
    }
  
  clear_new_breakpoint_list (new_breakpoints);
  
  do_cleanups (wipe);

  return retval;
}

/* Helper function for break_command_1 and disassemble_command.  */

void
resolve_sal_pc (struct symtab_and_line *sal)
{
  CORE_ADDR pc;

  if (sal->pc == 0 && sal->symtab != NULL)
    {
      /* APPLE LOCAL begin */
      CORE_ADDR end;
      if (!find_line_pc_range (*sal, &pc, &end))
	/* APPLE LOCAL end */
	error (_("No line %d in file \"%s\"."),
	       sal->line, sal->symtab->filename);
      sal->pc = pc;
    }

  if (sal->section == 0 && sal->symtab != NULL)
    {
      struct blockvector *bv;
      struct block *b;
      struct symbol *sym;
      int index;

      bv = blockvector_for_pc_sect (sal->pc, 0, &index, sal->symtab);
      if (bv != NULL)
	{
	  b = BLOCKVECTOR_BLOCK (bv, index);
	  sym = block_function (b);
	  if (sym != NULL)
	    {
	      fixup_symbol_section (sym, sal->symtab->objfile);
	      sal->section = SYMBOL_BFD_SECTION (sym);
	    }
	  else
	    {
	      /* It really is worthwhile to have the section, so we'll just
	         have to look harder. This case can be executed if we have 
	         line numbers but no functions (as can happen in assembly 
	         source).  */

	      struct minimal_symbol *msym;

	      msym = lookup_minimal_symbol_by_pc (sal->pc);
	      if (msym)
		sal->section = SYMBOL_BFD_SECTION (msym);
	    }
	}
    }
}

void
break_command (char *arg, int from_tty)
{
  break_command_1 (arg, 0, from_tty, NULL);
}

void
tbreak_command (char *arg, int from_tty)
{
  break_command_1 (arg, BP_TEMPFLAG, from_tty, NULL);
}

static void
hbreak_command (char *arg, int from_tty)
{
  break_command_1 (arg, BP_HARDWAREFLAG, from_tty, NULL);
}

static void
thbreak_command (char *arg, int from_tty)
{
  break_command_1 (arg, (BP_TEMPFLAG | BP_HARDWAREFLAG), from_tty, NULL);
}

static void
stop_command (char *arg, int from_tty)
{
  printf_filtered (_("Specify the type of breakpoint to set.\n\
Usage: stop in <function | address>\n\
       stop at <line>\n"));
}

static void
stopin_command (char *arg, int from_tty)
{
  int badInput = 0;

  if (arg == (char *) NULL)
    badInput = 1;
  else if (*arg != '*')
    {
      char *argptr = arg;
      int hasColon = 0;

      /* look for a ':'.  If this is a line number specification, then
         say it is bad, otherwise, it should be an address or
         function/method name */
      while (*argptr && !hasColon)
	{
	  hasColon = (*argptr == ':');
	  argptr++;
	}

      if (hasColon)
	badInput = (*argptr != ':');	/* Not a class::method */
      else
	badInput = isdigit (*arg);	/* a simple line number */
    }

  if (badInput)
    printf_filtered (_("Usage: stop in <function | address>\n"));
  else
    break_command_1 (arg, 0, from_tty, NULL);
}

static void
stopat_command (char *arg, int from_tty)
{
  int badInput = 0;

  if (arg == (char *) NULL || *arg == '*')	/* no line number */
    badInput = 1;
  else
    {
      char *argptr = arg;
      int hasColon = 0;

      /* look for a ':'.  If there is a '::' then get out, otherwise
         it is probably a line number. */
      while (*argptr && !hasColon)
	{
	  hasColon = (*argptr == ':');
	  argptr++;
	}

      if (hasColon)
	badInput = (*argptr == ':');	/* we have class::method */
      else
	badInput = !isdigit (*arg);	/* not a line number */
    }

  if (badInput)
    printf_filtered (_("Usage: stop at <line>\n"));
  else
    break_command_1 (arg, 0, from_tty, NULL);
}

/* accessflag:  hw_write:  watch write, 
                hw_read:   watch read, 
		hw_access: watch access (read or write) */
/* APPLE LOCAL: BY_LOCATION - if set, just watch the memory location
   pointed to by the expression in ARG, not all the locations leading
   up to it.  Also, don't tie the watch to the current scope.  */

static void
watch_command_1 (char *arg, int accessflag, int by_location, int from_tty)
{
  struct breakpoint *b;
  struct symtab_and_line sal;
  struct expression *exp;
  struct block *exp_valid_block;
  struct value *val, *mark;
  struct frame_info *frame;
  struct frame_info *prev_frame = NULL;
  char *exp_start = NULL;
  char *exp_end = NULL;
  char *tok, *end_tok;
  int toklen;
  char *cond_start = NULL;
  char *cond_end = NULL;
  char *exp_string = NULL;
  struct expression *cond = NULL;
  int i, other_type_used, target_resources_ok = 0;
  enum bptype bp_type;
  int mem_cnt = 0;

  init_sal (&sal);		/* initialize to zeroes */

  /* APPLE LOCAL: If they just wanted to watch the location, then
     evaluate the expression, and pull out the address, and make a new
     expression which is just the address cast to the correct
     type.  This is a little cheesy, because we lose the original expression.
     But that's kind of what you want to do.  For instance, watching this->member,
     you don't want to watch the local variable this, and you are going to lose
     access to it forever when you leave the function...  I've added this because
     people do it by hand all the time.  But there really isn't a way to keep more
     information than just the address...  */
  
  if (by_location)
    {
      struct value *orig_val;
      char *addr_str, *type_str, *tmp_str;
      
      exp_start = arg;
      exp = parse_exp_1 (&arg, 0, 0);
      exp_end = arg;

      orig_val = evaluate_expression (exp);
      if (value_lazy (orig_val))
	value_fetch_lazy (orig_val);
      orig_val = value_addr (orig_val);
      type_str = type_sprint (value_type (orig_val), "", -1);

      /* This is kind of grotty, but we need to change the expression 
	 over so that we don't try to reset the watchpoint using the
	 original expression, which might not work anymore.  */

      addr_str = paddr_nz (value_as_address (orig_val));
      exp_string = (char *) xmalloc (2 + strlen (type_str) + 6 + strlen (addr_str) + 1);
      sprintf (exp_string, "*((%s) 0x%s)", type_str, addr_str);
      xfree (type_str);
      tmp_str = exp_string;
      exp = parse_exp_1 (&tmp_str, 0, 0);

      /* Since we are just watching the address, we don't want to
	 pin it to a given frame.  */

      exp_valid_block = NULL;
      innermost_block = NULL;
    }
  else
    {
      /* Parse arguments.  */
      innermost_block = NULL;
      exp_start = arg;
      exp = parse_exp_1 (&arg, 0, 0);
      exp_end = arg;
      exp_valid_block = innermost_block;
      exp_string = savestring (exp_start, exp_end - exp_start);
    }

  mark = value_mark ();
  val = evaluate_expression (exp);
  if (TYPE_CODE (value_type (val)) == TYPE_CODE_ERROR)
    error ("Can not watch an expression of unknown type.");
  release_value (val);
  if (value_lazy (val))
    value_fetch_lazy (val);
  
  /* END APPLE LOCAL */

  tok = arg;
  while (*tok == ' ' || *tok == '\t')
    tok++;
  end_tok = tok;

  while (*end_tok != ' ' && *end_tok != '\t' && *end_tok != '\000')
    end_tok++;

  toklen = end_tok - tok;
  if (toklen >= 1 && strncmp (tok, "if", toklen) == 0)
    {
      tok = cond_start = end_tok + 1;
      cond = parse_exp_1 (&tok, 0, 0);
      cond_end = tok;
    }
  if (*tok)
    error (_("Junk at end of command."));

  if (accessflag == hw_read)
    bp_type = bp_read_watchpoint;
  else if (accessflag == hw_access)
    bp_type = bp_access_watchpoint;
  else
    bp_type = bp_hardware_watchpoint;

  mem_cnt = can_use_hardware_watchpoint (val);
  if (mem_cnt == 0 && bp_type != bp_hardware_watchpoint)
    error (_("Expression cannot be implemented with read/access watchpoint."));
  if (mem_cnt != 0)
    {
      i = hw_watchpoint_used_count (bp_type, &other_type_used);
      target_resources_ok = 
	TARGET_CAN_USE_HARDWARE_WATCHPOINT (bp_type, i + mem_cnt, 
					    other_type_used);
      if (target_resources_ok == 0 && bp_type != bp_hardware_watchpoint)
	error (_("Target does not support this type of hardware watchpoint."));

      if (target_resources_ok < 0 && bp_type != bp_hardware_watchpoint)
	error (_("Target can only support one kind of HW watchpoint at a time."));
    }

#if defined(HPUXHPPA)
  /*  On HP-UX if you set a h/w
     watchpoint before the "run" command, the inferior dies with a e.g.,
     SIGILL once you start it.  I initially believed this was due to a
     bad interaction between page protection traps and the initial
     startup sequence by the dynamic linker.

     However, I tried avoiding that by having HP-UX's implementation of
     TARGET_CAN_USE_HW_WATCHPOINT return FALSE if there was no inferior_ptid
     yet, which forced slow watches before a "run" or "attach", and it
     still fails somewhere in the startup code.

     Until I figure out what's happening, I'm disallowing watches altogether
     before the "run" or "attach" command.  We'll tell the user they must
     set watches after getting the program started. */
  if (!target_has_execution)
    {
      warning (_("can't do that without a running program; try \"break main\"), \"run\" first");
      return;
    }
#endif /* HPUXHPPA */

  /* Change the type of breakpoint to an ordinary watchpoint if a hardware
     watchpoint could not be set.  */
  if (!mem_cnt || target_resources_ok <= 0)
    bp_type = bp_watchpoint;

  /* Now set up the breakpoint.  */
      b = set_raw_breakpoint (sal, bp_type, 0);
  set_breakpoint_count (breakpoint_count + 1);
  b->number = breakpoint_count;
  b->disposition = disp_donttouch;
  b->exp = exp;
  b->exp_valid_block = exp_valid_block;
  b->exp_string = exp_string;
  b->val = val;
  b->cond = cond;
  if (cond_start)
    b->cond_string = savestring (cond_start, cond_end - cond_start);
  else
    b->cond_string = 0;

  b->bp_set_state = bp_state_set;

  frame = block_innermost_frame (exp_valid_block);
  if (frame)
    {
      prev_frame = get_prev_frame (frame);
      b->watchpoint_frame = get_frame_id (frame);
    }
  else
    {
      memset (&b->watchpoint_frame, 0, sizeof (b->watchpoint_frame));
    }

  /* If the expression is "local", then set up a "watchpoint scope"
     breakpoint at the point where we've left the scope of the watchpoint
     expression.  */
  if (innermost_block)
    {
      if (prev_frame)
	{
	  struct breakpoint *scope_breakpoint;
	  scope_breakpoint = create_internal_breakpoint (get_frame_pc (prev_frame),
							 bp_watchpoint_scope);

	  scope_breakpoint->enable_state = bp_enabled;

	  /* Automatically delete the breakpoint when it hits.  */
	  scope_breakpoint->disposition = disp_del;

	  /* Only break in the proper frame (help with recursion).  */
	  scope_breakpoint->frame_id = get_frame_id (prev_frame);

	  /* Set the address at which we will stop.  */
	  scope_breakpoint->loc->requested_address
	    = get_frame_pc (prev_frame);
	  scope_breakpoint->loc->address
	    = adjust_breakpoint_address (scope_breakpoint->loc->requested_address,
	                                 scope_breakpoint->type);

	  /* The scope breakpoint is related to the watchpoint.  We
	     will need to act on them together.  */
	  b->related_breakpoint = scope_breakpoint;
	}
    }
  value_free_to_mark (mark);
  mention (b);
}

/* Return count of locations need to be watched and can be handled
   in hardware.  If the watchpoint can not be handled
   in hardware return zero.  */

#if !defined(TARGET_REGION_OK_FOR_HW_WATCHPOINT)
#define TARGET_REGION_OK_FOR_HW_WATCHPOINT(ADDR,LEN) \
     (TARGET_REGION_SIZE_OK_FOR_HW_WATCHPOINT(LEN))
#endif

static int
can_use_hardware_watchpoint (struct value *v)
{
  int found_memory_cnt = 0;
  struct value *head = v;

  /* Did the user specifically forbid us to use hardware watchpoints? */
  if (!can_use_hw_watchpoints)
    return 0;

  /* Make sure that the value of the expression depends only upon
     memory contents, and values computed from them within GDB.  If we
     find any register references or function calls, we can't use a
     hardware watchpoint.

     The idea here is that evaluating an expression generates a series
     of values, one holding the value of every subexpression.  (The
     expression a*b+c has five subexpressions: a, b, a*b, c, and
     a*b+c.)  GDB's values hold almost enough information to establish
     the criteria given above --- they identify memory lvalues,
     register lvalues, computed values, etcetera.  So we can evaluate
     the expression, and then scan the chain of values that leaves
     behind to decide whether we can detect any possible change to the
     expression's final value using only hardware watchpoints.

     However, I don't think that the values returned by inferior
     function calls are special in any way.  So this function may not
     notice that an expression involving an inferior function call
     can't be watched with hardware watchpoints.  FIXME.  */
  for (; v; v = value_next (v))
    {
      if (VALUE_LVAL (v) == lval_memory)
	{
	  if (value_lazy (v))
	    /* A lazy memory lvalue is one that GDB never needed to fetch;
	       we either just used its address (e.g., `a' in `a.b') or
	       we never needed it at all (e.g., `a' in `a,b').  */
	    ;
	  else
	    {
	      /* Ahh, memory we actually used!  Check if we can cover
                 it with hardware watchpoints.  */
	      struct type *vtype = check_typedef (value_type (v));

	      /* We only watch structs and arrays if user asked for it
		 explicitly, never if they just happen to appear in a
		 middle of some value chain.  */
	      if (v == head
		  || (TYPE_CODE (vtype) != TYPE_CODE_STRUCT
		      && TYPE_CODE (vtype) != TYPE_CODE_ARRAY))
		{
		  CORE_ADDR vaddr = VALUE_ADDRESS (v) + value_offset (v);
		  int       len   = TYPE_LENGTH (value_type (v));

		  if (!TARGET_REGION_OK_FOR_HW_WATCHPOINT (vaddr, len))
		    return 0;
		  else
		    found_memory_cnt++;
		}
	    }
	}
      else if (VALUE_LVAL (v) != not_lval
	       && deprecated_value_modifiable (v) == 0)
	return 0;	/* ??? What does this represent? */
      else if (VALUE_LVAL (v) == lval_register)
	return 0;	/* cannot watch a register with a HW watchpoint */
      /* APPLE LOCAL begin literal register setting */
      else if (VALUE_LVAL (v) == lval_register_literal)
	return 0;	/* cannot watch a register with a HW watchpoint */
      /* APPLE LOCAL end literal register setting */
    }

  /* The expression itself looks suitable for using a hardware
     watchpoint, but give the target machine a chance to reject it.  */
  return found_memory_cnt;
}

void
watch_command_wrapper (char *arg, int by_location, int from_tty)
{
  watch_command_1 (arg, hw_write, by_location, from_tty);
}

int
detect_location_arg (char **arg)
{
  int by_location = 0;
  if (arg == NULL || *arg == NULL)
    return 0;

  while (**arg == ' ' || **arg == '\t')
    (*arg)++;

  if (strstr (*arg, "-location ") == *arg)
    {
      by_location = 1;
      (*arg) += strlen ("-location ");
    }
  return by_location;
}

static void
watch_command (char *arg, int from_tty)
{
  int by_location = detect_location_arg (&arg);
  watch_command_1 (arg, hw_write, by_location, from_tty);
}

void
rwatch_command_wrapper (char *arg, int by_location, int from_tty)
{
  watch_command_1 (arg, hw_read, by_location, from_tty);
}

static void
rwatch_command (char *arg, int from_tty)
{
  int by_location = detect_location_arg (&arg);
  watch_command_1 (arg, hw_read, by_location, from_tty);
}

void
awatch_command_wrapper (char *arg, int by_location, int from_tty)
{
  watch_command_1 (arg, hw_access, by_location, from_tty);
}

static void
awatch_command (char *arg, int from_tty)
{
  int by_location = detect_location_arg (&arg);
  watch_command_1 (arg, hw_access, by_location, from_tty);
}


/* Helper routines for the until_command routine in infcmd.c.  Here
   because it uses the mechanisms of breakpoints.  */

/* This function is called by fetch_inferior_event via the
   cmd_continuation pointer, to complete the until command. It takes
   care of cleaning up the temporary breakpoints set up by the until
   command. */
static void
until_break_command_continuation (struct continuation_arg *arg)
{
  struct cleanup *cleanups;

  cleanups = (struct cleanup *) arg->data.pointer;
  do_exec_cleanups (cleanups);
}

void
until_break_command (char *arg, int from_tty, int anywhere)
{
  struct symtabs_and_lines sals;
  struct symtab_and_line sal;
  struct frame_info *prev_frame = get_prev_frame (deprecated_selected_frame);
  struct breakpoint *breakpoint;
  struct cleanup *old_chain;
  struct continuation_arg *arg1;


  clear_proceed_status ();

  /* Set a breakpoint where the user wants it and at return from
     this function */

  if (default_breakpoint_valid)
    /* APPLE LOCAL begin return multiple symbols */
    sals = decode_line_1 (&arg, 1, default_breakpoint_symtab,
			  default_breakpoint_line, (char ***) NULL, NULL, 0);
    /* APPLE LOCAL end return multiple symbols */
  else
    /* APPLE LOCAL begin return multiple symbols */
    sals = decode_line_1 (&arg, 1, (struct symtab *) NULL, 
			  0, (char ***) NULL, NULL, 0);
    /* APPLE LOCAL end return multiple symbols */

  if (sals.nelts != 1)
    error (_("Couldn't get information on specified line."));

  sal = sals.sals[0];
  xfree (sals.sals);	/* malloc'd, so freed */

  if (*arg)
    error (_("Junk at end of arguments."));

  resolve_sal_pc (&sal);

  if (anywhere)
    /* If the user told us to continue until a specified location,
       we don't specify a frame at which we need to stop.  */
    breakpoint = set_momentary_breakpoint (sal, null_frame_id, bp_until);
  else
    /* Otherwise, specify the current frame, because we want to stop only
       at the very same frame.  */
    breakpoint = set_momentary_breakpoint (sal,
					   get_frame_id (deprecated_selected_frame),
					   bp_until);

  if (!target_can_async_p ())
    old_chain = make_cleanup_delete_breakpoint (breakpoint);
  else
    old_chain = make_exec_cleanup_delete_breakpoint (breakpoint);

  /* If we are running asynchronously, and the target supports async
     execution, we are not waiting for the target to stop, in the call
     tp proceed, below. This means that we cannot delete the
     brekpoints until the target has actually stopped. The only place
     where we get a chance to do that is in fetch_inferior_event, so
     we must set things up for that. */

  if (target_can_async_p ())
    {
      /* In this case the arg for the continuation is just the point
         in the exec_cleanups chain from where to start doing
         cleanups, because all the continuation does is the cleanups in
         the exec_cleanup_chain. */
      arg1 =
	(struct continuation_arg *) xmalloc (sizeof (struct continuation_arg));
      arg1->next         = NULL;
      arg1->data.pointer = old_chain;

      add_continuation (until_break_command_continuation, arg1);
    }

  /* Keep within the current frame, or in frames called by the current
     one.  */
  if (prev_frame)
    {
      sal = find_pc_line (get_frame_pc (prev_frame), 0);
      sal.pc = get_frame_pc (prev_frame);
      breakpoint = set_momentary_breakpoint (sal, get_frame_id (prev_frame),
					     bp_until);
      if (!target_can_async_p ())
	make_cleanup_delete_breakpoint (breakpoint);
      else
	make_exec_cleanup_delete_breakpoint (breakpoint);
    }

  proceed (-1, TARGET_SIGNAL_DEFAULT, 0);
  /* Do the cleanups now, anly if we are not running asynchronously,
     of if we are, but the target is still synchronous. */
  if (!target_can_async_p ())
    do_cleanups (old_chain);
}

static void
ep_skip_leading_whitespace (char **s)
{
  if ((s == NULL) || (*s == NULL))
    return;
  while (isspace (**s))
    *s += 1;
}

/* This function examines a string, and attempts to find a token
   that might be an event name in the leading characters.  If a
   possible match is found, a pointer to the last character of
   the token is returned.  Else, NULL is returned. */

static char *
ep_find_event_name_end (char *arg)
{
  char *s = arg;
  char *event_name_end = NULL;

  /* If we could depend upon the presense of strrpbrk, we'd use that... */
  if (arg == NULL)
    return NULL;

  /* We break out of the loop when we find a token delimiter.
     Basically, we're looking for alphanumerics and underscores;
     anything else delimites the token. */
  while (*s != '\0')
    {
      if (!isalnum (*s) && (*s != '_'))
	break;
      event_name_end = s;
      s++;
    }

  return event_name_end;
}


/* This function attempts to parse an optional "if <cond>" clause
   from the arg string.  If one is not found, it returns NULL.

   Else, it returns a pointer to the condition string.  (It does not
   attempt to evaluate the string against a particular block.)  And,
   it updates arg to point to the first character following the parsed
   if clause in the arg string. */

static char *
ep_parse_optional_if_clause (char **arg)
{
  char *cond_string;

  if (((*arg)[0] != 'i') || ((*arg)[1] != 'f') || !isspace ((*arg)[2]))
    return NULL;

  /* Skip the "if" keyword. */
  (*arg) += 2;

  /* Skip any extra leading whitespace, and record the start of the
     condition string. */
  ep_skip_leading_whitespace (arg);
  cond_string = *arg;

  /* Assume that the condition occupies the remainder of the arg string. */
  (*arg) += strlen (cond_string);

  return cond_string;
}

/* This function attempts to parse an optional filename from the arg
   string.  If one is not found, it returns NULL.

   Else, it returns a pointer to the parsed filename.  (This function
   makes no attempt to verify that a file of that name exists, or is
   accessible.)  And, it updates arg to point to the first character
   following the parsed filename in the arg string.

   Note that clients needing to preserve the returned filename for
   future access should copy it to their own buffers. */
static char *
ep_parse_optional_filename (char **arg)
{
  static char filename[1024];
  char *arg_p = *arg;
  int i;
  char c;

  if ((*arg_p == '\0') || isspace (*arg_p))
    return NULL;

  for (i = 0;; i++)
    {
      c = *arg_p;
      if (isspace (c))
	c = '\0';
      filename[i] = c;
      if (c == '\0')
	break;
      arg_p++;
    }
  *arg = arg_p;

  return filename;
}

/* Commands to deal with catching events, such as signals, exceptions,
   process start/exit, etc.  */

typedef enum
{
  catch_fork, catch_vfork
}
catch_fork_kind;

static void
catch_fork_command_1 (catch_fork_kind fork_kind, char *arg, int tempflag,
		      int from_tty)
{
  char *cond_string = NULL;

  ep_skip_leading_whitespace (&arg);

  /* The allowed syntax is:
     catch [v]fork
     catch [v]fork if <cond>

     First, check if there's an if clause. */
  cond_string = ep_parse_optional_if_clause (&arg);

  if ((*arg != '\0') && !isspace (*arg))
    error (_("Junk at end of arguments."));

  /* If this target supports it, create a fork or vfork catchpoint
     and enable reporting of such events. */
  switch (fork_kind)
    {
    case catch_fork:
      create_fork_event_catchpoint (tempflag, cond_string);
      break;
    case catch_vfork:
      create_vfork_event_catchpoint (tempflag, cond_string);
      break;
    default:
      error (_("unsupported or unknown fork kind; cannot catch it"));
      break;
    }
}

static void
catch_exec_command_1 (char *arg, int tempflag, int from_tty)
{
  char *cond_string = NULL;

  ep_skip_leading_whitespace (&arg);

  /* The allowed syntax is:
     catch exec
     catch exec if <cond>

     First, check if there's an if clause. */
  cond_string = ep_parse_optional_if_clause (&arg);

  if ((*arg != '\0') && !isspace (*arg))
    error (_("Junk at end of arguments."));

  /* If this target supports it, create an exec catchpoint
     and enable reporting of such events. */
  create_exec_event_catchpoint (tempflag, cond_string);
}

static void
catch_load_command_1 (char *arg, int tempflag, int from_tty)
{
  char *dll_pathname = NULL;
  char *cond_string = NULL;

  ep_skip_leading_whitespace (&arg);

  /* The allowed syntax is:
     catch load
     catch load if <cond>
     catch load <filename>
     catch load <filename> if <cond>

     The user is not allowed to specify the <filename> after an
     if clause.

     We'll ignore the pathological case of a file named "if".

     First, check if there's an if clause.  If so, then there
     cannot be a filename. */
  cond_string = ep_parse_optional_if_clause (&arg);

  /* If there was an if clause, then there cannot be a filename.
     Else, there might be a filename and an if clause. */
  if (cond_string == NULL)
    {
      dll_pathname = ep_parse_optional_filename (&arg);
      ep_skip_leading_whitespace (&arg);
      cond_string = ep_parse_optional_if_clause (&arg);
    }

  if ((*arg != '\0') && !isspace (*arg))
    error (_("Junk at end of arguments."));

  /* Create a load breakpoint that only triggers when a load of
     the specified dll (or any dll, if no pathname was specified)
     occurs. */
  SOLIB_CREATE_CATCH_LOAD_HOOK (PIDGET (inferior_ptid), tempflag, 
				dll_pathname, cond_string);
}

static void
catch_unload_command_1 (char *arg, int tempflag, int from_tty)
{
  char *dll_pathname = NULL;
  char *cond_string = NULL;

  ep_skip_leading_whitespace (&arg);

  /* The allowed syntax is:
     catch unload
     catch unload if <cond>
     catch unload <filename>
     catch unload <filename> if <cond>

     The user is not allowed to specify the <filename> after an
     if clause.

     We'll ignore the pathological case of a file named "if".

     First, check if there's an if clause.  If so, then there
     cannot be a filename. */
  cond_string = ep_parse_optional_if_clause (&arg);

  /* If there was an if clause, then there cannot be a filename.
     Else, there might be a filename and an if clause. */
  if (cond_string == NULL)
    {
      dll_pathname = ep_parse_optional_filename (&arg);
      ep_skip_leading_whitespace (&arg);
      cond_string = ep_parse_optional_if_clause (&arg);
    }

  if ((*arg != '\0') && !isspace (*arg))
    error (_("Junk at end of arguments."));

  /* Create an unload breakpoint that only triggers when an unload of
     the specified dll (or any dll, if no pathname was specified)
     occurs. */
  SOLIB_CREATE_CATCH_UNLOAD_HOOK (PIDGET (inferior_ptid), tempflag, 
				  dll_pathname, cond_string);
}

/* Commands to deal with catching exceptions.  */

/* APPLE LOCAL begin gnu v3 */
static char * gnu_v3_catch_symbol = "__cxa_begin_catch";
static char *gnu_v3_throw_symbol = "__cxa_throw";

static enum print_stop_action print_exception_catchpoint (struct breakpoint *b);
static void print_one_exception_catchpoint (struct breakpoint *b, CORE_ADDR *last_addr);
static void print_mention_exception_catchpoint (struct breakpoint *b);

static struct breakpoint_ops gnu_v3_exception_catchpoint_ops = {
  print_exception_catchpoint,
  print_one_exception_catchpoint,
  print_mention_exception_catchpoint
};

/* APPLE LOCAL end gnu v3 */

/* Set a breakpoint at the specified callback routine for an
   exception event callback */

/* APPLE LOCAL return a value */
struct breakpoint *
create_exception_catchpoint (int tempflag, char *cond_string,
			     /* APPLE LOCAL gnu v3 */
			     int gnu_v3_p,
                             enum exception_event_kind ex_event,
                             struct symtab_and_line *sal)
{
  struct breakpoint *b;
  int thread = -1;		/* All threads. */
  enum bptype bptype;
  /* APPLE LOCAL gnu v3 */
  char *addr_string = NULL;

  if (!sal)			/* no exception support? */
    /* APPLE LOCAL return a value */
    error (_("Internal error -- no exception support"));

  switch (ex_event)
    {
    case EX_EVENT_THROW:
      /* APPLE LOCAL begin gnu v3 */
      if (gnu_v3_p)
	{
	  bptype = bp_gnu_v3_catch_throw;
	  addr_string = xstrdup (gnu_v3_throw_symbol);
	}
      else
      /* APPLE LOCAL end gnu v3 */
      bptype = bp_catch_throw;
      break;
    case EX_EVENT_CATCH:
      /* APPLE LOCAL begin gnu v3 */
      if (gnu_v3_p)
	{
	  bptype = bp_gnu_v3_catch_catch;
	  addr_string = xstrdup (gnu_v3_catch_symbol);
	}
      else
      /* APPLE LOCAL end gnu v3 */
      bptype = bp_catch_catch;
      break;
    default:                    /* error condition */
      error (_("Internal error -- invalid catchpoint kind"));
    }

  b = set_raw_breakpoint (*sal, bptype, 0);
  set_breakpoint_count (breakpoint_count + 1);
  b->number = breakpoint_count;
  b->cond = NULL;
  b->cond_string = (cond_string == NULL) ? 
    NULL : savestring (cond_string, strlen (cond_string));
  b->thread = thread;
  b->addr_string = NULL;
  b->enable_state = bp_enabled;
  b->disposition = tempflag ? disp_del : disp_donttouch;
  /* APPLE LOCAL begin gnu v3 */
  if (gnu_v3_p)
    {
      b->addr_string = addr_string;
      b->ops = &gnu_v3_exception_catchpoint_ops;
    }
  /* APPLE LOCAL end gnu v3 */
  mention (b);
  /* APPLE LOCAL return a value */
  return b;
}

static enum print_stop_action
print_exception_catchpoint (struct breakpoint *b)
{
  switch (b->type) 
    {
    case bp_gnu_v3_catch_catch:
      printf_filtered ("\nCatchpoint %d (exception caught).", 
                       b->number);

    case bp_catch_catch:
      if (current_exception_event && 
          (CURRENT_EXCEPTION_KIND == EX_EVENT_CATCH))
        {
          annotate_catchpoint (b->number);
          print_catch_info (b);
	  ui_out_print_annotation_string (uiout, 0, "reason", "catch-exception");
          if (ui_out_is_mi_like_p (uiout))
            {
              return PRINT_SRC_AND_LOC;
            }
          else
            {
              /* don't bother to print location frame info */
              return PRINT_SRC_ONLY;
            }
        }
      else
        {
          /* really throw, some other bpstat will handle it */
          return PRINT_UNKNOWN; 
        }
      break;
      
    case bp_gnu_v3_catch_throw:
      printf_filtered ("\nCatchpoint %d (exception thrown).",
                       b->number);

    case bp_catch_throw:
      if (current_exception_event && 
          (CURRENT_EXCEPTION_KIND == EX_EVENT_THROW))
        {
          annotate_catchpoint (b->number);
          print_catch_info (b);

          /* don't bother to print location frame info */
	  ui_out_print_annotation_string (uiout, 0, "reason", "throw-exception");
          if (ui_out_is_mi_like_p (uiout))
            {
              return PRINT_SRC_AND_LOC;
            }
          else
            return PRINT_SRC_ONLY; 
        }
      else
        {
          /* really catch, some other bpstat will handle it */
          return PRINT_UNKNOWN; 
        }
      break;

    default:
      return PRINT_SRC_AND_LOC;
    }
}

static void
print_one_exception_catchpoint (struct breakpoint *b, CORE_ADDR *last_addr)
{
  if (addressprint)
    {
      annotate_field (4);
      ui_out_field_core_addr (uiout, "addr", b->loc->address);
    }
  *last_addr = b->loc->address;
  annotate_field (5);
  
  switch (b->type)
    {
    case bp_catch_catch:
    case bp_gnu_v3_catch_catch:
      ui_out_field_string (uiout, "what", "exception catch");
      break;
    case bp_catch_throw:
    case bp_gnu_v3_catch_throw:
      ui_out_field_string (uiout, "what", "exception throw");
      break;
    default:
      break;
    }
  
  ui_out_spaces (uiout, 1);
}

static void
print_mention_exception_catchpoint (struct breakpoint *b)
{
  if (strstr (b->addr_string, "throw") != NULL)
    printf_filtered (_("Catchpoint %d (throw)"), b->number);
  else
    printf_filtered (_("Catchpoint %d (catch)"), b->number);
}

/* APPLE LOCAL: The gnu_v3_exception handling code in gdb couldn't
   deal with the possibility that there was more than one copy of 
   the __cxa_throw/__cxa_begin_catch symbols in the running process.
   It also didn't handle the possibility that there might be no 
   instances of it in the startup app, and only have it loaded
   later.  I needed to add a bunch of code to take care of these
   possibilities.  */

/* This is a Mac OS X specific hack.  Just always say YES since we are always
   using gnu_v3 exceptions, but they may not show up till after the program
   launches.  */

int
handle_gnu_v3_exceptions (enum exception_event_kind ex_event)
{
  if (ex_event == EX_EVENT_CATCH)
    exception_catchpoint_catch_enabled = 1;
  else if (ex_event == EX_EVENT_THROW)
    exception_catchpoint_throw_enabled = 1;

  return 1;
}


struct sal_chain
{
  struct sal_chain *next;
  struct symtab_and_line sal;
};

void
gnu_v3_update_exception_catchpoints (enum exception_event_kind ex_event,
                              int tempflag, char *cond_string)
{
  enum bptype type;
  char *trigger_func_name, *nameptr;
  struct objfile *objfile;
  struct minimal_symbol *msymbol;
  unsigned int hash;

  switch (ex_event)
    {
    case EX_EVENT_CATCH:
      exception_catchpoint_catch_enabled = 1;
      trigger_func_name = xstrdup (gnu_v3_catch_symbol);
      type = bp_gnu_v3_catch_catch;
      break;
    case EX_EVENT_THROW:
      trigger_func_name = xstrdup (gnu_v3_throw_symbol);
      exception_catchpoint_throw_enabled = 1;
      type = bp_gnu_v3_catch_throw;
      break;
    default:
      internal_error (__FILE__, __LINE__,
		      "update_exception_catchpoint: bad argument.");
    }
  
  nameptr = trigger_func_name;  
  hash = msymbol_hash (nameptr) % MINIMAL_SYMBOL_HASH_SIZE;
  
  ALL_OBJFILES (objfile)
    {
      for (msymbol = objfile->msymbol_hash[hash];
           msymbol != NULL;
           msymbol = msymbol->hash_next)
        if (MSYMBOL_TYPE (msymbol) == mst_text
            && strcmp_iw (SYMBOL_LINKAGE_NAME (msymbol), nameptr) == 0)
          {
            /* We found one, add it here... */
            CORE_ADDR catchpoint_address;
            CORE_ADDR past_prologue;
	    struct symtab_and_line sal;
	    struct breakpoint *b;
	    int found_it = 0;

            init_sal (&sal);
            sal.symtab = NULL;

            catchpoint_address = SYMBOL_VALUE_ADDRESS (msymbol);
            past_prologue = SKIP_PROLOGUE (catchpoint_address);

            sal.pc = past_prologue;
            sal.line = 0;
            sal.end = past_prologue;


	    /* The sals contains ALL the exception catchpoints, but we
	       may have seen some of them already.  Don't put breakpoints
	       on already seen locations.  */
	    ALL_BREAKPOINTS (b)
	      {
		if ((b->type == type)
		    && (b->loc->address == sal.pc))
		  {
		    found_it = 1;
		    break;
		  }
	      }
	    if (!found_it)
	      {
		b = create_exception_catchpoint (tempflag, cond_string, 
						 1, ex_event,
						 &sal);
		if (objfile->name != NULL)
		  b->requested_shlib = xstrdup(objfile->name);
		b->bp_objfile = objfile;
		/* APPLE LOCAL: We may be resetting a catchpoint in a
		   library that hasn't been loaded yet.  If we do this
		   when we rerun, we won't be able to look at the text
		   yet, so we will fail scanning the prologue.  So set
		   ourselves up to re-do the breakpoint if this happens.  */
		if (!target_check_is_objfile_loaded (objfile))
		  b->bp_set_state = bp_state_waiting_load;
		else
		  b->bp_set_state = bp_state_set;
	      }
          }
    }
}

/* APPLE LOCAL: Update the exception catchpoints for event EX_EVENT.  
   If DELETE is 1, all catchpoints  are deleted first.  If OBJFILE is non-null
   then only it is searched for catchpoints.  

   If the catchpoint enabled flag is not set before calling this,
   it will also be set here.
   
   Returns 1 if breakpoints were set, 0 otherwise. */

int 
update_exception_catchpoints (enum exception_event_kind ex_event,
			      int tempflag, char *cond_string,
			      int delete, struct objfile *objfile)
{
  struct symtabs_and_lines *sals;
  
  if (ex_event == EX_EVENT_CATCH)
    exception_catchpoint_catch_enabled = 1;
  else if (ex_event == EX_EVENT_THROW)
    exception_catchpoint_throw_enabled = 1;

  sals = target_find_exception_catchpoints (ex_event,
					    objfile);
  if (sals && sals != (struct symtabs_and_lines *) -1)
    {
      int i;
      
      if (delete)
	{
	  struct breakpoint *b, *tmp;
	  enum bptype type;
	  int found_it;
	  
	  switch (ex_event)
	    {
	    case EX_EVENT_CATCH:
	      type = bp_catch_catch;
	      break;
	    case EX_EVENT_THROW:
	      type = bp_catch_throw;
	      break;
	    default:
	      internal_error (__FILE__, __LINE__, 
			      "update_exception_catchpoint: bad argument.");
	    }
	  
	  ALL_BREAKPOINTS_SAFE (b, tmp)
	    {
	      if (b->type == type)
		{
		  found_it = 0;
		  
		  for (i = 0; i < sals->nelts; i++)
		    {
		      if (b->loc->address == sals->sals[i].pc)
			{
			  sals->sals[i].pc = (CORE_ADDR) -1;
			      found_it = 1;
			      break;
			}
		    }
		  if (!found_it)
		    delete_breakpoint(b);
		}
	    }
	  for (i = 0; i < sals->nelts; i++)
	    if (sals->sals[i].pc != (CORE_ADDR) -1)
	      create_exception_catchpoint (tempflag, cond_string, 1,
					   ex_event, &(sals->sals[i]));
	}
      else
	{
	  for (i = 0; i < sals->nelts; i++)
	    {
	      create_exception_catchpoint (tempflag, cond_string, 1,
					   ex_event, &(sals->sals[i]));
	    }
	}
      if (sals->nelts >  0)
	xfree (sals->sals);
      xfree (sals);
    }
  else
    return 0;

  return 1;
}

/* APPLE LOCAL: Figure out whether the current exception object is one we 
   want to stop for.  Returns 1 if yes, and 0 if no. */

static int 
current_exception_should_stop (void)
{
  char *obj_type = CURRENT_EXCEPTION_TYPE;
  char *exception_type_regexp;
  char *msg;

  /* If we can't figure out the object type, always stop. */
  if (!obj_type)
    return 1;

  if (CURRENT_EXCEPTION_KIND == EX_EVENT_THROW)
    exception_type_regexp = exception_throw_type_regexp;
  else
    exception_type_regexp = exception_catch_type_regexp;

  if (exception_type_regexp)
    {
      msg = (char *) re_comp (exception_type_regexp);
      if (msg) 
	{
	  warning ("Error compiling exception type regexp: %s", msg);
	  return 1;
	}
      return re_exec (obj_type);
    }
  else
    return 1;  
}

/* Deal with "catch catch" and "catch throw" commands */

static void
catch_exception_command_1 (enum exception_event_kind ex_event, char *arg,
			   int tempflag, int from_tty)
{
  char *cond_string = NULL;
  int retval;

  ep_skip_leading_whitespace (&arg);

  cond_string = ep_parse_optional_if_clause (&arg);

  if ((*arg != '\0') && !isspace (*arg))
    error (_("Junk at end of arguments."));

  if ((ex_event != EX_EVENT_THROW) &&
      (ex_event != EX_EVENT_CATCH))
    error (_("Unsupported or unknown exception event; cannot catch it"));

  /* APPLE LOCAL begin gnu v3 exceptions */
  if (handle_gnu_v3_exceptions (ex_event))
    {
      gnu_v3_update_exception_catchpoints (ex_event, tempflag, cond_string);
      return;
    }

  /* See if we can find a callback routine */
  retval = target_enable_exception_callback (ex_event, 1);

  if (!retval) 
    error (_("Could not enable exception callback"));

  if (!update_exception_catchpoints (ex_event, tempflag, cond_string, 
                                     0, NULL))
  /* APPLE LOCAL end gnu v3 exceptions */
  warning (_("Unsupported with this platform/compiler combination."));
}

/* Cover routine to allow wrapping target_enable_exception_catchpoints
   inside a catch_errors */

static int
cover_target_enable_exception_callback (void *arg)
{
  args_for_catchpoint_enable *args = arg;

  return target_enable_exception_callback (args->kind, args->enable_p);
}

static void
catch_command_1 (char *arg, int tempflag, int from_tty)
{

  /* The first argument may be an event name, such as "start" or "load".
     If so, then handle it as such.  If it doesn't match an event name,
     then attempt to interpret it as an exception name.  (This latter is
     the v4.16-and-earlier GDB meaning of the "catch" command.)

     First, try to find the bounds of what might be an event name. */
  char *arg1_start = arg;
  char *arg1_end;
  int arg1_length;

  if (arg1_start == NULL)
    {
      /* Old behaviour was to use pre-v-4.16 syntax */
      /* catch_throw_command_1 (arg1_start, tempflag, from_tty); */
      /* return; */
      /* Now, this is not allowed */
      error (_("Catch requires an event name."));

    }
  arg1_end = ep_find_event_name_end (arg1_start);
  if (arg1_end == NULL)
    error (_("catch requires an event"));
  arg1_length = arg1_end + 1 - arg1_start;

  /* Try to match what we found against known event names. */
  if (strncmp (arg1_start, "signal", arg1_length) == 0)
    {
      error (_("Catch of signal not yet implemented"));
    }
  else if (strncmp (arg1_start, "catch", arg1_length) == 0)
    {
      /* APPLE LOCAL exception catchpoints */
      exception_catchpoint_catch_enabled = 1;
      catch_exception_command_1 (EX_EVENT_CATCH, arg1_end + 1, 
				 tempflag, from_tty);
    }
  else if (strncmp (arg1_start, "throw", arg1_length) == 0)
    {
      /* APPLE LOCAL exception catchpoints */
      exception_catchpoint_throw_enabled = 1;
      catch_exception_command_1 (EX_EVENT_THROW, arg1_end + 1, 
				 tempflag, from_tty);
    }
  else if (strncmp (arg1_start, "thread_start", arg1_length) == 0)
    {
      error (_("Catch of thread_start not yet implemented"));
    }
  else if (strncmp (arg1_start, "thread_exit", arg1_length) == 0)
    {
      error (_("Catch of thread_exit not yet implemented"));
    }
  else if (strncmp (arg1_start, "thread_join", arg1_length) == 0)
    {
      error (_("Catch of thread_join not yet implemented"));
    }
  else if (strncmp (arg1_start, "start", arg1_length) == 0)
    {
      error (_("Catch of start not yet implemented"));
    }
  else if (strncmp (arg1_start, "exit", arg1_length) == 0)
    {
      error (_("Catch of exit not yet implemented"));
    }
  else if (strncmp (arg1_start, "fork", arg1_length) == 0)
    {
      catch_fork_command_1 (catch_fork, arg1_end + 1, tempflag, from_tty);
    }
  else if (strncmp (arg1_start, "vfork", arg1_length) == 0)
    {
      catch_fork_command_1 (catch_vfork, arg1_end + 1, tempflag, from_tty);
    }
  else if (strncmp (arg1_start, "exec", arg1_length) == 0)
    {
      catch_exec_command_1 (arg1_end + 1, tempflag, from_tty);
    }
  else if (strncmp (arg1_start, "load", arg1_length) == 0)
    {
      catch_load_command_1 (arg1_end + 1, tempflag, from_tty);
    }
  else if (strncmp (arg1_start, "unload", arg1_length) == 0)
    {
      catch_unload_command_1 (arg1_end + 1, tempflag, from_tty);
    }
  else if (strncmp (arg1_start, "stop", arg1_length) == 0)
    {
      error (_("Catch of stop not yet implemented"));
    }

  /* This doesn't appear to be an event name */

  else
    {
      /* Pre-v.4.16 behaviour was to treat the argument
         as the name of an exception */
      /* catch_throw_command_1 (arg1_start, tempflag, from_tty); */
      /* Now this is not allowed */
      error (_("Unknown event kind specified for catch"));

    }
}

/* APPLE LOCAL begin mi */
/* Helper functions for the mi mi_cmd_break_catch.  */

int 
exception_catchpoints_enabled (enum exception_event_kind ex_event)
{
  switch (ex_event)
    {
    case EX_EVENT_CATCH:
      return exception_catchpoint_catch_enabled;
      break;
    case EX_EVENT_THROW:
      return exception_catchpoint_throw_enabled;
      break;
    default:
      internal_error (__FILE__, __LINE__, 
		      "Unknown event kind for execption_catchpoints_enabled");
      return 0;
    }
}

void
disable_exception_catch (enum exception_event_kind ex_event)
{
  struct breakpoint *b, *temp;
  enum bptype type, gnu_v3_type;

  if (! handle_gnu_v3_exceptions (ex_event))
    {
      target_enable_exception_callback (ex_event, 0);
    }

  switch (ex_event)
    {
    case EX_EVENT_THROW:
      exception_catchpoint_throw_enabled = 0;
      type = bp_catch_throw;
      gnu_v3_type = bp_gnu_v3_catch_throw;
      break;
    case EX_EVENT_CATCH:
      exception_catchpoint_catch_enabled = 0;
      type = bp_catch_catch;
      gnu_v3_type = bp_gnu_v3_catch_catch;
      break;
    default:
      internal_error (__FILE__, __LINE__, 
		      "disable_exception_catch: invalid event type.");
    }

  ALL_BREAKPOINTS_SAFE (b, temp)
    if (b->type == type || b->type == gnu_v3_type)
      delete_breakpoint (b);
}
/* APPLE LOCAL end mi */

/* Used by the gui, could be made a worker for other things. */

struct breakpoint *
set_breakpoint_sal (struct symtab_and_line sal)
{
  struct breakpoint *b;
  b = set_raw_breakpoint (sal, bp_breakpoint, 0);
  set_breakpoint_count (breakpoint_count + 1);
  b->number = breakpoint_count;
  b->cond = 0;
  b->thread = -1;
  return b;
}

static void
catch_command (char *arg, int from_tty)
{
  catch_command_1 (arg, 0, from_tty);
}


static void
tcatch_command (char *arg, int from_tty)
{
  catch_command_1 (arg, 1, from_tty);
}

/* Delete breakpoints by address or line.  */

static void
clear_command (char *arg, int from_tty)
{
  struct breakpoint *b, *tmp, *prev, *found;
  int default_match;
  struct symtabs_and_lines sals;
  struct symtab_and_line sal;
  int i;

  if (arg)
    {
      sals = decode_line_spec (arg, 1);
      default_match = 0;
    }
  else
    {
      sals.sals = (struct symtab_and_line *)
	xmalloc (sizeof (struct symtab_and_line));
      make_cleanup (xfree, sals.sals);
      init_sal (&sal);		/* initialize to zeroes */
      sal.line = default_breakpoint_line;
      sal.symtab = default_breakpoint_symtab;
      sal.pc = default_breakpoint_address;
      if (sal.symtab == 0)
	error (_("No source file specified."));

      sals.sals[0] = sal;
      sals.nelts = 1;

      default_match = 1;
    }

  /* For each line spec given, delete bps which correspond
     to it.  Do it in two passes, solely to preserve the current
     behavior that from_tty is forced true if we delete more than
     one breakpoint.  */

  found = NULL;
  for (i = 0; i < sals.nelts; i++)
    {
      /* If exact pc given, clear bpts at that pc.
         If line given (pc == 0), clear all bpts on specified line.
         If defaulting, clear all bpts on default line
         or at default pc.

         defaulting    sal.pc != 0    tests to do

         0              1             pc
         1              1             pc _and_ line
         0              0             line
         1              0             <can't happen> */

      sal = sals.sals[i];
      prev = NULL;

      /* Find all matching breakpoints, remove them from the
	 breakpoint chain, and add them to the 'found' chain.  */
      ALL_BREAKPOINTS_SAFE (b, tmp)
	{
	  /* Are we going to delete b? */
	  if (b->type != bp_none
	      && b->type != bp_watchpoint
	      && b->type != bp_hardware_watchpoint
	      && b->type != bp_read_watchpoint
	      && b->type != bp_access_watchpoint
	      /* Not if b is a watchpoint of any sort... */
	      && (((sal.pc && (b->loc->address == sal.pc)) 
		   && (!section_is_overlay (b->loc->section)
		       || b->loc->section == sal.section))
		  /* Yes, if sal.pc matches b (modulo overlays).  */
		  || ((default_match || (0 == sal.pc))
		      && b->source_file != NULL
		      && sal.symtab != NULL
		      && strcmp (b->source_file, sal.symtab->filename) == 0
		      && b->line_number == sal.line)))
	    /* Yes, if sal source file and line matches b.  */
	    {
	      /* Remove it from breakpoint_chain...  */
	      if (b == breakpoint_chain)
		{
		  /* b is at the head of the list */
		  breakpoint_chain = b->next;
		}
	      else
		{
		  prev->next = b->next;
		}
	      /* And add it to 'found' chain.  */
	      b->next = found;
	      found = b;
	    }
	  else
	    {
	      /* Keep b, and keep a pointer to it.  */
	      prev = b;
	    }
	}
    }
  /* Now go thru the 'found' chain and delete them.  */
  if (found == 0)
    {
      if (arg)
	error (_("No breakpoint at %s."), arg);
      else
	error (_("No breakpoint at this line."));
    }

  if (found->next)
    from_tty = 1;		/* Always report if deleted more than one */
  if (from_tty)
    {
      if (!found->next)
	printf_unfiltered (_("Deleted breakpoint "));
      else
	printf_unfiltered (_("Deleted breakpoints "));
    }
  breakpoints_changed ();
  while (found)
    {
      if (from_tty)
	printf_unfiltered ("%d ", found->number);
      tmp = found->next;
      delete_breakpoint (found);
      found = tmp;
    }
  if (from_tty)
    putchar_unfiltered ('\n');
}

/* Delete breakpoint in BS if they are `delete' breakpoints and
   all breakpoints that are marked for deletion, whether hit or not.
   This is called after any breakpoint is hit, or after errors.  */

void
breakpoint_auto_delete (bpstat bs)
{
  struct breakpoint *b, *temp;

  for (; bs; bs = bs->next)
    if (bs->breakpoint_at && bs->breakpoint_at->disposition == disp_del
	&& bs->stop)
      delete_breakpoint (bs->breakpoint_at);

  ALL_BREAKPOINTS_SAFE (b, temp)
  {
    if (b->disposition == disp_del_at_next_stop)
      delete_breakpoint (b);
  }
}

/* Delete a breakpoint and clean up all traces of it in the data
   structures. */

void
delete_breakpoint (struct breakpoint *bpt)
{
  struct breakpoint *b;
  bpstat bs;
  struct bp_location *loc;

  gdb_assert (bpt != NULL);

  /* Has this bp already been deleted?  This can happen because multiple
     lists can hold pointers to bp's.  bpstat lists are especial culprits.

     One example of this happening is a watchpoint's scope bp.  When the
     scope bp triggers, we notice that the watchpoint is out of scope, and
     delete it.  We also delete its scope bp.  But the scope bp is marked
     "auto-deleting", and is already on a bpstat.  That bpstat is then
     checked for auto-deleting bp's, which are deleted.

     A real solution to this problem might involve reference counts in bp's,
     and/or giving them pointers back to their referencing bpstat's, and
     teaching delete_breakpoint to only free a bp's storage when no more
     references were extent.  A cheaper bandaid was chosen.  */
  if (bpt->type == bp_none)
    return;

  if (deprecated_delete_breakpoint_hook)
    deprecated_delete_breakpoint_hook (bpt);
  breakpoint_delete_event (bpt->number);

  if (bpt->loc->inserted)
    remove_breakpoint (bpt->loc, mark_inserted);

  free_valchain (bpt->loc);

  if (breakpoint_chain == bpt)
    breakpoint_chain = bpt->next;

  if (bp_location_chain == bpt->loc)
    bp_location_chain = bpt->loc->next;

  /* If we have callback-style exception catchpoints, don't go through
     the adjustments to the C++ runtime library etc. if the inferior
     isn't actually running.  target_enable_exception_callback for a
     null target ops vector gives an undesirable error message, so we
     check here and avoid it. Since currently (1997-09-17) only HP-UX aCC's
     exceptions are supported in this way, it's OK for now.  FIXME */
  if (ep_is_exception_catchpoint (bpt) && target_has_execution)
    {
      /* Format possible error msg */
      char *message = xstrprintf ("Error in deleting catchpoint %d:\n",
				  bpt->number);
      struct cleanup *cleanups = make_cleanup (xfree, message);
      args_for_catchpoint_enable args;
      args.kind = bpt->type == bp_catch_catch ? 
	EX_EVENT_CATCH : EX_EVENT_THROW;
      args.enable_p = 0;
      catch_errors (cover_target_enable_exception_callback, &args,
		    message, RETURN_MASK_ALL);
      do_cleanups (cleanups);
    }


  ALL_BREAKPOINTS (b)
    if (b->next == bpt)
    {
      b->next = bpt->next;
      break;
    }

  ALL_BP_LOCATIONS (loc)
    if (loc->next == bpt->loc)
      {
	loc->next = bpt->loc->next;
	break;
      }

  check_duplicates (bpt);
  /* If this breakpoint was inserted, and there is another breakpoint
     at the same address, we need to insert the other breakpoint.  */
  if (bpt->loc->inserted
      && bpt->type != bp_hardware_watchpoint
      && bpt->type != bp_read_watchpoint
      && bpt->type != bp_access_watchpoint
      && bpt->type != bp_catch_fork
      && bpt->type != bp_catch_vfork
      && bpt->type != bp_catch_exec)
    {
      ALL_BREAKPOINTS (b)
	if (b->loc->address == bpt->loc->address
	    && b->loc->section == bpt->loc->section
	    && !b->loc->duplicate
	    && b->enable_state != bp_disabled
	    && b->enable_state != bp_shlib_disabled
	    && !b->pending
	    && b->enable_state != bp_call_disabled)
	{
	  int val;

	  /* We should never reach this point if there is a permanent
	     breakpoint at the same address as the one being deleted.
	     If there is a permanent breakpoint somewhere, it should
	     always be the only one inserted.  */
	  if (b->enable_state == bp_permanent)
	    internal_error (__FILE__, __LINE__,
			    _("another breakpoint was inserted on top of "
			    "a permanent breakpoint"));

	  if (b->type == bp_hardware_breakpoint)
	    val = target_insert_hw_breakpoint (b->loc->address, b->loc->shadow_contents);
	  else
	    val = target_insert_breakpoint (b->loc->address, b->loc->shadow_contents);

	  /* If there was an error in the insert, print a message, then stop execution.  */
	  if (val != 0)
	    {
	      struct ui_file *tmp_error_stream = mem_fileopen ();
	      make_cleanup_ui_file_delete (tmp_error_stream);
	     

	      if (b->type == bp_hardware_breakpoint)
		{
		  fprintf_unfiltered (tmp_error_stream, 
					"Cannot insert hardware breakpoint %d.\n"
				      "You may have requested too many hardware breakpoints.\n",
					b->number);
		  }
		else
		  {
		    fprintf_unfiltered (tmp_error_stream, "Cannot insert breakpoint %d.\n", b->number);
		    fprintf_filtered (tmp_error_stream, "Error accessing memory address ");
		    deprecated_print_address_numeric (b->loc->address, 1, tmp_error_stream);
		    fprintf_filtered (tmp_error_stream, ": %s.\n",
				      safe_strerror (val));
		  }
	      
	      fprintf_unfiltered (tmp_error_stream,"The same program may be running in another process.");
	      target_terminal_ours_for_output ();
	      error_stream(tmp_error_stream); 
	    }
	  else
	    b->loc->inserted = 1;
	}
    }

  free_command_lines (&bpt->commands);
  if (bpt->cond)
    xfree (bpt->cond);
  if (bpt->cond_string != NULL)
    xfree (bpt->cond_string);
  if (bpt->addr_string != NULL)
    xfree (bpt->addr_string);
  if (bpt->exp != NULL)
    xfree (bpt->exp);
  if (bpt->exp_string != NULL)
    xfree (bpt->exp_string);
  if (bpt->val != NULL)
    value_free (bpt->val);
  if (bpt->source_file != NULL)
    xfree (bpt->source_file);
  if (bpt->dll_pathname != NULL)
    xfree (bpt->dll_pathname);
  if (bpt->triggered_dll_pathname != NULL)
    xfree (bpt->triggered_dll_pathname);
  if (bpt->exec_pathname != NULL)
    xfree (bpt->exec_pathname);
  /* APPLE LOCAL begin requested shlib */
  if (bpt->requested_shlib != NULL)
    xfree (bpt->requested_shlib);
  /* APPLE LOCAL end requested shlib */
  /* APPLE LOCAL begin radar 5273932  */
  if (bpt->bp_objfile_name != NULL)
    xfree (bpt->bp_objfile_name);
  /* APPLE LOCAL end radar 5273932  */

  /* Be sure no bpstat's are pointing at it after it's been freed.  */
  /* FIXME, how can we find all bpstat's?
     We just check stop_bpstat for now.  */
  for (bs = stop_bpstat; bs; bs = bs->next)
    if (bs->breakpoint_at == bpt)
      {
	bs->breakpoint_at = NULL;
	bs->old_val = NULL;
	/* bs->commands will be freed later.  */
      }
  /* On the chance that someone will soon try again to delete this same
     bp, we mark it as deleted before freeing its storage. */
  bpt->type = bp_none;

  xfree (bpt->loc);
  xfree (bpt);
}

static void
do_delete_breakpoint_cleanup (void *b)
{
  delete_breakpoint (b);
}

struct cleanup *
make_cleanup_delete_breakpoint (struct breakpoint *b)
{
  return make_cleanup (do_delete_breakpoint_cleanup, b);
}

struct cleanup *
make_exec_cleanup_delete_breakpoint (struct breakpoint *b)
{
  return make_exec_cleanup (do_delete_breakpoint_cleanup, b);
}

void
delete_command (char *arg, int from_tty)
{
  struct breakpoint *b, *temp;

  dont_repeat ();

  if (arg == 0)
    {
      int breaks_to_delete = 0;

      /* Delete all breakpoints if no argument.
         Do not delete internal or call-dummy breakpoints, these
         have to be deleted with an explicit breakpoint number argument.  */
      ALL_BREAKPOINTS (b)
      {
	if (b->type != bp_call_dummy &&
	    b->type != bp_shlib_event &&
	    b->type != bp_thread_event &&
	    b->type != bp_overlay_event &&
	    b->number >= 0)
	  breaks_to_delete = 1;
      }

      /* Ask user only if there are some breakpoints to delete.  */
      if (!from_tty
	  || (breaks_to_delete && query (_("Delete all breakpoints? "))))
	{
	  ALL_BREAKPOINTS_SAFE (b, temp)
	  {
	    if (b->type != bp_call_dummy &&
		b->type != bp_shlib_event &&
		b->type != bp_thread_event &&
		b->type != bp_overlay_event &&
		b->number >= 0)
	      delete_breakpoint (b);
	  }
	}
    }
  else
    map_breakpoint_numbers (arg, delete_breakpoint);
}

/* Reset a breakpoint given it's struct breakpoint * BINT.
   APPLE LOCAL using return value
   The value we return ends up being the return value from catch_errors.  */

static int
breakpoint_re_set_one (void *bint)
{
  /* get past catch_errs */
  struct breakpoint *b = (struct breakpoint *) bint;
  struct value *mark;
  int i;
  /* APPLE LOCAL begin dealing correctly with multiple sals.  */
  int found;
  int j;
  /* APPLE LOCAL end dealing correctly with multiple sals.  */
  /* APPLE LOCAL breakpoint fix */
  /* Don't need locals not_found or not_found_ptr */
  struct symtabs_and_lines sals;
  char *s;
  enum enable_state save_enable;
  struct expression *s_exp;
  struct cleanup *restrict_cleanup;

  switch (b->type)
    {
    case bp_none:
      warning (_("attempted to reset apparently deleted breakpoint #%d?"),
	       b->number);
      return 0;
      /* APPLE LOCAL begin gnu_v3 breakpoints */
    case bp_gnu_v3_catch_throw:
    case bp_gnu_v3_catch_catch:
      /* APPLE LOCAL end gnu_v3 breakpoints */
    case bp_breakpoint:
    /* APPLE LOCAL begin subroutine inlining  */
    case bp_inlined_breakpoint:
    /* APPLE LOCAL end subroutine inlining  */
    case bp_hardware_breakpoint:
    case bp_catch_load:
    case bp_catch_unload:
      /* APPLE LOCAL: Our thread event breakpoint is set in a library
	 that will slide, so we need to reset it.  */
    case bp_thread_event:
      if (b->addr_string == NULL)
	{
	  /* Anything without a string can't be re-set. */
	  delete_breakpoint (b);
	  return 0;
	}
      /* HACK: cagney/2001-11-11: kettenis/2001-11-11: MarkK wrote:

	 ``And a hack it is, although Apple's Darwin version of GDB
	 contains an almost identical hack to implement a "future
	 break" command.  It seems to work in many real world cases,
	 but it is easy to come up with a test case where the patch
	 doesn't help at all.''

	 ``It seems that the way GDB implements breakpoints - in -
	 shared - libraries was designed for a.out shared library
	 systems (SunOS 4) where shared libraries were loaded at a
	 fixed address in memory.  Since ELF shared libraries can (and
	 will) be loaded at any address in memory, things break.
	 Fixing this is not trivial.  Therefore, I'm not sure whether
	 we should add this hack to the branch only.  I cannot
	 guarantee that things will be fixed on the trunk in the near
	 future.''

         In case we have a problem, disable this breakpoint.  We'll
         restore its status if we succeed.  Don't disable a
         shlib_disabled breakpoint though.  There's a fair chance we
         can't re-set it if the shared library it's in hasn't been
         loaded yet.  */

      if (b->pending)
	break;

      save_enable = b->enable_state;
      /* APPLE LOCAL begin breakpoint fix */
      /* The FSF version of this test has bp_disabled &
	 bp_shlib_disabled switched.  I don't understand this, you
	 surely don't want to mark a breakpoint USER disabled if you
	 can't resolve it now.  */
      if (b->enable_state != bp_disabled)
        b->enable_state = bp_shlib_disabled;
      /* APPLE LOCAL end breakpoint fix */

      set_language (b->language);
      input_radix = b->input_radix;
      s = b->addr_string;

      /* APPLE LOCAL begin narrow search */
      /* Narrow the search to the requested_shlib if it exists.  */
      if (b->requested_shlib != NULL)
	{
	  /* We may be reloading a library which we found the wrong
	     version of at init time.  So up the load level here just
	     to be safe.  */
	  /* But don't do this for exception catchpoints, 'cause if
	     the shlib changes, that's going to delete the breakpoint
	     out from under us.  */
	  if (b->type != bp_gnu_v3_catch_throw
	      && b->type != bp_gnu_v3_catch_catch)
	    {
	      if (objfile_name_set_load_state (b->requested_shlib, OBJF_SYM_ALL, 0) == -1)
		{
		  warning ("Couldn't raise load state for requested shlib: \"%s\" "
			   "for breakpoint %d.\n",
			   b->requested_shlib, b->number);
		  return 0;
		}
	    }
	  
	  restrict_cleanup 
	    = make_cleanup_restrict_to_shlib (b->requested_shlib);

	  if (restrict_cleanup == (void *) -1)
	    {
	      warning ("Couldn't find requested shlib: \"%s\" "
		       "for breakpoint %d.\n", 
		       b->requested_shlib, b->number);
	      return 0;
	    }	  
	}
      /* APPLE LOCAL begin radar 5273932  */
      else if (b->bp_objfile_name != NULL)
	{
	  /* But don't do this for exception catchpoints, 'cause if
	     the shlib changes, that's going to delete the breakpoint
	     out from under us.  */
	  if (b->type != bp_gnu_v3_catch_throw
	      && b->type != bp_gnu_v3_catch_catch)
	    {
	      if (objfile_name_set_load_state (b->bp_objfile_name, OBJF_SYM_ALL, 0) == -1)
		{
		  warning ("Couldn't raise load state for requested objfile: \"%s\" "
			   "for breakpoint %d/\n", b->bp_objfile_name, b->number);
		}
	    }
	  
	  restrict_cleanup 
	    = make_cleanup_restrict_to_objfile_by_name (b->bp_objfile_name);

	  if (restrict_cleanup == (void *) -1)
	    {
	      warning ("Couldn't find requested objfile: \"%s\" "
		       "for breakpoint %d.\n", b->bp_objfile_name, b->number);
	    }
	}
      /* APPLE LOCAL end radar 5273932  */
      else
	restrict_cleanup = make_cleanup (null_cleanup, NULL);
      /* APPLE LOCAL end narrow search */

      /* APPLE LOCAL begin return multiple symbols */
      sals = decode_line_1 (&s, 1, (struct symtab *) NULL, 0, (char ***) NULL, NULL, 0);
      /* APPLE LOCAL end return multiple symbols */

      /* APPLE LOCAL begin Don't go on if we found nothing...  */
      if (NULL == sals.sals)
	{
	  do_cleanups (restrict_cleanup);
	  return 0;
	}
      /* APPLE LOCAL end Don't go on if we found nothing...  */

      /* APPLE LOCAL begin dealing correctly with multiple sals.  */

      /* If there's more than one sal, figure out the right one to
	 use.  */

      found = 0;
      if (sals.nelts > 1)

	/* Go through the list of sals trying to find one whose
	   pc is the same as the breakpoint location address, or
	   whose pc is the same as the breakpoint location address 
	   plus an offset.   The offset is the beginning address of the
	   section in the objfile that contains the sal pc.  */

	for (j = 0; j < sals.nelts && !found; j++)
	  {

	    /* Check each SAL (until the correct one is found).  */

	    struct objfile *bp_objfile = NULL;
	    struct obj_section *osect = NULL;
	    struct symtab_and_line *sal = &(sals.sals[j]);
	    CORE_ADDR base_pc;
	    CORE_ADDR offset;
	    CORE_ADDR target_pc;

	    /* First, find the objfile for the SAL.  */
	    
	    if (sal->symtab != NULL)
	      bp_objfile = sal->symtab->objfile;
	    else if (sal->section != NULL)
	      {
		osect = find_pc_sect_section (sal->pc, sal->section);
		if (osect)
		  bp_objfile = osect->objfile;
	      }
	    else
	      {
		osect = find_pc_section (sal->pc);
		if (osect)
		  bp_objfile = osect->objfile;
	      }
	    if (bp_objfile && bp_objfile->separate_debug_objfile_backlink)
	      bp_objfile = bp_objfile->separate_debug_objfile_backlink;
	    
	    if (!bp_objfile)
	      continue;

	    /* Next, get the address of the old breakpoint location, and the
	       address in the sal.  */

	    base_pc = b->loc->address;
	    target_pc = sals.sals[j].pc;

	    /* If the two addresses match, we've found the right SAL and
	       we're done.  */

	    if (base_pc == target_pc)
	      {
		i = j;
		found = 1;
	      }
	    else
	      {

		/* If the addresses don't match, we need to try to find an
		   offset to add, from an objfile section,  to make them match.  */
	       
		offset = 0;

		/* Go through all the sections in the objfile, looking for the
		   one that contains the SAL's pc.  */

		ALL_OBJFILE_OSECTIONS (bp_objfile, osect)
		  if (bp_objfile->separate_debug_objfile_backlink == NULL
		      && osect->addr <= sals.sals[j].pc
		      && sals.sals[j].pc<= osect->endaddr)
		    {
		      /* We've found the correct section, so we use it's
			 offset.  */

		      offset = ANOFFSET (bp_objfile->section_offsets,
					 osect->the_bfd_section->index);
		      break;
		    }

		/* If breakpoint address plus offset matches the SAL pc, 
		   we've found the right SAL and we're done.  */

		if (base_pc + offset == target_pc)
		  {
		    i = j;
		    found = 1;
		  }

		/* Otherwise, keep looking.  */
	      }
	}

      /* If we couldn't figure out which SAL is the right one to use, just
	 use the last one (which mimic's the old behavior of this function).  */

      if (!found)
	i = sals.nelts - 1;

      /* APPLE LOCAL end dealing correctly with multiple sals.  */
	{
	  resolve_sal_pc (&sals.sals[i]);

	  /* Reparse conditions, they might contain references to the
	     old symtab.  */
	  if (b->cond_string != NULL)
	    {
	      s = b->cond_string;
	      if (b->cond)
		{
		  xfree (b->cond);
		  /* Avoid re-freeing b->exp if an error during the call
		     to parse_exp_1.  */
		  b->cond = NULL;
		}
	      /* APPLE LOCAL begin don't reparse cond */
	      /* Nulling the cond is OK, since it might contain
		 references to the old symtab.  But DON'T try to
		 reparse it here, since that could - in the case of
		 ObjC - call into the inferior, which leads to
		 infinite recursion of breakpoint_re_set_one.  We will
		 parse it later when we actually go to check the
		 condition.  */
	      /* APPLE LOCAL end don't reparse cond */
	    }

	  /* APPLE LOCAL begin bp_objfile */
	  /* Set the bp_objfile to the objfile that we found this
	     breakpoint in.  This way we will only shlib_disable this
	     breakpoint if the objfile it is in was changed (useful in
	     the case that more than one objfile shares the same
	     memory address.)  */
	  set_bp_objfile (b, &sals.sals[i]);
	  /* APPLE LOCAL end bp_objfile */

	  /* APPLE LOCAL begin breakpoints */
	  /* This is a bit of a hack, but if we are running a target
	     that can know about loaded & not loaded objfiles, it
	     usually reads from target memory.  That means if we tried
	     to set a breakpoint in an unloaded module, we probably
	     weren't able to read the prologue, and so we have set
	     this breakpoint wrong.  So we set the state to
	     bp_state_waiting_load, and breakpoint update will
	     specially try to reset these whenever it gets called.

	     And again, if we have a pure address breakpoint, then
	     just keep setting it.  */

	  if (!(target_check_is_objfile_loaded (b->bp_objfile)
		|| (b->loc->address != 0x0 && b->bp_objfile == NULL)))
	    {
	      b->bp_set_state = bp_state_waiting_load;
	      return 0;
	    }
	  /* APPLE LOCAL end breakpoints */

	  /* We need to re-set the breakpoint if the address changes... */
	  if (b->loc->address != sals.sals[i].pc
	  /* ...or new and old breakpoints both have source files, and
	     the source file name or the line number changes...  */
	      || (b->source_file != NULL
		  && sals.sals[i].symtab != NULL
		  && (strcmp (b->source_file, sals.sals[i].symtab->filename) != 0
		      || b->line_number != sals.sals[i].line)
	      )
	  /* ...or we switch between having a source file and not having
	     one.  */
	      || ((b->source_file == NULL) != (sals.sals[i].symtab == NULL))
	    )
	    {
	      if (b->source_file != NULL)
		xfree (b->source_file);
	      if (sals.sals[i].symtab == NULL)
		b->source_file = NULL;
	      else
		b->source_file =
		  savestring (sals.sals[i].symtab->filename,
			      strlen (sals.sals[i].symtab->filename));
	      b->line_number = sals.sals[i].line;
	      b->loc->requested_address = sals.sals[i].pc;
	      b->loc->address
	        = adjust_breakpoint_address (b->loc->requested_address,
		                             b->type);

	      /* Used to check for duplicates here, but that can
	         cause trouble, as it doesn't check for disabled
	         breakpoints. */

	      /* APPLE LOCAL begin modify breakpoint hook */
	      /* Run the modify breakpoint hook because we turn off
		 all the mention stuff with the dont_mention flag, but
		 we do want to tell the ui that an address might have
		 changed here.  */
	      if (deprecated_modify_breakpoint_hook)
		deprecated_modify_breakpoint_hook (b);
	      /* APPLE LOCAL end modify breakpoint hook */
	      mention (b);

	      /* Might be better to do this just once per breakpoint_re_set,
	         rather than once for every breakpoint.  */
	      breakpoints_changed ();
	    }
	  b->loc->section = sals.sals[i].section;
	  b->enable_state = save_enable;	/* Restore it, this worked. */

	  /* APPLE LOCAL begin breakpoints */
	  /* Unset the selected objfile, and record the
	     bp_objfile.  */

	  do_cleanups (restrict_cleanup);

	  /* Mark the breakpoint as set so we don't try to reset it.  */
	  b->bp_set_state = bp_state_set;
	  /* APPLE LOCAL end breakpoints */

	  /* Now that this is re-enabled, check_duplicates
	     can be used. */
	  check_duplicates (b);

	}
      xfree (sals.sals);
      break;

    case bp_watchpoint:
    case bp_hardware_watchpoint:
    case bp_read_watchpoint:
    case bp_access_watchpoint:
      innermost_block = NULL;
      /* The issue arises of what context to evaluate this in.  The
         same one as when it was set, but what does that mean when
         symbols have been re-read?  We could save the filename and
         functionname, but if the context is more local than that, the
         best we could do would be something like how many levels deep
         and which index at that particular level, but that's going to
         be less stable than filenames or function names.  */

      /* So for now, just use a global context.  */

      /* APPLE LOCAL begin delete global watchpoints */
      /* If symbols have changed so a saved global watchpoint no
         longer applies, delete it, lest gdb crash ungloriously.
         Also, call gdb_parse_exp_1 so we can trap the error rather
         than just bagging out.  */
      s = b->exp_string;
      if (! gdb_parse_exp_1 (&s, innermost_block, 0, &s_exp))
        {
          warning ("Unable to reset watchpoint %d (unable to "
                   "parse expression); deleting", b->number);
          delete_breakpoint (b);
          return 0;
        }
      if (b->exp)
        xfree (b->exp);
      b->exp = s_exp;
      /* APPLE LOCAL end delete global watchpoints */

      b->exp_valid_block = innermost_block;
      mark = value_mark ();
      if (b->val)
	{
	  value_free (b->val);
	  /* Avoid re-freeing b->val if an error during the call to
             evaluate_expression.  */
	  b->val = NULL;
	}
      b->val = evaluate_expression (b->exp);
      release_value (b->val);
      if (value_lazy (b->val) && breakpoint_enabled (b))
	value_fetch_lazy (b->val);

      if (b->cond_string != NULL)
	{
	  s = b->cond_string;
	  if (b->cond)
	    {
	      xfree (b->cond);
	      /* Avoid re-freeing b->exp if an error during the call
		 to parse_exp_1.  */
	      b->cond = NULL;
	    }
	  b->cond = parse_exp_1 (&s, (struct block *) 0, 0);
	}
      if (breakpoint_enabled (b))
	mention (b);
      value_free_to_mark (mark);
      break;
    case bp_catch_catch:
    case bp_catch_throw:
      break;
      /* We needn't really do anything to reset these, since the mask
         that requests them is unaffected by e.g., new libraries being
         loaded. */
    case bp_catch_fork:
    case bp_catch_vfork:
    case bp_catch_exec:
      break;

    default:
      printf_filtered (_("Deleting unknown breakpoint type %d\n"), b->type);
      /* fall through */
      /* Delete longjmp and overlay event breakpoints; they will be
         reset later by breakpoint_re_set.  */
    case bp_longjmp:
    case bp_longjmp_resume:
    case bp_overlay_event:
      delete_breakpoint (b);
      break;

      /* This breakpoint is special, it's set up when the inferior
         starts and we really don't want to touch it.  */
    case bp_shlib_event:

      /* Keep temporary breakpoints, which can be encountered when we step
         over a dlopen call and SOLIB_ADD is resetting the breakpoints.
         Otherwise these should have been blown away via the cleanup chain
         or by breakpoint_init_inferior when we rerun the executable.  */
    case bp_until:
    case bp_finish:
    case bp_watchpoint_scope:
    case bp_call_dummy:
    case bp_step_resume:
      break;
    }

  return 0;
}

/* APPLE LOCAL: restrict_search_cleanup is used to clean up
   after restricting the breakpoint update search to only the
   newly added libraries.  */

static void
restrict_search_cleanup (void *ignored)
{
  objfile_restrict_search (0);
  objfile_clear_restrict_list ();
}

/* APPLE LOCAL: Call breakpoint_update to sync breakpoints with the
   new symbol environment.  */

void 
breakpoint_update ()
{
  if (breakpoint_generation != symbol_generation) 
    {
      
      struct cleanup *old_cleanups;
      
      old_cleanups = make_cleanup (restrict_search_cleanup, NULL);
      
      breakpoint_re_set_all ();
      if (exception_catchpoints_enabled (EX_EVENT_THROW))
	{
	  objfile_restrict_search (1);
	  gnu_v3_update_exception_catchpoints (EX_EVENT_THROW, 0, NULL);
	}
      if (exception_catchpoints_enabled (EX_EVENT_CATCH))
	{
	  objfile_restrict_search (1);
	  gnu_v3_update_exception_catchpoints (EX_EVENT_CATCH, 0, NULL);
	}
      
      do_cleanups (old_cleanups);
            
      breakpoint_generation = symbol_generation;
    } 
  else
    {
      /* Always look for any breakpoints that are waiting for their libraries
	 to reload.  */
      struct breakpoint *b, *temp;

      ALL_BREAKPOINTS_SAFE (b, temp)
      {
	struct cleanup *restrict_cleanups;
	/* Format possible error msg */
	char *message;

	if (b->bp_set_state != bp_state_waiting_load
	    || b->bp_objfile == NULL)
	  continue;

	/* Note, this isn't strictly necessary, since breakpoint_re_set_one
	   will make the same check.  But since we know we are waiting till
	   this breakpoint's objfile gets reloaded into the target, then 
	   there's no reason to go through all the work done in that function
	   if it hasn't gotten reloaded.  */
	if (!(target_check_is_objfile_loaded (b->bp_objfile)
	      || (b->loc->address != 0x0 && b->bp_objfile == NULL)))
	  continue;

	message = xstrprintf ("Error in re-setting breakpoint %d:\n",
			      b->number);
	restrict_cleanups = make_cleanup_restrict_to_objfile (b->bp_objfile);
	make_cleanup (xfree, message);
	catch_errors (breakpoint_re_set_one, b, message, RETURN_MASK_ALL);
	do_cleanups (restrict_cleanups);

      }
    }
}

/* APPLE LOCAL: breakpoint_re_set gets called fairly often, and it is
   too expensive to scan all the shared libraries resetting breakpoints
   as often as it is called.

   So we use breakpoint_re_set to just tell the breakpoint system that
   the executable or shlibs have changed, and breakpoints need to be
   reset.  If OBJFILE is not NULL, then only the OBJFILE has changed,
   so put it on the restricted search list.  If OBJFILE is -1, then
   clear the restricted search list, and remind ourselves not to
   restrict the search this time around. 

   Then the actual code that was in breakpoint_re_set is moved to a
   new function, breakpoint_re_set_all, which gets called from
   breakpoint_update.  Then we have to put in calls to breakpoint_update
   at the points where we really want to sync up the breakpoints.  */

void
breakpoint_re_set (struct objfile *objfile)
{
  if (objfile != NULL)
    {
      objfile_add_to_restrict_list (objfile);
    }
  symbol_generation++;
}

/* Re-set all breakpoints after symbols have been re-loaded.  */

static void
breakpoint_re_set_all (void)
{
  struct breakpoint *b, *temp;
  enum language save_language;
  int save_input_radix;

  save_language = current_language->la_language;
  save_input_radix = input_radix;
  ALL_BREAKPOINTS_SAFE (b, temp)
  {
    /* Format possible error msg */
    char *message = xstrprintf ("Error in re-setting breakpoint %d:\n",
				b->number);
    struct cleanup *cleanups;

    /* APPLE LOCAL: we are going to unconditionally remake the longjmp
       breakpoint for every shared library load.  At least delete the
       extant ones before remaking them...  */

    if (b->type == bp_longjmp_resume || b->type == bp_longjmp)
      {
	delete_breakpoint (b);
	continue;
      }

    /* APPLE LOCAL: We are going to trust the bp_set_state flag here
       instead of just blindly resetting the breakpoints every time.
       This means that the shlib code needs to track this correctly,
       however.  I am not checking breakpoint type here, presumably
       if some type didn't want this behavior, it wouldn't set the flag.  */
    if (b->bp_set_state == bp_state_set)
      continue;
    /* END APPLE LOCAL */

    cleanups = make_cleanup (xfree, message);

    /* APPLE LOCAL: All breakpoint setting respects the objfile_list.  */
    /* APPLE LOCAL begin subroutine inlining  */
    if (b->type == bp_breakpoint || b->type == bp_inlined_breakpoint)
      objfile_restrict_search (1);
    /* APPLE LOCAL end subroutine inlining  */
    /* END APPLE LOCAL */
    catch_errors (breakpoint_re_set_one, b, message, RETURN_MASK_ALL);
    /* APPLE LOCAL: So we have to clear the search here...  */
    objfile_restrict_search (0);
    /* END APPLE LOCAL */
    do_cleanups (cleanups);
  }
  set_language (save_language);
  input_radix = save_input_radix;

  if (GET_LONGJMP_TARGET_P ())
    {
      create_longjmp_breakpoint ("longjmp");
      create_longjmp_breakpoint ("_longjmp");
      create_longjmp_breakpoint ("siglongjmp");
      create_longjmp_breakpoint ("_siglongjmp");
      create_longjmp_breakpoint (NULL);
    }

  /* APPLE LOCAL begin don't create overlay bp */
  /* The overlay event breakpoint should really be a target vector
     function, since this is very platform specific (and not relevant
     on X).  Someday if we are motivated, we will do this right, for
     now, just disable it. */
  /* APPLE LOCAL end don't create overlay bp */
}

/* Reset the thread number of this breakpoint:

   - If the breakpoint is for all threads, leave it as-is.
   - Else, reset it to the current thread for inferior_ptid. */
void
breakpoint_re_set_thread (struct breakpoint *b)
{
  if (b->thread != -1)
    {
      if (in_thread_list (inferior_ptid))
	b->thread = pid_to_thread_id (inferior_ptid);
    }
}

/* Set ignore-count of breakpoint number BPTNUM to COUNT.
   If from_tty is nonzero, it prints a message to that effect,
   which ends with a period (no newline).  */

void
set_ignore_count (int bptnum, int count, int from_tty)
{
  struct breakpoint *b;

  if (count < 0)
    count = 0;

  ALL_BREAKPOINTS (b)
    if (b->number == bptnum)
    {
      b->ignore_count = count;
      if (from_tty)
	{
	  if (count == 0)
	    printf_filtered (_("Will stop next time breakpoint %d is reached."),
			     bptnum);
	  else if (count == 1)
	    printf_filtered (_("Will ignore next crossing of breakpoint %d."),
			     bptnum);
	  else
	    printf_filtered (_("Will ignore next %d crossings of breakpoint %d."),
			     count, bptnum);
	}
      breakpoints_changed ();
      breakpoint_modify_event (b->number);
      return;
    }

  error (_("No breakpoint number %d."), bptnum);
}

/* Clear the ignore counts of all breakpoints.  */
void
breakpoint_clear_ignore_counts (void)
{
  struct breakpoint *b;

  ALL_BREAKPOINTS (b)
    b->ignore_count = 0;
}

/* Command to set ignore-count of breakpoint N to COUNT.  */

static void
ignore_command (char *args, int from_tty)
{
  char *p = args;
  int num;

  if (p == 0)
    error_no_arg (_("a breakpoint number"));

  num = get_number (&p);
  if (num == 0)
    error (_("bad breakpoint number: '%s'"), args);
  if (*p == 0)
    error (_("Second argument (specified ignore-count) is missing."));

  set_ignore_count (num,
		    longest_to_int (value_as_long (parse_and_eval (p))),
		    from_tty);
  if (from_tty)
    printf_filtered ("\n");
}

/* Call FUNCTION on each of the breakpoints
   whose numbers are given in ARGS.  */

static void
map_breakpoint_numbers (char *args, void (*function) (struct breakpoint *))
{
  char *p = args;
  char *p1;
  int num;
  struct breakpoint *b, *tmp;
  int match;

  if (p == 0)
    error_no_arg (_("one or more breakpoint numbers"));

  while (*p)
    {
      match = 0;
      p1 = p;

      num = get_number_or_range (&p1);
      if (num == 0)
	{
	  warning (_("bad breakpoint number at or near '%s'"), p);
	}
      else
	{
	  ALL_BREAKPOINTS_SAFE (b, tmp)
	    if (b->number == num)
	      {
		struct breakpoint *related_breakpoint = b->related_breakpoint;
		match = 1;
		function (b);
		if (related_breakpoint)
		  function (related_breakpoint);
		break;
	      }
	  if (match == 0)
	    printf_unfiltered (_("No breakpoint number %d.\n"), num);
	}
      p = p1;
    }
}

/* Set ignore-count of breakpoint number BPTNUM to COUNT.
   If from_tty is nonzero, it prints a message to that effect,
   which ends with a period (no newline).  */

void
disable_breakpoint (struct breakpoint *bpt)
{
  /* Never disable a watchpoint scope breakpoint; we want to
     hit them when we leave scope so we can delete both the
     watchpoint and its scope breakpoint at that time.  */
  if (bpt->type == bp_watchpoint_scope)
    return;

  /* You can't disable permanent breakpoints.  */
  if (bpt->enable_state == bp_permanent)
    return;

  bpt->enable_state = bp_disabled;

  /* APPLE LOCAL: Mark disabled breakpoints as unset, since we don't
     track changes in libraries for disabled breakpoints. */
  bpt->bp_set_state = bp_state_unset;
  /* END APPLE LOCAL */

  check_duplicates (bpt);

  if (deprecated_modify_breakpoint_hook)
    deprecated_modify_breakpoint_hook (bpt);
  breakpoint_modify_event (bpt->number);
}

static void
disable_command (char *args, int from_tty)
{
  struct breakpoint *bpt;
  if (args == 0)
    ALL_BREAKPOINTS (bpt)
      switch (bpt->type)
      {
      case bp_none:
	warning (_("attempted to disable apparently deleted breakpoint #%d?"),
		 bpt->number);
	continue;
      case bp_breakpoint:
      /* APPLE LOCAL begin subroutine inlining  */
      case bp_inlined_breakpoint:
      /* APPLE LOCAL end subroutine inlining  */
      case bp_catch_load:
      case bp_catch_unload:
      case bp_catch_fork:
      case bp_catch_vfork:
      case bp_catch_exec:
      case bp_catch_catch:
      case bp_catch_throw:
      /* APPLE LOCAL: Handle gnu_v3_catch types.  */
      case bp_gnu_v3_catch_catch:
      case bp_gnu_v3_catch_throw:
      case bp_hardware_breakpoint:
      case bp_watchpoint:
      case bp_hardware_watchpoint:
      case bp_read_watchpoint:
      case bp_access_watchpoint:
	disable_breakpoint (bpt);
      default:
	continue;
      }
  else
    map_breakpoint_numbers (args, disable_breakpoint);
}

static void
do_enable_breakpoint (struct breakpoint *bpt, enum bpdisp disposition)
{
  int target_resources_ok, other_type_used;
  struct value *mark;

  if (bpt->type == bp_hardware_breakpoint)
    {
      int i;
      i = hw_breakpoint_used_count ();
      target_resources_ok = 
	TARGET_CAN_USE_HARDWARE_WATCHPOINT (bp_hardware_breakpoint, 
					    i + 1, 0);
      if (target_resources_ok == 0)
	error (_("No hardware breakpoint support in the target."));
      else if (target_resources_ok < 0)
	error (_("Hardware breakpoints used exceeds limit."));
    }

  if (bpt->pending)
    {
      if (bpt->enable_state != bp_enabled)
	{
	  /* When enabling a pending breakpoint, we need to check if the breakpoint
	     is resolvable since shared libraries could have been loaded
	     after the breakpoint was disabled.  */
	  breakpoints_changed ();
 	  if (resolve_pending_breakpoint (bpt) == GDB_RC_OK)
	    {
	      /* APPLE LOCAL don't delete breakpoint */
	      return;
	    }
	  bpt->enable_state = bp_enabled;
	  bpt->disposition = disposition;
	}
    }
  else  /* Not a pending breakpoint.  */
    {
      if (bpt->enable_state != bp_permanent)
	bpt->enable_state = bp_enabled;

      /* APPLE LOCAL: We mark breakpoints as unset in disable so we need
	 to mark it as reset here.  */
      bpt->bp_set_state = bp_state_set;

      bpt->disposition = disposition;
      check_duplicates (bpt);
      breakpoints_changed ();
      
      if (bpt->type == bp_watchpoint || 
	  bpt->type == bp_hardware_watchpoint ||
	  bpt->type == bp_read_watchpoint || 
	  bpt->type == bp_access_watchpoint)
	{
	  struct frame_id saved_frame_id;

	  saved_frame_id = get_frame_id (get_selected_frame (NULL));
	  if (bpt->exp_valid_block != NULL)
	    {
	      struct frame_info *fr =
		fr = frame_find_by_id (bpt->watchpoint_frame);
	      if (fr == NULL)
		{
		  printf_filtered (_("\
Cannot enable watchpoint %d because the block in which its expression\n\
is valid is not currently in scope.\n"), bpt->number);
		  bpt->enable_state = bp_disabled;
		  return;
		}
	      select_frame (fr);
	    }
	  
	  value_free (bpt->val);
	  mark = value_mark ();
	  bpt->val = evaluate_expression (bpt->exp);
	  release_value (bpt->val);
	  if (value_lazy (bpt->val))
	    value_fetch_lazy (bpt->val);
	  
	  if (bpt->type == bp_hardware_watchpoint ||
	      bpt->type == bp_read_watchpoint ||
	      bpt->type == bp_access_watchpoint)
	    {
	      int i = hw_watchpoint_used_count (bpt->type, &other_type_used);
	      int mem_cnt = can_use_hardware_watchpoint (bpt->val);

	      /* APPLE LOCAL don't dummy-use locals */
	      target_resources_ok = TARGET_CAN_USE_HARDWARE_WATCHPOINT (
									bpt->type, i + mem_cnt, other_type_used);
	      /* we can consider of type is bp_hardware_watchpoint, convert to 
		 bp_watchpoint in the following condition */
	      if (target_resources_ok < 0)
		{
		  printf_filtered (_("\
Cannot enable watchpoint %d because target watch resources\n\
have been allocated for other watchpoints.\n"), bpt->number);
		  bpt->enable_state = bp_disabled;
		  value_free_to_mark (mark);
		  return;
		}
	    }
	  
	  select_frame (frame_find_by_id (saved_frame_id));
	  value_free_to_mark (mark);
	}
    }

  if (deprecated_modify_breakpoint_hook)
    deprecated_modify_breakpoint_hook (bpt);
  breakpoint_modify_event (bpt->number);
}

void
enable_breakpoint (struct breakpoint *bpt)
{
  do_enable_breakpoint (bpt, bpt->disposition);
}

/* The enable command enables the specified breakpoints (or all defined
   breakpoints) so they once again become (or continue to be) effective
   in stopping the inferior.  */

static void
enable_command (char *args, int from_tty)
{
  struct breakpoint *bpt;
  struct breakpoint *tmp;

  if (args == 0)
    /* APPLE LOCAL: Use ALL_BREAKPOINTS_SAFE since enable_breakpoint
       can add breakpoints to the breakpoint chain.  */
    ALL_BREAKPOINTS_SAFE (bpt, tmp)
      switch (bpt->type)
      {
      case bp_none:
	warning (_("attempted to enable apparently deleted breakpoint #%d?"),
		 bpt->number);
	continue;
      case bp_breakpoint:
      /* APPLE LOCAL begin subroutine inlining  */
      case bp_inlined_breakpoint:
      /* APPLE LOCAL end subroutine inlining  */
      case bp_catch_load:
      case bp_catch_unload:
      case bp_catch_fork:
      case bp_catch_vfork:
      case bp_catch_exec:
      /* APPLE LOCAL: Handle gnu_v3_catch types.  */
      case bp_gnu_v3_catch_catch:
      case bp_gnu_v3_catch_throw:
      case bp_catch_catch:
      case bp_catch_throw:
      case bp_hardware_breakpoint:
      case bp_watchpoint:
      case bp_hardware_watchpoint:
      case bp_read_watchpoint:
      case bp_access_watchpoint:
	enable_breakpoint (bpt);
      default:
	continue;
      }
  else
    map_breakpoint_numbers (args, enable_breakpoint);
}

static void
enable_once_breakpoint (struct breakpoint *bpt)
{
  do_enable_breakpoint (bpt, disp_disable);
}

static void
enable_once_command (char *args, int from_tty)
{
  map_breakpoint_numbers (args, enable_once_breakpoint);
}

static void
enable_delete_breakpoint (struct breakpoint *bpt)
{
  do_enable_breakpoint (bpt, disp_del);
}

static void
enable_delete_command (char *args, int from_tty)
{
  map_breakpoint_numbers (args, enable_delete_breakpoint);
}

static void
set_breakpoint_cmd (char *args, int from_tty)
{
}

static void
show_breakpoint_cmd (char *args, int from_tty)
{
}

/* APPLE LOCAL begin save-breakpoints command */
/* Generate a break, watch, or catch command defined by B to the STREAM.

   General worse case example,

     break <address> thread <t> if <expr>
     commands
       <command lines...>
     end
     ignore $bpnum <count>
     disable $bpnum

   In addition "set input-radix <r>" may precede the above sequence but
   that is generated by save_breakpoints_command() which is this
   function's caller.  */

static void
write_one_breakpoint (struct breakpoint *b, struct ui_file *stream, struct ui_out *uiout)
{
  register struct command_line *l;

  switch (b->type)
    {
    case bp_watchpoint:
    case bp_hardware_watchpoint:
    case bp_read_watchpoint:
    case bp_access_watchpoint:
      switch (b->original_flags) 
	{
	  case hw_read:
	      fprintf_unfiltered (stream, "rwatch %s", b->exp_string);
	        break;
		case hw_access:
		    fprintf_unfiltered (stream, "awatch %s", b->exp_string);
		      break;
		      case hw_execute:
			  internal_error (__FILE__, __LINE__, 
					  "execute watchpoints unsupported");
			    break;
			    case hw_write:
			      default:
				  fprintf_unfiltered (stream, 
						      "watch %s", b->exp_string);
				    break;
				    }
      break;
      
    case bp_catch_load:
    case bp_catch_unload:
      fprintf_unfiltered (stream, "%scatch %sload", 
			  b->disposition == disp_del ? "t" : "",
			  b->type == bp_catch_unload ? "un" : "");
      if (b->dll_pathname != NULL)
        fputs_unfiltered (b->dll_pathname, stream);
      break;

    case bp_catch_fork:
      fprintf_unfiltered (stream, "%scatch fork", 
			  b->disposition == disp_del ? "t" : "");
      break;

    case bp_catch_vfork:
      fprintf_unfiltered (stream, "%scatch vfork", 
			  b->disposition == disp_del ? "t" : "");
      break;

    case bp_catch_exec:
      fprintf_unfiltered (stream, "%scatch exec", 
			  b->disposition == disp_del ? "t" : "");
      break;

    case bp_catch_catch:
    case bp_gnu_v3_catch_catch:
      fprintf_unfiltered (stream, "%scatch catch", 
			  b->disposition == disp_del ? "t" : "");
      break;

    case bp_catch_throw:
    case bp_gnu_v3_catch_throw:
      fprintf_unfiltered (stream, "%scatch throw", 
			  b->disposition == disp_del ? "t" : "");
      break;

    case bp_breakpoint:
    /* APPLE LOCAL begin subroutine inlining  */
    case bp_inlined_breakpoint:
    /* APPLE LOCAL end subroutine inlining  */
    case bp_hardware_breakpoint:
      {
	char *hardwareflag, *futureflag, *tempflag;
	
	hardwareflag = (b->type == bp_hardware_breakpoint) ? "h" : "";
	futureflag = ((b->enable_state == bp_shlib_disabled) ||
		      (b->original_flags & BP_FUTUREFLAG)) ? "future-" : "";
	tempflag = (b->disposition == disp_del) ? "t" : "";

        fprintf_unfiltered (stream, "%s%s%sbreak", futureflag, tempflag, hardwareflag);
      
	if (b->addr_string)
	  {
	    int len = strlen (b->addr_string) - 1;
	    if (b->addr_string[len] == ' ')
	      b->addr_string[len] = 0;
	    else
	      len = 0;
	    fprintf_unfiltered (stream, " %s", b->addr_string);
	    if (len)
	      b->addr_string[len] = ' ';
	  }
	else if (b->source_file)
          fprintf_unfiltered (stream, " %s:%d", b->source_file, b->line_number);
	else
	  fprintf_unfiltered (stream, " %s",
			      hex_string_custom ((unsigned long) b->loc->address, 8));
      }
      break;

    default:
      internal_error (__FILE__, __LINE__, "unhandled switch case");
      break;
    }

  if (b->thread != -1)
    fprintf_unfiltered (stream, " thread %d", b->thread);

  if (b->cond_string)
    fprintf_unfiltered (stream, " if %s", b->cond_string);
  
  fputc_unfiltered ('\n', stream);

  if ((l = b->commands))
    {
      fputs_unfiltered ("commands\n", stream);
      print_command_lines (uiout, l, 4);
      fputs_unfiltered ("end\n", stream);
    }

  if (b->ignore_count)
    fprintf_unfiltered (stream, "ignore $bpnum %d\n", b->ignore_count);
    
  if (b->enable_state == bp_disabled)
      fputs_unfiltered ("disable $bpnum\n", stream);
}

static void 
save_breakpoints_command (char *arg, int from_tty)
{
  struct cleanup *cleanups;
  register struct breakpoint *b;
  int found_a_breakpoint = 0; 
  int current_radix = -1; 
  int skip; 
  struct ui_file *stream = NULL; 
  struct ui_out *uiout = NULL; 
  time_t t;
  char **argv; 
  char *pathname, buf[256];

  dont_repeat (); 
       
  if (arg == NULL) 
    error ("Arguments missing: file name in which to save breakpoint commands"); 
  else if ((argv = buildargv (arg)) == NULL) 
    nomem (0); 

  make_cleanup_freeargv (argv); 
  
  pathname = tilde_expand (*argv); 
  cleanups = make_cleanup (xfree, pathname); 

  ALL_BREAKPOINTS (b)
    {
      /* Filter out non-user breakpoints. */
      if (b->type != bp_breakpoint
          /* APPLE LOCAL begin subroutine inlining  */
          && b->type != bp_inlined_breakpoint
          /* APPLE LOCAL end subroutine inlining  */
          && b->type != bp_catch_load
          && b->type != bp_catch_unload
          && b->type != bp_catch_fork
          && b->type != bp_catch_vfork
          && b->type != bp_catch_exec
      /* APPLE LOCAL: Handle gnu_v3_catch types.  */
          && b->type != bp_gnu_v3_catch_catch
          && b->type != bp_gnu_v3_catch_throw
          && b->type != bp_catch_catch
          && b->type != bp_catch_throw
          && b->type != bp_hardware_breakpoint
          && b->type != bp_watchpoint
          && b->type != bp_read_watchpoint
          && b->type != bp_access_watchpoint
          && b->type != bp_hardware_watchpoint)
        continue;

      if (! found_a_breakpoint++)
        {
	    stream = gdb_fopen (pathname, FOPEN_WT);
          if (stream == NULL) 
            error ("Unable to open file '%s' for saving breakpoints (%s)",
		      arg, strerror (errno));
	    make_cleanup_ui_file_delete (stream); 
	      uiout = cli_out_new (stream); 
	        if (uiout == NULL) 
		      error ("Unable to create cli_out from file for saving breakpoints"); 
		  make_cleanup_ui_out_delete (uiout); 

          if (time (&t) != -1)
            {
              char *l = setlocale (LC_ALL, NULL);
              if (l)
                {
                  char *orig_locale = strcpy (xmalloc (strlen (l) + 1), l);
                  setlocale (LC_ALL, "");
                  if (strftime (buf, sizeof(buf), "%a %b %e %H:%M:%S %Z %Y", localtime (&t)))
                    fprintf_unfiltered (stream, "# Saved breakpoints file created on %s\n\n", buf);
                  setlocale (LC_ALL, orig_locale);
                }
            }
          fprintf_unfiltered (stream, "set $current_radix = $input_radix\n"
			            "set input-radix 012\n\n");
          current_radix = 10;
        }

      skip = (b->commands || b->ignore_count || b->enable_state == bp_disabled);
      if (skip)
        fputc_unfiltered ('\n', stream);

      if (b->input_radix != current_radix)
        {
          current_radix = b->input_radix;
          fprintf_unfiltered (stream, "set input-radix 0%o\n", current_radix);
        }

      write_one_breakpoint (b, stream, uiout);

      if (skip && b->next)
        fputc_unfiltered ('\n', stream);
    }

  if (! found_a_breakpoint)
    printf_filtered ("No breakpoints or watchpoints to save.\n");
  else
    {
      fputs_unfiltered ("\n", stream);
      if (current_radix != 10)
        fputs_unfiltered ("set input-radix 012\n", stream);
      fputs_unfiltered ("set input-radix $current_radix\n", stream);
      if (from_tty)
        printf_filtered ("Breakpoints saved to file '%s'.\n", pathname);
    }

  do_cleanups (cleanups);
}
/* APPLE LOCAL end save-breakpoints command */

/* Use default_breakpoint_'s, or nothing if they aren't valid.  */

struct symtabs_and_lines
decode_line_spec_1 (char *string, int funfirstline)
{
  struct symtabs_and_lines sals;
  if (string == 0)
    error (_("Empty line specification."));
  if (default_breakpoint_valid)
    /* APPLE LOCAL begin return multiple symbols */
    sals = decode_line_1 (&string, funfirstline,
			  default_breakpoint_symtab,
			  default_breakpoint_line,
			  (char ***) NULL, NULL, 0);
    /* APPLE LOCAL end return multiple symbols */
  else
    /* APPLE LOCAL begin return multiple symbols */
    sals = decode_line_1 (&string, funfirstline,
			  (struct symtab *) NULL, 0, (char ***) NULL, NULL, 0);
    /* APPLE LOCAL end return multiple symbols */
  if (*string)
    error (_("Junk at end of line specification: %s"), string);
  return sals;
}

/* APPLE LOCAL begin future-break command */
static void 
restore_saved_pending_break_support (void * val)
{
  pending_break_support = (enum auto_boolean) val; 
}

void
future_break_command (char *arg, int from_tty)
{
  struct cleanup *wipe = make_cleanup (restore_saved_pending_break_support, 
                                       (void *) pending_break_support);
  pending_break_support = AUTO_BOOLEAN_TRUE;
  break_command_1 (arg, 0, from_tty, NULL);
  do_cleanups (wipe);
}

/* APPLE LOCAL: If there is a finish breakpoint set up, return it. 
 Need this so that if you hit the solib event breakpoint and actually
 stop when there is a finish operation in progress, we will still 
 stop correctly.  */

struct breakpoint *
find_finish_breakpoint (void)
{
  struct breakpoint *b;
  
  ALL_BREAKPOINTS (b)
    
    if (b->type == bp_finish)
      return b;
  
  return NULL;
}

/* APPLE LOCAL: We are marking breakpoints set when they are
   successfully set now, with the bp_set_state flag, so we need to unset
   this flag whenever the objfile in which it is set changes.  Do this
   by calling this function with the changed objfile as argument. */

void
tell_breakpoints_objfile_changed_internal (struct objfile *objfile,
					   int set_pending)
{
  struct breakpoint *b, *tmp;

  if (objfile != NULL)
    {
      ALL_BREAKPOINTS_SAFE (b, tmp)
	{
	  if (b->bp_objfile != NULL)
	    {
	      if (b->bp_objfile == objfile)
		{
		  b->bp_set_state = bp_state_unset;
                  if (set_pending)
                    b->pending = 1;
                  /* APPLE LOCAL begin radar 5273932  */
                  /* Save name of objfile in bp_objfile_name, so
                     that if we attempt to reset the breakpoint
                     (in breakpoint_re_set_one), because, for
                     example, we have slid addresses, we will
                     have a record as to where the breakpoint is supposed
                     to be set (namely, in the same objfile where we 
                     originally set it).  */
                  if (b->bp_objfile->name)
                    b->bp_objfile_name = xstrdup (b->bp_objfile->name);
                  /* APPLE LOCAL end radar 5273932  */
                  b->bp_objfile = NULL;
                }
            }
	  else if (b->bp_set_state != bp_state_unset)
	    {
	      struct obj_section *osect;
	      
	      ALL_OBJFILE_OSECTIONS (objfile, osect)
		{
		  if ((osect->addr < b->loc->address) 
		      && (b->loc->address < osect->endaddr))
		    {
		      b->bp_set_state = bp_state_unset;
		      break;
		    }
		}
	    }
	}
    }
  else
    {
      ALL_BREAKPOINTS (b)
	b->bp_set_state = bp_state_unset;
    }
  breakpoint_generation--;
}

/* This one sets the breakpoint as unset, but doesn't mark
   it as pending.  That way breakpoint_re_set_all will re-set
   it.  */

void 
tell_breakpoints_objfile_changed (struct objfile *objfile)
{
  tell_breakpoints_objfile_changed_internal (objfile, 0);
}

/* Use this one if the the shared library has been unloaded.  We mark it
   as pending again, so it will get reset when the shared library reappears.  */
void 
tell_breakpoints_objfile_removed (struct objfile *objfile)
{
  tell_breakpoints_objfile_changed_internal (objfile, 1);
}

/* APPLE LOCAL end future-break command */

void
_initialize_breakpoint (void)
{
  static struct cmd_list_element *breakpoint_set_cmdlist;
  static struct cmd_list_element *breakpoint_show_cmdlist;
  struct cmd_list_element *c;

  observer_attach_solib_unloaded (disable_breakpoints_in_unloaded_shlib);

  breakpoint_chain = 0;
  /* Don't bother to call set_breakpoint_count.  $bpnum isn't useful
     before a breakpoint is set.  */
  breakpoint_count = 0;

  add_com ("ignore", class_breakpoint, ignore_command, _("\
Set ignore-count of breakpoint number N to COUNT.\n\
Usage is `ignore N COUNT'."));
  if (xdb_commands)
    add_com_alias ("bc", "ignore", class_breakpoint, 1);

  add_com ("commands", class_breakpoint, commands_command, _("\
Set commands to be executed when a breakpoint is hit.\n\
Give breakpoint number as argument after \"commands\".\n\
With no argument, the targeted breakpoint is the last one set.\n\
The commands themselves follow starting on the next line.\n\
Type a line containing \"end\" to indicate the end of them.\n\
Give \"silent\" as the first line to make the breakpoint silent;\n\
then no output is printed when it is hit, except what the commands print."));

  add_com ("condition", class_breakpoint, condition_command, _("\
Specify breakpoint number N to break only if COND is true.\n\
Usage is `condition N COND', where N is an integer and COND is an\n\
expression to be evaluated whenever breakpoint N is reached."));

  c = add_com ("tbreak", class_breakpoint, tbreak_command, _("\
Set a temporary breakpoint.  Args like \"break\" command.\n\
Like \"break\" except the breakpoint is only temporary,\n\
so it will be deleted when hit.  Equivalent to \"break\" followed\n\
by using \"enable delete\" on the breakpoint number."));
  set_cmd_completer (c, location_completer);

  c = add_com ("hbreak", class_breakpoint, hbreak_command, _("\
Set a hardware assisted  breakpoint. Args like \"break\" command.\n\
Like \"break\" except the breakpoint requires hardware support,\n\
some target hardware may not have this support."));
  set_cmd_completer (c, location_completer);

  c = add_com ("thbreak", class_breakpoint, thbreak_command, _("\
Set a temporary hardware assisted breakpoint. Args like \"break\" command.\n\
Like \"hbreak\" except the breakpoint is only temporary,\n\
so it will be deleted when hit."));
  set_cmd_completer (c, location_completer);

  add_prefix_cmd ("enable", class_breakpoint, enable_command, _("\
Enable some breakpoints.\n\
Give breakpoint numbers (separated by spaces) as arguments.\n\
With no subcommand, breakpoints are enabled until you command otherwise.\n\
This is used to cancel the effect of the \"disable\" command.\n\
With a subcommand you can enable temporarily."),
		  &enablelist, "enable ", 1, &cmdlist);
  if (xdb_commands)
    add_com ("ab", class_breakpoint, enable_command, _("\
Enable some breakpoints.\n\
Give breakpoint numbers (separated by spaces) as arguments.\n\
With no subcommand, breakpoints are enabled until you command otherwise.\n\
This is used to cancel the effect of the \"disable\" command.\n\
With a subcommand you can enable temporarily."));

  add_com_alias ("en", "enable", class_breakpoint, 1);

  add_abbrev_prefix_cmd ("breakpoints", class_breakpoint, enable_command, _("\
Enable some breakpoints.\n\
Give breakpoint numbers (separated by spaces) as arguments.\n\
This is used to cancel the effect of the \"disable\" command.\n\
May be abbreviated to simply \"enable\".\n"),
		   &enablebreaklist, "enable breakpoints ", 1, &enablelist);

  add_cmd ("once", no_class, enable_once_command, _("\
Enable breakpoints for one hit.  Give breakpoint numbers.\n\
If a breakpoint is hit while enabled in this fashion, it becomes disabled."),
	   &enablebreaklist);

  add_cmd ("delete", no_class, enable_delete_command, _("\
Enable breakpoints and delete when hit.  Give breakpoint numbers.\n\
If a breakpoint is hit while enabled in this fashion, it is deleted."),
	   &enablebreaklist);

  add_cmd ("delete", no_class, enable_delete_command, _("\
Enable breakpoints and delete when hit.  Give breakpoint numbers.\n\
If a breakpoint is hit while enabled in this fashion, it is deleted."),
	   &enablelist);

  add_cmd ("once", no_class, enable_once_command, _("\
Enable breakpoints for one hit.  Give breakpoint numbers.\n\
If a breakpoint is hit while enabled in this fashion, it becomes disabled."),
	   &enablelist);

  add_prefix_cmd ("disable", class_breakpoint, disable_command, _("\
Disable some breakpoints.\n\
Arguments are breakpoint numbers with spaces in between.\n\
To disable all breakpoints, give no argument.\n\
A disabled breakpoint is not forgotten, but has no effect until reenabled."),
		  &disablelist, "disable ", 1, &cmdlist);
  add_com_alias ("dis", "disable", class_breakpoint, 1);
  add_com_alias ("disa", "disable", class_breakpoint, 1);
  if (xdb_commands)
    add_com ("sb", class_breakpoint, disable_command, _("\
Disable some breakpoints.\n\
Arguments are breakpoint numbers with spaces in between.\n\
To disable all breakpoints, give no argument.\n\
A disabled breakpoint is not forgotten, but has no effect until reenabled."));

  add_cmd ("breakpoints", class_alias, disable_command, _("\
Disable some breakpoints.\n\
Arguments are breakpoint numbers with spaces in between.\n\
To disable all breakpoints, give no argument.\n\
A disabled breakpoint is not forgotten, but has no effect until reenabled.\n\
This command may be abbreviated \"disable\"."),
	   &disablelist);

  add_prefix_cmd ("delete", class_breakpoint, delete_command, _("\
Delete some breakpoints or auto-display expressions.\n\
Arguments are breakpoint numbers with spaces in between.\n\
To delete all breakpoints, give no argument.\n\
\n\
Also a prefix command for deletion of other GDB objects.\n\
The \"unset\" command is also an alias for \"delete\"."),
		  &deletelist, "delete ", 1, &cmdlist);
  add_com_alias ("d", "delete", class_breakpoint, 1);
  if (xdb_commands)
    add_com ("db", class_breakpoint, delete_command, _("\
Delete some breakpoints.\n\
Arguments are breakpoint numbers with spaces in between.\n\
To delete all breakpoints, give no argument.\n"));

  add_cmd ("breakpoints", class_alias, delete_command, _("\
Delete some breakpoints or auto-display expressions.\n\
Arguments are breakpoint numbers with spaces in between.\n\
To delete all breakpoints, give no argument.\n\
This command may be abbreviated \"delete\"."),
	   &deletelist);

  add_com ("clear", class_breakpoint, clear_command, _("\
Clear breakpoint at specified line or function.\n\
Argument may be line number, function name, or \"*\" and an address.\n\
If line number is specified, all breakpoints in that line are cleared.\n\
If function is specified, breakpoints at beginning of function are cleared.\n\
If an address is specified, breakpoints at that address are cleared.\n\
\n\
With no argument, clears all breakpoints in the line that the selected frame\n\
is executing in.\n\
\n\
See also the \"delete\" command which clears breakpoints by number."));

  /* APPLE LOCAL add to breakpoint help */
  c = add_com ("break", class_breakpoint, break_command, _("\
Set breakpoint at specified line or function.\n\
Argument may be line number, function name, or \"*\" and an address.\n\
If line number is specified, break at start of code for that line.\n\
If function is specified, break at start of code for that function.\n\
If an address is specified, break at that exact address.\n\
With no arg, uses current execution address of selected stack frame.\n\
This is useful for breaking on return to a stack frame.\n\
\n\
Multiple breakpoints at one place are permitted, and useful if conditional.\n\
\n\
break ... if <cond> sets condition <cond> on the breakpoint as it is created.\n\
\n\
Do \"help breakpoints\" for info on other commands dealing with breakpoints."));
  set_cmd_completer (c, location_completer);

  add_com_alias ("b", "break", class_run, 1);
  add_com_alias ("br", "break", class_run, 1);
  add_com_alias ("bre", "break", class_run, 1);
  add_com_alias ("brea", "break", class_run, 1);

  /* APPLE LOCAL begin future-break command */
  add_com ("future-break", class_breakpoint, future_break_command,
      "Set breakpoint at expression.  If it can't be done now, attempt it\n"
           "again each time code is dynamically loaded.");
  add_com_alias ("fb", "future-break", class_breakpoint, 2);
  /* APPLE LOCAL end future-break command */

 if (xdb_commands)
    {
      add_com_alias ("ba", "break", class_breakpoint, 1);
      add_com_alias ("bu", "ubreak", class_breakpoint, 1);
    }

  if (dbx_commands)
    {
      add_abbrev_prefix_cmd ("stop", class_breakpoint, stop_command, _("\
Break in function/address or break at a line in the current file."),
			     &stoplist, "stop ", 1, &cmdlist);
      add_cmd ("in", class_breakpoint, stopin_command,
	       _("Break in function or address."), &stoplist);
      add_cmd ("at", class_breakpoint, stopat_command,
	       _("Break at a line in the current file."), &stoplist);
      add_com ("status", class_info, breakpoints_info, _("\
Status of user-settable breakpoints, or breakpoint number NUMBER.\n\
The \"Type\" column indicates one of:\n\
\tbreakpoint     - normal breakpoint\n\
\twatchpoint     - watchpoint\n\
The \"Disp\" column contains one of \"keep\", \"del\", or \"dis\" to indicate\n\
the disposition of the breakpoint after it gets hit.  \"dis\" means that the\n\
breakpoint will be disabled.  The \"Address\" and \"What\" columns indicate the\n\
address and file/line number respectively.\n\
\n\
Convenience variable \"$_\" and default examine address for \"x\"\n\
are set to the address of the last breakpoint listed.\n\n\
Convenience variable \"$bpnum\" contains the number of the last\n\
breakpoint set."));
    }

  add_info ("breakpoints", breakpoints_info, _("\
Status of user-settable breakpoints, or breakpoint number NUMBER.\n\
The \"Type\" column indicates one of:\n\
\tbreakpoint     - normal breakpoint\n\
\twatchpoint     - watchpoint\n\
The \"Disp\" column contains one of \"keep\", \"del\", or \"dis\" to indicate\n\
the disposition of the breakpoint after it gets hit.  \"dis\" means that the\n\
breakpoint will be disabled.  The \"Address\" and \"What\" columns indicate the\n\
address and file/line number respectively.\n\
\n\
Convenience variable \"$_\" and default examine address for \"x\"\n\
are set to the address of the last breakpoint listed.\n\n\
Convenience variable \"$bpnum\" contains the number of the last\n\
breakpoint set."));

  if (xdb_commands)
    add_com ("lb", class_breakpoint, breakpoints_info, _("\
Status of user-settable breakpoints, or breakpoint number NUMBER.\n\
The \"Type\" column indicates one of:\n\
\tbreakpoint     - normal breakpoint\n\
\twatchpoint     - watchpoint\n\
The \"Disp\" column contains one of \"keep\", \"del\", or \"dis\" to indicate\n\
the disposition of the breakpoint after it gets hit.  \"dis\" means that the\n\
breakpoint will be disabled.  The \"Address\" and \"What\" columns indicate the\n\
address and file/line number respectively.\n\
\n\
Convenience variable \"$_\" and default examine address for \"x\"\n\
are set to the address of the last breakpoint listed.\n\n\
Convenience variable \"$bpnum\" contains the number of the last\n\
breakpoint set."));

  add_cmd ("breakpoints", class_maintenance, maintenance_info_breakpoints, _("\
Status of all breakpoints, or breakpoint number NUMBER.\n\
The \"Type\" column indicates one of:\n\
\tbreakpoint     - normal breakpoint\n\
\twatchpoint     - watchpoint\n\
\tlongjmp        - internal breakpoint used to step through longjmp()\n\
\tlongjmp resume - internal breakpoint at the target of longjmp()\n\
\tuntil          - internal breakpoint used by the \"until\" command\n\
\tfinish         - internal breakpoint used by the \"finish\" command\n\
The \"Disp\" column contains one of \"keep\", \"del\", or \"dis\" to indicate\n\
the disposition of the breakpoint after it gets hit.  \"dis\" means that the\n\
breakpoint will be disabled.  The \"Address\" and \"What\" columns indicate the\n\
address and file/line number respectively.\n\
\n\
Convenience variable \"$_\" and default examine address for \"x\"\n\
are set to the address of the last breakpoint listed.\n\
\n\
Convenience variable \"$bpnum\" contains the number of the last\n\
breakpoint set."),
	   &maintenanceinfolist);

  add_com ("catch", class_breakpoint, catch_command, _("\
Set catchpoints to catch events.\n\
Raised signals may be caught:\n\
\tcatch signal              - all signals\n\
\tcatch signal <signame>    - a particular signal\n\
Raised exceptions may be caught:\n\
\tcatch throw               - all exceptions, when thrown\n\
\tcatch throw <exceptname>  - a particular exception, when thrown\n\
\tcatch catch               - all exceptions, when caught\n\
\tcatch catch <exceptname>  - a particular exception, when caught\n\
Thread or process events may be caught:\n\
\tcatch thread_start        - any threads, just after creation\n\
\tcatch thread_exit         - any threads, just before expiration\n\
\tcatch thread_join         - any threads, just after joins\n\
Process events may be caught:\n\
\tcatch start               - any processes, just after creation\n\
\tcatch exit                - any processes, just before expiration\n\
\tcatch fork                - calls to fork()\n\
\tcatch vfork               - calls to vfork()\n\
\tcatch exec                - calls to exec()\n\
Dynamically-linked library events may be caught:\n\
\tcatch load                - loads of any library\n\
\tcatch load <libname>      - loads of a particular library\n\
\tcatch unload              - unloads of any library\n\
\tcatch unload <libname>    - unloads of a particular library\n\
The act of your program's execution stopping may also be caught:\n\
\tcatch stop\n\n\
C++ exceptions may be caught:\n\
\tcatch throw               - all exceptions, when thrown\n\
\tcatch catch               - all exceptions, when caught\n\
\n\
Do \"help set follow-fork-mode\" for info on debugging your program\n\
after a fork or vfork is caught.\n\n\
Do \"help breakpoints\" for info on other commands dealing with breakpoints."));

  add_com ("tcatch", class_breakpoint, tcatch_command, _("\
Set temporary catchpoints to catch events.\n\
Args like \"catch\" command.\n\
Like \"catch\" except the catchpoint is only temporary,\n\
so it will be deleted when hit.  Equivalent to \"catch\" followed\n\
by using \"enable delete\" on the catchpoint number."));

  add_setshow_string_cmd ("exception-throw-type-regexp", 
                       class_breakpoint, 
                       &exception_throw_type_regexp, 
                       "Set throw regexp",
                       "Show throw regexp",
                       "Set a regexp to match against the exception type of a "
                       "thrown object.  If the regexp matches, then gdb will "
                       "stop at the throw of that object.",
                       NULL, NULL,
                       &setlist, &showlist);

  add_setshow_string_cmd ("exception-catch-type-regexp", class_breakpoint,
                       &exception_catch_type_regexp, 
                       "Set exception regexp",
                       "Show exception regexp",
                       "Set a regexp to match against the exception type of a "
                       "caught object.  If the regexp matches, then gdb will "
                       "stop at the catch of that object.",
                       NULL, NULL,
                       &setlist, &showlist);

  c = add_com ("watch", class_breakpoint, watch_command, _("\
Set a watchpoint for an expression.\n\
A watchpoint stops execution of your program whenever the value of\n\
an expression changes.\n\
If you pass the \"-location\" flag to the command, the expression\n\
is resolved to its location, and only that, not the full expression\n\
is watched."));
  set_cmd_completer (c, location_completer);

  c = add_com ("rwatch", class_breakpoint, rwatch_command, _("\
Set a read watchpoint for an expression.\n\
A watchpoint stops execution of your program whenever the value of\n\
an expression is read.\n\
If you pass the \"-location\" flag to the command, the expression\n\
is resolved to its location, and only that, not the full expression\n\
is watched."));
  set_cmd_completer (c, location_completer);

  c = add_com ("awatch", class_breakpoint, awatch_command, _("\
Set a watchpoint for an expression.\n\
A watchpoint stops execution of your program whenever the value of\n\
an expression is either read or written.\n\
If you pass the \"-location\" flag to the command, the expression\n\
is resolved to its location, and only that, not the full expression\n\
is watched."));
  set_cmd_completer (c, location_completer);

  add_info ("watchpoints", breakpoints_info,
	    _("Synonym for ``info breakpoints''."));


  /* XXX: cagney/2005-02-23: This should be a boolean, and should
     respond to changes - contrary to the description.  */
  add_setshow_zinteger_cmd ("can-use-hw-watchpoints", class_support,
			    &can_use_hw_watchpoints, _("\
Set debugger's willingness to use watchpoint hardware."), _("\
Show debugger's willingness to use watchpoint hardware."), _("\
If zero, gdb will not use hardware for new watchpoints, even if\n\
such is available.  (However, any hardware watchpoints that were\n\
created before setting this to nonzero, will continue to use watchpoint\n\
hardware.)"),
			    NULL,
			    show_can_use_hw_watchpoints,
			    &setlist, &showlist);

  can_use_hw_watchpoints = 1;

  /* APPLE LOCAL begin save-breakpoints command */
  add_setshow_zinteger_cmd ("show_breakpoint_hit_counts", class_support,
			    &show_breakpoint_hit_counts, _("\
Set if GDB should show breakpoint hit counts.\n\
This will affect the output of 'info debug'"), _("\
Set if GDB should show breakpoint hit counts.\n\
This will affect the output of 'info debug'"), NULL,
			    NULL, NULL,
			    &setlist, &showlist);

c = add_cmd ("save-breakpoints", class_breakpoint, save_breakpoints_command,
           "Save current breakpoint definitions as a script.\n\
Use the -command option or 'source' command in another debug\n\
'session to restore them.", &cmdlist);
  set_cmd_completer (c, filename_completer);
  
  add_com_alias ("save_breakpoints", "save-breakpoints", class_breakpoint, 1);
  add_com_alias ("save_bp", "save-breakpoints", class_breakpoint, 1);
  add_com_alias ("savebp", "save-breakpoints", class_breakpoint, 1);
  /* APPLE LOCAL end save-breakpoints command */

  add_prefix_cmd ("breakpoint", class_maintenance, set_breakpoint_cmd, _("\
Breakpoint specific settings\n\
Configure various breakpoint-specific variables such as\n\
pending breakpoint behavior"),
		  &breakpoint_set_cmdlist, "set breakpoint ",
		  0/*allow-unknown*/, &setlist);
  add_prefix_cmd ("breakpoint", class_maintenance, show_breakpoint_cmd, _("\
Breakpoint specific settings\n\
Configure various breakpoint-specific variables such as\n\
pending breakpoint behavior"),
		  &breakpoint_show_cmdlist, "show breakpoint ",
		  0/*allow-unknown*/, &showlist);

  add_setshow_auto_boolean_cmd ("pending", no_class,
				&pending_break_support, _("\
Set debugger's behavior regarding pending breakpoints."), _("\
Show debugger's behavior regarding pending breakpoints."), _("\
If on, an unrecognized breakpoint location will cause gdb to create a\n\
pending breakpoint.  If off, an unrecognized breakpoint location results in\n\
an error.  If auto, an unrecognized breakpoint location results in a\n\
user-query to see if a pending breakpoint should be created."),
				NULL,
				show_pending_break_support,
				&breakpoint_set_cmdlist,
				&breakpoint_show_cmdlist);

  pending_break_support = AUTO_BOOLEAN_AUTO;
}

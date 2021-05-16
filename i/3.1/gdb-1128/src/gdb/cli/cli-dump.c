/* Dump-to-file commands, for GDB, the GNU debugger.

   Copyright 2002, 2005 Free Software Foundation, Inc.

   Contributed by Red Hat.

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
#include "gdb_string.h"
#include "cli/cli-decode.h"
#include "cli/cli-cmds.h"
#include "value.h"
#include "completer.h"
#include "cli/cli-dump.h"
#include "gdb_assert.h"
#include <ctype.h>
#include "target.h"
#include "readline/readline.h"

#define XMALLOC(TYPE) ((TYPE*) xmalloc (sizeof (TYPE)))

/* APPLE LOCAL: segment binary file downloads
 * default the binary file chunk size to the max value.  */
#define DEFAULT_MAX_BINARY_FILE_CHUNK LONG_MAX
static long  g_max_binary_file_chunk = DEFAULT_MAX_BINARY_FILE_CHUNK;

char *
skip_spaces (char *chp)
{
  if (chp == NULL)
    return NULL;
  while (isspace (*chp))
    chp++;
  return chp;
}

char *
scan_expression_with_cleanup (char **cmd, const char *def)
{
  if ((*cmd) == NULL || (**cmd) == '\0')
    {
      char *exp = xstrdup (def);
      make_cleanup (xfree, exp);
      return exp;
    }
  else
    {
      char *exp;
      char *end;

      end = (*cmd) + strcspn (*cmd, " \t");
      exp = savestring ((*cmd), end - (*cmd));
      make_cleanup (xfree, exp);
      (*cmd) = skip_spaces (end);
      return exp;
    }
}


static void
do_fclose_cleanup (void *arg)
{
  fclose (arg);
}

static struct cleanup *
make_cleanup_fclose (FILE *file)
{
  return make_cleanup (do_fclose_cleanup, file);
}

char *
scan_filename_with_cleanup (char **cmd, const char *defname)
{
  char *filename;
  char *fullname;

  /* FIXME: Need to get the ``/a(ppend)'' flag from somewhere.  */

  /* File.  */
  if ((*cmd) == NULL)
    {
      if (defname == NULL)
	error (_("Missing filename."));
      filename = xstrdup (defname);
      make_cleanup (xfree, filename);
    }
  else
    {
      /* FIXME: should parse a possibly quoted string.  */
      char *end;

      (*cmd) = skip_spaces (*cmd);
      end = *cmd + strcspn (*cmd, " \t");
      filename = savestring ((*cmd), end - (*cmd));
      make_cleanup (xfree, filename);
      (*cmd) = skip_spaces (end);
    }
  gdb_assert (filename != NULL);

  fullname = tilde_expand (filename);
  make_cleanup (xfree, fullname);
  
  return fullname;
}

FILE *
fopen_with_cleanup (const char *filename, const char *mode)
{
  FILE *file = fopen (filename, mode);
  if (file == NULL)
    perror_with_name (filename);
  make_cleanup_fclose (file);
  return file;
}

static bfd *
bfd_openr_with_cleanup (const char *filename, const char *target)
{
  bfd *ibfd;

  ibfd = bfd_openr (filename, target);
  if (ibfd == NULL)
    error (_("Failed to open %s: %s."), filename, 
	   bfd_errmsg (bfd_get_error ()));

  make_cleanup_bfd_close (ibfd);
  if (!bfd_check_format (ibfd, bfd_object))
    error (_("'%s' is not a recognized file format."), filename);

  return ibfd;
}

static bfd *
bfd_openw_with_cleanup (const char *filename, const char *target,
			const char *mode)
{
  bfd *obfd;

  if (*mode == 'w')	/* Write: create new file */
    {
      obfd = bfd_openw (filename, target);
      if (obfd == NULL)
	error (_("Failed to open %s: %s."), filename, 
	       bfd_errmsg (bfd_get_error ()));
      make_cleanup_bfd_close (obfd);
      if (!bfd_set_format (obfd, bfd_object))
	error (_("bfd_openw_with_cleanup: %s."), bfd_errmsg (bfd_get_error ()));
    }
  else if (*mode == 'a')	/* Append to existing file */
    {	/* FIXME -- doesn't work... */
      error (_("bfd_openw does not work with append."));
    }
  else
    error (_("bfd_openw_with_cleanup: unknown mode %s."), mode);

  return obfd;
}

struct cmd_list_element *dump_cmdlist;
struct cmd_list_element *append_cmdlist;
struct cmd_list_element *srec_cmdlist;
struct cmd_list_element *ihex_cmdlist;
struct cmd_list_element *tekhex_cmdlist;
struct cmd_list_element *binary_dump_cmdlist;
struct cmd_list_element *binary_append_cmdlist;

static void
dump_command (char *cmd, int from_tty)
{
  printf_unfiltered (_("\"dump\" must be followed by a subcommand.\n\n"));
  help_list (dump_cmdlist, "dump ", -1, gdb_stdout);
}

static void
append_command (char *cmd, int from_tty)
{
  printf_unfiltered (_("\"append\" must be followed by a subcommand.\n\n"));
  help_list (dump_cmdlist, "append ", -1, gdb_stdout);
}

static void
dump_binary_file (const char *filename, const char *mode, 
		  const bfd_byte *buf, int len)
{
  FILE *file;
  int status;

  file = fopen_with_cleanup (filename, mode);
  status = fwrite (buf, len, 1, file);
  if (status != 1)
    perror_with_name (filename);
}

static void
dump_bfd_file (const char *filename, const char *mode, 
	       const char *target, CORE_ADDR vaddr, 
	       const bfd_byte *buf, int len)
{
  bfd *obfd;
  asection *osection;

  obfd = bfd_openw_with_cleanup (filename, target, mode);
  osection = bfd_make_section_anyway (obfd, ".newsec");
  bfd_set_section_size (obfd, osection, len);
  bfd_set_section_vma (obfd, osection, vaddr);
  bfd_set_section_alignment (obfd, osection, 0);
  bfd_set_section_flags (obfd, osection, (SEC_HAS_CONTENTS
					  | SEC_ALLOC
					  | SEC_LOAD));
  osection->entsize = 0;
  bfd_set_section_contents (obfd, osection, buf, 0, len);
}

static void
dump_memory_to_file (char *cmd, char *mode, char *file_format)
{
  struct cleanup *old_cleanups = make_cleanup (null_cleanup, NULL);
  CORE_ADDR lo;
  CORE_ADDR hi;
  ULONGEST count;
  char *filename;
  void *buf;
  char *lo_exp;
  char *hi_exp;

  /* Open the file.  */
  filename = scan_filename_with_cleanup (&cmd, NULL);

  /* Find the low address.  */
  if (cmd == NULL || *cmd == '\0')
    error (_("Missing start address."));
  lo_exp = scan_expression_with_cleanup (&cmd, NULL);

  /* Find the second address - rest of line.  */
  if (cmd == NULL || *cmd == '\0')
    error (_("Missing stop address."));
  hi_exp = cmd;

  lo = parse_and_eval_address (lo_exp);
  hi = parse_and_eval_address (hi_exp);
  if (hi <= lo)
    error (_("Invalid memory address range (start >= end)."));
  count = hi - lo;

  /* FIXME: Should use read_memory_partial() and a magic blocking
     value.  */
  buf = xmalloc (count);
  make_cleanup (xfree, buf);
  target_read_memory (lo, buf, count);
  
  /* Have everything.  Open/write the data.  */
  if (file_format == NULL || strcmp (file_format, "binary") == 0)
    {
      dump_binary_file (filename, mode, buf, count);
    }
  else
    {
      dump_bfd_file (filename, mode, file_format, lo, buf, count);
    }

  do_cleanups (old_cleanups);
}

static void
dump_memory_command (char *cmd, char *mode)
{
  dump_memory_to_file (cmd, mode, "binary");
}

static void
dump_value_to_file (char *cmd, char *mode, char *file_format)
{
  struct cleanup *old_cleanups = make_cleanup (null_cleanup, NULL);
  struct value *val;
  char *filename;

  /* Open the file.  */
  filename = scan_filename_with_cleanup (&cmd, NULL);

  /* Find the value.  */
  if (cmd == NULL || *cmd == '\0')
    error (_("No value to %s."), *mode == 'a' ? "append" : "dump");
  val = parse_and_eval (cmd);
  if (val == NULL)
    error (_("Invalid expression."));

  /* Have everything.  Open/write the data.  */
  if (file_format == NULL || strcmp (file_format, "binary") == 0)
    {
      dump_binary_file (filename, mode, value_contents (val), 
			TYPE_LENGTH (value_type (val)));
    }
  else
    {
      CORE_ADDR vaddr;

      if (VALUE_LVAL (val))
	{
	  vaddr = VALUE_ADDRESS (val);
	}
      else
	{
	  vaddr = 0;
	  warning (_("value is not an lval: address assumed to be zero"));
	}

      dump_bfd_file (filename, mode, file_format, vaddr, 
		     value_contents (val), 
		     TYPE_LENGTH (value_type (val)));
    }

  do_cleanups (old_cleanups);
}

static void
dump_value_command (char *cmd, char *mode)
{
  dump_value_to_file (cmd, mode, "binary");
}

static void
dump_srec_memory (char *args, int from_tty)
{
  dump_memory_to_file (args, FOPEN_WB, "srec");
}

static void
dump_srec_value (char *args, int from_tty)
{
  dump_value_to_file (args, FOPEN_WB, "srec");
}

static void
dump_ihex_memory (char *args, int from_tty)
{
  dump_memory_to_file (args, FOPEN_WB, "ihex");
}

static void
dump_ihex_value (char *args, int from_tty)
{
  dump_value_to_file (args, FOPEN_WB, "ihex");
}

static void
dump_tekhex_memory (char *args, int from_tty)
{
  dump_memory_to_file (args, FOPEN_WB, "tekhex");
}

static void
dump_tekhex_value (char *args, int from_tty)
{
  dump_value_to_file (args, FOPEN_WB, "tekhex");
}

static void
dump_binary_memory (char *args, int from_tty)
{
  dump_memory_to_file (args, FOPEN_WB, "binary");
}

static void
dump_binary_value (char *args, int from_tty)
{
  dump_value_to_file (args, FOPEN_WB, "binary");
}

static void
append_binary_memory (char *args, int from_tty)
{
  dump_memory_to_file (args, FOPEN_AB, "binary");
}

static void
append_binary_value (char *args, int from_tty)
{
  dump_value_to_file (args, FOPEN_AB, "binary");
}

struct dump_context
{
  void (*func) (char *cmd, char *mode);
  char *mode;
};

static void
call_dump_func (struct cmd_list_element *c, char *args, int from_tty)
{
  struct dump_context *d = get_cmd_context (c);
  d->func (args, d->mode);
}

void
add_dump_command (char *name, void (*func) (char *args, char *mode),
		  char *descr)

{
  struct cmd_list_element *c;
  struct dump_context *d;

  c = add_cmd (name, all_commands, NULL, descr, &dump_cmdlist);
  c->completer =  filename_completer;
  d = XMALLOC (struct dump_context);
  d->func = func;
  d->mode = FOPEN_WB;
  set_cmd_context (c, d);
  c->func = call_dump_func;

  c = add_cmd (name, all_commands, NULL, descr, &append_cmdlist);
  c->completer =  filename_completer;
  d = XMALLOC (struct dump_context);
  d->func = func;
  d->mode = FOPEN_AB;
  set_cmd_context (c, d);
  c->func = call_dump_func;

  /* Replace "Dump " at start of docstring with "Append " (borrowed
     from [deleted] deprecated_add_show_from_set).  */
  if (   c->doc[0] == 'W' 
      && c->doc[1] == 'r' 
      && c->doc[2] == 'i'
      && c->doc[3] == 't' 
      && c->doc[4] == 'e'
      && c->doc[5] == ' ')
    c->doc = concat ("Append ", c->doc + 6, (char *)NULL);
}

/* Opaque data for restore_section_callback. */
struct callback_data {
  unsigned long load_offset;
  CORE_ADDR load_start;
  CORE_ADDR load_end;
};

/* Function: restore_section_callback.

   Callback function for bfd_map_over_sections.
   Selectively loads the sections into memory.  */

static void
restore_section_callback (bfd *ibfd, asection *isec, void *args)
{
  struct callback_data *data = args;
  bfd_vma sec_start  = bfd_section_vma (ibfd, isec);
  bfd_size_type size = bfd_section_size (ibfd, isec);
  bfd_vma sec_end    = sec_start + size;
  bfd_size_type sec_offset = 0;
  bfd_size_type sec_load_count = size;
  struct cleanup *old_chain;
  gdb_byte *buf;
  int ret;

  /* Ignore non-loadable sections, eg. from elf files. */
  if (!(bfd_get_section_flags (ibfd, isec) & SEC_LOAD))
    return;

  /* Does the section overlap with the desired restore range? */
  if (sec_end <= data->load_start 
      || (data->load_end > 0 && sec_start >= data->load_end))
    {
      /* No, no useable data in this section. */
      printf_filtered (_("skipping section %s...\n"), 
		       bfd_section_name (ibfd, isec));
      return;
    }

  /* Compare section address range with user-requested
     address range (if any).  Compute where the actual
     transfer should start and end.  */
  if (sec_start < data->load_start)
    sec_offset = data->load_start - sec_start;
  /* Size of a partial transfer: */
  sec_load_count -= sec_offset;
  if (data->load_end > 0 && sec_end > data->load_end)
    sec_load_count -= sec_end - data->load_end;

  /* Get the data.  */
  buf = xmalloc (size);
  old_chain = make_cleanup (xfree, buf);
  if (!bfd_get_section_contents (ibfd, isec, buf, 0, size))
    error (_("Failed to read bfd file %s: '%s'."), bfd_get_filename (ibfd), 
	   bfd_errmsg (bfd_get_error ()));

  printf_filtered ("Restoring section %s (0x%lx to 0x%lx)",
		   bfd_section_name (ibfd, isec), 
		   (unsigned long) sec_start, 
		   (unsigned long) sec_end);

  if (data->load_offset != 0 || data->load_start != 0 || data->load_end != 0)
    printf_filtered (" into memory (0x%s to 0x%s)\n", 
		     paddr_nz ((unsigned long) sec_start 
			       + sec_offset + data->load_offset), 
		     paddr_nz ((unsigned long) sec_start + sec_offset 
		       + data->load_offset + sec_load_count));
  else
    puts_filtered ("\n");

  /* Write the data.  */
  ret = target_write_memory (sec_start + sec_offset + data->load_offset, 
			     buf + sec_offset, sec_load_count);
  if (ret != 0)
    warning (_("restore: memory write failed (%s)."), safe_strerror (ret));
  do_cleanups (old_chain);
  return;
}

/* APPLE LOCAL BEGIN: segment binary file downloads  */

static void
restore_binary_file (char *filename, struct callback_data *data)
{
  FILE *file = fopen_with_cleanup (filename, FOPEN_RB);
  int status;
  gdb_byte *buf;
  long len, total_file_bytes;
  int max_errors;

  /* Get the file size for reading.  */
  if (fseek (file, 0, SEEK_END) == 0) 
    {
      total_file_bytes = len = ftell (file);
      if (len > g_max_binary_file_chunk)
        len = g_max_binary_file_chunk;
    }
  else
    perror_with_name (filename);

  if (len <= data->load_start)
    error (_("Start address is greater than length of binary file %s."), 
	   filename);

  /* Chop off "len" if it exceeds the requested load_end addr. */
  if (data->load_end != 0 && data->load_end < len)
    len = data->load_end;
  /* Chop off "len" if the requested load_start addr skips some bytes. */
  if (data->load_start > 0)
    len -= data->load_start;

  printf_filtered 
    ("Restoring binary file %s into memory (0x%lx to 0x%lx)\n", 
     filename, 
     (unsigned long) data->load_start + data->load_offset, 
     (unsigned long) data->load_start + data->load_offset + total_file_bytes);

  /* Now set the file pos to the requested load start pos.  */
  if (fseek (file, data->load_start, SEEK_SET) != 0)
    perror_with_name (filename);

  /* Now allocate a buffer and read the file contents.  */
  buf = xmalloc (len);
  make_cleanup (xfree, buf);

  status = 0;
  max_errors = 3;
  while (total_file_bytes > 0) 
    {
      if (fread (buf, 1, len, file) != len)
	perror_with_name (filename);
    
      /* Now write the buffer into target memory. */
       printf_unfiltered ("\rWriting 0x%s bytes to 0x%s", paddr_nz (len),
			  paddr_nz (data->load_start + data->load_offset));
       gdb_flush (gdb_stdout);
       status = target_write_memory (data->load_start + data->load_offset, 
				     buf, len);
    
       if (status == 0) 
	{
	  data->load_offset += len;
	  total_file_bytes -= len;
	  if (total_file_bytes < g_max_binary_file_chunk)
	    len = total_file_bytes;
	  else
	    len = g_max_binary_file_chunk;
	}
      else 
	{
	  if (--max_errors > 0) 
	    warning (_("\nrestore: memory write failed - retrying."));
	  else 
            break;
	}
    }
  printf_unfiltered ("\n");
    
  if (status != 0)
    warning (_("restore: memory write failed (%s)."), safe_strerror (status));
}

/* APPLE LOCAL END: segment binary file downloads  */

static void
restore_command (char *args, int from_tty)
{
  char *filename;
  struct callback_data data;
  bfd *ibfd;
  int binary_flag = 0;

  if (!target_has_execution)
    noprocess ();

  data.load_offset = 0;
  data.load_start  = 0;
  data.load_end    = 0;

  /* Parse the input arguments.  First is filename (required). */
  filename = scan_filename_with_cleanup (&args, NULL);
  if (args != NULL && *args != '\0')
    {
      char *binary_string = "binary";

      /* Look for optional "binary" flag.  */
      if (strncmp (args, binary_string, strlen (binary_string)) == 0)
	{
	  binary_flag = 1;
	  args += strlen (binary_string);
	  args = skip_spaces (args);
	}
      /* Parse offset (optional). */
      if (args != NULL && *args != '\0')
      data.load_offset = 
	parse_and_eval_long (scan_expression_with_cleanup (&args, NULL));
      if (args != NULL && *args != '\0')
	{
	  /* Parse start address (optional). */
	  data.load_start = 
	    parse_and_eval_long (scan_expression_with_cleanup (&args, NULL));
	  if (args != NULL && *args != '\0')
	    {
	      /* Parse end address (optional). */
	      data.load_end = parse_and_eval_long (args);
	      if (data.load_end <= data.load_start)
		error (_("Start must be less than end."));
	    }
	}
    }

  if (info_verbose)
    printf_filtered ("Restore file %s offset 0x%lx start 0x%lx end 0x%lx\n",
		     filename, (unsigned long) data.load_offset, 
		     (unsigned long) data.load_start, 
		     (unsigned long) data.load_end);

  if (binary_flag)
    {
      restore_binary_file (filename, &data);
    }
  else
    {
      /* Open the file for loading. */
      ibfd = bfd_openr_with_cleanup (filename, NULL);

      /* Process the sections. */
      bfd_map_over_sections (ibfd, restore_section_callback, &data);
    }
  return;
}

static void
srec_dump_command (char *cmd, int from_tty)
{
  printf_unfiltered ("\"dump srec\" must be followed by a subcommand.\n");
  help_list (srec_cmdlist, "dump srec ", -1, gdb_stdout);
}

static void
ihex_dump_command (char *cmd, int from_tty)
{
  printf_unfiltered ("\"dump ihex\" must be followed by a subcommand.\n");
  help_list (ihex_cmdlist, "dump ihex ", -1, gdb_stdout);
}

static void
tekhex_dump_command (char *cmd, int from_tty)
{
  printf_unfiltered ("\"dump tekhex\" must be followed by a subcommand.\n");
  help_list (tekhex_cmdlist, "dump tekhex ", -1, gdb_stdout);
}

static void
binary_dump_command (char *cmd, int from_tty)
{
  printf_unfiltered ("\"dump binary\" must be followed by a subcommand.\n");
  help_list (binary_dump_cmdlist, "dump binary ", -1, gdb_stdout);
}

static void
binary_append_command (char *cmd, int from_tty)
{
  printf_unfiltered ("\"append binary\" must be followed by a subcommand.\n");
  help_list (binary_append_cmdlist, "append binary ", -1, gdb_stdout);
}

extern initialize_file_ftype _initialize_cli_dump; /* -Wmissing-prototypes */

/* APPLE LOCAL BEGIN: segment binary file downloads  */

static void
show_binary_buffer_size (char *args, int from_tty)
{
    printf_filtered (_("The restore binary buffer size is %ld (0x%lx).\n"), 
		     g_max_binary_file_chunk, g_max_binary_file_chunk );
}

static void
set_binary_buffer_size (char *args, int from_tty)
{
  if (args == NULL) 
    {
      printf_filtered (_("Reverting the restore binary buffer size to the default value.\n")); 
      g_max_binary_file_chunk = DEFAULT_MAX_BINARY_FILE_CHUNK;
    }
  else
    {
      char *end;
      long binary_file_chunk = strtol (args, &end, 0);
      /* Make sure that the new value is larger than the the 
         minimum max remote packet size so we all remote
         command packets can still function properly.  */
      if (end == NULL || *end != '\0')
        {
          error (_("Invalid binary buffer size argument '%s'."), args);
        }
      else
	{
          g_max_binary_file_chunk = binary_file_chunk;
        }
    }
  show_binary_buffer_size (NULL, from_tty);
}

static void
set_restore_cmd (char *args, int from_tty)
{
}

static void
show_restore_cmd (char *args, int from_tty)
{
  show_binary_buffer_size (args, from_tty);
}

/* APPLE LOCAL END: segment binary file downloads  */


void
_initialize_cli_dump (void)
{
  struct cmd_list_element *c;

/* APPLE LOCAL BEGIN: segment binary file downloads  */
  static struct cmd_list_element *restore_set_cmdlist;
  static struct cmd_list_element *restore_show_cmdlist;

  add_prefix_cmd ("restore", no_class, set_restore_cmd, 
		  _("Set restore specific command settings\n"),
		  &restore_set_cmdlist, "set restore ",
		  0 /* allow-unknown */, &setlist);
  add_prefix_cmd ("restore", no_class, show_restore_cmd, 
		  _("Show current restore specific command settings"),
		  &restore_show_cmdlist, "show restore ",
		  0 /* allow-unknown */, &showlist);

  add_cmd ("binary-buffer-size", 
           no_class, set_binary_buffer_size, _("\
Set the max binary buffer size to use with the 'restore' command.\n\
The single optional argument SIZE can be given to specify the size of the\n\
buffer in bytes. When no argument is given, the default value is restored.\n\
When a binary file is restored to memory, SIZE bytes will be read from the\n\
current file position and then written to appropriate target memory address.\n\
This allows large files to be downloaded to target memory without the risk of\n\
large allocation failures in gdb."), 
	   &restore_set_cmdlist);
  add_cmd ("binary-buffer-size", 
           no_class, show_binary_buffer_size, 
	   _("Show the max binary buffer size to use with the 'restore' command."),
	   &restore_show_cmdlist);
  /* APPLE LOCAL END: segment binary file downloads  */

  add_prefix_cmd ("dump", class_vars, dump_command, _("\
Dump target code/data to a local file."),
		  &dump_cmdlist, "dump ",
		  0/*allow-unknown*/,
		  &cmdlist);
  add_prefix_cmd ("append", class_vars, append_command, _("\
Append target code/data to a local file."),
		  &append_cmdlist, "append ",
		  0/*allow-unknown*/,
		  &cmdlist);

  add_dump_command ("memory", dump_memory_command, "\
Write contents of memory to a raw binary file.\n\
Arguments are FILE START STOP.  Writes the contents of memory within the\n\
range [START .. STOP) to the specifed FILE in raw target ordered bytes.");

  add_dump_command ("value", dump_value_command, "\
Write the value of an expression to a raw binary file.\n\
Arguments are FILE EXPRESSION.  Writes the value of EXPRESSION to\n\
the specified FILE in raw target ordered bytes.");

  add_prefix_cmd ("srec", all_commands, srec_dump_command, _("\
Write target code/data to an srec file."),
		  &srec_cmdlist, "dump srec ", 
		  0 /*allow-unknown*/, 
		  &dump_cmdlist);

  add_prefix_cmd ("ihex", all_commands, ihex_dump_command, _("\
Write target code/data to an intel hex file."),
		  &ihex_cmdlist, "dump ihex ", 
		  0 /*allow-unknown*/, 
		  &dump_cmdlist);

  add_prefix_cmd ("tekhex", all_commands, tekhex_dump_command, _("\
Write target code/data to a tekhex file."),
		  &tekhex_cmdlist, "dump tekhex ", 
		  0 /*allow-unknown*/, 
		  &dump_cmdlist);

  add_prefix_cmd ("binary", all_commands, binary_dump_command, _("\
Write target code/data to a raw binary file."),
		  &binary_dump_cmdlist, "dump binary ", 
		  0 /*allow-unknown*/, 
		  &dump_cmdlist);

  add_prefix_cmd ("binary", all_commands, binary_append_command, _("\
Append target code/data to a raw binary file."),
		  &binary_append_cmdlist, "append binary ", 
		  0 /*allow-unknown*/, 
		  &append_cmdlist);

  add_cmd ("memory", all_commands, dump_srec_memory, _("\
Write contents of memory to an srec file.\n\
Arguments are FILE START STOP.  Writes the contents of memory\n\
within the range [START .. STOP) to the specifed FILE in srec format."),
	   &srec_cmdlist);

  add_cmd ("value", all_commands, dump_srec_value, _("\
Write the value of an expression to an srec file.\n\
Arguments are FILE EXPRESSION.  Writes the value of EXPRESSION\n\
to the specified FILE in srec format."),
	   &srec_cmdlist);

  add_cmd ("memory", all_commands, dump_ihex_memory, _("\
Write contents of memory to an ihex file.\n\
Arguments are FILE START STOP.  Writes the contents of memory within\n\
the range [START .. STOP) to the specifed FILE in intel hex format."),
	   &ihex_cmdlist);

  add_cmd ("value", all_commands, dump_ihex_value, _("\
Write the value of an expression to an ihex file.\n\
Arguments are FILE EXPRESSION.  Writes the value of EXPRESSION\n\
to the specified FILE in intel hex format."),
	   &ihex_cmdlist);

  add_cmd ("memory", all_commands, dump_tekhex_memory, _("\
Write contents of memory to a tekhex file.\n\
Arguments are FILE START STOP.  Writes the contents of memory\n\
within the range [START .. STOP) to the specifed FILE in tekhex format."),
	   &tekhex_cmdlist);

  add_cmd ("value", all_commands, dump_tekhex_value, _("\
Write the value of an expression to a tekhex file.\n\
Arguments are FILE EXPRESSION.  Writes the value of EXPRESSION\n\
to the specified FILE in tekhex format."),
	   &tekhex_cmdlist);

  add_cmd ("memory", all_commands, dump_binary_memory, _("\
Write contents of memory to a raw binary file.\n\
Arguments are FILE START STOP.  Writes the contents of memory\n\
within the range [START .. STOP) to the specifed FILE in binary format."),
	   &binary_dump_cmdlist);

  add_cmd ("value", all_commands, dump_binary_value, _("\
Write the value of an expression to a raw binary file.\n\
Arguments are FILE EXPRESSION.  Writes the value of EXPRESSION\n\
to the specified FILE in raw target ordered bytes."),
	   &binary_dump_cmdlist);

  add_cmd ("memory", all_commands, append_binary_memory, _("\
Append contents of memory to a raw binary file.\n\
Arguments are FILE START STOP.  Writes the contents of memory within the\n\
range [START .. STOP) to the specifed FILE in raw target ordered bytes."),
	   &binary_append_cmdlist);

  add_cmd ("value", all_commands, append_binary_value, _("\
Append the value of an expression to a raw binary file.\n\
Arguments are FILE EXPRESSION.  Writes the value of EXPRESSION\n\
to the specified FILE in raw target ordered bytes."),
	   &binary_append_cmdlist);

  c = add_com ("restore", class_vars, restore_command, _("\
Restore the contents of FILE to target memory.\n\
Arguments are FILE OFFSET START END where all except FILE are optional.\n\
OFFSET will be added to the base address of the file (default zero).\n\
If START and END are given, only the file contents within that range\n\
(file relative) will be restored to target memory."));
  c->completer = filename_completer;
  /* FIXME: completers for other commands. */
}

/* Parse and display command line options.
   Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.
   Contributed by Andy Vaught

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to the Free
Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA
02110-1301, USA.  */


#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tree.h"
#include "flags.h"
#include "intl.h"
#include "opts.h"
#include "options.h"
#include "tree-inline.h"

#include "gfortran.h"
#include "target.h"

gfc_option_t gfc_option;


/* Set flags that control warnings and errors for different
   Fortran standards to their default values.  */

static void
set_default_std_flags (void)
{
  gfc_option.allow_std = GFC_STD_F95_OBS | GFC_STD_F95_DEL
    | GFC_STD_F2003 | GFC_STD_F95 | GFC_STD_F77 | GFC_STD_GNU
    | GFC_STD_LEGACY;
  gfc_option.warn_std = GFC_STD_F95_DEL | GFC_STD_LEGACY;
}

/* Get ready for options handling.  */

unsigned int
gfc_init_options (unsigned int argc ATTRIBUTE_UNUSED,
		  const char **argv ATTRIBUTE_UNUSED)
{
  gfc_source_file = NULL;
  gfc_option.module_dir = NULL;
  gfc_option.source_form = FORM_UNKNOWN;
  gfc_option.fixed_line_length = -1;
  gfc_option.free_line_length = -1;
  gfc_option.max_continue_fixed = 19;
  gfc_option.max_continue_free = 39;
  gfc_option.max_identifier_length = GFC_MAX_SYMBOL_LEN;
  gfc_option.max_subrecord_length = 0;
  gfc_option.convert = CONVERT_NATIVE;
  gfc_option.record_marker = 0;
  gfc_option.verbose = 0;

  gfc_option.warn_aliasing = 0;
  gfc_option.warn_ampersand = 0;
  gfc_option.warn_character_truncation = 0;
  gfc_option.warn_conversion = 0;
  gfc_option.warn_implicit_interface = 0;
  gfc_option.warn_line_truncation = 0;
  gfc_option.warn_surprising = 0;
  gfc_option.warn_tabs = 1;
  gfc_option.warn_underflow = 1;
  gfc_option.max_errors = 25;

  gfc_option.flag_all_intrinsics = 0;
  gfc_option.flag_default_double = 0;
  gfc_option.flag_default_integer = 0;
  gfc_option.flag_default_real = 0;
  gfc_option.flag_dollar_ok = 0;
  gfc_option.flag_underscoring = 1;
  gfc_option.flag_f2c = 0;
  gfc_option.flag_second_underscore = -1;
  gfc_option.flag_implicit_none = 0;
  gfc_option.flag_max_stack_var_size = 32768;
  gfc_option.flag_no_backend = 0;
  gfc_option.flag_range_check = 1;
  gfc_option.flag_pack_derived = 0;
  gfc_option.flag_repack_arrays = 0;
  gfc_option.flag_preprocessed = 0;
  gfc_option.flag_automatic = 1;
  gfc_option.flag_backslash = 1;
  gfc_option.flag_cray_pointer = 0;
  gfc_option.flag_d_lines = -1;
  gfc_option.flag_openmp = 0;

  gfc_option.fpe = 0;

  /* Argument pointers cannot point to anything
     but their argument.  */
  flag_argument_noalias = 3;

  flag_errno_math = 0;

  set_default_std_flags ();

  gfc_option.warn_nonstd_intrinsics = 0;

  /* -fshort-enums can be default on some targets.  */
  gfc_option.fshort_enums = targetm.default_short_enums ();

  return CL_Fortran;
}


/* Determine the source form from the filename extension.  We assume
   case insensitivity.  */

static gfc_source_form
form_from_filename (const char *filename)
{

  static const struct
  {
    const char *extension;
    gfc_source_form form;
  }
  exttype[] =
  {
    {
    ".f90", FORM_FREE}
    ,
    {
    ".f95", FORM_FREE}
    ,
    {
    ".f03", FORM_FREE}
    ,
    {
    ".f", FORM_FIXED}
    ,
    {
    ".for", FORM_FIXED}
    ,
    {
    "", FORM_UNKNOWN}
  };		/* sentinel value */

  gfc_source_form f_form;
  const char *fileext;
  int i;

  /* Find end of file name.  Note, filename is either a NULL pointer or
     a NUL terminated string.  */
  i = 0;
  while (filename[i] != '\0')
    i++;

  /* Find last period.  */
  while (i >= 0 && (filename[i] != '.'))
    i--;

  /* Did we see a file extension?  */
  if (i < 0)
    return FORM_UNKNOWN; /* Nope  */

  /* Get file extension and compare it to others.  */
  fileext = &(filename[i]);

  i = -1;
  f_form = FORM_UNKNOWN;
  do
    {
      i++;
      if (strcasecmp (fileext, exttype[i].extension) == 0)
	{
	  f_form = exttype[i].form;
	  break;
	}
    }
  while (exttype[i].form != FORM_UNKNOWN);

  return f_form;
}


/* Finalize commandline options.  */

bool
gfc_post_options (const char **pfilename)
{
  const char *filename = *pfilename, *canon_source_file = NULL;
  char *source_path;
  int i;

  /* Verify the input file name.  */
  if (!filename || strcmp (filename, "-") == 0)
    {
      filename = "";
    }

  if (gfc_option.flag_preprocessed)
    {
      /* For preprocessed files, if the first tokens are of the form # NUM.
	 handle the directives so we know the original file name.  */
      gfc_source_file = gfc_read_orig_filename (filename, &canon_source_file);
      if (gfc_source_file == NULL)
	gfc_source_file = filename;
      else
	*pfilename = gfc_source_file;
    }
  else
    gfc_source_file = filename;

  if (canon_source_file == NULL)
    canon_source_file = gfc_source_file;

  /* Adds the path where the source file is to the list of include files.  */

  i = strlen (canon_source_file);
  while (i > 0 && !IS_DIR_SEPARATOR (canon_source_file[i]))
    i--;
  if (i != 0)
    {
      source_path = alloca (i + 1);
      memcpy (source_path, canon_source_file, i);
      source_path[i] = 0;
      gfc_add_include_path (source_path);
    }
  else
    gfc_add_include_path (".");

  if (canon_source_file != gfc_source_file)
    gfc_free ((void *) canon_source_file);

  /* Decide which form the file will be read in as.  */

  if (gfc_option.source_form != FORM_UNKNOWN)
    gfc_current_form = gfc_option.source_form;
  else
    {
      gfc_current_form = form_from_filename (filename);

      if (gfc_current_form == FORM_UNKNOWN)
	{
	  gfc_current_form = FORM_FREE;
	  gfc_warning_now ("Reading file '%s' as free form.", 
			   (filename[0] == '\0') ? "<stdin>" : filename);
	}
    }

  /* If the user specified -fd-lines-as-{code|comments} verify that we're
     in fixed form.  */
  if (gfc_current_form == FORM_FREE)
    {
      if (gfc_option.flag_d_lines == 0)
	gfc_warning_now ("'-fd-lines-as-comments' has no effect "
			 "in free form.");
      else if (gfc_option.flag_d_lines == 1)
	gfc_warning_now ("'-fd-lines-as-code' has no effect "
			 "in free form.");
    }

  flag_inline_trees = 1;

  /* Use tree inlining.  */
  if (!flag_no_inline)
    flag_no_inline = 1;
  if (flag_inline_functions)
    flag_inline_trees = 2;

  /* If -pedantic, warn about the use of GNU extensions.  */
  if (pedantic && (gfc_option.allow_std & GFC_STD_GNU) != 0)
    gfc_option.warn_std |= GFC_STD_GNU;
  /* -std=legacy -pedantic is effectively -std=gnu.  */
  if (pedantic && (gfc_option.allow_std & GFC_STD_LEGACY) != 0)
    gfc_option.warn_std |= GFC_STD_F95_OBS | GFC_STD_F95_DEL | GFC_STD_LEGACY;

  /* If the user didn't explicitly specify -f(no)-second-underscore we
     use it if we're trying to be compatible with f2c, and not
     otherwise.  */
  if (gfc_option.flag_second_underscore == -1)
    gfc_option.flag_second_underscore = gfc_option.flag_f2c;

  /* Implement -fno-automatic as -fmax-stack-var-size=0.  */
  if (!gfc_option.flag_automatic)
    gfc_option.flag_max_stack_var_size = 0;
  
  if (pedantic)
    {
      gfc_option.warn_ampersand = 1;
      gfc_option.warn_tabs = 0;
    }

  if (gfc_option.flag_all_intrinsics)
    gfc_option.warn_nonstd_intrinsics = 0;

  return false;
}


/* Set the options for -Wall.  */

static void
set_Wall (int setting)
{
  gfc_option.warn_aliasing = setting;
  gfc_option.warn_ampersand = setting;
  gfc_option.warn_line_truncation = setting;
  gfc_option.warn_nonstd_intrinsics = setting;
  gfc_option.warn_surprising = setting;
  gfc_option.warn_tabs = !setting;
  gfc_option.warn_underflow = setting;
  gfc_option.warn_character_truncation = setting;

  set_Wunused (setting);
  warn_return_type = setting;
  warn_switch = setting;

  /* We save the value of warn_uninitialized, since if they put
     -Wuninitialized on the command line, we need to generate a
     warning about not using it without also specifying -O.  */
  if (setting == 0)
    warn_uninitialized = 0;
  else if (warn_uninitialized != 1)
    warn_uninitialized = 2;
}


static void
gfc_handle_module_path_options (const char *arg)
{

  if (gfc_option.module_dir != NULL)
    {
      gfc_status ("gfortran: Only one -M option allowed\n");
      exit (3);
    }

  if (arg == NULL)
    {
      gfc_status ("gfortran: Directory required after -M\n");
      exit (3);
    }

  gfc_option.module_dir = (char *) gfc_getmem (strlen (arg) + 2);
  strcpy (gfc_option.module_dir, arg);
  strcat (gfc_option.module_dir, "/");

  gfc_add_include_path (gfc_option.module_dir);
}

static void
gfc_handle_fpe_trap_option (const char *arg)
{
  int result, pos = 0, n;
  static const char * const exception[] = { "invalid", "denormal", "zero",
                                            "overflow", "underflow",
					    "precision", NULL };
  static const int opt_exception[] = { GFC_FPE_INVALID, GFC_FPE_DENORMAL,
				       GFC_FPE_ZERO, GFC_FPE_OVERFLOW,
				       GFC_FPE_UNDERFLOW, GFC_FPE_PRECISION,
				       0 };
 
  while (*arg)
    {
      while (*arg == ',')
	arg++;
      while (arg[pos] && arg[pos] != ',')
	pos++;
      result = 0;
      for (n = 0; exception[n] != NULL; n++)
	{
	  if (exception[n] && strncmp (exception[n], arg, pos) == 0)
	    {
	      gfc_option.fpe |= opt_exception[n];
	      arg += pos;
	      pos = 0;
	      result = 1;
	      break;
	    }
	}
      if (! result)
	gfc_fatal_error ("Argument to -ffpe-trap is not valid: %s", arg);
    }
}

/* Handle command-line options.  Returns 0 if unrecognized, 1 if
   recognized and handled.  */
int
gfc_handle_option (size_t scode, const char *arg, int value)
{
  int result = 1;
  enum opt_code code = (enum opt_code) scode;

  /* Ignore file names.  */
  if (code == N_OPTS)
    return 1;

  switch (code)
    {
    default:
      result = 0;
      break;

    case OPT_Wall:
      set_Wall (value);
      break;

    case OPT_Waliasing:
      gfc_option.warn_aliasing = value;
      break;

    case OPT_Wampersand:
      gfc_option.warn_ampersand = value;
      break;

    case OPT_Wcharacter_truncation:
      gfc_option.warn_character_truncation = value;
      break;

    case OPT_Wconversion:
      gfc_option.warn_conversion = value;
      break;

    case OPT_Wimplicit_interface:
      gfc_option.warn_implicit_interface = value;
      break;

    case OPT_Wline_truncation:
      gfc_option.warn_line_truncation = value;
      break;

    case OPT_Wsurprising:
      gfc_option.warn_surprising = value;
      break;

    case OPT_Wtabs:
      gfc_option.warn_tabs = value;
      break;

    case OPT_Wunderflow:
      gfc_option.warn_underflow = value;
      break;

    case OPT_fall_intrinsics:
      gfc_option.flag_all_intrinsics = 1;
      break;

    case OPT_fautomatic:
      gfc_option.flag_automatic = value;
      break;

    case OPT_fbackslash:
      gfc_option.flag_backslash = value;
      break;
      
    case OPT_fcray_pointer:
      gfc_option.flag_cray_pointer = value;
      break;

    case OPT_ff2c:
      gfc_option.flag_f2c = value;
      break;

    case OPT_fdollar_ok:
      gfc_option.flag_dollar_ok = value;
      break;

    case OPT_fd_lines_as_code:
      gfc_option.flag_d_lines = 1;
      break;

    case OPT_fd_lines_as_comments:
      gfc_option.flag_d_lines = 0;
      break;

    case OPT_fdump_parse_tree:
      gfc_option.verbose = value;
      break;

    case OPT_ffixed_form:
      gfc_option.source_form = FORM_FIXED;
      break;

    case OPT_ffixed_line_length_none:
      gfc_option.fixed_line_length = 0;
      break;

    case OPT_ffixed_line_length_:
      if (value != 0 && value < 7)
	gfc_fatal_error ("Fixed line length must be at least seven.");
      gfc_option.fixed_line_length = value;
      break;

    case OPT_ffree_form:
      gfc_option.source_form = FORM_FREE;
      break;

    case OPT_fopenmp:
      gfc_option.flag_openmp = value;
      break;

    case OPT_ffree_line_length_none:
      gfc_option.free_line_length = 0;
      break;

    case OPT_ffree_line_length_:
      gfc_option.free_line_length = value;
      break;

    case OPT_funderscoring:
      gfc_option.flag_underscoring = value;
      break;

    case OPT_fsecond_underscore:
      gfc_option.flag_second_underscore = value;
      break;

    case OPT_fimplicit_none:
      gfc_option.flag_implicit_none = value;
      break;

    case OPT_fmax_errors_:
      gfc_option.max_errors = value;
      break;

    case OPT_fmax_stack_var_size_:
      gfc_option.flag_max_stack_var_size = value;
      break;

    case OPT_fno_backend:
      gfc_option.flag_no_backend = value;
      break;

    case OPT_frange_check:
      gfc_option.flag_range_check = value;
      break;

    case OPT_fpack_derived:
      gfc_option.flag_pack_derived = value;
      break;

    case OPT_frepack_arrays:
      gfc_option.flag_repack_arrays = value;
      break;

    case OPT_fpreprocessed:
      gfc_option.flag_preprocessed = value;
      break;

    case OPT_fmax_identifier_length_:
      if (value > GFC_MAX_SYMBOL_LEN)
	gfc_fatal_error ("Maximum supported identifier length is %d",
			 GFC_MAX_SYMBOL_LEN);
      gfc_option.max_identifier_length = value;
      break;

    case OPT_fdefault_integer_8:
      gfc_option.flag_default_integer = value;
      break;

    case OPT_fdefault_real_8:
      gfc_option.flag_default_real = value;
      break;

    case OPT_fdefault_double_8:
      gfc_option.flag_default_double = value;
      break;

    case OPT_I:
      gfc_add_include_path (arg);
      break;

    case OPT_J:
    case OPT_M:
      gfc_handle_module_path_options (arg);
      break;
    
    case OPT_ffpe_trap_:
      gfc_handle_fpe_trap_option (arg);
      break;

    case OPT_std_f95:
      gfc_option.allow_std = GFC_STD_F95_OBS | GFC_STD_F95 | GFC_STD_F77;
      gfc_option.warn_std = GFC_STD_F95_OBS;
      gfc_option.max_identifier_length = 31;
      gfc_option.warn_ampersand = 1;
      gfc_option.warn_tabs = 0;
      break;

    case OPT_std_f2003:
      gfc_option.allow_std = GFC_STD_F95_OBS | GFC_STD_F77 
	| GFC_STD_F2003 | GFC_STD_F95;
      gfc_option.warn_std = GFC_STD_F95_OBS;
      gfc_option.max_continue_fixed = 255;
      gfc_option.max_continue_free = 255;
      gfc_option.max_identifier_length = 63;
      gfc_option.warn_ampersand = 1;
      gfc_option.warn_tabs = 0;
      break;

    case OPT_std_gnu:
      set_default_std_flags ();
      break;

    case OPT_std_legacy:
      set_default_std_flags ();
      gfc_option.warn_std = 0;
      break;

    case OPT_Wnonstd_intrinsics:
      gfc_option.warn_nonstd_intrinsics = value;
      break;

    case OPT_fshort_enums:
      gfc_option.fshort_enums = 1;
      break;

    case OPT_fconvert_little_endian:
      gfc_option.convert = CONVERT_LITTLE;
      break;

    case OPT_fconvert_big_endian:
      gfc_option.convert = CONVERT_BIG;
      break;

    case OPT_fconvert_native:
      gfc_option.convert = CONVERT_NATIVE;
      break;

    case OPT_fconvert_swap:
      gfc_option.convert = CONVERT_SWAP;
      break;

    case OPT_frecord_marker_4:
      gfc_option.record_marker = 4;
      break;

    case OPT_frecord_marker_8:
      gfc_option.record_marker = 8;
      break;

    case OPT_fmax_subrecord_length_:
      if (value > MAX_SUBRECORD_LENGTH)
	gfc_fatal_error ("Maximum subrecord length cannot exceed %d", MAX_SUBRECORD_LENGTH);

      gfc_option.max_subrecord_length = value;
    }

  return result;
}

/* Dynamic architecture support for GDB, the GNU debugger.

   Copyright 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005
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

#include "arch-utils.h"
#include "buildsym.h"
#include "gdbcmd.h"
#include "inferior.h"		/* enum CALL_DUMMY_LOCATION et.al. */
#include "gdb_string.h"
#include "regcache.h"
#include "gdb_assert.h"
#include "sim-regno.h"
#include "gdbcore.h"
#include "osabi.h"

#include "version.h"

#include "floatformat.h"

/* Implementation of extract return value that grubs around in the
   register cache.  */
void
legacy_extract_return_value (struct type *type, struct regcache *regcache,
			     gdb_byte *valbuf)
{
  gdb_byte *registers = deprecated_grub_regcache_for_registers (regcache);
  gdb_byte *buf = valbuf;
  DEPRECATED_EXTRACT_RETURN_VALUE (type, registers, buf); /* OK */
}

/* Implementation of store return value that grubs the register cache.
   Takes a local copy of the buffer to avoid const problems.  */
void
legacy_store_return_value (struct type *type, struct regcache *regcache,
			   const gdb_byte *buf)
{
  gdb_byte *b = alloca (TYPE_LENGTH (type));
  gdb_assert (regcache == current_regcache);
  memcpy (b, buf, TYPE_LENGTH (type));
  DEPRECATED_STORE_RETURN_VALUE (type, b);
}

int
always_use_struct_convention (int gcc_p, struct type *value_type)
{
  return 1;
}

enum return_value_convention
legacy_return_value (struct gdbarch *gdbarch, struct type *valtype,
		     struct regcache *regcache, gdb_byte *readbuf,
		     const gdb_byte *writebuf)
{
  /* NOTE: cagney/2004-06-13: The gcc_p parameter to
     USE_STRUCT_CONVENTION isn't used.  */
  int struct_return = ((TYPE_CODE (valtype) == TYPE_CODE_STRUCT
			|| TYPE_CODE (valtype) == TYPE_CODE_UNION
			|| TYPE_CODE (valtype) == TYPE_CODE_ARRAY)
		       && DEPRECATED_USE_STRUCT_CONVENTION (0, valtype));

  if (writebuf != NULL)
    {
      gdb_assert (!struct_return);
      /* NOTE: cagney/2004-06-13: See stack.c:return_command.  Old
	 architectures don't expect STORE_RETURN_VALUE to handle small
	 structures.  Should not be called with such types.  */
      gdb_assert (TYPE_CODE (valtype) != TYPE_CODE_STRUCT
		  && TYPE_CODE (valtype) != TYPE_CODE_UNION);
      STORE_RETURN_VALUE (valtype, regcache, writebuf);
    }

  if (readbuf != NULL)
    {
      gdb_assert (!struct_return);
      EXTRACT_RETURN_VALUE (valtype, regcache, readbuf);
    }

  if (struct_return)
    return RETURN_VALUE_STRUCT_CONVENTION;
  else
    return RETURN_VALUE_REGISTER_CONVENTION;
}

int
legacy_register_sim_regno (int regnum)
{
  /* Only makes sense to supply raw registers.  */
  gdb_assert (regnum >= 0 && regnum < NUM_REGS);
  /* NOTE: cagney/2002-05-13: The old code did it this way and it is
     suspected that some GDB/SIM combinations may rely on this
     behavour.  The default should be one2one_register_sim_regno
     (below).  */
  if (REGISTER_NAME (regnum) != NULL
      && REGISTER_NAME (regnum)[0] != '\0')
    return regnum;
  else
    return LEGACY_SIM_REGNO_IGNORE;
}

CORE_ADDR
generic_skip_trampoline_code (CORE_ADDR pc)
{
  return 0;
}

CORE_ADDR
generic_skip_solib_resolver (struct gdbarch *gdbarch, CORE_ADDR pc)
{
  return 0;
}

int
generic_in_solib_return_trampoline (CORE_ADDR pc, char *name)
{
  return 0;
}

int
generic_in_function_epilogue_p (struct gdbarch *gdbarch, CORE_ADDR pc)
{
  return 0;
}

void
generic_remote_translate_xfer_address (struct gdbarch *gdbarch,
				       struct regcache *regcache,
				       CORE_ADDR gdb_addr, int gdb_len,
				       CORE_ADDR * rem_addr, int *rem_len)
{
  *rem_addr = gdb_addr;
  *rem_len = gdb_len;
}

/* Helper functions for INNER_THAN */

int
core_addr_lessthan (CORE_ADDR lhs, CORE_ADDR rhs)
{
  return (lhs < rhs);
}

int
core_addr_greaterthan (CORE_ADDR lhs, CORE_ADDR rhs)
{
  return (lhs > rhs);
}


/* Helper functions for TARGET_{FLOAT,DOUBLE}_FORMAT */

const struct floatformat *
default_float_format (struct gdbarch *gdbarch)
{
  int byte_order = gdbarch_byte_order (gdbarch);
  switch (byte_order)
    {
    case BFD_ENDIAN_BIG:
      return &floatformat_ieee_single_big;
    case BFD_ENDIAN_LITTLE:
      return &floatformat_ieee_single_little;
    default:
      internal_error (__FILE__, __LINE__,
		      _("default_float_format: bad byte order"));
    }
}


const struct floatformat *
default_double_format (struct gdbarch *gdbarch)
{
  int byte_order = gdbarch_byte_order (gdbarch);
  switch (byte_order)
    {
    case BFD_ENDIAN_BIG:
      return &floatformat_ieee_double_big;
    case BFD_ENDIAN_LITTLE:
      return &floatformat_ieee_double_little;
    default:
      internal_error (__FILE__, __LINE__,
		      _("default_double_format: bad byte order"));
    }
}

/* Misc helper functions for targets. */

CORE_ADDR
core_addr_identity (CORE_ADDR addr)
{
  return addr;
}

CORE_ADDR
convert_from_func_ptr_addr_identity (struct gdbarch *gdbarch, CORE_ADDR addr,
				     struct target_ops *targ)
{
  return addr;
}

int
no_op_reg_to_regnum (int reg)
{
  return reg;
}

void
default_elf_make_msymbol_special (asymbol *sym, struct minimal_symbol *msym)
{
  return;
}

void
default_coff_make_msymbol_special (int val, struct minimal_symbol *msym)
{
  return;
}

void
default_dbx_make_msymbol_special (int16_t val, struct minimal_symbol *msym)
{
  return;
}

int
cannot_register_not (int regnum)
{
  return 0;
}

/* Legacy version of target_virtual_frame_pointer().  Assumes that
   there is an DEPRECATED_FP_REGNUM and that it is the same, cooked or
   raw.  */

void
legacy_virtual_frame_pointer (CORE_ADDR pc,
			      int *frame_regnum,
			      LONGEST *frame_offset)
{
  /* FIXME: cagney/2002-09-13: This code is used when identifying the
     frame pointer of the current PC.  It is assuming that a single
     register and an offset can determine this.  I think it should
     instead generate a byte code expression as that would work better
     with things like Dwarf2's CFI.  */
  if (DEPRECATED_FP_REGNUM >= 0 && DEPRECATED_FP_REGNUM < NUM_REGS)
    *frame_regnum = DEPRECATED_FP_REGNUM;
  else if (SP_REGNUM >= 0 && SP_REGNUM < NUM_REGS)
    *frame_regnum = SP_REGNUM;
  else
    /* Should this be an internal error?  I guess so, it is reflecting
       an architectural limitation in the current design.  */
    internal_error (__FILE__, __LINE__, _("No virtual frame pointer available"));
  *frame_offset = 0;
}

/* Assume the world is sane, every register's virtual and real size
   is identical.  */

int
generic_register_size (int regnum)
{
  gdb_assert (regnum >= 0 && regnum < NUM_REGS + NUM_PSEUDO_REGS);
  return TYPE_LENGTH (gdbarch_register_type (current_gdbarch, regnum));
}

/* Assume all registers are adjacent.  */

int
generic_register_byte (int regnum)
{
  int byte;
  int i;
  gdb_assert (regnum >= 0 && regnum < NUM_REGS + NUM_PSEUDO_REGS);
  byte = 0;
  for (i = 0; i < regnum; i++)
    {
      byte += generic_register_size (i);
    }
  return byte;
}


int
legacy_pc_in_sigtramp (CORE_ADDR pc, char *name)
{
#if defined (DEPRECATED_IN_SIGTRAMP)
  return DEPRECATED_IN_SIGTRAMP (pc, name);
#else
  return name && strcmp ("_sigtramp", name) == 0;
#endif
}

int
generic_convert_register_p (int regnum, struct type *type)
{
  return 0;
}

int
default_stabs_argument_has_addr (struct gdbarch *gdbarch, struct type *type)
{
  if (DEPRECATED_REG_STRUCT_HAS_ADDR_P ()
      && DEPRECATED_REG_STRUCT_HAS_ADDR (processing_gcc_compilation, type))
    {
      CHECK_TYPEDEF (type);

      return (TYPE_CODE (type) == TYPE_CODE_STRUCT
	      || TYPE_CODE (type) == TYPE_CODE_UNION
	      || TYPE_CODE (type) == TYPE_CODE_SET
	      || TYPE_CODE (type) == TYPE_CODE_BITSTRING);
    }

  return 0;
}

int
generic_instruction_nullified (struct gdbarch *gdbarch,
			       struct regcache *regcache)
{
  return 0;
}


/* Functions to manipulate the endianness of the target.  */

/* ``target_byte_order'' is only used when non- multi-arch.
   Multi-arch targets obtain the current byte order using the
   TARGET_BYTE_ORDER gdbarch method.

   The choice of initial value is entirely arbitrary.  During startup,
   the function initialize_current_architecture() updates this value
   based on default byte-order information extracted from BFD.  */
static int target_byte_order = BFD_ENDIAN_BIG;
static int target_byte_order_auto = 1;

enum bfd_endian
selected_byte_order (void)
{
  if (target_byte_order_auto)
    return BFD_ENDIAN_UNKNOWN;
  else
    return target_byte_order;
}

static const char endian_big[] = "big";
static const char endian_little[] = "little";
static const char endian_auto[] = "auto";
static const char *endian_enum[] =
{
  endian_big,
  endian_little,
  endian_auto,
  NULL,
};
static const char *set_endian_string;

/* Called by ``show endian''.  */

static void
show_endian (struct ui_file *file, int from_tty, struct cmd_list_element *c,
	     const char *value)
{
  /* APPLE LOCAL begin endianness */
  /* Output the endianness (both what is set by the user, and the
     endianness currently effective) in MI format, if appropriate. */
  if (!ui_out_is_mi_like_p (uiout)) 
    {
      if (target_byte_order_auto)
	printf_unfiltered ("The target endianness is set automatically (currently %s endian)\n",
			   (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG ? "big" : "little"));
      else
	printf_unfiltered ("The target is assumed to be %s endian\n",
			   (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG ? "big" : "little"));
    }
  else
    {
      if (target_byte_order_auto)
	ui_out_field_string (uiout, "value", "auto");
      else
	ui_out_field_string (uiout, "value", (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG ? "big" : "little"));

      ui_out_field_string (uiout, "current", (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG ? "big" : "little"));
    }
  /* APPLE LOCAL end endianness */
}

static void
set_endian (char *ignore_args, int from_tty, struct cmd_list_element *c)
{
  if (set_endian_string == endian_auto)
    {
      target_byte_order_auto = 1;
    }
  else if (set_endian_string == endian_little)
    {
      struct gdbarch_info info;
      target_byte_order_auto = 0;
      gdbarch_info_init (&info);
      info.byte_order = BFD_ENDIAN_LITTLE;
      if (! gdbarch_update_p (info))
	printf_unfiltered (_("Little endian target not supported by GDB\n"));
    }
  else if (set_endian_string == endian_big)
    {
      struct gdbarch_info info;
      target_byte_order_auto = 0;
      gdbarch_info_init (&info);
      info.byte_order = BFD_ENDIAN_BIG;
      if (! gdbarch_update_p (info))
	printf_unfiltered (_("Big endian target not supported by GDB\n"));
    }
  else
    internal_error (__FILE__, __LINE__,
		    _("set_endian: bad value"));
  show_endian (gdb_stdout, from_tty, NULL, NULL);
}

/* Functions to manipulate the architecture of the target */

enum set_arch { set_arch_auto, set_arch_manual };

static int target_architecture_auto = 1;

static const char *set_architecture_string;

const char *
selected_architecture_name (void)
{
  if (target_architecture_auto)
    return NULL;
  else
    return set_architecture_string;
}

/* Called if the user enters ``show architecture'' without an
   argument. */

static void
show_architecture (struct ui_file *file, int from_tty,
		   struct cmd_list_element *c, const char *value)
{
  const char *arch;
  arch = TARGET_ARCHITECTURE->printable_name;
  if (target_architecture_auto)
    fprintf_filtered (file, _("\
The target architecture is set automatically (currently %s)\n"), arch);
  else
    fprintf_filtered (file, _("\
The target architecture is assumed to be %s\n"), arch);
}


int
set_architecture_from_string (char *new_arch)
{
  set_architecture_string = xstrdup (new_arch);
  struct gdbarch_info info;
  gdbarch_info_init (&info);
  info.bfd_arch_info = bfd_scan_arch (set_architecture_string);
  if (info.bfd_arch_info == NULL)
    internal_error (__FILE__, __LINE__,
		    _("set_architecture: bfd_scan_arch failed"));
  if (gdbarch_update_p (info))
    target_architecture_auto = 0;
  else
    {
      warning("Architecture `%s' not recognized.\n",
		       set_architecture_string);
      return 0;
    }
  return 1;
}

/* Called if the user enters ``set architecture'' with or without an
   argument. */

static void
set_architecture (char *ignore_args, int from_tty, struct cmd_list_element *c)
{
  if (strcmp (set_architecture_string, "auto") == 0)
    {
      target_architecture_auto = 1;
    }
  else
    {
      struct gdbarch_info info;
      gdbarch_info_init (&info);
      info.bfd_arch_info = bfd_scan_arch (set_architecture_string);
      if (info.bfd_arch_info == NULL)
	internal_error (__FILE__, __LINE__,
			_("set_architecture: bfd_scan_arch failed"));
      if (gdbarch_update_p (info))
	target_architecture_auto = 0;
      else
	printf_unfiltered (_("Architecture `%s' not recognized.\n"),
			   set_architecture_string);
    }
  show_architecture (gdb_stdout, from_tty, NULL, NULL);
}

/* Try to select a global architecture that matches "info".  Return
   non-zero if the attempt succeds.  */
int
gdbarch_update_p (struct gdbarch_info info)
{
  struct gdbarch *new_gdbarch = gdbarch_find_by_info (info);

  /* If there no architecture by that name, reject the request.  */
  if (new_gdbarch == NULL)
    {
      if (gdbarch_debug)
	fprintf_unfiltered (gdb_stdlog, "gdbarch_update_p: "
			    "Architecture not found\n");
      return 0;
    }

  /* If it is the same old architecture, accept the request (but don't
     swap anything).  */
  if (new_gdbarch == current_gdbarch)
    {
      if (gdbarch_debug)
	fprintf_unfiltered (gdb_stdlog, "gdbarch_update_p: "
			    "Architecture 0x%08lx (%s) unchanged\n",
			    (long) new_gdbarch,
			    gdbarch_bfd_arch_info (new_gdbarch)->printable_name);
      return 1;
    }

  /* It's a new architecture, swap it in.  */
  if (gdbarch_debug)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_update_p: "
			"New architecture 0x%08lx (%s) selected\n",
			(long) new_gdbarch,
			gdbarch_bfd_arch_info (new_gdbarch)->printable_name);
  deprecated_current_gdbarch_select_hack (new_gdbarch);

  return 1;
}

/* Return the architecture for ABFD.  If no suitable architecture
   could be find, return NULL.  */

struct gdbarch *
gdbarch_from_bfd (bfd *abfd)
{
  struct gdbarch_info info;

  gdbarch_info_init (&info);
  info.abfd = abfd;
  return gdbarch_find_by_info (info);
}

/* Set the dynamic target-system-dependent parameters (architecture,
   byte-order) using information found in the BFD */

void
set_gdbarch_from_file (bfd *abfd)
{
  struct gdbarch *gdbarch;

  gdbarch = gdbarch_from_bfd (abfd);
  if (gdbarch == NULL)
    error (_("Architecture of file not recognized."));
#if defined (TARGET_ARM) && defined (TM_NEXTSTEP)
  /* APPLE LOCAL: We may have a arm binary with a lesser CPU subtype running on
     a more capable device. We need to pick the highest of the two architectures
     and go with that.  */
  if (!target_architecture_auto && gdbarch != current_gdbarch)
    {
      const struct bfd_arch_info * abfd_bfd_arch_info;
      const struct bfd_arch_info * curr_bfd_arch_info;
      const struct bfd_arch_info * compatible_bfd_arch_info;
      abfd_bfd_arch_info = bfd_get_arch_info (abfd);
      curr_bfd_arch_info = gdbarch_bfd_arch_info (current_gdbarch);
      if (abfd_bfd_arch_info != NULL && curr_bfd_arch_info != NULL)
	{
	  compatible_bfd_arch_info = bfd_default_compatible (abfd_bfd_arch_info, 
							 curr_bfd_arch_info);
	  if (compatible_bfd_arch_info == NULL)
	    {
	      warning ("Architecture of file \"%s\" (%s) is not compatible "
		       "with the user selected architecture (%s).",
		       abfd->filename ? abfd->filename : "<UNKNOWN>",
		       curr_bfd_arch_info->printable_name,
		       abfd_bfd_arch_info->printable_name);
	    }
	}
      /* Only select the gdbarch from ABFD if it was the compatible one.  */
      if (compatible_bfd_arch_info == abfd_bfd_arch_info)
	deprecated_current_gdbarch_select_hack (gdbarch);
    }

#else
  if (!target_architecture_auto && gdbarch != current_gdbarch)
    error ("Architecture of file \"%s\" does not match user selected architecture.",
	   abfd->filename ? abfd->filename : "<UNKNOWN>" );
  deprecated_current_gdbarch_select_hack (gdbarch);
#endif
}

/* Initialize the current architecture.  Update the ``set
   architecture'' command so that it specifies a list of valid
   architectures.  */

/* APPLE LOCAL default to the first powerpc arch */
#define bfd_powerpc_arch bfd_powerpc_archs[0]

#ifdef DEFAULT_BFD_ARCH
extern const bfd_arch_info_type DEFAULT_BFD_ARCH;
static const bfd_arch_info_type *default_bfd_arch = &DEFAULT_BFD_ARCH;
#else
static const bfd_arch_info_type *default_bfd_arch;
#endif

#ifdef DEFAULT_BFD_VEC
extern const bfd_target DEFAULT_BFD_VEC;
static const bfd_target *default_bfd_vec = &DEFAULT_BFD_VEC;
#else
static const bfd_target *default_bfd_vec;
#endif

void
initialize_current_architecture (void)
{
  const char **arches = gdbarch_printable_names ();

  /* determine a default architecture and byte order. */
  struct gdbarch_info info;
  gdbarch_info_init (&info);
  
  /* Find a default architecture. */
  if (info.bfd_arch_info == NULL
      && default_bfd_arch != NULL)
    info.bfd_arch_info = default_bfd_arch;
  if (info.bfd_arch_info == NULL)
    {
      /* Choose the architecture by taking the first one
	 alphabetically. */
      const char *chosen = arches[0];
      const char **arch;
      for (arch = arches; *arch != NULL; arch++)
	{
	  if (strcmp (*arch, chosen) < 0)
	    chosen = *arch;
	}
      if (chosen == NULL)
	internal_error (__FILE__, __LINE__,
			_("initialize_current_architecture: No arch"));
      info.bfd_arch_info = bfd_scan_arch (chosen);
      if (info.bfd_arch_info == NULL)
	internal_error (__FILE__, __LINE__,
			_("initialize_current_architecture: Arch not found"));
    }

  /* Take several guesses at a byte order.  */
  if (info.byte_order == BFD_ENDIAN_UNKNOWN
      && default_bfd_vec != NULL)
    {
      /* Extract BFD's default vector's byte order. */
      switch (default_bfd_vec->byteorder)
	{
	case BFD_ENDIAN_BIG:
	  info.byte_order = BFD_ENDIAN_BIG;
	  break;
	case BFD_ENDIAN_LITTLE:
	  info.byte_order = BFD_ENDIAN_LITTLE;
	  break;
	default:
	  break;
	}
    }
  if (info.byte_order == BFD_ENDIAN_UNKNOWN)
    {
      /* look for ``*el-*'' in the target name. */
      const char *chp;
      chp = strchr (target_name, '-');
      if (chp != NULL
	  && chp - 2 >= target_name
	  && strncmp (chp - 2, "el", 2) == 0)
	info.byte_order = BFD_ENDIAN_LITTLE;
    }
  if (info.byte_order == BFD_ENDIAN_UNKNOWN)
    {
      /* Wire it to big-endian!!! */
      info.byte_order = BFD_ENDIAN_BIG;
    }

  if (! gdbarch_update_p (info))
    internal_error (__FILE__, __LINE__,
		    _("initialize_current_architecture: Selection of "
		      "initial architecture failed"));

  /* Create the ``set architecture'' command appending ``auto'' to the
     list of architectures. */
  {
    /* Append ``auto''. */
    int nr;
    for (nr = 0; arches[nr] != NULL; nr++);
    arches = xrealloc (arches, sizeof (char*) * (nr + 2));
    arches[nr + 0] = "auto";
    arches[nr + 1] = NULL;
    add_setshow_enum_cmd ("architecture", class_support,
			  arches, &set_architecture_string, _("\
Set architecture of target."), _("\
Show architecture of target."), NULL,
			  set_architecture, show_architecture,
			  &setlist, &showlist);
    add_alias_cmd ("processor", "architecture", class_support, 1, &setlist);
  }
}


/* Initialize a gdbarch info to values that will be automatically
   overridden.  Note: Originally, this ``struct info'' was initialized
   using memset(0).  Unfortunately, that ran into problems, namely
   BFD_ENDIAN_BIG is zero.  An explicit initialization function that
   can explicitly set each field to a well defined value is used.  */

void
gdbarch_info_init (struct gdbarch_info *info)
{
  memset (info, 0, sizeof (struct gdbarch_info));
  info->byte_order = BFD_ENDIAN_UNKNOWN;
  info->osabi = GDB_OSABI_UNINITIALIZED;
}

/* Similar to init, but this time fill in the blanks.  Information is
   obtained from the specified architecture, global "set ..." options,
   and explicitly initialized INFO fields.  */

void
gdbarch_info_fill (struct gdbarch *gdbarch, struct gdbarch_info *info)
{
  /* APPLE LOCAL: I moved the "info->abfd" test before the gdbarch
     one.  If somebody passed us in a bfd in the info, they probably
     meant for us to use that first...  The problem with doing it the
     other way around is that if there is a user selected
     architecture, you will always just get that, so you can't ask the
     question "does the architecture of THIS BFD match the currently
     selected gdbarch".  That's a useful question to have
     answered.  */
  if (info->bfd_arch_info == NULL
      && info->abfd != NULL
      && bfd_get_arch (info->abfd) != bfd_arch_unknown
      && bfd_get_arch (info->abfd) != bfd_arch_obscure)
    info->bfd_arch_info = bfd_get_arch_info (info->abfd);

  /* "(gdb) set architecture ...".  */
  if (info->bfd_arch_info == NULL
      && !target_architecture_auto
      && gdbarch != NULL)
    info->bfd_arch_info = gdbarch_bfd_arch_info (gdbarch);
  if (info->bfd_arch_info == NULL
      && gdbarch != NULL)
    info->bfd_arch_info = gdbarch_bfd_arch_info (gdbarch);

  /* "(gdb) set byte-order ...".  */
  if (info->byte_order == BFD_ENDIAN_UNKNOWN
      && !target_byte_order_auto
      && gdbarch != NULL)
    info->byte_order = gdbarch_byte_order (gdbarch);
  /* From the INFO struct.  */
  if (info->byte_order == BFD_ENDIAN_UNKNOWN
      && info->abfd != NULL)
    info->byte_order = (bfd_big_endian (info->abfd) ? BFD_ENDIAN_BIG
		       : bfd_little_endian (info->abfd) ? BFD_ENDIAN_LITTLE
		       : BFD_ENDIAN_UNKNOWN);
  /* From the current target.  */
  if (info->byte_order == BFD_ENDIAN_UNKNOWN
      && gdbarch != NULL)
    info->byte_order = gdbarch_byte_order (gdbarch);

  /* "(gdb) set osabi ...".  Handled by gdbarch_lookup_osabi.  */
  if (info->osabi == GDB_OSABI_UNINITIALIZED)
    info->osabi = gdbarch_lookup_osabi (info->abfd);
  if (info->osabi == GDB_OSABI_UNINITIALIZED
      && gdbarch != NULL)
    info->osabi = gdbarch_osabi (gdbarch);

  /* Must have at least filled in the architecture.  */
  gdb_assert (info->bfd_arch_info != NULL);
}

/* */

extern initialize_file_ftype _initialize_gdbarch_utils; /* -Wmissing-prototypes */

void
_initialize_gdbarch_utils (void)
{
  add_setshow_enum_cmd ("endian", class_support,
			endian_enum, &set_endian_string, _("\
Set endianness of target."), _("\
Show endianness of target."), NULL,
			set_endian, show_endian,
			&setlist, &showlist);
}

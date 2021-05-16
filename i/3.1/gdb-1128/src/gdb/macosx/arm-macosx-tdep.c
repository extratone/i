/* Mac OS X support for GDB, the GNU debugger.
   Copyright 2005
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


/* When we're doing native debugging, and we attach to a process,
   we start out by finding the in-memory dyld -- the osabi of that
   dyld is stashed away here for use when picking the right osabi of
   a fat file.  In the case of cross-debugging, none of this happens
   and this global remains untouched.  */

#include "defs.h"
#include "frame.h"
#include "inferior.h"
#include "symtab.h"
#include "target.h"
#include "gdbcore.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdbcmd.h"
#include "arch-utils.h"
#include "floatformat.h"
#include "gdbtypes.h"
#include "regcache.h"
#include "reggroups.h"
#include "frame-base.h"
#include "frame-unwind.h"
#include "dummy-frame.h"

#include "libbfd.h"

#include "arm-tdep.h"
#include "elf-bfd.h"
#include "dis-asm.h"
#include "gdbarch.h"
#include "osabi.h"

#include <mach-o/nlist.h>
#include <mach/mach.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include "mach-o.h" /* for BFD mach definitions.  */

/* A boolean indicating if we must use software single stepping. Some 
   targets may not support using IMVA mismatching for a variety of reasons,
   and some remote targets may not implement the "s" packet.  */
static enum gdb_osabi arm_mach_o_osabi_sniffer_use_dyld_hint (bfd *abfd);
static void arm_macosx_init_abi (struct gdbarch_info info,
                                 struct gdbarch *gdbarch);

static void arm_macosx_init_abi_v6 (struct gdbarch_info info,
                                    struct gdbarch *gdbarch);

/* Built in type for displaying ARM PSR and FPSCR register contents.  */
struct type *builtin_type_arm_psr = NULL;
struct type *builtin_type_arm_fpscr = NULL;

/* VFPv1 registers.  */
static register_info_t g_reginfo_arm_vfpv1[] =
{
  { "r0",   0, &builtin_type_int32 },
  { "r1",   0, &builtin_type_int32 },
  { "r2",   0, &builtin_type_int32 },
  { "r3",   0, &builtin_type_int32 },
  { "r4",   0, &builtin_type_int32 },
  { "r5",   0, &builtin_type_int32 },
  { "r6",   0, &builtin_type_int32 },
  { "r7",   0, &builtin_type_int32 },
  { "r8",   0, &builtin_type_int32 },
  { "r9",   0, &builtin_type_int32 },
  { "r10",  0, &builtin_type_int32 },
  { "r11",  0, &builtin_type_int32 },
  { "r12",  0, &builtin_type_int32 },
  { "sp",   0, &builtin_type_int32 },
  { "lr",   0, &builtin_type_int32 },
  { "pc",   0, &builtin_type_int32 },
/* Set f0-f7 and fps reg names blank so they don't show up in 
   "info all-registers" or "info float" commands. We reserve the register
   numbers and register cache space for them so we can maintian FSF 
   gdbserver compatability. If these register do need to be displayed, we 
   can re-set the names to valid values using a new command.  */
  { "",	    0, &builtin_type_arm_ext_littlebyte_bigword },
  { "",	    0, &builtin_type_arm_ext_littlebyte_bigword },
  { "",	    0, &builtin_type_arm_ext_littlebyte_bigword },
  { "",	    0, &builtin_type_arm_ext_littlebyte_bigword },
  { "",	    0, &builtin_type_arm_ext_littlebyte_bigword },
  { "",	    0, &builtin_type_arm_ext_littlebyte_bigword },
  { "",	    0, &builtin_type_arm_ext_littlebyte_bigword },
  { "",	    0, &builtin_type_arm_ext_littlebyte_bigword },
  { "",	    0, &builtin_type_arm_ext_littlebyte_bigword }, 
  { "cpsr", 0, &builtin_type_arm_psr },

  /* VFPv1 registers.  */
  { "s0",   0, &builtin_type_ieee_single_little },
  { "s1",   0, &builtin_type_ieee_single_little }, 
  { "s2",   0, &builtin_type_ieee_single_little },
  { "s3",   0, &builtin_type_ieee_single_little },
  { "s4",   0, &builtin_type_ieee_single_little },
  { "s5",   0, &builtin_type_ieee_single_little },
  { "s6",   0, &builtin_type_ieee_single_little },
  { "s7",   0, &builtin_type_ieee_single_little },
  { "s8",   0, &builtin_type_ieee_single_little },
  { "s9",   0, &builtin_type_ieee_single_little },
  { "s10",  0, &builtin_type_ieee_single_little },
  { "s11",  0, &builtin_type_ieee_single_little },
  { "s12",  0, &builtin_type_ieee_single_little },
  { "s13",  0, &builtin_type_ieee_single_little },
  { "s14",  0, &builtin_type_ieee_single_little },
  { "s15",  0, &builtin_type_ieee_single_little },
  { "s16",  0, &builtin_type_ieee_single_little },
  { "s17",  0, &builtin_type_ieee_single_little },
  { "s18",  0, &builtin_type_ieee_single_little },
  { "s19",  0, &builtin_type_ieee_single_little },
  { "s20",  0, &builtin_type_ieee_single_little },
  { "s21",  0, &builtin_type_ieee_single_little },
  { "s22",  0, &builtin_type_ieee_single_little },
  { "s23",  0, &builtin_type_ieee_single_little },
  { "s24",  0, &builtin_type_ieee_single_little },
  { "s25",  0, &builtin_type_ieee_single_little },
  { "s26",  0, &builtin_type_ieee_single_little },
  { "s27",  0, &builtin_type_ieee_single_little },
  { "s28",  0, &builtin_type_ieee_single_little },
  { "s29",  0, &builtin_type_ieee_single_little },
  { "s30",  0, &builtin_type_ieee_single_little },
  { "s31",  0, &builtin_type_ieee_single_little },
  { "fpscr",0, &builtin_type_arm_fpscr },
  
  /* VFPv1 pseudo registers.  */
  { "d0",   0, &builtin_type_ieee_double_little },
  { "d1",   0, &builtin_type_ieee_double_little },
  { "d2",   0, &builtin_type_ieee_double_little },
  { "d3",   0, &builtin_type_ieee_double_little },
  { "d4",   0, &builtin_type_ieee_double_little },
  { "d5",   0, &builtin_type_ieee_double_little },
  { "d6",   0, &builtin_type_ieee_double_little },
  { "d7",   0, &builtin_type_ieee_double_little },
  { "d8",   0, &builtin_type_ieee_double_little },
  { "d9",   0, &builtin_type_ieee_double_little },
  { "d10",  0, &builtin_type_ieee_double_little },
  { "d11",  0, &builtin_type_ieee_double_little },
  { "d12",  0, &builtin_type_ieee_double_little },
  { "d13",  0, &builtin_type_ieee_double_little },
  { "d14",  0, &builtin_type_ieee_double_little },
  { "d15",  0, &builtin_type_ieee_double_little }
};
const uint32_t g_reginfo_arm_vfpv1_count = sizeof(g_reginfo_arm_vfpv1)/
					   sizeof(register_info_t);

/* VFPv3 registers.  */
static register_info_t g_reginfo_arm_vfpv3[] =
{
  { "r0",   0, &builtin_type_int32 },
  { "r1",   0, &builtin_type_int32 },
  { "r2",   0, &builtin_type_int32 },
  { "r3",   0, &builtin_type_int32 },
  { "r4",   0, &builtin_type_int32 },
  { "r5",   0, &builtin_type_int32 },
  { "r6",   0, &builtin_type_int32 },
  { "r7",   0, &builtin_type_int32 },
  { "r8",   0, &builtin_type_int32 },
  { "r9",   0, &builtin_type_int32 },
  { "r10",  0, &builtin_type_int32 },
  { "r11",  0, &builtin_type_int32 },
  { "r12",  0, &builtin_type_int32 },
  { "sp",   0, &builtin_type_int32 },
  { "lr",   0, &builtin_type_int32 },
  { "pc",   0, &builtin_type_int32 },
/* Set f0-f7 and fps reg names blank so they don't show up in 
   "info all-registers" or "info float" commands. We reserve the register
   numbers and register cache space for them so we can maintian FSF 
   gdbserver compatability. If these register do need to be displayed, we 
   can re-set the names to valid values using a new command.  */
  { "",	    0, &builtin_type_arm_ext_littlebyte_bigword },
  { "",	    0, &builtin_type_arm_ext_littlebyte_bigword },
  { "",	    0, &builtin_type_arm_ext_littlebyte_bigword },
  { "",	    0, &builtin_type_arm_ext_littlebyte_bigword },
  { "",	    0, &builtin_type_arm_ext_littlebyte_bigword },
  { "",	    0, &builtin_type_arm_ext_littlebyte_bigword },
  { "",	    0, &builtin_type_arm_ext_littlebyte_bigword },
  { "",	    0, &builtin_type_arm_ext_littlebyte_bigword },
  { "",	    0, &builtin_type_arm_ext_littlebyte_bigword }, 
  { "cpsr", 0, &builtin_type_arm_psr },
  
  /* VFPv1 registers.  */
  { "s0",   0, &builtin_type_ieee_single_little },
  { "s1",   0, &builtin_type_ieee_single_little }, 
  { "s2",   0, &builtin_type_ieee_single_little },
  { "s3",   0, &builtin_type_ieee_single_little },
  { "s4",   0, &builtin_type_ieee_single_little },
  { "s5",   0, &builtin_type_ieee_single_little },
  { "s6",   0, &builtin_type_ieee_single_little },
  { "s7",   0, &builtin_type_ieee_single_little },
  { "s8",   0, &builtin_type_ieee_single_little },
  { "s9",   0, &builtin_type_ieee_single_little },
  { "s10",  0, &builtin_type_ieee_single_little },
  { "s11",  0, &builtin_type_ieee_single_little },
  { "s12",  0, &builtin_type_ieee_single_little },
  { "s13",  0, &builtin_type_ieee_single_little },
  { "s14",  0, &builtin_type_ieee_single_little },
  { "s15",  0, &builtin_type_ieee_single_little },
  { "s16",  0, &builtin_type_ieee_single_little },
  { "s17",  0, &builtin_type_ieee_single_little },
  { "s18",  0, &builtin_type_ieee_single_little },
  { "s19",  0, &builtin_type_ieee_single_little },
  { "s20",  0, &builtin_type_ieee_single_little },
  { "s21",  0, &builtin_type_ieee_single_little },
  { "s22",  0, &builtin_type_ieee_single_little },
  { "s23",  0, &builtin_type_ieee_single_little },
  { "s24",  0, &builtin_type_ieee_single_little },
  { "s25",  0, &builtin_type_ieee_single_little },
  { "s26",  0, &builtin_type_ieee_single_little },
  { "s27",  0, &builtin_type_ieee_single_little },
  { "s28",  0, &builtin_type_ieee_single_little },
  { "s29",  0, &builtin_type_ieee_single_little },
  { "s30",  0, &builtin_type_ieee_single_little },
  { "s31",  0, &builtin_type_ieee_single_little },
  { "fpscr",0, &builtin_type_arm_fpscr },

  /* VFPv3 registers.  */
  { "d16",  0, &builtin_type_ieee_double_little },
  { "d17",  0, &builtin_type_ieee_double_little },
  { "d18",  0, &builtin_type_ieee_double_little },
  { "d19",  0, &builtin_type_ieee_double_little },
  { "d20",  0, &builtin_type_ieee_double_little },
  { "d21",  0, &builtin_type_ieee_double_little },
  { "d22",  0, &builtin_type_ieee_double_little },
  { "d23",  0, &builtin_type_ieee_double_little },
  { "d24",  0, &builtin_type_ieee_double_little },
  { "d25",  0, &builtin_type_ieee_double_little },
  { "d26",  0, &builtin_type_ieee_double_little },
  { "d27",  0, &builtin_type_ieee_double_little },
  { "d28",  0, &builtin_type_ieee_double_little },
  { "d29",  0, &builtin_type_ieee_double_little },
  { "d30",  0, &builtin_type_ieee_double_little },
  { "d31",  0, &builtin_type_ieee_double_little },

  /* VFPv1 pseudo registers.  */
  { "d0",   0, &builtin_type_ieee_double_little },
  { "d1",   0, &builtin_type_ieee_double_little },
  { "d2",   0, &builtin_type_ieee_double_little },
  { "d3",   0, &builtin_type_ieee_double_little },
  { "d4",   0, &builtin_type_ieee_double_little },
  { "d5",   0, &builtin_type_ieee_double_little },
  { "d6",   0, &builtin_type_ieee_double_little },
  { "d7",   0, &builtin_type_ieee_double_little },
  { "d8",   0, &builtin_type_ieee_double_little },
  { "d9",   0, &builtin_type_ieee_double_little },
  { "d10",  0, &builtin_type_ieee_double_little },
  { "d11",  0, &builtin_type_ieee_double_little },
  { "d12",  0, &builtin_type_ieee_double_little },
  { "d13",  0, &builtin_type_ieee_double_little },
  { "d14",  0, &builtin_type_ieee_double_little },
  { "d15",  0, &builtin_type_ieee_double_little },

  /* SIMD pseudo registers.  */
  { "q0",   0, &builtin_type_vec128 },
  { "q1",   0, &builtin_type_vec128 }, 
  { "q2",   0, &builtin_type_vec128 },
  { "q3",   0, &builtin_type_vec128 },
  { "q4",   0, &builtin_type_vec128 },
  { "q5",   0, &builtin_type_vec128 },
  { "q6",   0, &builtin_type_vec128 },
  { "q7",   0, &builtin_type_vec128 },
  { "q8",   0, &builtin_type_vec128 },
  { "q9",   0, &builtin_type_vec128 },
  { "q10",  0, &builtin_type_vec128 },
  { "q11",  0, &builtin_type_vec128 },
  { "q12",  0, &builtin_type_vec128 },
  { "q13",  0, &builtin_type_vec128 },
  { "q14",  0, &builtin_type_vec128 },
  { "q15",  0, &builtin_type_vec128 }
};
const uint32_t g_reginfo_arm_vfpv3_count = sizeof(g_reginfo_arm_vfpv3)/
					   sizeof(register_info_t);


/* Add PSR and FPSCR built in types for displaying register contents as
   bitfields.  */
static struct type *
build_builtin_type_arm_psr_mode_enum (void)
{
  static struct gdbtypes_enum_info mode_enums[] = {
    {"usr",	0x10 },
    {"fiq",	0x11 },
    {"irq",	0x12 },
    {"svc",	0x13 },
    {"dbg",	0x15 },	/* XScale debug mode.  */
    {"abt",	0x17 },
    {"und",	0x1d },
    {"sys",	0x1f }
  };
  uint32_t num_mode_enums = sizeof (mode_enums)/sizeof (mode_enums[0]);
  return build_builtin_enum ("_arm_ext_psr_mode_enum", 4, 
			     TYPE_FLAG_UNSIGNED, mode_enums, num_mode_enums);
}

static struct type *
build_builtin_type_arm_psr (void)
{
  struct gdbtypes_bitfield_info psr_bitfields[] = {
    /* Print entire value first and use the void data pointer
       so that the value gets displayed as hex by default. By giving
       the value an empty name, the register value can be assigned
       in expressions using "p $cpsr = 0x00000000, yet the bitfield
       values can still be accessed and modified individually.*/
    {"",	builtin_type_void_data_ptr,  31,  0 },
    {"n",	builtin_type_uint32,  31, 31 },
    {"z",	builtin_type_uint32,  30, 30 },
    {"c",	builtin_type_uint32,  29, 29 },
    {"v",	builtin_type_uint32,  28, 28 },
    {"q",	builtin_type_uint32,  27, 27 },
    {"j",	builtin_type_uint32,  24, 24 },
    {"ge",      builtin_type_uint32,  19, 16 },
    {"e",	builtin_type_uint32,   9,  9 },
    {"a",	builtin_type_uint32,   8,  8 },
    {"i",	builtin_type_uint32,   7,  7 },
    {"f",	builtin_type_uint32,   6,  6 },
    {"t",	builtin_type_uint32,   5,  5 },
    {"mode",    build_builtin_type_arm_psr_mode_enum (),   4,  0 },
  };

  uint32_t num_psr_bitfields = sizeof (psr_bitfields)/sizeof (psr_bitfields[0]);
  return build_builtin_bitfield ("_arm_ext_psr", 4, 
				psr_bitfields, num_psr_bitfields);
}

static struct type *
build_builtin_type_arm_fpscr (void)
{
  struct gdbtypes_bitfield_info fpscr_bitfields[] = {
    /* Print entire value first and use the void data pointer
       so that the value gets displayed as hex by default. By giving
       the value an empty name, the register value can be assigned
       in expressions using "p $fpscr = 0x00000000, yet the bitfield
       values can still be accessed and modified individually.*/
    {"",	builtin_type_void_data_ptr,  31,  0 },
    {"n",	builtin_type_uint32,  31, 31 },
    {"z",	builtin_type_uint32,  30, 30 },
    {"c",	builtin_type_uint32,  29, 29 },
    {"v",	builtin_type_uint32,  28, 28 },
    {"dn",      builtin_type_uint32,  25, 25 },
    {"fz",      builtin_type_uint32,  24, 24 },
    {"rmode",   builtin_type_uint32,  23, 22 },
    {"stride",  builtin_type_uint32,  21, 20 },
    {"len",     builtin_type_uint32,  18, 16 },
    {"ide",     builtin_type_uint32,  15, 15 },
    {"ixe",     builtin_type_uint32,  12, 12 },
    {"ufe",     builtin_type_uint32,  11, 11 },
    {"ofe",     builtin_type_uint32,  10, 10 },
    {"dze",     builtin_type_uint32,   9,  9 },
    {"ioe",     builtin_type_uint32,   8,  8 },
    {"idc",     builtin_type_uint32,   7,  7 },
    {"ixc",     builtin_type_uint32,   4,  4 },
    {"ufc",     builtin_type_uint32,   3,  3 },
    {"ofc",     builtin_type_uint32,   2,  2 },
    {"dzc",     builtin_type_uint32,   1,  1 },
    {"ioc",     builtin_type_uint32,   0,  0 }
  };
  uint32_t num_fpscr_bitfields = sizeof (fpscr_bitfields)/
				 sizeof (fpscr_bitfields[0]);
  return build_builtin_bitfield ("_arm_ext_fpscr", 4, 
				fpscr_bitfields, num_fpscr_bitfields);
}


enum gdb_osabi
arm_host_osabi ()
{
  host_basic_info_data_t info;
  mach_msg_type_number_t count;

  count = HOST_BASIC_INFO_COUNT;
  host_info (mach_host_self (), HOST_BASIC_INFO, (host_info_t) & info, &count);

  if (info.cpu_type == BFD_MACH_O_CPU_TYPE_ARM)
    {
      if (info.cpu_subtype == BFD_MACH_O_CPU_SUBTYPE_ARM_6)
	return GDB_OSABI_DARWINV6;
      if (info.cpu_subtype == BFD_MACH_O_CPU_SUBTYPE_ARM_7)
	return GDB_OSABI_DARWINV7;
      else
	return GDB_OSABI_DARWIN;
    }
  return GDB_OSABI_UNKNOWN;
}

enum gdb_osabi
arm_set_osabi_from_host_info ()
{
  struct gdbarch_info info;
  gdbarch_info_init (&info);
  gdbarch_info_fill (current_gdbarch, &info);
  info.byte_order = gdbarch_byte_order (current_gdbarch);
  info.osabi = arm_host_osabi ();
  
  if (info.osabi == GDB_OSABI_DARWINV7)
    info.bfd_arch_info = bfd_lookup_arch (bfd_arch_arm, bfd_mach_arm_7);
  else if (info.osabi == GDB_OSABI_DARWINV6)
    info.bfd_arch_info = bfd_lookup_arch (bfd_arch_arm, bfd_mach_arm_6);
  else if (info.osabi == GDB_OSABI_DARWIN)
    info.bfd_arch_info = bfd_lookup_arch (bfd_arch_arm, 0);

  if (info.osabi != GDB_OSABI_UNKNOWN)
    gdbarch_update_p (info);
  return TARGET_OSABI;
}

/* Two functions in one!  If this is a "bfd_archive" (read: a MachO fat file),
   recurse for each separate fork of the fat file.
   If this is not a fat file, detect whether the file is arm32 or arm64.
   Before either of these, check if we've already sniffed an appropriate
   OSABI from dyld (in the case of attaching to a process) and prefer that.  */

static enum gdb_osabi
arm_mach_o_osabi_sniffer (bfd *abfd)
{
  enum gdb_osabi ret;

  /* The way loading works on Darwin is that for FAT files, the
     fork corresponding to the host system will be the one used
     regardless of what the type of the main executable was.
     So the osabi has to be determined solely by the type of
     the host system.  */


  if (strcmp (bfd_get_target (abfd), "mach-o-le") == 0)
    {
      const bfd_arch_info_type *arch_info = bfd_get_arch_info (abfd);
      if (arch_info->arch == bfd_arch_arm)
	{
	  if (arch_info->mach == bfd_mach_arm_4T)
	    return GDB_OSABI_DARWIN;
	  else if (arch_info->mach == bfd_mach_arm_6)
	    return GDB_OSABI_DARWINV6;
	  else if (arch_info->mach == bfd_mach_arm_7)
	    return GDB_OSABI_DARWINV7;
	  else
	    return GDB_OSABI_DARWIN;
	}
    }

  /* However, if we are running a cross gdb, we won't know what
     the target architecture is.  FIXME - going to have to determine
     this somehow.  For now, fall back to looking at the binary.  */

  ret = arm_mach_o_osabi_sniffer_use_dyld_hint (abfd);
  if (ret == GDB_OSABI_DARWINV6 || ret == GDB_OSABI_DARWIN ||
      ret == GDB_OSABI_DARWINV7)
    return ret;

  if (bfd_check_format (abfd, bfd_archive))
    {
      enum gdb_osabi best = GDB_OSABI_UNKNOWN;
      enum gdb_osabi cur = GDB_OSABI_UNKNOWN;
      const enum gdb_osabi host_osabi = arm_host_osabi ();

      bfd *nbfd = NULL;
      while ((nbfd = bfd_openr_next_archived_file (abfd, nbfd)) != NULL)
        {
          if (!bfd_check_format (nbfd, bfd_object))
            continue;

          cur = arm_mach_o_osabi_sniffer (nbfd);
	  
	  if (cur == host_osabi)
	    return cur;
          else if (cur > best && cur < host_osabi)
	    best = cur;
        }
      return best;
    }

  if (!bfd_check_format (abfd, bfd_object))
    return GDB_OSABI_UNKNOWN;

  return GDB_OSABI_UNKNOWN;
}

/* If we're attaching to a process, we start by finding the dyld that
   is loaded and go from there.  So when we're selecting the OSABI,
   prefer the osabi of the actually-loaded dyld when we can.  */

static enum gdb_osabi
arm_mach_o_osabi_sniffer_use_dyld_hint (bfd *abfd)
{
  if (osabi_seen_in_attached_dyld == GDB_OSABI_UNKNOWN)
    return GDB_OSABI_UNKNOWN;

  bfd *nbfd = NULL;
  for (;;)
    {
      const bfd_arch_info_type *arch_info;
      nbfd = bfd_openr_next_archived_file (abfd, nbfd);

      if (nbfd == NULL)
        break;
      if (!bfd_check_format (nbfd, bfd_object))
        continue;
      arch_info = bfd_get_arch_info (nbfd);

      if (arch_info->arch == bfd_arch_arm)
	{
	  if (arch_info->mach == bfd_mach_arm_4T
	      && osabi_seen_in_attached_dyld == GDB_OSABI_DARWIN)
	    return GDB_OSABI_DARWIN;
	  
	  if (arch_info->mach == bfd_mach_arm_6
	      && osabi_seen_in_attached_dyld == GDB_OSABI_DARWINV6)
	    return GDB_OSABI_DARWINV6;

	  if (arch_info->mach == bfd_mach_arm_7
	      && osabi_seen_in_attached_dyld == GDB_OSABI_DARWINV7)
	    return GDB_OSABI_DARWINV7;
	}
    }

  return GDB_OSABI_UNKNOWN;
}

int
arm_macosx_in_switch_glue (CORE_ADDR pc)
{
  int retval;
  char *name;

  retval = find_pc_partial_function (pc, &name,  NULL, NULL);
  if (retval)
    {
      if (strstr (name, "__switch") == name)
	{
	  char *end = name + strlen ("__switch");
	  int len = strlen (end);
	  if ((len == 1 && *end == '8')
	      || (len == 2 
		  && ((*end == 'u' && *(end + 1) == '8')
		      || (*end == '1' && *(end + 1) == '6')
		      || (*end == '3' && *(end + 1) == '2'))))
	    {
	      return 1;
	    }
	}
    }
  return 0;
}


arm_macosx_tdep_inf_status_t arm_macosx_tdep_inf_status = { (CORE_ADDR)-1 };

int
arm_macosx_keep_going (CORE_ADDR stop_pc)
{
  int result = 0;
  if (stop_pc == arm_macosx_tdep_inf_status.macosx_half_step_pc)
    {
      /* Half way through a 32 bit thumb inst HW single step.  */
      result = 1;
    }
  else if (step_over_calls != STEP_OVER_NONE)
    {
      /* We are doing a step over.  */
      
      /* See if we are in the ARM/Thumb __switchXXX functions. If we are*/
      if (arm_macosx_in_switch_glue (stop_pc))
	result = 1;  
    }

  return result;
}

void *
arm_macosx_save_thread_inferior_status ()
{
  /* See if we have anything relevant to save?  */
  if (arm_macosx_tdep_inf_status.macosx_half_step_pc == (CORE_ADDR)-1)
    return NULL;
  
  arm_macosx_tdep_inf_status_t *tdep_inf_status;
  
  tdep_inf_status = XMALLOC (arm_macosx_tdep_inf_status_t);
  if (tdep_inf_status)
    tdep_inf_status->macosx_half_step_pc = 
				arm_macosx_tdep_inf_status.macosx_half_step_pc;
  return tdep_inf_status;
}

void
arm_macosx_restore_thread_inferior_status (void *tdep_inf_status)
{
  if (tdep_inf_status != NULL)
    {
      arm_macosx_tdep_inf_status.macosx_half_step_pc = 
	((arm_macosx_tdep_inf_status_t *)tdep_inf_status)->macosx_half_step_pc;
    }
}

void
arm_macosx_free_thread_inferior_status (void *tdep_inf_status)
{
  xfree (tdep_inf_status);
}

/* Get the ith function argument for the current function.  */
CORE_ADDR
arm_fetch_pointer_argument (struct frame_info *frame, int argi,
                            struct type *type)
{
  CORE_ADDR addr;

  addr = get_frame_register_unsigned (frame, argi);

  return addr;
}

#define submask(x) ((1L << ((x) + 1)) - 1)
#define bit(obj,st) ((((uint32_t)obj) >> ((uint32_t)st)) & (uint32_t)1)
#define bits(obj,st,fn) ((uint32_t)((((uint32_t)obj) >> ((uint32_t)st)) & submask (((uint32_t)fn) - ((uint32_t)st))))

/* Print interesting information about the floating point processor
   (if present) or emulator.  */
static void
arm_macosx_print_float_info_vfp (struct gdbarch *gdbarch, 
				 struct ui_file *file, 
				 struct frame_info *frame, const char *args)
{
  static const char* enabled_strings[2] = {"disabled", "enabled"};
  static const char* Rmode_strings[4] = {
    "Round to nearest (RN) mode",
    "Round towards plus infinity (RP) mode",
    "Round towards minus infinity (RM) mode",
    "Round towards zero (RZ) mode"
  };
  uint32_t fpscr = read_register (ARM_VFP_REGNUM_FPSCR);
  uint32_t b;
  printf (_("VFP fpscr = 0x%8.8x\n"), fpscr);
  printf (_("     N = %u  Set if comparison produces a less than result\n"),
	  bit (fpscr, 31));
  printf (_("     Z = %u  Set if comparison produces an equal result\n"), 
	  bit (fpscr, 30));
  printf (_("     C = %u  Set if comparison produces an equal, greater "
	  "than, or unordered result\n"), 
	  bit (fpscr, 29));
  printf (_("     V = %u  Set if comparison produces an unordered result\n"), 
	  bit (fpscr, 28));
  b = bit (fpscr, 25);
  printf (_("    DN = %u  default NaN mode %s\n"), b, enabled_strings[b]);
  b = bit (fpscr, 24);
  printf (_("    Fz = %u  flush-to-zero mode %s\n"), b, enabled_strings[b]);
  b = bits (fpscr, 22, 23);
  printf (_(" Rmode = %u  %s\n"), b, Rmode_strings[b]);
  printf (_("Stride = %u\n"), bits (fpscr, 20, 21));
  printf (_("   LEN = %u\n"), bits (fpscr, 16, 18));
  printf (_("   IDE = %u  Input Subnormal exception\n"), bit (fpscr, 15));
  printf (_("   IXE = %u  Inexact exception\n"), bit (fpscr, 12));
  printf (_("   UFE = %u  Underflow exception\n"), bit (fpscr, 11));
  printf (_("   OFE = %u  Overflow exception\n"), bit (fpscr, 10));
  printf (_("   DZE = %u  Division by Zero exception\n"), bit (fpscr, 9));
  printf (_("   IOE = %u  Invalid Operation exception\n"), bit (fpscr, 8));
  printf (_("   IDC = %u  Input Subnormal cumulative\n"), bit (fpscr, 7));
  printf (_("   IXC = %u  Inexact cumulative\n"), bit (fpscr, 4));
  printf (_("   UFC = %u  Underflow cumulative\n"), bit (fpscr, 3));
  printf (_("   OFC = %u  Overflow cumulative\n"), bit (fpscr, 2));
  printf (_("   DZC = %u  Division by Zero cumulative\n"), bit (fpscr, 1));
  printf (_("   IOC = %u  Invalid Operation cumulative\n"), bit (fpscr, 0));
}

static void
arm_macosx_pseudo_register_read_vfpv1 (struct gdbarch *gdbarch, 
				       struct regcache *regcache, int reg, 
				       gdb_byte *buf)
{
  int s_reg_lsw = 2 * (reg - ARM_VFPV1_PSEUDO_REGNUM_D0) + ARM_VFP_REGNUM_S0;
  int s_reg_msw = s_reg_lsw + 1;
  regcache_cooked_read (regcache, s_reg_lsw, buf);
  regcache_cooked_read (regcache, s_reg_msw, buf + 4);
}

static void
arm_macosx_pseudo_register_write_vfpv1 (struct gdbarch *gdbarch,
				        struct regcache *regcache, int reg, 
					const gdb_byte *buf)
{
  int s_reg_lsw = 2 * (reg - ARM_VFPV1_PSEUDO_REGNUM_D0) + ARM_VFP_REGNUM_S0;
  int s_reg_msw = s_reg_lsw + 1;
  regcache_cooked_write (regcache, s_reg_lsw, buf);
  regcache_cooked_write (regcache, s_reg_msw, buf + 4);
}

static void
arm_macosx_pseudo_register_read_vfpv3 (struct gdbarch *gdbarch, 
				       struct regcache *regcache, int reg, 
				       gdb_byte *buf)
{
  int s_reg_lsw = 0;
  int s_reg_msw = 0;
  int regno;
  int stride_byte_size = 0;
  if (reg >= ARM_VFPV3_PSEUDO_REGNUM_D0 && reg <= ARM_VFPV3_PSEUDO_REGNUM_D15)
    {
      /* D0- D15 overlap with the values for S0 - S31 where 2 consecutive S
         registers make up a D register.  */
      int d = reg - ARM_VFPV3_PSEUDO_REGNUM_D0;
      s_reg_lsw = 2 * d + ARM_VFP_REGNUM_S0;
      s_reg_msw = s_reg_lsw + 1;
      
      /* Set the stride byte size to be 4 as each pseudo consecutive S register
         is 4 bytes in size.  */
      stride_byte_size = 4;
    }
  else if (reg >= ARM_SIMD_PSEUDO_REGNUM_Q0 && 
	   reg <= ARM_SIMD_PSEUDO_REGNUM_Q15)
    {
      int q = reg - ARM_SIMD_PSEUDO_REGNUM_Q0;
      if (q < 8)
	{
	  /* Q0-Q7 overlap with the values for S0-S31 where 4 consecutive S
	     registers make up a Q register.  */
	  s_reg_lsw = 4 * q + ARM_VFP_REGNUM_S0;
	  s_reg_msw = s_reg_lsw + 3;
	  /* Set the stride byte size to be 4 as each pseudo Q register will
	     overlap 4 4 byte consecutive S registers.  */
	  stride_byte_size = 4;
	}
      else
	{
	  /* Q8-Q15 overlap with the values for D15-D31 where 2 consecutive D
	     registers make up a Q register.  */
	  s_reg_lsw = 2 * (q - 8) + ARM_VFPV3_REGNUM_D16;
	  s_reg_msw = s_reg_lsw + 1;
	  /* Set the stride byte size to be 8 as each pseudo Q register will
	     overlap 2 8 byte consecutive D registers.  */
	  stride_byte_size = 8;
	}
    }
    
  for (regno=s_reg_lsw; regno<=s_reg_msw; regno++)
    regcache_cooked_read (regcache, regno, 
			  buf + (stride_byte_size * (regno - s_reg_lsw)));
}

static void
arm_macosx_pseudo_register_write_vfpv3 (struct gdbarch *gdbarch, 
					struct regcache *regcache, int reg, 
					const gdb_byte *buf)
{
  int s_reg_lsw = 0;
  int s_reg_msw = 0;
  int regno;
  int stride_byte_size = 0;
  if (reg >= ARM_VFPV3_PSEUDO_REGNUM_D0 && reg <= ARM_VFPV3_PSEUDO_REGNUM_D15)
    {
      /* D0- D15 overlap with the values for S0 - S31 where 2 consecutive S
         registers make up a D register.  */
      int d = reg - ARM_VFPV3_PSEUDO_REGNUM_D0;
      s_reg_lsw = 2 * d + ARM_VFP_REGNUM_S0;
      s_reg_msw = s_reg_lsw + 1;
      
      /* Set the stride byte size to be 4 as each pseudo consecutive S register
         is 4 bytes in size.  */
      stride_byte_size = 4;
    }
  else if (reg >= ARM_SIMD_PSEUDO_REGNUM_Q0 && 
	   reg <= ARM_SIMD_PSEUDO_REGNUM_Q15)
    {
      int q = reg - ARM_SIMD_PSEUDO_REGNUM_Q0;
      if (q < 8)
	{
	  /* Q0-Q7 overlap with the values for S0-S31 where 4 consecutive S
	     registers make up a Q register.  */
	  s_reg_lsw = 4 * q + ARM_VFP_REGNUM_S0;
	  s_reg_msw = s_reg_lsw + 3;
	  /* Set the stride byte size to be 4 as each pseudo Q register will
	     overlap 4 4 byte consecutive S registers.  */
	  stride_byte_size = 4;
	}
      else
	{
	  /* Q8-Q15 overlap with the values for D15-D31 where 2 consecutive D
	     registers make up a Q register.  */
	  s_reg_lsw = 2 * (q - 8) + ARM_VFPV3_REGNUM_D16;
	  s_reg_msw = s_reg_lsw + 1;
	  /* Set the stride byte size to be 8 as each pseudo Q register will
	     overlap 2 8 byte consecutive D registers.  */
	  stride_byte_size = 8;
	}
    }
    
  for (regno=s_reg_lsw; regno<=s_reg_msw; regno++)
    regcache_cooked_write (regcache, regno, 
			   buf + (stride_byte_size * (regno - s_reg_lsw)));
}

/* This is cribbed from arm-tdep.c.  I don't want to add all the mach-o 
   code to that file, since then I'll have to deal with merge conflicts,
   but I need this bit.  */

/*
 * The GDB_N_ARM_THUMB_DEF bit of the n_desc field indicates that the symbol is
 * a defintion of a Thumb function.
 */
#define GDB_N_ARM_THUMB_DEF	0x0008 /* symbol is a Thumb function (ARM) */


#define MSYMBOL_SET_SPECIAL(msym)					\
	MSYMBOL_INFO (msym) = (char *) (((long) MSYMBOL_INFO (msym))	\
					| 0x80000000)
static void
arm_macosx_dbx_make_msymbol_special (int16_t desc, struct minimal_symbol *msym)
{
  if (desc & GDB_N_ARM_THUMB_DEF)
    MSYMBOL_SET_SPECIAL (msym);
}

/* Convert a dbx stab register number (from `r' declaration) to a gdb
   REGNUM. */
int
arm_macosx_stab_reg_to_regnum (int num)
{
  int regnum = num;

  /* Check for the VFP floating point registers numbers.  */
  if (num >= ARM_MACOSX_FIRST_VFP_STABS_REGNUM 
      && num <= ARM_MACOSX_LAST_VFP_STABS_REGNUM)
    regnum = ARM_VFP_REGNUM_S0 + num - ARM_MACOSX_FIRST_VFP_STABS_REGNUM;
  else
    regnum = num; /* Most registers do not need any modification.  */
    
  return regnum;
}

/* Grub around in the argument list to find the exception object,
   and return the type info string (without the "typeinfo for " bits).
   CURR_FRAME is the __cxa_throw frame.
   NOTE: We are getting the mangled name of the typeinfo object, and
   demangling that.  We could instead look inside the object, and pull
   out the string description field, but then we have to know where this
   is in the typeinfo object, or call a function.  Getting the mangled
   name seems much safer & easier.
*/

char *
arm_throw_catch_find_typeinfo (struct frame_info *curr_frame,
                               int exception_type)
{
  struct minimal_symbol *typeinfo_sym = NULL;
  ULONGEST typeinfo_ptr;
  char *typeinfo_str;

  if (exception_type == EX_EVENT_THROW)
    {
      frame_unwind_unsigned_register (curr_frame,
                                      ARM_R0_REGNUM + 1,
                                      &typeinfo_ptr);
      typeinfo_sym = lookup_minimal_symbol_by_pc (typeinfo_ptr);

    }
  else
    {
      /* This is hacky, the runtime code gets a pointer to an _Unwind_Exception,
         which is actually contained in the __cxa_exception that we want.  But
         the function that does the cast is a static inline, so we can't see it.
         FIXME: we need to get the runtime to keep this so we aren't relying on
         the particular layout of the __cxa_exception...
         Anyway, then the first field of __cxa_exception is the type object. */
      ULONGEST type_obj_addr = 0;

      frame_unwind_unsigned_register (curr_frame,
                                      ARM_R0_REGNUM,
                                      &typeinfo_ptr);

      /* This is also a bit bogus.  We assume that an unsigned integer is the
         same size as an address on our system.  */
      if (safe_read_memory_unsigned_integer
          (typeinfo_ptr - 44, 4, &type_obj_addr))
        typeinfo_sym = lookup_minimal_symbol_by_pc (type_obj_addr);
    }

  if (!typeinfo_sym)
    return NULL;

  typeinfo_str =
    typeinfo_sym->ginfo.language_specific.cplus_specific.demangled_name;
  if ((typeinfo_str == NULL)
      || (strstr (typeinfo_str, "typeinfo for ") != typeinfo_str))
    return NULL;

  return typeinfo_str + strlen ("typeinfo for ");
}

static void
arm_macosx_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  /* We actually don't have any software float registers, so lets remove 
     the float info printer so we don't crash on "info float" commands.  */
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  tdep->fp_model = ARM_FLOAT_NONE;
  tdep->vfp_version = ARM_VFP_UNSUPPORTED;

  set_gdbarch_print_float_info (gdbarch, NULL);
  set_gdbarch_stab_reg_to_regnum (gdbarch, arm_macosx_stab_reg_to_regnum);

  set_gdbarch_skip_trampoline_code (gdbarch, macosx_skip_trampoline_code);

  set_gdbarch_in_solib_return_trampoline (gdbarch,
                                          macosx_in_solib_return_trampoline);
  set_gdbarch_fetch_pointer_argument (gdbarch, arm_fetch_pointer_argument);

  set_gdbarch_num_regs (gdbarch, ARM_MACOSX_NUM_REGS);

  set_gdbarch_dbx_make_msymbol_special (gdbarch, arm_macosx_dbx_make_msymbol_special);
  
  if (get_arm_single_step_mode () == arm_single_step_mode_auto)
    {
     /* I don't believe that any ARMv4 devices will be able to use our hardware
	single stepping methods, but we should check on this once we get 
	some.  */
    }
}

static struct type *
arm_macosx_register_type_vfpv1 (struct gdbarch *gdbarch, int regnum)
{
  /* APPLE LOCAL: Use register info table.   */
  if (regnum < g_reginfo_arm_vfpv1_count && g_reginfo_arm_vfpv1[regnum].type)
    return *g_reginfo_arm_vfpv1[regnum].type;
  return builtin_type_int32;
}

static struct type *
arm_macosx_register_type_vfpv3 (struct gdbarch *gdbarch, int regnum)
{
  /* APPLE LOCAL: Use register info table.   */
  if (regnum < g_reginfo_arm_vfpv3_count && g_reginfo_arm_vfpv3[regnum].type)
    return *g_reginfo_arm_vfpv3[regnum].type;
  return builtin_type_int32;
}

/* Return the ARM register name corresponding to register REGNUM.  */
static const char *
arm_macosx_register_name_vfpv1 (int regnum)
{
  if (regnum < g_reginfo_arm_vfpv1_count)
    return g_reginfo_arm_vfpv1[regnum].name;
  return NULL;
}

/* Return the ARM register name corresponding to register REGNUM.  */
static const char *
arm_macosx_register_name_vfpv3 (int regnum)
{
  if (regnum < g_reginfo_arm_vfpv3_count)
    return g_reginfo_arm_vfpv3[regnum].name;
  return NULL;
}

/* Index within `registers' of the first byte of the space for
   register N.  */

static int
arm_macosx_register_byte_vfpv1 (int regnum)
{
  /* APPLE LOCAL: Use register info table.   */
  if (regnum < g_reginfo_arm_vfpv1_count)
    return g_reginfo_arm_vfpv1[regnum].offset;
  return 0;
}

static int
arm_macosx_register_byte_vfpv3 (int regnum)
{
  /* APPLE LOCAL: Use register info table.   */
  if (regnum < g_reginfo_arm_vfpv3_count)
    return g_reginfo_arm_vfpv3[regnum].offset;
  return 0;
}

static void
arm_macosx_init_abi_v6 (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  /* Set the floating point model to be VFP and also initialize the
     stab register number converter.  */
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  tdep->fp_model = ARM_FLOAT_VFP;
  tdep->vfp_version = ARM_VFP_VERSION_1;
  set_gdbarch_stab_reg_to_regnum (gdbarch, arm_macosx_stab_reg_to_regnum);

  set_gdbarch_skip_trampoline_code (gdbarch, macosx_skip_trampoline_code);

  set_gdbarch_in_solib_return_trampoline (gdbarch,
                                          macosx_in_solib_return_trampoline);
  set_gdbarch_fetch_pointer_argument (gdbarch, arm_fetch_pointer_argument);

  set_gdbarch_deprecated_fp_regnum (gdbarch, ARM_FP_REGNUM);
  set_gdbarch_print_float_info (gdbarch, arm_macosx_print_float_info_vfp);
  set_gdbarch_sp_regnum (gdbarch, ARM_SP_REGNUM);
  set_gdbarch_pc_regnum (gdbarch, ARM_PC_REGNUM);
  set_gdbarch_register_name (gdbarch, arm_macosx_register_name_vfpv1);
  set_gdbarch_deprecated_register_byte (gdbarch, 
					arm_macosx_register_byte_vfpv1);
  set_gdbarch_num_regs (gdbarch, ARM_V6_MACOSX_NUM_REGS);
  set_gdbarch_num_pseudo_regs (gdbarch, ARM_MACOSX_NUM_VFPV1_PSEUDO_REGS);
  set_gdbarch_pseudo_register_read (gdbarch, 
				    arm_macosx_pseudo_register_read_vfpv1);
  set_gdbarch_pseudo_register_write (gdbarch, 
				     arm_macosx_pseudo_register_write_vfpv1);
  set_gdbarch_register_type (gdbarch, arm_macosx_register_type_vfpv1);

  set_gdbarch_dbx_make_msymbol_special (gdbarch, 
					arm_macosx_dbx_make_msymbol_special);

  /* Disable software single stepping unless otherwise requested.  */
  if (get_arm_single_step_mode () == arm_single_step_mode_auto)
    {
#ifdef NM_NEXTSTEP
      /* Check a built in sysctl for the number of supported hardware
         breakpoint registers on native builds.  */
      uint32_t num_hw_bkpts = 0;
      size_t num_hw_bkpts_len = sizeof(num_hw_bkpts);
      if (sysctlbyname("hw.optional.breakpoint", &num_hw_bkpts, 
		       &num_hw_bkpts_len, NULL, 0) == 0)
	{
	  if (num_hw_bkpts > 0)
	    set_gdbarch_software_single_step (gdbarch, NULL);
	}
      else
	{
	  /* Use hardware single stepping by default for armv6.  */
	  set_gdbarch_software_single_step (gdbarch, NULL);
	}
#else
      /* Assume we have a remote connection to debugserver which can now
         do single stepping.  */
      set_gdbarch_software_single_step (gdbarch, NULL);
#endif
    }
}

static void
arm_macosx_init_abi_v7 (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  /* Set the floating point model to be VFP and also initialize the
     stab register number converter.  */
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  tdep->fp_model = ARM_FLOAT_VFP;
  tdep->vfp_version = ARM_VFP_VERSION_3;
  set_gdbarch_stab_reg_to_regnum (gdbarch, arm_macosx_stab_reg_to_regnum);

  set_gdbarch_skip_trampoline_code (gdbarch, macosx_skip_trampoline_code);

  set_gdbarch_in_solib_return_trampoline (gdbarch,
                                          macosx_in_solib_return_trampoline);
  set_gdbarch_fetch_pointer_argument (gdbarch, arm_fetch_pointer_argument);

  set_gdbarch_deprecated_fp_regnum (gdbarch, ARM_FP_REGNUM);
  set_gdbarch_print_float_info (gdbarch, arm_macosx_print_float_info_vfp);
  set_gdbarch_sp_regnum (gdbarch, ARM_SP_REGNUM);
  set_gdbarch_pc_regnum (gdbarch, ARM_PC_REGNUM);
  set_gdbarch_register_name (gdbarch, arm_macosx_register_name_vfpv3);
  set_gdbarch_deprecated_register_byte (gdbarch, 
					arm_macosx_register_byte_vfpv3);
  set_gdbarch_num_regs (gdbarch, ARM_V7_MACOSX_NUM_REGS);
  set_gdbarch_num_pseudo_regs (gdbarch, ARM_MACOSX_NUM_VFPV3_PSEUDO_REGS);
  set_gdbarch_pseudo_register_read (gdbarch, 
				    arm_macosx_pseudo_register_read_vfpv3);
  set_gdbarch_pseudo_register_write (gdbarch, 
				     arm_macosx_pseudo_register_write_vfpv3);
  set_gdbarch_register_type (gdbarch, arm_macosx_register_type_vfpv3);

  set_gdbarch_dbx_make_msymbol_special (gdbarch, 
					arm_macosx_dbx_make_msymbol_special);

  /* Disable software single stepping unless otherwise requested.  */
  if (get_arm_single_step_mode () == arm_single_step_mode_auto)
    {
#ifdef NM_NEXTSTEP
      /* Check a built in sysctl for the number of supported hardware
         breakpoint registers on native builds.  */
      uint32_t num_hw_bkpts = 0;
      size_t num_hw_bkpts_len = sizeof(num_hw_bkpts);
      if (sysctlbyname("hw.optional.breakpoint", &num_hw_bkpts, 
		       &num_hw_bkpts_len, NULL, 0) == 0)
	{
	  if (num_hw_bkpts > 0)
	    set_gdbarch_software_single_step (gdbarch, NULL);
	}
      else
	{
	  /* Use hardware single stepping by default for armv7.  */
	  /* Disable hardware single stepping on armv7 for now due to 
	     some issues with our current silicon where the debug registers
	     weren't hooked up.
	  set_gdbarch_software_single_step (gdbarch, NULL);
	  */
	}
#else
      /* Assume we have a remote connection to debugserver which can now
         do single stepping.  */
      set_gdbarch_software_single_step (gdbarch, NULL);
#endif
    }
}

void
_initialize_arm_macosx_tdep ()
{
  uint32_t i;

  /* Initialize ARM PSR and FPSCR built in types.  */
  builtin_type_arm_psr = build_builtin_type_arm_psr ();
  builtin_type_arm_fpscr = build_builtin_type_arm_fpscr ();

  /* Calculcate the offsets to be used by arm_macosx_register_byte_vfpv1.  */
  g_reginfo_arm_vfpv1[0].offset = 0;
  for (i=1; i<g_reginfo_arm_vfpv1_count; i++)
    {
      if (g_reginfo_arm_vfpv1[i-1].type)
	g_reginfo_arm_vfpv1[i].offset = g_reginfo_arm_vfpv1[i-1].offset + 
                                 TYPE_LENGTH (*g_reginfo_arm_vfpv1[i-1].type);
    }

  /* Calculcate the offsets to be used by arm_macosx_register_byte_vfpv3.  */
  g_reginfo_arm_vfpv3[0].offset = 0;
  for (i=1; i<g_reginfo_arm_vfpv3_count; i++)
    {
      if (g_reginfo_arm_vfpv3[i-1].type)
	g_reginfo_arm_vfpv3[i].offset = g_reginfo_arm_vfpv3[i-1].offset + 
                                 TYPE_LENGTH (*g_reginfo_arm_vfpv3[i-1].type);
    }

  /* This is already done in arm-tdep.c.  I wonder if we shouldn't move this 
     code into there so we can be sure all the initializations happen in the
     right order, etc.  */

  /* register_gdbarch_init (bfd_arch_arm, arm_gdbarch_init); */

  gdbarch_register_osabi_sniffer (bfd_arch_unknown, bfd_target_mach_o_flavour,
                                  arm_mach_o_osabi_sniffer);

  gdbarch_register_osabi (bfd_arch_arm, 
			  0, 
			  GDB_OSABI_DARWIN,
                          arm_macosx_init_abi);

  gdbarch_register_osabi ((bfd_lookup_arch (bfd_arch_arm, bfd_mach_arm_4T))->arch, 
			  bfd_mach_arm_4T,
			  GDB_OSABI_DARWIN,
                          arm_macosx_init_abi);

  gdbarch_register_osabi ((bfd_lookup_arch (bfd_arch_arm, bfd_mach_arm_6))->arch, 
			  bfd_mach_arm_6,
                          GDB_OSABI_DARWINV6, 
			  arm_macosx_init_abi_v6);

  /* Use the ARM_MACOSX_INIT_ABI_V6 function for armv7 as well.  */
  gdbarch_register_osabi ((bfd_lookup_arch (bfd_arch_arm, bfd_mach_arm_7))->arch, 
			  bfd_mach_arm_7,
                          GDB_OSABI_DARWINV7, 
			  arm_macosx_init_abi_v7);

}

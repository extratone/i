/* Common target dependent code for GDB on ARM systems.

   Copyright 1988, 1989, 1991, 1992, 1993, 1995, 1996, 1998, 1999,
   2000, 2001, 2002, 2003, 2004, 2005 Free Software Foundation, Inc.

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

#include <ctype.h>		/* XXX for isupper () */

#include "defs.h"
#include "frame.h"
#include "inferior.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "gdb_string.h"
#include "dis-asm.h"		/* For register styles. */
#include "regcache.h"
#include "reggroups.h"
#include "doublest.h"
#include "value.h"
#include "arch-utils.h"
#include "osabi.h"
#include "frame-unwind.h"
#include "frame-base.h"
#include "trad-frame.h"
#include "objfiles.h"
#include "dwarf2-frame.h"

#include "arm-tdep.h"
#include "gdb/sim-arm.h"

#include "elf-bfd.h"
#include "coff/internal.h"
#include "elf/arm.h"

#include "gdb_assert.h"

/* Support routines for single stepping.  Calculate the next PC value.  */
#define submask(x) ((1L << ((x) + 1)) - 1)
#define bit(obj,st) (((obj) >> (st)) & 1)
#define bits(obj,st,fn) (((obj) >> (st)) & submask ((fn) - (st)))
#define sbits(obj,st,fn) \
  ((long) (bits(obj,st,fn) | ((long) bit(obj,fn) * ~ submask (fn - st))))
#define BranchDest(addr,instr) \
  ((CORE_ADDR) (((long) (addr)) + 8 + (sbits (instr, 0, 23) << 2)))
#define ARM_PC_32 1
#define IS_THUMB32_OP(op) (((op) & 0xe000) == 0xe000 && bits ((op), 11, 12) != 0)
static int arm_debug;
static void
show_arm_debug (struct ui_file *file, int from_tty,
		struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("ARM debugging is %s.\n"), value);
}

/* Each OS has a different mechanism for accessing the various
   registers stored in the sigcontext structure.

   SIGCONTEXT_REGISTER_ADDRESS should be defined to the name (or
   function pointer) which may be used to determine the addresses
   of the various saved registers in the sigcontext structure.

   For the ARM target, there are three parameters to this function. 
   The first is the pc value of the frame under consideration, the
   second the stack pointer of this frame, and the last is the
   register number to fetch.  

   If the tm.h file does not define this macro, then it's assumed that
   no mechanism is needed and we define SIGCONTEXT_REGISTER_ADDRESS to
   be 0. 
   
   When it comes time to multi-arching this code, see the identically
   named machinery in ia64-tdep.c for an example of how it could be
   done.  It should not be necessary to modify the code below where
   this macro is used.  */

#ifdef SIGCONTEXT_REGISTER_ADDRESS
#ifndef SIGCONTEXT_REGISTER_ADDRESS_P
#define SIGCONTEXT_REGISTER_ADDRESS_P() 1
#endif
#else
#define SIGCONTEXT_REGISTER_ADDRESS(SP,PC,REG) 0
#define SIGCONTEXT_REGISTER_ADDRESS_P() 0
#endif

/* Macros for setting and testing a bit in a minimal symbol that marks
   it as Thumb function.  The MSB of the minimal symbol's "info" field
   is used for this purpose.

   MSYMBOL_SET_SPECIAL	Actually sets the "special" bit.
   MSYMBOL_IS_SPECIAL   Tests the "special" bit in a minimal symbol.  */

#define MSYMBOL_SET_SPECIAL(msym)					\
	MSYMBOL_INFO (msym) = (char *) (((long) MSYMBOL_INFO (msym))	\
					| 0x80000000)

#define MSYMBOL_IS_SPECIAL(msym)				\
	(((long) MSYMBOL_INFO (msym) & 0x80000000) != 0)

/* The list of available "set arm ..." and "show arm ..." commands.  */
static struct cmd_list_element *setarmcmdlist = NULL;
static struct cmd_list_element *showarmcmdlist = NULL;

/* The type of floating-point to use.  Keep this in sync with enum
   arm_float_model, and the help string in _initialize_arm_tdep.  */
static const char *fp_model_strings[] =
{
  "auto",
  "softfpa",
  "fpa",
  "softvfp",
  "vfp",
  "none",
  NULL
};

/* A variable that can be configured by the user.  */
static enum arm_float_model arm_fp_model = ARM_FLOAT_AUTO;
static const char *current_fp_model = "auto";

/* The ABI to use.  Keep this in sync with arm_abi_kind.  */
static const char *arm_abi_strings[] =
{
  "auto",
  "APCS",
  "AAPCS",
  NULL
};

/* A variable that can be configured by the user.  */
static enum arm_abi_kind arm_abi_global = ARM_ABI_AUTO;
static const char *arm_abi_string = "auto";

/* Number of different reg name sets (options).  */
static int num_disassembly_options;

/* The offsets in all REGISTER_INFO structs in g_register_info get initialized 
   in _initialize_arm_tdep() using the "struct type" information from the
   previous register.  */

static register_info_t g_register_info[] =
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
  { "f0",   0, &builtin_type_arm_ext_littlebyte_bigword },
  { "f1",   0, &builtin_type_arm_ext_littlebyte_bigword },
  { "f2",   0, &builtin_type_arm_ext_littlebyte_bigword },
  { "f3",   0, &builtin_type_arm_ext_littlebyte_bigword },
  { "f4",   0, &builtin_type_arm_ext_littlebyte_bigword },
  { "f5",   0, &builtin_type_arm_ext_littlebyte_bigword },
  { "f6",   0, &builtin_type_arm_ext_littlebyte_bigword },
  { "f7",   0, &builtin_type_arm_ext_littlebyte_bigword },
  { "fps",  0, &builtin_type_uint32 },
  { "cpsr", 0, &builtin_type_uint32 }
};

const uint32_t g_register_info_count = sizeof(g_register_info)/
				       sizeof(register_info_t);

/* Valid register name styles.  */
static const char **valid_disassembly_styles;

/* Disassembly style to use. Default to "std" register names.  */
static const char *disassembly_style;

/* Temp variable used by the "set/show arm show-opcode-bytes" for whether
   we are to show opcode bytes when displaying disassembly.  */
static int show_opcode_bytes = 0;

/* Allow hardware single stepping to be enabled/disabled.  */

/* The type of floating-point to use.  Keep this in sync with enum
   arm_float_model, and the help string in _initialize_arm_tdep.  */
static const char *arm_single_step_mode_strings[] =
{
  "auto",
  "software",
  "hardware",
  NULL
};

static int arm_single_step_mode = arm_single_step_mode_auto;
static const char *arm_single_step_mode_str = "auto";

/* Index to that option in the opcodes table.  */
static int current_option;

/* This is used to keep the bfd arch_info in sync with the disassembly
   style.  */
static void set_disassembly_style_sfunc(char *, int,
					 struct cmd_list_element *);

static void arm_set_show_opcode_bytes (char *args, int from_tty, 
				       struct cmd_list_element *c);

static void set_disassembly_style (void);

static void convert_from_extended (const struct floatformat *, const void *,
				   void *);
static void convert_to_extended (const struct floatformat *, void *,
				 const void *);

struct arm_prologue_cache
{
  /* The stack pointer at the time this frame was created; i.e. the
     caller's stack pointer when this function was called.  It is used
     to identify this frame.  */
  CORE_ADDR prev_sp;
  
  /* Also known as the start of the function.  */
  CORE_ADDR prologue_start; 

  /* The frame base for this frame is just prev_sp + frame offset -
     frame size.  FRAMESIZE is the size of this stack frame, and
     FRAMEOFFSET if the initial offset from the stack pointer (this
     frame's stack pointer, not PREV_SP) to the frame base.  */

  int framesize;
  int frameoffset;

  /* The register used to hold the frame pointer for this frame.  */
  int framereg;

  /* The unwound frame pointer value, or zero if not available.  */
  CORE_ADDR prev_fp;
  
  int prev_pc_is_thumb;	/* previous function is a thumb function.  */

  /* Saved register offsets.  */
  struct trad_frame_saved_reg *saved_regs;
};

typedef struct arm_prologue_cache arm_prologue_cache_t;

arm_prologue_cache_t *
get_arm_prologue_cache (struct frame_info *frame)
{
  arm_prologue_cache_t *arm_frame_cache = NULL;

  if (frame)
    {
      enum frame_type frame_type = get_frame_type (frame);
      if (frame_type != DUMMY_FRAME)
	{
	  void **frame_cache = frame_cache_hack (frame);
	  if (frame_cache)
	    arm_frame_cache = *frame_cache;
	}
    }

  return arm_frame_cache;
}


/* APPLE LOCAL START: Centralize the prologue scan and skip code.

   I reworked the way the ARM and Thumb function prologues were
   implemented since the the skip and scan prologue functions were not
   using the same code. Now we build an array for all prologue opcodes
   we are interested in parsing. Each table entry is a arm_opcode_info_t
   structure that contains a mask to apply to the opcode, and a value to
   compare it to, and if equal, we can look at the flags for that entry
   to be able to tell more about the opcode. Each entry can have an
   optional callback function that can called when scanning the
   prologue. A description member is also available for debugging and
   logging purposes. This change allows the stack prologue skipping and
   parsing to use the same data so the two can't get out of date. It
   should also make it easier to add new opcodes and maintain existing
   ones.
 */

/* Maintain any state needed by the arm_scan_prolog_ftype callback
   functions in this structure to keep the number are arguments for 
   the callback down.  */
#define MAX_NUM_ARM_REGISTERS ARM_VFPV3_REGNUM_D31
struct arm_prologue_state
{
  CORE_ADDR pc;	  /* Current PC value for instruction.  */
  int sp_offset;
  int fp_offset;
  int ip_offset;
  int findmask;
  int reg_saved_in_reg[MAX_NUM_ARM_REGISTERS];
  CORE_ADDR reg_loaded_from_address[MAX_NUM_ARM_REGISTERS];
};

#define THUMB_PROLOGUE_PUSH	1
#define THUMB_PROLOGUE_FP_SETUP	2
#define THUMB_PROLOGUE_SUB_SP	4
#define THUMB_PROLOGUE_ALL	7
typedef struct arm_prologue_state arm_prologue_state_t;

/* Callback definition for prologue instruction scanning functions 
   used in the arm_opcode_info_tag structure. The return value is
   to be one of the prolog_XXX values described below.  */
typedef int (arm_scan_prolog_ftype) (const uint32_t insn, 
				     arm_prologue_cache_t *cache,
				     arm_prologue_state_t *state);

/* 
  Definitions for the arm_opcode_info_tag TYPE member.
  
  prolog_ignore - Ignore any of these opcodes as they could be before,
		  in the middle, or after valid prologue opcodes.
		       
  prolog_yes    - The instruction is normally a vital part of a function 
		  prologue. The SCAN_PROLOG function should decode all 
		  necessary info about any modifications to the framesize,
		  frameoffset, what and where registers are saved and anything
		  else that enables us to build our frame cache.
		      
  prolog_no     - Instructions that can't be in a prologue. We will 
		  currently scan until we hit the prologue limit length
		  (MAX_ARM_PROLOGUE_SIZE bytes for ARM and 
		  MAX_THUMB_PROLOGUE_SIZE for Thumb) so that we can deal
		  with assembly functions that do some sanity checking on
		  args and bail before creating a proper stack frame.
 */

enum
{
  prolog_no = 0,
  prolog_ignore,
  prolog_yes,
};

enum
{
  ARMv4	    = (1 << 0),
  ARMv4T    = (1 << 1),
  ARMv5T    = (1 << 2),
  ARMv5TE   = (1 << 3),
  ARMv5TEJ  = (1 << 4),
  ARMv6	    = (1 << 5),
  ARMv6K    = (1 << 6),
  ARMv6T2   = (1 << 7),
  ARMv7	    = (1 << 8),
};

#define ARM_ALL_VARIANTS  (ARMv4|ARMv4T|ARMv5T|ARMv5TE|ARMv5TEJ|ARMv6|ARMv6K|ARMv6T2|ARMv7)
#define ARMV4T_AND_ABOVE  (ARMv4T|ARMv5T|ARMv5TE|ARMv5TEJ|ARMv6|ARMv6K|ARMv6T2|ARMv7)
#define ARMV5_AND_ABOVE	  (ARMv5T|ARMv5TE|ARMv5TEJ|ARMv6|ARMv6K|ARMv6T2|ARMv7)
#define ARMV6T2_AND_ABOVE (ARMv6T2|ARMv7)
struct arm_opcode_info_tag
{
  uint32_t mask;	/* Mask to apply to the instruction.  */
  uint32_t value;	/* Value to compare to the masked instruction.  */
  arm_scan_prolog_ftype *scan_prolog;
  uint32_t variant_mask; /* Mask of bits that indicate which variants support this opcode.  */
  const char *description;
};

typedef struct arm_opcode_info_tag arm_opcode_info_t;

static uint32_t data_proc_immediate (const uint32_t insn);

/* Prologue scanning function declarations.  */

static int arm_scan_prolog_insn_stmfd_sp (const uint32_t insn, 
					  arm_prologue_cache_t *cache,
					  arm_prologue_state_t *state);

static int arm_scan_prolog_insn_data_proc_imm (const uint32_t insn, 
					       arm_prologue_cache_t *cache,
					       arm_prologue_state_t *state);

static int arm_scan_prolog_insn_mov_ip_sp (const uint32_t insn, 
					   arm_prologue_cache_t *cache,
					   arm_prologue_state_t *state);

static int arm_scan_prolog_insn_str_rd_sp (const uint32_t insn, 
					   arm_prologue_cache_t *cache,
					   arm_prologue_state_t *state);

static int arm_scan_prolog_insn_stmfd_sp (const uint32_t insn, 
					  arm_prologue_cache_t *cache,
					  arm_prologue_state_t *state);

static int arm_scan_prolog_insn_stfe_fn_sp_minus_12 (const uint32_t insn, 
						     arm_prologue_cache_t *cache,
						     arm_prologue_state_t *state);

static int arm_scan_prolog_insn_sfmfd_fn_4_sp (const uint32_t insn, 
					       arm_prologue_cache_t *cache,
					       arm_prologue_state_t *state);

static int arm_scan_prolog_insn_fstmdb (const uint32_t insn, 
					arm_prologue_cache_t *cache,
					arm_prologue_state_t *state);

static int thumb_scan_prolog_insn_push (const uint32_t insn, 
					arm_prologue_cache_t *cache,
					arm_prologue_state_t *state);

static int thumb_scan_prolog_insn_sub4_sp_imm (const uint32_t insn, 
					       arm_prologue_cache_t *cache,
					       arm_prologue_state_t *state);


static int thumb_scan_prolog_insn_add6_r7_sp (const uint32_t insn, 
					      arm_prologue_cache_t *cache,
					      arm_prologue_state_t *state);
					       
static int thumb_scan_prolog_insn_add_sp_rm (const uint32_t insn, 
					     arm_prologue_cache_t *cache,
					     arm_prologue_state_t *state);

static int thumb_scan_prolog_insn_ldr_rd_pc_rel (const uint32_t insn, 
						 arm_prologue_cache_t *cache,
						 arm_prologue_state_t *state);

static int thumb_scan_prolog_insn_mov_r7_sp (const uint32_t insn, 
					     arm_prologue_cache_t *cache,
					     arm_prologue_state_t *state);

static int thumb_scan_prolog_insn_mov_rlo_rhi (const uint32_t insn, 
					       arm_prologue_cache_t *cache,
					       arm_prologue_state_t *state);

static int thumb2_scan_prolog_insn_stmfd_sp (const uint32_t insn, 
					     arm_prologue_cache_t *cache,
					     arm_prologue_state_t *state);

static int thumb2_scan_prolog_insn_push_w (const uint32_t insn, 
					   arm_prologue_cache_t *cache,
					   arm_prologue_state_t *state);

static int thumb2_scan_prolog_insn_push_w_rt (const uint32_t insn, 
					      arm_prologue_cache_t *cache,
					      arm_prologue_state_t *state);

static int thumb2_scan_prolog_insn_vpush (const uint32_t insn, 
					  arm_prologue_cache_t *cache,
					  arm_prologue_state_t *state);

static int thumb2_scan_prolog_insn_sub_sp_const (const uint32_t insn, 
						 arm_prologue_cache_t *cache,
						 arm_prologue_state_t *state);

static int thumb2_scan_prolog_insn_sub_sp_imm12 (const uint32_t insn, 
						 arm_prologue_cache_t *cache,
						 arm_prologue_state_t *state);

static int thumb_scan_prolog_insn_blx (const uint32_t insn, 
				       arm_prologue_cache_t *cache,
				       arm_prologue_state_t *state);

static int 
scan_prolog_insn_return_no (const uint32_t insn, 
			    arm_prologue_cache_t *cache,
			    arm_prologue_state_t *state)
{
  /* Any opcodes that we know couldn't be in a prologue can use this as their
     SCAN_PROLOG callback.  */
  return prolog_no;
}

/* ARM Prologue Instruction Information
   This table gets used by arm_macosx_scan_prologue and 
   arm_macosx_skip_prologue so the two functions don't get out of sync. 
   Each entry consists of:
   - A mask to apply to a potential prologue instruction.
   - A value to compare it to after it is masked.
   - Information about if the instruction is, is not, or could be part
     of a prologue.
   - An optional callback function for scanning the prologue instruction.
   - A description for debugging and logging this table
   
   This should allow stack prologue scan and skipping to be easily 
   maintained and updated for new opcodes.  */

static arm_opcode_info_t arm_opcode_info[] =
{
/*  mask        value       scan_prolog                                 variants          description
    ----------  ----------  ------------------------------------------  ---------------   ------------------  */
  { 0xffff0000, 0xe92d0000, arm_scan_prolog_insn_stmfd_sp,		ARM_ALL_VARIANTS, "stmfd sp!,{...}"},
  { 0xfffff000, 0xe24c7000, arm_scan_prolog_insn_data_proc_imm,		ARM_ALL_VARIANTS, "sub r7, ip, #n"},
  { 0xfffff000, 0xe28d7000, arm_scan_prolog_insn_data_proc_imm,		ARM_ALL_VARIANTS, "add r7, sp, #n"},
  { 0xfffff000, 0xe24dd000, arm_scan_prolog_insn_data_proc_imm,		ARM_ALL_VARIANTS, "sub sp, sp, #n"},
  { 0xfffff000, 0xe28dc000, arm_scan_prolog_insn_data_proc_imm,		ARM_ALL_VARIANTS, "add ip, sp, #n"},
  { 0xffffffff, 0xe24dc000, arm_scan_prolog_insn_data_proc_imm,		ARM_ALL_VARIANTS, "sub ip, sp, #n"},
  { 0xffffffff, 0xe1a0c00d, arm_scan_prolog_insn_mov_ip_sp,		ARM_ALL_VARIANTS, "mov ip, sp"},
  { 0xffff0000, 0xe52d0000, arm_scan_prolog_insn_str_rd_sp,		ARM_ALL_VARIANTS, "str Rd, [sp, #-n]!"},
  { 0xffbf0fff, 0xec2d0200, arm_scan_prolog_insn_sfmfd_fn_4_sp,		ARM_ALL_VARIANTS, "sfmfd fn, <cnt>, [sp]!"},
  { 0xffff8fff, 0xed6d0103, arm_scan_prolog_insn_stfe_fn_sp_minus_12,	ARM_ALL_VARIANTS, "stfe fn, [sp, #-12]!"},
  { 0xffbf0e00, 0xed2d0a00, arm_scan_prolog_insn_fstmdb,		ARM_ALL_VARIANTS, "fstmdb sp!, {...}"},
  /* After all possible prologue instructions, specify any instructions that
     can't be in a prologue.  */
  { 0x0f000000, 0x0b000000, scan_prolog_insn_return_no,			ARM_ALL_VARIANTS, "bl"},
  { 0xfe000000, 0xfa000000, scan_prolog_insn_return_no,			ARMV5_AND_ABOVE,  "blx(1)"},
  { 0x0ffffff0, 0x012fff30, scan_prolog_insn_return_no,			ARMV5_AND_ABOVE,  "blx(2)"},
  { 0xfe200000, 0xe8200000, scan_prolog_insn_return_no,			ARM_ALL_VARIANTS, "ldm"}
};

#define ARM_OPCOPE_INFO_COUNT	(sizeof (arm_opcode_info)/sizeof (arm_opcode_info_t))



/* Thumb Prologue Instruction Information
   This table gets used by thumb_macosx_scan_prologue and 
   thumb_macosx_skip_prologue so the two functions don't get out of
   sync. Each entry consists of:
   - A mask to apply to a potential prologue instruction.
   - A value to compare it to after it is masked.
   - Information about if the instruction is, is not, or could be part
     of a prologue.
   - An optional callback function for scanning the prologue instruction.
   - A description for debugging and logging this table
   
   This should allow stack prologue scan and skipping to be easily 
   maintained and updated for new opcodes.  */

static arm_opcode_info_t thumb_opcode_info[] =  
{
/*  mask        value	    scan_prolog                           variants          description
    ----------  ----------  ------------------------------------- ----------------  --------------------------------  */
  { 0xfffffe00, 0xb400    , thumb_scan_prolog_insn_push,          ARMV4T_AND_ABOVE, "push<c> <registers> (encoding T1)"},
  { 0xffffff80, 0xb080    , thumb_scan_prolog_insn_sub4_sp_imm,   ARMV4T_AND_ABOVE, "sub(4) sp, #imm"},
  { 0xffffff00, 0xaf00    , thumb_scan_prolog_insn_add6_r7_sp,    ARMV4T_AND_ABOVE, "add(6) r7, sp, #imm"},
  { 0xffffff87, 0x4485    , thumb_scan_prolog_insn_add_sp_rm,     ARMV4T_AND_ABOVE, "add(4) sp, <Rm>"},
  { 0xfffff800, 0x4800    , thumb_scan_prolog_insn_ldr_rd_pc_rel, ARMV4T_AND_ABOVE, "ldr(3) <Rd>, [PC, #imm]"},
  { 0xffffffff, 0x466f    , thumb_scan_prolog_insn_mov_r7_sp,     ARMV4T_AND_ABOVE, "mov r7, sp"},
  { 0xffffffc0, 0x4640    , thumb_scan_prolog_insn_mov_rlo_rhi,   ARMV4T_AND_ABOVE, "mov r0-r7, r8-r15"},
  /* Thumb2 */
//{ 0xfe5f0000, 0xe80d0000, thumb2_scan_prolog_insn_stmia_sp,	  ARMV6T2_AND_ABOVE, "stmia, sp!,{...}"},
  { 0xffffa000, 0xe92d0000, thumb2_scan_prolog_insn_stmfd_sp,	  ARMV6T2_AND_ABOVE, "stmfd, sp!,{...}"},
  { 0xffffa000, 0xe8ad0000, thumb2_scan_prolog_insn_push_w,	  ARMV6T2_AND_ABOVE, "push<c>.w <registers> (encoding T2)"},  
  { 0xffff0fff, 0xf84d0d04, thumb2_scan_prolog_insn_push_w_rt,	  ARMV6T2_AND_ABOVE, "push<c>.w <registers> (encoding T3)"},  
  { 0xffbf0e00, 0xed2d0a00, thumb2_scan_prolog_insn_vpush,	  ARMV6T2_AND_ABOVE, "vpush<c> <list> (encodings T1/A1 and T2/A2)"},  
  { 0xfbef8f00, 0xf1ad0d00, thumb2_scan_prolog_insn_sub_sp_const, ARMV6T2_AND_ABOVE, "sub{s}<c>.w SP,SP,#<const> (encoding T2)"},
  { 0xfbff8f00, 0xf2ad0d00, thumb2_scan_prolog_insn_sub_sp_imm12, ARMV6T2_AND_ABOVE, "sub{s}<c>.w SP,SP,#<imm12> (encoding T3)"},

  /* Check target address and make sure it isn't something that saves s, d or
     q regs more effieciently by switching to ARM mode.  */
  { 0xf800e800, 0xf000e800, thumb_scan_prolog_insn_blx,		  ARMV4T_AND_ABOVE, "bl, blx <target_addr>"},
  
  /* After all possible prologue instructions, specify any instructions that
     can't be in a prologue.  */
  { 0xffffff80, 0xb000    , scan_prolog_insn_return_no,		  ARMV4T_AND_ABOVE, "add(7) sp, #imm"},
  { 0xfffffe00, 0xbc00    , scan_prolog_insn_return_no,		  ARMV4T_AND_ABOVE, "pop {...}"},
  { 0xffffff80, 0x4780    , scan_prolog_insn_return_no,		  ARMV4T_AND_ABOVE, "blx <Rm>"},
  { 0xffffff00, 0xdf00    , scan_prolog_insn_return_no,		  ARMV4T_AND_ABOVE, "swi <immed_8>"},
  /* Thumb2 */
  { 0xffff0000, 0xe8bd0000, scan_prolog_insn_return_no,		  ARMV6T2_AND_ABOVE, "pop.w {...}"},
  { 0xffff0fff, 0xf85d9b04, scan_prolog_insn_return_no,		  ARMV6T2_AND_ABOVE, "pop.w {...}"},
  { 0xffbf0e00, 0xecbd0a00, scan_prolog_insn_return_no,		  ARMV6T2_AND_ABOVE, "vpop <list> (encodings T1/A1 and T2/A2)"},  

};

static uint32_t
ror_c (uint32_t value, uint32_t N, uint32_t shift)
{
    uint32_t m = shift % N;
    uint32_t result = (value >> m) | (value << (N - m));
    return result;
}

static uint32_t 
thumb_expand_imm_c (uint32_t insn)
{
  /* Expands the modified immediate constants in Thumb instructions.  
     These opcode have an immediate that is 12 bits that is broken
     up into an i, imm3, a, b, c, d, e, f, g, h fields is the ARM
     architecture reference manual. This code was made by reading
     the pseudo code that was found in that manual.  */
  uint32_t imm32 = 0;
  const uint32_t i = bit (insn, 26);
  const uint32_t imm3 = bits (insn, 12, 14);
  const uint32_t abcdefgh = bits (insn, 0, 7);
  const uint32_t imm12 = i << 11 | imm3 << 8 | abcdefgh;

  if (bits(imm12, 10, 11) == 0)
    {
      switch (bits(imm12, 8, 9)) 
	{
	case 0:
            imm32 = abcdefgh;
	    break;

	case 1:
	    imm32 = abcdefgh << 16 | abcdefgh;
	    break;

	case 2:
	    imm32 = abcdefgh << 24 | abcdefgh << 8;
	    break;

	case 3:
	    imm32 = abcdefgh  << 24 | abcdefgh << 16 | abcdefgh << 8 | abcdefgh; 
	    break;
	}
    }
  else
    {
      const uint32_t unrotated_value = 0x80 | bits(imm12, 0, 6);
      imm32 = ror_c (unrotated_value, 32, bits(imm12, 7, 11));
    }
  return imm32;
}

#define THUMB_OPCOPE_INFO_COUNT	(sizeof (thumb_opcode_info)/sizeof (arm_opcode_info_t))
#define MAX_THUMB_PROLOGUE_SIZE	40
#define MAX_ARM_PROLOGUE_SIZE	64

/* APPLE LOCAL END: Centralize the prologue scan and skip code.  */

/* Addresses for calling Thumb functions have the bit 0 set and if
   bit 1 is set, it has to be thumb since it isn't a mutliple of four.
   Here are some macros to test, set, or clear bit 0 of addresses.  */
#define IS_THUMB_ADDR(addr)	((addr) & 3)
#define MAKE_THUMB_ADDR(addr)	((addr) | 1)
#define UNMAKE_THUMB_ADDR(addr) ((addr) & ~1)

/* Set to true if the 32-bit mode is in use.  */

int arm_apcs_32 = 1;

/* Determine if the program counter specified in MEMADDR is in a Thumb
   function.  */

int
arm_pc_is_thumb (CORE_ADDR memaddr)
{
  struct minimal_symbol *sym;

  /* If bit 0 of the address is set, assume this is a Thumb address.  */
  if (IS_THUMB_ADDR (memaddr))
    return 1;

  /* Thumb functions have a "special" bit set in minimal symbols.  */
  sym = lookup_minimal_symbol_by_pc (memaddr);
  if (sym)
    {
      return (MSYMBOL_IS_SPECIAL (sym));
    }
  else
    {
      return 0;
    }
}

/* Read a 16 or 32 bit thumb opcode from ADDR and specify the size of the
   opcode by filling the size in bytes in OPCODE_SIZE_PTR if it is not 
   NULL. Returns the opcode that was read in a single 32 bit value. If the
   thumb opcode size is 2 bytes, the opcode will be returned in bits 15:0,
   with bits 31:16 set to zero. If the thumb opcode size is 4 bytes, the 
   first 16 bit opcode will be in bits 31:16, and the second 16 bit opcode 
   will be in bits 15:0.  */
static uint32_t
read_thumb_instruction (CORE_ADDR addr, uint32_t *opcode_size_ptr)
{
  uint32_t insn = read_memory_unsigned_integer (addr, 2);
  
  /* Check if this is a 32 bit instruction and read the low 16 bits
     after shifting the high 16 bits into 31:16.  */
  if ((insn & 0xe000) != 0xe000 || bits (insn, 11, 12) == 0)
    {
      /* We have a 16 bit thumb instruction.  */
      if (opcode_size_ptr)
	*opcode_size_ptr = 2;
    }
  else
    {
      /* We have a 32 bit thumb instruction.  */
      insn = insn << 16 | read_memory_unsigned_integer (addr + 2, 2);
      
      /* Fill in the opcode size if requested.  */
      if (opcode_size_ptr)
	*opcode_size_ptr = 4;
    }
  
  return insn;
}


/* Remove useless bits from addresses in a running program.  */
static CORE_ADDR
arm_addr_bits_remove (CORE_ADDR val)
{
  if (arm_apcs_32)
    return (val & (arm_pc_is_thumb (val) ? 0xfffffffe : 0xfffffffc));
  else
    return (val & 0x03fffffc);
}

/* When reading symbols, we need to zap the low bit of the address,
   which may be set to 1 for Thumb functions.  */
static CORE_ADDR
arm_smash_text_address (CORE_ADDR val)
{
  return val & ~1;
}

/* Immediately after a function call, return the saved pc.  Can't
   always go through the frames for this because on some machines the
   new frame is not set up until the new function executes some
   instructions.  */

static CORE_ADDR
arm_saved_pc_after_call (struct frame_info *frame)
{
  return ADDR_BITS_REMOVE (read_register (ARM_LR_REGNUM));
}

/* A typical Thumb prologue looks like this:
   push    {r7, lr}
   add     sp, sp, #-28
   add     r7, sp, #12
   Sometimes the latter instruction may be replaced by:
   mov     r7, sp
   
   or like this:
   push    {r7, lr}
   mov     r7, sp
   sub	   sp, #12
   
   or, on tpcs, like this:
   sub     sp,#16
   push    {r7, lr}
   (many instructions)
   mov     r7, sp
   sub	   sp, #12

   There is always one instruction of three classes:
   1 - push
   2 - setting of r7
   3 - adjusting of sp
   
   When we have found at least one of each class we are done with the prolog.
   Note that the "sub sp, #NN" before the push does not count.
   */

#ifndef TM_NEXTSTEP

static CORE_ADDR
thumb_skip_prologue (CORE_ADDR pc, CORE_ADDR func_end)
{
  CORE_ADDR current_pc;
  /* findmask:
     bit 0 - push { rlist }
     bit 1 - mov r7, sp  OR  add r7, sp, #imm  (setting of r7)
     bit 2 - sub sp, #simm  OR  add sp, #simm  (adjusting of sp)
  */
  int findmask = 0;

  for (current_pc = pc;
       current_pc + 2 < func_end && current_pc < pc + 40;
       current_pc += 2)
    {
      unsigned short insn = read_memory_unsigned_integer (current_pc, 2);

      if ((insn & 0xfe00) == 0xb400)		/* push { rlist } */
	{
	  findmask |= THUMB_PROLOGUE_PUSH;		/* push found */
	}
      else if ((insn & 0xff00) == 0xb000)	/* add sp, #simm  OR  
						   sub sp, #simm */
	{
	  if ((findmask & THUMB_PROLOGUE_PUSH) == 0)	/* before push ? */
	    continue;
	  else
	    findmask |= THUMB_PROLOGUE_SUB_SP;	/* add/sub sp found */
	}
      else if ((insn & 0xff00) == 0xaf00)	/* add r7, sp, #imm */
	{
	  findmask |= THUMB_PROLOGUE_FP_SETUP;	/* setting of r7 found */
	}
      else if (insn == 0x466f)			/* mov r7, sp */
	{
	  findmask |= THUMB_PROLOGUE_FP_SETUP;	/* setting of r7 found */
	}
      else if (findmask == THUMB_PROLOGUE_ALL)
	{
	  /* We have found one of each type of prologue instruction */
	  break;
	}
      else
	/* Something in the prolog that we don't care about or some
	   instruction from outside the prolog scheduled here for
	   optimization.  */
	continue;
    }

  return current_pc;
}

#endif

static void
init_prologue_state (arm_prologue_state_t *state)
{
  int i;
  if (state)
    {
      memset(state, 0, sizeof(arm_prologue_state_t));
      /* Initialize the saved register map.  When register H is copied to
	 register L, we will put H in saved_reg[L].  */
      for (i = 0; i < MAX_NUM_ARM_REGISTERS; i++)
	{
	  state->reg_saved_in_reg[i] = i;
	  state->reg_loaded_from_address[i] = INVALID_ADDRESS;
	}
    }
}

static CORE_ADDR
thumb_macosx_skip_prologue (CORE_ADDR pc, CORE_ADDR func_end)
{
  int i;
  arm_prologue_state_t state;
  init_prologue_state (&state);

  /* APPLE LOCAL ADDITION: Make sure we find some prologue opcodes by keeping 
     track of the last valid prologue instruction we find in END_PROLOGUE.
     The PC passed in may not be at the start of a function and the last
     'else' in the for loop could think that all instructions up until the
     end of the function could be due to "optimized prologue instruction
     scheduling". So END_PROLOGUE should only get set to CURRENT_PC if we
     recognized the instruction as a prologue instruction.  */
  CORE_ADDR last_prologue_inst_addr = pc;
  if (arm_debug > 3)
    fprintf_unfiltered (gdb_stdlog, "thumb_macosx_skip_prologue (0x%s, 0x%s)\n", 
			paddr (pc), paddr (func_end));
  /* findmask:
     bit 0 - push { rlist }
     bit 1 - mov r7, sp  OR  add r7, sp, #imm  (setting of r7)
     bit 2 - sub sp, #simm  OR  add sp, #simm  (adjusting of sp)
  */
  CORE_ADDR max_prologue_addr = func_end;
  if (max_prologue_addr > pc + MAX_THUMB_PROLOGUE_SIZE)
    max_prologue_addr = pc + MAX_THUMB_PROLOGUE_SIZE;
    
  uint32_t opcode_size = 2;  /* Size in bytes of the thumb instruction.  */
  for (state.pc = pc; state.pc < max_prologue_addr; state.pc += opcode_size)
    {
      /* Read a 16 or 32 bit thumb instruction into a 32 bit value.  */
      uint32_t insn = read_thumb_instruction (state.pc, &opcode_size);
      
      /* Make sure we got a 2 or 4 byte opcode size. We could conceivably 
         got an opcode_size of zero if we failed to read memory. */
      if ((opcode_size != 2) && (opcode_size != 4))
	break;

      if (arm_debug > 3)
	{
	  if (opcode_size == 4)
	    fprintf_unfiltered (gdb_stdlog, " 0x%s: 0x%8.8x", 
				paddr (state.pc), insn);      
	  else
	    fprintf_unfiltered (gdb_stdlog, " 0x%s: 0x%4.4x    ", 
				paddr (state.pc), insn);
	}
     
      /* Iterate through our opcode information array and figure out which
	 instructions can be part of a progloue, or which ones can't be
	 part of a prologue.  */
      for (i=0; i<THUMB_OPCOPE_INFO_COUNT; i++)
	{
	  arm_opcode_info_t *op_info = &thumb_opcode_info[i];

	  if ((insn & op_info->mask) == op_info->value)
	    {
	      if (arm_debug > 3)
		fprintf_unfiltered (gdb_stdlog, " -> [%3d] %-30s ", i, 
				    op_info->description);

	      int opcode_type = op_info->scan_prolog(insn, NULL, &state);

	      if (opcode_type == prolog_yes)
		{
		  /* We have a proglogue opcode. */
		  if (arm_debug > 3)
		    fprintf_unfiltered (gdb_stdlog, "prolog_yes");
		  last_prologue_inst_addr = state.pc + opcode_size;
		}
	      else if (opcode_type == prolog_no)
		{
		  if (arm_debug > 3)
		    fprintf_unfiltered (gdb_stdlog, "prolog_no");
		  /* We have an instruction that can't appear in a 
		     proglogue, so return the last instruction that 
		     was a valid prologue opcode. */
		  state.pc = max_prologue_addr;
		}
	      else
		{
		  if (arm_debug > 3)
		    fprintf_unfiltered (gdb_stdlog, "prolog_ignore");
		}
	      /* We found an opcode match, break out of the mask/value loop.  */
	      break;
	    }
	}
      if (arm_debug > 3)
	fprintf_unfiltered (gdb_stdlog, "\n");
    }

  return last_prologue_inst_addr;
}

/* Advance the PC across any function entry prologue instructions to
   reach some "real" code.

   The APCS (ARM Procedure Call Standard) defines the following
   prologue:

   mov          ip, sp
   [stmfd       sp!, {a1,a2,a3,a4}]
   stmfd        sp!, {...,fp,ip,lr,pc}
   [stfe        f7, [sp, #-12]!]
   [stfe        f6, [sp, #-12]!]
   [stfe        f5, [sp, #-12]!]
   [stfe        f4, [sp, #-12]!]
   sub fp, ip, #nn @@ nn == 20 or 4 depending on second insn */
#ifndef TM_NEXTSTEP

static CORE_ADDR
arm_skip_prologue (CORE_ADDR pc)
{
  unsigned long inst;
  CORE_ADDR skip_pc;
  CORE_ADDR func_addr, func_end = 0;
  char *func_name;
  struct symtab_and_line sal;

  /* If we're in a dummy frame, don't even try to skip the prologue.  */
  if (deprecated_pc_in_call_dummy (pc))
    return pc;

  /* See what the symbol table says.  */

  if (find_pc_partial_function (pc, &func_name, &func_addr, &func_end))
    {
      struct symbol *sym;

      /* Found a function.  */
      sym = lookup_symbol (func_name, NULL, VAR_DOMAIN, NULL, NULL);
      if (sym && SYMBOL_LANGUAGE (sym) != language_asm)
        {
	  /* Don't use this trick for assembly source files.  */
	  sal = find_pc_line (func_addr, 0);
	  if ((sal.line != 0) && (sal.end < func_end))
	    return sal.end;
        }
    }

  /* Check if this is Thumb code.  */
  if (arm_pc_is_thumb (pc))
    return thumb_skip_prologue (pc, func_end);

  /* Can't find the prologue end in the symbol table, try it the hard way
     by disassembling the instructions.  */

  /* Like arm_scan_prologue, stop no later than pc + 64. */
  if (func_end == 0 || func_end > pc + 64)
    func_end = pc + 64;

  for (skip_pc = pc; skip_pc < func_end; skip_pc += 4)
    {
      inst = read_memory_integer (skip_pc, 4);

      /* "mov ip, sp" is no longer a required part of the prologue.  */
      if (inst == 0xe1a0c00d)			/* mov ip, sp */
	continue;

      if ((inst & 0xfffff000) == 0xe28dc000)    /* add ip, sp #n */
	continue;

      if ((inst & 0xfffff000) == 0xe24dc000)    /* sub ip, sp #n */
	continue;

      /* Some prologues begin with "str Rd, [sp, #-n]!".  */
      if ((inst & 0xffff0000) == 0xe52d0000)	/* str Rd, [sp, #-n]! */
	continue;

      if ((inst & 0xfffffff0) == 0xe92d0000)	/* stmfd sp!,{a1,a2,a3,a4} */
	continue;

      if ((inst & 0xfffff800) == 0xe92dd800)	/* stmfd sp!,{fp,ip,lr,pc} */
	continue;

      /* Any insns after this point may float into the code, if it makes
	 for better instruction scheduling, so we skip them only if we
	 find them, but still consider the function to be frame-ful.  */

      /* We may have either one sfmfd instruction here, or several stfe
	 insns, depending on the version of floating point code we
	 support.  */
      if ((inst & 0xffbf0fff) == 0xec2d0200)	/* sfmfd fn, <cnt>, [sp]! */
	continue;

      if ((inst & 0xffff8fff) == 0xed6d0103)	/* stfe fn, [sp, #-12]! */
	continue;

      if ((inst & 0xfffff000) == 0xe24cb000)	/* sub fp, ip, #nn */
	continue;

      if ((inst & 0xfffff000) == 0xe24dd000)	/* sub sp, sp, #nn */
	continue;

      if ((inst & 0xffffc000) == 0xe54b0000 ||	/* strb r(0123),[r11,#-nn] */
	  (inst & 0xffffc0f0) == 0xe14b00b0 ||	/* strh r(0123),[r11,#-nn] */
	  (inst & 0xffffc000) == 0xe50b0000)	/* str  r(0123),[r11,#-nn] */
	continue;

      if ((inst & 0xffffc000) == 0xe5cd0000 ||	/* strb r(0123),[sp,#nn] */
	  (inst & 0xffffc0f0) == 0xe1cd00b0 ||	/* strh r(0123),[sp,#nn] */
	  (inst & 0xffffc000) == 0xe58d0000)	/* str  r(0123),[sp,#nn] */
	continue;

      /* Un-recognized instruction; stop scanning.  */
      break;
    }

  return skip_pc;		/* End of prologue */
}

#endif

static CORE_ADDR
arm_macosx_skip_prologue_addr_ctx (struct address_context *pc_addr_ctx)
{
  CORE_ADDR pc = pc_addr_ctx->address;
  uint32_t insn;
  uint32_t i;
  arm_prologue_state_t state;
  init_prologue_state (&state);
  CORE_ADDR prologue_start, prologue_end = 0;
  CORE_ADDR last_prologue_inst_addr = pc;

  /* If we're in a dummy frame, don't even try to skip the prologue.  */
  if (deprecated_pc_in_call_dummy (pc))
    return pc;
    
  /* One way to find the end of the prologue (which works well
     for unoptimized code) is to do the following:

	struct symtab_and_line sal = find_pc_line (prologue_start, 0);

	if (sal.line == 0)
	  prologue_end = prev_pc;
	else if (sal.end < prologue_end)
	  prologue_end = sal.end;

     This mechanism is very accurate so long as the optimizer
     doesn't move any instructions from the function body into the
     prologue.  If this happens, sal.end will be the last
     instruction in the first hunk of prologue code just before
     the first instruction that the scheduler has moved from
     the body to the prologue.

     In order to make sure that we scan all of the prologue
     instructions, we use a slightly less accurate mechanism which
     may scan more than necessary.  To help compensate for this
     lack of accuracy, the prologue scanning loop below contains
     several clauses which'll cause the loop to terminate early if
     an implausible prologue instruction is encountered.  
     
     The expression
     
	  prologue_start + MAX_ARM_PROLOGUE_SIZE
	
     is a suitable endpoint since it accounts for the largest
     possible prologue plus up to five instructions inserted by
     the scheduler.  */

  /* See what the symbol table says.  */
  if (pc_addr_ctx->symbol 
      && SYMBOL_CLASS (pc_addr_ctx->symbol) == LOC_BLOCK
      && SYMBOL_BLOCK_VALUE (pc_addr_ctx->symbol))
    {
      prologue_start = SYMBOL_BLOCK_VALUE (pc_addr_ctx->symbol)->startaddr;
      prologue_end =  SYMBOL_BLOCK_VALUE (pc_addr_ctx->symbol)->endaddr;
    }
  else
  if (!find_pc_partial_function (pc, NULL, &prologue_start, &prologue_end))
    {
      /* We didn't find any function bounds for the given PC, just use 
	 the current PC as the prologue start address.  */
      prologue_start = pc;
    }

  /* We don't use the line table to find the prolgue start and end 
     since there could be optimized instructions built into the prologue,
     so make sure to limit the end to MAX_ARM_PROLOGUE_SIZE bytes past the start if needed.  */
  if (prologue_end == 0 || prologue_end > pc + MAX_ARM_PROLOGUE_SIZE)
    prologue_end = prologue_start + MAX_ARM_PROLOGUE_SIZE;

  /* Check if this is Thumb code.  */
  if (arm_pc_is_thumb (pc))
    return thumb_macosx_skip_prologue (pc, prologue_end);

  if (arm_debug > 3)
    fprintf_unfiltered (gdb_stdlog, "arm_macosx_skip_prologue (0x%s)\n", paddr (pc));

  for (state.pc = pc; state.pc < prologue_end; state.pc += 4)
    {
      insn = read_memory_unsigned_integer (state.pc, 4);
      if (arm_debug > 3)
	fprintf_unfiltered (gdb_stdlog, "  0x%s: 0x%8.8x", paddr (state.pc), 
			    insn);
     
      /* Iterate through our opcode information array and figure out which
	 instructions can be part of a progloue, or which ones can't be
	 part of a prologue.  */
      for (i=0; i<ARM_OPCOPE_INFO_COUNT; i++)
	{
	  arm_opcode_info_t *op_info = &arm_opcode_info[i];

	  if ((insn & op_info->mask) == op_info->value)
	    {
	      if (arm_debug > 3)
		fprintf_unfiltered (gdb_stdlog, " -> [%3d] %-30s ", 
				    i, op_info->description);

	      int opcode_type = op_info->scan_prolog(insn, NULL, &state);
	      if (opcode_type == prolog_yes)
		{
		  /* We have a proglogue opcode. */
		  if (arm_debug > 3)
		    fprintf_unfiltered (gdb_stdlog, "prolog_yes");
		  last_prologue_inst_addr = state.pc + 4;
		}
	      else if (opcode_type == prolog_no)
		{
		  if (arm_debug > 3)
		    fprintf_unfiltered (gdb_stdlog, "prolog_no");
		  /* We have an instruction that can't appear in a 
		     proglogue, so return the last instruction that 
		     was a valid prologue opcode. */
		  state.pc = prologue_end;
		}
	      else
		{
		  if (arm_debug > 3)
		    fprintf_unfiltered (gdb_stdlog, "prolog_ignore");
		}
	      /* We found an opcode match, break out of the mask/value loop.  */
	      break;
	    }
	}
      if (arm_debug > 3)
	fprintf_unfiltered (gdb_stdlog, "\n");
    }

  return last_prologue_inst_addr;		/* End of prologue */
}

/* APPLE LOCAL: Convert a DWARF 2 register number to a GDB register number.  */
static int
arm_dwarf2_reg_to_regnum (int num)
{
  /* Core registers.  */
  if (0 <= num && num <= ARM_PC_REGNUM)
    return num;

  /* FPA registers, ARM_F0_REGNUM .. ARM_F7_REGNUM */
  if (16 <= num && num <= 23)
    return num - 16 + ARM_F0_REGNUM;

  /* FIXME gcc 4.0 starts the VFP-v1 registers at 63 (63 == S0).
     The ARM DWARF ABI doc specifies 64 == S0.
     gcc 4.2 correctly remaps its internal regnums so 64 == S0.
     For now, have gdb assume the incorrect gcc-4.0 register numbering;
     we should probably  switch all of these to use the new 256-287 DWARF 
     register numbers to avoid ambiguity.  */
  if (63 <= num && num <= 94)
    return num - 63 + ARM_VFP_REGNUM_S0;

  /* Handle 95 specially in case we're processing output from a compiler
     that uses the correct 64..95 register numbers for the VFP-v1 regs.  */
  if (num == 95)
    return ARM_VFP_REGNUM_S31;

  /* Obsolete register numbers for the FPA register 0-7 */
  if (96 <= num && num <= 103)
    return num - 96 + ARM_F0_REGNUM;

  /* Map the new VFP-v3/Neon register numbers to S0..S31 as per the 
     recommendation in the ARM "DWARF for the ARM Architecture" ABI doc.
     gdb may need to map these to different internal registers if
     we start to work on a target with the real VFP-v3 (thirty-two
     64-bit regs) support.  But presumably such a target would just
     change the definition of the VFP registers so that they were 64-bits
     wide.  */
  if (256 <= num && num <= 287)
    return num - 256 + ARM_VFP_REGNUM_S0;

  return num;
}

/* APPLE LOCAL START: new prologue skip code.  */
static CORE_ADDR
arm_macosx_skip_prologue (CORE_ADDR pc)
{
  struct address_context pc_addr_ctx;
  init_address_context (&pc_addr_ctx);
  pc_addr_ctx.address = pc;
  return arm_macosx_skip_prologue_addr_ctx (&pc_addr_ctx);
}

/* APPLE LOCAL END: new prologue skip code.  */


/* *INDENT-OFF* */
/* Function: thumb_scan_prologue (helper function for arm_scan_prologue)
   This function decodes a Thumb function prologue to determine:
     1) the size of the stack frame
     2) which registers are saved on it
     3) the offsets of saved regs
     4) the offset from the stack pointer to the frame pointer

   A typical Thumb function prologue would create this stack frame
   (offsets relative to FP)
     old SP ->	24  stack parameters
		20  LR
		16  R7
     R7 ->       0  local variables (16 bytes)
     SP ->     -12  additional stack space (12 bytes)
   The frame size would thus be 36 bytes, and the frame offset would be
   12 bytes.  The frame register is R7. 
   
   The comments for thumb_skip_prolog() describe the algorithm we use
   to detect the end of the prolog.  */
/* *INDENT-ON* */

#ifndef TM_NEXTSTEP

static void
thumb_scan_prologue (CORE_ADDR prev_pc, arm_prologue_cache_t *cache)
{
  CORE_ADDR prologue_start;
  CORE_ADDR prologue_end;
  CORE_ADDR current_pc;
  /* Which register has been copied to register n?  */
  int saved_reg[16];
  /* findmask:
     bit 0 - push { rlist }
     bit 1 - mov r7, sp  OR  add r7, sp, #imm  (setting of r7)
     bit 2 - sub sp, #simm  OR  add sp, #simm  (adjusting of sp)
  */
  int findmask = 0;
  int i;

  if (arm_debug > 3)
    fprintf_unfiltered (gdb_stdlog, "thumb_scan_prologue (0x%s, %p)\n", 
			paddr (prev_pc), cache);

  if (find_pc_partial_function (prev_pc, NULL, &prologue_start, &prologue_end))
    {
      struct symtab_and_line sal = find_pc_line (prologue_start, 0);

      if (sal.line == 0)		/* no line info, use current PC  */
	prologue_end = prev_pc;
      else if (sal.end < prologue_end)	/* next line begins after fn end */
	prologue_end = sal.end;		/* (probably means no prologue)  */
    }
  else
    /* We're in the boondocks: allow for 
       16 pushes, an add, and "mv fp,sp".  */
    prologue_end = prologue_start + 40;

  prologue_end = min (prologue_end, prev_pc);

  /* Initialize the saved register map.  When register H is copied to
     register L, we will put H in saved_reg[L].  */
  for (i = 0; i < 16; i++)
    saved_reg[i] = i;

  /* Search the prologue looking for instructions that set up the
     frame pointer, adjust the stack pointer, and save registers.
     Do this until all basic prolog instructions are found.  */

  cache->framesize = 0;
  for (current_pc = prologue_start;
       current_pc < prologue_end && findmask != THUMB_PROLOGUE_ALL;
       current_pc += 2)
    {
      unsigned short insn;
      int regno;
      int offset;

      insn = read_memory_unsigned_integer (current_pc, 2);

      if ((insn & 0xfe00) == 0xb400)	/* push { rlist } */
	{
	  int mask;
	  findmask |= THUMB_PROLOGUE_PUSH;	/* push found */
	  /* Bits 0-7 contain a mask for registers R0-R7.  Bit 8 says
	     whether to save LR (R14).  */
	  mask = (insn & 0xff) | ((insn & 0x100) << 6);

	  /* Calculate offsets of saved R0-R7 and LR.  */
	  for (regno = ARM_LR_REGNUM; regno >= 0; regno--)
	    if (mask & (1 << regno))
	      {
		cache->framesize += 4;
		cache->saved_regs[saved_reg[regno]].addr = -cache->framesize;
		/* Reset saved register map.  */
		saved_reg[regno] = regno;
	      }
	}
      else if ((insn & 0xff00) == 0xb000)	/* add sp, #simm  OR  
						   sub sp, #simm */
	{
	  if (!(findmask & THUMB_PROLOGUE_PUSH))/* before push?  */
	    continue;
	  else
	    findmask |= THUMB_PROLOGUE_SUB_SP;	/* add/sub sp found */
	  
	  offset = (insn & 0x7f) << 2;		/* get scaled offset */
	  if (insn & 0x80)		/* is it signed? (==subtracting) */
	    {
	      cache->frameoffset += offset;
	      offset = -offset;
	    }
	  cache->framesize -= offset;
	}
      else if ((insn & 0xff00) == 0xaf00)	/* add r7, sp, #imm */
	{
	  findmask |= THUMB_PROLOGUE_FP_SETUP;	/* setting of r7 found */
	  cache->framereg = THUMB_FP_REGNUM;
	  /* get scaled offset */
	  cache->frameoffset = (insn & 0xff) << 2;
	}
      else if (insn == 0x466f)			/* mov r7, sp */
	{
	  findmask |= THUMB_PROLOGUE_FP_SETUP;	/* setting of r7 found */
	  cache->framereg = THUMB_FP_REGNUM;
	  cache->frameoffset = 0;
	  saved_reg[THUMB_FP_REGNUM] = ARM_SP_REGNUM;
	}
      else if ((insn & 0xffc0) == 0x4640)	/* mov r0-r7, r8-r15 */
	{
	  int lo_reg = insn & 7;		/* dest.  register (r0-r7) */
	  int hi_reg = ((insn >> 3) & 7) + 8;	/* source register (r8-15) */
	  saved_reg[lo_reg] = hi_reg;		/* remember hi reg was saved */
	}
      else
	/* Something in the prolog that we don't care about or some
	   instruction from outside the prolog scheduled here for
	   optimization.  */ 
	continue;
    }
}

#endif

static void
thumb_macosx_scan_prologue (CORE_ADDR prev_pc, arm_prologue_cache_t *cache)
{
  CORE_ADDR prologue_end;
  uint32_t insn = 0;
  int i;
  /* Which register has been copied to register n?  */
  arm_prologue_state_t state;
  init_prologue_state (&state);
  /* findmask:
     bit 0 - push { rlist }
     bit 1 - mov r7, sp  OR  add r7, sp, #imm  (setting of r7)
     bit 2 - sub sp, #simm  OR  add sp, #simm  (adjusting of sp)
  */
  if (arm_debug > 3)
    fprintf_unfiltered (gdb_stdlog, "thumb_macosx_scan_prologue (0x%s, %p)\n", 
			paddr (prev_pc), cache);


  cache->prologue_start = prologue_end = state.pc = 0;

  /* Find the function prologue.  If we can't find the function in
     the symbol table, peek in the stack frame to find the PC.  */
  if (find_pc_partial_function (prev_pc, NULL, &cache->prologue_start, &prologue_end))
    {
      /* One way to find the end of the prologue (which works well
         for unoptimized code) is to do the following:

	    struct symtab_and_line sal = find_pc_line (cache->prologue_start, 0);

	    if (sal.line == 0)
	      prologue_end = prev_pc;
	    else if (sal.end < prologue_end)
	      prologue_end = sal.end;

	 This mechanism is very accurate so long as the optimizer
	 doesn't move any instructions from the function body into the
	 prologue.  If this happens, sal.end will be the last
	 instruction in the first hunk of prologue code just before
	 the first instruction that the scheduler has moved from
	 the body to the prologue.

	 In order to make sure that we scan all of the prologue
	 instructions, we use a slightly less accurate mechanism which
	 may scan more than necessary.  To help compensate for this
	 lack of accuracy, the prologue scanning loop below contains
	 several clauses which'll cause the loop to terminate early if
	 an implausible prologue instruction is encountered.  
	 
	 The expression
	 
	      cache->prologue_start + MAX_THUMB_PROLOGUE_SIZE
	    
	 is a suitable endpoint since it accounts for the largest
	 possible prologue plus up to five instructions inserted by
	 the scheduler.  */
         
      if (prologue_end > cache->prologue_start + MAX_THUMB_PROLOGUE_SIZE)
	  prologue_end = cache->prologue_start + MAX_THUMB_PROLOGUE_SIZE;
      if (arm_debug > 4)
	fprintf_unfiltered (gdb_stdlog, 
			    "prologue_start found in symbols starting at 0x%s\n",
			    paddr (cache->prologue_start));      
    }
  else
    {
      /* We have no symbol information.  We need to search backward for a 
         push instruction.  */

      /* Initialize the prologue start and end addresses in case we don't
	 find anything useful.  */
      cache->prologue_start = prologue_end = 0;
      ULONGEST ulongest = 0; 
      CORE_ADDR prologue_pc;
      uint32_t count = 0;
      uint32_t consecutive_zero_opcodes = 0;
      if (arm_debug > 4)
	fprintf_unfiltered (gdb_stdlog, 
			"Find prologue_start by reading memory opcodes:\n");      

      for (prologue_pc = prev_pc;
	   safe_read_memory_unsigned_integer (prologue_pc, 2, &ulongest);
	   prologue_pc -= 2)
	{
	  insn = ulongest;
	  if (insn == 0)
	    consecutive_zero_opcodes++;
	  else
	    consecutive_zero_opcodes = 0;
	  if (consecutive_zero_opcodes > 1)
	    {
	      if (arm_debug > 4)
		fprintf_unfiltered (gdb_stdlog, 
				    "  prologue_start - unable find start \
of function, aborting at 0x%s after consecutive zero opcodes\n",
				    paddr (prologue_pc));
	      return;
	    }

	  if (arm_debug > 4)
	    fprintf_unfiltered (gdb_stdlog, " 0x%s: 0x%4.4x\n",
				paddr (prologue_pc), insn);      

	  if ((insn & 0xff80) == 0xb580)	/* push { lr, r7, ... } */
	    {
	      /* Also check for pretend arguments just in case.  */
	      if (safe_read_memory_unsigned_integer (prologue_pc - 2, 2, &ulongest))
		 {
		    uint32_t prev_insn = ulongest;
		    if ((prev_insn & 0xffffff80) == 0xb080)	/* sub sp, #n (subtract immediated from SP)  */
		      {
			/* We found some pretend arguments just before our
			   PUSH instruction, so this is the start of the
			   function prologue. This happens when an argument
			   value is split between regs and stack such as:
			   int f (int x, int y, int z, long long ll);   */
			prologue_pc = prologue_pc - 2;
		      }
		  }

	    
	      /* Watch for a push that would start a frame.  */
	      cache->prologue_start = prologue_pc;
	      prologue_end = cache->prologue_start + MAX_THUMB_PROLOGUE_SIZE;	/* See above.  */
	      if (arm_debug > 4)
		fprintf_unfiltered (gdb_stdlog, 
				    "prologue_start - found start of function 0x%s: 0x%s\n",
				    paddr (prologue_pc),
				    paddr (insn));

	      break;
	    }
	  /* Only check a certain number of instructions before we give
	     when trying to find the start of a function.  */
	  if (count++ > 256)
	    {
	      if (arm_debug > 4)
		fprintf_unfiltered (gdb_stdlog, 
				    "prologue_start - unable find start of function within 256 opcodes from 0x%s\n",
				    paddr (prev_pc));
	      return;
	    }
	}
	
    }

  prologue_end = min (prologue_end, prev_pc);


  /* Search the prologue looking for instructions that set up the
     frame pointer, adjust the stack pointer, and save registers.
     Do this until all basic prolog instructions are found.  */

  cache->framesize = 0;
  uint32_t opcode_size = 2;  /* Size in bytes of the thumb instruction.  */
  for (state.pc = cache->prologue_start;
       state.pc < prologue_end;
       state.pc += opcode_size)
    {
      /* Read a 16 or 32 bit thumb instruction into a 32 bit value.  */
      insn = read_thumb_instruction (state.pc, &opcode_size);

      /* Make sure we got a 2 or 4 byte opcode size. We could conceivably 
         got an opcode_size of zero if we failed to read memory. */
      if ((opcode_size != 2) && (opcode_size != 4))
	break;
      
      if (arm_debug > 3)
	{
	  if (opcode_size == 4)
	    fprintf_unfiltered (gdb_stdlog, "  0x%s: 0x%8.8x", 
				paddr (state.pc), insn);      
	  else
	    fprintf_unfiltered (gdb_stdlog, "  0x%s: 0x%4.4x    ", 
				paddr (state.pc), insn);
	}

      for (i=0; i<THUMB_OPCOPE_INFO_COUNT; i++)
	{
	  arm_opcode_info_t *op_info = &thumb_opcode_info[i];
	  if ((insn & op_info->mask) == op_info->value)
	    {
	      /* We have a matching opcode, check if we need to scan it! */
	      if (arm_debug > 3)
		fprintf_unfiltered (gdb_stdlog, " -> {%3d} %-30s ",
				    i, op_info->description);

	      int opcode_type = op_info->scan_prolog(insn, cache, &state);

	      if (opcode_type == prolog_yes)
		{
		  /* We have a proglogue opcode, scan the prologue instruction
		     if there is a callback.  */
		  if (arm_debug > 3)
		    fprintf_unfiltered (gdb_stdlog, "prolog_yes");
		}
	      else if (opcode_type == prolog_no)
		{
		  if (arm_debug > 3)
		    fprintf_unfiltered (gdb_stdlog, "prolog_no");

		  /* We have an instruction that can't appear in a 
		     proglogue, so we should stop parsing.  */
		  state.pc = prologue_end;
		}
	      else
		{
		  if (arm_debug > 3)
		    fprintf_unfiltered (gdb_stdlog, "prolog_ignore");
		}
	      /* We found an opcode match, break out of the mask/value loop.  */
	      break;
	    }
	}
      if (arm_debug > 3)
	fprintf_unfiltered (gdb_stdlog, "\n");
    }
}


/* This function decodes an ARM function prologue to determine:
   1) the size of the stack frame
   2) which registers are saved on it
   3) the offsets of saved regs
   4) the offset from the stack pointer to the frame pointer
   This information is stored in the "extra" fields of the frame_info.

   There are two basic forms for the ARM prologue.  The fixed argument
   function call will look like:

   mov    ip, sp
   stmfd  sp!, {fp, ip, lr, pc}
   sub    fp, ip, #4
   [sub sp, sp, #4]

   Which would create this stack frame (offsets relative to FP):
   IP ->   4    (caller's stack)
   FP ->   0    PC (points to address of stmfd instruction + 8 in callee)
   -4   LR (return address in caller)
   -8   IP (copy of caller's SP)
   -12  FP (caller's FP)
   SP -> -28    Local variables

   The frame size would thus be 32 bytes, and the frame offset would be
   28 bytes.  The stmfd call can also save any of the vN registers it
   plans to use, which increases the frame size accordingly.

   Note: The stored PC is 8 off of the STMFD instruction that stored it
   because the ARM Store instructions always store PC + 8 when you read
   the PC register.

   A variable argument function call will look like:

   mov    ip, sp
   stmfd  sp!, {a1, a2, a3, a4}
   stmfd  sp!, {fp, ip, lr, pc}
   sub    fp, ip, #20

   Which would create this stack frame (offsets relative to FP):
   IP ->  20    (caller's stack)
   16  A4
   12  A3
   8  A2
   4  A1
   FP ->   0    PC (points to address of stmfd instruction + 8 in callee)
   -4   LR (return address in caller)
   -8   IP (copy of caller's SP)
   -12  FP (caller's FP)
   SP -> -28    Local variables

   The frame size would thus be 48 bytes, and the frame offset would be
   28 bytes.

   There is another potential complication, which is that the optimizer
   will try to separate the store of fp in the "stmfd" instruction from
   the "sub fp, ip, #NN" instruction.  Almost anything can be there, so
   we just key on the stmfd, and then scan for the "sub fp, ip, #NN"...

   Also, note, the original version of the ARM toolchain claimed that there
   should be an

   instruction at the end of the prologue.  I have never seen GCC produce
   this, and the ARM docs don't mention it.  We still test for it below in
   case it happens...

 */
#ifndef TM_NEXTSTEP

static void
arm_scan_prologue (struct frame_info *next_frame, struct arm_prologue_cache *cache)
{
  int regno, sp_offset, fp_offset, ip_offset;
  unsigned int insn;
  CORE_ADDR prologue_start, prologue_end, current_pc;
  CORE_ADDR prev_pc = frame_pc_unwind (next_frame);

  /* Assume there is no frame until proven otherwise.  */
  cache->framereg = ARM_SP_REGNUM;
  cache->framesize = 0;
  cache->frameoffset = 0;

  /* Check for Thumb prologue.  */
  if (arm_pc_is_thumb (prev_pc))
    {
      thumb_scan_prologue (prev_pc, cache);
      return;
    }
  if (arm_debug > 3)
    fprintf_unfiltered (gdb_stdlog, "arm_scan_prologue (0x%s, %p)\n", 
			paddr (prev_pc), cache);

  prologue_start = prologue_end = current_pc = 0;

  /* Find the function prologue.  If we can't find the function in
     the symbol table, peek in the stack frame to find the PC.  */
  if (find_pc_partial_function (prev_pc, NULL, &prologue_start, &prologue_end))
    {
      /* One way to find the end of the prologue (which works well
         for unoptimized code) is to do the following:

	    struct symtab_and_line sal = find_pc_line (prologue_start, 0);

	    if (sal.line == 0)
	      prologue_end = prev_pc;
	    else if (sal.end < prologue_end)
	      prologue_end = sal.end;

	 This mechanism is very accurate so long as the optimizer
	 doesn't move any instructions from the function body into the
	 prologue.  If this happens, sal.end will be the last
	 instruction in the first hunk of prologue code just before
	 the first instruction that the scheduler has moved from
	 the body to the prologue.

	 In order to make sure that we scan all of the prologue
	 instructions, we use a slightly less accurate mechanism which
	 may scan more than necessary.  To help compensate for this
	 lack of accuracy, the prologue scanning loop below contains
	 several clauses which'll cause the loop to terminate early if
	 an implausible prologue instruction is encountered.  
	 
	 The expression
	 
	      prologue_start + 64
	    
	 is a suitable endpoint since it accounts for the largest
	 possible prologue plus up to five instructions inserted by
	 the scheduler.  */
         
      if (prologue_end > prologue_start + 64)
	{
	  prologue_end = prologue_start + 64;	/* See above.  */
	}
      if (arm_debug > 4)
	fprintf_unfiltered (gdb_stdlog, 
			    "prologue_start found in symbols starting at 0x%s\n",
			    paddr (prologue_start));      
    }
  else
    {
      /* We have no symbol information.  Our only option is to assume this
	 function has a standard stack frame and the normal frame register.
	 Then, we can find the value of our frame pointer on entrance to
	 the callee (or at the present moment if this is the innermost frame).
	 The value stored there should be the address of the stmfd + 8.  */
      CORE_ADDR frame_loc;
      LONGEST return_value;

      frame_loc = frame_unwind_register_unsigned (next_frame, ARM_FP_REGNUM);
      if (!safe_read_memory_integer (frame_loc, 4, &return_value))
        return;
      else
        {
          prologue_start = gdbarch_addr_bits_remove 
			     (current_gdbarch, return_value) - 8;
          prologue_end = prologue_start + 64;	/* See above.  */
        }
    }

  if (prev_pc < prologue_end)
    prologue_end = prev_pc;

  /* Now search the prologue looking for instructions that set up the
     frame pointer, adjust the stack pointer, and save registers.

     Be careful, however, and if it doesn't look like a prologue,
     don't try to scan it.  If, for instance, a frameless function
     begins with stmfd sp!, then we will tell ourselves there is
     a frame, which will confuse stack traceback, as well as "finish" 
     and other operations that rely on a knowledge of the stack
     traceback.

     In the APCS, the prologue should start with  "mov ip, sp" so
     if we don't see this as the first insn, we will stop.  

     [Note: This doesn't seem to be true any longer, so it's now an
     optional part of the prologue.  - Kevin Buettner, 2001-11-20]

     [Note further: The "mov ip,sp" only seems to be missing in
     frameless functions at optimization level "-O2" or above,
     in which case it is often (but not always) replaced by
     "str lr, [sp, #-4]!".  - Michael Snyder, 2002-04-23]  */

  sp_offset = fp_offset = ip_offset = 0;

  for (current_pc = prologue_start;
       current_pc < prologue_end;
       current_pc += 4)
    {
      insn = read_memory_unsigned_integer (current_pc, 4);

      if (arm_debug > 4)
	fprintf_unfiltered (gdb_stdlog, " 0x%s: 0x%8.8x\n",
				    paddr (current_pc), insn);      


      if (insn == 0xe1a0c00d)		/* mov ip, sp */
	{
	  ip_offset = 0;
	  continue;
	}
      else if ((insn & 0xfffff000) == 0xe28dc000) /* add ip, sp #n */
	{
	  unsigned imm = insn & 0xff;                   /* immediate value */
	  unsigned rot = (insn & 0xf00) >> 7;           /* rotate amount */
	  imm = (imm >> rot) | (imm << (32 - rot));
	  ip_offset = imm;
	  continue;
	}
      else if ((insn & 0xfffff000) == 0xe24dc000) /* sub ip, sp #n */
	{
	  unsigned imm = insn & 0xff;                   /* immediate value */
	  unsigned rot = (insn & 0xf00) >> 7;           /* rotate amount */
	  imm = (imm >> rot) | (imm << (32 - rot));
	  ip_offset = -imm;
	  continue;
	}
      else if (insn == 0xe52de004)	/* str lr, [sp, #-4]! */
	{
	  sp_offset -= 4;
	  cache->saved_regs[ARM_LR_REGNUM].addr = sp_offset;
	  continue;
	}
      else if ((insn & 0xffff0000) == 0xe92d0000)
	/* stmfd sp!, {..., fp, ip, lr, pc}
	   or
	   stmfd sp!, {a1, a2, a3, a4}  */
	{
	  int mask = insn & 0xffff;

	  /* Calculate offsets of saved registers.  */
	  for (regno = ARM_PC_REGNUM; regno >= 0; regno--)
	    if (mask & (1 << regno))
	      {
		sp_offset -= 4;
		cache->saved_regs[regno].addr = sp_offset;
	      }
	}
      else if ((insn & 0xffffc000) == 0xe54b0000 ||	/* strb rx,[r11,#-n] */
	       (insn & 0xffffc0f0) == 0xe14b00b0 ||	/* strh rx,[r11,#-n] */
	       (insn & 0xffffc000) == 0xe50b0000)	/* str  rx,[r11,#-n] */
	{
	  /* No need to add this to saved_regs -- it's just an arg reg.  */
	  continue;
	}
      else if ((insn & 0xffffc000) == 0xe5cd0000 ||	/* strb rx,[sp,#n] */
	       (insn & 0xffffc0f0) == 0xe1cd00b0 ||	/* strh rx,[sp,#n] */
	       (insn & 0xffffc000) == 0xe58d0000)	/* str  rx,[sp,#n] */
	{
	  /* No need to add this to saved_regs -- it's just an arg reg.  */
	  continue;
	}
      else if ((insn & 0xfffff000) == 0xe24cb000)	/* sub fp, ip #n */
	{
	  unsigned imm = insn & 0xff;			/* immediate value */
	  unsigned rot = (insn & 0xf00) >> 7;		/* rotate amount */
	  imm = (imm >> rot) | (imm << (32 - rot));
	  fp_offset = -imm + ip_offset;
	  cache->framereg = ARM_FP_REGNUM;
	}
      else if ((insn & 0xfffff000) == 0xe24dd000)	/* sub sp, sp #n */
	{
	  unsigned imm = insn & 0xff;			/* immediate value */
	  unsigned rot = (insn & 0xf00) >> 7;		/* rotate amount */
	  imm = (imm >> rot) | (imm << (32 - rot));
	  sp_offset -= imm;
	}
      else if ((insn & 0xffff7fff) == 0xed6d0103)	/* stfe f?, [sp, -#c]! */
	{
	  sp_offset -= 12;
	  regno = ARM_F0_REGNUM + ((insn >> 12) & 0x07);
	  cache->saved_regs[regno].addr = sp_offset;
	}
      else if ((insn & 0xffbf0fff) == 0xec2d0200)	/* sfmfd f0, 4, [sp!] */
	{
	  int n_saved_fp_regs;
	  unsigned int fp_start_reg, fp_bound_reg;

	  if ((insn & 0x800) == 0x800)		/* N0 is set */
	    {
	      if ((insn & 0x40000) == 0x40000)	/* N1 is set */
		n_saved_fp_regs = 3;
	      else
		n_saved_fp_regs = 1;
	    }
	  else
	    {
	      if ((insn & 0x40000) == 0x40000)	/* N1 is set */
		n_saved_fp_regs = 2;
	      else
		n_saved_fp_regs = 4;
	    }

	  fp_start_reg = ARM_F0_REGNUM + ((insn >> 12) & 0x7);
	  fp_bound_reg = fp_start_reg + n_saved_fp_regs;
	  for (; fp_start_reg < fp_bound_reg; fp_start_reg++)
	    {
	      sp_offset -= 12;
	      cache->saved_regs[fp_start_reg++].addr = sp_offset;
	    }
	}
      else if ((insn & 0xf0000000) != 0xe0000000)
	break;			/* Condition not true, exit early */
      else if ((insn & 0xfe200000) == 0xe8200000)	/* ldm? */
	break;			/* Don't scan past a block load */
      else
	/* The optimizer might shove anything into the prologue,
	   so we just skip what we don't recognize.  */
	continue;
    }

  /* The frame size is just the negative of the offset (from the
     original SP) of the last thing thing we pushed on the stack. 
     The frame offset is [new FP] - [new SP].  */
  cache->framesize = -sp_offset;
  if (cache->framereg == ARM_FP_REGNUM)
    cache->frameoffset = fp_offset - sp_offset;
  else
    cache->frameoffset = 0;
}

#endif

/* Get the real frame for this frame by getting the previous frame as long as
   THIS_FRAME is inlined. We do this so we can check the real type of the 
   current frame (is this frame really a sigtramp frame, or is it really a
   normal frame).  */

struct frame_info *
get_non_inlined_frame (struct frame_info *this_frame)
{
  while (this_frame != NULL)
    {
      if (get_frame_type(this_frame) != INLINED_FRAME)
	break;
      this_frame = get_prev_frame (this_frame);
    }
  return this_frame;
}


static CORE_ADDR
arm_macosx_locate_prologue_start (const CORE_ADDR addr)
{
  /* Initialize the prologue start and end addresses in case we don't
     find anything useful.  */
  ULONGEST ulongest = 0; 
  CORE_ADDR pc;
  /* Default our return value to be the same address that was passed in.  */
  CORE_ADDR prologue_start = addr;
  uint32_t consecutive_zero_opcodes = 0;
  const uint32_t max_insns = 1024;
  const uint32_t opcode_size = 4;
  const uint32_t max_consecutive_zero_opcodes = 2;
  CORE_ADDR low_addr;
  
  if (addr >= max_insns * opcode_size)
    low_addr = addr - (max_insns * opcode_size);
  else
    low_addr = 0;
    
  for (pc = addr; pc > low_addr; pc -= opcode_size)
    {
      if (!safe_read_memory_unsigned_integer (pc, opcode_size, &ulongest))
	{
	  if (arm_debug > 4)
	    fprintf_unfiltered (gdb_stdlog, 
				"%s: failed to read opcode at 0x%s.\n",
				__FUNCTION__, paddr (pc));
	  break;
	}

      uint32_t insn = ulongest;

      if (arm_debug > 4)
	fprintf_unfiltered (gdb_stdlog, " 0x%s: 0x%8.8x\n", paddr (pc), insn);      

      if (insn == 0)
	{
	  if (++consecutive_zero_opcodes >= max_consecutive_zero_opcodes)
	    {
	      if (arm_debug > 4)
		fprintf_unfiltered (gdb_stdlog, 
				    "%s: aborting at 0x%s after %u consecutive "
				    "0x00000000 opcodes.\n",
				    __FUNCTION__, paddr (pc), 
				    max_consecutive_zero_opcodes);
	      break;
	    }
	}
      else
	consecutive_zero_opcodes = 0;

      /* Look for a stmdb instructions that push at least the LR.  */
      if ((insn & 0xffff4000u) == 0xe92d4000u)
	{
	  /* Also check for pretend arguments just in case.  */
	  if (safe_read_memory_unsigned_integer (pc - opcode_size, opcode_size, 
						 &ulongest))
	    {
	      uint32_t prev_insn = ulongest;
	      
	      if ((prev_insn & 0xfffff000u) == 0xe24dd000)	
		{
		  /* We found some pretend arguments just before our
		     STMDB instruction, so this is the start of the
		     function prologue. This happens when an argument
		     value is split between regs and stack such as:
		     int f (int x, int y, int z, long long ll);   */
		  pc = pc - opcode_size;
		}
	    }
	  prologue_start = pc;
	  break;
	}
    }

  if (arm_debug > 4)
    fprintf_unfiltered (gdb_stdlog, "%s(0x%s): 0x%s %s\n", __FUNCTION__, 
			paddr (addr), paddr(prologue_start),
			addr == prologue_start ? "FAIL" : "SUCCESS");

  return prologue_start;
}

static void
arm_macosx_scan_prologue (struct frame_info *next_frame, arm_prologue_cache_t *cache)
{
  arm_prologue_state_t state;
  int i;
  uint32_t insn;
  CORE_ADDR prologue_end;
  CORE_ADDR prev_pc = frame_pc_unwind (next_frame);

  init_prologue_state (&state);

  arm_prologue_cache_t *next_cache;
  
  next_cache = get_arm_prologue_cache (next_frame);
  /* Assume there is no frame until proven otherwise.  */
  cache->framereg = ARM_SP_REGNUM;
  cache->framesize = 0;
  cache->frameoffset = 0;
  
  /* If we are above frame zero, then we assume we have an R7 frame pointer
     as long the previous frame is not a sigtramp frame. We have to be careful
     when tracking down the previous frame because we want the previous frame
     to the real frame that corresponds to NEXT_FRAME. NEXT_FRAME could be
     an inlined frame with n number of other inlined frames before we get
     to the real next frame. Then we must do the same to get to the real
     previous frame. We do this in case REAL_NEXT_FRAME is a leaf function
     and PREV_BASE_FRAME is a sigtramp frame. 
     
     If we have a leaf function that is called by a sigtramp with no inline
     frames, the problem case would look like this:
     
     # Function	  frame type	  variable
     - ---------- --------------- ----------------------------------------
     2 leaf_func  NORMAL_FRAME    NEXT_FRAME == REAL_NEXT_FRAME
     1 sigtramp   SIGTRAMP_FRAME  REAL_PREV_FRAME
     0 sighandler SENTINEL_FRAME
     
     
     If we have a leaf function that is called by a sigtramp with inline
     frames, the problem case would look like this:

     # Function   frame type      variable
     - ---------- --------------- ----------------------------------------
     6 leaf_func  INLINED_FRAME   NEXT_FRAME
     5 leaf_func  INLINED_FRAME
     4 leaf_func  NORMAL_FRAME    REAL_NEXT_FRAME
     3 sigtramp   INLINED_FRAME
     2 sigtramp   INLINED_FRAME
     1 sigtramp   SIGTRAMP_FRAME  REAL_PREV_FRAME
     0 sighandler SENTINEL_FRAME

  */
  if (frame_relative_level (next_frame) > 0)
    {
      struct frame_info *real_next_frame;
      struct frame_info *real_prev_frame;

      /* Get the non-inlined frame for NEXT_FRAME.  */
      real_next_frame = get_non_inlined_frame (next_frame);
      /* Check for NULL since we can't pass NULL to get_prev_frame ().  */
      if (real_next_frame != NULL)
	{
	  /* Get the non-inlined frame previous to REAL_NEXT_FRAME.  */
	  real_prev_frame = get_non_inlined_frame (get_prev_frame (
							      real_next_frame));
	  if (real_prev_frame != NULL)
	    {
	      /* Make sure the previous real frame is a normal frame (and not
	         a SIGTRAMP_FRAME).  */
	      if (get_frame_type (real_prev_frame) != SIGTRAMP_FRAME)
		{
		  /* Our current base frame and the previous base frame are
		     normal, so we can safely assume our frame register is
		     ARM_FP_REGNUM.  */
		  cache->framereg = ARM_FP_REGNUM;
		}
	    }
	}
    }
  

  /* Check for Thumb prologue.  */
  if ((next_cache && next_cache->prev_pc_is_thumb) || arm_pc_is_thumb (prev_pc))
    {
      thumb_macosx_scan_prologue (prev_pc, cache);
      return;
    }

  if (arm_debug > 3)
    fprintf_unfiltered (gdb_stdlog, "arm_macosx_scan_prologue (0x%s, %p)\n", 
			paddr (prev_pc), cache);

  cache->prologue_start = prologue_end = state.pc = 0;

  /* Find the function prologue.  If we can't find the function in
     the symbol table, peek in the stack frame to find the PC.  */
  if (find_pc_partial_function (prev_pc, NULL, &cache->prologue_start, &prologue_end))
    {
      /* One way to find the end of the prologue (which works well
         for unoptimized code) is to do the following:

	    struct symtab_and_line sal = find_pc_line (cache->prologue_start, 0);

	    if (sal.line == 0)
	      prologue_end = prev_pc;
	    else if (sal.end < prologue_end)
	      prologue_end = sal.end;

	 This mechanism is very accurate so long as the optimizer
	 doesn't move any instructions from the function body into the
	 prologue.  If this happens, sal.end will be the last
	 instruction in the first hunk of prologue code just before
	 the first instruction that the scheduler has moved from
	 the body to the prologue.

	 In order to make sure that we scan all of the prologue
	 instructions, we use a slightly less accurate mechanism which
	 may scan more than necessary.  To help compensate for this
	 lack of accuracy, the prologue scanning loop below contains
	 several clauses which'll cause the loop to terminate early if
	 an implausible prologue instruction is encountered.  
	 
	 The expression
	 
	      cache->prologue_start + MAX_ARM_PROLOGUE_SIZE
	    
	 is a suitable endpoint since it accounts for the largest
	 possible prologue plus up to five instructions inserted by
	 the scheduler.  */
         
      if (prologue_end > cache->prologue_start + MAX_ARM_PROLOGUE_SIZE)
	  prologue_end = cache->prologue_start + MAX_ARM_PROLOGUE_SIZE;

      if (arm_debug > 4)
	fprintf_unfiltered (gdb_stdlog, 
			    "  prologue_start found in symbols starting at 0x%s\n",
			    paddr (cache->prologue_start));      
    }
  else
    {
      /* Find the prologue start by searching backwards in memory.  */
      cache->prologue_start = arm_macosx_locate_prologue_start (prev_pc);
      prologue_end = cache->prologue_start + MAX_ARM_PROLOGUE_SIZE;
    }

  prologue_end = min (prologue_end, prev_pc);

  /* Now search the prologue looking for instructions that set up the
     frame pointer, adjust the stack pointer, and save registers.

     Be careful, however, and if it doesn't look like a prologue,
     don't try to scan it.  If, for instance, a frameless function
     begins with stmfd sp!, then we will tell ourselves there is
     a frame, which will confuse stack traceback, as well as "finish" 
     and other operations that rely on a knowledge of the stack
     traceback.

     In the APCS, the prologue should start with  "mov ip, sp" so
     if we don't see this as the first insn, we will stop.  

     [Note: This doesn't seem to be true any longer, so it's now an
     optional part of the prologue.  - Kevin Buettner, 2001-11-20]

     [Note further: The "mov ip,sp" only seems to be missing in
     frameless functions at optimization level "-O2" or above,
     in which case it is often (but not always) replaced by
     "str lr, [sp, #-4]!".  - Michael Snyder, 2002-04-23]  */

  for (state.pc = cache->prologue_start;
       state.pc < prologue_end;
       state.pc += 4)
    {
      insn = read_memory_unsigned_integer (state.pc, 4);

      if (arm_debug > 3)
	fprintf_unfiltered (gdb_stdlog, "  0x%s: 0x%8.8x ",
				    paddr (state.pc), insn);      


      for (i=0; i<ARM_OPCOPE_INFO_COUNT; i++)
	{
	  arm_opcode_info_t *op_info = &arm_opcode_info[i];
	  if ((insn & op_info->mask) == op_info->value)
	    {
	      if (arm_debug > 3)
		fprintf_unfiltered (gdb_stdlog, " -> {%3d} %-30s ", 
				    i, op_info->description);

	      /* We have a matching opcode, check if we need to scan it! */
	      int opcode_type = op_info->scan_prolog(insn, cache, &state);
		
	      if (opcode_type == prolog_yes)
		{
		  /* We have a proglogue opcode, scan the prologue instruction
		     if there is a callback.  */
		  if (arm_debug > 3)
		    fprintf_unfiltered (gdb_stdlog, "prolog_yes");
		}
	      else if (opcode_type == prolog_no)
		{
		  if (arm_debug > 3)
		    fprintf_unfiltered (gdb_stdlog, "prolog_no");

		  /* We have an instruction that can't appear in a 
		     proglogue, so we should stop parsing.  */
		  state.pc = prologue_end;
		}
	      else
		{
		  if (arm_debug > 3)
		    fprintf_unfiltered (gdb_stdlog, "prolog_ignore");
		}
	      /* We found an opcode match, break out of the mask/value loop.  */
	      break;
	    }
	}
      if (arm_debug > 3)
	fprintf_unfiltered (gdb_stdlog, "\n");
    }

  /* The frame size is just the negative of the offset (from the
     original SP) of the last thing thing we pushed on the stack. 
     The frame offset is [new FP] - [new SP].  */
  cache->framesize = -state.sp_offset;
  if (cache->framereg == ARM_FP_REGNUM)
    cache->frameoffset = state.fp_offset - state.sp_offset;
  else
    cache->frameoffset = 0;
}



static arm_prologue_cache_t *
arm_make_prologue_cache (struct frame_info *next_frame)
{
  int reg;
  arm_prologue_cache_t *cache;

  cache = frame_obstack_zalloc (sizeof (arm_prologue_cache_t));
  cache->saved_regs = trad_frame_alloc_saved_regs (next_frame);

#ifdef TM_NEXTSTEP
  arm_macosx_scan_prologue (next_frame, cache);
#else
  arm_scan_prologue (next_frame, cache);
#endif

  cache->prev_fp = frame_unwind_register_unsigned (next_frame, cache->framereg);
  if (cache->prev_fp != 0)
    {
      cache->prev_sp = cache->prev_fp + cache->framesize - cache->frameoffset;
      /* Calculate actual addresses of saved registers using offsets
	determined by arm_scan_prologue.  */
      for (reg = 0; reg < NUM_REGS; reg++)
	{
	  if (trad_frame_addr_p (cache->saved_regs, reg))
	    cache->saved_regs[reg].addr += cache->prev_sp;
	  else if (cache->framereg == ARM_FP_REGNUM)
	    {
	      /* If we don't have anything on our previous registers and the
	         register is the FP or the PC, we can figure out the address
		 using if we have a FP based frame. On darwin the frame
		 backchain always points to the previous FP and the PC is at
		 FP + 4. The FP backchain does NOT include the start address 
		 of the previous function (like the FSF gcc does).  */
	      if (reg == ARM_FP_REGNUM)
		cache->saved_regs[reg].addr = cache->prev_fp;
	      else if (reg == ARM_PC_REGNUM)
		cache->saved_regs[reg].addr = cache->prev_fp + 4;
	    }
	}
    }

  if (arm_debug > 6)
    {
      int reg;
      fprintf_unfiltered (gdb_stdlog, "#%i (arm_prologue_cache_t *)%p:\n"
	"\tprev_sp = 0x%s\n\tprologue_start = 0x%s\n"
	"\tframesize = %i\n\tframeoffset = %i\n\tframereg = %i\n"
	"\tprev_fp = 0x%s\n\tprev_pc_is_thumb = %i\n",
	frame_relative_level (next_frame), cache, paddr(cache->prev_sp),
	paddr(cache->prologue_start), cache->framesize, cache->frameoffset,
	cache->framereg, paddr(cache->prev_fp), cache->prev_pc_is_thumb);

      /* Dump the register location information that we found.  */
      for (reg = 0; reg < NUM_REGS; reg++)
	if (cache->saved_regs[reg].addr != -1 || 
	    cache->saved_regs[reg].realreg != reg)
	  fprintf_unfiltered (gdb_stdlog, "\tsaved_regs[%i] = { addr = 0x%s, realreg = %i }\n",
			      reg, 
			      paddr(cache->saved_regs[reg].addr), 
			      cache->saved_regs[reg].realreg);
    }
  return cache;
}

/* Our frame ID for a normal frame is the current function's starting PC
   and the caller's SP when we were called.  */

static void
arm_prologue_this_id (struct frame_info *next_frame,
		      void **this_cache,
		      struct frame_id *this_id)
{
  arm_prologue_cache_t *cache;
  struct frame_id id;
  CORE_ADDR func;

  if (*this_cache == NULL)
    *this_cache = arm_make_prologue_cache (next_frame);
  cache = *this_cache;

  func = frame_func_unwind (next_frame);

  /* APPLE LOCAL ADDITION: If we didn't get anything sensical from a call
     to frame_func_unwind, see if we were able to find our prologue
     start in our arch specific prologue cache information and use
     it. This will allow us to stack crawl through code that has no
     symbols.  */
  if (func == 0 && cache != NULL)
    func = cache->prologue_start;
  /* APPLE LOCAL END  */

  /* This is meant to halt the backtrace at "_start".  Make sure we
     don't halt it at a generic dummy frame. */
  if (func <= LOWEST_PC)
    return;

  /* If we've hit a wall, stop.  */
  if (cache->prev_sp == 0)
    return;

  id = frame_id_build (cache->prev_sp, func);
  *this_id = id;
}

static void
arm_prologue_prev_register (struct frame_info *next_frame,
			    void **this_cache,
			    int prev_regnum,
			    int *optimized,
			    enum lval_type *lvalp,
			    CORE_ADDR *addrp,
			    int *realnump,
			    gdb_byte *valuep)
{
  arm_prologue_cache_t *cache;

  if (*this_cache == NULL)
    *this_cache = arm_make_prologue_cache (next_frame);
  cache = *this_cache;

  /* If we are asked to unwind the PC, then we need to return the LR
     instead.  The saved value of PC points into this frame's
     prologue, not the next frame's resume location.  */
  if (prev_regnum == ARM_PC_REGNUM)
    prev_regnum = ARM_LR_REGNUM;

  /* SP is generally not saved to the stack, but this frame is
     identified by NEXT_FRAME's stack pointer at the time of the call.
     The value was already reconstructed into PREV_SP.  */
  if (prev_regnum == ARM_SP_REGNUM)
    {
      *lvalp = not_lval;
      if (valuep)
	store_unsigned_integer (valuep, 4, cache->prev_sp);
      return;
    }

  enum opt_state e_opt;
  trad_frame_get_prev_register (next_frame, cache->saved_regs, prev_regnum,
				optimized ? &e_opt : NULL, lvalp, addrp, 
				realnump, valuep);
  if (optimized)
    *optimized = e_opt;
}

struct frame_unwind arm_prologue_unwind = {
  NORMAL_FRAME,
  arm_prologue_this_id,
  arm_prologue_prev_register
};

static const struct frame_unwind *
arm_prologue_unwind_sniffer (struct frame_info *next_frame)
{
  return &arm_prologue_unwind;
}

static arm_prologue_cache_t *
arm_make_stub_cache (struct frame_info *next_frame)
{
  arm_prologue_cache_t *cache;

  cache = frame_obstack_zalloc (sizeof (arm_prologue_cache_t));
  cache->saved_regs = trad_frame_alloc_saved_regs (next_frame);

  cache->prev_sp = frame_unwind_register_unsigned (next_frame, ARM_SP_REGNUM);

  return cache;
}

/* Our frame ID for a stub frame is the current SP and LR.  */

static void
arm_stub_this_id (struct frame_info *next_frame,
		  void **this_cache,
		  struct frame_id *this_id)
{
  arm_prologue_cache_t *cache;

  if (*this_cache == NULL)
    *this_cache = arm_make_stub_cache (next_frame);
  cache = *this_cache;

  *this_id = frame_id_build (cache->prev_sp,
			     frame_pc_unwind (next_frame));
}

struct frame_unwind arm_stub_unwind = {
  NORMAL_FRAME,
  arm_stub_this_id,
  arm_prologue_prev_register
};

#ifndef TM_NEXTSTEP

static const struct frame_unwind *
arm_stub_unwind_sniffer (struct frame_info *next_frame)
{
  gdb_byte dummy[4];

  if (in_plt_section (frame_unwind_address_in_block (next_frame), NULL)
      || target_read_memory (frame_pc_unwind (next_frame), dummy, 4) != 0)
    return &arm_stub_unwind;

  return NULL;
}

#endif

static CORE_ADDR
arm_normal_frame_base (struct frame_info *next_frame, void **this_cache)
{
  arm_prologue_cache_t *cache;

  if (*this_cache == NULL)
    *this_cache = arm_make_prologue_cache (next_frame);
  cache = *this_cache;

  return cache->prev_sp + cache->frameoffset - cache->framesize;
}

struct frame_base arm_normal_base = {
  &arm_prologue_unwind,
  arm_normal_frame_base,
  arm_normal_frame_base,
  arm_normal_frame_base
};

#ifdef TM_NEXTSTEP  
/* APPLE LOCAL: Install the Mac OS X specific sigtramp sniffer.

   Mac OS X _sigtramp frame format

[FP+  0] previous FP
[FP+  4] previous PC
[FP+  8] arg
[FP+ 12] siginfo.si_signo
[FP+ 16] siginfo.si_errno
[FP+ 20] siginfo.si_code
[FP+ 24] siginfo.si_pid
[FP+ 28] siginfo.si_uid
[FP+ 32] siginfo.si_status
[FP+ 36] siginfo.si_addr
[FP+ 40] siginfo.si_value
[FP+ 44] siginfo.si_band
[FP+ 48] siginfo.__pad[0]
[FP+ 52] siginfo.__pad[1]
[FP+ 56] siginfo.__pad[2]
[FP+ 60] siginfo.__pad[3]
[FP+ 64] siginfo.__pad[4]
[FP+ 68] siginfo.__pad[5]
[FP+ 72] siginfo.__pad[6]
[FP+ 76] ucontext.uc_onstack
[FP+ 80] ucontext.uc_sigmask
[FP+ 84] ucontext.uc_stack.ss_sp
[FP+ 88] ucontext.uc_stack.ss_size
[FP+ 92] ucontext.uc_stack.ss_flags
[FP+ 96] ucontext.uc_link
[FP+100] ucontext.uc_mcsize
[FP+104] ucontext.uc_mcontext
[FP+108] mcontext.es.__exception
[FP+112] mcontext.es.__fsr
[FP+116] mcontext.es.__far
[FP+120] mcontext.ss.R0		 ARM_THREAD_STATE
[FP+124] mcontext.ss.R1
[FP+128] mcontext.ss.R2
[FP+132] mcontext.ss.R3
[FP+136] mcontext.ss.R4
[FP+140] mcontext.ss.R5
[FP+144] mcontext.ss.R6
[FP+148] mcontext.ss.R7
[FP+152] mcontext.ss.R8
[FP+156] mcontext.ss.R9
[FP+160] mcontext.ss.R10
[FP+164] mcontext.ss.R11
[FP+168] mcontext.ss.R12
[FP+172] mcontext.ss.SP
[FP+176] mcontext.ss.LR
[FP+180] mcontext.ss.PC
[FP+184] mcontext.ss.CPSR
[FP+188] mcontext.fs.s0		  ARM_VFP_STATE
[FP+192] mcontext.fs.s1
[FP+196] mcontext.fs.s2
[FP+200] mcontext.fs.s3
[FP+204] mcontext.fs.s4
[FP+208] mcontext.fs.s5
[FP+212] mcontext.fs.s6
[FP+216] mcontext.fs.s7
[FP+220] mcontext.fs.s8
[FP+224] mcontext.fs.s9
[FP+228] mcontext.fs.s10
[FP+232] mcontext.fs.s11
[FP+236] mcontext.fs.s12
[FP+240] mcontext.fs.s13
[FP+244] mcontext.fs.s14
[FP+248] mcontext.fs.s15
[FP+252] mcontext.fs.s16
[FP+256] mcontext.fs.s17
[FP+260] mcontext.fs.s18
[FP+264] mcontext.fs.s19
[FP+268] mcontext.fs.s20
[FP+272] mcontext.fs.s21
[FP+276] mcontext.fs.s22
[FP+280] mcontext.fs.s23
[FP+284] mcontext.fs.s24
[FP+288] mcontext.fs.s25
[FP+292] mcontext.fs.s26
[FP+296] mcontext.fs.s27
[FP+300] mcontext.fs.s28
[FP+304] mcontext.fs.s29
[FP+308] mcontext.fs.s30
[FP+312] mcontext.fs.s31
[FP+316] mcontext.fs.FPSCR

*/
/* Define register sizes and register group sizes for the general purpose,
   FP and VFP registers.  */
enum 
{ 
  GP_REG_SIZE = 4,
  FP_REG_SIZE = 12,
  VFP_REG_SIZE = 4,
  SR_REG_SIZE = 4,
  EXC_STATE_SIZE = (3 * GP_REG_SIZE), 
  GP_STATE_SIZE = (((ARM_MACOSX_NUM_GP_REGS) * GP_REG_SIZE) + SR_REG_SIZE),
  FP_STATE_SIZE = (((ARM_MACOSX_NUM_FP_REGS) * FP_REG_SIZE) + SR_REG_SIZE),
  VFP_STATE_SIZE = (((ARM_MACOSX_NUM_VFP_REGS) * VFP_REG_SIZE) + SR_REG_SIZE) 
};

static arm_prologue_cache_t *
arm_macosx_sigtramp_frame_cache (struct frame_info *next_frame, void **this_cache)
{

  arm_prologue_cache_t *cache = NULL;
  CORE_ADDR mcontext_addr = 0;
  CORE_ADDR gpr_addr = 0;
  CORE_ADDR fpr_addr = 0;
  CORE_ADDR vfp_addr = 0;
  ULONGEST mcontext_size = 0;
  CORE_ADDR frame_base = 0;
  int reg_size = 0;
  unsigned int i = 0;

  /* Have we already built this cache?  */
  if (*this_cache)
    return *this_cache;

  /* Make a normal frame cache first so we can tell a bit about where
     we are in the _sigtramp frame and so we can get our frame base.  */
  *this_cache = cache = arm_make_prologue_cache (next_frame);

  /* Figure out what our frame base is so we can reliably find the
     sigtramp information on the frame.  */
  frame_base = cache->prev_sp + cache->frameoffset - cache->framesize;

  /* Extract the ucontext.uc_mcsize and ucontext.uc_mcontext.  */
  mcontext_size = read_memory_unsigned_integer (frame_base + 100, GP_REG_SIZE);
  mcontext_addr = read_memory_unsigned_integer (frame_base + 104, GP_REG_SIZE);

  /* Determine what registers are saved in the signal context based on
     the size of the mcontext structure.  */
  switch (mcontext_size)
    {
    case (EXC_STATE_SIZE + GP_STATE_SIZE + VFP_STATE_SIZE):
      vfp_addr = mcontext_addr + EXC_STATE_SIZE + GP_STATE_SIZE;
      gpr_addr = mcontext_addr + EXC_STATE_SIZE;
      break;

    case (EXC_STATE_SIZE + GP_STATE_SIZE):
      gpr_addr = mcontext_addr + EXC_STATE_SIZE;
      break;

    case (EXC_STATE_SIZE + GP_STATE_SIZE + FP_STATE_SIZE):
      fpr_addr = mcontext_addr + EXC_STATE_SIZE + GP_STATE_SIZE;
      gpr_addr = mcontext_addr + EXC_STATE_SIZE;
      break;

    default:
      warning ("unrecognized length (0x%lx) for sigtramp context",
               (unsigned long) mcontext_size);
      break;
    }

  /* Use any addresses found to extract the information from the 
     context structure.  */
  if (gpr_addr != 0)
    {
      reg_size = GP_REG_SIZE;
      for (i = 0; i < ARM_MACOSX_NUM_GP_REGS; i++)
	cache->saved_regs[ARM_R0_REGNUM + i].addr = gpr_addr + (i * reg_size);
      cache->saved_regs[ARM_PS_REGNUM].addr = gpr_addr + (i * reg_size);
    }

  if (fpr_addr != 0)
    {
      reg_size = FP_REGISTER_SIZE;
      for (i = 0; i < ARM_MACOSX_NUM_FP_REGS; i++)
	cache->saved_regs[ARM_F0_REGNUM + i].addr = fpr_addr + (i * reg_size);
      cache->saved_regs[ARM_FPS_REGNUM].addr = fpr_addr + (i * reg_size);
    }

  if (vfp_addr != 0)
    {
      reg_size = VFP_REG_SIZE;
      for (i = 0; i < 32; i++)
	cache->saved_regs[ARM_VFP_REGNUM_S0 + i].addr = vfp_addr + (i * reg_size);
      cache->saved_regs[ARM_VFP_REGNUM_FPSCR].addr = vfp_addr + (i * reg_size);
    }

  return cache;
}

static void
arm_macosx_sigtramp_frame_this_id (struct frame_info *next_frame, 
				   void **this_cache, struct frame_id *this_id)
{
  struct frame_id id;
  CORE_ADDR func;
  arm_prologue_cache_t *cache;

  if (*this_cache == NULL)
    *this_cache = arm_macosx_sigtramp_frame_cache (next_frame, this_cache);
    
  cache = *this_cache;

  func = frame_func_unwind (next_frame);

  /* If we didn't get anything sensical from a call to frame_func_unwind, 
     see if we were able to find our prologue start in our arch specific 
     prologue cache information and use it. This will allow us to stack 
     crawl through code that has no symbols.  */
  if (func == 0 && cache != NULL)
    func = cache->prologue_start;

  /* If we've hit a wall, stop.  */
  if (cache->prev_sp == 0)
    return;

  id = frame_id_build (cache->prev_sp, func);
  *this_id = id;
}

/* Extract a register from our sigtramp cache.  */
static void
arm_macosx_sigtramp_frame_prev_register (struct frame_info *next_frame,
                                  void **this_cache,
                                  int regnum, int *optimizedp,
                                  enum lval_type *lvalp, CORE_ADDR * addrp,
                                  int *realnump, gdb_byte *valuep)
{
  arm_prologue_cache_t *cache;

  if (*this_cache == NULL)
    *this_cache = arm_macosx_sigtramp_frame_cache (next_frame, this_cache);
    
  cache = *this_cache;

  enum opt_state e_opt;
  trad_frame_get_prev_register (next_frame, cache->saved_regs, regnum,
				optimizedp ? &e_opt : NULL, lvalp, addrp, 
				realnump, valuep);

  if (optimizedp)
    *optimizedp = e_opt;
}

static const struct frame_unwind arm_macosx_sigtramp_frame_unwind = {
  SIGTRAMP_FRAME,
  arm_macosx_sigtramp_frame_this_id,
  arm_macosx_sigtramp_frame_prev_register
};


static const struct frame_unwind *
arm_macosx_sigtramp_unwind_sniffer (struct frame_info *next_frame)
{
  /* Function statics that will keep a cached address range from the
     expensive find_pc_partial_function function results.  */
  static struct minimal_symbol *g_sigtramp_msymbol = NULL;
  static CORE_ADDR g_sigtramp_start = 0;
  static CORE_ADDR g_sigtramp_end = 0;
  
  /* Lookup the minimal symbol for _sigtramp.  */
  struct minimal_symbol *msymbol;
  msymbol = lookup_minimal_symbol ("_sigtramp", NULL, NULL);
  if (msymbol)
    {
      /* Check if we need to cache (or re-cache) the results.  */
      CORE_ADDR addr = SYMBOL_VALUE_ADDRESS (msymbol);
      if (msymbol != g_sigtramp_msymbol || addr != g_sigtramp_start)
        {
          char *name = NULL;
          CORE_ADDR start = 0;
          CORE_ADDR end = 0;
          if (find_pc_partial_function (addr, &name, &start, &end))
            {
              /* Make sure nothing went awry in the address to name and
                 function bounds lookup.  */
              if (name && strcmp ("_sigtramp", name) == 0)
                {
                  g_sigtramp_msymbol = msymbol;
                  g_sigtramp_start = start;
                  g_sigtramp_end = end;
                }
            }
        }
    }
  
  if (g_sigtramp_start != 0)
    {
      CORE_ADDR pc = frame_pc_unwind (next_frame);

      if (pc >= g_sigtramp_start && pc < g_sigtramp_end)
        return &arm_macosx_sigtramp_frame_unwind;
    }
  return NULL;
}

#else
static arm_prologue_cache_t *
arm_make_sigtramp_cache (struct frame_info *next_frame)
{
  arm_prologue_cache_t *cache;
  int reg;

  cache = frame_obstack_zalloc (sizeof (arm_prologue_cache_t));

  cache->prev_sp = frame_unwind_register_unsigned (next_frame, ARM_SP_REGNUM);

  cache->saved_regs = trad_frame_alloc_saved_regs (next_frame);

  for (reg = 0; reg < NUM_REGS; reg++)
    cache->saved_regs[reg].addr
      = SIGCONTEXT_REGISTER_ADDRESS (cache->prev_sp,
				     frame_pc_unwind (next_frame), reg);

  /* FIXME: What about thumb mode?  */
  cache->framereg = ARM_SP_REGNUM;
  cache->prev_sp
    = read_memory_integer (cache->saved_regs[cache->framereg].addr,
			   register_size (current_gdbarch, cache->framereg));

  return cache;
}

static void
arm_sigtramp_this_id (struct frame_info *next_frame,
		      void **this_cache,
		      struct frame_id *this_id)
{
  arm_prologue_cache_t *cache;

  if (*this_cache == NULL)
    *this_cache = arm_make_sigtramp_cache (next_frame);
  cache = *this_cache;

  /* FIXME drow/2003-07-07: This isn't right if we single-step within
     the sigtramp frame; the PC should be the beginning of the trampoline.  */
  *this_id = frame_id_build (cache->prev_sp, frame_pc_unwind (next_frame));
}

static void
arm_sigtramp_prev_register (struct frame_info *next_frame,
			    void **this_cache,
			    int prev_regnum,
			    int *optimized,
			    enum lval_type *lvalp,
			    CORE_ADDR *addrp,
			    int *realnump,
			    gdb_byte *valuep)
{
  arm_prologue_cache_t *cache;

  if (*this_cache == NULL)
    *this_cache = arm_make_sigtramp_cache (next_frame);
  cache = *this_cache;

  trad_frame_get_prev_register (next_frame, cache->saved_regs, prev_regnum,
				optimized, lvalp, addrp, realnump, valuep);
}

struct frame_unwind arm_sigtramp_unwind = {
  SIGTRAMP_FRAME,
  arm_sigtramp_this_id,
  arm_sigtramp_prev_register
};

static const struct frame_unwind *
arm_sigtramp_unwind_sniffer (struct frame_info *next_frame)
{
  if (SIGCONTEXT_REGISTER_ADDRESS_P ()
      && legacy_pc_in_sigtramp (frame_pc_unwind (next_frame), (char *) 0))
    return &arm_sigtramp_unwind;

  return NULL;
}

#endif

/* Assuming NEXT_FRAME->prev is a dummy, return the frame ID of that
   dummy frame.  The frame ID's base needs to match the TOS value
   saved by save_dummy_frame_tos() and returned from
   arm_push_dummy_call, and the PC needs to match the dummy frame's
   breakpoint.  */

static struct frame_id
arm_unwind_dummy_id (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_id_build (frame_unwind_register_unsigned (next_frame, ARM_SP_REGNUM),
			 frame_pc_unwind (next_frame));
}

/* Given THIS_FRAME, find the previous frame's resume PC using the FP
   if it is safe to do so (if we are not the bottom most frame). If the
   PC can't be safely extracted from the frame, return DEFAULT_PC.  */

static CORE_ADDR
arm_unwind_pc_using_fp (struct frame_info *this_frame, CORE_ADDR default_pc)
{
  CORE_ADDR pc = default_pc;

  /* If this isn't the bottom most frame then lets verify that this
     matches the previous PC at [FP+4].  */
  if (get_frame_type(this_frame) == NORMAL_FRAME &&
      frame_relative_level (this_frame) > 0)
    {
      void **this_cache = frame_cache_hack (this_frame);
      arm_prologue_cache_t *cache;

      if (*this_cache == NULL)
	*this_cache = arm_make_prologue_cache ( get_next_frame (this_frame));
      cache = *this_cache;
  
      /* Verify we have a frame that isn't just the stack pointer.  */
      if (cache && cache->framereg == ARM_FP_REGNUM)
	{
	  ULONGEST fp_pc;
	  CORE_ADDR fp = get_frame_register_unsigned (this_frame, ARM_FP_REGNUM);
	  if (fp != 0 && safe_read_memory_unsigned_integer (fp+4, 4, &fp_pc))
	    {
	      if (pc != fp_pc)
		{
		  /* Our previous PC doesn't match the PC we found by walking 
		     the frame pointer. We need to determine if we should use
		     this prev_pc or not.  */
		     pc = fp_pc;	/* For now always use it.  */
		}
	    }
	}
    }
  return pc;  /* RAW PC value with the thumb bit (bit zero) still set.  */
}

/* Given THIS_FRAME, find the previous frame's resume PC (which will
   be used to construct the previous frame's ID, after looking up the
   containing function).  */

static CORE_ADDR
arm_unwind_pc (struct gdbarch *gdbarch, struct frame_info *this_frame)
{
  arm_prologue_cache_t *cache = NULL;
  CORE_ADDR pc;
  pc = frame_unwind_register_unsigned (this_frame, ARM_PC_REGNUM);
#ifdef TM_NEXTSTEP  
  cache = get_arm_prologue_cache (this_frame);
  /* APPLE LOCAL ADDITION: Try and use the frame pointer to locate the 
     previous PC.  */
  pc = arm_unwind_pc_using_fp(this_frame, pc);
#endif
  if (IS_THUMB_ADDR (pc))
    {
      if (cache)
	cache->prev_pc_is_thumb = 1;
      pc = UNMAKE_THUMB_ADDR (pc);
    }
  else
    {
      if (cache)
	cache->prev_pc_is_thumb = 0;
    }
  return pc;
}

static CORE_ADDR
arm_unwind_sp (struct gdbarch *gdbarch, struct frame_info *this_frame)
{
  return frame_unwind_register_unsigned (this_frame, ARM_SP_REGNUM);
}

/* When arguments must be pushed onto the stack, they go on in reverse
   order.  The code below implements a FILO (stack) to do this.  */

struct stack_item
{
  int len;
  struct stack_item *prev;
  void *data;
};

static struct stack_item *
push_stack_item (struct stack_item *prev, void *contents, int len)
{
  struct stack_item *si;
  si = xmalloc (sizeof (struct stack_item));
  si->data = xmalloc (len);
  si->len = len;
  si->prev = prev;
  memcpy (si->data, contents, len);
  return si;
}

static struct stack_item *
pop_stack_item (struct stack_item *si)
{
  struct stack_item *dead = si;
  si = si->prev;
  xfree (dead->data);
  xfree (dead);
  return si;
}

/* We currently only support passing parameters in integer registers.  This
   conforms with GCC's default model.  Several other variants exist and
   we should probably support some of them based on the selected ABI.  */

static CORE_ADDR
arm_push_dummy_call (struct gdbarch *gdbarch, struct value *function,
		     struct regcache *regcache, CORE_ADDR bp_addr, int nargs,
		     struct value **args, CORE_ADDR sp, int struct_return,
		     CORE_ADDR struct_addr)
{
  int argnum;
  int argreg;
  int nstack;
  struct stack_item *si = NULL;

  /* Set the return address.  For the ARM, the return breakpoint is
     always at BP_ADDR.  */
  /* XXX Fix for Thumb.  */
  regcache_cooked_write_unsigned (regcache, ARM_LR_REGNUM, bp_addr);

  /* Walk through the list of args and determine how large a temporary
     stack is required.  Need to take care here as structs may be
     passed on the stack, and we have to to push them.  */
  nstack = 0;

  argreg = ARM_A1_REGNUM;
  nstack = 0;

  /* Some platforms require a double-word aligned stack.  Make sure sp
     is correctly aligned before we start.  We always do this even if
     it isn't really needed -- it can never hurt things.  */
  sp &= ~(CORE_ADDR)(2 * DEPRECATED_REGISTER_SIZE - 1);

  /* The struct_return pointer occupies the first parameter
     passing register.  */
  if (struct_return)
    {
      if (arm_debug > 5)
	fprintf_unfiltered (gdb_stdlog, "struct return in %s = 0x%s\n",
			    REGISTER_NAME (argreg), paddr (struct_addr));
      regcache_cooked_write_unsigned (regcache, argreg, struct_addr);
      argreg++;
    }

  for (argnum = 0; argnum < nargs; argnum++)
    {
      int len;
      struct type *arg_type;
      struct type *target_type;
      enum type_code typecode;
      bfd_byte *val;

      arg_type = check_typedef (value_type (args[argnum]));
      len = TYPE_LENGTH (arg_type);
      target_type = TYPE_TARGET_TYPE (arg_type);
      typecode = TYPE_CODE (arg_type);
      val = value_contents_writeable (args[argnum]);

      /* If the argument is a pointer to a function, and it is a
	 Thumb function, create a LOCAL copy of the value and set
	 the THUMB bit in it.  */
      if (TYPE_CODE_PTR == typecode
	  && target_type != NULL
	  && TYPE_CODE_FUNC == TYPE_CODE (target_type))
	{
	  CORE_ADDR regval = extract_unsigned_integer (val, len);
	  if (arm_pc_is_thumb (regval))
	    {
	      val = alloca (len);
	      store_unsigned_integer (val, len, MAKE_THUMB_ADDR (regval));
	    }
	}

      /* Copy the argument to general registers or the stack in
	 register-sized pieces.  Large arguments are split between
	 registers and stack.  */
      while (len > 0)
	{
	  int partial_len = len < DEPRECATED_REGISTER_SIZE ? len : DEPRECATED_REGISTER_SIZE;

	  if (argreg <= ARM_LAST_ARG_REGNUM)
	    {
	      /* The argument is being passed in a general purpose
		 register.  */
	      CORE_ADDR regval = extract_unsigned_integer (val, partial_len);
	      if (arm_debug > 5)
		fprintf_unfiltered (gdb_stdlog, "arg %d in %s = 0x%s\n",
				    argnum, REGISTER_NAME (argreg),
				    phex (regval, DEPRECATED_REGISTER_SIZE));
	      regcache_cooked_write_unsigned (regcache, argreg, regval);
	      argreg++;
	    }
	  else
	    {
	      /* Push the arguments onto the stack.  */
	      if (arm_debug > 5)
		fprintf_unfiltered (gdb_stdlog, "arg %d @ sp + %d\n",
				    argnum, nstack);
	      si = push_stack_item (si, val, DEPRECATED_REGISTER_SIZE);
	      nstack += DEPRECATED_REGISTER_SIZE;
	    }
	      
	  len -= partial_len;
	  val += partial_len;
	}
    }
  /* If we have an odd number of words to push, then decrement the stack
     by one word now, so first stack argument will be dword aligned.  */
  if (nstack & 4)
    sp -= 4;

  while (si)
    {
      sp -= si->len;
      write_memory (sp, si->data, si->len);
      si = pop_stack_item (si);
    }

  /* Finally, update teh SP register.  */
  regcache_cooked_write_unsigned (regcache, ARM_SP_REGNUM, sp);

  return sp;
}

static void
print_fpu_flags (int flags)
{
  if (flags & (1 << 0))
    fputs ("IVO ", stdout);
  if (flags & (1 << 1))
    fputs ("DVZ ", stdout);
  if (flags & (1 << 2))
    fputs ("OFL ", stdout);
  if (flags & (1 << 3))
    fputs ("UFL ", stdout);
  if (flags & (1 << 4))
    fputs ("INX ", stdout);
  putchar ('\n');
}

/* Print interesting information about the floating point processor
   (if present) or emulator.  */
static void
arm_print_float_info (struct gdbarch *gdbarch, struct ui_file *file,
		      struct frame_info *frame, const char *args)
{
  unsigned long status = read_register (ARM_FPS_REGNUM);
  int type;

  type = (status >> 24) & 127;
  if (status & (1 << 31))
    printf (_("Hardware FPU type %d\n"), type);
  else
    printf (_("Software FPU type %d\n"), type);
  /* i18n: [floating point unit] mask */
  fputs (_("mask: "), stdout);
  print_fpu_flags (status >> 16);
  /* i18n: [floating point unit] flags */
  fputs (_("flags: "), stdout);
  print_fpu_flags (status);
}


/* Return the GDB type object for the "standard" data type of data in
   register N.  */

static struct type *
arm_register_type (struct gdbarch *gdbarch, int regnum)
{
  /* APPLE LOCAL: Use register info table.   */
  if (regnum < g_register_info_count && g_register_info[regnum].type)
    return *g_register_info[regnum].type;
;
  return builtin_type_int32;
}

/* Index within `registers' of the first byte of the space for
   register N.  */

static int
arm_register_byte (int regnum)
{
  /* APPLE LOCAL: Use register info table.   */
  if (regnum < g_register_info_count)
    return g_register_info[regnum].offset;
  return 0;
}

/* Map GDB internal REGNUM onto the Arm simulator register numbers.  */
static int
arm_register_sim_regno (int regnum)
{
  int reg = regnum;
  gdb_assert (reg >= 0 && reg < NUM_REGS);

  if (reg < NUM_GREGS)
    return SIM_ARM_R0_REGNUM + reg;
  reg -= NUM_GREGS;

  if (reg < NUM_FREGS)
    return SIM_ARM_FP0_REGNUM + reg;
  reg -= NUM_FREGS;

  if (reg < NUM_SREGS)
    return SIM_ARM_FPS_REGNUM + reg;
  reg -= NUM_SREGS;

  internal_error (__FILE__, __LINE__, _("Bad REGNUM %d"), regnum);
}

/* NOTE: cagney/2001-08-20: Both convert_from_extended() and
   convert_to_extended() use floatformat_arm_ext_littlebyte_bigword.
   It is thought that this is is the floating-point register format on
   little-endian systems.  */

static void
convert_from_extended (const struct floatformat *fmt, const void *ptr,
		       void *dbl)
{
  DOUBLEST d;
  if (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG)
    floatformat_to_doublest (&floatformat_arm_ext_big, ptr, &d);
  else
    floatformat_to_doublest (&floatformat_arm_ext_littlebyte_bigword,
			     ptr, &d);
  floatformat_from_doublest (fmt, &d, dbl);
}

static void
convert_to_extended (const struct floatformat *fmt, void *dbl, const void *ptr)
{
  DOUBLEST d;
  floatformat_to_doublest (fmt, ptr, &d);
  if (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG)
    floatformat_from_doublest (&floatformat_arm_ext_big, &d, dbl);
  else
    floatformat_from_doublest (&floatformat_arm_ext_littlebyte_bigword,
			       &d, dbl);
}

static int
condition_true (uint32_t cond, uint32_t status_reg)
{
  if (cond == INST_AL || cond == INST_NV)
    return 1;

  switch (cond)
    {
    case INST_EQ:
      return ((status_reg & FLAG_Z) != 0);
    case INST_NE:
      return ((status_reg & FLAG_Z) == 0);
    case INST_CS:
      return ((status_reg & FLAG_C) != 0);
    case INST_CC:
      return ((status_reg & FLAG_C) == 0);
    case INST_MI:
      return ((status_reg & FLAG_N) != 0);
    case INST_PL:
      return ((status_reg & FLAG_N) == 0);
    case INST_VS:
      return ((status_reg & FLAG_V) != 0);
    case INST_VC:
      return ((status_reg & FLAG_V) == 0);
    case INST_HI:
      return  ((status_reg & FLAG_C) != 0 && (status_reg & FLAG_Z) == 0);
    case INST_LS:
      return !((status_reg & FLAG_C) != 0 && (status_reg & FLAG_Z) == 0);
    case INST_GE:
      return  (((status_reg & FLAG_N) == 0) == ((status_reg & FLAG_V) == 0));
    case INST_LT:
      return !(((status_reg & FLAG_N) == 0) == ((status_reg & FLAG_V) == 0));
    case INST_GT:
      return  (((status_reg & FLAG_Z) == 0) &&
              (((status_reg & FLAG_N) == 0) == ((status_reg & FLAG_V) == 0)));
    case INST_LE:
      return !(((status_reg & FLAG_Z) == 0) &&
              (((status_reg & FLAG_N) == 0) == ((status_reg & FLAG_V) == 0)));
    }
  return 1;
}

/* APPLE LOCAL: Centralize the prologue scan and skip code. */

/* Callback for arm_macosx_scan_prologue for the "mov ip, sp" opcode.  */
static int
arm_scan_prolog_insn_mov_ip_sp (const uint32_t insn, 
				arm_prologue_cache_t *cache,
				arm_prologue_state_t *state)
{
  state->ip_offset = 0;
  return prolog_yes;
}

/* Callback for arm_macosx_scan_prologue that scans prologue instructions that
   are addressing mode 1 data processing commands whose shifter operand 
   is an immediate (bit 25 set).  */
static int
arm_scan_prolog_insn_data_proc_imm (const uint32_t insn, 
				    arm_prologue_cache_t *cache,
				    arm_prologue_state_t *state)
{
  uint32_t data_processing_op = bits (insn, 21, 24);
  int Rd = bits (insn, 12, 15);
  int Rn = bits (insn, 16, 19);
  int handled = 0;

  uint32_t imm = data_proc_immediate (insn);

  if (Rd == ARM_FP_REGNUM)
    {
      /* Data processing instructions to the FP.  */
      if (Rn == ARM_SP_REGNUM)
	{
	  switch (data_processing_op)
	    {
	      case ARM_DATA_PROC_OP_ADD:  /* add fp, sp #n  */
		state->fp_offset = imm + state->sp_offset;	    
		if (cache) 
		  cache->framereg = ARM_FP_REGNUM;
		handled = 1;
		break;
	    }
	}
      else if (Rn == ARM_IP_REGNUM)
	{
	  switch (data_processing_op)
	    {
	      case ARM_DATA_PROC_OP_SUB:  /* sub fp, ip #n  */
		state->fp_offset = -imm + state->ip_offset;
		if (cache) 
		  cache->framereg = ARM_FP_REGNUM;
		handled = 1;
		break;
	    }
	}
    }
  else if (Rd == ARM_IP_REGNUM)
    {
      /* Data processing instructions to the IP.  */
       if (Rn == ARM_SP_REGNUM)
	{
     	  switch (data_processing_op)
	    {
	      case ARM_DATA_PROC_OP_SUB:  /* sub ip, sp #n */
		state->ip_offset = -imm;
		handled = 1;
		break;
		
	      case ARM_DATA_PROC_OP_ADD:  /* add ip, sp #n */
		state->ip_offset = imm;
		handled = 1;
		break;
	    }
	}
    }
  else if (Rd == ARM_SP_REGNUM)
    {
      /* Data processing instructions to the SP.  */
      if (Rn == ARM_SP_REGNUM)
	{
     	  switch (data_processing_op)
	    {
	      case ARM_DATA_PROC_OP_SUB:  /* sub sp, sp #n */
		state->sp_offset -= imm; 
		handled = 1;
		break;
	    }
	}
    }

  if (!handled)
      warning ("Unhandled case in arm_scan_prolog_insn_arithmetic_rd_sp_imm "
	       "for instruction 0x%8.8x\n", insn);
  return prolog_yes;
}

/* Callback for arm_macosx_scan_prologue that scans prologue instructions that
   are store operations to the SP that also update the SP.  */
static int
arm_scan_prolog_insn_str_rd_sp (const uint32_t insn, 
				arm_prologue_cache_t *cache,
				arm_prologue_state_t *state)
{
    /* str Rd, [sp, #-n]! */
    long Rd = bits (insn, 12, 15);
    long offset_12 = (insn & 0xfff);
    state->sp_offset -= offset_12;
    if (cache) 
      cache->saved_regs[Rd].addr = state->sp_offset;
    return prolog_yes;
}

/* Callback for arm_macosx_scan_prologue that scans prologue instructions that
   are store multiple register values by first decrementing the SP by
   the number of registers and then stores them sequentially in memory.  */
static int
arm_scan_prolog_insn_stmfd_sp (const uint32_t insn, 
			       arm_prologue_cache_t *cache,
			       arm_prologue_state_t *state)
{
  /* stmfd sp!,{...} */
  int mask = insn & 0xffff;
  int regno;
  
  /* Calculate offsets of saved registers.  */
  for (regno = ARM_PC_REGNUM; regno >= 0; regno--)
    {
      if (mask & (1 << regno))
	{
	  state->sp_offset -= 4;
	  if (cache) 
	    cache->saved_regs[regno].addr = state->sp_offset;
	}
    }
  return prolog_yes;
}

static int
arm_scan_prolog_insn_stfe_fn_sp_minus_12 (const uint32_t insn, 
					  arm_prologue_cache_t *cache,
					  arm_prologue_state_t *state)
{
  /* stfe f?, [sp, -#c]! */
  int regno = ARM_F0_REGNUM + ((insn >> 12) & 0x07);
  state->sp_offset -= 12;
  if (cache)
    cache->saved_regs[regno].addr = state->sp_offset;
  return prolog_yes;
}


static int
arm_scan_prolog_insn_sfmfd_fn_4_sp (const uint32_t insn, 
				    arm_prologue_cache_t *cache,
				    arm_prologue_state_t *state)
{
  /* sfmfd f0, 4, [sp!] */
  int n_saved_fp_regs;
  uint32_t fp_start_reg, fp_bound_reg;

  if ((insn & 0x800) == 0x800)		/* N0 is set */
    {
      if ((insn & 0x40000) == 0x40000)	/* N1 is set */
	n_saved_fp_regs = 3;
      else
	n_saved_fp_regs = 1;
    }
  else
    {
      if ((insn & 0x40000) == 0x40000)	/* N1 is set */
	n_saved_fp_regs = 2;
      else
	n_saved_fp_regs = 4;
    }

  fp_start_reg = ARM_F0_REGNUM + ((insn >> 12) & 0x7);
  fp_bound_reg = fp_start_reg + n_saved_fp_regs;
  for (; fp_start_reg < fp_bound_reg; fp_start_reg++)
    {
      state->sp_offset -= 12;
      if (cache)
	cache->saved_regs[fp_start_reg++].addr = state->sp_offset;
    }

  return prolog_yes;
}

static int
arm_scan_prolog_insn_fstmdb (const uint32_t insn, arm_prologue_cache_t *cache,
			     arm_prologue_state_t *state)
{
  /* fstmdb sp!, {...} ; for single, double and and unknown precisions. */
  int32_t lo_reg = -1; 
  int32_t hi_reg = -1;
  uint32_t float_op = bits (insn, 8, 11);
  uint32_t offset = insn & 0xFFu;
  int32_t Sn; /* Must be a signed value.  */

  switch (float_op)
    {
      case 0xa:	/* 0b1010 - Single Precision VFP regs (s0-s31) */
	{
	  uint32_t Fd = bits (insn, 12, 15);
	  uint32_t d = bit (insn, 22); 
	  /* Since Single precision regs go from 0-31, and Fd is only
	     4 bits, we need to use bit 22 as the LSBit:
	       - Sn[4:1] = Fd
	       - Sn[0] = d  */
	  lo_reg = (Fd << 1) + d; 
	  hi_reg = lo_reg + offset - 1;
	}
	break;

      case 0xb:	/* 0b1010 - Double Precision VFP regs (d0-d15) */
	{
	  uint32_t Dd = bits (insn, 12, 15);
	  /* Skip bit zero of the offset in bits 7:0 since if it is set, 
	     it implies this insn is a fstmdbx and it doesn't change how
	     we handle the two.  */
	  offset = offset & 0xFEu;
	  lo_reg = (Dd * 2) + ARM_VFP_REGNUM_S0;
	  hi_reg = lo_reg + offset - 1;
	}
	break;
      
      default:
	warning (_("arm_scan_prolog_insn_fstmdb unhandled float op: 0x%x"), 
		 float_op);
	return prolog_yes;
    }
    
  /* Calculate offsets of saved registers for the Single Precision VFP
     regs only, since the values of the double precisions registers will
     also get updated. Note the Sn must be signed integer for this to work
     in all cases. */
  if (lo_reg >= 0 && hi_reg >= lo_reg)
    {
      for (Sn = hi_reg; Sn >= lo_reg; Sn--)
	{
	  state->sp_offset -= 4;
	  if (cache)
	    cache->saved_regs[Sn].addr = state->sp_offset;
	}
    }
  else
    {
      warning (_("arm_scan_prolog_insn_fstmdb got invalid register range: "
	       "s%d-s%d"), lo_reg, hi_reg);
    }
  return prolog_yes;
}

static int 
thumb_scan_prolog_insn_blx (const uint32_t insn, 
			    arm_prologue_cache_t *cache,
			    arm_prologue_state_t *state)
{
  /* We need to check if the branch and link branches to any supported
     functions that push registers onto the stack. Currently this involves
     a call to a "___save_vfp_d8_d15_regs" which saves d8-d15 on the stack
     in ARM code and returns to thumb code.  */
  if (bit (insn, 12) == 0)
    {
      /* We have a BLX.  */
      
      uint32_t S = bit(insn, 26);
      uint32_t imm10H = bits(insn, 16, 25);
      uint32_t J1 = bit(insn, 13);
      uint32_t J2 = bit(insn, 11);
      uint32_t imm10L = bits(insn, 1, 10);
      uint32_t I1 = !(J1 ^ S);
      uint32_t I2 = !(J2 ^ S);
      uint32_t offset = (I1 << 23) + (I2 << 22) + (imm10H << 12) + (imm10L << 2);
      if (S)
	{
	  uint32_t sign_mask = -(1 << 24);
	  offset |= sign_mask; /* Sign extend.  */
	}
      uint32_t blx_pc = (state->pc & 0xFFFFFFFC) + 4 + offset;
      struct minimal_symbol *msymbol;
      msymbol = lookup_minimal_symbol_by_pc (blx_pc);
      if (msymbol)
	{
	  char *name = SYMBOL_NATURAL_NAME (msymbol);
	  if (name && strcmp(name, "__save_vfp_d8_d15_regs") == 0)
	    {
	      /* Register d8 is equivalent to s15 and s16, so below we save
		 the registers d8 - d15 as s15 - s31 since this is how they
		 are known to gdb.  */
	      int regno;
	      int regno_hi = ARM_VFP_REGNUM_S31;
	      int regno_lo = ARM_VFP_REGNUM_S0 + 16;
	      int reg_size = 4;
	      for (regno = regno_hi; regno >= regno_lo; regno--)
		{
		  if (cache)
		    {
		      cache->framesize += reg_size;
		      if (state->findmask & THUMB_PROLOGUE_FP_SETUP)
			cache->frameoffset += reg_size;
		      cache->saved_regs[state->reg_saved_in_reg[regno]].addr = -cache->framesize;
		    }
		  /* Reset saved register map.  */
		  state->reg_saved_in_reg[regno] = regno;
		}
	      return prolog_yes;
	    }
	}
    }
  return prolog_no;
}


static int
thumb2_scan_prolog_insn_stmfd_sp (const uint32_t insn, 
				  arm_prologue_cache_t *cache,
				  arm_prologue_state_t *state)
{
  /* stmfd sp!,{...} */
  int mask = insn & 0xffff;
  int regno;
  state->findmask |= THUMB_PROLOGUE_PUSH; /* push found */

  /* Calculate offsets of saved R0-R7 and LR.  */
  for (regno = ARM_LR_REGNUM; regno >= 0; regno--)
    {
      if (mask & (1 << regno))
	{
	  if (cache)
	    {
	      cache->framesize += 4;
	      if (state->findmask & THUMB_PROLOGUE_FP_SETUP)
		cache->frameoffset += 4;	
	      cache->saved_regs[state->reg_saved_in_reg[regno]].addr = -cache->framesize;
	    }
	  /* Reset saved register map.  */
	  state->reg_saved_in_reg[regno] = regno;
	}
    }
  return prolog_yes;
}


static int 
thumb_scan_prolog_insn_push (const uint32_t insn, 
			     arm_prologue_cache_t *cache,
			     arm_prologue_state_t *state)			     
{
  int regno;
  state->findmask |= THUMB_PROLOGUE_PUSH; /* push found */
  /* Bits 0-7 contain a mask for registers R0-R7.  Bit 8 says
     whether to save LR (R14).  */
  uint32_t mask = (insn & 0xff) | ((insn & 0x100) << 6);

  /* Calculate offsets of saved R0-R7 and LR.  */
  for (regno = ARM_LR_REGNUM; regno >= 0; regno--)
    {
      if (mask & (1 << regno))
	{
	  if (cache)
	    {
	      cache->framesize += 4;
	      cache->saved_regs[state->reg_saved_in_reg[regno]].addr = -cache->framesize;
	    }
	  /* Reset saved register map.  */
	  state->reg_saved_in_reg[regno] = regno;
	}
    }
  return prolog_yes;
}

static int 
thumb_scan_prolog_insn_sub4_sp_imm (const uint32_t insn, 
				    arm_prologue_cache_t *cache,
				    arm_prologue_state_t *state)
{
  int offset;
  if ((state->findmask & THUMB_PROLOGUE_PUSH) == 0) /* before push?  */
    return prolog_yes;
  else
    state->findmask |= THUMB_PROLOGUE_SUB_SP;	  /* sub sp found */

  offset = bits (insn, 0, 6) * 4; /* get scaled offset */
  if (cache)
    {
      cache->frameoffset += offset;
      cache->framesize += offset;
    }
  return prolog_yes;
}


static int 
thumb_scan_prolog_insn_add6_r7_sp (const uint32_t insn, 
				   arm_prologue_cache_t *cache,
				   arm_prologue_state_t *state)
{
  state->findmask |= THUMB_PROLOGUE_FP_SETUP;  /* setting of r7 found */
  if (cache)
    {
      cache->framereg = THUMB_FP_REGNUM;
      /* get scaled offset */
      cache->frameoffset = (insn & 0xff) << 2;
    }
  return prolog_yes;
}

static int 
thumb_scan_prolog_insn_add_sp_rm (const uint32_t insn, 
				   arm_prologue_cache_t *cache,
				   arm_prologue_state_t *state)
{
  /* Make sure we have found our push instruction first.  */
  if ((state->findmask & THUMB_PROLOGUE_PUSH) == 0) 
    return prolog_yes; /* before push.  */

  /* add/sub sp found */
  state->findmask |= THUMB_PROLOGUE_SUB_SP;	  
  
  uint32_t Rm = bits (insn, 3, 6);
  CORE_ADDR rm_load_addr = state->reg_loaded_from_address[Rm];

  if (rm_load_addr != INVALID_ADDRESS)
    {
      int32_t offset = read_memory_integer (rm_load_addr, 4);

      if (arm_debug > 6)
	fprintf_unfiltered (gdb_stdlog, "thumb_scan_prolog_insn_add_sp_rm () "
			    "read immediate from [0x%s] = 0x%8.8x (%d)\n", 
			    paddr (rm_load_addr), offset, offset);

      if (cache)
	{
	  cache->frameoffset -= offset;
	  cache->framesize -= offset;
	}
    }
  else
    {
      /* If we see this warning it means are trying to add sp, <Rm>
         and we don't know what the contents of Rm are (our state doesn't).
	 We currently handle PC relative immediate loads into registers
	 by saving the load address (see the function below named
	 thumb_scan_prolog_insn_ldr_rd_pc_relative ()). We may need to
	 add support for more ways to load a register in the future. Post
	 a warning so we know that we need to take care of such issues.  */
      warning (_("thumb_scan_prolog_insn_add_sp_rm: add sp, r%u "
	       "encountered with unknown contents for r%u"), Rm, Rm);
    }
  return prolog_yes;
}

/* If we track PC relative loads into registers we can catch large
   modifications to the stack pointer with code such as:
     0x00001a64:  b5b0     push   {r4, r5, r7, lr}
     0x00001a66:  af02     add    r7, sp, #8
     0x00001a68:  4c33     ldr    r4, [pc, #imm]
     0x00001a6a:  44a5     add    sp, r4
   We just need to save the load address from which reg Rm (r4 in this
   case) gets loaded from (done in thumb_scan_prolog_insn_ldr_rd_pc_rel ()) 
   and we can read the immediate data from memory (done in
   thumb_scan_prolog_insn_add_sp_rm ()) and figure out the stack offset
   for large thumb stacks.  */
static int
thumb_scan_prolog_insn_ldr_rd_pc_rel (const uint32_t insn, 
				      arm_prologue_cache_t *cache,
				      arm_prologue_state_t *state)
{
  uint32_t Rd = bits (insn, 8, 10); 
  uint32_t immed_8 = bits (insn, 0, 7);
  
  /* The PC is the value for this instruction, so we need to add four
     since the PC is 4 past the current location in the CPU pipeline. 
     We also need to mask PC bit 1 (and we do bit zero for good 
     measure) to zero. */
  state->reg_loaded_from_address[Rd] = (state->pc & 0xFFFFFFFC) + 4 + 
				       (immed_8 * 4);
  if (arm_debug > 6)
    fprintf_unfiltered (gdb_stdlog, "thumb_scan_prolog_insn_ldr_rd_pc_rel () "
			"r%u #imm @ 0x%s\n", Rd, 
			paddr (state->reg_loaded_from_address[Rd]));
  return prolog_ignore;
}


static int
thumb_scan_prolog_insn_mov_r7_sp (const uint32_t insn, 
				  arm_prologue_cache_t *cache,
				  arm_prologue_state_t *state)
{
  state->findmask |= THUMB_PROLOGUE_FP_SETUP;/* setting of r7 found */
  if (cache)
    {
      cache->framereg = THUMB_FP_REGNUM;
      cache->frameoffset = 0;
    }
  state->reg_saved_in_reg[THUMB_FP_REGNUM] = ARM_SP_REGNUM;
  return prolog_yes;
}

static int 
thumb_scan_prolog_insn_mov_rlo_rhi (const uint32_t insn, 
				    arm_prologue_cache_t *cache,
				    arm_prologue_state_t *state)
{
  int lo_reg = insn & 7;		    /* dest.  register (r0-r7) */
  int hi_reg = ((insn >> 3) & 7) + 8;	    /* source register (r8-15) */
  state->reg_saved_in_reg[lo_reg] = hi_reg; /* remember hi reg was saved */
  return prolog_ignore;
}

static int
thumb2_scan_prolog_insn_push_w (const uint32_t insn, 
				arm_prologue_cache_t *cache,
				arm_prologue_state_t *state)
{
  int regno;
  state->findmask |= THUMB_PROLOGUE_PUSH; /* push found */
  /* Bits 0-15 contain a mask for registers R0-R15.  */
  uint32_t mask = (insn & 0xffff);

  /* Calculate offsets of saved R0-R7 and LR.  */
  for (regno = ARM_LR_REGNUM; regno >= 0; regno--)
    {
      if (mask & (1 << regno))
	{
	  if (cache)
	    {
	      cache->framesize += 4;
	      if (state->findmask & THUMB_PROLOGUE_FP_SETUP)
		cache->frameoffset += 4;
	      cache->saved_regs[state->reg_saved_in_reg[regno]].addr = -cache->framesize;
	    }
	  /* Reset saved register map.  */
	  state->reg_saved_in_reg[regno] = regno;
	}
    }
  return prolog_yes;
}

static int
thumb2_scan_prolog_insn_push_w_rt (const uint32_t insn, 
				   arm_prologue_cache_t *cache,
				   arm_prologue_state_t *state)
{
  /* Decode Rt.  */
  int regno = bits (insn, 12, 15);
  if (cache)
    {
      cache->framesize += 4;
      if (state->findmask & THUMB_PROLOGUE_FP_SETUP)
	cache->frameoffset += 4;
      cache->saved_regs[state->reg_saved_in_reg[regno]].addr = -cache->framesize;
    }
  /* Reset saved register map.  */
  state->reg_saved_in_reg[regno] = regno;
  return prolog_yes;
}

static int
thumb2_scan_prolog_insn_vpush (const uint32_t insn, 
			       arm_prologue_cache_t *cache,
			       arm_prologue_state_t *state)
{
  /* sfmfd f0, 4, [sp!] */
  uint32_t imm8 = bits (insn, 0, 7);
  uint32_t d = 0;
  int single_regs = bit (insn, 8) == 0;
  int reg;
  int reg_size = single_regs ? 4 : 8;
  int regs = 0;
  if (single_regs)
    {
      d = bits (insn, 12, 15) << 1 | bit (insn, 22);
      regs = imm8;
    }
  else
    {
      if (imm8 & 1)
	{
	  /* FSTMX */
	}
      else
	{
	  d = bit (insn, 22) << 4 | bits (insn, 12, 15);
	  regs = imm8 / 2;
	}
    }

  gdb_assert (regs > 0 && regs <= 16 && (d + regs) <=32);
  for (reg = 0; reg < regs; ++reg)
    {
      int regno = d + regs - 1 - reg;
      if (single_regs)
	{
	  regno += ARM_VFP_REGNUM_S0;
	}
      else
	{
	  /* The FPSCR register is currently between D15 and D16, so
	     watch for this gap and adjust accordingly. */
	  if (regno < 16)
	    regno += ARM_VFP_REGNUM_S0;
	  else
	    regno += (ARM_VFPV3_REGNUM_D16 - 16);
	}
      state->sp_offset -= reg_size;
      if (cache)
	cache->saved_regs[regno].addr = state->sp_offset;
    }
  return prolog_yes;
}


static int
thumb2_scan_prolog_insn_sub_sp_const (const uint32_t insn, 
				      arm_prologue_cache_t *cache,
				      arm_prologue_state_t *state)
{
  if (cache)
    {
      uint32_t imm32 = thumb_expand_imm_c (insn);
      cache->framesize += imm32;
      if (state->findmask & THUMB_PROLOGUE_FP_SETUP)
	cache->frameoffset += imm32;
    }
  return prolog_yes;
}

static int
thumb2_scan_prolog_insn_sub_sp_imm12 (const uint32_t insn, 
				      arm_prologue_cache_t *cache,
				      arm_prologue_state_t *state)
{
  if (cache)
    {
      const uint32_t i = bit (insn, 26);
      const uint32_t imm3 = bits (insn, 12, 14);
      const uint32_t imm8 = bits (insn, 0, 7);
      uint32_t imm32 = (i << 11) + (imm3 << 8) + imm8;
      cache->framesize += imm32;
      if (state->findmask & THUMB_PROLOGUE_FP_SETUP)
	cache->frameoffset += imm32;
    }
  return prolog_yes;
}

static uint32_t
data_proc_immediate (const uint32_t insn)
{
  uint32_t imm = bits (insn, 0, 7);		/* immediate value */
  uint32_t rot = 2 * bits (insn, 8, 11);	/* rotate amount */
  imm = (imm >> rot) | (imm << (32 - rot));
  return imm;
}

/* APPLE LOCAL END: Centralize the prologue scan and skip code. */

/* APPLE LOCAL BEGIN: fast stacks. */

/*
 * This is set to the FAST_COUNT_STACK macro for arm.  The return value
 * is 1 if no errors were encountered traversing the stack, and 0 otherwise.
 * It sets COUNT to the stack depth.  If PRINT_FUN is non-null, then 
 * it will be passed the pc & fp for each frame as it is encountered.
 */

/*
 * COUNT_LIMIT parameter sets a limit on the number of frames that
 * will be counted by this function.  -1 means unlimited.
 *
 * PRINT_LIMIT parameter sets a limit on the number of frames for
 * which the full information is printed.  -1 means unlimited.
 *
 */

int
arm_macosx_fast_show_stack (unsigned int count_limit, unsigned int print_limit,
			    unsigned int *count,
			    void (print_fun) (struct ui_out * uiout, 
					      int frame_num,
					      CORE_ADDR pc, CORE_ADDR fp))
{
  CORE_ADDR fp, prev_fp;
  static CORE_ADDR sigtramp_start = 0;
  static CORE_ADDR sigtramp_end = 0;
  unsigned int i = 0;
  int more_frames;
  int success = 1;
  struct frame_info *fi;
  ULONGEST next_fp = 0;
  ULONGEST pc = 0;
  int wordsize = gdbarch_tdep (current_gdbarch)->wordsize;

  more_frames = fast_show_stack_trace_prologue (count_limit, print_limit, 
						wordsize, &sigtramp_start, 
						&sigtramp_end, &i, &fi, 
						print_fun);

  if (more_frames < 0)
    {
      /* An error occurred during the initial stack frames.  */
      success = 0;
    }
  else if (more_frames == 0)
    {
      /* We already have all the frames we need.  */
      success = 1;
    }
  else if (i < count_limit)
    {
      /* We got some stack frames and still need more.  */
      arm_prologue_cache_t *cache = get_arm_prologue_cache (fi);
      fp = get_frame_register_unsigned (fi, cache ? cache->framereg : 
					   ARM_FP_REGNUM);
      prev_fp = fp;
      int done = (fp == 0);

      while (!done && i < count_limit)
	{
	  int add_frame = 0;
	  CORE_ADDR next_fp_addr = 0;
	  CORE_ADDR next_pc_addr = 0;
	  if ((sigtramp_start <= pc) && (pc < sigtramp_end))
	    {
	      CORE_ADDR mcontext_addr;
	      CORE_ADDR gpr_addr;
	      /* We are in signal trampoline.  */
	      mcontext_addr = read_memory_unsigned_integer (fp + 104, GP_REG_SIZE);
	      gpr_addr = mcontext_addr + EXC_STATE_SIZE;
	      next_fp_addr = gpr_addr + (ARM_FP_REGNUM * GP_REG_SIZE);
	      next_pc_addr = gpr_addr + (ARM_PC_REGNUM * GP_REG_SIZE);
	    }        
	  else
	    {
	      /* We have a normal frame.  */
	      next_fp_addr = fp;
	      next_pc_addr = fp + 4;
	    }
	    
	  if (next_fp_addr != 0 && next_pc_addr != 0)
	    {
	      /* Read the next FP by dereferencing the current FP.  */
	      if (safe_read_memory_unsigned_integer (next_fp_addr, GP_REG_SIZE, 
						     &next_fp))
		{
		  if (next_fp == 0)
		    done = 1; /* normal end of our FP chain.  */
		  else if (next_fp == fp)
		    {
		      warning ("Frame pointer point back at the previous frame");
		      done = 1; /* Avoid infinite loop.  */
		      success = 0;  /* This is not good, return error... */
		    }
		  else 
		    {
		      /* Read the previous PC value.  */
		      if (safe_read_memory_unsigned_integer (next_pc_addr, 
							     GP_REG_SIZE, &pc))
			{
			  if (pc == 0)
			    done = 1;
			  else
			    add_frame = 1; 
			}
		      else
			{
			  done = 1; /* Couldn't read the previous PC.  */
			}
		    }
		}
	      else
		{
		  done = 1; /* Couldn't read the previous FP.  */
		}
	    }
	  else
	    {
	      done = 1; /* Invalid previous FP and PC addresses.  */
	    }

	  if (add_frame)
	    {
	      prev_fp = fp;
	      fp = next_fp;
	      /* Strip bit zero (thumb bit) for any return addresses since
	         we read this from memory.  */
	      pc = ADDR_BITS_REMOVE (pc);
	      pc_set_load_state (pc, OBJF_SYM_ALL, 0);
      
	      if (print_fun && (i < print_limit))
		print_fun (uiout, i, pc, fp);
	      i++;

	      if (!backtrace_past_main && addr_inside_main_func (pc))
		done = 1;
	    }
	  else
	    done = 1;
	}
    }

  if (print_fun)
    ui_out_end (uiout, ui_out_type_list);

  *count = i;
  return success;
}

static uint32_t
shifted_reg_val (uint32_t insn, int carry, uint32_t pc_val,
		 uint32_t status_reg)
{
  uint32_t res, shift;
  int rm = bits (insn, 0, 3);
  uint32_t shifttype = bits (insn, 5, 6);

  if (bit (insn, 4))
    {
      int rs = bits (insn, 8, 11);
      shift = (rs == 15 ? pc_val + 8 : read_register (rs)) & 0xFF;
    }
  else
    shift = bits (insn, 7, 11);

  res = (rm == 15
	 ? ((pc_val | (ARM_PC_32 ? 0 : status_reg))
	    + (bit (insn, 4) ? 12 : 8))
	 : read_register (rm));

  switch (shifttype)
    {
    case 0:			/* LSL */
      res = shift >= 32 ? 0 : res << shift;
      break;

    case 1:			/* LSR */
      res = shift >= 32 ? 0 : res >> shift;
      break;

    case 2:			/* ASR */
      if (shift >= 32)
	shift = 31;
      res = ((res & 0x80000000L)
	     ? ~((~res) >> shift) : res >> shift);
      break;

    case 3:			/* ROR/RRX */
      shift &= 31;
      if (shift == 0)
	res = (res >> 1) | (carry ? 0x80000000L : 0);
      else
	res = (res >> shift) | (res << (32 - shift));
      break;
    }

  return res & 0xffffffff;
}

/* Return number of 1-bits in VAL.  */

static int
bitcount (uint32_t val)
{
  int nbits;
  for (nbits = 0; val != 0; nbits++)
    val &= val - 1;		/* delete rightmost 1-bit in val */
  return nbits;
}

CORE_ADDR
thumb_get_next_pc (CORE_ADDR pc)
{
  uint32_t pc_val = ((uint32_t) pc) + 4;	/* PC after prefetch */
  const uint16_t inst1 = read_memory_integer (pc, 2);
  const int inst_is_thumb32 = IS_THUMB32_OP(inst1);
  const uint16_t inst2 = inst_is_thumb32 ? read_memory_integer (pc + 2, 2) : 0;
  CORE_ADDR nextpc = pc + (inst_is_thumb32 ? 4 : 2);
  uint32_t offset;

  const uint32_t cpsr = read_register (ARM_PS_REGNUM);

  if (arm_debug)
    {
      fprintf_unfiltered (gdb_stdlog, "thumb_get_next_pc (%s):  cpsr = 0x%8.8x, "
			  "inst = %4.4x", paddr (pc), cpsr, inst1);
      if (inst_is_thumb32)
	fprintf_unfiltered (gdb_stdlog, "%4.4x\n", inst2);
      else
	fprintf_unfiltered (gdb_stdlog, "\n");
    }


  if ((inst1 & 0xff00) == 0xbd00)	/* pop {rlist, pc} */
    {
      CORE_ADDR sp;

      /* Fetch the saved PC from the stack.  It's stored above
         all of the other registers.  */
      offset = bitcount (bits (inst1, 0, 7)) * DEPRECATED_REGISTER_SIZE;
      sp = read_register (ARM_SP_REGNUM);
      nextpc = (CORE_ADDR) read_memory_integer (sp + offset, 4);
      nextpc = ADDR_BITS_REMOVE (nextpc);
      if (nextpc == pc)
	error (_("Infinite loop detected"));
    }
  else if ((inst1 & 0xf000) == 0xd000)	/* conditional branch */
    {
      uint32_t cond = bits (inst1, 8, 11);
      if (cond != 0x0f && condition_true (cond, cpsr))    /* 0x0f = SWI */
	nextpc = pc_val + (sbits (inst1, 0, 7) << 1);
    }
  else if ((inst1 & 0xf800) == 0xe000)	/* unconditional branch */
    {
      nextpc = pc_val + (sbits (inst1, 0, 10) << 1);
    }
  else if ((inst1 & 0xf500) == 0xb100)	/* CBNZ/CBZ (ARMv6T2, ARMv7) */
    {
      uint32_t Rn = read_register (bits (inst1, 0, 2));
      uint32_t op = bit(inst1, 11);
      if (op ^ (Rn == 0))
	{
	  uint32_t i = bit(inst1, 9);
	  uint32_t imm5 = bits (inst1, 3, 7);
	  nextpc = pc_val + (i << 6) + (imm5 << 1);
	}
    }
  else if (inst_is_thumb32) /* 32 bit Thumb instruction */
    {
      if ((inst1 & 0xf800) == 0xf000 && (inst2 & 0x8000))
	{
	  /* Branches and miscellaneous control */
	  uint32_t op = bits (inst1, 4, 10);
	  uint32_t op1 = bits (inst2, 12, 14);
	  
	  switch (op1)
	    {
	      case 0: /* 000 */
	      case 2: /* 010 */
		switch (op)
		  {
		    case 0x38:
		      /* 0111000 - MSR. */
		      break;

		    case 0x3a:
		      /* 0111010 - Change Processor State. */
		      break;

		    case 0x3b:
		      /* 0111011 - Misc control instructions.  */
		      break;
		    
		    case 0x3c: 
		      /* 0111100 - Branch and Exchange Jazelle (v6T2).  */
		      break;
		      
		    case 0x3d:
		      /* 0111101 - Exception Return (v6T2).  */
		      break;
		      
		    case 0x3e:
		    case 0x3f:
		      /* 011111x - MRS (v6T2). */
		      break;		      

		    case 0x7f: /* 1111111 */
		      if (op1 == 0)
			{
			  /* Secure Monitor Call.  */
			}
		      else
			{
			  /* Permanently UNDEFINED.  */
			}
		      break;
		      
		    default:
		      if ((op & 0x38) != 0x38)
			{
			  /* not x111xxx - Conditional branch.  */
			  uint32_t cond = bits(inst1, 6, 9);
			  if (condition_true (cond, cpsr))
			    {
			      uint32_t S = bit(inst1, 10);
			      uint32_t imm6 = bits(inst1, 0, 5);
			      uint32_t J1 = bit(inst2, 13);
			      uint32_t J2 = bit(inst2, 11);
			      uint32_t imm11 = bits(inst2, 0, 10);
			      offset = (J1 << 19) + (J2 << 18) + (imm6 << 12) + 
				       (imm11 << 1);
			      if (S)
				{
				  uint32_t sign_mask = -(1 << 20);
				  offset |= sign_mask; /* Sign extend.  */
				}
			      nextpc = pc_val + offset;
			    }
			}
		      break;
		  }
		break;

	      case 1: /* 001 Branch (v6T2) Encoding T4 ARMv6T2, ARMv7. */
	      case 3: /* 011 Branch (v6T2) Encoding T4 ARMv6T2, ARMv7. */
	      case 4: /* 100 Branch with Link and Exchange (v5T) Encoding T2. */
	      case 5: /* 101 Branch with Link (v4T) Encoding T1. */
	      case 6: /* 110 Branch with Link and Exchange (v5T) Encoding T2. */
	      case 7: /* 111 Branch with Link (v4T) Encoding T1. */
		{
		  uint32_t S = bit(inst1, 10);
		  uint32_t imm10 = bits(inst1, 0, 9);
		  uint32_t J1 = bit(inst2, 13);
		  uint32_t J2 = bit(inst2, 11);
		  uint32_t imm11 = bits(inst2, 0, 10);
		  uint32_t I1 = !(J1 ^ S);
		  uint32_t I2 = !(J2 ^ S);
		  offset = (I1 << 23) + (I2 << 22) + (imm10 << 12) + (imm11 << 1);
		  if (S)
		    {
		      uint32_t sign_mask = -(1 << 24);
		      offset |= sign_mask; /* Sign extend.  */
		    }
		  nextpc = pc_val + offset;
		  
		  if (op1 == 4 || op1 == 6)
		    nextpc = (pc_val + offset) & 0xfffffffc;
		  else
		    nextpc = (pc_val + offset) & 0xfffffffe;
		}
		break;
	    }
	}
      else if ((inst1 & 0xffd0) == 0xe890 && (inst2 & 0xe000) == 0x8000)  
	{
	  /* POP<c>.W <registers> (encoding T2) that pops the PC.  */
	  /* LDM<c>.W <Rn>{!},<registers> (encoding T2) that loads the PC.  */
	    CORE_ADDR Rn;
	    /* Fetch the saved PC from the stack.  It's stored above
	       all of the other registers.  */
	    offset = bitcount (bits (inst2, 0, 12)) * DEPRECATED_REGISTER_SIZE;
	    Rn = read_register (bits (inst1, 0, 3));
	    nextpc = (CORE_ADDR) read_memory_integer (Rn + offset, 4);
	    nextpc = ADDR_BITS_REMOVE (nextpc);
	    if (nextpc == pc)
	      error (_("Infinite loop detected"));
	}
      else if (inst1 == 0xf85d && inst2 == 0xfb04)  
	{
	  /* POP<c>.W {PC} (encoding T3).  */
	  CORE_ADDR sp = read_register (ARM_SP_REGNUM);
	  nextpc = (CORE_ADDR) read_memory_integer (sp, 4);
	  nextpc = ADDR_BITS_REMOVE (nextpc);
	  if (nextpc == pc)
	    error (_("Infinite loop detected"));
	}
      else if ((inst1 & 0xfff0) == 0xf850 && bits(inst2, 6, 11) == 0)
	{
	  /* LDR<c>.W <Rt>,[<Rn>,<Rm>{,LSL #<imm2>}] (encoding T2).  */
	  uint32_t Rn = bits(inst1, 0, 3);
	  if (Rn == 15)
	    {
	      /* LDR (literal).  */
	    }
	  else
	    {
	      uint32_t Rt = bits(inst2, 12, 15);
	      if (Rt == ARM_PC_REGNUM)  
		{
		  /* Rt is the PC.  */
		  uint32_t imm2 = bits(inst2, 4, 5);
		  uint32_t Rm = bits(inst2, 0, 3);
		  offset = Rm << imm2;
		  CORE_ADDR pc_addr = Rn + offset;
		  /* make sure pc address is 4 byte aligned.  */
		  gdb_assert ((pc_addr & 3) == 0);  
		  nextpc = (CORE_ADDR) read_memory_integer (pc_addr, 4);
		  nextpc = ADDR_BITS_REMOVE (nextpc);
		  if (nextpc == pc)
		    error (_("Infinite loop detected"));
		}
	    }
	}
      else if ((inst1 & 0xfff0) == 0xe8d0 && (inst2 & 0xffe0) == 0xf000)
	{
	  /* TBB, TBH (ARMv6T2, ARMv7).  */
	  uint32_t Rn = bits(inst1, 0, 3); /* Table base address.  */	  
	  if (Rn != 13)
	    {
	      uint32_t elem_index = read_register(bits(inst2, 0, 3));
	      uint32_t H = bit(inst2, 4); /* Half word elements.  */
	      uint32_t base;
	      if (Rn == 15)
		base = pc_val;
	      else 
		base = read_register(Rn);
	      uint32_t elem_size = 1 << H;
	      uint32_t elem_addr = base + (elem_index * elem_size);
	      offset = read_memory_integer (elem_addr, elem_size) << 1;
	      nextpc = pc_val + offset;
	    }
	}
   }
  else if ((inst1 & 0xff00) == 0x4700)	/* bx REG, blx REG */
    {
      if (bits (inst1, 3, 6) == 0x0f)
	nextpc = pc_val;
      else
	nextpc = read_register (bits (inst1, 3, 6));

      nextpc = ADDR_BITS_REMOVE (nextpc);
      if (nextpc == pc)
	error (_("Infinite loop detected"));
    }
  else if ((inst1 & 0xff87) == 0x4687)  /* mov pc, <Rm> */
    {
      nextpc = read_register (bits (inst1, 3, 6));
    }
  else if ((inst1 & 0xff00) == 0xbf00)
    {
      const uint32_t mask = bits(inst1, 0, 3);
      if (mask != 0)
	{
	  /* IT or If then blocks (ARMv6T2, ARMv7).  */
	  uint32_t i;
	  uint32_t firstcond = bits(inst1, 4, 7);
	  /* If the IF/THEN condition is true, the next instruction is the one
	     that follows this one, else we need to find the next instruction
	     in the IF/THEN block that is an else instruction, or the next 
	     instruction past the end of the IT block if they are all IF
	     ops.  */
	  if (!condition_true (firstcond, cpsr))
	    {
	      typedef enum { it_op_none = 0, it_op_if, it_op_else } it_op_t;
	      uint32_t firstcond_0 = firstcond & 1;
	      it_op_t it_ops[3] = { it_op_none, it_op_none, it_op_none };
	      if (mask & 7)
		it_ops[0] = bit(mask, 3) == firstcond_0 ? it_op_if : it_op_else;

	      if (mask & 3)
		it_ops[1] = bit(mask, 2) == firstcond_0 ? it_op_if : it_op_else;

	      if (mask & 1)
		it_ops[2] = bit(mask, 1) == firstcond_0 ? it_op_if : it_op_else;

	      uint32_t num_opcodes_to_skip = 1;
	      if (it_ops[0] == it_op_if)
		{
		  num_opcodes_to_skip++;
		  if (it_ops[1] == it_op_if)
		    {
		      num_opcodes_to_skip++;
		      if (it_ops[2] == it_op_if)
			num_opcodes_to_skip++;
		    }
		}

	      for (i=0; i<num_opcodes_to_skip; ++i)
		{
		  uint16_t next_inst = read_memory_integer (nextpc, 2);
		  if (IS_THUMB32_OP(next_inst))
		    nextpc += 4;
		  else
		    nextpc += 2;
		}
	    }
	}
    }

  /* Extract IT[7:0] which is split up in the CPSR.  */
  const uint32_t if_then_state = (bits(cpsr, 10, 15) << 2) + bits(cpsr, 25, 26);

  if (if_then_state != 0) /* Are we in a Thumb IT (if-then) block?  */
    {
      /* Could NEXTPC still be in the current IT block?  */
      if (pc < nextpc && nextpc < pc + 16)
	{
	  /* Yes, the NEXTPC is possibly within the current Thumb if-then 
	     block, so we need to verify that the nextpc address isn't one 
	     that won't get executed due to the if-then-else conditions for
	     each instruction in the if-then block.  */
	  const uint32_t cond_base = bits(if_then_state, 5, 7) << 1;
	  uint32_t if_then_bits;
	  CORE_ADDR if_then_pc = pc;
	  /* The first if-then instruction in this loop is already in inst1, so
	     no need to re-read it.  */
	  uint16_t if_then_inst = inst1; 
	  for (if_then_bits = bits(if_then_state, 0, 4); 
	       if_then_bits & 0xf;
	       if_then_bits <<= 1)
	    {
	      const int if_then_inst_is_thumb32 = IS_THUMB32_OP(if_then_inst);
	      /* See if NEXTPC is one of our if-then block instructions.  */
	      if (nextpc == if_then_pc)
		{
		  /* NEXTPC is one of the if-then block instructions, so we
		     need to extract the condition for the this instruction
		     and make sure it will get executed.  */
		  const uint32_t cond = cond_base | bit(if_then_bits, 4);
		  const int cond_true = condition_true (cond, cpsr);
		  if (arm_debug)
		    fprintf_unfiltered (gdb_stdlog, 
					"0x%s: if-then cond = %i%i%i%i => %i\n",
					paddr (if_then_pc), bit(cond, 3), 
					bit(cond, 2), bit(cond, 1), 
					bit(cond, 0), cond_true);
		  if (cond_true)
		    {
		      /* We will be executing the instruction at NEXTPC, so we
		         are done with our loop!  */
		      break;
		    }
		  else
		    {
		      /* We won't be executing this instruction since the 
			 if-then-else condition didn't evaluate to true, so
			 increment NEXTPC by the size of this instruction.  */
		      if (if_then_inst_is_thumb32)
			nextpc += 4;
		      else
			nextpc += 2;
		    }
		}
	      
	      /* Increment our if-then block pc and read the next opcode.  */
	      if (if_then_inst_is_thumb32)
		if_then_pc += 4;
	      else
		if_then_pc += 2;
	      if_then_inst = read_memory_integer (if_then_pc, 2);
	    }
	}
    }
  return nextpc;
}

CORE_ADDR
arm_get_next_pc (CORE_ADDR pc)
{
  uint32_t pc_val;
  uint32_t this_instr;
  uint32_t status;
  CORE_ADDR nextpc;
  uint32_t condition;
  if (arm_pc_is_thumb (pc))
    return thumb_get_next_pc (pc);

  pc_val = (uint32_t) pc;
  this_instr = read_memory_integer (pc, 4);
  status = read_register (ARM_PS_REGNUM);
  nextpc = (CORE_ADDR) (pc_val + 4);	/* Default case */

  if (arm_debug)
    fprintf_unfiltered (gdb_stdlog, "arm_get_next_pc (%s): 0x%8.8x\n", 
			paddr (pc), this_instr);

  condition = bits (this_instr, 28, 31);
  
  if (condition == INST_NV)
    {
      /* Unconditional instructions.  */
      switch (bits (this_instr, 24, 27))
	{
	case 0x0:
	case 0x1: /* CPS, SETEND.  */
	case 0x2: /* Advanced SIMD data-processing instructions.  */
	case 0x3: /* Advanced SIMD data-processing instructions.  */
	case 0x4: /* Advanced SIMD elem or struc load/store inst, PLI.  */
	case 0x5: /* PLD, CLREX, DSB, DMB, ISB.  */
	case 0x6: /* PLI.  */
	case 0x7: /* PLD.  */
	case 0xc: /* LDC, LDC2 (immediate).  */
	case 0xd: /* LDC, LDC2 (literal), STC, STC2.  */
	case 0xe: /* CDP, CDP2, MCR, MCR2, MRC, MRC2.  */
	case 0xf:
	  /* None of these instructions modify the PC in any special way.  */
	  break;
	  
	case 0x8: /* SRS and RFE.  */
	case 0x9:
	  if ((this_instr & 0xFE50FFFF) == 0xF8100A00)	/* RFE?  */
	    {
	      uint32_t Rn = bits (this_instr, 16, 19);
	      if (Rn != 15)
		{
		  /* Return From Exception.  */
		  uint32_t P = bit (this_instr, 24);
		  uint32_t U = bit (this_instr, 23);
		  uint32_t increment = (U == 1);
		  uint32_t wordhigher = (P == U);
		  CORE_ADDR addr = read_register (Rn);
		  if (!increment)
		    addr -= 8;
		  if (wordhigher)
		    addr += 4;
		  nextpc = (CORE_ADDR) read_memory_integer ((CORE_ADDR) addr, 4);
		  nextpc = ADDR_BITS_REMOVE (nextpc);
		  if (nextpc == pc)
		    error (_("Infinite loop detected"));
		}
	    }
	  break;

	case 0xa:
	case 0xb:	/* BL, BLX (immediate).  */
	  nextpc = BranchDest (pc, this_instr);
	  nextpc |= bit (this_instr, 24) << 1;
	  nextpc = ADDR_BITS_REMOVE (nextpc);
	  if (nextpc == pc)
	    error (_("Infinite loop detected"));
	  break;
	}    
    }
  else if (condition_true (condition, status))
    {
      switch (bits (this_instr, 24, 27))
	{
	case 0x0:
	case 0x1:			/* data processing */
	case 0x2:
	case 0x3:
	  {
	    uint32_t operand1, operand2, result = 0;
	    uint32_t rn;
	    int c;

	    if (bits (this_instr, 12, 15) != 15)
	      break;

	    if (bits (this_instr, 22, 25) == 0
		&& bits (this_instr, 4, 7) == 9)	/* multiply */
	      error (_("Invalid update to pc in instruction"));

	    /* BX <reg>, BLX <reg> */
	    if (bits (this_instr, 4, 27) == 0x12fff1 || 
                bits (this_instr, 4, 27) == 0x12fff3)
	      {
		rn = bits (this_instr, 0, 3);
		result = (rn == 15) ? pc_val + 8 : read_register (rn);
		nextpc = (CORE_ADDR) ADDR_BITS_REMOVE (result);

		if (nextpc == pc)
		  error (_("Infinite loop detected"));

		return nextpc;
	      }

	    /* Multiply into PC */
	    c = (status & FLAG_C) ? 1 : 0;
	    rn = bits (this_instr, 16, 19);
	    operand1 = (rn == 15) ? pc_val + 8 : read_register (rn);

	    if (bit (this_instr, 25))
	      operand2 = data_proc_immediate (this_instr);
	    else		/* operand 2 is a shifted register */
	      operand2 = shifted_reg_val (this_instr, c, pc_val, status);

	    switch (bits (this_instr, 21, 24))
	      {
	      case ARM_DATA_PROC_OP_AND:	/*and */
		result = operand1 & operand2;
		break;

	      case ARM_DATA_PROC_OP_EOR:	/*eor */
		result = operand1 ^ operand2;
		break;

	      case ARM_DATA_PROC_OP_SUB:	/*sub */
		result = operand1 - operand2;
		break;

	      case ARM_DATA_PROC_OP_RSB:	/*rsb */
		result = operand2 - operand1;
		break;

	      case ARM_DATA_PROC_OP_ADD:	/*add */
		result = operand1 + operand2;
		break;

	      case ARM_DATA_PROC_OP_ADC:	/*adc */
		result = operand1 + operand2 + c;
		break;

	      case ARM_DATA_PROC_OP_SBC:	/*sbc */
		result = operand1 - operand2 + c;
		break;

	      case ARM_DATA_PROC_OP_RSC:	/*rsc */
		result = operand2 - operand1 + c;
		break;

	      case ARM_DATA_PROC_OP_TST:
	      case ARM_DATA_PROC_OP_TEQ:
	      case ARM_DATA_PROC_OP_CMP:
	      case ARM_DATA_PROC_OP_CMN:	/* tst, teq, cmp, cmn */
		result = (uint32_t) nextpc;
		break;

	      case ARM_DATA_PROC_OP_ORR:	/*orr */
		result = operand1 | operand2;
		break;

	      case ARM_DATA_PROC_OP_MOV:	/*mov */
		/* Always step into a function.  */
		result = operand2;
		break;

	      case ARM_DATA_PROC_OP_BIC:	/*bic */
		result = operand1 & ~operand2;
		break;

	      case ARM_DATA_PROC_OP_MVN:	/*mvn */
		result = ~operand2;
		break;
	      }
	    nextpc = (CORE_ADDR) ADDR_BITS_REMOVE (result);

	    if (nextpc == pc)
	      error (_("Infinite loop detected"));
	    break;
	  }

	case 0x4:
	case 0x5:		/* data transfer */
	case 0x6:
	case 0x7:
	  if (bit (this_instr, 20))
	    {
	      /* load */
	      if (bits (this_instr, 12, 15) == 15)
		{
		  /* rd == pc */
		  uint32_t rn;
		  uint32_t base;

		  if (bit (this_instr, 22))
		    error (_("Invalid update to pc in instruction"));

		  /* byte write to PC */
		  rn = bits (this_instr, 16, 19);
		  base = (rn == 15) ? pc_val + 8 : read_register (rn);
		  if (bit (this_instr, 24))
		    {
		      /* pre-indexed */
		      int c = (status & FLAG_C) ? 1 : 0;
		      uint32_t offset =
		      (bit (this_instr, 25)
		       ? shifted_reg_val (this_instr, c, pc_val, status)
		       : bits (this_instr, 0, 11));

		      if (bit (this_instr, 23))
			base += offset;
		      else
			base -= offset;
		    }
		  nextpc = (CORE_ADDR) read_memory_integer ((CORE_ADDR) base,
							    4);

		  nextpc = ADDR_BITS_REMOVE (nextpc);

		  if (nextpc == pc)
		    error (_("Infinite loop detected"));
		}
	    }
	  break;

	case 0x8:
	case 0x9:		/* block transfer */
	  if (bit (this_instr, 20))
	    {
	      /* LDM */
	      if (bit (this_instr, 15))
		{
		  /* loading pc */
		  int offset = 0;

		  if (bit (this_instr, 23))
		    {
		      /* up */
		      uint32_t reglist = bits (this_instr, 0, 14);
		      offset = bitcount (reglist) * 4;
		      if (bit (this_instr, 24))		/* pre */
			offset += 4;
		    }
		  else if (bit (this_instr, 24))
		    offset = -4;

		  {
		    uint32_t rn_val =
		    read_register (bits (this_instr, 16, 19));
		    nextpc =
		      (CORE_ADDR) read_memory_integer ((CORE_ADDR) (rn_val
								  + offset),
						       4);
		  }
		  nextpc = ADDR_BITS_REMOVE (nextpc);
		  if (nextpc == pc)
		    error (_("Infinite loop detected"));
		}
	    }
	  break;

	case 0xb:		/* branch & link */
	case 0xa:		/* branch */
	  {
	    nextpc = BranchDest (pc, this_instr);
	    nextpc = ADDR_BITS_REMOVE (nextpc);
	    if (nextpc == pc)
	      error (_("Infinite loop detected"));
	    break;
	  }

	case 0xc:
	case 0xd:
	case 0xe:		/* coproc ops */
	case 0xf:		/* SWI */
	  break;

	default:
	  fprintf_filtered (gdb_stderr, _("Bad bit-field extraction\n"));
	  return (pc);
	}
    }

  return nextpc;
}

/* single_step() is called just before we want to resume the inferior,
   if we want to single-step it but there is no hardware or kernel
   single-step support.  We find the target of the coming instruction
   and breakpoint it.

   single_step() is also called just after the inferior stops.  If we
   had set up a simulated single-step, we undo our damage.  */

static void
arm_software_single_step (enum target_signal sig, int insert_bpt)
{
  static uint32_t curr_pc;		 /* State between setting and unsetting.  */
  static uint32_t next_pc;		 /* State between setting and unsetting.  */
  static gdb_byte break_mem[BREAKPOINT_MAX]; /* Temporary storage for mem@bpt */
  /* APPLE LOCAL: When you set a single step breakpoint you have to lock
     the scheduler to this thread.  Otherwise - at least on MacOS X - a bunch
     of threads might hit the step breakpoint.  */
  static enum scheduler_locking_mode old_mode;

  if (insert_bpt)
    {
      curr_pc = read_register (ARM_PC_REGNUM);
      next_pc = arm_get_next_pc (curr_pc);
      target_insert_breakpoint (next_pc, break_mem);
      if (current_target.to_has_thread_control & tc_schedlock)
	old_mode = set_scheduler_locking_mode (scheduler_locking_on);
    }
  else
    {
      target_remove_breakpoint (next_pc, break_mem);
      if (current_target.to_has_thread_control & tc_schedlock)
	set_scheduler_locking_mode (old_mode);
    }

  if (arm_debug)
    fprintf_unfiltered (gdb_stdlog, "arm_software_single_step (%i, %i):"
		   "curr_pc 0x%s ==> next_pc = 0x%s, old_scheduler_mode = %i\n",
			sig, insert_bpt, paddr (curr_pc), paddr (next_pc),
			old_mode);
}

#include "bfd-in2.h"
#include "libcoff.h"
extern char g_examine_i_size;

static int
gdb_print_insn_arm (bfd_vma memaddr, disassemble_info *info)
{
  int is_thumb = 0;
  /* To allow random code to be disassembled in ARM or Thumb (overriding any
     special symbols), we watch to the g_examine_i_size global that is set
     in printcmd.c. It defaults to 'b' for byte when no size is specified with
     the 'i' examine format, but it can be overridden to allow disassembly
     to be told the width of the instruction that should be used to disassemble.
     'h' indicates a half word, or Thumb mode. 'w' indicates a word size or
     ARM mode. Anything else will auto detect the ARM/Thumb-ness of an address.
     The global is reset immediately following the disassembly call so that
     normal disassembly using the "disassemble" command won't be affecting, only
     the instruction examine format ("x/4ih") is affected.  */
  switch (g_examine_i_size)
    {
    default:
    case 'b':
      is_thumb = arm_pc_is_thumb (memaddr);
      break;
    case 'h':
      is_thumb = 1;
      break;
    case 'w':
      is_thumb = 0;
      break;
    }

  if (is_thumb)
    {
      static asymbol *asym;
      static combined_entry_type ce;
      static struct coff_symbol_struct csym;
      static struct bfd fake_bfd;
      static bfd_target fake_target;

      if (csym.native == NULL)
	{
	  /* Create a fake symbol vector containing a Thumb symbol.
	     This is solely so that the code in print_insn_little_arm() 
	     and print_insn_big_arm() in opcodes/arm-dis.c will detect
	     the presence of a Thumb symbol and switch to decoding
	     Thumb instructions.  */

	  fake_target.flavour = bfd_target_coff_flavour;
	  fake_bfd.xvec = &fake_target;
	  ce.u.syment.n_sclass = C_THUMBEXTFUNC;
	  csym.native = &ce;
	  csym.symbol.the_bfd = &fake_bfd;
	  csym.symbol.name = "fake";
	  asym = (asymbol *) & csym;
	}

      memaddr = UNMAKE_THUMB_ADDR (memaddr);
      info->symbols = &asym;
    }
  else
    info->symbols = NULL;

  if (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG)
    return print_insn_big_arm (memaddr, info);
  else
    return print_insn_little_arm (memaddr, info);
}

/* The following define instruction sequences that will cause ARM
   cpu's to take an undefined instruction trap.  These are used to
   signal a breakpoint to GDB.
   
   The newer ARMv4T cpu's are capable of operating in ARM or Thumb
   modes.  A different instruction is required for each mode.  The ARM
   cpu's can also be big or little endian.  Thus four different
   instructions are needed to support all cases.
   
   Note: ARMv4 defines several new instructions that will take the
   undefined instruction trap.  ARM7TDMI is nominally ARMv4T, but does
   not in fact add the new instructions.  The new undefined
   instructions in ARMv4 are all instructions that had no defined
   behaviour in earlier chips.  There is no guarantee that they will
   raise an exception, but may be treated as NOP's.  In practice, it
   may only safe to rely on instructions matching:
   
   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1 
   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
   C C C C 0 1 1 x x x x x x x x x x x x x x x x x x x x 1 x x x x
   
   Even this may only true if the condition predicate is true. The
   following use a condition predicate of ALWAYS so it is always TRUE.
   
   There are other ways of forcing a breakpoint.  GNU/Linux, RISC iX,
   and NetBSD all use a software interrupt rather than an undefined
   instruction to force a trap.  This can be handled by by the
   abi-specific code during establishment of the gdbarch vector.  */


/* NOTE rearnsha 2002-02-18: for now we allow a non-multi-arch gdb to
   override these definitions.  */
#ifndef ARM_LE_BREAKPOINT
#define ARM_LE_BREAKPOINT {0xFE,0xDE,0xFF,0xE7}
#endif
#ifndef ARM_BE_BREAKPOINT
#define ARM_BE_BREAKPOINT {0xE7,0xFF,0xDE,0xFE}
#endif
#ifndef THUMB_LE_BREAKPOINT
#ifdef TM_NEXTSTEP
/* APPLE LOCAL: Don't use a SWI instruction for Thumb breakpoints.  */
#define THUMB_LE_BREAKPOINT {0xfe,0xde}
#else
#define THUMB_LE_BREAKPOINT {0xfe,0xdf}
#endif
#endif
#ifndef THUMB_BE_BREAKPOINT
#ifdef TM_NEXTSTEP
/* APPLE LOCAL: Don't use a SWI instruction for Thumb breakpoints.  */
#define THUMB_BE_BREAKPOINT {0xde,0xfe}
#else
#define THUMB_BE_BREAKPOINT {0xdf,0xfe}
#endif
#endif


static const gdb_byte arm_default_arm_le_breakpoint[] = ARM_LE_BREAKPOINT;
static const gdb_byte arm_default_arm_be_breakpoint[] = ARM_BE_BREAKPOINT;
static const gdb_byte arm_default_thumb_le_breakpoint[] = THUMB_LE_BREAKPOINT;
static const gdb_byte arm_default_thumb_be_breakpoint[] = THUMB_BE_BREAKPOINT;

/* Determine the type and size of breakpoint to insert at PCPTR.  Uses
   the program counter value to determine whether a 16-bit or 32-bit
   breakpoint should be used.  It returns a pointer to a string of
   bytes that encode a breakpoint instruction, stores the length of
   the string to *lenptr, and adjusts the program counter (if
   necessary) to point to the actual memory location where the
   breakpoint should be inserted.  */

/* XXX ??? from old tm-arm.h: if we're using RDP, then we're inserting
   breakpoints and storing their handles instread of what was in
   memory.  It is nice that this is the same size as a handle -
   otherwise remote-rdp will have to change.  */

static const unsigned char *
arm_breakpoint_from_pc (CORE_ADDR *pcptr, int *lenptr)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);

  if (arm_pc_is_thumb (*pcptr))
    {
      *pcptr = UNMAKE_THUMB_ADDR (*pcptr);
      *lenptr = tdep->thumb_breakpoint_size;
      return tdep->thumb_breakpoint;
    }
  else
    {
      *lenptr = tdep->arm_breakpoint_size;
      return tdep->arm_breakpoint;
    }
}

/* Extract from an array REGBUF containing the (raw) register state a
   function return value of type TYPE, and copy that, in virtual
   format, into VALBUF.  */

static void
arm_extract_return_value (struct type *type, struct regcache *regs,
			  gdb_byte *valbuf)
{
  if (TYPE_CODE_FLT == TYPE_CODE (type))
    {
      switch (gdbarch_tdep (current_gdbarch)->fp_model)
	{
	case ARM_FLOAT_FPA:
	  {
	    /* The value is in register F0 in internal format.  We need to
	       extract the raw value and then convert it to the desired
	       internal type.  */
	    bfd_byte tmpbuf[FP_REGISTER_SIZE];

	    regcache_cooked_read (regs, ARM_F0_REGNUM, tmpbuf);
	    convert_from_extended (floatformat_from_type (type), tmpbuf,
				   valbuf);
	  }
	  break;

	case ARM_FLOAT_SOFT_FPA:
	case ARM_FLOAT_SOFT_VFP:
	case ARM_FLOAT_NONE:
	case ARM_FLOAT_VFP:
	  regcache_cooked_read (regs, ARM_A1_REGNUM, valbuf);
	  if (TYPE_LENGTH (type) > 4)
	    regcache_cooked_read (regs, ARM_A1_REGNUM + 1,
				  valbuf + INT_REGISTER_SIZE);
	  break;

/*	APPLE FUTURE ADDITION: We may need to comment out the ARM_FLOAT_VFP
	case above and use this code if we decide to send and return float 
	and double values in the VFP registers in the future. Currently we
	don't adopt this optional procedure call standard.
	case ARM_FLOAT_VFP:
	  if (TYPE_LENGTH (type) <= 4)
	    regcache_cooked_read (regs, ARM_VFP_REGNUM_S0, valbuf);
	  else if (TYPE_LENGTH (type) <= 8)
	    regcache_cooked_read (regs, ARM_FIRST_VFP_PSEUDO_REGNUM, valbuf);
	  break;
*/
	default:
	  internal_error
	    (__FILE__, __LINE__,
	     _("arm_extract_return_value: Floating point model not supported"));
	  break;
	}
    }
  else if (TYPE_CODE (type) == TYPE_CODE_INT
	   || TYPE_CODE (type) == TYPE_CODE_CHAR
	   || TYPE_CODE (type) == TYPE_CODE_BOOL
	   || TYPE_CODE (type) == TYPE_CODE_PTR
	   || TYPE_CODE (type) == TYPE_CODE_REF
	   || TYPE_CODE (type) == TYPE_CODE_ENUM)
    {
      /* If the the type is a plain integer, then the access is
	 straight-forward.  Otherwise we have to play around a bit more.  */
      int len = TYPE_LENGTH (type);
      int regno = ARM_A1_REGNUM;
      ULONGEST tmp;

      while (len > 0)
	{
	  /* By using store_unsigned_integer we avoid having to do
	     anything special for small big-endian values.  */
	  regcache_cooked_read_unsigned (regs, regno++, &tmp);
	  store_unsigned_integer (valbuf, 
				  (len > INT_REGISTER_SIZE
				   ? INT_REGISTER_SIZE : len),
				  tmp);
	  len -= INT_REGISTER_SIZE;
	  valbuf += INT_REGISTER_SIZE;
	}
    }
  else
    {
      /* For a structure or union the behaviour is as if the value had
         been stored to word-aligned memory and then loaded into 
         registers with 32-bit load instruction(s).  */
      int len = TYPE_LENGTH (type);
      int regno = ARM_A1_REGNUM;
      bfd_byte tmpbuf[INT_REGISTER_SIZE];

      while (len > 0)
	{
	  regcache_cooked_read (regs, regno++, tmpbuf);
	  memcpy (valbuf, tmpbuf,
		  len > INT_REGISTER_SIZE ? INT_REGISTER_SIZE : len);
	  len -= INT_REGISTER_SIZE;
	  valbuf += INT_REGISTER_SIZE;
	}
    }
}

/* Extract from an array REGBUF containing the (raw) register state
   the address in which a function should return its structure value.  */

static CORE_ADDR
arm_extract_struct_value_address (struct regcache *regcache)
{
  ULONGEST ret;

  regcache_cooked_read_unsigned (regcache, ARM_A1_REGNUM, &ret);
  return ret;
}

/* Will a function return an aggregate type in memory or in a
   register?  Return 0 if an aggregate type can be returned in a
   register, 1 if it must be returned in memory.  */

static int
arm_return_in_memory (struct gdbarch *gdbarch, struct type *type)
{
  int nRc;
  enum type_code code;

  CHECK_TYPEDEF (type);

  /* In the ARM ABI, "integer" like aggregate types are returned in
     registers.  For an aggregate type to be integer like, its size
     must be less than or equal to DEPRECATED_REGISTER_SIZE and the
     offset of each addressable subfield must be zero.  Note that bit
     fields are not addressable, and all addressable subfields of
     unions always start at offset zero.

     This function is based on the behaviour of GCC 2.95.1.
     See: gcc/arm.c: arm_return_in_memory() for details.

     Note: All versions of GCC before GCC 2.95.2 do not set up the
     parameters correctly for a function returning the following
     structure: struct { float f;}; This should be returned in memory,
     not a register.  Richard Earnshaw sent me a patch, but I do not
     know of any way to detect if a function like the above has been
     compiled with the correct calling convention.  */

  /* All aggregate types that won't fit in a register must be returned
     in memory.  */
  if (TYPE_LENGTH (type) > DEPRECATED_REGISTER_SIZE)
    {
      return 1;
    }

  /* The only aggregate types that can be returned in a register are
     structs and unions.  Arrays must be returned in memory.  */
  code = TYPE_CODE (type);
  if ((TYPE_CODE_STRUCT != code) && (TYPE_CODE_UNION != code))
    {
      return 1;
    }

  /* Assume all other aggregate types can be returned in a register.
     Run a check for structures, unions and arrays.  */
  nRc = 0;

  if ((TYPE_CODE_STRUCT == code) || (TYPE_CODE_UNION == code))
    {
      int i;
      /* Need to check if this struct/union is "integer" like.  For
         this to be true, its size must be less than or equal to
         DEPRECATED_REGISTER_SIZE and the offset of each addressable
         subfield must be zero.  Note that bit fields are not
         addressable, and unions always start at offset zero.  If any
         of the subfields is a floating point type, the struct/union
         cannot be an integer type.  */

      /* For each field in the object, check:
         1) Is it FP? --> yes, nRc = 1;
         2) Is it addressable (bitpos != 0) and
         not packed (bitsize == 0)?
         --> yes, nRc = 1  
       */

      for (i = 0; i < TYPE_NFIELDS (type); i++)
	{
	  enum type_code field_type_code;
	  field_type_code = TYPE_CODE (check_typedef (TYPE_FIELD_TYPE (type, i)));

	  /* Is it a floating point type field?  */
	  if (field_type_code == TYPE_CODE_FLT)
	    {
	      nRc = 1;
	      break;
	    }

	  /* If bitpos != 0, then we have to care about it.  */
	  if (TYPE_FIELD_BITPOS (type, i) != 0)
	    {
	      /* Bitfields are not addressable.  If the field bitsize is 
	         zero, then the field is not packed.  Hence it cannot be
	         a bitfield or any other packed type.  */
	      if (TYPE_FIELD_BITSIZE (type, i) == 0)
		{
		  nRc = 1;
		  break;
		}
	    }
	}
    }

  return nRc;
}

/* Write into appropriate registers a function return value of type
   TYPE, given in virtual format.  */

static void
arm_store_return_value (struct type *type, struct regcache *regs,
			const gdb_byte *valbuf)
{
  if (TYPE_CODE (type) == TYPE_CODE_FLT)
    {
      gdb_byte buf[MAX_REGISTER_SIZE];

      switch (gdbarch_tdep (current_gdbarch)->fp_model)
	{
	case ARM_FLOAT_FPA:

	  convert_to_extended (floatformat_from_type (type), buf, valbuf);
	  regcache_cooked_write (regs, ARM_F0_REGNUM, buf);
	  break;

	case ARM_FLOAT_SOFT_FPA:
	case ARM_FLOAT_SOFT_VFP:
	case ARM_FLOAT_NONE:
	case ARM_FLOAT_VFP:
	  regcache_cooked_write (regs, ARM_A1_REGNUM, valbuf);
	  if (TYPE_LENGTH (type) > 4)
	    regcache_cooked_write (regs, ARM_A1_REGNUM + 1, 
				   valbuf + INT_REGISTER_SIZE);
	  break;

/*	APPLE FUTURE ADDITION: We may need to comment out the ARM_FLOAT_VFP
	case above and use this code if we decide to send and return float 
	and double values in the VFP registers in the future. Currently we
	don't adopt this optional procedure call standard.
	case ARM_FLOAT_VFP:
	  if (TYPE_LENGTH (type) <= 4)
	    regcache_cooked_read (regs, ARM_VFP_REGNUM_S0, valbuf);
	  else if (TYPE_LENGTH (type) <= 8)
	    regcache_cooked_read (regs, ARM_FIRST_VFP_PSEUDO_REGNUM, valbuf);
	  break;
*/
	default:
	  internal_error
	    (__FILE__, __LINE__,
	     _("arm_store_return_value: Floating point model not supported"));
	  break;
	}
    }
  else if (TYPE_CODE (type) == TYPE_CODE_INT
	   || TYPE_CODE (type) == TYPE_CODE_CHAR
	   || TYPE_CODE (type) == TYPE_CODE_BOOL
	   || TYPE_CODE (type) == TYPE_CODE_PTR
	   || TYPE_CODE (type) == TYPE_CODE_REF
	   || TYPE_CODE (type) == TYPE_CODE_ENUM)
    {
      if (TYPE_LENGTH (type) <= 4)
	{
	  /* Values of one word or less are zero/sign-extended and
	     returned in r0.  */
	  bfd_byte tmpbuf[INT_REGISTER_SIZE];
	  LONGEST val = unpack_long (type, valbuf);

	  store_signed_integer (tmpbuf, INT_REGISTER_SIZE, val);
	  regcache_cooked_write (regs, ARM_A1_REGNUM, tmpbuf);
	}
      else
	{
	  /* Integral values greater than one word are stored in consecutive
	     registers starting with r0.  This will always be a multiple of
	     the regiser size.  */
	  int len = TYPE_LENGTH (type);
	  int regno = ARM_A1_REGNUM;

	  while (len > 0)
	    {
	      regcache_cooked_write (regs, regno++, valbuf);
	      len -= INT_REGISTER_SIZE;
	      valbuf += INT_REGISTER_SIZE;
	    }
	}
    }
  else
    {
      /* For a structure or union the behaviour is as if the value had
         been stored to word-aligned memory and then loaded into 
         registers with 32-bit load instruction(s).  */
      int len = TYPE_LENGTH (type);
      int regno = ARM_A1_REGNUM;
      bfd_byte tmpbuf[INT_REGISTER_SIZE];

      while (len > 0)
	{
	  memcpy (tmpbuf, valbuf,
		  len > INT_REGISTER_SIZE ? INT_REGISTER_SIZE : len);
	  regcache_cooked_write (regs, regno++, tmpbuf);
	  len -= INT_REGISTER_SIZE;
	  valbuf += INT_REGISTER_SIZE;
	}
    }
}

/* APPLE LOCAL: This is how it works on TOT.  */
/* Handle function return values.  */

static enum return_value_convention
arm_return_value (struct gdbarch *gdbarch, struct type *valtype,
		  struct regcache *regcache, gdb_byte *readbuf,
		  const gdb_byte *writebuf)
{
  if (TYPE_CODE (valtype) == TYPE_CODE_STRUCT
      || TYPE_CODE (valtype) == TYPE_CODE_UNION
      || TYPE_CODE (valtype) == TYPE_CODE_ARRAY)
    {
      if (arm_return_in_memory (gdbarch, valtype))
	{
	  if (writebuf || readbuf)
	    {
	      CORE_ADDR r0 = arm_extract_struct_value_address (regcache);
	      if (writebuf)
		target_write_memory (r0, writebuf, TYPE_LENGTH (valtype));

	      if (readbuf)
		target_read_memory (r0, readbuf, TYPE_LENGTH (valtype));
	    }
	  return RETURN_VALUE_ABI_RETURNS_ADDRESS;
	}
    }

  if (writebuf)
    arm_store_return_value (valtype, regcache, writebuf);

  if (readbuf)
    arm_extract_return_value (valtype, regcache, readbuf);

  return RETURN_VALUE_REGISTER_CONVENTION;
}

static int
arm_get_longjmp_target (CORE_ADDR *pc)
{
  CORE_ADDR jb_addr;
  gdb_byte buf[INT_REGISTER_SIZE];
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  
  jb_addr = read_register (ARM_A1_REGNUM);

  if (target_read_memory (jb_addr + tdep->jb_pc * tdep->jb_elt_size, buf,
			  INT_REGISTER_SIZE))
    return 0;

  *pc = extract_unsigned_integer (buf, INT_REGISTER_SIZE);
  return 1;
}

/* Return non-zero if the PC is inside a thumb call thunk.  */

int
arm_in_call_stub (CORE_ADDR pc, char *name)
{
  CORE_ADDR start_addr;

  /* Find the starting address of the function containing the PC.  If
     the caller didn't give us a name, look it up at the same time.  */
  if (0 == find_pc_partial_function (pc, name ? NULL : &name, 
				     &start_addr, NULL))
    return 0;

  return strncmp (name, "_call_via_r", 11) == 0;
}

/* If PC is in a Thumb call or return stub, return the address of the
   target PC, which is in a register.  The thunk functions are called
   _called_via_xx, where x is the register name.  The possible names
   are r0-r9, sl, fp, ip, sp, and lr.  */

CORE_ADDR
arm_skip_stub (CORE_ADDR pc)
{
  char *name;
  CORE_ADDR start_addr;

  /* Find the starting address and name of the function containing the PC.  */
  if (find_pc_partial_function (pc, &name, &start_addr, NULL) == 0)
    return 0;

  /* Call thunks always start with "_call_via_".  */
  if (strncmp (name, "_call_via_", 10) == 0)
    {
      /* Use the name suffix to determine which register contains the
         target PC.  */
      static char *table[15] =
      {"r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
       "r8", "r9", "sl", "fp", "ip", "sp", "lr"
      };
      int regno;

      for (regno = 0; regno <= 14; regno++)
	if (strcmp (&name[10], table[regno]) == 0)
	  return read_register (regno);
    }

  return 0;			/* not a stub */
}

static void
set_arm_command (char *args, int from_tty)
{
  printf_unfiltered (_("\
\"set arm\" must be followed by an apporpriate subcommand.\n"));
  help_list (setarmcmdlist, "set arm ", all_commands, gdb_stdout);
}

static void
show_arm_command (char *args, int from_tty)
{
  cmd_show_list (showarmcmdlist, from_tty, "");
}

static void
arm_update_current_architecture (void)
{
  struct gdbarch_info info;

  /* If the current architecture is not ARM, we have nothing to do.  */
  if (gdbarch_bfd_arch_info (current_gdbarch)->arch != bfd_arch_arm)
    return;

  /* Update the architecture.  */
  gdbarch_info_init (&info);

  if (!gdbarch_update_p (info))
    internal_error (__FILE__, __LINE__, "could not update architecture");
}

static void
set_fp_model_sfunc (char *args, int from_tty,
		    struct cmd_list_element *c)
{
  enum arm_float_model fp_model;

  for (fp_model = ARM_FLOAT_AUTO; fp_model != ARM_FLOAT_LAST; fp_model++)
    if (strcmp (current_fp_model, fp_model_strings[fp_model]) == 0)
      {
	arm_fp_model = fp_model;
	break;
      }

  if (fp_model == ARM_FLOAT_LAST)
    internal_error (__FILE__, __LINE__, _("Invalid fp model accepted: %s."),
		    current_fp_model);

  arm_update_current_architecture ();
}

static void
show_fp_model (struct ui_file *file, int from_tty,
	       struct cmd_list_element *c, const char *value)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);

  if (arm_fp_model == ARM_FLOAT_AUTO
      && gdbarch_bfd_arch_info (current_gdbarch)->arch == bfd_arch_arm)
    fprintf_filtered (file, _("\
The current ARM floating point model is \"auto\" (currently \"%s\").\n"),
		      fp_model_strings[tdep->fp_model]);
  else
    fprintf_filtered (file, _("\
The current ARM floating point model is \"%s\".\n"),
		      fp_model_strings[arm_fp_model]);
}

static void
arm_set_abi (char *args, int from_tty,
	     struct cmd_list_element *c)
{
  enum arm_abi_kind arm_abi;

  for (arm_abi = ARM_ABI_AUTO; arm_abi != ARM_ABI_LAST; arm_abi++)
    if (strcmp (arm_abi_string, arm_abi_strings[arm_abi]) == 0)
      {
	arm_abi_global = arm_abi;
	break;
      }

  if (arm_abi == ARM_ABI_LAST)
    internal_error (__FILE__, __LINE__, _("Invalid ABI accepted: %s."),
		    arm_abi_string);

  arm_update_current_architecture ();
}

static void
arm_show_abi (struct ui_file *file, int from_tty,
	     struct cmd_list_element *c, const char *value)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);

  if (arm_abi_global == ARM_ABI_AUTO
      && gdbarch_bfd_arch_info (current_gdbarch)->arch == bfd_arch_arm)
    fprintf_filtered (file, _("\
The current ARM ABI is \"auto\" (currently \"%s\").\n"),
		      arm_abi_strings[tdep->arm_abi]);
  else
    fprintf_filtered (file, _("The current ARM ABI is \"%s\".\n"),
		      arm_abi_string);
}

/* If the user changes the register disassembly style used for info
   register and other commands, we have to also switch the style used
   in opcodes for disassembly output.  This function is run in the "set
   arm disassembly" command, and does that.  */

static void
set_disassembly_style_sfunc (char *args, int from_tty,
			      struct cmd_list_element *c)
{
  set_disassembly_style ();
}

static void
set_arm_single_step_mode_sfunc (char *args, int from_tty, struct cmd_list_element *c)
{
  int i;
  for (i=0; arm_single_step_mode_strings[i] != NULL; i++)
    {
      if (strcmp(arm_single_step_mode_str, arm_single_step_mode_strings[i]) == 0)
	{
	  arm_single_step_mode = i;
	  break;
	}
    }
}

int
set_arm_single_step_mode (struct gdbarch *gdbarch, int single_step_mode)
{
  if (single_step_mode >= arm_single_step_mode_auto && 
      single_step_mode <= arm_single_step_mode_hardware)
    {
      arm_single_step_mode = single_step_mode; 
      switch (arm_single_step_mode)
	{
	  default:
	  case arm_single_step_mode_auto:
	  case arm_single_step_mode_software:
	    set_gdbarch_software_single_step (gdbarch, arm_software_single_step);
	    break;

	  case arm_single_step_mode_hardware:
	    set_gdbarch_software_single_step (gdbarch, NULL);
	    break;
	}
    }
  return arm_single_step_mode;
}

int
get_arm_single_step_mode ()
{
  return arm_single_step_mode;
}

void
arm_set_show_opcode_bytes (char *args, int from_tty, struct cmd_list_element *c)
{
  set_arm_show_opcode_bytes_option (show_opcode_bytes);
}

/* Return the ARM register name corresponding to register I.  */
static const char *
arm_register_name (int i)
{
  return g_register_info[i].name;
}

static void
set_disassembly_style (void)
{
  const char *setname, *setdesc, *const *regnames;
  int numregs, j;

  /* Find the style that the user wants in the opcodes table.  */
  int current = 0;
  numregs = get_arm_regnames (current, &setname, &setdesc, &regnames);
  while ((disassembly_style != setname)
	 && (current < num_disassembly_options))
    get_arm_regnames (++current, &setname, &setdesc, &regnames);
  current_option = current;

  /* Fill our copy.  */
  for (j = 0; j < numregs; j++)
    g_register_info[j].name = (char *) regnames[j];

  /* Adjust case.  */
  if (isupper (*regnames[ARM_PC_REGNUM]))
    {
      g_register_info[ARM_FPS_REGNUM].name = "FPS";
      g_register_info[ARM_PS_REGNUM].name = "CPSR";
    }
  else
    {
      g_register_info[ARM_FPS_REGNUM].name = "fps";
      g_register_info[ARM_PS_REGNUM].name = "cpsr";
    }

  /* Synchronize the disassembler.  */
  set_arm_regname_option (current);
}

/* Test whether the coff symbol specific value corresponds to a Thumb
   function.  */

static int
coff_sym_is_thumb (int val)
{
  return (val == C_THUMBEXT ||
	  val == C_THUMBSTAT ||
	  val == C_THUMBEXTFUNC ||
	  val == C_THUMBSTATFUNC ||
	  val == C_THUMBLABEL);
}

/* arm_coff_make_msymbol_special()
   arm_elf_make_msymbol_special()
   
   These functions test whether the COFF or ELF symbol corresponds to
   an address in thumb code, and set a "special" bit in a minimal
   symbol to indicate that it does.  */
   
static void
arm_elf_make_msymbol_special(asymbol *sym, struct minimal_symbol *msym)
{
  /* Thumb symbols are of type STT_LOPROC, (synonymous with
     STT_ARM_TFUNC).  */
  if (ELF_ST_TYPE (((elf_symbol_type *)sym)->internal_elf_sym.st_info)
      == STT_LOPROC)
    MSYMBOL_SET_SPECIAL (msym);
}

static void
arm_coff_make_msymbol_special(int val, struct minimal_symbol *msym)
{
  if (coff_sym_is_thumb (val))
    MSYMBOL_SET_SPECIAL (msym);
}

static void
arm_write_pc (CORE_ADDR pc, ptid_t ptid)
{
  write_register_pid (ARM_PC_REGNUM, pc, ptid);

  /* If necessary, set the T bit.  */
  if (arm_apcs_32)
    {
      CORE_ADDR val = read_register_pid (ARM_PS_REGNUM, ptid);
      if (arm_pc_is_thumb (pc))
	write_register_pid (ARM_PS_REGNUM, val | 0x20, ptid);
      else
	write_register_pid (ARM_PS_REGNUM, val & ~(CORE_ADDR) 0x20, ptid);
    }
}

static enum gdb_osabi
arm_elf_osabi_sniffer (bfd *abfd)
{
  unsigned int elfosabi;
  enum gdb_osabi osabi = GDB_OSABI_UNKNOWN;

  elfosabi = elf_elfheader (abfd)->e_ident[EI_OSABI];

  if (elfosabi == ELFOSABI_ARM)
    /* GNU tools use this value.  Check note sections in this case,
       as well.  */
    bfd_map_over_sections (abfd,
			   generic_elf_osabi_sniff_abi_tag_sections, 
			   &osabi);

  /* Anything else will be handled by the generic ELF sniffer.  */
  return osabi;
}


int
arm_register_reggroup_p (struct gdbarch *gdbarch, int regnum,
                         struct reggroup *group)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);

  if (tdep->fp_model == ARM_FLOAT_VFP)
    {
      if (regnum >= ARM_VFP_REGNUM_S0)
	{
	  if (group == float_reggroup
	      || group == all_reggroup)
	    return 1;
	  else
	    return 0;
	}
    }

  if (regnum == ARM_FPS_REGNUM)
  {
    if (group == float_reggroup
	|| group == all_reggroup)
      return 1;
    else
      return 0;
  }
      
  return default_register_reggroup_p (gdbarch, regnum, group);
}


/* Initialize the current architecture based on INFO.  If possible,
   re-use an architecture from ARCHES, which is a list of
   architectures already created during this debugging session.

   Called e.g. at program startup, when reading a core file, and when
   reading a binary file.  */

static struct gdbarch *
arm_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch_tdep *tdep;
  struct gdbarch *gdbarch;
  struct gdbarch_list *best_arch;
  enum arm_abi_kind arm_abi = arm_abi_global;
  enum arm_float_model fp_model = arm_fp_model;
  enum arm_vfp_version vfp_version = ARM_VFP_UNSUPPORTED;

  /* If we have an object to base this architecture on, try to determine
     its ABI.  */

  if (arm_abi == ARM_ABI_AUTO && info.abfd != NULL)
    {
      int ei_osabi;

      switch (bfd_get_flavour (info.abfd))
	{
	case bfd_target_aout_flavour:
	  /* Assume it's an old APCS-style ABI.  */
	  arm_abi = ARM_ABI_APCS;
	  break;

	case bfd_target_coff_flavour:
	  /* Assume it's an old APCS-style ABI.  */
	  /* XXX WinCE?  */
	  arm_abi = ARM_ABI_APCS;
	  break;

	case bfd_target_elf_flavour:
	  ei_osabi = elf_elfheader (info.abfd)->e_ident[EI_OSABI];
	  if (ei_osabi == ELFOSABI_ARM)
	    {
	      /* GNU tools used to use this value, but do not for EABI
		 objects.  There's nowhere to tag an EABI version anyway,
		 so assume APCS.  */
	      arm_abi = ARM_ABI_APCS;
	    }
	  else if (ei_osabi == ELFOSABI_NONE)
	    {
	      int e_flags, eabi_ver;

	      e_flags = elf_elfheader (info.abfd)->e_flags;
	      eabi_ver = EF_ARM_EABI_VERSION (e_flags);

	      switch (eabi_ver)
		{
		case EF_ARM_EABI_UNKNOWN:
		  /* Assume GNU tools.  */
		  arm_abi = ARM_ABI_APCS;
		  break;

		case EF_ARM_EABI_VER4:
		  arm_abi = ARM_ABI_AAPCS;
		  break;

		default:
		  warning (_("unknown ARM EABI version 0x%x"), eabi_ver);
		  arm_abi = ARM_ABI_APCS;
		  break;
		}
	    }
	  break;

	default:
	  /* Leave it as "auto".  */
	  break;
	}
    }

  /* Now that we have inferred any architecture settings that we
     can, try to inherit from the last ARM ABI.  */
  if (arches != NULL)
    {
      if (arm_abi == ARM_ABI_AUTO)
	arm_abi = gdbarch_tdep (arches->gdbarch)->arm_abi;

      if (fp_model == ARM_FLOAT_AUTO)
	fp_model = gdbarch_tdep (arches->gdbarch)->fp_model;
    }
  else
    {
      /* There was no prior ARM architecture; fill in default values.  */

      if (arm_abi == ARM_ABI_AUTO)
	arm_abi = ARM_ABI_APCS;

      /* We used to default to FPA for generic ARM, but almost nobody
	 uses that now, and we now provide a way for the user to force
	 the model.  So default to the most useful variant.  */
      if (fp_model == ARM_FLOAT_AUTO)
	fp_model = ARM_FLOAT_NONE;  /* APPLE LOCAL: default to no fp or fp
				       emulation (for libgcc which has float
				       sw support, but it just doesn't attempt
				       to emulate floating point registers in
				       memory).  */
    }

  /* If there is already a candidate, use it.  */
  for (best_arch = gdbarch_list_lookup_by_info (arches, &info);
       best_arch != NULL;
       best_arch = gdbarch_list_lookup_by_info (best_arch->next, &info))
    {
      if (arm_abi != gdbarch_tdep (best_arch->gdbarch)->arm_abi)
	continue;

      if (fp_model != gdbarch_tdep (best_arch->gdbarch)->fp_model)
	continue;

      if (fp_model == ARM_FLOAT_VFP)
	vfp_version = gdbarch_tdep (best_arch->gdbarch)->vfp_version;
	
      /* Found a match.  */
      break;
    }

  if (best_arch != NULL)
    return best_arch->gdbarch;

  tdep = xcalloc (1, sizeof (struct gdbarch_tdep));
  gdbarch = gdbarch_alloc (&info, tdep);

  /* Record additional information about the architecture we are defining.
     These are gdbarch discriminators, like the OSABI.  */
  tdep->arm_abi = arm_abi;
  tdep->fp_model = fp_model;
  tdep->vfp_version = vfp_version;

#ifdef TM_NEXTSTEP
  /* APPLE LOCAL HACK - we set the wordsize to 4 to keep the dyld code happy.  */
  tdep->wordsize = 4;
#endif

  /* APPLE LOCAL Map the DWARF register numbers to gdb's internal numberings. */
  set_gdbarch_dwarf2_reg_to_regnum (gdbarch, arm_dwarf2_reg_to_regnum);

  /* Breakpoints.  */
  switch (info.byte_order)
    {
    case BFD_ENDIAN_BIG:
      tdep->arm_breakpoint = arm_default_arm_be_breakpoint;
      tdep->arm_breakpoint_size = sizeof (arm_default_arm_be_breakpoint);
      tdep->thumb_breakpoint = arm_default_thumb_be_breakpoint;
      tdep->thumb_breakpoint_size = sizeof (arm_default_thumb_be_breakpoint);

      break;

    case BFD_ENDIAN_LITTLE:
      tdep->arm_breakpoint = arm_default_arm_le_breakpoint;
      tdep->arm_breakpoint_size = sizeof (arm_default_arm_le_breakpoint);
      tdep->thumb_breakpoint = arm_default_thumb_le_breakpoint;
      tdep->thumb_breakpoint_size = sizeof (arm_default_thumb_le_breakpoint);

      break;

    default:
      internal_error (__FILE__, __LINE__,
		      _("arm_gdbarch_init: bad byte order for float format"));
    }

#ifdef TM_NEXTSTEP
  /* APPLE LOCAL - For now lets turn off all the floating point stuff.  */
  set_gdbarch_register_reggroup_p (gdbarch, arm_register_reggroup_p);
#endif

  /* On ARM targets char defaults to unsigned.  */
  set_gdbarch_char_signed (gdbarch, 0);

  /* This should be low enough for everything.  */
  tdep->lowest_pc = 0x20;
  tdep->jb_pc = -1;	/* Longjump support not enabled by default.  */

  set_gdbarch_push_dummy_call (gdbarch, arm_push_dummy_call);

  set_gdbarch_write_pc (gdbarch, arm_write_pc);

  /* Frame handling.  */
  set_gdbarch_unwind_dummy_id (gdbarch, arm_unwind_dummy_id);
  set_gdbarch_unwind_pc (gdbarch, arm_unwind_pc);
  set_gdbarch_unwind_sp (gdbarch, arm_unwind_sp);

  frame_base_set_default (gdbarch, &arm_normal_base);

  /* Address manipulation.  */
  set_gdbarch_smash_text_address (gdbarch, arm_smash_text_address);
  set_gdbarch_addr_bits_remove (gdbarch, arm_addr_bits_remove);

  /* Advance PC across function entry code.  */
#ifdef TM_NEXTSTEP
  set_gdbarch_skip_prologue_addr_ctx (gdbarch, 
				      arm_macosx_skip_prologue_addr_ctx);
  set_gdbarch_skip_prologue (gdbarch, arm_macosx_skip_prologue);
#else
  set_gdbarch_skip_prologue (gdbarch, arm_skip_prologue);
#endif

  /* Get the PC when a frame might not be available.  */
  set_gdbarch_deprecated_saved_pc_after_call (gdbarch, arm_saved_pc_after_call);

  /* The stack grows downward.  */
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);

  /* Breakpoint manipulation.  */
  set_gdbarch_breakpoint_from_pc (gdbarch, arm_breakpoint_from_pc);

  /* Information about registers, etc.  */
  set_gdbarch_print_float_info (gdbarch, arm_print_float_info);
  set_gdbarch_deprecated_fp_regnum (gdbarch, ARM_FP_REGNUM);	/* ??? */
  set_gdbarch_sp_regnum (gdbarch, ARM_SP_REGNUM);
  set_gdbarch_pc_regnum (gdbarch, ARM_PC_REGNUM);
  set_gdbarch_deprecated_register_byte (gdbarch, arm_register_byte);
  set_gdbarch_num_regs (gdbarch, NUM_GREGS + NUM_FREGS + NUM_SREGS);
  set_gdbarch_register_type (gdbarch, arm_register_type);

  /* Internal <-> external register number maps.  */
  set_gdbarch_register_sim_regno (gdbarch, arm_register_sim_regno);

  /* Integer registers are 4 bytes.  */
  set_gdbarch_deprecated_register_size (gdbarch, 4);
  set_gdbarch_register_name (gdbarch, arm_register_name);

  /* Returning results.  */
#if 0 /* APPLE LOCAL: This is the old way */
  set_gdbarch_extract_return_value (gdbarch, arm_extract_return_value);
  set_gdbarch_store_return_value (gdbarch, arm_store_return_value);
  set_gdbarch_deprecated_use_struct_convention (gdbarch, arm_use_struct_convention);
  set_gdbarch_deprecated_extract_struct_value_address (gdbarch, arm_extract_struct_value_address);
#else
  set_gdbarch_return_value (gdbarch, arm_return_value);
#endif
  /* Single stepping.  */
  /* APPLE LOCAL BEGIN: user controllable single stepping mode.  */
  set_arm_single_step_mode (gdbarch, get_arm_single_step_mode ());
  /* APPLE LOCAL END: user controllable single stepping mode.  */

  /* Disassembly.  */
  set_gdbarch_print_insn (gdbarch, gdb_print_insn_arm);

  /* Minsymbol frobbing.  */
  set_gdbarch_elf_make_msymbol_special (gdbarch, arm_elf_make_msymbol_special);
  set_gdbarch_coff_make_msymbol_special (gdbarch,
					 arm_coff_make_msymbol_special);

  /* Hook in the ABI-specific overrides, if they have been registered.  */
  gdbarch_init_osabi (info, gdbarch);

  /* Add some default predicates.  */
#ifdef TM_NEXTSTEP  
  /* APPLE LOCAL: Install the Mac OS X specific sigtramp sniffer.  */
  frame_unwind_append_sniffer (gdbarch, arm_macosx_sigtramp_unwind_sniffer);
#else
  /* APPLE LOCAL: We don't have these ".plt stubs" so don't add the
     sniffer for them.  */
  frame_unwind_append_sniffer (gdbarch, arm_stub_unwind_sniffer);
  frame_unwind_append_sniffer (gdbarch, arm_sigtramp_unwind_sniffer);
#endif
  frame_unwind_append_sniffer (gdbarch, dwarf2_frame_sniffer);
  frame_unwind_append_sniffer (gdbarch, arm_prologue_unwind_sniffer);

  /* Now we have tuned the configuration, set a few final things,
     based on what the OS ABI has told us.  */

  if (tdep->jb_pc >= 0)
    set_gdbarch_get_longjmp_target (gdbarch, arm_get_longjmp_target);

  /* Floating point sizes and format.  */
  switch (info.byte_order)
    {
    case BFD_ENDIAN_BIG:
      set_gdbarch_float_format (gdbarch, &floatformat_ieee_single_big);
      set_gdbarch_double_format (gdbarch, &floatformat_ieee_double_big);
      set_gdbarch_long_double_format (gdbarch, &floatformat_ieee_double_big);
      break;

    case BFD_ENDIAN_LITTLE:
      set_gdbarch_float_format (gdbarch, &floatformat_ieee_single_little);
      if (fp_model == ARM_FLOAT_SOFT_FPA || fp_model == ARM_FLOAT_FPA)
	{
	  set_gdbarch_double_format
	    (gdbarch, &floatformat_ieee_double_littlebyte_bigword);
	  set_gdbarch_long_double_format
	    (gdbarch, &floatformat_ieee_double_littlebyte_bigword);
	}
      else
	{
	  set_gdbarch_double_format (gdbarch, &floatformat_ieee_double_little);
	  set_gdbarch_long_double_format (gdbarch,
					  &floatformat_ieee_double_little);
	}
      break;

    default:
      internal_error (__FILE__, __LINE__,
		      _("arm_gdbarch_init: bad byte order for float format"));
    }

  return gdbarch;
}

static void
arm_dump_tdep (struct gdbarch *current_gdbarch, struct ui_file *file)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);

  if (tdep == NULL)
    return;

  fprintf_unfiltered (file, _("arm_dump_tdep: Lowest pc = 0x%s"),
		      paddr (tdep->lowest_pc));
}

extern initialize_file_ftype _initialize_arm_tdep; /* -Wmissing-prototypes */

void
_initialize_arm_tdep (void)
{
  struct ui_file *stb;
  long length;
  const char *setname;
  const char *setdesc;
  const char *const *regnames;
  int numregs, i, j;
  static char *helptext;
  char regdesc[1024], *rdptr = regdesc;
  size_t rest = sizeof (regdesc);

  /* APPLE LOCAL START: Build the offsets to be used by arm_register_byte.  */
  g_register_info[0].offset = 0;
  for (i=1; i<g_register_info_count; i++)
    {
      if (g_register_info[i - 1].type)
	g_register_info[i].offset = g_register_info[i - 1].offset + 
				    TYPE_LENGTH (*g_register_info[i - 1].type);
    }
  /* APPLE LOCAL END.  */

  gdbarch_register (bfd_arch_arm, arm_gdbarch_init, arm_dump_tdep);

  /* Register an ELF OS ABI sniffer for ARM binaries.  */
  gdbarch_register_osabi_sniffer (bfd_arch_arm,
				  bfd_target_elf_flavour,
				  arm_elf_osabi_sniffer);

  /* Get the number of possible sets of register names defined in opcodes.  */
  num_disassembly_options = get_arm_regname_num_options ();

  /* Add root prefix command for all "set arm"/"show arm" commands.  */
  add_prefix_cmd ("arm", no_class, set_arm_command,
		  _("Various ARM-specific commands."),
		  &setarmcmdlist, "set arm ", 0, &setlist);

  add_prefix_cmd ("arm", no_class, show_arm_command,
		  _("Various ARM-specific commands."),
		  &showarmcmdlist, "show arm ", 0, &showlist);

  /* Sync the opcode insn printer with our register viewer.  */
  parse_arm_disassembler_option ("reg-names-std");

  /* Initialize the array that will be passed to
     add_setshow_enum_cmd().  */
  valid_disassembly_styles
    = xmalloc ((num_disassembly_options + 1) * sizeof (char *));
  for (i = 0; i < num_disassembly_options; i++)
    {
      numregs = get_arm_regnames (i, &setname, &setdesc, &regnames);
      valid_disassembly_styles[i] = setname;
      length = snprintf (rdptr, rest, "%s - %s\n", setname, setdesc);
      rdptr += length;
      rest -= length;
      /* Copy the default names (if found) and synchronize disassembler.  */
      if (!strcmp (setname, "std"))
	{
          disassembly_style = setname;
          current_option = i;
	  for (j = 0; j < numregs; j++)
            g_register_info[j].name = (char *) regnames[j];
          set_arm_regname_option (i);
	}
    }
  /* Mark the end of valid options.  */
  valid_disassembly_styles[num_disassembly_options] = NULL;

  /* Create the help text.  */
  stb = mem_fileopen ();
  fprintf_unfiltered (stb, "%s%s%s",
		      _("The valid values are:\n"),
		      regdesc,
		      _("The default is \"std\"."));
  helptext = ui_file_xstrdup (stb, &length);
  ui_file_delete (stb);

  add_setshow_enum_cmd("disassembler", no_class,
		       valid_disassembly_styles, &disassembly_style,
		       _("Set the disassembly style."),
		       _("Show the disassembly style."),
		       helptext,
		       set_disassembly_style_sfunc,
		       NULL, /* FIXME: i18n: The disassembly style is \"%s\".  */
		       &setarmcmdlist, &showarmcmdlist);

  add_setshow_boolean_cmd ("show-opcode-bytes", no_class, &show_opcode_bytes,
			   _("Set ARM and Thumb opcode byte display in disassembly."),
			   _("Show ARM and Thumb opcode byte display in disassembly."),
			   _("When on, the hex representation of the opcode "
			     "bytes will be displayed along\nwith any disassembly."),
			   arm_set_show_opcode_bytes, NULL,
			   &setarmcmdlist, &showarmcmdlist);

  add_setshow_enum_cmd ("single-step", no_class, 
			arm_single_step_mode_strings, &arm_single_step_mode_str, 
			_("Set the ARM stepping mode."),
			_("Show the ARM stepping mode."),
			_("Valid values are 'auto', 'software' or 'hardware'.\n"
			  "auto: lets each OS ABI automatically determine "
			  "which single stepping mode to use.\n"
			  "software: always use software single step ('s' "
			  "packets will NOT be used in 'target remote' variants).\n"
			  "hardware: let targets step using hardware ('s' "
			  "packets will be used in 'target remote' variants)."),
			set_arm_single_step_mode_sfunc, NULL, &setarmcmdlist, 
			&showarmcmdlist);

  add_setshow_boolean_cmd ("apcs32", no_class, &arm_apcs_32,
			   _("Set usage of ARM 32-bit mode."),
			   _("Show usage of ARM 32-bit mode."),
			   _("When off, a 26-bit PC will be used."),
			   NULL,
			   NULL, /* FIXME: i18n: Usage of ARM 32-bit mode is %s.  */
			   &setarmcmdlist, &showarmcmdlist);

  /* Add a command to allow the user to force the FPU model.  */
  add_setshow_enum_cmd ("fpu", no_class, fp_model_strings, &current_fp_model,
			_("Set the floating point type."),
			_("Show the floating point type."),
			_("auto - Determine the FP typefrom the OS-ABI.\n\
softfpa - Software FP, mixed-endian doubles on little-endian ARMs.\n\
fpa - FPA co-processor (GCC compiled).\n\
softvfp - Software FP with pure-endian doubles.\n\
vfp - VFP co-processor.\n\
none - No floating point hardware or software emulation."),
			set_fp_model_sfunc, show_fp_model,
			&setarmcmdlist, &showarmcmdlist);

  /* Add a command to allow the user to force the ABI.  */
  add_setshow_enum_cmd ("abi", class_support, arm_abi_strings, &arm_abi_string,
			_("Set the ABI."),
			_("Show the ABI."),
			NULL, arm_set_abi, arm_show_abi,
			&setarmcmdlist, &showarmcmdlist);

  /* Debugging flag.  */
  add_setshow_zinteger_cmd ("arm", class_maintenance, &arm_debug,
			   _("Set ARM debugging."),
			   _("Show ARM debugging."),
			   _("When non-zero, arm-specific debugging is enabled."),
			   NULL,
			   show_arm_debug,
			   &setdebuglist, &showdebuglist);
}


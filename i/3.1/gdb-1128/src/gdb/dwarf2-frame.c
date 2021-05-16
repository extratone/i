/* Frame unwinder for frames with DWARF Call Frame Information.

   Copyright (C) 2003, 2004, 2005, 2007 Free Software Foundation, Inc.

   Contributed by Mark Kettenis.

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

#include "defs.h"
#include "dwarf2expr.h"
#include "elf/dwarf2.h"
#include "frame.h"
#include "frame-base.h"
#include "frame-unwind.h"
#include "gdbcore.h"
#include "gdbtypes.h"
#include "symtab.h"
#include "objfiles.h"
#include "regcache.h"
#include "value.h"
#include "dwarf2read.h"

#include "gdb_assert.h"
#include "gdb_string.h"

#include "complaints.h"
#include "dwarf2-frame.h"

/* Call Frame Information (CFI).  */

/* Common Information Entry (CIE).  */

struct dwarf2_cie
{
  /* Offset into the .debug_frame section where this CIE was found.
     Used to identify this CIE.  */
  ULONGEST cie_pointer;

  /* Constant that is factored out of all advance location
     instructions.  */
  ULONGEST code_alignment_factor;

  /* Constants that is factored out of all offset instructions.  */
  LONGEST data_alignment_factor;

  /* Return address column.  */
  ULONGEST return_address_register;

  /* Instruction sequence to initialize a register set.  */
  gdb_byte *initial_instructions;
  gdb_byte *end;

  /* Saved augmentation, in case it's needed later.  */
  char *augmentation;

  /* Encoding of addresses.  */
  gdb_byte encoding;

  /* True if a 'z' augmentation existed.  */
  unsigned char saw_z_augmentation;

  /* True if an 'S' augmentation existed.  */
  unsigned char signal_frame;

  /* The version recorded in the CIE.  */
  unsigned char version;

  struct dwarf2_cie *next;
};

/* Frame Description Entry (FDE).  */

struct dwarf2_fde
{
  /* CIE for this FDE.  */
  struct dwarf2_cie *cie;

  /* First location associated with this FDE.  */
  CORE_ADDR initial_location;

  /* Number of bytes of program instructions described by this FDE.  */
  CORE_ADDR address_range;

  /* Instruction sequence.  */
  gdb_byte *instructions;
  gdb_byte *end;

  /* True if this FDE is read from a .eh_frame instead of a .debug_frame
     section.  */
  unsigned char eh_frame_p;

  struct dwarf2_fde *next;
};

static struct dwarf2_fde *dwarf2_frame_find_fde (CORE_ADDR *pc);



/* Structure describing a frame state.  */

struct dwarf2_frame_state
{
  /* Each register save state can be described in terms of a CFA slot,
     another register, or a location expression.  */
  struct dwarf2_frame_state_reg_info
  {
    struct dwarf2_frame_state_reg *reg;
    int num_regs;

    /* Used to implement DW_CFA_remember_state.  */
    struct dwarf2_frame_state_reg_info *prev;
  } regs;

  LONGEST cfa_offset;
  ULONGEST cfa_reg;
  gdb_byte *cfa_exp;
  enum {
    CFA_UNSET,
    CFA_REG_OFFSET,
    CFA_EXP
  } cfa_how;

  /* The PC described by the current frame state.  */
  CORE_ADDR pc;

  /* Initial register set from the CIE.
     Used to implement DW_CFA_restore.  */
  struct dwarf2_frame_state_reg_info initial;

  /* The information we care about from the CIE.  */
  LONGEST data_align;
  ULONGEST code_align;
  ULONGEST retaddr_column;

  /* Flags for known producer quirks.  */

  /* The ARM compilers, in DWARF2 mode, assume that DW_CFA_def_cfa
     and DW_CFA_def_cfa_offset takes a factored offset.  */
  int armcc_cfa_offsets_sf;

  /* The ARM compilers, in DWARF2 or DWARF3 mode, may assume that
     the CFA is defined as REG - OFFSET rather than REG + OFFSET.  */
  int armcc_cfa_offsets_reversed;
};

/* Store the length the expression for the CFA in the `cfa_reg' field,
   which is unused in that case.  */
#define cfa_exp_len cfa_reg

/* Assert that the register set RS is large enough to store gdbarch_num_regs
   columns.  If necessary, enlarge the register set.  */

static void
dwarf2_frame_state_alloc_regs (struct dwarf2_frame_state_reg_info *rs,
			       int num_regs)
{
  size_t size = sizeof (struct dwarf2_frame_state_reg);

  if (num_regs <= rs->num_regs)
    return;

  rs->reg = (struct dwarf2_frame_state_reg *)
    xrealloc (rs->reg, num_regs * size);

  /* Initialize newly allocated registers.  */
  memset (rs->reg + rs->num_regs, 0, (num_regs - rs->num_regs) * size);
  rs->num_regs = num_regs;
}

/* Copy the register columns in register set RS into newly allocated
   memory and return a pointer to this newly created copy.  */

static struct dwarf2_frame_state_reg *
dwarf2_frame_state_copy_regs (struct dwarf2_frame_state_reg_info *rs)
{
  size_t size = rs->num_regs * sizeof (struct dwarf2_frame_state_reg);
  struct dwarf2_frame_state_reg *reg;

  reg = (struct dwarf2_frame_state_reg *) xmalloc (size);
  memcpy (reg, rs->reg, size);

  return reg;
}

/* Release the memory allocated to register set RS.  */

static void
dwarf2_frame_state_free_regs (struct dwarf2_frame_state_reg_info *rs)
{
  if (rs)
    {
      dwarf2_frame_state_free_regs (rs->prev);

      xfree (rs->reg);
      xfree (rs);
    }
}

/* Release the memory allocated to the frame state FS.  */

static void
dwarf2_frame_state_free (void *p)
{
  struct dwarf2_frame_state *fs = p;

  dwarf2_frame_state_free_regs (fs->initial.prev);
  dwarf2_frame_state_free_regs (fs->regs.prev);
  xfree (fs->initial.reg);
  xfree (fs->regs.reg);
  xfree (fs);
}


/* Helper functions for execute_stack_op.  */

static CORE_ADDR
read_reg (void *baton, int reg)
{
  struct frame_info *next_frame = (struct frame_info *) baton;
  struct gdbarch *gdbarch = get_frame_arch (next_frame);
  int regnum;
  gdb_byte *buf;

  regnum = gdbarch_dwarf2_reg_to_regnum (current_gdbarch, reg);
  int regsize = register_size (current_gdbarch, regnum);

  buf = alloca (register_size (gdbarch, regnum));
  frame_unwind_register (next_frame, regnum, buf);

  /* Convert the register to an integer.  This returns a LONGEST
     rather than a CORE_ADDR, but unpack_pointer does the same thing
     under the covers, and this makes more sense for non-pointer
     registers.  Maybe read_reg and the associated interfaces should
     deal with "struct value" instead of CORE_ADDR.  */

  /* APPLE LOCAL: Don't use unpack_long or we'll get sign extension on 
     addresses and it'll cause us all sorts of problems when debugging
     32-bit programs with our 64-bit CORE_ADDRs.  I think Andrew was 
     wrong to use unpack_long here.  */
  return extract_unsigned_integer (buf, regsize);
}

static void
read_mem (void *baton, gdb_byte *buf, CORE_ADDR addr, size_t len)
{
  read_memory (addr, buf, len);
}

static void
no_get_frame_base (void *baton, gdb_byte **start, size_t *length)
{
  internal_error (__FILE__, __LINE__,
		  _("Support for DW_OP_fbreg is unimplemented"));
}

static CORE_ADDR
no_get_tls_address (void *baton, CORE_ADDR offset)
{
  internal_error (__FILE__, __LINE__,
		  _("Support for DW_OP_GNU_push_tls_address is unimplemented"));
}

static CORE_ADDR
execute_stack_op (gdb_byte *exp, ULONGEST len,
		  struct frame_info *next_frame, CORE_ADDR initial,
                  int eh_frame_p)
{
  struct dwarf_expr_context *ctx;
  CORE_ADDR result;

  ctx = new_dwarf_expr_context ();
  ctx->baton = next_frame;
  ctx->read_reg = read_reg;
  ctx->read_mem = read_mem;
  ctx->get_frame_base = no_get_frame_base;
  ctx->get_tls_address = no_get_tls_address;

  dwarf_expr_push (ctx, initial);
  dwarf_expr_eval (ctx, exp, len, 1);
  result = dwarf_expr_fetch (ctx, 0);

  if (ctx->in_reg)
    result = read_reg (next_frame, result);

  free_dwarf_expr_context (ctx);

  return result;
}


static void
execute_cfa_program (gdb_byte *insn_ptr, gdb_byte *insn_end,
		     struct frame_info *next_frame,
		     struct dwarf2_frame_state *fs, int eh_frame_p)
{
  CORE_ADDR pc = frame_pc_unwind (next_frame);
  int bytes_read;
  struct gdbarch *gdbarch = get_frame_arch (next_frame);

  while (insn_ptr < insn_end && fs->pc <= pc)
    {
      gdb_byte insn = *insn_ptr++;
      ULONGEST utmp, reg;
      LONGEST offset;

      if ((insn & 0xc0) == DW_CFA_advance_loc)
	fs->pc += (insn & 0x3f) * fs->code_align;
      else if ((insn & 0xc0) == DW_CFA_offset)
	{
	  reg = insn & 0x3f;
	  reg = dwarf2_frame_adjust_regnum (gdbarch, reg, eh_frame_p);
	  insn_ptr = read_uleb128 (insn_ptr, insn_end, &utmp);
	  offset = utmp * fs->data_align;
	  dwarf2_frame_state_alloc_regs (&fs->regs, reg + 1);
	  fs->regs.reg[reg].how = DWARF2_FRAME_REG_SAVED_OFFSET;
	  fs->regs.reg[reg].loc.offset = offset;
	}
      else if ((insn & 0xc0) == DW_CFA_restore)
	{
	  gdb_assert (fs->initial.reg);
	  reg = insn & 0x3f;
	  reg = dwarf2_frame_adjust_regnum (gdbarch, reg, eh_frame_p);
	  dwarf2_frame_state_alloc_regs (&fs->regs, reg + 1);
	  if (reg < fs->initial.num_regs)
	    fs->regs.reg[reg] = fs->initial.reg[reg];
	  else 
	    fs->regs.reg[reg].how = DWARF2_FRAME_REG_UNSPECIFIED;

	  if (fs->regs.reg[reg].how == DWARF2_FRAME_REG_UNSPECIFIED)
	    complaint (&symfile_complaints, _("\
incomplete CFI data; DW_CFA_restore unspecified\n\
register %s (#%d) at 0x%s"),
		       gdbarch_register_name
			 (current_gdbarch, gdbarch_dwarf2_reg_to_regnum
					     (current_gdbarch, reg)),
		       gdbarch_dwarf2_reg_to_regnum (current_gdbarch, reg),
		       paddr (fs->pc));
	}
      else
	{
	  switch (insn)
	    {
	    case DW_CFA_set_loc:
	      fs->pc = dwarf2_read_address (insn_ptr, insn_end, &bytes_read);
	      insn_ptr += bytes_read;
	      break;

	    case DW_CFA_advance_loc1:
	      utmp = extract_unsigned_integer (insn_ptr, 1);
	      fs->pc += utmp * fs->code_align;
	      insn_ptr++;
	      break;
	    case DW_CFA_advance_loc2:
	      utmp = extract_unsigned_integer (insn_ptr, 2);
	      fs->pc += utmp * fs->code_align;
	      insn_ptr += 2;
	      break;
	    case DW_CFA_advance_loc4:
	      utmp = extract_unsigned_integer (insn_ptr, 4);
	      fs->pc += utmp * fs->code_align;
	      insn_ptr += 4;
	      break;

	    case DW_CFA_offset_extended:
	      insn_ptr = read_uleb128 (insn_ptr, insn_end, &reg);
	      reg = dwarf2_frame_adjust_regnum (gdbarch, reg, eh_frame_p);
	      insn_ptr = read_uleb128 (insn_ptr, insn_end, &utmp);
	      offset = utmp * fs->data_align;
	      dwarf2_frame_state_alloc_regs (&fs->regs, reg + 1);
	      fs->regs.reg[reg].how = DWARF2_FRAME_REG_SAVED_OFFSET;
	      fs->regs.reg[reg].loc.offset = offset;
	      break;

	    case DW_CFA_restore_extended:
	      gdb_assert (fs->initial.reg);
	      insn_ptr = read_uleb128 (insn_ptr, insn_end, &reg);
	      reg = dwarf2_frame_adjust_regnum (gdbarch, reg, eh_frame_p);
	      dwarf2_frame_state_alloc_regs (&fs->regs, reg + 1);
	      fs->regs.reg[reg] = fs->initial.reg[reg];
	      break;

	    case DW_CFA_undefined:
	      insn_ptr = read_uleb128 (insn_ptr, insn_end, &reg);
	      reg = dwarf2_frame_adjust_regnum (gdbarch, reg, eh_frame_p);
	      dwarf2_frame_state_alloc_regs (&fs->regs, reg + 1);
	      fs->regs.reg[reg].how = DWARF2_FRAME_REG_UNDEFINED;
	      break;

	    case DW_CFA_same_value:
	      insn_ptr = read_uleb128 (insn_ptr, insn_end, &reg);
	      reg = dwarf2_frame_adjust_regnum (gdbarch, reg, eh_frame_p);
	      dwarf2_frame_state_alloc_regs (&fs->regs, reg + 1);
	      fs->regs.reg[reg].how = DWARF2_FRAME_REG_SAME_VALUE;
	      break;

	    case DW_CFA_register:
	      insn_ptr = read_uleb128 (insn_ptr, insn_end, &reg);
	      reg = dwarf2_frame_adjust_regnum (gdbarch, reg, eh_frame_p);
	      insn_ptr = read_uleb128 (insn_ptr, insn_end, &utmp);
	      utmp = dwarf2_frame_adjust_regnum (gdbarch, utmp, eh_frame_p);
	      dwarf2_frame_state_alloc_regs (&fs->regs, reg + 1);
	      fs->regs.reg[reg].how = DWARF2_FRAME_REG_SAVED_REG;
	      fs->regs.reg[reg].loc.reg = utmp;
	      break;

	    case DW_CFA_remember_state:
	      {
		struct dwarf2_frame_state_reg_info *new_rs;

		new_rs = XMALLOC (struct dwarf2_frame_state_reg_info);
		*new_rs = fs->regs;
		fs->regs.reg = dwarf2_frame_state_copy_regs (&fs->regs);
		fs->regs.prev = new_rs;
	      }
	      break;

	    case DW_CFA_restore_state:
	      {
		struct dwarf2_frame_state_reg_info *old_rs = fs->regs.prev;

		if (old_rs == NULL)
		  {
		    complaint (&symfile_complaints, _("\
bad CFI data; mismatched DW_CFA_restore_state at 0x%s"), paddr (fs->pc));
		  }
		else
		  {
		    xfree (fs->regs.reg);
		    fs->regs = *old_rs;
		    xfree (old_rs);
		  }
	      }
	      break;

	    case DW_CFA_def_cfa:
	      insn_ptr = read_uleb128 (insn_ptr, insn_end, &fs->cfa_reg);
	      insn_ptr = read_uleb128 (insn_ptr, insn_end, &utmp);

              /* APPLE LOCAL: Don't forget to translate the CFA register 
                 number... */
              fs->cfa_reg = dwarf2_frame_adjust_regnum (gdbarch, fs->cfa_reg,
                                                        eh_frame_p);

	      if (fs->armcc_cfa_offsets_sf)
		utmp *= fs->data_align;

	      fs->cfa_offset = utmp;
	      fs->cfa_how = CFA_REG_OFFSET;
	      break;

	    case DW_CFA_def_cfa_register:
	      insn_ptr = read_uleb128 (insn_ptr, insn_end, &fs->cfa_reg);
	      fs->cfa_reg = dwarf2_frame_adjust_regnum (gdbarch, fs->cfa_reg,
							eh_frame_p);
	      fs->cfa_how = CFA_REG_OFFSET;
	      break;

	    case DW_CFA_def_cfa_offset:
	      insn_ptr = read_uleb128 (insn_ptr, insn_end, &utmp);

	      if (fs->armcc_cfa_offsets_sf)
		utmp *= fs->data_align;

	      fs->cfa_offset = utmp;
	      /* cfa_how deliberately not set.  */
	      break;

	    case DW_CFA_nop:
	      break;

	    case DW_CFA_def_cfa_expression:
	      insn_ptr = read_uleb128 (insn_ptr, insn_end, &fs->cfa_exp_len);
	      fs->cfa_exp = insn_ptr;
	      fs->cfa_how = CFA_EXP;
	      insn_ptr += fs->cfa_exp_len;
	      break;

	    case DW_CFA_expression:
	      insn_ptr = read_uleb128 (insn_ptr, insn_end, &reg);
	      reg = dwarf2_frame_adjust_regnum (gdbarch, reg, eh_frame_p);
	      dwarf2_frame_state_alloc_regs (&fs->regs, reg + 1);
	      insn_ptr = read_uleb128 (insn_ptr, insn_end, &utmp);
	      fs->regs.reg[reg].loc.exp = insn_ptr;
	      fs->regs.reg[reg].exp_len = utmp;
	      fs->regs.reg[reg].how = DWARF2_FRAME_REG_SAVED_EXP;
	      insn_ptr += utmp;
	      break;

	    case DW_CFA_offset_extended_sf:
	      insn_ptr = read_uleb128 (insn_ptr, insn_end, &reg);
	      reg = dwarf2_frame_adjust_regnum (gdbarch, reg, eh_frame_p);
	      insn_ptr = read_sleb128 (insn_ptr, insn_end, &offset);
	      offset *= fs->data_align;
	      dwarf2_frame_state_alloc_regs (&fs->regs, reg + 1);
	      fs->regs.reg[reg].how = DWARF2_FRAME_REG_SAVED_OFFSET;
	      fs->regs.reg[reg].loc.offset = offset;
	      break;

	    case DW_CFA_val_offset:
	      insn_ptr = read_uleb128 (insn_ptr, insn_end, &reg);
	      dwarf2_frame_state_alloc_regs (&fs->regs, reg + 1);
	      insn_ptr = read_uleb128 (insn_ptr, insn_end, &utmp);
	      offset = utmp * fs->data_align;
	      fs->regs.reg[reg].how = DWARF2_FRAME_REG_SAVED_VAL_OFFSET;
	      fs->regs.reg[reg].loc.offset = offset;
	      break;

	    case DW_CFA_val_offset_sf:
	      insn_ptr = read_uleb128 (insn_ptr, insn_end, &reg);
	      dwarf2_frame_state_alloc_regs (&fs->regs, reg + 1);
	      insn_ptr = read_sleb128 (insn_ptr, insn_end, &offset);
	      offset *= fs->data_align;
	      fs->regs.reg[reg].how = DWARF2_FRAME_REG_SAVED_VAL_OFFSET;
	      fs->regs.reg[reg].loc.offset = offset;
	      break;

	    case DW_CFA_val_expression:
	      insn_ptr = read_uleb128 (insn_ptr, insn_end, &reg);
	      dwarf2_frame_state_alloc_regs (&fs->regs, reg + 1);
	      insn_ptr = read_uleb128 (insn_ptr, insn_end, &utmp);
	      fs->regs.reg[reg].loc.exp = insn_ptr;
	      fs->regs.reg[reg].exp_len = utmp;
	      fs->regs.reg[reg].how = DWARF2_FRAME_REG_SAVED_VAL_EXP;
	      insn_ptr += utmp;
	      break;

	    case DW_CFA_def_cfa_sf:
	      insn_ptr = read_uleb128 (insn_ptr, insn_end, &fs->cfa_reg);
	      fs->cfa_reg = dwarf2_frame_adjust_regnum (gdbarch, fs->cfa_reg,
							eh_frame_p);
	      insn_ptr = read_sleb128 (insn_ptr, insn_end, &offset);
	      fs->cfa_offset = offset * fs->data_align;
	      fs->cfa_how = CFA_REG_OFFSET;
	      break;

	    case DW_CFA_def_cfa_offset_sf:
	      insn_ptr = read_sleb128 (insn_ptr, insn_end, &offset);
	      fs->cfa_offset = offset * fs->data_align;
	      /* cfa_how deliberately not set.  */
	      break;

	    case DW_CFA_GNU_window_save:
	      /* This is SPARC-specific code, and contains hard-coded
		 constants for the register numbering scheme used by
		 GCC.  Rather than having a architecture-specific
		 operation that's only ever used by a single
		 architecture, we provide the implementation here.
		 Incidentally that's what GCC does too in its
		 unwinder.  */
	      {
		struct gdbarch *gdbarch = get_frame_arch (next_frame);
		int size = register_size(gdbarch, 0);
		dwarf2_frame_state_alloc_regs (&fs->regs, 32);
		for (reg = 8; reg < 16; reg++)
		  {
		    fs->regs.reg[reg].how = DWARF2_FRAME_REG_SAVED_REG;
		    fs->regs.reg[reg].loc.reg = reg + 16;
		  }
		for (reg = 16; reg < 32; reg++)
		  {
		    fs->regs.reg[reg].how = DWARF2_FRAME_REG_SAVED_OFFSET;
		    fs->regs.reg[reg].loc.offset = (reg - 16) * size;
		  }
	      }
	      break;

	    case DW_CFA_GNU_args_size:
	      /* Ignored.  */
	      insn_ptr = read_uleb128 (insn_ptr, insn_end, &utmp);
	      break;

	    case DW_CFA_GNU_negative_offset_extended:
	      insn_ptr = read_uleb128 (insn_ptr, insn_end, &reg);
	      reg = dwarf2_frame_adjust_regnum (gdbarch, reg, eh_frame_p);
	      insn_ptr = read_uleb128 (insn_ptr, insn_end, &offset);
	      offset *= fs->data_align;
	      dwarf2_frame_state_alloc_regs (&fs->regs, reg + 1);
	      fs->regs.reg[reg].how = DWARF2_FRAME_REG_SAVED_OFFSET;
	      fs->regs.reg[reg].loc.offset = -offset;
	      break;

	    default:
	      internal_error (__FILE__, __LINE__, _("Unknown CFI encountered."));
	    }
	}
    }

  /* Don't allow remember/restore between CIE and FDE programs.  */
  dwarf2_frame_state_free_regs (fs->regs.prev);
  fs->regs.prev = NULL;
}


/* Architecture-specific operations.  */

/* Per-architecture data key.  */
static struct gdbarch_data *dwarf2_frame_data;

struct dwarf2_frame_ops
{
  /* Pre-initialize the register state REG for register REGNUM.  */
  void (*init_reg) (struct gdbarch *, int, struct dwarf2_frame_state_reg *,
		    struct frame_info *);

  /* Check whether the frame preceding NEXT_FRAME will be a signal
     trampoline.  */
  int (*signal_frame_p) (struct gdbarch *, struct frame_info *);

  /* Convert .eh_frame register number to DWARF register number, or
     adjust .debug_frame register number.  */
  int (*adjust_regnum) (struct gdbarch *, int, int);
};

/* Default architecture-specific register state initialization
   function.  */

static void
dwarf2_frame_default_init_reg (struct gdbarch *gdbarch, int regnum,
			       struct dwarf2_frame_state_reg *reg,
			       struct frame_info *next_frame)
{
  /* If we have a register that acts as a program counter, mark it as
     a destination for the return address.  If we have a register that
     serves as the stack pointer, arrange for it to be filled with the
     call frame address (CFA).  The other registers are marked as
     unspecified.

     We copy the return address to the program counter, since many
     parts in GDB assume that it is possible to get the return address
     by unwinding the program counter register.  However, on ISA's
     with a dedicated return address register, the CFI usually only
     contains information to unwind that return address register.

     The reason we're treating the stack pointer special here is
     because in many cases GCC doesn't emit CFI for the stack pointer
     and implicitly assumes that it is equal to the CFA.  This makes
     some sense since the DWARF specification (version 3, draft 8,
     p. 102) says that:

     "Typically, the CFA is defined to be the value of the stack
     pointer at the call site in the previous frame (which may be
     different from its value on entry to the current frame)."

     However, this isn't true for all platforms supported by GCC
     (e.g. IBM S/390 and zSeries).  Those architectures should provide
     their own architecture-specific initialization function.  */

  if (regnum == gdbarch_pc_regnum (current_gdbarch))
    reg->how = DWARF2_FRAME_REG_RA;
  else if (regnum == gdbarch_sp_regnum (current_gdbarch))
    reg->how = DWARF2_FRAME_REG_CFA;
}

/* Return a default for the architecture-specific operations.  */

static void *
dwarf2_frame_init (struct obstack *obstack)
{
  struct dwarf2_frame_ops *ops;
  
  ops = OBSTACK_ZALLOC (obstack, struct dwarf2_frame_ops);
  ops->init_reg = dwarf2_frame_default_init_reg;
  return ops;
}

/* Set the architecture-specific register state initialization
   function for GDBARCH to INIT_REG.  */

void
dwarf2_frame_set_init_reg (struct gdbarch *gdbarch,
			   void (*init_reg) (struct gdbarch *, int,
					     struct dwarf2_frame_state_reg *,
					     struct frame_info *))
{
  struct dwarf2_frame_ops *ops = gdbarch_data (gdbarch, dwarf2_frame_data);

  ops->init_reg = init_reg;
}

/* Pre-initialize the register state REG for register REGNUM.  */

static void
dwarf2_frame_init_reg (struct gdbarch *gdbarch, int regnum,
		       struct dwarf2_frame_state_reg *reg,
		       struct frame_info *next_frame)
{
  struct dwarf2_frame_ops *ops = gdbarch_data (gdbarch, dwarf2_frame_data);

  ops->init_reg (gdbarch, regnum, reg, next_frame);
}

/* Set the architecture-specific signal trampoline recognition
   function for GDBARCH to SIGNAL_FRAME_P.  */

void
dwarf2_frame_set_signal_frame_p (struct gdbarch *gdbarch,
				 int (*signal_frame_p) (struct gdbarch *,
							struct frame_info *))
{
  struct dwarf2_frame_ops *ops = gdbarch_data (gdbarch, dwarf2_frame_data);

  ops->signal_frame_p = signal_frame_p;
}

/* Query the architecture-specific signal frame recognizer for
   NEXT_FRAME.  */

static int
dwarf2_frame_signal_frame_p (struct gdbarch *gdbarch,
			     struct frame_info *next_frame)
{
  struct dwarf2_frame_ops *ops = gdbarch_data (gdbarch, dwarf2_frame_data);

  if (ops->signal_frame_p == NULL)
    return 0;
  return ops->signal_frame_p (gdbarch, next_frame);
}

/* Set the architecture-specific adjustment of .eh_frame and .debug_frame
   register numbers.  */

void
dwarf2_frame_set_adjust_regnum (struct gdbarch *gdbarch,
				int (*adjust_regnum) (struct gdbarch *,
						      int, int))
{
  struct dwarf2_frame_ops *ops = gdbarch_data (gdbarch, dwarf2_frame_data);

  ops->adjust_regnum = adjust_regnum;
}

/* Translate a .eh_frame register to DWARF register, or adjust a .debug_frame
   register.  */

int
dwarf2_frame_adjust_regnum (struct gdbarch *gdbarch, int regnum, int eh_frame_p)
{
  struct dwarf2_frame_ops *ops = gdbarch_data (gdbarch, dwarf2_frame_data);

  if (ops->adjust_regnum == NULL)
    return regnum;
  return ops->adjust_regnum (gdbarch, regnum, eh_frame_p);
}

static void
dwarf2_frame_find_quirks (struct dwarf2_frame_state *fs,
			  struct dwarf2_fde *fde)
{
  static const char *arm_idents[] = {
    "ARM C Compiler, ADS",
    "Thumb C Compiler, ADS",
    "ARM C++ Compiler, ADS",
    "Thumb C++ Compiler, ADS",
    "ARM/Thumb C/C++ Compiler, RVCT"
  };
  int i;

  struct symtab *s;

  s = find_pc_symtab (fs->pc);
  if (s == NULL || s->producer == NULL)
    return;

  for (i = 0; i < ARRAY_SIZE (arm_idents); i++)
    if (strncmp (s->producer, arm_idents[i], strlen (arm_idents[i])) == 0)
      {
	if (fde->cie->version == 1)
	  fs->armcc_cfa_offsets_sf = 1;

	if (fde->cie->version == 1)
	  fs->armcc_cfa_offsets_reversed = 1;

	/* The reversed offset problem is present in some compilers
	   using DWARF3, but it was eventually fixed.  Check the ARM
	   defined augmentations, which are in the format "armcc" followed
	   by a list of one-character options.  The "+" option means
	   this problem is fixed (no quirk needed).  If the armcc
	   augmentation is missing, the quirk is needed.  */
	if (fde->cie->version == 3
	    && (strncmp (fde->cie->augmentation, "armcc", 5) != 0
		|| strchr (fde->cie->augmentation + 5, '+') == NULL))
	  fs->armcc_cfa_offsets_reversed = 1;

	return;
      }
}


struct dwarf2_frame_cache
{
  /* DWARF Call Frame Address.  */
  CORE_ADDR cfa;

  /* Set if the return address column was marked as undefined.  */
  int undefined_retaddr;

  /* Saved registers, indexed by GDB register number, not by DWARF
     register number.  */
  struct dwarf2_frame_state_reg *reg;

  /* Return address register.  */
  struct dwarf2_frame_state_reg retaddr_reg;

  int eh_frame_p;
};

static struct dwarf2_frame_cache *
dwarf2_frame_cache (struct frame_info *next_frame, void **this_cache)
{
  struct cleanup *old_chain;
  struct gdbarch *gdbarch = get_frame_arch (next_frame);

  /* APPLE LOCAL: We should really use something like 
     gdbarch_num_dwarf_registers here.  gdb may know about additional
     registers that don't have code points in DWARF, e.g. on x86-64
     we have things like fstat, ftag, fiseg, fioff, foseg, fooff, etc.,
     which gcc will never try to communicate to us.
     The end result is that the number of registers for the current
     architecture is quite a bit higher than the number of DWARF registers
     communicated between the compiler & the debugger and we emit warnings
     when we try to fill out the entire table.  */
  const int num_regs = gdbarch_num_regs (current_gdbarch)
		       + gdbarch_num_pseudo_regs (current_gdbarch);

  struct dwarf2_frame_cache *cache;
  struct dwarf2_frame_state *fs;
  struct dwarf2_fde *fde;

  if (*this_cache)
    return *this_cache;

  /* Allocate a new cache.  */
  cache = FRAME_OBSTACK_ZALLOC (struct dwarf2_frame_cache);
  cache->reg = FRAME_OBSTACK_CALLOC (num_regs, struct dwarf2_frame_state_reg);

  /* Allocate and initialize the frame state.  */
  fs = XMALLOC (struct dwarf2_frame_state);
  memset (fs, 0, sizeof (struct dwarf2_frame_state));
  old_chain = make_cleanup (dwarf2_frame_state_free, fs);

  /* Unwind the PC.

     Note that if NEXT_FRAME is never supposed to return (i.e. a call
     to abort), the compiler might optimize away the instruction at
     NEXT_FRAME's return address.  As a result the return address will
     point at some random instruction, and the CFI for that
     instruction is probably worthless to us.  GCC's unwinder solves
     this problem by substracting 1 from the return address to get an
     address in the middle of a presumed call instruction (or the
     instruction in the associated delay slot).  This should only be
     done for "normal" frames and not for resume-type frames (signal
     handlers, sentinel frames, dummy frames).  The function
     frame_unwind_address_in_block does just this.  It's not clear how
     reliable the method is though; there is the potential for the
     register state pre-call being different to that on return.  */
  fs->pc = frame_unwind_address_in_block (next_frame);

  /* Find the correct FDE.  */
  fde = dwarf2_frame_find_fde (&fs->pc);
  gdb_assert (fde != NULL);

  /* Extract any interesting information from the CIE.  */
  fs->data_align = fde->cie->data_alignment_factor;
  fs->code_align = fde->cie->code_alignment_factor;
  fs->retaddr_column = fde->cie->return_address_register;

  /* Check for "quirks" - known bugs in producers.  */
  dwarf2_frame_find_quirks (fs, fde);

  cache->eh_frame_p = fde->eh_frame_p;

  /* First decode all the insns in the CIE.  */
  execute_cfa_program (fde->cie->initial_instructions,
		       fde->cie->end, next_frame, fs, fde->eh_frame_p);

  /* Save the initialized register set.  */
  fs->initial = fs->regs;
  fs->initial.reg = dwarf2_frame_state_copy_regs (&fs->regs);

  /* Then decode the insns in the FDE up to our target PC.  */
  execute_cfa_program (fde->instructions, fde->end, next_frame, fs,
		       fde->eh_frame_p);

  /* Caclulate the CFA.  */
  switch (fs->cfa_how)
    {
    case CFA_REG_OFFSET:
      cache->cfa = read_reg (next_frame, fs->cfa_reg);
      if (fs->armcc_cfa_offsets_reversed)
	cache->cfa -= fs->cfa_offset;
      else
	cache->cfa += fs->cfa_offset;
      break;

    case CFA_EXP:
      cache->cfa =
	execute_stack_op (fs->cfa_exp, fs->cfa_exp_len, next_frame, 0, 
                          fde->eh_frame_p);
      break;

    default:
      internal_error (__FILE__, __LINE__, _("Unknown CFA rule."));
    }

  /* Initialize the register state.  */
  {
    int regnum;

    for (regnum = 0; regnum < num_regs; regnum++)
      dwarf2_frame_init_reg (gdbarch, regnum, &cache->reg[regnum], next_frame);
  }

  /* Go through the DWARF2 CFI generated table and save its register
     location information in the cache.  Note that we don't skip the
     return address column; it's perfectly all right for it to
     correspond to a real register.  If it doesn't correspond to a
     real register, or if we shouldn't treat it as such,
     gdbarch_dwarf2_reg_to_regnum should be defined to return a number outside
     the range [0, gdbarch_num_regs).  */
  {
    int column;		/* CFI speak for "register number".  */

    for (column = 0; column < fs->regs.num_regs; column++)
      {
	/* Use the GDB register number as the destination index.  */
	int regnum = gdbarch_dwarf2_reg_to_regnum (current_gdbarch, column);

	/* If there's no corresponding GDB register, ignore it.  */
	if (regnum < 0 || regnum >= num_regs)
	  continue;

	/* NOTE: cagney/2003-09-05: CFI should specify the disposition
	   of all debug info registers.  If it doesn't, complain (but
	   not too loudly).  It turns out that GCC assumes that an
	   unspecified register implies "same value" when CFI (draft
	   7) specifies nothing at all.  Such a register could equally
	   be interpreted as "undefined".  Also note that this check
	   isn't sufficient; it only checks that all registers in the
	   range [0 .. max column] are specified, and won't detect
	   problems when a debug info register falls outside of the
	   table.  We need a way of iterating through all the valid
	   DWARF2 register numbers.  */
	if (fs->regs.reg[column].how == DWARF2_FRAME_REG_UNSPECIFIED)
	  {
	    if (cache->reg[regnum].how == DWARF2_FRAME_REG_UNSPECIFIED)
              {
/* APPLE LOCAL: I don't care.  jmolenda/2008-05-21 */
#if 0
	        complaint (&symfile_complaints, _("\
incomplete CFI data; unspecified registers (e.g., %s) at 0x%s"),
			 gdbarch_register_name (gdbarch, regnum),
			 paddr_nz (fs->pc));
#endif
              }
	  }
	else
	  cache->reg[regnum] = fs->regs.reg[column];
      }
  }

  /* Eliminate any DWARF2_FRAME_REG_RA rules, and save the information
     we need for evaluating DWARF2_FRAME_REG_RA_OFFSET rules.  */
  {
    int regnum;

    for (regnum = 0; regnum < num_regs; regnum++)
      {
	if (cache->reg[regnum].how == DWARF2_FRAME_REG_RA
	    || cache->reg[regnum].how == DWARF2_FRAME_REG_RA_OFFSET)
	  {
	    struct dwarf2_frame_state_reg *retaddr_reg =
	      &fs->regs.reg[fs->retaddr_column];

	    /* It seems rather bizarre to specify an "empty" column as
               the return adress column.  However, this is exactly
               what GCC does on some targets.  It turns out that GCC
               assumes that the return address can be found in the
               register corresponding to the return address column.
               Incidentally, that's how we should treat a return
               address column specifying "same value" too.  */
	    if (fs->retaddr_column < fs->regs.num_regs
		&& retaddr_reg->how != DWARF2_FRAME_REG_UNSPECIFIED
		&& retaddr_reg->how != DWARF2_FRAME_REG_SAME_VALUE)
	      {
		if (cache->reg[regnum].how == DWARF2_FRAME_REG_RA)
		  cache->reg[regnum] = *retaddr_reg;
		else
		  cache->retaddr_reg = *retaddr_reg;
	      }
	    else
	      {
		if (cache->reg[regnum].how == DWARF2_FRAME_REG_RA)
		  {
		    cache->reg[regnum].loc.reg = fs->retaddr_column;
		    cache->reg[regnum].how = DWARF2_FRAME_REG_SAVED_REG;
		  }
		else
		  {
		    cache->retaddr_reg.loc.reg = fs->retaddr_column;
		    cache->retaddr_reg.how = DWARF2_FRAME_REG_SAVED_REG;
		  }
	      }
	  }
      }
  }

  if (fs->retaddr_column < fs->regs.num_regs
      && fs->regs.reg[fs->retaddr_column].how == DWARF2_FRAME_REG_UNDEFINED)
    cache->undefined_retaddr = 1;

  do_cleanups (old_chain);

  *this_cache = cache;
  return cache;
}

static void
dwarf2_frame_this_id (struct frame_info *next_frame, void **this_cache,
		      struct frame_id *this_id)
{
  struct dwarf2_frame_cache *cache =
    dwarf2_frame_cache (next_frame, this_cache);

  if (cache->undefined_retaddr)
    return;

  (*this_id) = frame_id_build (cache->cfa,
			       frame_func_unwind (next_frame));
}

static void
dwarf2_signal_frame_this_id (struct frame_info *next_frame, void **this_cache,
			     struct frame_id *this_id)
{
  struct dwarf2_frame_cache *cache =
    dwarf2_frame_cache (next_frame, this_cache);

  if (cache->undefined_retaddr)
    return;

  (*this_id) = frame_id_build (cache->cfa,
			       frame_func_unwind (next_frame));
}

static void
dwarf2_frame_prev_register (struct frame_info *next_frame, void **this_cache,
			    int regnum, int *optimizedp,
			    enum lval_type *lvalp, CORE_ADDR *addrp,
			    int *realnump, gdb_byte *valuep)
{
  struct gdbarch *gdbarch = get_frame_arch (next_frame);
  struct dwarf2_frame_cache *cache =
    dwarf2_frame_cache (next_frame, this_cache);

  switch (cache->reg[regnum].how)
    {
    case DWARF2_FRAME_REG_UNDEFINED:
      /* If CFI explicitly specified that the value isn't defined,
	 mark it as optimized away; the value isn't available.  */
      /* APPLE LOCAL variable opt states.  */
      *optimizedp = opt_away;
      *lvalp = not_lval;
      *addrp = 0;
      *realnump = -1;
      if (valuep)
	{
	  /* In some cases, for example %eflags on the i386, we have
	     to provide a sane value, even though this register wasn't
	     saved.  Assume we can get it from NEXT_FRAME.  */
	  frame_unwind_register (next_frame, regnum, valuep);
	}
      break;

    case DWARF2_FRAME_REG_SAVED_OFFSET:
      /* APPLE LOCAL variable opt states.  */
      *optimizedp = opt_okay;
      *lvalp = lval_memory;
      *addrp = cache->cfa + cache->reg[regnum].loc.offset;
      *realnump = -1;
      if (valuep)
	{
	  /* Read the value in from memory.  */
	  read_memory (*addrp, valuep, register_size (gdbarch, regnum));
	}
      break;

    case DWARF2_FRAME_REG_SAVED_REG:
      /* APPLE LOCAL variable opt states.  */
      *optimizedp = opt_okay;
      *lvalp = lval_register;
      *addrp = 0;
      *realnump = gdbarch_dwarf2_reg_to_regnum
		    (current_gdbarch, cache->reg[regnum].loc.reg);
      if (valuep)
	frame_unwind_register (next_frame, (*realnump), valuep);
      break;

    case DWARF2_FRAME_REG_SAVED_EXP:
      /* APPLE LOCAL variable opt states.  */
      *optimizedp = opt_okay;
      *lvalp = lval_memory;
      *addrp = execute_stack_op (cache->reg[regnum].loc.exp,
				 cache->reg[regnum].exp_len,
				 next_frame, cache->cfa, cache->eh_frame_p);
      *realnump = -1;
      if (valuep)
	{
	  /* Read the value in from memory.  */
	  read_memory (*addrp, valuep, register_size (gdbarch, regnum));
	}
      break;

    case DWARF2_FRAME_REG_SAVED_VAL_OFFSET:
      /* APPLE LOCAL variable opt states.  */
      *optimizedp = opt_okay;
      *lvalp = not_lval;
      *addrp = 0;
      *realnump = -1;
      if (valuep)
	store_unsigned_integer (valuep, register_size (gdbarch, regnum),
				cache->cfa + cache->reg[regnum].loc.offset);
      break;

    case DWARF2_FRAME_REG_SAVED_VAL_EXP:
      /* APPLE LOCAL variable opt states.  */
      *optimizedp = opt_okay;
      *lvalp = not_lval;
      *addrp = 0;
      *realnump = -1;
      if (valuep)
	store_unsigned_integer (valuep, register_size (gdbarch, regnum),
				execute_stack_op (cache->reg[regnum].loc.exp,
						  cache->reg[regnum].exp_len,
						  next_frame, cache->cfa,
                                                  cache->eh_frame_p));
      break;

    case DWARF2_FRAME_REG_UNSPECIFIED:
      /* GCC, in its infinite wisdom decided to not provide unwind
	 information for registers that are "same value".  Since
	 DWARF2 (3 draft 7) doesn't define such behavior, said
	 registers are actually undefined (which is different to CFI
	 "undefined").  Code above issues a complaint about this.
	 Here just fudge the books, assume GCC, and that the value is
	 more inner on the stack.  */
      /* APPLE LOCAL variable opt states.  */
      *optimizedp = opt_okay;
      *lvalp = lval_register;
      *addrp = 0;
      *realnump = regnum;
      if (valuep)
	frame_unwind_register (next_frame, (*realnump), valuep);
      break;

    case DWARF2_FRAME_REG_SAME_VALUE:
      /* APPLE LOCAL variable opt states.  */
      *optimizedp = opt_okay;
      *lvalp = lval_register;
      *addrp = 0;
      *realnump = regnum;
      if (valuep)
	frame_unwind_register (next_frame, (*realnump), valuep);
      break;

    case DWARF2_FRAME_REG_CFA:
      /* APPLE LOCAL variable opt states.  */
      *optimizedp = opt_okay;
      *lvalp = not_lval;
      *addrp = 0;
      *realnump = -1;
      if (valuep)
	pack_long (valuep, register_type (gdbarch, regnum), cache->cfa);
      break;

    case DWARF2_FRAME_REG_CFA_OFFSET:
      /* APPLE LOCAL variable opt states.  */
      *optimizedp = opt_okay;
      *lvalp = not_lval;
      *addrp = 0;
      *realnump = -1;
      if (valuep)
	pack_long (valuep, register_type (gdbarch, regnum),
		   cache->cfa + cache->reg[regnum].loc.offset);
      break;

    case DWARF2_FRAME_REG_RA_OFFSET:
      /* APPLE LOCAL variable opt states.  */
      *optimizedp = opt_okay;
      *lvalp = not_lval;
      *addrp = 0;
      *realnump = -1;
      if (valuep)
        {
          CORE_ADDR pc = cache->reg[regnum].loc.offset;

          regnum = gdbarch_dwarf2_reg_to_regnum
		     (current_gdbarch, cache->retaddr_reg.loc.reg);
          pc += frame_unwind_register_unsigned (next_frame, regnum);
          pack_long (valuep, register_type (gdbarch, regnum), pc);
        }
      break;

    default:
      internal_error (__FILE__, __LINE__, _("Unknown register rule."));
    }
}

static const struct frame_unwind dwarf2_frame_unwind =
{
  NORMAL_FRAME,
  dwarf2_frame_this_id,
  dwarf2_frame_prev_register
};

static const struct frame_unwind dwarf2_signal_frame_unwind =
{
  SIGTRAMP_FRAME,
  dwarf2_signal_frame_this_id,
  dwarf2_frame_prev_register
};

const struct frame_unwind *
dwarf2_frame_sniffer (struct frame_info *next_frame)
{
  /* Grab an address that is guarenteed to reside somewhere within the
     function.  frame_pc_unwind(), for a no-return next function, can
     end up returning something past the end of this function's body.
     If the frame we're sniffing for is a signal frame whose start
     address is placed on the stack by the OS, its FDE must
     extend one byte before its start address or we will miss it.  */
  CORE_ADDR block_addr = frame_unwind_address_in_block (next_frame);

  struct dwarf2_fde *fde = dwarf2_frame_find_fde (&block_addr);
  if (!fde)
    return NULL;

  /* On some targets, signal trampolines may have unwind information.
     We need to recognize them so that we set the frame type
     correctly.  */

  if (fde->cie->signal_frame
      || dwarf2_frame_signal_frame_p (get_frame_arch (next_frame),
				      next_frame))
    return &dwarf2_signal_frame_unwind;

  return &dwarf2_frame_unwind;
}


/* There is no explicitly defined relationship between the CFA and the
   location of frame's local variables and arguments/parameters.
   Therefore, frame base methods on this page should probably only be
   used as a last resort, just to avoid printing total garbage as a
   response to the "info frame" command.  */

static CORE_ADDR
dwarf2_frame_base_address (struct frame_info *next_frame, void **this_cache)
{
  struct dwarf2_frame_cache *cache =
    dwarf2_frame_cache (next_frame, this_cache);

  return cache->cfa;
}

static const struct frame_base dwarf2_frame_base =
{
  &dwarf2_frame_unwind,
  dwarf2_frame_base_address,
  dwarf2_frame_base_address,
  dwarf2_frame_base_address
};

const struct frame_base *
dwarf2_frame_base_sniffer (struct frame_info *next_frame)
{
  CORE_ADDR block_addr = frame_unwind_address_in_block (next_frame);

  if (dwarf2_frame_find_fde (&block_addr))
    return &dwarf2_frame_base;

  return NULL;
}

/* A minimal decoding of DWARF2 compilation units.  We only decode
   what's needed to get to the call frame information.  */

struct comp_unit
{
  /* Keep the bfd convenient.  */
  bfd *abfd;

  struct objfile *objfile;

  /* Linked list of CIEs for this object.  */
  struct dwarf2_cie *cie;

  /* Pointer to the .debug_frame section loaded into memory.  */
  gdb_byte *dwarf_frame_buffer;

  /* Length of the loaded .debug_frame section.  */
  unsigned long dwarf_frame_size;

  /* Pointer to the .debug_frame section.  */
  asection *dwarf_frame_section;

  /* Base for DW_EH_PE_datarel encodings.  */
  bfd_vma dbase;

  /* Base for DW_EH_PE_textrel encodings.  */
  bfd_vma tbase;
};

const struct objfile_data *dwarf2_frame_objfile_data;

static unsigned int
read_1_byte (bfd *abfd, gdb_byte *buf)
{
  return bfd_get_8 (abfd, buf);
}

static unsigned int
read_4_bytes (bfd *abfd, gdb_byte *buf)
{
  return bfd_get_32 (abfd, buf);
}

static ULONGEST
read_8_bytes (bfd *abfd, gdb_byte *buf)
{
  return bfd_get_64 (abfd, buf);
}

static ULONGEST
read_unsigned_leb128 (bfd *abfd, gdb_byte *buf, unsigned int *bytes_read_ptr)
{
  ULONGEST result;
  unsigned int num_read;
  int shift;
  gdb_byte byte;

  result = 0;
  shift = 0;
  num_read = 0;

  do
    {
      byte = bfd_get_8 (abfd, (bfd_byte *) buf);
      buf++;
      num_read++;
      result |= ((byte & 0x7f) << shift);
      shift += 7;
    }
  while (byte & 0x80);

  *bytes_read_ptr = num_read;

  return result;
}

static LONGEST
read_signed_leb128 (bfd *abfd, gdb_byte *buf, unsigned int *bytes_read_ptr)
{
  LONGEST result;
  int shift;
  unsigned int num_read;
  gdb_byte byte;

  result = 0;
  shift = 0;
  num_read = 0;

  do
    {
      byte = bfd_get_8 (abfd, (bfd_byte *) buf);
      buf++;
      num_read++;
      result |= ((byte & 0x7f) << shift);
      shift += 7;
    }
  while (byte & 0x80);

  if (shift < 8 * sizeof (result) && (byte & 0x40))
    result |= -(((LONGEST)1) << shift);

  *bytes_read_ptr = num_read;

  return result;
}

static ULONGEST
read_initial_length (bfd *abfd, gdb_byte *buf, unsigned int *bytes_read_ptr)
{
  LONGEST result;

  result = bfd_get_32 (abfd, buf);
  if (result == 0xffffffff)
    {
      result = bfd_get_64 (abfd, buf + 4);
      *bytes_read_ptr = 12;
    }
  else
    *bytes_read_ptr = 4;

  return result;
}


/* Pointer encoding helper functions.  */

/* GCC supports exception handling based on DWARF2 CFI.  However, for
   technical reasons, it encodes addresses in its FDE's in a different
   way.  Several "pointer encodings" are supported.  The encoding
   that's used for a particular FDE is determined by the 'R'
   augmentation in the associated CIE.  The argument of this
   augmentation is a single byte.  

   The address can be encoded as 2 bytes, 4 bytes, 8 bytes, or as a
   LEB128.  This is encoded in bits 0, 1 and 2.  Bit 3 encodes whether
   the address is signed or unsigned.  Bits 4, 5 and 6 encode how the
   address should be interpreted (absolute, relative to the current
   position in the FDE, ...).  Bit 7, indicates that the address
   should be dereferenced.  */

static gdb_byte
encoding_for_size (unsigned int size)
{
  switch (size)
    {
    case 2:
      return DW_EH_PE_udata2;
    case 4:
      return DW_EH_PE_udata4;
    case 8:
      return DW_EH_PE_udata8;
    default:
      internal_error (__FILE__, __LINE__, _("Unsupported address size"));
    }
}

static unsigned int
size_of_encoded_value (gdb_byte encoding)
{
  if (encoding == DW_EH_PE_omit)
    return 0;

  switch (encoding & 0x07)
    {
    case DW_EH_PE_absptr:
      return TYPE_LENGTH (builtin_type_void_data_ptr);
    case DW_EH_PE_udata2:
      return 2;
    case DW_EH_PE_udata4:
      return 4;
    case DW_EH_PE_udata8:
      return 8;
    default:
      internal_error (__FILE__, __LINE__, _("Invalid or unsupported encoding"));
    }
}

static CORE_ADDR
read_encoded_value (struct comp_unit *unit, gdb_byte encoding,
		    gdb_byte *buf, unsigned int *bytes_read_ptr)
{
  int ptr_len = size_of_encoded_value (DW_EH_PE_absptr);
  ptrdiff_t offset;
  CORE_ADDR base;

  /* GCC currently doesn't generate DW_EH_PE_indirect encodings for
     FDE's.  */
  if (encoding & DW_EH_PE_indirect)
    internal_error (__FILE__, __LINE__, 
		    _("Unsupported encoding: DW_EH_PE_indirect"));

  *bytes_read_ptr = 0;

  switch (encoding & 0x70)
    {
    case DW_EH_PE_absptr:
      base = 0;
      break;
    case DW_EH_PE_pcrel:
      base = bfd_get_section_vma (unit->abfd, unit->dwarf_frame_section);
      base += (buf - unit->dwarf_frame_buffer);
      break;
    case DW_EH_PE_datarel:
      base = unit->dbase;
      break;
    case DW_EH_PE_textrel:
      base = unit->tbase;
      break;
    case DW_EH_PE_funcrel:
      /* FIXME: kettenis/20040501: For now just pretend
         DW_EH_PE_funcrel is equivalent to DW_EH_PE_absptr.  For
         reading the initial location of an FDE it should be treated
         as such, and currently that's the only place where this code
         is used.  */
      base = 0;
      break;
    case DW_EH_PE_aligned:
      base = 0;
      offset = buf - unit->dwarf_frame_buffer;
      if ((offset % ptr_len) != 0)
	{
	  *bytes_read_ptr = ptr_len - (offset % ptr_len);
	  buf += *bytes_read_ptr;
	}
      break;
    default:
      internal_error (__FILE__, __LINE__, _("Invalid or unsupported encoding"));
    }

  if ((encoding & 0x07) == 0x00)
    {
      encoding |= encoding_for_size (ptr_len);
      if (bfd_get_sign_extend_vma (unit->abfd))
	encoding |= DW_EH_PE_signed;
    }

  switch (encoding & 0x0f)
    {
    case DW_EH_PE_uleb128:
      {
	ULONGEST value;
	gdb_byte *end_buf = buf + (sizeof (value) + 1) * 8 / 7;
	*bytes_read_ptr += read_uleb128 (buf, end_buf, &value) - buf;
	return base + value;
      }
    case DW_EH_PE_udata2:
      *bytes_read_ptr += 2;
      return (base + bfd_get_16 (unit->abfd, (bfd_byte *) buf));
    case DW_EH_PE_udata4:
      *bytes_read_ptr += 4;
      return (base + bfd_get_32 (unit->abfd, (bfd_byte *) buf));
    case DW_EH_PE_udata8:
      *bytes_read_ptr += 8;
      return (base + bfd_get_64 (unit->abfd, (bfd_byte *) buf));
    case DW_EH_PE_sleb128:
      {
	LONGEST value;
	gdb_byte *end_buf = buf + (sizeof (value) + 1) * 8 / 7;
	*bytes_read_ptr += read_sleb128 (buf, end_buf, &value) - buf;
	return base + value;
      }
    case DW_EH_PE_sdata2:
      *bytes_read_ptr += 2;
      return (base + bfd_get_signed_16 (unit->abfd, (bfd_byte *) buf));
    case DW_EH_PE_sdata4:
      *bytes_read_ptr += 4;
      return (base + bfd_get_signed_32 (unit->abfd, (bfd_byte *) buf));
    case DW_EH_PE_sdata8:
      *bytes_read_ptr += 8;
      return (base + bfd_get_signed_64 (unit->abfd, (bfd_byte *) buf));
    default:
      internal_error (__FILE__, __LINE__, _("Invalid or unsupported encoding"));
    }
}


/* GCC uses a single CIE for all FDEs in a .debug_frame section.
   That's why we use a simple linked list here.  */

static struct dwarf2_cie *
find_cie (struct comp_unit *unit, ULONGEST cie_pointer)
{
  struct dwarf2_cie *cie = unit->cie;

  while (cie)
    {
      if (cie->cie_pointer == cie_pointer)
	return cie;

      cie = cie->next;
    }

  return NULL;
}

static void
add_cie (struct comp_unit *unit, struct dwarf2_cie *cie)
{
  cie->next = unit->cie;
  unit->cie = cie;
}

/* Find the FDE for *PC.  Return a pointer to the FDE, and store the
   inital location associated with it into *PC.  */

static struct dwarf2_fde *
dwarf2_frame_find_fde (CORE_ADDR *pc)
{
  struct objfile *objfile;

  ALL_OBJFILES (objfile)
    {
      struct dwarf2_fde *fde;
      CORE_ADDR offset;

      fde = objfile_data (objfile, dwarf2_frame_objfile_data);
      if (fde == NULL)
	continue;

      gdb_assert (objfile->section_offsets);
      offset = objfile_text_section_offset (objfile);

      while (fde)
	{
	  if (*pc >= fde->initial_location + offset
	      && *pc < fde->initial_location + offset + fde->address_range)
	    {
	      *pc = fde->initial_location + offset;
	      return fde;
	    }

	  fde = fde->next;
	}
    }

  return NULL;
}

static void
add_fde (struct comp_unit *unit, struct dwarf2_fde *fde)
{
  fde->next = objfile_data (unit->objfile, dwarf2_frame_objfile_data);
  set_objfile_data (unit->objfile, dwarf2_frame_objfile_data, fde);
}

#ifdef CC_HAS_LONG_LONG
#define DW64_CIE_ID 0xffffffffffffffffULL
#else
#define DW64_CIE_ID ~0
#endif

static gdb_byte *decode_frame_entry (struct comp_unit *unit, gdb_byte *start,
				     int eh_frame_p);

/* Decode the next CIE or FDE.  Return NULL if invalid input, otherwise
   the next byte to be processed.  */
static gdb_byte *
decode_frame_entry_1 (struct comp_unit *unit, gdb_byte *start, int eh_frame_p)
{
  gdb_byte *buf, *end;
  LONGEST length;
  unsigned int bytes_read;
  int dwarf64_p;
  ULONGEST cie_id;
  ULONGEST cie_pointer;

  buf = start;
  length = read_initial_length (unit->abfd, buf, &bytes_read);
  buf += bytes_read;
  end = buf + length;

  /* Are we still within the section? */
  if (end > unit->dwarf_frame_buffer + unit->dwarf_frame_size)
    return NULL;

  if (length == 0)
    return end;

  /* Distinguish between 32 and 64-bit encoded frame info.  */
  dwarf64_p = (bytes_read == 12);

  /* In a .eh_frame section, zero is used to distinguish CIEs from FDEs.  */
  if (eh_frame_p)
    cie_id = 0;
  else if (dwarf64_p)
    cie_id = DW64_CIE_ID;
  else
    cie_id = DW_CIE_ID;

  if (dwarf64_p)
    {
      cie_pointer = read_8_bytes (unit->abfd, buf);
      buf += 8;
    }
  else
    {
      cie_pointer = read_4_bytes (unit->abfd, buf);
      buf += 4;
    }

  if (cie_pointer == cie_id)
    {
      /* This is a CIE.  */
      struct dwarf2_cie *cie;
      char *augmentation;
      unsigned int cie_version;

      /* Record the offset into the .debug_frame section of this CIE.  */
      cie_pointer = start - unit->dwarf_frame_buffer;

      /* Check whether we've already read it.  */
      if (find_cie (unit, cie_pointer))
	return end;

      cie = (struct dwarf2_cie *)
	obstack_alloc (&unit->objfile->objfile_obstack,
		       sizeof (struct dwarf2_cie));
      cie->initial_instructions = NULL;
      cie->cie_pointer = cie_pointer;

      /* The encoding for FDE's in a normal .debug_frame section
         depends on the target address size.  */
      cie->encoding = DW_EH_PE_absptr;

      /* We'll determine the final value later, but we need to
	 initialize it conservatively.  */
      cie->signal_frame = 0;

      /* Check version number.  */
      cie_version = read_1_byte (unit->abfd, buf);
      if (cie_version != 1 && cie_version != 3)
	return NULL;
      cie->version = cie_version;
      buf += 1;

      /* Interpret the interesting bits of the augmentation.  */
      cie->augmentation = augmentation = (char *) buf;
      buf += (strlen (augmentation) + 1);

      /* Ignore armcc augmentations.  We only use them for quirks,
	 and that doesn't happen until later.  */
      if (strncmp (augmentation, "armcc", 5) == 0)
	augmentation += strlen (augmentation);

      /* The GCC 2.x "eh" augmentation has a pointer immediately
         following the augmentation string, so it must be handled
         first.  */
      if (augmentation[0] == 'e' && augmentation[1] == 'h')
	{
	  /* Skip.  */
	  buf += TYPE_LENGTH (builtin_type_void_data_ptr);
	  augmentation += 2;
	}

      cie->code_alignment_factor =
	read_unsigned_leb128 (unit->abfd, buf, &bytes_read);
      buf += bytes_read;

      cie->data_alignment_factor =
	read_signed_leb128 (unit->abfd, buf, &bytes_read);
      buf += bytes_read;

      if (cie_version == 1)
	{
	  cie->return_address_register = read_1_byte (unit->abfd, buf);
	  bytes_read = 1;
	}
      else
	cie->return_address_register = read_unsigned_leb128 (unit->abfd, buf,
							     &bytes_read);
      cie->return_address_register
	= dwarf2_frame_adjust_regnum (current_gdbarch,
				      cie->return_address_register,
				      eh_frame_p);

      buf += bytes_read;

      cie->saw_z_augmentation = (*augmentation == 'z');
      if (cie->saw_z_augmentation)
	{
	  ULONGEST length;

	  length = read_unsigned_leb128 (unit->abfd, buf, &bytes_read);
	  buf += bytes_read;
	  if (buf > end)
	    return NULL;
	  cie->initial_instructions = buf + length;
	  augmentation++;
	}

      while (*augmentation)
	{
	  /* "L" indicates a byte showing how the LSDA pointer is encoded.  */
	  if (*augmentation == 'L')
	    {
	      /* Skip.  */
	      buf++;
	      augmentation++;
	    }

	  /* "R" indicates a byte indicating how FDE addresses are encoded.  */
	  else if (*augmentation == 'R')
	    {
	      cie->encoding = *buf++;
	      augmentation++;
	    }

	  /* "P" indicates a personality routine in the CIE augmentation.  */
	  else if (*augmentation == 'P')
	    {
	      /* Skip.  Avoid indirection since we throw away the result.  */
	      gdb_byte encoding = (*buf++) & ~DW_EH_PE_indirect;
	      read_encoded_value (unit, encoding, buf, &bytes_read);
	      buf += bytes_read;
	      augmentation++;
	    }

	  /* "S" indicates a signal frame, such that the return
	     address must not be decremented to locate the call frame
	     info for the previous frame; it might even be the first
	     instruction of a function, so decrementing it would take
	     us to a different function.  */
	  else if (*augmentation == 'S')
	    {
	      cie->signal_frame = 1;
	      augmentation++;
	    }

	  /* Otherwise we have an unknown augmentation.  Assume that either
	     there is no augmentation data, or we saw a 'z' prefix.  */
	  else
	    {
	      if (cie->initial_instructions)
		buf = cie->initial_instructions;
	      break;
	    }
	}

      cie->initial_instructions = buf;
      cie->end = end;

      add_cie (unit, cie);
    }
  else
    {
      /* This is a FDE.  */
      struct dwarf2_fde *fde;

      /* In an .eh_frame section, the CIE pointer is the delta between the
	 address within the FDE where the CIE pointer is stored and the
	 address of the CIE.  Convert it to an offset into the .eh_frame
	 section.  */
      if (eh_frame_p)
	{
	  cie_pointer = buf - unit->dwarf_frame_buffer - cie_pointer;
	  cie_pointer -= (dwarf64_p ? 8 : 4);
	}

      /* In either case, validate the result is still within the section.  */
      if (cie_pointer >= unit->dwarf_frame_size)
	return NULL;

      fde = (struct dwarf2_fde *)
	obstack_alloc (&unit->objfile->objfile_obstack,
		       sizeof (struct dwarf2_fde));
      fde->cie = find_cie (unit, cie_pointer);
      if (fde->cie == NULL)
	{
	  decode_frame_entry (unit, unit->dwarf_frame_buffer + cie_pointer,
			      eh_frame_p);
	  fde->cie = find_cie (unit, cie_pointer);
	}

      gdb_assert (fde->cie != NULL);

      fde->initial_location =
	read_encoded_value (unit, fde->cie->encoding, buf, &bytes_read);
      buf += bytes_read;

      fde->address_range =
	read_encoded_value (unit, fde->cie->encoding & 0x0f, buf, &bytes_read);
      buf += bytes_read;

      /* A 'z' augmentation in the CIE implies the presence of an
	 augmentation field in the FDE as well.  The only thing known
	 to be in here at present is the LSDA entry for EH.  So we
	 can skip the whole thing.  */
      if (fde->cie->saw_z_augmentation)
	{
	  ULONGEST length;

	  length = read_unsigned_leb128 (unit->abfd, buf, &bytes_read);
	  buf += bytes_read + length;
	  if (buf > end)
	    return NULL;
	}

      fde->instructions = buf;
      fde->end = end;

      fde->eh_frame_p = eh_frame_p;

      add_fde (unit, fde);
    }

  return end;
}

/* Read a CIE or FDE in BUF and decode it.  */
static gdb_byte *
decode_frame_entry (struct comp_unit *unit, gdb_byte *start, int eh_frame_p)
{
  enum { NONE, ALIGN4, ALIGN8, FAIL } workaround = NONE;
  gdb_byte *ret;
  ptrdiff_t start_offset;

  while (1)
    {
      ret = decode_frame_entry_1 (unit, start, eh_frame_p);
      if (ret != NULL)
	break;

      /* We have corrupt input data of some form.  */

      /* ??? Try, weakly, to work around compiler/assembler/linker bugs
	 and mismatches wrt padding and alignment of debug sections.  */
      /* Note that there is no requirement in the standard for any
	 alignment at all in the frame unwind sections.  Testing for
	 alignment before trying to interpret data would be incorrect.

	 However, GCC traditionally arranged for frame sections to be
	 sized such that the FDE length and CIE fields happen to be
	 aligned (in theory, for performance).  This, unfortunately,
	 was done with .align directives, which had the side effect of
	 forcing the section to be aligned by the linker.

	 This becomes a problem when you have some other producer that
	 creates frame sections that are not as strictly aligned.  That
	 produces a hole in the frame info that gets filled by the 
	 linker with zeros.

	 The GCC behaviour is arguably a bug, but it's effectively now
	 part of the ABI, so we're now stuck with it, at least at the
	 object file level.  A smart linker may decide, in the process
	 of compressing duplicate CIE information, that it can rewrite
	 the entire output section without this extra padding.  */

      start_offset = start - unit->dwarf_frame_buffer;
      if (workaround < ALIGN4 && (start_offset & 3) != 0)
	{
	  start += 4 - (start_offset & 3);
	  workaround = ALIGN4;
	  continue;
	}
      if (workaround < ALIGN8 && (start_offset & 7) != 0)
	{
	  start += 8 - (start_offset & 7);
	  workaround = ALIGN8;
	  continue;
	}

      /* Nothing left to try.  Arrange to return as if we've consumed
	 the entire input section.  Hopefully we'll get valid info from
	 the other of .debug_frame/.eh_frame.  */
      workaround = FAIL;
      ret = unit->dwarf_frame_buffer + unit->dwarf_frame_size;
      break;
    }

  switch (workaround)
    {
    case NONE:
      break;

    case ALIGN4:
      complaint (&symfile_complaints,
		 _("Corrupt data in %s:%s; align 4 workaround apparently succeeded"),
		 unit->dwarf_frame_section->owner->filename,
		 unit->dwarf_frame_section->name);
      break;

    case ALIGN8:
      complaint (&symfile_complaints,
		 _("Corrupt data in %s:%s; align 8 workaround apparently succeeded"),
		 unit->dwarf_frame_section->owner->filename,
		 unit->dwarf_frame_section->name);
      break;

    default:
      complaint (&symfile_complaints,
		 _("Corrupt data in %s:%s"),
		 unit->dwarf_frame_section->owner->filename,
		 unit->dwarf_frame_section->name);
      break;
    }

  return ret;
}


/* FIXME: kettenis/20030504: This still needs to be integrated with
   dwarf2read.c in a better way.  */

/* Imported from dwarf2read.c.  */
extern asection *dwarf_frame_section;
extern asection *dwarf_eh_frame_section;

/* Imported from dwarf2read.c.  */
void
dwarf2_build_frame_info (struct objfile *objfile)
{
  struct comp_unit unit;
  gdb_byte *frame_ptr;

  /* Build a minimal decoding of the DWARF2 compilation unit.  */
  unit.abfd = objfile->obfd;
  unit.objfile = objfile;
  unit.dbase = 0;
  unit.tbase = 0;

  /* First add the information from the .eh_frame section.  That way,
     the FDEs from that section are searched last.  */
  if (dwarf_eh_frame_section)
    {
      asection *got, *txt;

      unit.cie = NULL;
      unit.dwarf_frame_buffer = dwarf2_read_section (objfile,  objfile->obfd,
						     dwarf_eh_frame_section);

      unit.dwarf_frame_size = bfd_get_section_size (dwarf_eh_frame_section);
      unit.dwarf_frame_section = dwarf_eh_frame_section;

      /* FIXME: kettenis/20030602: This is the DW_EH_PE_datarel base
	 that is used for the i386/amd64 target, which currently is
	 the only target in GCC that supports/uses the
	 DW_EH_PE_datarel encoding.  */
      got = bfd_get_section_by_name (unit.abfd, ".got");
      if (got)
	unit.dbase = got->vma;

      /* GCC emits the DW_EH_PE_textrel encoding type on sh and ia64
         so far.  */
      txt = bfd_get_section_by_name (unit.abfd, ".text");
      if (txt)
	unit.tbase = txt->vma;

      frame_ptr = unit.dwarf_frame_buffer;
      while (frame_ptr < unit.dwarf_frame_buffer + unit.dwarf_frame_size)
	frame_ptr = decode_frame_entry (&unit, frame_ptr, 1);
    }

  if (dwarf_frame_section)
    {
      unit.cie = NULL;
      unit.dwarf_frame_buffer = dwarf2_read_section (objfile,  objfile->obfd,
						     dwarf_frame_section);
      unit.dwarf_frame_size = bfd_get_section_size (dwarf_frame_section);
      unit.dwarf_frame_section = dwarf_frame_section;

      frame_ptr = unit.dwarf_frame_buffer;
      while (frame_ptr < unit.dwarf_frame_buffer + unit.dwarf_frame_size)
	frame_ptr = decode_frame_entry (&unit, frame_ptr, 0);
    }
}

/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_dwarf2_frame (void);

void
_initialize_dwarf2_frame (void)
{
  dwarf2_frame_data = gdbarch_data_register_pre_init (dwarf2_frame_init);
  dwarf2_frame_objfile_data = register_objfile_data ();
}

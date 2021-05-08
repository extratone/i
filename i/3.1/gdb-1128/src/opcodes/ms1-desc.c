/* CPU data for ms1.

THIS FILE IS MACHINE GENERATED WITH CGEN.

Copyright 1996-2005 Free Software Foundation, Inc.

This file is part of the GNU Binutils and/or GDB, the GNU debugger.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.

*/

#include "sysdep.h"
#include <stdio.h>
#include <stdarg.h>
#include "ansidecl.h"
#include "bfd.h"
#include "symcat.h"
#include "ms1-desc.h"
#include "ms1-opc.h"
#include "opintl.h"
#include "libiberty.h"
#include "xregex.h"

/* Attributes.  */

static const CGEN_ATTR_ENTRY bool_attr[] =
{
  { "#f", 0 },
  { "#t", 1 },
  { 0, 0 }
};

static const CGEN_ATTR_ENTRY MACH_attr[] ATTRIBUTE_UNUSED =
{
  { "base", MACH_BASE },
  { "ms1", MACH_MS1 },
  { "ms1_003", MACH_MS1_003 },
  { "max", MACH_MAX },
  { 0, 0 }
};

static const CGEN_ATTR_ENTRY ISA_attr[] ATTRIBUTE_UNUSED =
{
  { "ms1", ISA_MS1 },
  { "max", ISA_MAX },
  { 0, 0 }
};

const CGEN_ATTR_TABLE ms1_cgen_ifield_attr_table[] =
{
  { "MACH", & MACH_attr[0], & MACH_attr[0] },
  { "VIRTUAL", &bool_attr[0], &bool_attr[0] },
  { "PCREL-ADDR", &bool_attr[0], &bool_attr[0] },
  { "ABS-ADDR", &bool_attr[0], &bool_attr[0] },
  { "RESERVED", &bool_attr[0], &bool_attr[0] },
  { "SIGN-OPT", &bool_attr[0], &bool_attr[0] },
  { "SIGNED", &bool_attr[0], &bool_attr[0] },
  { 0, 0, 0 }
};

const CGEN_ATTR_TABLE ms1_cgen_hardware_attr_table[] =
{
  { "MACH", & MACH_attr[0], & MACH_attr[0] },
  { "VIRTUAL", &bool_attr[0], &bool_attr[0] },
  { "CACHE-ADDR", &bool_attr[0], &bool_attr[0] },
  { "PC", &bool_attr[0], &bool_attr[0] },
  { "PROFILE", &bool_attr[0], &bool_attr[0] },
  { 0, 0, 0 }
};

const CGEN_ATTR_TABLE ms1_cgen_operand_attr_table[] =
{
  { "MACH", & MACH_attr[0], & MACH_attr[0] },
  { "VIRTUAL", &bool_attr[0], &bool_attr[0] },
  { "PCREL-ADDR", &bool_attr[0], &bool_attr[0] },
  { "ABS-ADDR", &bool_attr[0], &bool_attr[0] },
  { "SIGN-OPT", &bool_attr[0], &bool_attr[0] },
  { "SIGNED", &bool_attr[0], &bool_attr[0] },
  { "NEGATIVE", &bool_attr[0], &bool_attr[0] },
  { "RELAX", &bool_attr[0], &bool_attr[0] },
  { "SEM-ONLY", &bool_attr[0], &bool_attr[0] },
  { 0, 0, 0 }
};

const CGEN_ATTR_TABLE ms1_cgen_insn_attr_table[] =
{
  { "MACH", & MACH_attr[0], & MACH_attr[0] },
  { "ALIAS", &bool_attr[0], &bool_attr[0] },
  { "VIRTUAL", &bool_attr[0], &bool_attr[0] },
  { "UNCOND-CTI", &bool_attr[0], &bool_attr[0] },
  { "COND-CTI", &bool_attr[0], &bool_attr[0] },
  { "SKIP-CTI", &bool_attr[0], &bool_attr[0] },
  { "DELAY-SLOT", &bool_attr[0], &bool_attr[0] },
  { "RELAXABLE", &bool_attr[0], &bool_attr[0] },
  { "RELAXED", &bool_attr[0], &bool_attr[0] },
  { "NO-DIS", &bool_attr[0], &bool_attr[0] },
  { "PBB", &bool_attr[0], &bool_attr[0] },
  { "LOAD-DELAY", &bool_attr[0], &bool_attr[0] },
  { "MEMORY-ACCESS", &bool_attr[0], &bool_attr[0] },
  { "AL-INSN", &bool_attr[0], &bool_attr[0] },
  { "IO-INSN", &bool_attr[0], &bool_attr[0] },
  { "BR-INSN", &bool_attr[0], &bool_attr[0] },
  { "USES-FRDR", &bool_attr[0], &bool_attr[0] },
  { "USES-FRDRRR", &bool_attr[0], &bool_attr[0] },
  { "USES-FRSR1", &bool_attr[0], &bool_attr[0] },
  { "USES-FRSR2", &bool_attr[0], &bool_attr[0] },
  { "SKIPA", &bool_attr[0], &bool_attr[0] },
  { 0, 0, 0 }
};

/* Instruction set variants.  */

static const CGEN_ISA ms1_cgen_isa_table[] = {
  { "ms1", 32, 32, 32, 32 },
  { 0, 0, 0, 0, 0 }
};

/* Machine variants.  */

static const CGEN_MACH ms1_cgen_mach_table[] = {
  { "ms1", "ms1", MACH_MS1, 0 },
  { "ms1-003", "ms1-003", MACH_MS1_003, 0 },
  { 0, 0, 0, 0 }
};

static CGEN_KEYWORD_ENTRY ms1_cgen_opval_msys_syms_entries[] =
{
  { "DUP", 1, {0, {0}}, 0, 0 },
  { "XX", 0, {0, {0}}, 0, 0 }
};

CGEN_KEYWORD ms1_cgen_opval_msys_syms =
{
  & ms1_cgen_opval_msys_syms_entries[0],
  2,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY ms1_cgen_opval_h_spr_entries[] =
{
  { "R0", 0, {0, {0}}, 0, 0 },
  { "R1", 1, {0, {0}}, 0, 0 },
  { "R2", 2, {0, {0}}, 0, 0 },
  { "R3", 3, {0, {0}}, 0, 0 },
  { "R4", 4, {0, {0}}, 0, 0 },
  { "R5", 5, {0, {0}}, 0, 0 },
  { "R6", 6, {0, {0}}, 0, 0 },
  { "R7", 7, {0, {0}}, 0, 0 },
  { "R8", 8, {0, {0}}, 0, 0 },
  { "R9", 9, {0, {0}}, 0, 0 },
  { "R10", 10, {0, {0}}, 0, 0 },
  { "R11", 11, {0, {0}}, 0, 0 },
  { "R12", 12, {0, {0}}, 0, 0 },
  { "fp", 12, {0, {0}}, 0, 0 },
  { "R13", 13, {0, {0}}, 0, 0 },
  { "sp", 13, {0, {0}}, 0, 0 },
  { "R14", 14, {0, {0}}, 0, 0 },
  { "ra", 14, {0, {0}}, 0, 0 },
  { "R15", 15, {0, {0}}, 0, 0 },
  { "ira", 15, {0, {0}}, 0, 0 }
};

CGEN_KEYWORD ms1_cgen_opval_h_spr =
{
  & ms1_cgen_opval_h_spr_entries[0],
  20,
  0, 0, 0, 0, ""
};


/* The hardware table.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_HW_##a)
#else
#define A(a) (1 << CGEN_HW_/**/a)
#endif

const CGEN_HW_ENTRY ms1_cgen_hw_table[] =
{
  { "h-memory", HW_H_MEMORY, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-sint", HW_H_SINT, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-uint", HW_H_UINT, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-addr", HW_H_ADDR, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-iaddr", HW_H_IADDR, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-spr", HW_H_SPR, CGEN_ASM_KEYWORD, (PTR) & ms1_cgen_opval_h_spr, { 0, { (1<<MACH_BASE) } } },
  { "h-pc", HW_H_PC, CGEN_ASM_NONE, 0, { 0|A(PROFILE)|A(PC), { (1<<MACH_BASE) } } },
  { 0, 0, CGEN_ASM_NONE, 0, {0, {0}} }
};

#undef A


/* The instruction field table.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_IFLD_##a)
#else
#define A(a) (1 << CGEN_IFLD_/**/a)
#endif

const CGEN_IFLD ms1_cgen_ifld_table[] =
{
  { MS1_F_NIL, "f-nil", 0, 0, 0, 0, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_ANYOF, "f-anyof", 0, 0, 0, 0, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_MSYS, "f-msys", 0, 32, 31, 1, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_OPC, "f-opc", 0, 32, 30, 6, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_IMM, "f-imm", 0, 32, 24, 1, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_UU24, "f-uu24", 0, 32, 23, 24, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_SR1, "f-sr1", 0, 32, 23, 4, { 0|A(ABS_ADDR), { (1<<MACH_BASE) } }  },
  { MS1_F_SR2, "f-sr2", 0, 32, 19, 4, { 0|A(ABS_ADDR), { (1<<MACH_BASE) } }  },
  { MS1_F_DR, "f-dr", 0, 32, 19, 4, { 0|A(ABS_ADDR), { (1<<MACH_BASE) } }  },
  { MS1_F_DRRR, "f-drrr", 0, 32, 15, 4, { 0|A(ABS_ADDR), { (1<<MACH_BASE) } }  },
  { MS1_F_IMM16U, "f-imm16u", 0, 32, 15, 16, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_IMM16S, "f-imm16s", 0, 32, 15, 16, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_IMM16A, "f-imm16a", 0, 32, 15, 16, { 0|A(PCREL_ADDR), { (1<<MACH_BASE) } }  },
  { MS1_F_UU4A, "f-uu4a", 0, 32, 19, 4, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_UU4B, "f-uu4b", 0, 32, 23, 4, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_UU12, "f-uu12", 0, 32, 11, 12, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_UU16, "f-uu16", 0, 32, 15, 16, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_MSOPC, "f-msopc", 0, 32, 30, 5, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_UU_26_25, "f-uu-26-25", 0, 32, 25, 26, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_MASK, "f-mask", 0, 32, 25, 16, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_BANKADDR, "f-bankaddr", 0, 32, 25, 13, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_RDA, "f-rda", 0, 32, 25, 1, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_UU_2_25, "f-uu-2-25", 0, 32, 25, 2, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_RBBC, "f-rbbc", 0, 32, 25, 2, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_PERM, "f-perm", 0, 32, 25, 2, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_MODE, "f-mode", 0, 32, 25, 2, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_UU_1_24, "f-uu-1-24", 0, 32, 24, 1, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_WR, "f-wr", 0, 32, 24, 1, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_FBINCR, "f-fbincr", 0, 32, 23, 4, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_UU_2_23, "f-uu-2-23", 0, 32, 23, 2, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_XMODE, "f-xmode", 0, 32, 23, 1, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_A23, "f-a23", 0, 32, 23, 1, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_MASK1, "f-mask1", 0, 32, 22, 3, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_CR, "f-cr", 0, 32, 22, 3, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_TYPE, "f-type", 0, 32, 21, 2, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_INCAMT, "f-incamt", 0, 32, 19, 8, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_CBS, "f-cbs", 0, 32, 19, 2, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_UU_1_19, "f-uu-1-19", 0, 32, 19, 1, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_BALL, "f-ball", 0, 32, 19, 1, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_COLNUM, "f-colnum", 0, 32, 18, 3, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_BRC, "f-brc", 0, 32, 18, 3, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_INCR, "f-incr", 0, 32, 17, 6, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_FBDISP, "f-fbdisp", 0, 32, 15, 6, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_UU_4_15, "f-uu-4-15", 0, 32, 15, 4, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_LENGTH, "f-length", 0, 32, 15, 3, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_UU_1_15, "f-uu-1-15", 0, 32, 15, 1, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_RC, "f-rc", 0, 32, 15, 1, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_RCNUM, "f-rcnum", 0, 32, 14, 3, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_ROWNUM, "f-rownum", 0, 32, 14, 3, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_CBX, "f-cbx", 0, 32, 14, 3, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_ID, "f-id", 0, 32, 14, 1, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_SIZE, "f-size", 0, 32, 13, 14, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_ROWNUM1, "f-rownum1", 0, 32, 12, 3, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_UU_3_11, "f-uu-3-11", 0, 32, 11, 3, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_RC1, "f-rc1", 0, 32, 11, 1, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_CCB, "f-ccb", 0, 32, 11, 1, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_CBRB, "f-cbrb", 0, 32, 10, 1, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_CDB, "f-cdb", 0, 32, 10, 1, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_ROWNUM2, "f-rownum2", 0, 32, 9, 3, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_CELL, "f-cell", 0, 32, 9, 3, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_UU_3_9, "f-uu-3-9", 0, 32, 9, 3, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_CONTNUM, "f-contnum", 0, 32, 8, 9, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_UU_1_6, "f-uu-1-6", 0, 32, 6, 1, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_DUP, "f-dup", 0, 32, 6, 1, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_RC2, "f-rc2", 0, 32, 6, 1, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_CTXDISP, "f-ctxdisp", 0, 32, 5, 6, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_MSYSFRSR2, "f-msysfrsr2", 0, 32, 19, 4, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_BRC2, "f-brc2", 0, 32, 14, 3, { 0, { (1<<MACH_BASE) } }  },
  { MS1_F_BALL2, "f-ball2", 0, 32, 15, 1, { 0, { (1<<MACH_BASE) } }  },
  { 0, 0, 0, 0, 0, 0, {0, {0}} }
};

#undef A



/* multi ifield declarations */



/* multi ifield definitions */


/* The operand table.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_OPERAND_##a)
#else
#define A(a) (1 << CGEN_OPERAND_/**/a)
#endif
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define OPERAND(op) MS1_OPERAND_##op
#else
#define OPERAND(op) MS1_OPERAND_/**/op
#endif

const CGEN_OPERAND ms1_cgen_operand_table[] =
{
/* pc: program counter */
  { "pc", MS1_OPERAND_PC, HW_H_PC, 0, 0,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_NIL] } }, 
    { 0|A(SEM_ONLY), { (1<<MACH_BASE) } }  },
/* frsr1: register */
  { "frsr1", MS1_OPERAND_FRSR1, HW_H_SPR, 23, 4,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_SR1] } }, 
    { 0|A(ABS_ADDR), { (1<<MACH_BASE) } }  },
/* frsr2: register */
  { "frsr2", MS1_OPERAND_FRSR2, HW_H_SPR, 19, 4,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_SR2] } }, 
    { 0|A(ABS_ADDR), { (1<<MACH_BASE) } }  },
/* frdr: register */
  { "frdr", MS1_OPERAND_FRDR, HW_H_SPR, 19, 4,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_DR] } }, 
    { 0|A(ABS_ADDR), { (1<<MACH_BASE) } }  },
/* frdrrr: register */
  { "frdrrr", MS1_OPERAND_FRDRRR, HW_H_SPR, 15, 4,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_DRRR] } }, 
    { 0|A(ABS_ADDR), { (1<<MACH_BASE) } }  },
/* imm16: immediate value - sign extd */
  { "imm16", MS1_OPERAND_IMM16, HW_H_SINT, 15, 16,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_IMM16S] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* imm16z: immediate value - zero extd */
  { "imm16z", MS1_OPERAND_IMM16Z, HW_H_UINT, 15, 16,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_IMM16U] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* imm16o: immediate value */
  { "imm16o", MS1_OPERAND_IMM16O, HW_H_UINT, 15, 16,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_IMM16S] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* rc: rc */
  { "rc", MS1_OPERAND_RC, HW_H_UINT, 15, 1,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_RC] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* rcnum: rcnum */
  { "rcnum", MS1_OPERAND_RCNUM, HW_H_UINT, 14, 3,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_RCNUM] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* contnum: context number */
  { "contnum", MS1_OPERAND_CONTNUM, HW_H_UINT, 8, 9,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_CONTNUM] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* rbbc: omega network configuration */
  { "rbbc", MS1_OPERAND_RBBC, HW_H_UINT, 25, 2,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_RBBC] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* colnum: column number */
  { "colnum", MS1_OPERAND_COLNUM, HW_H_UINT, 18, 3,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_COLNUM] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* rownum: row number */
  { "rownum", MS1_OPERAND_ROWNUM, HW_H_UINT, 14, 3,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_ROWNUM] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* rownum1: row number */
  { "rownum1", MS1_OPERAND_ROWNUM1, HW_H_UINT, 12, 3,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_ROWNUM1] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* rownum2: row number */
  { "rownum2", MS1_OPERAND_ROWNUM2, HW_H_UINT, 9, 3,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_ROWNUM2] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* rc1: rc1 */
  { "rc1", MS1_OPERAND_RC1, HW_H_UINT, 11, 1,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_RC1] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* rc2: rc2 */
  { "rc2", MS1_OPERAND_RC2, HW_H_UINT, 6, 1,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_RC2] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* cbrb: data-bus orientation */
  { "cbrb", MS1_OPERAND_CBRB, HW_H_UINT, 10, 1,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_CBRB] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* cell: cell */
  { "cell", MS1_OPERAND_CELL, HW_H_UINT, 9, 3,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_CELL] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* dup: dup */
  { "dup", MS1_OPERAND_DUP, HW_H_UINT, 6, 1,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_DUP] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* ctxdisp: context displacement */
  { "ctxdisp", MS1_OPERAND_CTXDISP, HW_H_UINT, 5, 6,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_CTXDISP] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* fbdisp: frame buffer displacement */
  { "fbdisp", MS1_OPERAND_FBDISP, HW_H_UINT, 15, 6,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_FBDISP] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* type: type */
  { "type", MS1_OPERAND_TYPE, HW_H_UINT, 21, 2,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_TYPE] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* mask: mask */
  { "mask", MS1_OPERAND_MASK, HW_H_UINT, 25, 16,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_MASK] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* bankaddr: bank address */
  { "bankaddr", MS1_OPERAND_BANKADDR, HW_H_UINT, 25, 13,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_BANKADDR] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* incamt: increment amount */
  { "incamt", MS1_OPERAND_INCAMT, HW_H_UINT, 19, 8,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_INCAMT] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* xmode: xmode */
  { "xmode", MS1_OPERAND_XMODE, HW_H_UINT, 23, 1,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_XMODE] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* mask1: mask1 */
  { "mask1", MS1_OPERAND_MASK1, HW_H_UINT, 22, 3,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_MASK1] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* ball: b_all */
  { "ball", MS1_OPERAND_BALL, HW_H_UINT, 19, 1,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_BALL] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* brc: b_r_c */
  { "brc", MS1_OPERAND_BRC, HW_H_UINT, 18, 3,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_BRC] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* rda: rd */
  { "rda", MS1_OPERAND_RDA, HW_H_UINT, 25, 1,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_RDA] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* wr: wr */
  { "wr", MS1_OPERAND_WR, HW_H_UINT, 24, 1,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_WR] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* ball2: b_all2 */
  { "ball2", MS1_OPERAND_BALL2, HW_H_UINT, 15, 1,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_BALL2] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* brc2: b_r_c2 */
  { "brc2", MS1_OPERAND_BRC2, HW_H_UINT, 14, 3,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_BRC2] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* perm: perm */
  { "perm", MS1_OPERAND_PERM, HW_H_UINT, 25, 2,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_PERM] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* a23: a23 */
  { "a23", MS1_OPERAND_A23, HW_H_UINT, 23, 1,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_A23] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* cr: c-r */
  { "cr", MS1_OPERAND_CR, HW_H_UINT, 22, 3,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_CR] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* cbs: cbs */
  { "cbs", MS1_OPERAND_CBS, HW_H_UINT, 19, 2,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_CBS] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* incr: incr */
  { "incr", MS1_OPERAND_INCR, HW_H_UINT, 17, 6,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_INCR] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* length: length */
  { "length", MS1_OPERAND_LENGTH, HW_H_UINT, 15, 3,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_LENGTH] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* cbx: cbx */
  { "cbx", MS1_OPERAND_CBX, HW_H_UINT, 14, 3,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_CBX] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* ccb: ccb */
  { "ccb", MS1_OPERAND_CCB, HW_H_UINT, 11, 1,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_CCB] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* cdb: cdb */
  { "cdb", MS1_OPERAND_CDB, HW_H_UINT, 10, 1,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_CDB] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* mode: mode */
  { "mode", MS1_OPERAND_MODE, HW_H_UINT, 25, 2,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_MODE] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* id: i/d */
  { "id", MS1_OPERAND_ID, HW_H_UINT, 14, 1,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_ID] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* size: size */
  { "size", MS1_OPERAND_SIZE, HW_H_UINT, 13, 14,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_SIZE] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* fbincr: fb incr */
  { "fbincr", MS1_OPERAND_FBINCR, HW_H_UINT, 23, 4,
    { 0, { (const PTR) &ms1_cgen_ifld_table[MS1_F_FBINCR] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* sentinel */
  { 0, 0, 0, 0, 0,
    { 0, { (const PTR) 0 } },
    { 0, { 0 } } }
};

#undef A


/* The instruction table.  */

#define OP(field) CGEN_SYNTAX_MAKE_FIELD (OPERAND (field))
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_INSN_##a)
#else
#define A(a) (1 << CGEN_INSN_/**/a)
#endif

static const CGEN_IBASE ms1_cgen_insn_table[MAX_INSNS] =
{
  /* Special null first entry.
     A `num' value of zero is thus invalid.
     Also, the special `invalid' insn resides here.  */
  { 0, 0, 0, 0, {0, {0}} },
/* add $frdrrr,$frsr1,$frsr2 */
  {
    MS1_INSN_ADD, "add", "add", 32,
    { 0|A(USES_FRSR2)|A(USES_FRSR1)|A(USES_FRDRRR)|A(AL_INSN), { (1<<MACH_BASE) } }
  },
/* addu $frdrrr,$frsr1,$frsr2 */
  {
    MS1_INSN_ADDU, "addu", "addu", 32,
    { 0|A(USES_FRSR2)|A(USES_FRSR1)|A(USES_FRDRRR)|A(AL_INSN), { (1<<MACH_BASE) } }
  },
/* addi $frdr,$frsr1,#$imm16 */
  {
    MS1_INSN_ADDI, "addi", "addi", 32,
    { 0|A(USES_FRSR1)|A(USES_FRDR)|A(AL_INSN), { (1<<MACH_BASE) } }
  },
/* addui $frdr,$frsr1,#$imm16z */
  {
    MS1_INSN_ADDUI, "addui", "addui", 32,
    { 0|A(USES_FRSR1)|A(USES_FRDR)|A(AL_INSN), { (1<<MACH_BASE) } }
  },
/* sub $frdrrr,$frsr1,$frsr2 */
  {
    MS1_INSN_SUB, "sub", "sub", 32,
    { 0|A(USES_FRSR2)|A(USES_FRSR1)|A(USES_FRDRRR)|A(AL_INSN), { (1<<MACH_BASE) } }
  },
/* subu $frdrrr,$frsr1,$frsr2 */
  {
    MS1_INSN_SUBU, "subu", "subu", 32,
    { 0|A(USES_FRSR2)|A(USES_FRSR1)|A(USES_FRDRRR)|A(AL_INSN), { (1<<MACH_BASE) } }
  },
/* subi $frdr,$frsr1,#$imm16 */
  {
    MS1_INSN_SUBI, "subi", "subi", 32,
    { 0|A(USES_FRSR1)|A(USES_FRDR)|A(AL_INSN), { (1<<MACH_BASE) } }
  },
/* subui $frdr,$frsr1,#$imm16z */
  {
    MS1_INSN_SUBUI, "subui", "subui", 32,
    { 0|A(USES_FRSR1)|A(USES_FRDR)|A(AL_INSN), { (1<<MACH_BASE) } }
  },
/* mul $frdrrr,$frsr1,$frsr2 */
  {
    MS1_INSN_MUL, "mul", "mul", 32,
    { 0|A(USES_FRSR2)|A(USES_FRSR1)|A(USES_FRDRRR)|A(AL_INSN), { (1<<MACH_MS1_003) } }
  },
/* muli $frdr,$frsr1,#$imm16 */
  {
    MS1_INSN_MULI, "muli", "muli", 32,
    { 0|A(USES_FRSR1)|A(USES_FRDR)|A(AL_INSN), { (1<<MACH_MS1_003) } }
  },
/* and $frdrrr,$frsr1,$frsr2 */
  {
    MS1_INSN_AND, "and", "and", 32,
    { 0|A(USES_FRSR2)|A(USES_FRSR1)|A(USES_FRDRRR)|A(AL_INSN), { (1<<MACH_BASE) } }
  },
/* andi $frdr,$frsr1,#$imm16z */
  {
    MS1_INSN_ANDI, "andi", "andi", 32,
    { 0|A(USES_FRSR1)|A(USES_FRDR)|A(AL_INSN), { (1<<MACH_BASE) } }
  },
/* or $frdrrr,$frsr1,$frsr2 */
  {
    MS1_INSN_OR, "or", "or", 32,
    { 0|A(USES_FRSR2)|A(USES_FRSR1)|A(USES_FRDRRR)|A(AL_INSN), { (1<<MACH_BASE) } }
  },
/* nop */
  {
    MS1_INSN_NOP, "nop", "nop", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* ori $frdr,$frsr1,#$imm16z */
  {
    MS1_INSN_ORI, "ori", "ori", 32,
    { 0|A(USES_FRSR1)|A(USES_FRDR)|A(AL_INSN), { (1<<MACH_BASE) } }
  },
/* xor $frdrrr,$frsr1,$frsr2 */
  {
    MS1_INSN_XOR, "xor", "xor", 32,
    { 0|A(USES_FRSR2)|A(USES_FRSR1)|A(USES_FRDRRR)|A(AL_INSN), { (1<<MACH_BASE) } }
  },
/* xori $frdr,$frsr1,#$imm16z */
  {
    MS1_INSN_XORI, "xori", "xori", 32,
    { 0|A(USES_FRSR1)|A(USES_FRDR)|A(AL_INSN), { (1<<MACH_BASE) } }
  },
/* nand $frdrrr,$frsr1,$frsr2 */
  {
    MS1_INSN_NAND, "nand", "nand", 32,
    { 0|A(USES_FRSR2)|A(USES_FRSR1)|A(USES_FRDRRR)|A(AL_INSN), { (1<<MACH_BASE) } }
  },
/* nandi $frdr,$frsr1,#$imm16z */
  {
    MS1_INSN_NANDI, "nandi", "nandi", 32,
    { 0|A(USES_FRSR1)|A(USES_FRDR)|A(AL_INSN), { (1<<MACH_BASE) } }
  },
/* nor $frdrrr,$frsr1,$frsr2 */
  {
    MS1_INSN_NOR, "nor", "nor", 32,
    { 0|A(USES_FRSR2)|A(USES_FRSR1)|A(USES_FRDRRR)|A(AL_INSN), { (1<<MACH_BASE) } }
  },
/* nori $frdr,$frsr1,#$imm16z */
  {
    MS1_INSN_NORI, "nori", "nori", 32,
    { 0|A(USES_FRSR1)|A(USES_FRDR)|A(AL_INSN), { (1<<MACH_BASE) } }
  },
/* xnor $frdrrr,$frsr1,$frsr2 */
  {
    MS1_INSN_XNOR, "xnor", "xnor", 32,
    { 0|A(USES_FRSR2)|A(USES_FRSR1)|A(USES_FRDRRR)|A(AL_INSN), { (1<<MACH_BASE) } }
  },
/* xnori $frdr,$frsr1,#$imm16z */
  {
    MS1_INSN_XNORI, "xnori", "xnori", 32,
    { 0|A(USES_FRSR1)|A(USES_FRDR)|A(AL_INSN), { (1<<MACH_BASE) } }
  },
/* ldui $frdr,#$imm16z */
  {
    MS1_INSN_LDUI, "ldui", "ldui", 32,
    { 0|A(USES_FRDR)|A(AL_INSN), { (1<<MACH_BASE) } }
  },
/* lsl $frdrrr,$frsr1,$frsr2 */
  {
    MS1_INSN_LSL, "lsl", "lsl", 32,
    { 0|A(USES_FRSR2)|A(USES_FRSR1)|A(USES_FRDRRR), { (1<<MACH_BASE) } }
  },
/* lsli $frdr,$frsr1,#$imm16 */
  {
    MS1_INSN_LSLI, "lsli", "lsli", 32,
    { 0|A(USES_FRSR1)|A(USES_FRDR), { (1<<MACH_BASE) } }
  },
/* lsr $frdrrr,$frsr1,$frsr2 */
  {
    MS1_INSN_LSR, "lsr", "lsr", 32,
    { 0|A(USES_FRSR2)|A(USES_FRSR1)|A(USES_FRDRRR), { (1<<MACH_BASE) } }
  },
/* lsri $frdr,$frsr1,#$imm16 */
  {
    MS1_INSN_LSRI, "lsri", "lsri", 32,
    { 0|A(USES_FRSR1)|A(USES_FRDR), { (1<<MACH_BASE) } }
  },
/* asr $frdrrr,$frsr1,$frsr2 */
  {
    MS1_INSN_ASR, "asr", "asr", 32,
    { 0|A(USES_FRSR2)|A(USES_FRSR1)|A(USES_FRDRRR), { (1<<MACH_BASE) } }
  },
/* asri $frdr,$frsr1,#$imm16 */
  {
    MS1_INSN_ASRI, "asri", "asri", 32,
    { 0|A(USES_FRSR1)|A(USES_FRDR), { (1<<MACH_BASE) } }
  },
/* brlt $frsr1,$frsr2,$imm16o */
  {
    MS1_INSN_BRLT, "brlt", "brlt", 32,
    { 0|A(USES_FRSR2)|A(USES_FRSR1)|A(USES_FRDRRR)|A(DELAY_SLOT)|A(BR_INSN), { (1<<MACH_BASE) } }
  },
/* brle $frsr1,$frsr2,$imm16o */
  {
    MS1_INSN_BRLE, "brle", "brle", 32,
    { 0|A(USES_FRSR2)|A(USES_FRSR1)|A(DELAY_SLOT)|A(BR_INSN), { (1<<MACH_BASE) } }
  },
/* breq $frsr1,$frsr2,$imm16o */
  {
    MS1_INSN_BREQ, "breq", "breq", 32,
    { 0|A(USES_FRSR2)|A(USES_FRSR1)|A(DELAY_SLOT)|A(BR_INSN), { (1<<MACH_BASE) } }
  },
/* brne $frsr1,$frsr2,$imm16o */
  {
    MS1_INSN_BRNE, "brne", "brne", 32,
    { 0|A(USES_FRSR2)|A(USES_FRSR1)|A(DELAY_SLOT)|A(BR_INSN), { (1<<MACH_BASE) } }
  },
/* jmp $imm16o */
  {
    MS1_INSN_JMP, "jmp", "jmp", 32,
    { 0|A(BR_INSN)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* jal $frdrrr,$frsr1 */
  {
    MS1_INSN_JAL, "jal", "jal", 32,
    { 0|A(USES_FRSR1)|A(USES_FRDR)|A(BR_INSN)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* dbnz $frsr1,$imm16o */
  {
    MS1_INSN_DBNZ, "dbnz", "dbnz", 32,
    { 0|A(USES_FRSR1)|A(DELAY_SLOT)|A(BR_INSN), { (1<<MACH_MS1_003) } }
  },
/* ei */
  {
    MS1_INSN_EI, "ei", "ei", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* di */
  {
    MS1_INSN_DI, "di", "di", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* si $frdrrr */
  {
    MS1_INSN_SI, "si", "si", 32,
    { 0|A(USES_FRDR)|A(BR_INSN)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* reti $frsr1 */
  {
    MS1_INSN_RETI, "reti", "reti", 32,
    { 0|A(USES_FRSR1)|A(BR_INSN)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* ldw $frdr,$frsr1,#$imm16 */
  {
    MS1_INSN_LDW, "ldw", "ldw", 32,
    { 0|A(USES_FRSR1)|A(USES_FRDR)|A(MEMORY_ACCESS)|A(LOAD_DELAY), { (1<<MACH_BASE) } }
  },
/* stw $frsr2,$frsr1,#$imm16 */
  {
    MS1_INSN_STW, "stw", "stw", 32,
    { 0|A(USES_FRSR2)|A(USES_FRSR1)|A(MEMORY_ACCESS), { (1<<MACH_BASE) } }
  },
/* break */
  {
    MS1_INSN_BREAK, "break", "break", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* iflush */
  {
    MS1_INSN_IFLUSH, "iflush", "iflush", 32,
    { 0, { (1<<MACH_MS1_003) } }
  },
/* ldctxt $frsr1,$frsr2,#$rc,#$rcnum,#$contnum */
  {
    MS1_INSN_LDCTXT, "ldctxt", "ldctxt", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* ldfb $frsr1,$frsr2,#$imm16z */
  {
    MS1_INSN_LDFB, "ldfb", "ldfb", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* stfb $frsr1,$frsr2,#$imm16z */
  {
    MS1_INSN_STFB, "stfb", "stfb", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* fbcb $frsr1,#$rbbc,#$ball,#$brc,#$rc1,#$cbrb,#$cell,#$dup,#$ctxdisp */
  {
    MS1_INSN_FBCB, "fbcb", "fbcb", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* mfbcb $frsr1,#$rbbc,$frsr2,#$rc1,#$cbrb,#$cell,#$dup,#$ctxdisp */
  {
    MS1_INSN_MFBCB, "mfbcb", "mfbcb", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* fbcci $frsr1,#$rbbc,#$ball,#$brc,#$fbdisp,#$cell,#$dup,#$ctxdisp */
  {
    MS1_INSN_FBCCI, "fbcci", "fbcci", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* fbrci $frsr1,#$rbbc,#$ball,#$brc,#$fbdisp,#$cell,#$dup,#$ctxdisp */
  {
    MS1_INSN_FBRCI, "fbrci", "fbrci", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* fbcri $frsr1,#$rbbc,#$ball,#$brc,#$fbdisp,#$cell,#$dup,#$ctxdisp */
  {
    MS1_INSN_FBCRI, "fbcri", "fbcri", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* fbrri $frsr1,#$rbbc,#$ball,#$brc,#$fbdisp,#$cell,#$dup,#$ctxdisp */
  {
    MS1_INSN_FBRRI, "fbrri", "fbrri", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* mfbcci $frsr1,#$rbbc,$frsr2,#$fbdisp,#$cell,#$dup,#$ctxdisp */
  {
    MS1_INSN_MFBCCI, "mfbcci", "mfbcci", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* mfbrci $frsr1,#$rbbc,$frsr2,#$fbdisp,#$cell,#$dup,#$ctxdisp */
  {
    MS1_INSN_MFBRCI, "mfbrci", "mfbrci", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* mfbcri $frsr1,#$rbbc,$frsr2,#$fbdisp,#$cell,#$dup,#$ctxdisp */
  {
    MS1_INSN_MFBCRI, "mfbcri", "mfbcri", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* mfbrri $frsr1,#$rbbc,$frsr2,#$fbdisp,#$cell,#$dup,#$ctxdisp */
  {
    MS1_INSN_MFBRRI, "mfbrri", "mfbrri", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* fbcbdr $frsr1,#$rbbc,$frsr2,#$ball2,#$brc2,#$rc1,#$cbrb,#$cell,#$dup,#$ctxdisp */
  {
    MS1_INSN_FBCBDR, "fbcbdr", "fbcbdr", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* rcfbcb #$rbbc,#$type,#$ball,#$brc,#$rownum,#$rc1,#$cbrb,#$cell,#$dup,#$ctxdisp */
  {
    MS1_INSN_RCFBCB, "rcfbcb", "rcfbcb", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* mrcfbcb $frsr2,#$rbbc,#$type,#$rownum,#$rc1,#$cbrb,#$cell,#$dup,#$ctxdisp */
  {
    MS1_INSN_MRCFBCB, "mrcfbcb", "mrcfbcb", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* cbcast #$mask,#$rc2,#$ctxdisp */
  {
    MS1_INSN_CBCAST, "cbcast", "cbcast", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* dupcbcast #$mask,#$cell,#$rc2,#$ctxdisp */
  {
    MS1_INSN_DUPCBCAST, "dupcbcast", "dupcbcast", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* wfbi #$bankaddr,#$rownum1,#$cell,#$dup,#$ctxdisp */
  {
    MS1_INSN_WFBI, "wfbi", "wfbi", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* wfb $frsr1,$frsr2,#$fbdisp,#$rownum2,#$ctxdisp */
  {
    MS1_INSN_WFB, "wfb", "wfb", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* rcrisc $frdrrr,#$rbbc,$frsr1,#$colnum,#$rc1,#$cbrb,#$cell,#$dup,#$ctxdisp */
  {
    MS1_INSN_RCRISC, "rcrisc", "rcrisc", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* fbcbinc $frsr1,#$rbbc,#$incamt,#$rc1,#$cbrb,#$cell,#$dup,#$ctxdisp */
  {
    MS1_INSN_FBCBINC, "fbcbinc", "fbcbinc", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* rcxmode $frsr2,#$rda,#$wr,#$xmode,#$mask1,#$fbdisp,#$rownum2,#$rc2,#$ctxdisp */
  {
    MS1_INSN_RCXMODE, "rcxmode", "rcxmode", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* intlvr $frsr1,#$mode,$frsr2,#$id,#$size */
  {
    MS1_INSN_INTERLEAVER, "interleaver", "intlvr", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* wfbinc #$rda,#$wr,#$fbincr,#$ball,#$colnum,#$length,#$rownum1,#$rownum2,#$dup,#$ctxdisp */
  {
    MS1_INSN_WFBINC, "wfbinc", "wfbinc", 32,
    { 0, { (1<<MACH_MS1_003) } }
  },
/* mwfbinc $frsr2,#$rda,#$wr,#$fbincr,#$length,#$rownum1,#$rownum2,#$dup,#$ctxdisp */
  {
    MS1_INSN_MWFBINC, "mwfbinc", "mwfbinc", 32,
    { 0, { (1<<MACH_MS1_003) } }
  },
/* wfbincr $frsr1,#$rda,#$wr,#$ball,#$colnum,#$length,#$rownum1,#$rownum2,#$dup,#$ctxdisp */
  {
    MS1_INSN_WFBINCR, "wfbincr", "wfbincr", 32,
    { 0, { (1<<MACH_MS1_003) } }
  },
/* mwfbincr $frsr1,$frsr2,#$rda,#$wr,#$length,#$rownum1,#$rownum2,#$dup,#$ctxdisp */
  {
    MS1_INSN_MWFBINCR, "mwfbincr", "mwfbincr", 32,
    { 0, { (1<<MACH_MS1_003) } }
  },
/* fbcbincs #$perm,#$a23,#$cr,#$cbs,#$incr,#$ccb,#$cdb,#$rownum2,#$dup,#$ctxdisp */
  {
    MS1_INSN_FBCBINCS, "fbcbincs", "fbcbincs", 32,
    { 0, { (1<<MACH_MS1_003) } }
  },
/* mfbcbincs $frsr1,#$perm,#$cbs,#$incr,#$ccb,#$cdb,#$rownum2,#$dup,#$ctxdisp */
  {
    MS1_INSN_MFBCBINCS, "mfbcbincs", "mfbcbincs", 32,
    { 0, { (1<<MACH_MS1_003) } }
  },
/* fbcbincrs $frsr1,#$perm,#$ball,#$colnum,#$cbx,#$ccb,#$cdb,#$rownum2,#$dup,#$ctxdisp */
  {
    MS1_INSN_FBCBINCRS, "fbcbincrs", "fbcbincrs", 32,
    { 0, { (1<<MACH_MS1_003) } }
  },
/* mfbcbincrs $frsr1,$frsr2,#$perm,#$cbx,#$ccb,#$cdb,#$rownum2,#$dup,#$ctxdisp */
  {
    MS1_INSN_MFBCBINCRS, "mfbcbincrs", "mfbcbincrs", 32,
    { 0, { (1<<MACH_MS1_003) } }
  },
};

#undef OP
#undef A

/* Initialize anything needed to be done once, before any cpu_open call.  */

static void
init_tables (void)
{
}

static const CGEN_MACH * lookup_mach_via_bfd_name (const CGEN_MACH *, const char *);
static void build_hw_table      (CGEN_CPU_TABLE *);
static void build_ifield_table  (CGEN_CPU_TABLE *);
static void build_operand_table (CGEN_CPU_TABLE *);
static void build_insn_table    (CGEN_CPU_TABLE *);
static void ms1_cgen_rebuild_tables (CGEN_CPU_TABLE *);

/* Subroutine of ms1_cgen_cpu_open to look up a mach via its bfd name.  */

static const CGEN_MACH *
lookup_mach_via_bfd_name (const CGEN_MACH *table, const char *name)
{
  while (table->name)
    {
      if (strcmp (name, table->bfd_name) == 0)
	return table;
      ++table;
    }
  abort ();
}

/* Subroutine of ms1_cgen_cpu_open to build the hardware table.  */

static void
build_hw_table (CGEN_CPU_TABLE *cd)
{
  int i;
  int machs = cd->machs;
  const CGEN_HW_ENTRY *init = & ms1_cgen_hw_table[0];
  /* MAX_HW is only an upper bound on the number of selected entries.
     However each entry is indexed by it's enum so there can be holes in
     the table.  */
  const CGEN_HW_ENTRY **selected =
    (const CGEN_HW_ENTRY **) xmalloc (MAX_HW * sizeof (CGEN_HW_ENTRY *));

  cd->hw_table.init_entries = init;
  cd->hw_table.entry_size = sizeof (CGEN_HW_ENTRY);
  memset (selected, 0, MAX_HW * sizeof (CGEN_HW_ENTRY *));
  /* ??? For now we just use machs to determine which ones we want.  */
  for (i = 0; init[i].name != NULL; ++i)
    if (CGEN_HW_ATTR_VALUE (&init[i], CGEN_HW_MACH)
	& machs)
      selected[init[i].type] = &init[i];
  cd->hw_table.entries = selected;
  cd->hw_table.num_entries = MAX_HW;
}

/* Subroutine of ms1_cgen_cpu_open to build the hardware table.  */

static void
build_ifield_table (CGEN_CPU_TABLE *cd)
{
  cd->ifld_table = & ms1_cgen_ifld_table[0];
}

/* Subroutine of ms1_cgen_cpu_open to build the hardware table.  */

static void
build_operand_table (CGEN_CPU_TABLE *cd)
{
  int i;
  int machs = cd->machs;
  const CGEN_OPERAND *init = & ms1_cgen_operand_table[0];
  /* MAX_OPERANDS is only an upper bound on the number of selected entries.
     However each entry is indexed by it's enum so there can be holes in
     the table.  */
  const CGEN_OPERAND **selected = xmalloc (MAX_OPERANDS * sizeof (* selected));

  cd->operand_table.init_entries = init;
  cd->operand_table.entry_size = sizeof (CGEN_OPERAND);
  memset (selected, 0, MAX_OPERANDS * sizeof (CGEN_OPERAND *));
  /* ??? For now we just use mach to determine which ones we want.  */
  for (i = 0; init[i].name != NULL; ++i)
    if (CGEN_OPERAND_ATTR_VALUE (&init[i], CGEN_OPERAND_MACH)
	& machs)
      selected[init[i].type] = &init[i];
  cd->operand_table.entries = selected;
  cd->operand_table.num_entries = MAX_OPERANDS;
}

/* Subroutine of ms1_cgen_cpu_open to build the hardware table.
   ??? This could leave out insns not supported by the specified mach/isa,
   but that would cause errors like "foo only supported by bar" to become
   "unknown insn", so for now we include all insns and require the app to
   do the checking later.
   ??? On the other hand, parsing of such insns may require their hardware or
   operand elements to be in the table [which they mightn't be].  */

static void
build_insn_table (CGEN_CPU_TABLE *cd)
{
  int i;
  const CGEN_IBASE *ib = & ms1_cgen_insn_table[0];
  CGEN_INSN *insns = xmalloc (MAX_INSNS * sizeof (CGEN_INSN));

  memset (insns, 0, MAX_INSNS * sizeof (CGEN_INSN));
  for (i = 0; i < MAX_INSNS; ++i)
    insns[i].base = &ib[i];
  cd->insn_table.init_entries = insns;
  cd->insn_table.entry_size = sizeof (CGEN_IBASE);
  cd->insn_table.num_init_entries = MAX_INSNS;
}

/* Subroutine of ms1_cgen_cpu_open to rebuild the tables.  */

static void
ms1_cgen_rebuild_tables (CGEN_CPU_TABLE *cd)
{
  int i;
  unsigned int isas = cd->isas;
  unsigned int machs = cd->machs;

  cd->int_insn_p = CGEN_INT_INSN_P;

  /* Data derived from the isa spec.  */
#define UNSET (CGEN_SIZE_UNKNOWN + 1)
  cd->default_insn_bitsize = UNSET;
  cd->base_insn_bitsize = UNSET;
  cd->min_insn_bitsize = 65535; /* Some ridiculously big number.  */
  cd->max_insn_bitsize = 0;
  for (i = 0; i < MAX_ISAS; ++i)
    if (((1 << i) & isas) != 0)
      {
	const CGEN_ISA *isa = & ms1_cgen_isa_table[i];

	/* Default insn sizes of all selected isas must be
	   equal or we set the result to 0, meaning "unknown".  */
	if (cd->default_insn_bitsize == UNSET)
	  cd->default_insn_bitsize = isa->default_insn_bitsize;
	else if (isa->default_insn_bitsize == cd->default_insn_bitsize)
	  ; /* This is ok.  */
	else
	  cd->default_insn_bitsize = CGEN_SIZE_UNKNOWN;

	/* Base insn sizes of all selected isas must be equal
	   or we set the result to 0, meaning "unknown".  */
	if (cd->base_insn_bitsize == UNSET)
	  cd->base_insn_bitsize = isa->base_insn_bitsize;
	else if (isa->base_insn_bitsize == cd->base_insn_bitsize)
	  ; /* This is ok.  */
	else
	  cd->base_insn_bitsize = CGEN_SIZE_UNKNOWN;

	/* Set min,max insn sizes.  */
	if (isa->min_insn_bitsize < cd->min_insn_bitsize)
	  cd->min_insn_bitsize = isa->min_insn_bitsize;
	if (isa->max_insn_bitsize > cd->max_insn_bitsize)
	  cd->max_insn_bitsize = isa->max_insn_bitsize;
      }

  /* Data derived from the mach spec.  */
  for (i = 0; i < MAX_MACHS; ++i)
    if (((1 << i) & machs) != 0)
      {
	const CGEN_MACH *mach = & ms1_cgen_mach_table[i];

	if (mach->insn_chunk_bitsize != 0)
	{
	  if (cd->insn_chunk_bitsize != 0 && cd->insn_chunk_bitsize != mach->insn_chunk_bitsize)
	    {
	      fprintf (stderr, "ms1_cgen_rebuild_tables: conflicting insn-chunk-bitsize values: `%d' vs. `%d'\n",
		       cd->insn_chunk_bitsize, mach->insn_chunk_bitsize);
	      abort ();
	    }

 	  cd->insn_chunk_bitsize = mach->insn_chunk_bitsize;
	}
      }

  /* Determine which hw elements are used by MACH.  */
  build_hw_table (cd);

  /* Build the ifield table.  */
  build_ifield_table (cd);

  /* Determine which operands are used by MACH/ISA.  */
  build_operand_table (cd);

  /* Build the instruction table.  */
  build_insn_table (cd);
}

/* Initialize a cpu table and return a descriptor.
   It's much like opening a file, and must be the first function called.
   The arguments are a set of (type/value) pairs, terminated with
   CGEN_CPU_OPEN_END.

   Currently supported values:
   CGEN_CPU_OPEN_ISAS:    bitmap of values in enum isa_attr
   CGEN_CPU_OPEN_MACHS:   bitmap of values in enum mach_attr
   CGEN_CPU_OPEN_BFDMACH: specify 1 mach using bfd name
   CGEN_CPU_OPEN_ENDIAN:  specify endian choice
   CGEN_CPU_OPEN_END:     terminates arguments

   ??? Simultaneous multiple isas might not make sense, but it's not (yet)
   precluded.

   ??? We only support ISO C stdargs here, not K&R.
   Laziness, plus experiment to see if anything requires K&R - eventually
   K&R will no longer be supported - e.g. GDB is currently trying this.  */

CGEN_CPU_DESC
ms1_cgen_cpu_open (enum cgen_cpu_open_arg arg_type, ...)
{
  CGEN_CPU_TABLE *cd = (CGEN_CPU_TABLE *) xmalloc (sizeof (CGEN_CPU_TABLE));
  static int init_p;
  unsigned int isas = 0;  /* 0 = "unspecified" */
  unsigned int machs = 0; /* 0 = "unspecified" */
  enum cgen_endian endian = CGEN_ENDIAN_UNKNOWN;
  va_list ap;

  if (! init_p)
    {
      init_tables ();
      init_p = 1;
    }

  memset (cd, 0, sizeof (*cd));

  va_start (ap, arg_type);
  while (arg_type != CGEN_CPU_OPEN_END)
    {
      switch (arg_type)
	{
	case CGEN_CPU_OPEN_ISAS :
	  isas = va_arg (ap, unsigned int);
	  break;
	case CGEN_CPU_OPEN_MACHS :
	  machs = va_arg (ap, unsigned int);
	  break;
	case CGEN_CPU_OPEN_BFDMACH :
	  {
	    const char *name = va_arg (ap, const char *);
	    const CGEN_MACH *mach =
	      lookup_mach_via_bfd_name (ms1_cgen_mach_table, name);

	    machs |= 1 << mach->num;
	    break;
	  }
	case CGEN_CPU_OPEN_ENDIAN :
	  endian = va_arg (ap, enum cgen_endian);
	  break;
	default :
	  fprintf (stderr, "ms1_cgen_cpu_open: unsupported argument `%d'\n",
		   arg_type);
	  abort (); /* ??? return NULL? */
	}
      arg_type = va_arg (ap, enum cgen_cpu_open_arg);
    }
  va_end (ap);

  /* Mach unspecified means "all".  */
  if (machs == 0)
    machs = (1 << MAX_MACHS) - 1;
  /* Base mach is always selected.  */
  machs |= 1;
  /* ISA unspecified means "all".  */
  if (isas == 0)
    isas = (1 << MAX_ISAS) - 1;
  if (endian == CGEN_ENDIAN_UNKNOWN)
    {
      /* ??? If target has only one, could have a default.  */
      fprintf (stderr, "ms1_cgen_cpu_open: no endianness specified\n");
      abort ();
    }

  cd->isas = isas;
  cd->machs = machs;
  cd->endian = endian;
  /* FIXME: for the sparc case we can determine insn-endianness statically.
     The worry here is where both data and insn endian can be independently
     chosen, in which case this function will need another argument.
     Actually, will want to allow for more arguments in the future anyway.  */
  cd->insn_endian = endian;

  /* Table (re)builder.  */
  cd->rebuild_tables = ms1_cgen_rebuild_tables;
  ms1_cgen_rebuild_tables (cd);

  /* Default to not allowing signed overflow.  */
  cd->signed_overflow_ok_p = 0;
  
  return (CGEN_CPU_DESC) cd;
}

/* Cover fn to ms1_cgen_cpu_open to handle the simple case of 1 isa, 1 mach.
   MACH_NAME is the bfd name of the mach.  */

CGEN_CPU_DESC
ms1_cgen_cpu_open_1 (const char *mach_name, enum cgen_endian endian)
{
  return ms1_cgen_cpu_open (CGEN_CPU_OPEN_BFDMACH, mach_name,
			       CGEN_CPU_OPEN_ENDIAN, endian,
			       CGEN_CPU_OPEN_END);
}

/* Close a cpu table.
   ??? This can live in a machine independent file, but there's currently
   no place to put this file (there's no libcgen).  libopcodes is the wrong
   place as some simulator ports use this but they don't use libopcodes.  */

void
ms1_cgen_cpu_close (CGEN_CPU_DESC cd)
{
  unsigned int i;
  const CGEN_INSN *insns;

  if (cd->macro_insn_table.init_entries)
    {
      insns = cd->macro_insn_table.init_entries;
      for (i = 0; i < cd->macro_insn_table.num_init_entries; ++i, ++insns)
	if (CGEN_INSN_RX ((insns)))
	  regfree (CGEN_INSN_RX (insns));
    }

  if (cd->insn_table.init_entries)
    {
      insns = cd->insn_table.init_entries;
      for (i = 0; i < cd->insn_table.num_init_entries; ++i, ++insns)
	if (CGEN_INSN_RX (insns))
	  regfree (CGEN_INSN_RX (insns));
    }  

  if (cd->macro_insn_table.init_entries)
    free ((CGEN_INSN *) cd->macro_insn_table.init_entries);

  if (cd->insn_table.init_entries)
    free ((CGEN_INSN *) cd->insn_table.init_entries);

  if (cd->hw_table.entries)
    free ((CGEN_HW_ENTRY *) cd->hw_table.entries);

  if (cd->operand_table.entries)
    free ((CGEN_HW_ENTRY *) cd->operand_table.entries);

  free (cd);
}


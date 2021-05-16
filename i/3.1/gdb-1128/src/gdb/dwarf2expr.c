/* DWARF 2 Expression Evaluator.

   Copyright 2001, 2002, 2003, 2005 Free Software Foundation, Inc.

   Contributed by Daniel Berlin (dan@dberlin.org)

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
#include "symtab.h"
#include "gdbtypes.h"
#include "value.h"
#include "gdbcore.h"
#include "elf/dwarf2.h"
#include "dwarf2expr.h"
/* APPLE LOCAL variable initialized status  */
#include "exceptions.h"
#include "dwarf2-frame.h"

/* Local prototypes.  */

static void execute_stack_op (struct dwarf_expr_context *,
			      gdb_byte *, gdb_byte *, int eh_frame_p);

/* Create a new context for the expression evaluator.  */

struct dwarf_expr_context *
new_dwarf_expr_context (void)
{
  struct dwarf_expr_context *retval;
  retval = xcalloc (1, sizeof (struct dwarf_expr_context));
  retval->stack_len = 0;
  retval->stack_allocated = 10;
  retval->stack = xmalloc (retval->stack_allocated * sizeof (CORE_ADDR));
  retval->num_pieces = 0;
  retval->pieces = 0;
  return retval;
}

/* Release the memory allocated to CTX.  */

void
free_dwarf_expr_context (struct dwarf_expr_context *ctx)
{
  xfree (ctx->stack);
  xfree (ctx->pieces);
  xfree (ctx);
}

/* Expand the memory allocated to CTX's stack to contain at least
   NEED more elements than are currently used.  */

static void
dwarf_expr_grow_stack (struct dwarf_expr_context *ctx, size_t need)
{
  if (ctx->stack_len + need > ctx->stack_allocated)
    {
      size_t newlen = ctx->stack_len + need + 10;
      ctx->stack = xrealloc (ctx->stack,
			     newlen * sizeof (CORE_ADDR));
      ctx->stack_allocated = newlen;
    }
}

/* Push VALUE onto CTX's stack.  */

void
dwarf_expr_push (struct dwarf_expr_context *ctx, CORE_ADDR value)
{
  dwarf_expr_grow_stack (ctx, 1);
  ctx->stack[ctx->stack_len++] = value;
}

/* Pop the top item off of CTX's stack.  */

void
dwarf_expr_pop (struct dwarf_expr_context *ctx)
{
  if (ctx->stack_len <= 0)
    error (_("dwarf expression stack underflow"));
  ctx->stack_len--;
}

/* Retrieve the N'th item on CTX's stack.  */

CORE_ADDR
dwarf_expr_fetch (struct dwarf_expr_context *ctx, int n)
{
  if (ctx->stack_len < n)
     error (_("Asked for position %d of stack, stack only has %d elements on it."),
	    n, ctx->stack_len);
  return ctx->stack[ctx->stack_len - (1 + n)];

}

/* Add a new piece to CTX's piece list.  */
/* APPLE LOCAL variable initialized status  */
void
add_piece (struct dwarf_expr_context *ctx,
           int in_reg, CORE_ADDR value, ULONGEST size)
{
  struct dwarf_expr_piece *p;

  ctx->num_pieces++;

  if (ctx->pieces)
    ctx->pieces = xrealloc (ctx->pieces,
                            (ctx->num_pieces
                             * sizeof (struct dwarf_expr_piece)));
  else
    ctx->pieces = xmalloc (ctx->num_pieces
                           * sizeof (struct dwarf_expr_piece));

  p = &ctx->pieces[ctx->num_pieces - 1];
  p->in_reg = in_reg;
  p->value = value;
  p->size = size;
}

/* Evaluate the expression at ADDR (LEN bytes long) using the context
   CTX.  */

void
dwarf_expr_eval (struct dwarf_expr_context *ctx, gdb_byte *addr, size_t len, int eh_frame_p)
{
  execute_stack_op (ctx, addr, addr + len, eh_frame_p);
}

/* Decode the unsigned LEB128 constant at BUF into the variable pointed to
   by R, and return the new value of BUF.  Verify that it doesn't extend
   past BUF_END.  */

gdb_byte *
read_uleb128 (gdb_byte *buf, gdb_byte *buf_end, ULONGEST * r)
{
  unsigned shift = 0;
  ULONGEST result = 0;
  gdb_byte byte;

  while (1)
    {
      if (buf >= buf_end)
	error (_("read_uleb128: Corrupted DWARF expression."));

      byte = *buf++;
      result |= (byte & 0x7f) << shift;
      if ((byte & 0x80) == 0)
	break;
      shift += 7;
    }
  *r = result;
  return buf;
}

/* Decode the signed LEB128 constant at BUF into the variable pointed to
   by R, and return the new value of BUF.  Verify that it doesn't extend
   past BUF_END.  */

gdb_byte *
read_sleb128 (gdb_byte *buf, gdb_byte *buf_end, LONGEST * r)
{
  unsigned shift = 0;
  LONGEST result = 0;
  gdb_byte byte;

  while (1)
    {
      if (buf >= buf_end)
	error (_("read_sleb128: Corrupted DWARF expression."));

      byte = *buf++;
      result |= (byte & 0x7f) << shift;
      shift += 7;
      if ((byte & 0x80) == 0)
	break;
    }
  if (shift < (sizeof (*r) * 8) && (byte & 0x40) != 0)
    result |= -(1 << shift);

  *r = result;
  return buf;
}

/* Read an address from BUF, and verify that it doesn't extend past
   BUF_END.  The address is returned, and *BYTES_READ is set to the
   number of bytes read from BUF.  */

CORE_ADDR
dwarf2_read_address (gdb_byte *buf, gdb_byte *buf_end, int *bytes_read)
{
  CORE_ADDR result;

  if (buf_end - buf < TARGET_ADDR_BIT / TARGET_CHAR_BIT)
    error (_("dwarf2_read_address: Corrupted DWARF expression."));

  *bytes_read = TARGET_ADDR_BIT / TARGET_CHAR_BIT;
  /* NOTE: cagney/2003-05-22: This extract is assuming that a DWARF 2
     address is always unsigned.  That may or may not be true.  */
  result = extract_unsigned_integer (buf, TARGET_ADDR_BIT / TARGET_CHAR_BIT);
  return result;
}

/* Return the type of an address, for unsigned arithmetic.  */

/* APPLE LOCAL variable initialized status.  */
struct type *
unsigned_address_type (void)
{
  switch (TARGET_ADDR_BIT / TARGET_CHAR_BIT)
    {
    case 2:
      return builtin_type_uint16;
    case 4:
      return builtin_type_uint32;
    case 8:
      return builtin_type_uint64;
    default:
      internal_error (__FILE__, __LINE__,
		      _("Unsupported address size.\n"));
    }
}


/* Return the type of an address, for signed arithmetic.  */

/* APPLE LOCAL variable initialized status  */
struct type *
signed_address_type (void)
{
  switch (TARGET_ADDR_BIT / TARGET_CHAR_BIT)
    {
    case 2:
      return builtin_type_int16;
    case 4:
      return builtin_type_int32;
    case 8:
      return builtin_type_int64;
    default:
      internal_error (__FILE__, __LINE__,
		      _("Unsupported address size.\n"));
    }
}

/* The engine for the expression evaluator.  Using the context in CTX,
   evaluate the expression between OP_PTR and OP_END.  */

static void
execute_stack_op (struct dwarf_expr_context *ctx,
		  gdb_byte *op_ptr, gdb_byte *op_end, int eh_frame_p)
{
  ctx->in_reg = 0;
  /* APPLE LOCAL variable initialized status.  */
  ctx->var_status = 1;  /* Default is initialized.  */

  while (op_ptr < op_end)
    {
      enum dwarf_location_atom op = *op_ptr++;
      CORE_ADDR result;
      ULONGEST uoffset, reg;
      LONGEST offset;
      int bytes_read;

      switch (op)
	{
	case DW_OP_lit0:
	case DW_OP_lit1:
	case DW_OP_lit2:
	case DW_OP_lit3:
	case DW_OP_lit4:
	case DW_OP_lit5:
	case DW_OP_lit6:
	case DW_OP_lit7:
	case DW_OP_lit8:
	case DW_OP_lit9:
	case DW_OP_lit10:
	case DW_OP_lit11:
	case DW_OP_lit12:
	case DW_OP_lit13:
	case DW_OP_lit14:
	case DW_OP_lit15:
	case DW_OP_lit16:
	case DW_OP_lit17:
	case DW_OP_lit18:
	case DW_OP_lit19:
	case DW_OP_lit20:
	case DW_OP_lit21:
	case DW_OP_lit22:
	case DW_OP_lit23:
	case DW_OP_lit24:
	case DW_OP_lit25:
	case DW_OP_lit26:
	case DW_OP_lit27:
	case DW_OP_lit28:
	case DW_OP_lit29:
	case DW_OP_lit30:
	case DW_OP_lit31:
	  result = op - DW_OP_lit0;
	  break;

	case DW_OP_addr:
	  result = dwarf2_read_address (op_ptr, op_end, &bytes_read);
	  op_ptr += bytes_read;
	  break;

	case DW_OP_const1u:
	  result = extract_unsigned_integer (op_ptr, 1);
	  op_ptr += 1;
	  break;
	case DW_OP_const1s:
	  result = extract_signed_integer (op_ptr, 1);
	  op_ptr += 1;
	  break;
	case DW_OP_const2u:
	  result = extract_unsigned_integer (op_ptr, 2);
	  op_ptr += 2;
	  break;
	case DW_OP_const2s:
	  result = extract_signed_integer (op_ptr, 2);
	  op_ptr += 2;
	  break;
	case DW_OP_const4u:
	  result = extract_unsigned_integer (op_ptr, 4);
	  op_ptr += 4;
	  break;
	case DW_OP_const4s:
	  result = extract_signed_integer (op_ptr, 4);
	  op_ptr += 4;
	  break;
	case DW_OP_const8u:
	  result = extract_unsigned_integer (op_ptr, 8);
	  op_ptr += 8;
	  break;
	case DW_OP_const8s:
	  result = extract_signed_integer (op_ptr, 8);
	  op_ptr += 8;
	  break;
	case DW_OP_constu:
	  op_ptr = read_uleb128 (op_ptr, op_end, &uoffset);
	  result = uoffset;
	  break;
	case DW_OP_consts:
	  op_ptr = read_sleb128 (op_ptr, op_end, &offset);
	  result = offset;
	  break;

	/* The DW_OP_reg operations are required to occur alone in
	   location expressions.  */
	case DW_OP_reg0:
	case DW_OP_reg1:
	case DW_OP_reg2:
	case DW_OP_reg3:
	case DW_OP_reg4:
	case DW_OP_reg5:
	case DW_OP_reg6:
	case DW_OP_reg7:
	case DW_OP_reg8:
	case DW_OP_reg9:
	case DW_OP_reg10:
	case DW_OP_reg11:
	case DW_OP_reg12:
	case DW_OP_reg13:
	case DW_OP_reg14:
	case DW_OP_reg15:
	case DW_OP_reg16:
	case DW_OP_reg17:
	case DW_OP_reg18:
	case DW_OP_reg19:
	case DW_OP_reg20:
	case DW_OP_reg21:
	case DW_OP_reg22:
	case DW_OP_reg23:
	case DW_OP_reg24:
	case DW_OP_reg25:
	case DW_OP_reg26:
	case DW_OP_reg27:
	case DW_OP_reg28:
	case DW_OP_reg29:
	case DW_OP_reg30:
	case DW_OP_reg31:
	  /* APPLE LOCAL begin variable initialized status  */
	  if (op_ptr != op_end 
	      && *op_ptr != DW_OP_piece 
	      && *op_ptr != DW_OP_APPLE_uninit)
	  /* APPLE LOCAL end variable initialized status  */
	    error (_("DWARF-2 expression error: DW_OP_reg operations must be "
		   "used either alone or in conjuction with DW_OP_piece."));

	  result = op - DW_OP_reg0;
          result = dwarf2_frame_adjust_regnum (current_gdbarch, result, 
                                               eh_frame_p);
	  ctx->in_reg = 1;

	  break;

	case DW_OP_regx:
	  op_ptr = read_uleb128 (op_ptr, op_end, &reg);
	  /* APPLE LOCAL begin variable initialized status  */
	  if (op_ptr != op_end 
	      && *op_ptr != DW_OP_piece
	      && *op_ptr != DW_OP_APPLE_uninit)
	  /* APPLE LOCAL end variable initialized status  */
	    error (_("DWARF-2 expression error: DW_OP_reg operations must be "
		   "used either alone or in conjuction with DW_OP_piece."));

          result = dwarf2_frame_adjust_regnum (current_gdbarch, reg, 
                                               eh_frame_p);
	  ctx->in_reg = 1;
	  break;

	case DW_OP_breg0:
	case DW_OP_breg1:
	case DW_OP_breg2:
	case DW_OP_breg3:
	case DW_OP_breg4:
	case DW_OP_breg5:
	case DW_OP_breg6:
	case DW_OP_breg7:
	case DW_OP_breg8:
	case DW_OP_breg9:
	case DW_OP_breg10:
	case DW_OP_breg11:
	case DW_OP_breg12:
	case DW_OP_breg13:
	case DW_OP_breg14:
	case DW_OP_breg15:
	case DW_OP_breg16:
	case DW_OP_breg17:
	case DW_OP_breg18:
	case DW_OP_breg19:
	case DW_OP_breg20:
	case DW_OP_breg21:
	case DW_OP_breg22:
	case DW_OP_breg23:
	case DW_OP_breg24:
	case DW_OP_breg25:
	case DW_OP_breg26:
	case DW_OP_breg27:
	case DW_OP_breg28:
	case DW_OP_breg29:
	case DW_OP_breg30:
	case DW_OP_breg31:
	  {
	    op_ptr = read_sleb128 (op_ptr, op_end, &offset);
            reg = dwarf2_frame_adjust_regnum (current_gdbarch, op - DW_OP_breg0,
                                              eh_frame_p);
	    result = (ctx->read_reg) (ctx->baton, reg);
	    result += offset;
	  }
	  break;
	case DW_OP_bregx:
	  {
	    op_ptr = read_uleb128 (op_ptr, op_end, &reg);
            reg = dwarf2_frame_adjust_regnum (current_gdbarch, reg, 
                                               eh_frame_p);
	    op_ptr = read_sleb128 (op_ptr, op_end, &offset);
	    result = (ctx->read_reg) (ctx->baton, reg);
	    result += offset;
	  }
	  break;
	case DW_OP_fbreg:
	  {
	    gdb_byte *datastart;
	    size_t datalen;
	    unsigned int before_stack_len;

	    op_ptr = read_sleb128 (op_ptr, op_end, &offset);
	    /* Rather than create a whole new context, we simply
	       record the stack length before execution, then reset it
	       afterwards, effectively erasing whatever the recursive
	       call put there.  */
	    before_stack_len = ctx->stack_len;
	    /* FIXME: cagney/2003-03-26: This code should be using
               get_frame_base_address(), and then implement a dwarf2
               specific this_base method.  */
	    (ctx->get_frame_base) (ctx->baton, &datastart, &datalen);
	    dwarf_expr_eval (ctx, datastart, datalen, eh_frame_p);
	    result = dwarf_expr_fetch (ctx, 0);
	    if (ctx->in_reg)
	      result = (ctx->read_reg) (ctx->baton, result);
	    result = result + offset;
	    ctx->stack_len = before_stack_len;
	    ctx->in_reg = 0;
	  }
	  break;
	case DW_OP_dup:
	  result = dwarf_expr_fetch (ctx, 0);
	  break;

	case DW_OP_drop:
	  dwarf_expr_pop (ctx);
	  goto no_push;

	case DW_OP_pick:
	  offset = *op_ptr++;
	  result = dwarf_expr_fetch (ctx, offset);
	  break;

	case DW_OP_over:
	  result = dwarf_expr_fetch (ctx, 1);
	  break;

	case DW_OP_rot:
	  {
	    CORE_ADDR t1, t2, t3;

	    if (ctx->stack_len < 3)
	       error (_("Not enough elements for DW_OP_rot. Need 3, have %d."),
		      ctx->stack_len);
	    t1 = ctx->stack[ctx->stack_len - 1];
	    t2 = ctx->stack[ctx->stack_len - 2];
	    t3 = ctx->stack[ctx->stack_len - 3];
	    ctx->stack[ctx->stack_len - 1] = t2;
	    ctx->stack[ctx->stack_len - 2] = t3;
	    ctx->stack[ctx->stack_len - 3] = t1;
	    goto no_push;
	  }

	case DW_OP_deref:
	case DW_OP_deref_size:
	case DW_OP_abs:
	case DW_OP_neg:
	case DW_OP_not:
	case DW_OP_plus_uconst:
	  /* Unary operations.  */
	  result = dwarf_expr_fetch (ctx, 0);
	  dwarf_expr_pop (ctx);

	  switch (op)
	    {
	    case DW_OP_deref:
	      {
		gdb_byte *buf = alloca (TARGET_ADDR_BIT / TARGET_CHAR_BIT);
		int bytes_read;

		(ctx->read_mem) (ctx->baton, buf, result,
				 TARGET_ADDR_BIT / TARGET_CHAR_BIT);
		result = dwarf2_read_address (buf,
					      buf + (TARGET_ADDR_BIT
						     / TARGET_CHAR_BIT),
					      &bytes_read);
	      }
	      break;

	    case DW_OP_deref_size:
	      {
		gdb_byte *buf = alloca (TARGET_ADDR_BIT / TARGET_CHAR_BIT);
		int bytes_read;

		(ctx->read_mem) (ctx->baton, buf, result, *op_ptr++);
		result = dwarf2_read_address (buf,
					      buf + (TARGET_ADDR_BIT
						     / TARGET_CHAR_BIT),
					      &bytes_read);
	      }
	      break;

	    case DW_OP_abs:
	      if ((signed int) result < 0)
		result = -result;
	      break;
	    case DW_OP_neg:
	      result = -result;
	      break;
	    case DW_OP_not:
	      result = ~result;
	      break;
	    case DW_OP_plus_uconst:
	      op_ptr = read_uleb128 (op_ptr, op_end, &reg);
	      result += reg;
	      break;
	      /* APPLE LOCAL begin eliminate warning about incomplete switch stmt */
	    default:
	      break;
	      /* APPLE LOCAL end eliminate warning... */
	    }
	  break;

	case DW_OP_and:
	case DW_OP_div:
	case DW_OP_minus:
	case DW_OP_mod:
	case DW_OP_mul:
	case DW_OP_or:
	case DW_OP_plus:
	case DW_OP_shl:
	case DW_OP_shr:
	case DW_OP_shra:
	case DW_OP_xor:
	case DW_OP_le:
	case DW_OP_ge:
	case DW_OP_eq:
	case DW_OP_lt:
	case DW_OP_gt:
	case DW_OP_ne:
	  {
	    /* Binary operations.  Use the value engine to do computations in
	       the right width.  */
	    CORE_ADDR first, second;
	    enum exp_opcode binop;
	    struct value *val1, *val2;

	    second = dwarf_expr_fetch (ctx, 0);
	    dwarf_expr_pop (ctx);

	    first = dwarf_expr_fetch (ctx, 0);
	    dwarf_expr_pop (ctx);

	    val1 = value_from_longest (unsigned_address_type (), first);
	    val2 = value_from_longest (unsigned_address_type (), second);

	    switch (op)
	      {
	      case DW_OP_and:
		binop = BINOP_BITWISE_AND;
		break;
	      case DW_OP_div:
		binop = BINOP_DIV;
                break;
	      case DW_OP_minus:
		binop = BINOP_SUB;
		break;
	      case DW_OP_mod:
		binop = BINOP_MOD;
		break;
	      case DW_OP_mul:
		binop = BINOP_MUL;
		break;
	      case DW_OP_or:
		binop = BINOP_BITWISE_IOR;
		break;
	      case DW_OP_plus:
		binop = BINOP_ADD;
		break;
	      case DW_OP_shl:
		binop = BINOP_LSH;
		break;
	      case DW_OP_shr:
		binop = BINOP_RSH;
                break;
	      case DW_OP_shra:
		binop = BINOP_RSH;
		val1 = value_from_longest (signed_address_type (), first);
		break;
	      case DW_OP_xor:
		binop = BINOP_BITWISE_XOR;
		break;
	      case DW_OP_le:
		binop = BINOP_LEQ;
		break;
	      case DW_OP_ge:
		binop = BINOP_GEQ;
		break;
	      case DW_OP_eq:
		binop = BINOP_EQUAL;
		break;
	      case DW_OP_lt:
		binop = BINOP_LESS;
		break;
	      case DW_OP_gt:
		binop = BINOP_GTR;
		break;
	      case DW_OP_ne:
		binop = BINOP_NOTEQUAL;
		break;
	      default:
		internal_error (__FILE__, __LINE__,
				_("Can't be reached."));
	      }
	    result = value_as_long (value_binop (val1, val2, binop));
	  }
	  break;

	case DW_OP_GNU_push_tls_address:
	  /* Variable is at a constant offset in the thread-local
	  storage block into the objfile for the current thread and
	  the dynamic linker module containing this expression. Here
	  we return returns the offset from that base.  The top of the
	  stack has the offset from the beginning of the thread
	  control block at which the variable is located.  Nothing
	  should follow this operator, so the top of stack would be
	  returned.  */
	  result = dwarf_expr_fetch (ctx, 0);
	  dwarf_expr_pop (ctx);
	  result = (ctx->get_tls_address) (ctx->baton, result);
	  break;

	case DW_OP_skip:
	  offset = extract_signed_integer (op_ptr, 2);
	  op_ptr += 2;
	  op_ptr += offset;
	  goto no_push;

	case DW_OP_bra:
	  offset = extract_signed_integer (op_ptr, 2);
	  op_ptr += 2;
	  if (dwarf_expr_fetch (ctx, 0) != 0)
	    op_ptr += offset;
	  dwarf_expr_pop (ctx);
	  goto no_push;

	case DW_OP_nop:
	  goto no_push;

        case DW_OP_swap:
          {
            CORE_ADDR result1 = dwarf_expr_fetch (ctx, 0);
            dwarf_expr_pop (ctx);
            CORE_ADDR result2 = dwarf_expr_fetch (ctx, 0);
            dwarf_expr_pop (ctx);
            dwarf_expr_push (ctx, result1);
            dwarf_expr_push (ctx, result2);
          }
          goto no_push;

        case DW_OP_piece:
          {
            ULONGEST size;
            CORE_ADDR addr_or_regnum;

            /* APPLE LOCAL: DW_OP_piece requires that a register or address
               be pushed on the stack.  The dwarf_expr_pop () call below will 
               error() if the stack doesn't have something there.
               (NB: The standard allows for a DW_OP_piece operator with NO
                location specified -- this would indicate a variable which
                has been partially optimized away by the compiler.  gcc does
                not emit these today.)

	       gcc-4.0 is generating bad expressions for 64-bit
	       variables in 32-bit programs when location lists are
	       being used.  These bad expressions follow a very
	       regular pattern - they have two DW_OP_piece operators
	       showing the low/high 4-bytes of the 8-byte data,
	       then they have a DW_OP_piece with no address/register
	       specified, then they have another one or two DW_OP_piece
	       operators specifying additional data.  Here is an example:

         TAG_variable [31]  
          AT_name( "loffset" )
          AT_decl_file( 0x01 )
          AT_decl_line( 0x75 )
          AT_type( {0x00055936} ( uint64_t ) )
          AT_location( 0x00019b4c
             0x0003acb8 - 0x0003ad58: reg20, piece 0x0004, reg21, piece 0x0004
             0x0003ad58 - 0x0003b050: reg20, piece 0x0004, reg21, piece 0x0004, piece 0x0008, reg21 , piece 0x0004
             0x0003b050 - 0x0003b118: reg20, piece 0x0004, reg21, piece 0x0004 )

               As a hack, instead of error()ing out here, we will recgonize 
               that we're facing this broken debug info from the compiler
               and stop evaluating this expression at this point.  We've
               already retrieved the full variable location by now.  */

            if (ctx->stack_len == 0)
              return;

            /* Record the piece.  */
            op_ptr = read_uleb128 (op_ptr, op_end, &size);
            addr_or_regnum = dwarf_expr_fetch (ctx, 0);
            add_piece (ctx, ctx->in_reg, addr_or_regnum, size);

            /* Pop off the address/regnum, and clear the in_reg flag.  */
            dwarf_expr_pop (ctx);
            ctx->in_reg = 0;
          }
          goto no_push;

	/* APPLE LOCAL begin variable initialized status  */
	case DW_OP_APPLE_uninit:
#if 0
          /* gcc-4.2 is not outputting trustworthy DW_OP_APPLE_uninit flags; 
             ignore them for now.  */
	  ctx->var_status = 0;
	  /* If the variable is uninitialized, throw an error instead of
	     trying to evaluate it.  */
	  throw_error (GENERIC_ERROR, "Variable is currently uninitialized.");
#endif
	  goto no_push;
	/* APPLE LOCAL end variable initialized status  */

	default:
	  error (_("Unhandled dwarf expression opcode 0x%x"), op);
	}

      /* Most things push a result value.  */
      dwarf_expr_push (ctx, result);
    no_push:;
    }
}

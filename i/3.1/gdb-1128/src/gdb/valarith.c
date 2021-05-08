/* Perform arithmetic and other operations on values, for GDB.

   Copyright 1986, 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995,
   1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005 Free
   Software Foundation, Inc.

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
#include "value.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "expression.h"
#include "target.h"
#include "language.h"
#include "gdb_string.h"
#include "doublest.h"
#include <math.h>
#include "infcall.h"

/* Define whether or not the C operator '/' truncates towards zero for
   differently signed operands (truncation direction is undefined in C). */

#ifndef TRUNCATION_TOWARDS_ZERO
#define TRUNCATION_TOWARDS_ZERO ((-5 / 2) == -2)
#endif

static struct value *value_subscripted_rvalue (struct value *, struct value *);

void _initialize_valarith (void);


/* Given a pointer, return the size of its target.
   If the pointer type is void *, then return 1.
   If the target type is incomplete, then error out.
   This isn't a general purpose function, but just a 
   helper for value_sub & value_add.
*/

static LONGEST
find_size_for_pointer_math (struct type *ptr_type)
{
  LONGEST sz = -1;
  struct type *ptr_target;

  ptr_target = check_typedef (TYPE_TARGET_TYPE (ptr_type));

  sz = TYPE_LENGTH (ptr_target);
  if (sz == 0)
    {
      if (TYPE_CODE (ptr_type) == TYPE_CODE_VOID)
	sz = 1;
      else
	{
	  char *name;
	  
	  name = TYPE_NAME (ptr_target);
	  if (name == NULL)
	    name = TYPE_TAG_NAME (ptr_target);
	  if (name == NULL)
	    error (_("Cannot perform pointer math on incomplete types, "
		   "try casting to a known type, or void *."));
	  else
	    error (_("Cannot perform pointer math on incomplete type \"%s\", "
		   "try casting to a known type, or void *."), name);
	}
    }
  return sz;
}

struct value *
value_add (struct value *arg1, struct value *arg2)
{
  struct value *valint;
  struct value *valptr;
  LONGEST sz;
  struct type *type1, *type2, *valptrtype;

  arg1 = coerce_array (arg1);
  arg2 = coerce_array (arg2);
  type1 = check_typedef (value_type (arg1));
  type2 = check_typedef (value_type (arg2));

  if ((TYPE_CODE (type1) == TYPE_CODE_PTR
       || TYPE_CODE (type2) == TYPE_CODE_PTR)
      &&
      (is_integral_type (type1) || is_integral_type (type2)))
    /* Exactly one argument is a pointer, and one is an integer.  */
    {
      struct value *retval;

      if (TYPE_CODE (type1) == TYPE_CODE_PTR)
	{
	  valptr = arg1;
	  valint = arg2;
	  valptrtype = type1;
	}
      else
	{
	  valptr = arg2;
	  valint = arg1;
	  valptrtype = type2;
	}

      sz = find_size_for_pointer_math (valptrtype);

      retval = value_from_pointer (valptrtype,
				   value_as_address (valptr)
				   + (sz * value_as_long (valint)));
      return retval;
    }

  return value_binop (arg1, arg2, BINOP_ADD);
}

struct value *
value_sub (struct value *arg1, struct value *arg2)
{
  struct type *type1, *type2;
  arg1 = coerce_array (arg1);
  arg2 = coerce_array (arg2);
  type1 = check_typedef (value_type (arg1));
  type2 = check_typedef (value_type (arg2));

  if (TYPE_CODE (type1) == TYPE_CODE_PTR)
    {
      if (is_integral_type (type2))
	{
	  /* pointer - integer.  */
	  LONGEST sz = find_size_for_pointer_math (type1);

	  return value_from_pointer (type1,
				     (value_as_address (arg1)
				      - (sz * value_as_long (arg2))));
	}
      else if (TYPE_CODE (type2) == TYPE_CODE_PTR
	       && TYPE_LENGTH (check_typedef (TYPE_TARGET_TYPE (type1)))
	       == TYPE_LENGTH (check_typedef (TYPE_TARGET_TYPE (type2))))
	{
	  /* pointer to <type x> - pointer to <type x>.  */
	  LONGEST sz = TYPE_LENGTH (check_typedef (TYPE_TARGET_TYPE (type1)));
	  return value_from_longest
	    (builtin_type_long,	/* FIXME -- should be ptrdiff_t */
	     (value_as_long (arg1) - value_as_long (arg2)) / sz);
	}
      else
	{
	  error (_("\
First argument of `-' is a pointer and second argument is neither\n\
an integer nor a pointer of the same type."));
	}
    }

  return value_binop (arg1, arg2, BINOP_SUB);
}

/* Return the value of ARRAY[IDX].
   See comments in value_coerce_array() for rationale for reason for
   doing lower bounds adjustment here rather than there.
   FIXME:  Perhaps we should validate that the index is valid and if
   verbosity is set, warn about invalid indices (but still use them). */

struct value *
value_subscript (struct value *array, struct value *idx)
{
  struct value *bound;
  int c_style = current_language->c_style_arrays;
  struct type *tarray;

  array = coerce_ref (array);
  tarray = check_typedef (value_type (array));

  if (TYPE_CODE (tarray) == TYPE_CODE_ARRAY
      || TYPE_CODE (tarray) == TYPE_CODE_STRING)
    {
      LONGEST lowerbound, upperbound, stride;
      get_array_bounds (tarray, &lowerbound, &upperbound, &stride);

      if (VALUE_LVAL (array) != lval_memory)
	return value_subscripted_rvalue (array, idx);

      if (c_style == 0)
	{
	  LONGEST index = value_as_long (idx);
	  if (index >= lowerbound && index <= upperbound)
	    return value_subscripted_rvalue (array, idx);
  	  /* Emit warning unless we have an array of unknown size.
  	     An array of unknown size has lowerbound 0 and upperbound -1.  */
 	  if ((upperbound != -1) || (lowerbound != 0))
	    warning ("array or string index out of range");
	  /* fall doing C stuff */
	  c_style = 1;
	}

       array = value_coerce_array (array);
 
       /* APPLE LOCAL: Support reverse-indexing of arrays using the
	  'stride' attribute off of the range type of the array. */

       if (stride == 1)
	 {
	   if (lowerbound != 0)
	     {
	       bound = value_from_longest (builtin_type_int, (LONGEST) lowerbound);
	       idx = value_sub (idx, bound);
	     }
	 }
       else if (stride == -1)
	 {
	   if (upperbound != 0)
	     {
	       bound = value_from_longest (builtin_type_int, (LONGEST) upperbound);
	       idx = value_sub (bound, idx);
	     }
	 }
       else
	 internal_error (__FILE__, __LINE__, _("unsupported stride %ld"), stride);
 	  
       return value_ind (value_add (array, idx));
    }

  if (TYPE_CODE (tarray) == TYPE_CODE_BITSTRING)
    {
      struct type *range_type = TYPE_INDEX_TYPE (tarray);
      LONGEST index = value_as_long (idx);
      struct value *v;
      int offset, byte, bit_index;
      LONGEST lowerbound, upperbound;
      get_discrete_bounds (range_type, &lowerbound, &upperbound);
      if (index < lowerbound || index > upperbound)
	error (_("bitstring index out of range"));
      index -= lowerbound;
      offset = index / TARGET_CHAR_BIT;
      byte = *((char *) value_contents (array) + offset);
      bit_index = index % TARGET_CHAR_BIT;
      byte >>= (BITS_BIG_ENDIAN ? TARGET_CHAR_BIT - 1 - bit_index : bit_index);
      v = value_from_longest (LA_BOOL_TYPE, byte & 1);
      set_value_bitpos (v, bit_index);
      set_value_bitsize (v, 1);
      VALUE_LVAL (v) = VALUE_LVAL (array);
      if (VALUE_LVAL (array) == lval_internalvar)
	VALUE_LVAL (v) = lval_internalvar_component;
      VALUE_ADDRESS (v) = VALUE_ADDRESS (array);
      VALUE_FRAME_ID (v) = VALUE_FRAME_ID (array);
      set_value_offset (v, offset + value_offset (array));
      return v;
    }

  if (c_style)
    return value_ind (value_add (array, idx));
  else
    error (_("not an array or string"));
}

/* Return the value of EXPR[IDX], expr an aggregate rvalue
   (eg, a vector register).  This routine used to promote floats
   to doubles, but no longer does.

   APPLE LOCAL: No longer pass in the lower bound explicitly.  Instead
   re-deduce it using the range type of the array.  This cheap,
   cleaner, and necessary to properly support array strides (since now
   we could need either the lower or the upper bound of the array). */

static struct value *
value_subscripted_rvalue (struct value *array, struct value *idx)
{
  struct type *array_type = check_typedef (value_type (array));
  struct type *elt_type = check_typedef (TYPE_TARGET_TYPE (array_type));
  unsigned int elt_size = TYPE_LENGTH (elt_type);
  LONGEST lowerbound, upperbound, stride;
  LONGEST index;
  unsigned int elt_offs;
  struct value *v;

  index = value_as_long (idx);
  get_array_bounds (array_type, &lowerbound, &upperbound, &stride);

  if ((index < lowerbound) || (index > upperbound))
    error (_("no such vector element"));

  if (stride == 1)
    elt_offs = elt_size * longest_to_int (index - lowerbound);  
  else if (stride == -1)
    elt_offs = elt_size * longest_to_int (upperbound - index);
  else
    internal_error (__FILE__, __LINE__, _("unsupported vector stride %ld"), stride);
   
  if (elt_offs >= TYPE_LENGTH (array_type))
    error (_("invalid array offset"));

  v = allocate_value (elt_type);
  if (value_lazy (array))
    set_value_lazy (v, 1);
  else
    memcpy (value_contents_writeable (v),
	    value_contents (array) + elt_offs, elt_size);

  if (VALUE_LVAL (array) == lval_internalvar)
    VALUE_LVAL (v) = lval_internalvar_component;
  else
    VALUE_LVAL (v) = VALUE_LVAL (array);
  VALUE_ADDRESS (v) = VALUE_ADDRESS (array);
  VALUE_REGNUM (v) = VALUE_REGNUM (array);
  VALUE_FRAME_ID (v) = VALUE_FRAME_ID (array);
  set_value_offset (v, value_offset (array) + elt_offs);
  return v;
}

/* Check to see if either argument is a structure.  This is called so
   we know whether to go ahead with the normal binop or look for a 
   user defined function instead.

   For now, we do not overload the `=' operator.  */

int
binop_user_defined_p (enum exp_opcode op, struct value *arg1, struct value *arg2)
{
  struct type *type1, *type2;
  if (op == BINOP_ASSIGN || op == BINOP_CONCAT)
    return 0;
  type1 = check_typedef (value_type (arg1));
  type2 = check_typedef (value_type (arg2));
  return (TYPE_CODE (type1) == TYPE_CODE_STRUCT
	  || TYPE_CODE (type2) == TYPE_CODE_STRUCT
	  || (TYPE_CODE (type1) == TYPE_CODE_REF
	      && TYPE_CODE (TYPE_TARGET_TYPE (type1)) == TYPE_CODE_STRUCT)
	  || (TYPE_CODE (type2) == TYPE_CODE_REF
	      && TYPE_CODE (TYPE_TARGET_TYPE (type2)) == TYPE_CODE_STRUCT));
}

/* Check to see if argument is a structure.  This is called so
   we know whether to go ahead with the normal unop or look for a 
   user defined function instead.

   For now, we do not overload the `&' operator.  */

int
unop_user_defined_p (enum exp_opcode op, struct value *arg1)
{
  struct type *type1;
  if (op == UNOP_ADDR)
    return 0;
  type1 = check_typedef (value_type (arg1));
  for (;;)
    {
      if (TYPE_CODE (type1) == TYPE_CODE_STRUCT)
	return 1;
      else if (TYPE_CODE (type1) == TYPE_CODE_REF)
	type1 = TYPE_TARGET_TYPE (type1);
      else
	return 0;
    }
}

/* We know either arg1 or arg2 is a structure, so try to find the right
   user defined function.  Create an argument vector that calls 
   arg1.operator @ (arg1,arg2) and return that value (where '@' is any
   binary operator which is legal for GNU C++).

   OP is the operatore, and if it is BINOP_ASSIGN_MODIFY, then OTHEROP
   is the opcode saying how to modify it.  Otherwise, OTHEROP is
   unused.  */

struct value *
value_x_binop (struct value *arg1, struct value *arg2, enum exp_opcode op,
	       enum exp_opcode otherop, enum noside noside)
{
  struct value **argvec;
  char *ptr;
  char tstr[13];
  int static_memfuncp;

  arg1 = coerce_ref (arg1);
  arg2 = coerce_ref (arg2);
  arg1 = coerce_enum (arg1);
  arg2 = coerce_enum (arg2);

  /* now we know that what we have to do is construct our
     arg vector and find the right function to call it with.  */

  if (TYPE_CODE (check_typedef (value_type (arg1))) != TYPE_CODE_STRUCT)
    error (_("Can't do that binary op on that type"));	/* FIXME be explicit */

  argvec = (struct value **) alloca (sizeof (struct value *) * 4);
  argvec[1] = value_addr (arg1);
  argvec[2] = arg2;
  argvec[3] = 0;

  /* make the right function name up */
  strcpy (tstr, "operator__");
  ptr = tstr + 8;
  switch (op)
    {
    case BINOP_ADD:
      strcpy (ptr, "+");
      break;
    case BINOP_SUB:
      strcpy (ptr, "-");
      break;
    case BINOP_MUL:
      strcpy (ptr, "*");
      break;
    case BINOP_DIV:
      strcpy (ptr, "/");
      break;
    case BINOP_REM:
      strcpy (ptr, "%");
      break;
    case BINOP_LSH:
      strcpy (ptr, "<<");
      break;
    case BINOP_RSH:
      strcpy (ptr, ">>");
      break;
    case BINOP_BITWISE_AND:
      strcpy (ptr, "&");
      break;
    case BINOP_BITWISE_IOR:
      strcpy (ptr, "|");
      break;
    case BINOP_BITWISE_XOR:
      strcpy (ptr, "^");
      break;
    case BINOP_LOGICAL_AND:
      strcpy (ptr, "&&");
      break;
    case BINOP_LOGICAL_OR:
      strcpy (ptr, "||");
      break;
    case BINOP_MIN:
      strcpy (ptr, "<?");
      break;
    case BINOP_MAX:
      strcpy (ptr, ">?");
      break;
    case BINOP_ASSIGN:
      strcpy (ptr, "=");
      break;
    case BINOP_ASSIGN_MODIFY:
      switch (otherop)
	{
	case BINOP_ADD:
	  strcpy (ptr, "+=");
	  break;
	case BINOP_SUB:
	  strcpy (ptr, "-=");
	  break;
	case BINOP_MUL:
	  strcpy (ptr, "*=");
	  break;
	case BINOP_DIV:
	  strcpy (ptr, "/=");
	  break;
	case BINOP_REM:
	  strcpy (ptr, "%=");
	  break;
	case BINOP_BITWISE_AND:
	  strcpy (ptr, "&=");
	  break;
	case BINOP_BITWISE_IOR:
	  strcpy (ptr, "|=");
	  break;
	case BINOP_BITWISE_XOR:
	  strcpy (ptr, "^=");
	  break;
	case BINOP_MOD:	/* invalid */
	default:
	  error (_("Invalid binary operation specified."));
	}
      break;
    case BINOP_SUBSCRIPT:
      strcpy (ptr, "[]");
      break;
    case BINOP_EQUAL:
      strcpy (ptr, "==");
      break;
    case BINOP_NOTEQUAL:
      strcpy (ptr, "!=");
      break;
    case BINOP_LESS:
      strcpy (ptr, "<");
      break;
    case BINOP_GTR:
      strcpy (ptr, ">");
      break;
    case BINOP_GEQ:
      strcpy (ptr, ">=");
      break;
    case BINOP_LEQ:
      strcpy (ptr, "<=");
      break;
    case BINOP_MOD:		/* invalid */
    default:
      error (_("Invalid binary operation specified."));
    }

  argvec[0] = value_struct_elt (&arg1, argvec + 1, tstr, &static_memfuncp, "structure");

  if (argvec[0])
    {
      if (static_memfuncp)
	{
	  argvec[1] = argvec[0];
	  argvec++;
	}
      if (noside == EVAL_AVOID_SIDE_EFFECTS)
	{
	  struct type *return_type;
	  return_type
	    = TYPE_TARGET_TYPE (check_typedef (value_type (argvec[0])));
	  return value_zero (return_type, VALUE_LVAL (arg1));
	}
      return call_function_by_hand (argvec[0], 2 - static_memfuncp, argvec + 1);
    }
  error (_("member function %s not found"), tstr);
#ifdef lint
  return call_function_by_hand (argvec[0], 2 - static_memfuncp, argvec + 1);
#endif
}

/* We know that arg1 is a structure, so try to find a unary user
   defined operator that matches the operator in question.  
   Create an argument vector that calls arg1.operator @ (arg1)
   and return that value (where '@' is (almost) any unary operator which
   is legal for GNU C++).  */

struct value *
value_x_unop (struct value *arg1, enum exp_opcode op, enum noside noside)
{
  struct value **argvec;
  char *ptr, *mangle_ptr;
  char tstr[13], mangle_tstr[13];
  int static_memfuncp, nargs;

  arg1 = coerce_ref (arg1);
  arg1 = coerce_enum (arg1);

  /* now we know that what we have to do is construct our
     arg vector and find the right function to call it with.  */

  if (TYPE_CODE (check_typedef (value_type (arg1))) != TYPE_CODE_STRUCT)
    error (_("Can't do that unary op on that type"));	/* FIXME be explicit */

  argvec = (struct value **) alloca (sizeof (struct value *) * 4);
  argvec[1] = value_addr (arg1);
  argvec[2] = 0;

  nargs = 1;

  /* make the right function name up */
  strcpy (tstr, "operator__");
  ptr = tstr + 8;
  strcpy (mangle_tstr, "__");
  mangle_ptr = mangle_tstr + 2;
  switch (op)
    {
    case UNOP_PREINCREMENT:
      strcpy (ptr, "++");
      break;
    case UNOP_PREDECREMENT:
      strcpy (ptr, "--");
      break;
    case UNOP_POSTINCREMENT:
      strcpy (ptr, "++");
      argvec[2] = value_from_longest (builtin_type_int, 0);
      argvec[3] = 0;
      nargs ++;
      break;
    case UNOP_POSTDECREMENT:
      strcpy (ptr, "--");
      argvec[2] = value_from_longest (builtin_type_int, 0);
      argvec[3] = 0;
      nargs ++;
      break;
    case UNOP_LOGICAL_NOT:
      strcpy (ptr, "!");
      break;
    case UNOP_COMPLEMENT:
      strcpy (ptr, "~");
      break;
    case UNOP_NEG:
      strcpy (ptr, "-");
      break;
    case UNOP_PLUS:
      strcpy (ptr, "+");
      break;
    case UNOP_IND:
      strcpy (ptr, "*");
      break;
    default:
      error (_("Invalid unary operation specified."));
    }

  argvec[0] = value_struct_elt (&arg1, argvec + 1, tstr, &static_memfuncp, "structure");

  if (argvec[0])
    {
      if (static_memfuncp)
	{
	  argvec[1] = argvec[0];
	  nargs --;
	  argvec++;
	}
      if (noside == EVAL_AVOID_SIDE_EFFECTS)
	{
	  struct type *return_type;
	  return_type
	    = TYPE_TARGET_TYPE (check_typedef (value_type (argvec[0])));
	  return value_zero (return_type, VALUE_LVAL (arg1));
	}
      return call_function_by_hand (argvec[0], nargs, argvec + 1);
    }
  error (_("member function %s not found"), tstr);
  return 0;			/* For lint -- never reached */
}


/* Concatenate two values with the following conditions:

   (1)  Both values must be either bitstring values or character string
   values and the resulting value consists of the concatenation of
   ARG1 followed by ARG2.

   or

   One value must be an integer value and the other value must be
   either a bitstring value or character string value, which is
   to be repeated by the number of times specified by the integer
   value.


   (2)  Boolean values are also allowed and are treated as bit string
   values of length 1.

   (3)  Character values are also allowed and are treated as character
   string values of length 1.
 */

struct value *
value_concat (struct value *arg1, struct value *arg2)
{
  struct value *inval1;
  struct value *inval2;
  struct value *outval = NULL;
  int inval1len, inval2len;
  int count, idx;
  char *ptr;
  char inchar;
  struct type *type1 = check_typedef (value_type (arg1));
  struct type *type2 = check_typedef (value_type (arg2));

  /* First figure out if we are dealing with two values to be concatenated
     or a repeat count and a value to be repeated.  INVAL1 is set to the
     first of two concatenated values, or the repeat count.  INVAL2 is set
     to the second of the two concatenated values or the value to be 
     repeated. */

  if (TYPE_CODE (type2) == TYPE_CODE_INT)
    {
      struct type *tmp = type1;
      type1 = tmp;
      tmp = type2;
      inval1 = arg2;
      inval2 = arg1;
    }
  else
    {
      inval1 = arg1;
      inval2 = arg2;
    }

  /* Now process the input values. */

  if (TYPE_CODE (type1) == TYPE_CODE_INT)
    {
      /* We have a repeat count.  Validate the second value and then
         construct a value repeated that many times. */
      if (TYPE_CODE (type2) == TYPE_CODE_STRING
	  || TYPE_CODE (type2) == TYPE_CODE_CHAR)
	{
	  count = longest_to_int (value_as_long (inval1));
	  inval2len = TYPE_LENGTH (type2);
	  ptr = (char *) alloca (count * inval2len);
	  if (TYPE_CODE (type2) == TYPE_CODE_CHAR)
	    {
	      inchar = (char) unpack_long (type2,
					   value_contents (inval2));
	      for (idx = 0; idx < count; idx++)
		{
		  *(ptr + idx) = inchar;
		}
	    }
	  else
	    {
	      for (idx = 0; idx < count; idx++)
		{
		  memcpy (ptr + (idx * inval2len), value_contents (inval2),
			  inval2len);
		}
	    }
	  outval = value_string (ptr, count * inval2len);
	}
      else if (TYPE_CODE (type2) == TYPE_CODE_BITSTRING
	       || TYPE_CODE (type2) == TYPE_CODE_BOOL)
	{
	  error (_("unimplemented support for bitstring/boolean repeats"));
	}
      else
	{
	  error (_("can't repeat values of that type"));
	}
    }
  else if (TYPE_CODE (type1) == TYPE_CODE_STRING
	   || TYPE_CODE (type1) == TYPE_CODE_CHAR)
    {
      /* We have two character strings to concatenate. */
      if (TYPE_CODE (type2) != TYPE_CODE_STRING
	  && TYPE_CODE (type2) != TYPE_CODE_CHAR)
	{
	  error (_("Strings can only be concatenated with other strings."));
	}
      inval1len = TYPE_LENGTH (type1);
      inval2len = TYPE_LENGTH (type2);
      ptr = (char *) alloca (inval1len + inval2len);
      if (TYPE_CODE (type1) == TYPE_CODE_CHAR)
	{
	  *ptr = (char) unpack_long (type1, value_contents (inval1));
	}
      else
	{
	  memcpy (ptr, value_contents (inval1), inval1len);
	}
      if (TYPE_CODE (type2) == TYPE_CODE_CHAR)
	{
	  *(ptr + inval1len) =
	    (char) unpack_long (type2, value_contents (inval2));
	}
      else
	{
	  memcpy (ptr + inval1len, value_contents (inval2), inval2len);
	}
      outval = value_string (ptr, inval1len + inval2len);
    }
  else if (TYPE_CODE (type1) == TYPE_CODE_BITSTRING
	   || TYPE_CODE (type1) == TYPE_CODE_BOOL)
    {
      /* We have two bitstrings to concatenate. */
      if (TYPE_CODE (type2) != TYPE_CODE_BITSTRING
	  && TYPE_CODE (type2) != TYPE_CODE_BOOL)
	{
	  error (_("Bitstrings or booleans can only be concatenated with other bitstrings or booleans."));
	}
      error (_("unimplemented support for bitstring/boolean concatenation."));
    }
  else
    {
      /* We don't know how to concatenate these operands. */
      error (_("illegal operands for concatenation."));
    }
  return (outval);
}



/* Perform a binary operation on two operands which have reasonable
   representations as integers or floats.  This includes booleans,
   characters, integers, or floats.
   Does not support addition and subtraction on pointers;
   use value_add or value_sub if you want to handle those possibilities.  */

struct value *
value_binop (struct value *arg1, struct value *arg2, enum exp_opcode op)
{
  struct value *val;
  struct type *type1, *type2;

  arg1 = coerce_ref (arg1);
  arg2 = coerce_ref (arg2);
  type1 = check_typedef (value_type (arg1));
  type2 = check_typedef (value_type (arg2));

  if ((TYPE_CODE (type1) != TYPE_CODE_FLT && !is_integral_type (type1))
      ||
      (TYPE_CODE (type2) != TYPE_CODE_FLT && !is_integral_type (type2)))
    error (_("Argument to arithmetic operation not a number or boolean."));

  if (TYPE_CODE (type1) == TYPE_CODE_FLT
      ||
      TYPE_CODE (type2) == TYPE_CODE_FLT)
    {
      /* FIXME-if-picky-about-floating-accuracy: Should be doing this
         in target format.  real.c in GCC probably has the necessary
         code.  */
      DOUBLEST v1, v2, v = 0;
      v1 = value_as_double (arg1);
      v2 = value_as_double (arg2);
      switch (op)
	{
	case BINOP_ADD:
	  v = v1 + v2;
	  break;

	case BINOP_SUB:
	  v = v1 - v2;
	  break;

	case BINOP_MUL:
	  v = v1 * v2;
	  break;

	case BINOP_DIV:
	  v = v1 / v2;
	  break;

	case BINOP_EXP:
	  errno = 0;
	  v = pow (v1, v2);
	  if (errno)
	    error (_("Cannot perform exponentiation: %s"), safe_strerror (errno));
	  break;

	default:
	  error (_("Integer-only operation on floating point number."));
	}

      /* If either arg was long double, make sure that value is also long
         double.  */

      if (TYPE_LENGTH (type1) * 8 > TARGET_DOUBLE_BIT
	  || TYPE_LENGTH (type2) * 8 > TARGET_DOUBLE_BIT)
	val = allocate_value (builtin_type_long_double);
      else
	val = allocate_value (builtin_type_double);

      store_typed_floating (value_contents_raw (val), value_type (val), v);
    }
  else if (TYPE_CODE (type1) == TYPE_CODE_BOOL
	   &&
	   TYPE_CODE (type2) == TYPE_CODE_BOOL)
    {
      LONGEST v1, v2, v = 0;
      v1 = value_as_long (arg1);
      v2 = value_as_long (arg2);

      switch (op)
	{
	case BINOP_BITWISE_AND:
	  v = v1 & v2;
	  break;

	case BINOP_BITWISE_IOR:
	  v = v1 | v2;
	  break;

	case BINOP_BITWISE_XOR:
	  v = v1 ^ v2;
          break;
              
        case BINOP_EQUAL:
          v = v1 == v2;
          break;
          
        case BINOP_NOTEQUAL:
          v = v1 != v2;
	  break;

	default:
	  error (_("Invalid operation on booleans."));
	}

      val = allocate_value (type1);
      store_signed_integer (value_contents_raw (val),
			    TYPE_LENGTH (type1),
			    v);
    }
  else
    /* Integral operations here.  */
    /* FIXME:  Also mixed integral/booleans, with result an integer. */
    /* FIXME: This implements ANSI C rules (also correct for C++).
       What about FORTRAN and (the deleted) chill ?  */
    {
      unsigned int promoted_len1 = TYPE_LENGTH (type1);
      unsigned int promoted_len2 = TYPE_LENGTH (type2);
      int is_unsigned1 = TYPE_UNSIGNED (type1);
      int is_unsigned2 = TYPE_UNSIGNED (type2);
      unsigned int result_len;
      int unsigned_operation;

      /* Determine type length and signedness after promotion for
         both operands.  */
      if (promoted_len1 < TYPE_LENGTH (builtin_type_int))
	{
	  is_unsigned1 = 0;
	  promoted_len1 = TYPE_LENGTH (builtin_type_int);
	}
      if (promoted_len2 < TYPE_LENGTH (builtin_type_int))
	{
	  is_unsigned2 = 0;
	  promoted_len2 = TYPE_LENGTH (builtin_type_int);
	}

      /* Determine type length of the result, and if the operation should
         be done unsigned.
         Use the signedness of the operand with the greater length.
         If both operands are of equal length, use unsigned operation
         if one of the operands is unsigned.  */
      if (op == BINOP_RSH || op == BINOP_LSH)
	{
	  /* In case of the shift operators the type of the result only
	     depends on the type of the left operand.  */
	  unsigned_operation = is_unsigned1;
	  result_len = promoted_len1;
	}
      else if (promoted_len1 > promoted_len2)
	{
	  unsigned_operation = is_unsigned1;
	  result_len = promoted_len1;
	}
      else if (promoted_len2 > promoted_len1)
	{
	  unsigned_operation = is_unsigned2;
	  result_len = promoted_len2;
	}
      else
	{
	  unsigned_operation = is_unsigned1 || is_unsigned2;
	  result_len = promoted_len1;
	}

      if (unsigned_operation)
	{
	  ULONGEST v1, v2, v = 0;
	  v1 = (ULONGEST) value_as_long (arg1);
	  v2 = (ULONGEST) value_as_long (arg2);

	  /* Truncate values to the type length of the result.  */
	  if (result_len < sizeof (ULONGEST))
	    {
	      v1 &= ((LONGEST) 1 << HOST_CHAR_BIT * result_len) - 1;
	      v2 &= ((LONGEST) 1 << HOST_CHAR_BIT * result_len) - 1;
	    }

	  switch (op)
	    {
	    case BINOP_ADD:
	      v = v1 + v2;
	      break;

	    case BINOP_SUB:
	      v = v1 - v2;
	      break;

	    case BINOP_MUL:
	      v = v1 * v2;
	      break;

	    case BINOP_DIV:
	      v = v1 / v2;
	      break;

	    case BINOP_EXP:
	      errno = 0;
	      v = pow (v1, v2);
	      if (errno)
		error (_("Cannot perform exponentiation: %s"), safe_strerror (errno));
	      break;

	    case BINOP_REM:
	      v = v1 % v2;
	      break;

	    case BINOP_MOD:
	      /* Knuth 1.2.4, integer only.  Note that unlike the C '%' op,
	         v1 mod 0 has a defined value, v1. */
	      if (v2 == 0)
		{
		  v = v1;
		}
	      else
		{
		  v = v1 / v2;
		  /* Note floor(v1/v2) == v1/v2 for unsigned. */
		  v = v1 - (v2 * v);
		}
	      break;

	    case BINOP_LSH:
	      v = v1 << v2;
	      break;

	    case BINOP_RSH:
	      v = v1 >> v2;
	      break;

	    case BINOP_BITWISE_AND:
	      v = v1 & v2;
	      break;

	    case BINOP_BITWISE_IOR:
	      v = v1 | v2;
	      break;

	    case BINOP_BITWISE_XOR:
	      v = v1 ^ v2;
	      break;

	    case BINOP_LOGICAL_AND:
	      v = v1 && v2;
	      break;

	    case BINOP_LOGICAL_OR:
	      v = v1 || v2;
	      break;

	    case BINOP_MIN:
	      v = v1 < v2 ? v1 : v2;
	      break;

	    case BINOP_MAX:
	      v = v1 > v2 ? v1 : v2;
	      break;

	    case BINOP_EQUAL:
	      v = v1 == v2;
	      break;

            case BINOP_NOTEQUAL:
              v = v1 != v2;
              break;

	    case BINOP_LESS:
	      v = v1 < v2;
	      break;

	    default:
	      error (_("Invalid binary operation on numbers."));
	    }

	  /* This is a kludge to get around the fact that we don't
	     know how to determine the result type from the types of
	     the operands.  (I'm not really sure how much we feel the
	     need to duplicate the exact rules of the current
	     language.  They can get really hairy.  But not to do so
	     makes it hard to document just what we *do* do).  */

	  /* Can't just call init_type because we wouldn't know what
	     name to give the type.  */
	  val = allocate_value
	    (result_len > TARGET_LONG_BIT / HOST_CHAR_BIT
	     ? builtin_type_unsigned_long_long
	     : builtin_type_unsigned_long);
	  store_unsigned_integer (value_contents_raw (val),
				  TYPE_LENGTH (value_type (val)),
				  v);
	}
      else
	{
	  LONGEST v1, v2, v = 0;
	  v1 = value_as_long (arg1);
	  v2 = value_as_long (arg2);

	  switch (op)
	    {
	    case BINOP_ADD:
	      v = v1 + v2;
	      break;

	    case BINOP_SUB:
	      v = v1 - v2;
	      break;

	    case BINOP_MUL:
	      v = v1 * v2;
	      break;

	    case BINOP_DIV:
	      if (v2 != 0)
		v = v1 / v2;
	      else
		error (_("Division by zero"));
              break;

	    case BINOP_EXP:
	      errno = 0;
	      v = pow (v1, v2);
	      if (errno)
		error (_("Cannot perform exponentiation: %s"), safe_strerror (errno));
	      break;

	    case BINOP_REM:
	      if (v2 != 0)
		v = v1 % v2;
	      else
		error (_("Division by zero"));
	      break;

	    case BINOP_MOD:
	      /* Knuth 1.2.4, integer only.  Note that unlike the C '%' op,
	         X mod 0 has a defined value, X. */
	      if (v2 == 0)
		{
		  v = v1;
		}
	      else
		{
		  v = v1 / v2;
		  /* Compute floor. */
		  if (TRUNCATION_TOWARDS_ZERO && (v < 0) && ((v1 % v2) != 0))
		    {
		      v--;
		    }
		  v = v1 - (v2 * v);
		}
	      break;

	    case BINOP_LSH:
	      v = v1 << v2;
	      break;

	    case BINOP_RSH:
	      v = v1 >> v2;
	      break;

	    case BINOP_BITWISE_AND:
	      v = v1 & v2;
	      break;

	    case BINOP_BITWISE_IOR:
	      v = v1 | v2;
	      break;

	    case BINOP_BITWISE_XOR:
	      v = v1 ^ v2;
	      break;

	    case BINOP_LOGICAL_AND:
	      v = v1 && v2;
	      break;

	    case BINOP_LOGICAL_OR:
	      v = v1 || v2;
	      break;

	    case BINOP_MIN:
	      v = v1 < v2 ? v1 : v2;
	      break;

	    case BINOP_MAX:
	      v = v1 > v2 ? v1 : v2;
	      break;

	    case BINOP_EQUAL:
	      v = v1 == v2;
	      break;

	    case BINOP_LESS:
	      v = v1 < v2;
	      break;

	    default:
	      error (_("Invalid binary operation on numbers."));
	    }

	  /* This is a kludge to get around the fact that we don't
	     know how to determine the result type from the types of
	     the operands.  (I'm not really sure how much we feel the
	     need to duplicate the exact rules of the current
	     language.  They can get really hairy.  But not to do so
	     makes it hard to document just what we *do* do).  */

	  /* Can't just call init_type because we wouldn't know what
	     name to give the type.  */
	  val = allocate_value
	    (result_len > TARGET_LONG_BIT / HOST_CHAR_BIT
	     ? builtin_type_long_long
	     : builtin_type_long);
	  store_signed_integer (value_contents_raw (val),
				TYPE_LENGTH (value_type (val)),
				v);
	}
    }

  return val;
}

/* Simulate the C operator ! -- return 1 if ARG1 contains zero.  */

int
value_logical_not (struct value *arg1)
{
  int len;
  const gdb_byte *p;
  struct type *type1;

  arg1 = coerce_number (arg1);
  type1 = check_typedef (value_type (arg1));

  if (TYPE_CODE (type1) == TYPE_CODE_FLT)
    return 0 == value_as_double (arg1);

  len = TYPE_LENGTH (type1);
  p = value_contents (arg1);

  while (--len >= 0)
    {
      if (*p++)
	break;
    }

  return len < 0;
}

/* Perform a comparison on two string values (whose content are not
   necessarily null terminated) based on their length */

static int
value_strcmp (struct value *arg1, struct value *arg2)
{
  int len1 = TYPE_LENGTH (value_type (arg1));
  int len2 = TYPE_LENGTH (value_type (arg2));
  const gdb_byte *s1 = value_contents (arg1);
  const gdb_byte *s2 = value_contents (arg2);
  int i, len = len1 < len2 ? len1 : len2;

  for (i = 0; i < len; i++)
    {
      if (s1[i] < s2[i])
        return -1;
      else if (s1[i] > s2[i])
        return 1;
      else
        continue;
    }

  if (len1 < len2)
    return -1;
  else if (len1 > len2)
    return 1;
  else
    return 0;
}

/* Simulate the C operator == by returning a 1
   iff ARG1 and ARG2 have equal contents.  */

int
value_equal (struct value *arg1, struct value *arg2)
{
  int len;
  const gdb_byte *p1;
  const gdb_byte *p2;
  struct type *type1, *type2;
  enum type_code code1;
  enum type_code code2;
  int is_int1, is_int2;

  arg1 = coerce_array (arg1);
  arg2 = coerce_array (arg2);

  type1 = check_typedef (value_type (arg1));
  type2 = check_typedef (value_type (arg2));
  code1 = TYPE_CODE (type1);
  code2 = TYPE_CODE (type2);
  is_int1 = is_integral_type (type1);
  is_int2 = is_integral_type (type2);

  if (is_int1 && is_int2)
    return longest_to_int (value_as_long (value_binop (arg1, arg2,
						       BINOP_EQUAL)));
  else if ((code1 == TYPE_CODE_FLT || is_int1)
	   && (code2 == TYPE_CODE_FLT || is_int2))
    return value_as_double (arg1) == value_as_double (arg2);

  /* FIXME: Need to promote to either CORE_ADDR or LONGEST, whichever
     is bigger.  */
  else if (code1 == TYPE_CODE_PTR && is_int2)
    return value_as_address (arg1) == (CORE_ADDR) value_as_long (arg2);
  else if (code2 == TYPE_CODE_PTR && is_int1)
    return (CORE_ADDR) value_as_long (arg1) == value_as_address (arg2);

  else if (code1 == code2
	   && ((len = (int) TYPE_LENGTH (type1))
	       == (int) TYPE_LENGTH (type2)))
    {
      p1 = value_contents (arg1);
      p2 = value_contents (arg2);
      while (--len >= 0)
	{
	  if (*p1++ != *p2++)
	    break;
	}
      return len < 0;
    }
  else if (code1 == TYPE_CODE_STRING && code2 == TYPE_CODE_STRING)
    {
      return value_strcmp (arg1, arg2) == 0;
    }
  else
    {
      error (_("Invalid type combination in equality test."));
      return 0;			/* For lint -- never reached */
    }
}

/* Simulate the C operator < by returning 1
   iff ARG1's contents are less than ARG2's.  */

int
value_less (struct value *arg1, struct value *arg2)
{
  enum type_code code1;
  enum type_code code2;
  struct type *type1, *type2;
  int is_int1, is_int2;

  arg1 = coerce_array (arg1);
  arg2 = coerce_array (arg2);

  type1 = check_typedef (value_type (arg1));
  type2 = check_typedef (value_type (arg2));
  code1 = TYPE_CODE (type1);
  code2 = TYPE_CODE (type2);
  is_int1 = is_integral_type (type1);
  is_int2 = is_integral_type (type2);

  if (is_int1 && is_int2)
    return longest_to_int (value_as_long (value_binop (arg1, arg2,
						       BINOP_LESS)));
  else if ((code1 == TYPE_CODE_FLT || is_int1)
	   && (code2 == TYPE_CODE_FLT || is_int2))
    return value_as_double (arg1) < value_as_double (arg2);
  else if (code1 == TYPE_CODE_PTR && code2 == TYPE_CODE_PTR)
    return value_as_address (arg1) < value_as_address (arg2);

  /* FIXME: Need to promote to either CORE_ADDR or LONGEST, whichever
     is bigger.  */
  else if (code1 == TYPE_CODE_PTR && is_int2)
    return value_as_address (arg1) < (CORE_ADDR) value_as_long (arg2);
  else if (code2 == TYPE_CODE_PTR && is_int1)
    return (CORE_ADDR) value_as_long (arg1) < value_as_address (arg2);
  else if (code1 == TYPE_CODE_STRING && code2 == TYPE_CODE_STRING)
    return value_strcmp (arg1, arg2) < 0;
  else
    {
      error (_("Invalid type combination in ordering comparison."));
      return 0;
    }
}

/* The unary operators +, - and ~.  They free the argument ARG1.  */

struct value *
value_pos (struct value *arg1)
{
  struct type *type;

  arg1 = coerce_ref (arg1);

  type = check_typedef (value_type (arg1));

  if (TYPE_CODE (type) == TYPE_CODE_FLT)
    return value_from_double (type, value_as_double (arg1));
  else if (is_integral_type (type))
    {
      /* Perform integral promotion for ANSI C/C++.  FIXME: What about
         FORTRAN and (the deleted) chill ?  */
      if (TYPE_LENGTH (type) < TYPE_LENGTH (builtin_type_int))
	type = builtin_type_int;

      return value_from_longest (type, value_as_long (arg1));
    }
  else
    {
      error ("Argument to positive operation not a number.");
      return 0;			/* For lint -- never reached */
    }
}

struct value *
value_neg (struct value *arg1)
{
  struct type *type;
  struct type *result_type = value_type (arg1);

  arg1 = coerce_ref (arg1);

  type = check_typedef (value_type (arg1));

  if (TYPE_CODE (type) == TYPE_CODE_FLT)
    return value_from_double (result_type, -value_as_double (arg1));
  else if (is_integral_type (type))
    {
      /* Perform integral promotion for ANSI C/C++.  FIXME: What about
         FORTRAN and (the deleted) chill ?  */
      if (TYPE_LENGTH (type) < TYPE_LENGTH (builtin_type_int))
	result_type = builtin_type_int;

      return value_from_longest (result_type, -value_as_long (arg1));
    }
  else
    {
      error (_("Argument to negate operation not a number."));
      return 0;			/* For lint -- never reached */
    }
}

struct value *
value_complement (struct value *arg1)
{
  struct type *type;
  struct type *result_type = value_type (arg1);

  arg1 = coerce_ref (arg1);

  type = check_typedef (value_type (arg1));

  if (!is_integral_type (type))
    error (_("Argument to complement operation not an integer or boolean."));

  /* Perform integral promotion for ANSI C/C++.
     FIXME: What about FORTRAN ?  */
  if (TYPE_LENGTH (type) < TYPE_LENGTH (builtin_type_int))
    result_type = builtin_type_int;

  return value_from_longest (result_type, ~value_as_long (arg1));
}

/* The INDEX'th bit of SET value whose value_type is TYPE,
   and whose value_contents is valaddr.
   Return -1 if out of range, -2 other error. */

int
value_bit_index (struct type *type, const gdb_byte *valaddr, int index)
{
  LONGEST low_bound, high_bound;
  LONGEST word;
  unsigned rel_index;
  struct type *range = TYPE_FIELD_TYPE (type, 0);
  if (get_discrete_bounds (range, &low_bound, &high_bound) < 0)
    return -2;
  if (index < low_bound || index > high_bound)
    return -1;
  rel_index = index - low_bound;
  word = unpack_long (builtin_type_unsigned_char,
		      valaddr + (rel_index / TARGET_CHAR_BIT));
  rel_index %= TARGET_CHAR_BIT;
  if (BITS_BIG_ENDIAN)
    rel_index = TARGET_CHAR_BIT - 1 - rel_index;
  return (word >> rel_index) & 1;
}

struct value *
value_in (struct value *element, struct value *set)
{
  int member;
  struct type *settype = check_typedef (value_type (set));
  struct type *eltype = check_typedef (value_type (element));
  if (TYPE_CODE (eltype) == TYPE_CODE_RANGE)
    eltype = TYPE_TARGET_TYPE (eltype);
  if (TYPE_CODE (settype) != TYPE_CODE_SET)
    error (_("Second argument of 'IN' has wrong type"));
  if (TYPE_CODE (eltype) != TYPE_CODE_INT
      && TYPE_CODE (eltype) != TYPE_CODE_CHAR
      && TYPE_CODE (eltype) != TYPE_CODE_ENUM
      && TYPE_CODE (eltype) != TYPE_CODE_BOOL)
    error (_("First argument of 'IN' has wrong type"));
  member = value_bit_index (settype, value_contents (set),
			    value_as_long (element));
  if (member < 0)
    error (_("First argument of 'IN' not in range"));
  return value_from_longest (LA_BOOL_TYPE, member);
}

void
_initialize_valarith (void)
{
}

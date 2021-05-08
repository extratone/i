/* Expression translation
   Copyright (C) 2002, 2003, 2004, 2005, 2006 Free Software Foundation, Inc.
   Contributed by Paul Brook <paul@nowt.org>
   and Steven Bosscher <s.bosscher@student.tudelft.nl>

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

/* trans-expr.c-- generate GENERIC trees for gfc_expr.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tree.h"
#include "convert.h"
#include "ggc.h"
#include "toplev.h"
#include "real.h"
#include "tree-gimple.h"
#include "langhooks.h"
#include "flags.h"
#include "gfortran.h"
#include "trans.h"
#include "trans-const.h"
#include "trans-types.h"
#include "trans-array.h"
/* Only for gfc_trans_assign and gfc_trans_pointer_assign.  */
#include "trans-stmt.h"
#include "dependency.h"

static tree gfc_trans_structure_assign (tree dest, gfc_expr * expr);
static void gfc_apply_interface_mapping_to_expr (gfc_interface_mapping *,
						 gfc_expr *);

/* Copy the scalarization loop variables.  */

static void
gfc_copy_se_loopvars (gfc_se * dest, gfc_se * src)
{
  dest->ss = src->ss;
  dest->loop = src->loop;
}


/* Initialize a simple expression holder.

   Care must be taken when multiple se are created with the same parent.
   The child se must be kept in sync.  The easiest way is to delay creation
   of a child se until after after the previous se has been translated.  */

void
gfc_init_se (gfc_se * se, gfc_se * parent)
{
  memset (se, 0, sizeof (gfc_se));
  gfc_init_block (&se->pre);
  gfc_init_block (&se->post);

  se->parent = parent;

  if (parent)
    gfc_copy_se_loopvars (se, parent);
}


/* Advances to the next SS in the chain.  Use this rather than setting
   se->ss = se->ss->next because all the parents needs to be kept in sync.
   See gfc_init_se.  */

void
gfc_advance_se_ss_chain (gfc_se * se)
{
  gfc_se *p;

  gcc_assert (se != NULL && se->ss != NULL && se->ss != gfc_ss_terminator);

  p = se;
  /* Walk down the parent chain.  */
  while (p != NULL)
    {
      /* Simple consistency check.  */
      gcc_assert (p->parent == NULL || p->parent->ss == p->ss);

      p->ss = p->ss->next;

      p = p->parent;
    }
}


/* Ensures the result of the expression as either a temporary variable
   or a constant so that it can be used repeatedly.  */

void
gfc_make_safe_expr (gfc_se * se)
{
  tree var;

  if (CONSTANT_CLASS_P (se->expr))
    return;

  /* We need a temporary for this result.  */
  var = gfc_create_var (TREE_TYPE (se->expr), NULL);
  gfc_add_modify_expr (&se->pre, var, se->expr);
  se->expr = var;
}


/* Return an expression which determines if a dummy parameter is present.
   Also used for arguments to procedures with multiple entry points.  */

tree
gfc_conv_expr_present (gfc_symbol * sym)
{
  tree decl;

  gcc_assert (sym->attr.dummy);

  decl = gfc_get_symbol_decl (sym);
  if (TREE_CODE (decl) != PARM_DECL)
    {
      /* Array parameters use a temporary descriptor, we want the real
         parameter.  */
      gcc_assert (GFC_DESCRIPTOR_TYPE_P (TREE_TYPE (decl))
             || GFC_ARRAY_TYPE_P (TREE_TYPE (decl)));
      decl = GFC_DECL_SAVED_DESCRIPTOR (decl);
    }
  return build2 (NE_EXPR, boolean_type_node, decl,
		 fold_convert (TREE_TYPE (decl), null_pointer_node));
}


/* Converts a missing, dummy argument into a null or zero.  */

void
gfc_conv_missing_dummy (gfc_se * se, gfc_expr * arg, gfc_typespec ts)
{
  tree present;
  tree tmp;

  present = gfc_conv_expr_present (arg->symtree->n.sym);
  tmp = build3 (COND_EXPR, TREE_TYPE (se->expr), present, se->expr,
		fold_convert (TREE_TYPE (se->expr), integer_zero_node));

  tmp = gfc_evaluate_now (tmp, &se->pre);
  se->expr = tmp;
  if (ts.type == BT_CHARACTER)
    {
      tmp = build_int_cst (gfc_charlen_type_node, 0);
      tmp = build3 (COND_EXPR, gfc_charlen_type_node, present,
		    se->string_length, tmp);
      tmp = gfc_evaluate_now (tmp, &se->pre);
      se->string_length = tmp;
    }
  return;
}


/* Get the character length of an expression, looking through gfc_refs
   if necessary.  */

tree
gfc_get_expr_charlen (gfc_expr *e)
{
  gfc_ref *r;
  tree length;

  gcc_assert (e->expr_type == EXPR_VARIABLE 
	      && e->ts.type == BT_CHARACTER);
  
  length = NULL; /* To silence compiler warning.  */

  /* First candidate: if the variable is of type CHARACTER, the
     expression's length could be the length of the character
     variable.  */
  if (e->symtree->n.sym->ts.type == BT_CHARACTER)
    length = e->symtree->n.sym->ts.cl->backend_decl;

  /* Look through the reference chain for component references.  */
  for (r = e->ref; r; r = r->next)
    {
      switch (r->type)
	{
	case REF_COMPONENT:
	  if (r->u.c.component->ts.type == BT_CHARACTER)
	    length = r->u.c.component->ts.cl->backend_decl;
	  break;

	case REF_ARRAY:
	  /* Do nothing.  */
	  break;

	default:
	  /* We should never got substring references here.  These will be
	     broken down by the scalarizer.  */
	  gcc_unreachable ();
	}
    }

  gcc_assert (length != NULL);
  return length;
}

  

/* Generate code to initialize a string length variable. Returns the
   value.  */

void
gfc_trans_init_string_length (gfc_charlen * cl, stmtblock_t * pblock)
{
  gfc_se se;
  tree tmp;

  gfc_init_se (&se, NULL);
  gfc_conv_expr_type (&se, cl->length, gfc_charlen_type_node);
  gfc_add_block_to_block (pblock, &se.pre);

  tmp = cl->backend_decl;
  gfc_add_modify_expr (pblock, tmp, se.expr);
}


static void
gfc_conv_substring (gfc_se * se, gfc_ref * ref, int kind)
{
  tree tmp;
  tree type;
  tree var;
  gfc_se start;
  gfc_se end;

  type = gfc_get_character_type (kind, ref->u.ss.length);
  type = build_pointer_type (type);

  var = NULL_TREE;
  gfc_init_se (&start, se);
  gfc_conv_expr_type (&start, ref->u.ss.start, gfc_charlen_type_node);
  gfc_add_block_to_block (&se->pre, &start.pre);

  if (integer_onep (start.expr))
    gfc_conv_string_parameter (se);
  else
    {
      /* Avoid multiple evaluation of substring start.  */
      if (!CONSTANT_CLASS_P (start.expr) && !DECL_P (start.expr))
	start.expr = gfc_evaluate_now (start.expr, &se->pre);

      /* Change the start of the string.  */
      if (TYPE_STRING_FLAG (TREE_TYPE (se->expr)))
	tmp = se->expr;
      else
	tmp = build_fold_indirect_ref (se->expr);
      tmp = gfc_build_array_ref (tmp, start.expr);
      se->expr = gfc_build_addr_expr (type, tmp);
    }

  /* Length = end + 1 - start.  */
  gfc_init_se (&end, se);
  if (ref->u.ss.end == NULL)
    end.expr = se->string_length;
  else
    {
      gfc_conv_expr_type (&end, ref->u.ss.end, gfc_charlen_type_node);
      gfc_add_block_to_block (&se->pre, &end.pre);
    }

  if (!CONSTANT_CLASS_P (end.expr) && !DECL_P (end.expr))
    end.expr = gfc_evaluate_now (end.expr, &se->pre);

  tmp = fold_build2 (MINUS_EXPR, gfc_charlen_type_node,
		     build_int_cst (gfc_charlen_type_node, 1),
		     start.expr);
  tmp = fold_build2 (PLUS_EXPR, gfc_charlen_type_node, end.expr, tmp);
  tmp = fold_build2 (MAX_EXPR, gfc_charlen_type_node, tmp,
		     build_int_cst (gfc_charlen_type_node, 0));
  se->string_length = tmp;
}


/* Convert a derived type component reference.  */

static void
gfc_conv_component_ref (gfc_se * se, gfc_ref * ref)
{
  gfc_component *c;
  tree tmp;
  tree decl;
  tree field;

  c = ref->u.c.component;

  gcc_assert (c->backend_decl);

  field = c->backend_decl;
  gcc_assert (TREE_CODE (field) == FIELD_DECL);
  decl = se->expr;
  tmp = build3 (COMPONENT_REF, TREE_TYPE (field), decl, field, NULL_TREE);

  se->expr = tmp;

  if (c->ts.type == BT_CHARACTER)
    {
      tmp = c->ts.cl->backend_decl;
      /* Components must always be constant length.  */
      gcc_assert (tmp && INTEGER_CST_P (tmp));
      se->string_length = tmp;
    }

  if (c->pointer && c->dimension == 0 && c->ts.type != BT_CHARACTER)
    se->expr = build_fold_indirect_ref (se->expr);
}


/* Return the contents of a variable. Also handles reference/pointer
   variables (all Fortran pointer references are implicit).  */

static void
gfc_conv_variable (gfc_se * se, gfc_expr * expr)
{
  gfc_ref *ref;
  gfc_symbol *sym;
  tree parent_decl;
  int parent_flag;
  bool return_value;
  bool alternate_entry;
  bool entry_master;

  sym = expr->symtree->n.sym;
  if (se->ss != NULL)
    {
      /* Check that something hasn't gone horribly wrong.  */
      gcc_assert (se->ss != gfc_ss_terminator);
      gcc_assert (se->ss->expr == expr);

      /* A scalarized term.  We already know the descriptor.  */
      se->expr = se->ss->data.info.descriptor;
      se->string_length = se->ss->string_length;
      for (ref = se->ss->data.info.ref; ref; ref = ref->next)
	if (ref->type == REF_ARRAY && ref->u.ar.type != AR_ELEMENT)
	  break;
    }
  else
    {
      tree se_expr = NULL_TREE;

      se->expr = gfc_get_symbol_decl (sym);

      /* Deal with references to a parent results or entries by storing
	 the current_function_decl and moving to the parent_decl.  */
      return_value = sym->attr.function && sym->result == sym;
      alternate_entry = sym->attr.function && sym->attr.entry
			&& sym->result == sym;
      entry_master = sym->attr.result
		     && sym->ns->proc_name->attr.entry_master
		     && !gfc_return_by_reference (sym->ns->proc_name);
      parent_decl = DECL_CONTEXT (current_function_decl);

      if ((se->expr == parent_decl && return_value)
	   || (sym->ns && sym->ns->proc_name
	       && parent_decl
	       && sym->ns->proc_name->backend_decl == parent_decl
	       && (alternate_entry || entry_master)))
	parent_flag = 1;
      else
	parent_flag = 0;

      /* Special case for assigning the return value of a function.
	 Self recursive functions must have an explicit return value.  */
      if (return_value && (se->expr == current_function_decl || parent_flag))
	se_expr = gfc_get_fake_result_decl (sym, parent_flag);

      /* Similarly for alternate entry points.  */
      else if (alternate_entry 
	       && (sym->ns->proc_name->backend_decl == current_function_decl
		   || parent_flag))
	{
	  gfc_entry_list *el = NULL;

	  for (el = sym->ns->entries; el; el = el->next)
	    if (sym == el->sym)
	      {
		se_expr = gfc_get_fake_result_decl (sym, parent_flag);
		break;
	      }
	}

      else if (entry_master
	       && (sym->ns->proc_name->backend_decl == current_function_decl
		   || parent_flag))
	se_expr = gfc_get_fake_result_decl (sym, parent_flag);

      if (se_expr)
	se->expr = se_expr;

      /* Procedure actual arguments.  */
      else if (sym->attr.flavor == FL_PROCEDURE
	       && se->expr != current_function_decl)
	{
	  gcc_assert (se->want_pointer);
	  if (!sym->attr.dummy)
	    {
	      gcc_assert (TREE_CODE (se->expr) == FUNCTION_DECL);
	      se->expr = build_fold_addr_expr (se->expr);
	    }
	  return;
	}


      /* Dereference the expression, where needed. Since characters
	 are entirely different from other types, they are treated 
	 separately.  */
      if (sym->ts.type == BT_CHARACTER)
	{
          /* Dereference character pointer dummy arguments
	     or results.  */
	  if ((sym->attr.pointer || sym->attr.allocatable)
	      && (sym->attr.dummy
		  || sym->attr.function
		  || sym->attr.result))
	    se->expr = build_fold_indirect_ref (se->expr);
	}
      else
	{
          /* Dereference non-character scalar dummy arguments.  */
	  if (sym->attr.dummy && !sym->attr.dimension)
	    se->expr = build_fold_indirect_ref (se->expr);

          /* Dereference scalar hidden result.  */
	  if (gfc_option.flag_f2c && sym->ts.type == BT_COMPLEX
	      && (sym->attr.function || sym->attr.result)
	      && !sym->attr.dimension && !sym->attr.pointer)
	    se->expr = build_fold_indirect_ref (se->expr);

          /* Dereference non-character pointer variables. 
	     These must be dummies, results, or scalars.  */
	  if ((sym->attr.pointer || sym->attr.allocatable)
	      && (sym->attr.dummy
		  || sym->attr.function
		  || sym->attr.result
		  || !sym->attr.dimension))
	    se->expr = build_fold_indirect_ref (se->expr);
	}

      ref = expr->ref;
    }

  /* For character variables, also get the length.  */
  if (sym->ts.type == BT_CHARACTER)
    {
      /* If the character length of an entry isn't set, get the length from
         the master function instead.  */
      if (sym->attr.entry && !sym->ts.cl->backend_decl)
        se->string_length = sym->ns->proc_name->ts.cl->backend_decl;
      else
        se->string_length = sym->ts.cl->backend_decl;
      gcc_assert (se->string_length);
    }

  while (ref)
    {
      switch (ref->type)
	{
	case REF_ARRAY:
	  /* Return the descriptor if that's what we want and this is an array
	     section reference.  */
	  if (se->descriptor_only && ref->u.ar.type != AR_ELEMENT)
	    return;
/* TODO: Pointers to single elements of array sections, eg elemental subs.  */
	  /* Return the descriptor for array pointers and allocations.  */
	  if (se->want_pointer
	      && ref->next == NULL && (se->descriptor_only))
	    return;

	  gfc_conv_array_ref (se, &ref->u.ar, sym, &expr->where);
	  /* Return a pointer to an element.  */
	  break;

	case REF_COMPONENT:
	  gfc_conv_component_ref (se, ref);
	  break;

	case REF_SUBSTRING:
	  gfc_conv_substring (se, ref, expr->ts.kind);
	  break;

	default:
	  gcc_unreachable ();
	  break;
	}
      ref = ref->next;
    }
  /* Pointer assignment, allocation or pass by reference.  Arrays are handled
     separately.  */
  if (se->want_pointer)
    {
      if (expr->ts.type == BT_CHARACTER)
	gfc_conv_string_parameter (se);
      else 
	se->expr = build_fold_addr_expr (se->expr);
    }
}


/* Unary ops are easy... Or they would be if ! was a valid op.  */

static void
gfc_conv_unary_op (enum tree_code code, gfc_se * se, gfc_expr * expr)
{
  gfc_se operand;
  tree type;

  gcc_assert (expr->ts.type != BT_CHARACTER);
  /* Initialize the operand.  */
  gfc_init_se (&operand, se);
  gfc_conv_expr_val (&operand, expr->value.op.op1);
  gfc_add_block_to_block (&se->pre, &operand.pre);

  type = gfc_typenode_for_spec (&expr->ts);

  /* TRUTH_NOT_EXPR is not a "true" unary operator in GCC.
     We must convert it to a compare to 0 (e.g. EQ_EXPR (op1, 0)).
     All other unary operators have an equivalent GIMPLE unary operator.  */
  if (code == TRUTH_NOT_EXPR)
    se->expr = build2 (EQ_EXPR, type, operand.expr,
		       build_int_cst (type, 0));
  else
    se->expr = build1 (code, type, operand.expr);

}

/* Expand power operator to optimal multiplications when a value is raised
   to a constant integer n. See section 4.6.3, "Evaluation of Powers" of
   Donald E. Knuth, "Seminumerical Algorithms", Vol. 2, "The Art of Computer
   Programming", 3rd Edition, 1998.  */

/* This code is mostly duplicated from expand_powi in the backend.
   We establish the "optimal power tree" lookup table with the defined size.
   The items in the table are the exponents used to calculate the index
   exponents. Any integer n less than the value can get an "addition chain",
   with the first node being one.  */
#define POWI_TABLE_SIZE 256

/* The table is from builtins.c.  */
static const unsigned char powi_table[POWI_TABLE_SIZE] =
  {
      0,   1,   1,   2,   2,   3,   3,   4,  /*   0 -   7 */
      4,   6,   5,   6,   6,  10,   7,   9,  /*   8 -  15 */
      8,  16,   9,  16,  10,  12,  11,  13,  /*  16 -  23 */
     12,  17,  13,  18,  14,  24,  15,  26,  /*  24 -  31 */
     16,  17,  17,  19,  18,  33,  19,  26,  /*  32 -  39 */
     20,  25,  21,  40,  22,  27,  23,  44,  /*  40 -  47 */
     24,  32,  25,  34,  26,  29,  27,  44,  /*  48 -  55 */
     28,  31,  29,  34,  30,  60,  31,  36,  /*  56 -  63 */
     32,  64,  33,  34,  34,  46,  35,  37,  /*  64 -  71 */
     36,  65,  37,  50,  38,  48,  39,  69,  /*  72 -  79 */
     40,  49,  41,  43,  42,  51,  43,  58,  /*  80 -  87 */
     44,  64,  45,  47,  46,  59,  47,  76,  /*  88 -  95 */
     48,  65,  49,  66,  50,  67,  51,  66,  /*  96 - 103 */
     52,  70,  53,  74,  54, 104,  55,  74,  /* 104 - 111 */
     56,  64,  57,  69,  58,  78,  59,  68,  /* 112 - 119 */
     60,  61,  61,  80,  62,  75,  63,  68,  /* 120 - 127 */
     64,  65,  65, 128,  66, 129,  67,  90,  /* 128 - 135 */
     68,  73,  69, 131,  70,  94,  71,  88,  /* 136 - 143 */
     72, 128,  73,  98,  74, 132,  75, 121,  /* 144 - 151 */
     76, 102,  77, 124,  78, 132,  79, 106,  /* 152 - 159 */
     80,  97,  81, 160,  82,  99,  83, 134,  /* 160 - 167 */
     84,  86,  85,  95,  86, 160,  87, 100,  /* 168 - 175 */
     88, 113,  89,  98,  90, 107,  91, 122,  /* 176 - 183 */
     92, 111,  93, 102,  94, 126,  95, 150,  /* 184 - 191 */
     96, 128,  97, 130,  98, 133,  99, 195,  /* 192 - 199 */
    100, 128, 101, 123, 102, 164, 103, 138,  /* 200 - 207 */
    104, 145, 105, 146, 106, 109, 107, 149,  /* 208 - 215 */
    108, 200, 109, 146, 110, 170, 111, 157,  /* 216 - 223 */
    112, 128, 113, 130, 114, 182, 115, 132,  /* 224 - 231 */
    116, 200, 117, 132, 118, 158, 119, 206,  /* 232 - 239 */
    120, 240, 121, 162, 122, 147, 123, 152,  /* 240 - 247 */
    124, 166, 125, 214, 126, 138, 127, 153,  /* 248 - 255 */
  };

/* If n is larger than lookup table's max index, we use the "window 
   method".  */
#define POWI_WINDOW_SIZE 3

/* Recursive function to expand the power operator. The temporary 
   values are put in tmpvar. The function returns tmpvar[1] ** n.  */
static tree
gfc_conv_powi (gfc_se * se, int n, tree * tmpvar)
{
  tree op0;
  tree op1;
  tree tmp;
  int digit;

  if (n < POWI_TABLE_SIZE)
    {
      if (tmpvar[n])
        return tmpvar[n];

      op0 = gfc_conv_powi (se, n - powi_table[n], tmpvar);
      op1 = gfc_conv_powi (se, powi_table[n], tmpvar);
    }
  else if (n & 1)
    {
      digit = n & ((1 << POWI_WINDOW_SIZE) - 1);
      op0 = gfc_conv_powi (se, n - digit, tmpvar);
      op1 = gfc_conv_powi (se, digit, tmpvar);
    }
  else
    {
      op0 = gfc_conv_powi (se, n >> 1, tmpvar);
      op1 = op0;
    }

  tmp = fold_build2 (MULT_EXPR, TREE_TYPE (op0), op0, op1);
  tmp = gfc_evaluate_now (tmp, &se->pre);

  if (n < POWI_TABLE_SIZE)
    tmpvar[n] = tmp;

  return tmp;
}


/* Expand lhs ** rhs. rhs is a constant integer. If it expands successfully,
   return 1. Else return 0 and a call to runtime library functions
   will have to be built.  */
static int
gfc_conv_cst_int_power (gfc_se * se, tree lhs, tree rhs)
{
  tree cond;
  tree tmp;
  tree type;
  tree vartmp[POWI_TABLE_SIZE];
  int n;
  int sgn;

  type = TREE_TYPE (lhs);
  n = abs (TREE_INT_CST_LOW (rhs));
  sgn = tree_int_cst_sgn (rhs);

  if (((FLOAT_TYPE_P (type) && !flag_unsafe_math_optimizations) || optimize_size)
      && (n > 2 || n < -1))
    return 0;

  /* rhs == 0  */
  if (sgn == 0)
    {
      se->expr = gfc_build_const (type, integer_one_node);
      return 1;
    }
  /* If rhs < 0 and lhs is an integer, the result is -1, 0 or 1.  */
  if ((sgn == -1) && (TREE_CODE (type) == INTEGER_TYPE))
    {
      tmp = build2 (EQ_EXPR, boolean_type_node, lhs,
		    build_int_cst (TREE_TYPE (lhs), -1));
      cond = build2 (EQ_EXPR, boolean_type_node, lhs,
		     build_int_cst (TREE_TYPE (lhs), 1));

      /* If rhs is even,
	 result = (lhs == 1 || lhs == -1) ? 1 : 0.  */
      if ((n & 1) == 0)
        {
	  tmp = build2 (TRUTH_OR_EXPR, boolean_type_node, tmp, cond);
	  se->expr = build3 (COND_EXPR, type, tmp, build_int_cst (type, 1),
			     build_int_cst (type, 0));
	  return 1;
	}
      /* If rhs is odd,
	 result = (lhs == 1) ? 1 : (lhs == -1) ? -1 : 0.  */
      tmp = build3 (COND_EXPR, type, tmp, build_int_cst (type, -1),
		    build_int_cst (type, 0));
      se->expr = build3 (COND_EXPR, type, cond, build_int_cst (type, 1), tmp);
      return 1;
    }

  memset (vartmp, 0, sizeof (vartmp));
  vartmp[1] = lhs;
  if (sgn == -1)
    {
      tmp = gfc_build_const (type, integer_one_node);
      vartmp[1] = build2 (RDIV_EXPR, type, tmp, vartmp[1]);
    }

  se->expr = gfc_conv_powi (se, n, vartmp);

  return 1;
}


/* Power op (**).  Constant integer exponent has special handling.  */

static void
gfc_conv_power_op (gfc_se * se, gfc_expr * expr)
{
  tree gfc_int4_type_node;
  int kind;
  int ikind;
  gfc_se lse;
  gfc_se rse;
  tree fndecl;
  tree tmp;

  gfc_init_se (&lse, se);
  gfc_conv_expr_val (&lse, expr->value.op.op1);
  lse.expr = gfc_evaluate_now (lse.expr, &lse.pre);
  gfc_add_block_to_block (&se->pre, &lse.pre);

  gfc_init_se (&rse, se);
  gfc_conv_expr_val (&rse, expr->value.op.op2);
  gfc_add_block_to_block (&se->pre, &rse.pre);

  if (expr->value.op.op2->ts.type == BT_INTEGER
	 && expr->value.op.op2->expr_type == EXPR_CONSTANT)
    if (gfc_conv_cst_int_power (se, lse.expr, rse.expr))
      return;        

  gfc_int4_type_node = gfc_get_int_type (4);

  kind = expr->value.op.op1->ts.kind;
  switch (expr->value.op.op2->ts.type)
    {
    case BT_INTEGER:
      ikind = expr->value.op.op2->ts.kind;
      switch (ikind)
	{
	case 1:
	case 2:
	  rse.expr = convert (gfc_int4_type_node, rse.expr);
	  /* Fall through.  */

	case 4:
	  ikind = 0;
	  break;
	  
	case 8:
	  ikind = 1;
	  break;

	case 16:
	  ikind = 2;
	  break;

	default:
	  gcc_unreachable ();
	}
      switch (kind)
	{
	case 1:
	case 2:
	  if (expr->value.op.op1->ts.type == BT_INTEGER)
	    lse.expr = convert (gfc_int4_type_node, lse.expr);
	  else
	    gcc_unreachable ();
	  /* Fall through.  */

	case 4:
	  kind = 0;
	  break;
	  
	case 8:
	  kind = 1;
	  break;

	case 10:
	  kind = 2;
	  break;

	case 16:
	  kind = 3;
	  break;

	default:
	  gcc_unreachable ();
	}
      
      switch (expr->value.op.op1->ts.type)
	{
	case BT_INTEGER:
	  if (kind == 3) /* Case 16 was not handled properly above.  */
	    kind = 2;
	  fndecl = gfor_fndecl_math_powi[kind][ikind].integer;
	  break;

	case BT_REAL:
	  fndecl = gfor_fndecl_math_powi[kind][ikind].real;
	  break;

	case BT_COMPLEX:
	  fndecl = gfor_fndecl_math_powi[kind][ikind].cmplx;
	  break;

	default:
	  gcc_unreachable ();
 	}
      break;

    case BT_REAL:
      switch (kind)
	{
	case 4:
	  fndecl = built_in_decls[BUILT_IN_POWF];
	  break;
	case 8:
	  fndecl = built_in_decls[BUILT_IN_POW];
	  break;
	case 10:
	case 16:
	  fndecl = built_in_decls[BUILT_IN_POWL];
	  break;
	default:
	  gcc_unreachable ();
	}
      break;

    case BT_COMPLEX:
      switch (kind)
	{
	case 4:
	  fndecl = gfor_fndecl_math_cpowf;
	  break;
	case 8:
	  fndecl = gfor_fndecl_math_cpow;
	  break;
	case 10:
	  fndecl = gfor_fndecl_math_cpowl10;
	  break;
	case 16:
	  fndecl = gfor_fndecl_math_cpowl16;
	  break;
	default:
	  gcc_unreachable ();
	}
      break;

    default:
      gcc_unreachable ();
      break;
    }

  tmp = gfc_chainon_list (NULL_TREE, lse.expr);
  tmp = gfc_chainon_list (tmp, rse.expr);
  se->expr = build_function_call_expr (fndecl, tmp);
}


/* Generate code to allocate a string temporary.  */

tree
gfc_conv_string_tmp (gfc_se * se, tree type, tree len)
{
  tree var;
  tree tmp;
  tree args;

  gcc_assert (TREE_TYPE (len) == gfc_charlen_type_node);

  if (gfc_can_put_var_on_stack (len))
    {
      /* Create a temporary variable to hold the result.  */
      tmp = fold_build2 (MINUS_EXPR, gfc_charlen_type_node, len,
			 build_int_cst (gfc_charlen_type_node, 1));
      tmp = build_range_type (gfc_array_index_type, gfc_index_zero_node, tmp);
      tmp = build_array_type (gfc_character1_type_node, tmp);
      var = gfc_create_var (tmp, "str");
      var = gfc_build_addr_expr (type, var);
    }
  else
    {
      /* Allocate a temporary to hold the result.  */
      var = gfc_create_var (type, "pstr");
      args = gfc_chainon_list (NULL_TREE, len);
      tmp = build_function_call_expr (gfor_fndecl_internal_malloc, args);
      tmp = convert (type, tmp);
      gfc_add_modify_expr (&se->pre, var, tmp);

      /* Free the temporary afterwards.  */
      tmp = convert (pvoid_type_node, var);
      args = gfc_chainon_list (NULL_TREE, tmp);
      tmp = build_function_call_expr (gfor_fndecl_internal_free, args);
      gfc_add_expr_to_block (&se->post, tmp);
    }

  return var;
}


/* Handle a string concatenation operation.  A temporary will be allocated to
   hold the result.  */

static void
gfc_conv_concat_op (gfc_se * se, gfc_expr * expr)
{
  gfc_se lse;
  gfc_se rse;
  tree len;
  tree type;
  tree var;
  tree args;
  tree tmp;

  gcc_assert (expr->value.op.op1->ts.type == BT_CHARACTER
	  && expr->value.op.op2->ts.type == BT_CHARACTER);

  gfc_init_se (&lse, se);
  gfc_conv_expr (&lse, expr->value.op.op1);
  gfc_conv_string_parameter (&lse);
  gfc_init_se (&rse, se);
  gfc_conv_expr (&rse, expr->value.op.op2);
  gfc_conv_string_parameter (&rse);

  gfc_add_block_to_block (&se->pre, &lse.pre);
  gfc_add_block_to_block (&se->pre, &rse.pre);

  type = gfc_get_character_type (expr->ts.kind, expr->ts.cl);
  len = TYPE_MAX_VALUE (TYPE_DOMAIN (type));
  if (len == NULL_TREE)
    {
      len = fold_build2 (PLUS_EXPR, TREE_TYPE (lse.string_length),
			 lse.string_length, rse.string_length);
    }

  type = build_pointer_type (type);

  var = gfc_conv_string_tmp (se, type, len);

  /* Do the actual concatenation.  */
  args = NULL_TREE;
  args = gfc_chainon_list (args, len);
  args = gfc_chainon_list (args, var);
  args = gfc_chainon_list (args, lse.string_length);
  args = gfc_chainon_list (args, lse.expr);
  args = gfc_chainon_list (args, rse.string_length);
  args = gfc_chainon_list (args, rse.expr);
  tmp = build_function_call_expr (gfor_fndecl_concat_string, args);
  gfc_add_expr_to_block (&se->pre, tmp);

  /* Add the cleanup for the operands.  */
  gfc_add_block_to_block (&se->pre, &rse.post);
  gfc_add_block_to_block (&se->pre, &lse.post);

  se->expr = var;
  se->string_length = len;
}

/* Translates an op expression. Common (binary) cases are handled by this
   function, others are passed on. Recursion is used in either case.
   We use the fact that (op1.ts == op2.ts) (except for the power
   operator **).
   Operators need no special handling for scalarized expressions as long as
   they call gfc_conv_simple_val to get their operands.
   Character strings get special handling.  */

static void
gfc_conv_expr_op (gfc_se * se, gfc_expr * expr)
{
  enum tree_code code;
  gfc_se lse;
  gfc_se rse;
  tree type;
  tree tmp;
  int lop;
  int checkstring;

  checkstring = 0;
  lop = 0;
  switch (expr->value.op.operator)
    {
    case INTRINSIC_UPLUS:
    case INTRINSIC_PARENTHESES:
      gfc_conv_expr (se, expr->value.op.op1);
      return;

    case INTRINSIC_UMINUS:
      gfc_conv_unary_op (NEGATE_EXPR, se, expr);
      return;

    case INTRINSIC_NOT:
      gfc_conv_unary_op (TRUTH_NOT_EXPR, se, expr);
      return;

    case INTRINSIC_PLUS:
      code = PLUS_EXPR;
      break;

    case INTRINSIC_MINUS:
      code = MINUS_EXPR;
      break;

    case INTRINSIC_TIMES:
      code = MULT_EXPR;
      break;

    case INTRINSIC_DIVIDE:
      /* If expr is a real or complex expr, use an RDIV_EXPR. If op1 is
         an integer, we must round towards zero, so we use a
         TRUNC_DIV_EXPR.  */
      if (expr->ts.type == BT_INTEGER)
	code = TRUNC_DIV_EXPR;
      else
	code = RDIV_EXPR;
      break;

    case INTRINSIC_POWER:
      gfc_conv_power_op (se, expr);
      return;

    case INTRINSIC_CONCAT:
      gfc_conv_concat_op (se, expr);
      return;

    case INTRINSIC_AND:
      code = TRUTH_ANDIF_EXPR;
      lop = 1;
      break;

    case INTRINSIC_OR:
      code = TRUTH_ORIF_EXPR;
      lop = 1;
      break;

      /* EQV and NEQV only work on logicals, but since we represent them
         as integers, we can use EQ_EXPR and NE_EXPR for them in GIMPLE.  */
    case INTRINSIC_EQ:
    case INTRINSIC_EQV:
      code = EQ_EXPR;
      checkstring = 1;
      lop = 1;
      break;

    case INTRINSIC_NE:
    case INTRINSIC_NEQV:
      code = NE_EXPR;
      checkstring = 1;
      lop = 1;
      break;

    case INTRINSIC_GT:
      code = GT_EXPR;
      checkstring = 1;
      lop = 1;
      break;

    case INTRINSIC_GE:
      code = GE_EXPR;
      checkstring = 1;
      lop = 1;
      break;

    case INTRINSIC_LT:
      code = LT_EXPR;
      checkstring = 1;
      lop = 1;
      break;

    case INTRINSIC_LE:
      code = LE_EXPR;
      checkstring = 1;
      lop = 1;
      break;

    case INTRINSIC_USER:
    case INTRINSIC_ASSIGN:
      /* These should be converted into function calls by the frontend.  */
      gcc_unreachable ();

    default:
      fatal_error ("Unknown intrinsic op");
      return;
    }

  /* The only exception to this is **, which is handled separately anyway.  */
  gcc_assert (expr->value.op.op1->ts.type == expr->value.op.op2->ts.type);

  if (checkstring && expr->value.op.op1->ts.type != BT_CHARACTER)
    checkstring = 0;

  /* lhs */
  gfc_init_se (&lse, se);
  gfc_conv_expr (&lse, expr->value.op.op1);
  gfc_add_block_to_block (&se->pre, &lse.pre);

  /* rhs */
  gfc_init_se (&rse, se);
  gfc_conv_expr (&rse, expr->value.op.op2);
  gfc_add_block_to_block (&se->pre, &rse.pre);

  if (checkstring)
    {
      gfc_conv_string_parameter (&lse);
      gfc_conv_string_parameter (&rse);

      lse.expr = gfc_build_compare_string (lse.string_length, lse.expr,
					   rse.string_length, rse.expr);
      rse.expr = integer_zero_node;
      gfc_add_block_to_block (&lse.post, &rse.post);
    }

  type = gfc_typenode_for_spec (&expr->ts);

  if (lop)
    {
      /* The result of logical ops is always boolean_type_node.  */
      tmp = fold_build2 (code, type, lse.expr, rse.expr);
      se->expr = convert (type, tmp);
    }
  else
    se->expr = fold_build2 (code, type, lse.expr, rse.expr);

  /* Add the post blocks.  */
  gfc_add_block_to_block (&se->post, &rse.post);
  gfc_add_block_to_block (&se->post, &lse.post);
}

/* If a string's length is one, we convert it to a single character.  */

static tree
gfc_to_single_character (tree len, tree str)
{
  gcc_assert (POINTER_TYPE_P (TREE_TYPE (str)));

  if (INTEGER_CST_P (len) && TREE_INT_CST_LOW (len) == 1
    && TREE_INT_CST_HIGH (len) == 0)
    {
      str = fold_convert (pchar_type_node, str);
      return build_fold_indirect_ref (str);
    }

  return NULL_TREE;
}

/* Compare two strings. If they are all single characters, the result is the
   subtraction of them. Otherwise, we build a library call.  */

tree
gfc_build_compare_string (tree len1, tree str1, tree len2, tree str2)
{
  tree sc1;
  tree sc2;
  tree type;
  tree tmp;

  gcc_assert (POINTER_TYPE_P (TREE_TYPE (str1)));
  gcc_assert (POINTER_TYPE_P (TREE_TYPE (str2)));

  type = gfc_get_int_type (gfc_default_integer_kind);

  sc1 = gfc_to_single_character (len1, str1);
  sc2 = gfc_to_single_character (len2, str2);

  /* Deal with single character specially.  */
  if (sc1 != NULL_TREE && sc2 != NULL_TREE)
    {
      sc1 = fold_convert (type, sc1);
      sc2 = fold_convert (type, sc2);
      tmp = fold_build2 (MINUS_EXPR, type, sc1, sc2);
    }
   else
    {
      tmp = NULL_TREE;
      tmp = gfc_chainon_list (tmp, len1);
      tmp = gfc_chainon_list (tmp, str1);
      tmp = gfc_chainon_list (tmp, len2);
      tmp = gfc_chainon_list (tmp, str2);

      /* Build a call for the comparison.  */
      tmp = build_function_call_expr (gfor_fndecl_compare_string, tmp);
    }

  return tmp;
}

static void
gfc_conv_function_val (gfc_se * se, gfc_symbol * sym)
{
  tree tmp;

  if (sym->attr.dummy)
    {
      tmp = gfc_get_symbol_decl (sym);
      gcc_assert (TREE_CODE (TREE_TYPE (tmp)) == POINTER_TYPE
	      && TREE_CODE (TREE_TYPE (TREE_TYPE (tmp))) == FUNCTION_TYPE);
    }
  else
    {
      if (!sym->backend_decl)
	sym->backend_decl = gfc_get_extern_function_decl (sym);

      tmp = sym->backend_decl;
      if (sym->attr.cray_pointee)
	tmp = convert (build_pointer_type (TREE_TYPE (tmp)),
		       gfc_get_symbol_decl (sym->cp_pointer));
      if (!POINTER_TYPE_P (TREE_TYPE (tmp)))
	{
	  gcc_assert (TREE_CODE (tmp) == FUNCTION_DECL);
	  tmp = build_fold_addr_expr (tmp);
	}
    }
  se->expr = tmp;
}


/* Translate the call for an elemental subroutine call used in an operator
   assignment.  This is a simplified version of gfc_conv_function_call.  */

tree
gfc_conv_operator_assign (gfc_se *lse, gfc_se *rse, gfc_symbol *sym)
{
  tree args;
  tree tmp;
  gfc_se se;
  stmtblock_t block;

  /* Only elemental subroutines with two arguments.  */
  gcc_assert (sym->attr.elemental && sym->attr.subroutine);
  gcc_assert (sym->formal->next->next == NULL);

  gfc_init_block (&block);

  gfc_add_block_to_block (&block, &lse->pre);
  gfc_add_block_to_block (&block, &rse->pre);

  /* Build the argument list for the call, including hidden string lengths.  */
  args = gfc_chainon_list (NULL_TREE, build_fold_addr_expr (lse->expr));
  args = gfc_chainon_list (args, build_fold_addr_expr (rse->expr));
  if (lse->string_length != NULL_TREE)
    args = gfc_chainon_list (args, lse->string_length);
  if (rse->string_length != NULL_TREE)
    args = gfc_chainon_list (args, rse->string_length);    

  /* Build the function call.  */
  gfc_init_se (&se, NULL);
  gfc_conv_function_val (&se, sym);
  tmp = TREE_TYPE (TREE_TYPE (TREE_TYPE (se.expr)));
  tmp = build3 (CALL_EXPR, tmp, se.expr, args, NULL_TREE);
  gfc_add_expr_to_block (&block, tmp);

  gfc_add_block_to_block (&block, &lse->post);
  gfc_add_block_to_block (&block, &rse->post);

  return gfc_finish_block (&block);
}


/* Initialize MAPPING.  */

void
gfc_init_interface_mapping (gfc_interface_mapping * mapping)
{
  mapping->syms = NULL;
  mapping->charlens = NULL;
}


/* Free all memory held by MAPPING (but not MAPPING itself).  */

void
gfc_free_interface_mapping (gfc_interface_mapping * mapping)
{
  gfc_interface_sym_mapping *sym;
  gfc_interface_sym_mapping *nextsym;
  gfc_charlen *cl;
  gfc_charlen *nextcl;

  for (sym = mapping->syms; sym; sym = nextsym)
    {
      nextsym = sym->next;
      gfc_free_symbol (sym->new->n.sym);
      gfc_free (sym->new);
      gfc_free (sym);
    }
  for (cl = mapping->charlens; cl; cl = nextcl)
    {
      nextcl = cl->next;
      gfc_free_expr (cl->length);
      gfc_free (cl);
    }
}


/* Return a copy of gfc_charlen CL.  Add the returned structure to
   MAPPING so that it will be freed by gfc_free_interface_mapping.  */

static gfc_charlen *
gfc_get_interface_mapping_charlen (gfc_interface_mapping * mapping,
				   gfc_charlen * cl)
{
  gfc_charlen *new;

  new = gfc_get_charlen ();
  new->next = mapping->charlens;
  new->length = gfc_copy_expr (cl->length);

  mapping->charlens = new;
  return new;
}


/* A subroutine of gfc_add_interface_mapping.  Return a descriptorless
   array variable that can be used as the actual argument for dummy
   argument SYM.  Add any initialization code to BLOCK.  PACKED is as
   for gfc_get_nodesc_array_type and DATA points to the first element
   in the passed array.  */

static tree
gfc_get_interface_mapping_array (stmtblock_t * block, gfc_symbol * sym,
				 int packed, tree data)
{
  tree type;
  tree var;

  type = gfc_typenode_for_spec (&sym->ts);
  type = gfc_get_nodesc_array_type (type, sym->as, packed);

  var = gfc_create_var (type, "ifm");
  gfc_add_modify_expr (block, var, fold_convert (type, data));

  return var;
}


/* A subroutine of gfc_add_interface_mapping.  Set the stride, upper bounds
   and offset of descriptorless array type TYPE given that it has the same
   size as DESC.  Add any set-up code to BLOCK.  */

static void
gfc_set_interface_mapping_bounds (stmtblock_t * block, tree type, tree desc)
{
  int n;
  tree dim;
  tree offset;
  tree tmp;

  offset = gfc_index_zero_node;
  for (n = 0; n < GFC_TYPE_ARRAY_RANK (type); n++)
    {
      dim = gfc_rank_cst[n];
      GFC_TYPE_ARRAY_STRIDE (type, n) = gfc_conv_array_stride (desc, n);
      if (GFC_TYPE_ARRAY_LBOUND (type, n) == NULL_TREE)
	{
	  GFC_TYPE_ARRAY_LBOUND (type, n)
		= gfc_conv_descriptor_lbound (desc, dim);
	  GFC_TYPE_ARRAY_UBOUND (type, n)
		= gfc_conv_descriptor_ubound (desc, dim);
	}
      else if (GFC_TYPE_ARRAY_UBOUND (type, n) == NULL_TREE)
	{
	  tmp = fold_build2 (MINUS_EXPR, gfc_array_index_type,
			     gfc_conv_descriptor_ubound (desc, dim),
			     gfc_conv_descriptor_lbound (desc, dim));
	  tmp = fold_build2 (PLUS_EXPR, gfc_array_index_type,
			     GFC_TYPE_ARRAY_LBOUND (type, n),
			     tmp);
	  tmp = gfc_evaluate_now (tmp, block);
	  GFC_TYPE_ARRAY_UBOUND (type, n) = tmp;
	}
      tmp = fold_build2 (MULT_EXPR, gfc_array_index_type,
			 GFC_TYPE_ARRAY_LBOUND (type, n),
			 GFC_TYPE_ARRAY_STRIDE (type, n));
      offset = fold_build2 (MINUS_EXPR, gfc_array_index_type, offset, tmp);
    }
  offset = gfc_evaluate_now (offset, block);
  GFC_TYPE_ARRAY_OFFSET (type) = offset;
}


/* Extend MAPPING so that it maps dummy argument SYM to the value stored
   in SE.  The caller may still use se->expr and se->string_length after
   calling this function.  */

void
gfc_add_interface_mapping (gfc_interface_mapping * mapping,
			   gfc_symbol * sym, gfc_se * se)
{
  gfc_interface_sym_mapping *sm;
  tree desc;
  tree tmp;
  tree value;
  gfc_symbol *new_sym;
  gfc_symtree *root;
  gfc_symtree *new_symtree;

  /* Create a new symbol to represent the actual argument.  */
  new_sym = gfc_new_symbol (sym->name, NULL);
  new_sym->ts = sym->ts;
  new_sym->attr.referenced = 1;
  new_sym->attr.dimension = sym->attr.dimension;
  new_sym->attr.pointer = sym->attr.pointer;
  new_sym->attr.allocatable = sym->attr.allocatable;
  new_sym->attr.flavor = sym->attr.flavor;

  /* Create a fake symtree for it.  */
  root = NULL;
  new_symtree = gfc_new_symtree (&root, sym->name);
  new_symtree->n.sym = new_sym;
  gcc_assert (new_symtree == root);

  /* Create a dummy->actual mapping.  */
  sm = gfc_getmem (sizeof (*sm));
  sm->next = mapping->syms;
  sm->old = sym;
  sm->new = new_symtree;
  mapping->syms = sm;

  /* Stabilize the argument's value.  */
  se->expr = gfc_evaluate_now (se->expr, &se->pre);

  if (sym->ts.type == BT_CHARACTER)
    {
      /* Create a copy of the dummy argument's length.  */
      new_sym->ts.cl = gfc_get_interface_mapping_charlen (mapping, sym->ts.cl);

      /* If the length is specified as "*", record the length that
	 the caller is passing.  We should use the callee's length
	 in all other cases.  */
      if (!new_sym->ts.cl->length)
	{
	  se->string_length = gfc_evaluate_now (se->string_length, &se->pre);
	  new_sym->ts.cl->backend_decl = se->string_length;
	}
    }

  /* Use the passed value as-is if the argument is a function.  */
  if (sym->attr.flavor == FL_PROCEDURE)
    value = se->expr;

  /* If the argument is either a string or a pointer to a string,
     convert it to a boundless character type.  */
  else if (!sym->attr.dimension && sym->ts.type == BT_CHARACTER)
    {
      tmp = gfc_get_character_type_len (sym->ts.kind, NULL);
      tmp = build_pointer_type (tmp);
      if (sym->attr.pointer)
        value = build_fold_indirect_ref (se->expr);
      else
        value = se->expr;
      value = fold_convert (tmp, value);
    }

  /* If the argument is a scalar, a pointer to an array or an allocatable,
     dereference it.  */
  else if (!sym->attr.dimension || sym->attr.pointer || sym->attr.allocatable)
    value = build_fold_indirect_ref (se->expr);
  
  /* For character(*), use the actual argument's descriptor.  */  
  else if (sym->ts.type == BT_CHARACTER && !new_sym->ts.cl->length)
    value = build_fold_indirect_ref (se->expr);

  /* If the argument is an array descriptor, use it to determine
     information about the actual argument's shape.  */
  else if (POINTER_TYPE_P (TREE_TYPE (se->expr))
	   && GFC_DESCRIPTOR_TYPE_P (TREE_TYPE (TREE_TYPE (se->expr))))
    {
      /* Get the actual argument's descriptor.  */
      desc = build_fold_indirect_ref (se->expr);

      /* Create the replacement variable.  */
      tmp = gfc_conv_descriptor_data_get (desc);
      value = gfc_get_interface_mapping_array (&se->pre, sym, 0, tmp);

      /* Use DESC to work out the upper bounds, strides and offset.  */
      gfc_set_interface_mapping_bounds (&se->pre, TREE_TYPE (value), desc);
    }
  else
    /* Otherwise we have a packed array.  */
    value = gfc_get_interface_mapping_array (&se->pre, sym, 2, se->expr);

  new_sym->backend_decl = value;
}


/* Called once all dummy argument mappings have been added to MAPPING,
   but before the mapping is used to evaluate expressions.  Pre-evaluate
   the length of each argument, adding any initialization code to PRE and
   any finalization code to POST.  */

void
gfc_finish_interface_mapping (gfc_interface_mapping * mapping,
			      stmtblock_t * pre, stmtblock_t * post)
{
  gfc_interface_sym_mapping *sym;
  gfc_expr *expr;
  gfc_se se;

  for (sym = mapping->syms; sym; sym = sym->next)
    if (sym->new->n.sym->ts.type == BT_CHARACTER
	&& !sym->new->n.sym->ts.cl->backend_decl)
      {
	expr = sym->new->n.sym->ts.cl->length;
	gfc_apply_interface_mapping_to_expr (mapping, expr);
	gfc_init_se (&se, NULL);
	gfc_conv_expr (&se, expr);

	se.expr = gfc_evaluate_now (se.expr, &se.pre);
	gfc_add_block_to_block (pre, &se.pre);
	gfc_add_block_to_block (post, &se.post);

	sym->new->n.sym->ts.cl->backend_decl = se.expr;
      }
}


/* Like gfc_apply_interface_mapping_to_expr, but applied to
   constructor C.  */

static void
gfc_apply_interface_mapping_to_cons (gfc_interface_mapping * mapping,
				     gfc_constructor * c)
{
  for (; c; c = c->next)
    {
      gfc_apply_interface_mapping_to_expr (mapping, c->expr);
      if (c->iterator)
	{
	  gfc_apply_interface_mapping_to_expr (mapping, c->iterator->start);
	  gfc_apply_interface_mapping_to_expr (mapping, c->iterator->end);
	  gfc_apply_interface_mapping_to_expr (mapping, c->iterator->step);
	}
    }
}


/* Like gfc_apply_interface_mapping_to_expr, but applied to
   reference REF.  */

static void
gfc_apply_interface_mapping_to_ref (gfc_interface_mapping * mapping,
				    gfc_ref * ref)
{
  int n;

  for (; ref; ref = ref->next)
    switch (ref->type)
      {
      case REF_ARRAY:
	for (n = 0; n < ref->u.ar.dimen; n++)
	  {
	    gfc_apply_interface_mapping_to_expr (mapping, ref->u.ar.start[n]);
	    gfc_apply_interface_mapping_to_expr (mapping, ref->u.ar.end[n]);
	    gfc_apply_interface_mapping_to_expr (mapping, ref->u.ar.stride[n]);
	  }
	gfc_apply_interface_mapping_to_expr (mapping, ref->u.ar.offset);
	break;

      case REF_COMPONENT:
	break;

      case REF_SUBSTRING:
	gfc_apply_interface_mapping_to_expr (mapping, ref->u.ss.start);
	gfc_apply_interface_mapping_to_expr (mapping, ref->u.ss.end);
	break;
      }
}


/* EXPR is a copy of an expression that appeared in the interface
   associated with MAPPING.  Walk it recursively looking for references to
   dummy arguments that MAPPING maps to actual arguments.  Replace each such
   reference with a reference to the associated actual argument.  */

static void
gfc_apply_interface_mapping_to_expr (gfc_interface_mapping * mapping,
				     gfc_expr * expr)
{
  gfc_interface_sym_mapping *sym;
  gfc_actual_arglist *actual;

  if (!expr)
    return;

  /* Copying an expression does not copy its length, so do that here.  */
  if (expr->ts.type == BT_CHARACTER && expr->ts.cl)
    {
      expr->ts.cl = gfc_get_interface_mapping_charlen (mapping, expr->ts.cl);
      gfc_apply_interface_mapping_to_expr (mapping, expr->ts.cl->length);
    }

  /* Apply the mapping to any references.  */
  gfc_apply_interface_mapping_to_ref (mapping, expr->ref);

  /* ...and to the expression's symbol, if it has one.  */
  if (expr->symtree)
    for (sym = mapping->syms; sym; sym = sym->next)
      if (sym->old == expr->symtree->n.sym)
	expr->symtree = sym->new;

  /* ...and to subexpressions in expr->value.  */
  switch (expr->expr_type)
    {
    case EXPR_VARIABLE:
    case EXPR_CONSTANT:
    case EXPR_NULL:
    case EXPR_SUBSTRING:
      break;

    case EXPR_OP:
      gfc_apply_interface_mapping_to_expr (mapping, expr->value.op.op1);
      gfc_apply_interface_mapping_to_expr (mapping, expr->value.op.op2);
      break;

    case EXPR_FUNCTION:
      for (sym = mapping->syms; sym; sym = sym->next)
	if (sym->old == expr->value.function.esym)
	  expr->value.function.esym = sym->new->n.sym;

      for (actual = expr->value.function.actual; actual; actual = actual->next)
	gfc_apply_interface_mapping_to_expr (mapping, actual->expr);
      break;

    case EXPR_ARRAY:
    case EXPR_STRUCTURE:
      gfc_apply_interface_mapping_to_cons (mapping, expr->value.constructor);
      break;
    }
}


/* Evaluate interface expression EXPR using MAPPING.  Store the result
   in SE.  */

void
gfc_apply_interface_mapping (gfc_interface_mapping * mapping,
			     gfc_se * se, gfc_expr * expr)
{
  expr = gfc_copy_expr (expr);
  gfc_apply_interface_mapping_to_expr (mapping, expr);
  gfc_conv_expr (se, expr);
  se->expr = gfc_evaluate_now (se->expr, &se->pre);
  gfc_free_expr (expr);
}

/* Returns a reference to a temporary array into which a component of
   an actual argument derived type array is copied and then returned
   after the function call.
   TODO Get rid of this kludge, when array descriptors are capable of
   handling arrays with a bigger stride in bytes than size.  */

void
gfc_conv_aliased_arg (gfc_se * parmse, gfc_expr * expr,
		      int g77, sym_intent intent)
{
  gfc_se lse;
  gfc_se rse;
  gfc_ss *lss;
  gfc_ss *rss;
  gfc_loopinfo loop;
  gfc_loopinfo loop2;
  gfc_ss_info *info;
  tree offset;
  tree tmp_index;
  tree tmp;
  tree base_type;
  stmtblock_t body;
  int n;

  gcc_assert (expr->expr_type == EXPR_VARIABLE);

  gfc_init_se (&lse, NULL);
  gfc_init_se (&rse, NULL);

  /* Walk the argument expression.  */
  rss = gfc_walk_expr (expr);

  gcc_assert (rss != gfc_ss_terminator);
 
  /* Initialize the scalarizer.  */
  gfc_init_loopinfo (&loop);
  gfc_add_ss_to_loop (&loop, rss);

  /* Calculate the bounds of the scalarization.  */
  gfc_conv_ss_startstride (&loop);

  /* Build an ss for the temporary.  */
  base_type = gfc_typenode_for_spec (&expr->ts);
  if (GFC_ARRAY_TYPE_P (base_type)
		|| GFC_DESCRIPTOR_TYPE_P (base_type))
    base_type = gfc_get_element_type (base_type);

  loop.temp_ss = gfc_get_ss ();;
  loop.temp_ss->type = GFC_SS_TEMP;
  loop.temp_ss->data.temp.type = base_type;

  if (expr->ts.type == BT_CHARACTER)
    {
      gfc_ref *char_ref = expr->ref;

      for (; char_ref; char_ref = char_ref->next)
	if (char_ref->type == REF_SUBSTRING)
	  {
	    gfc_se tmp_se;

	    expr->ts.cl = gfc_get_charlen ();
	    expr->ts.cl->next = char_ref->u.ss.length->next;
	    char_ref->u.ss.length->next = expr->ts.cl;

	    gfc_init_se (&tmp_se, NULL);
	    gfc_conv_expr_type (&tmp_se, char_ref->u.ss.end,
				gfc_array_index_type);
	    tmp = fold_build2 (PLUS_EXPR, gfc_array_index_type,
			       tmp_se.expr, gfc_index_one_node);
	    tmp = gfc_evaluate_now (tmp, &parmse->pre);
	    gfc_init_se (&tmp_se, NULL);
	    gfc_conv_expr_type (&tmp_se, char_ref->u.ss.start,
				gfc_array_index_type);
	    tmp = fold_build2 (MINUS_EXPR, gfc_array_index_type,
			       tmp, tmp_se.expr);
	    expr->ts.cl->backend_decl = tmp;

	    break;
	  }
      loop.temp_ss->data.temp.type
		= gfc_typenode_for_spec (&expr->ts);
      loop.temp_ss->string_length = expr->ts.cl->backend_decl;
    }

  loop.temp_ss->data.temp.dimen = loop.dimen;
  loop.temp_ss->next = gfc_ss_terminator;

  /* Associate the SS with the loop.  */
  gfc_add_ss_to_loop (&loop, loop.temp_ss);

  /* Setup the scalarizing loops.  */
  gfc_conv_loop_setup (&loop);

  /* Pass the temporary descriptor back to the caller.  */
  info = &loop.temp_ss->data.info;
  parmse->expr = info->descriptor;

  /* Setup the gfc_se structures.  */
  gfc_copy_loopinfo_to_se (&lse, &loop);
  gfc_copy_loopinfo_to_se (&rse, &loop);

  rse.ss = rss;
  lse.ss = loop.temp_ss;
  gfc_mark_ss_chain_used (rss, 1);
  gfc_mark_ss_chain_used (loop.temp_ss, 1);

  /* Start the scalarized loop body.  */
  gfc_start_scalarized_body (&loop, &body);

  /* Translate the expression.  */
  gfc_conv_expr (&rse, expr);

  gfc_conv_tmp_array_ref (&lse);
  gfc_advance_se_ss_chain (&lse);

  if (intent != INTENT_OUT)
    {
      tmp = gfc_trans_scalar_assign (&lse, &rse, expr->ts, true, false);
      gfc_add_expr_to_block (&body, tmp);
      gcc_assert (rse.ss == gfc_ss_terminator);
      gfc_trans_scalarizing_loops (&loop, &body);
    }
  else
    {
      /* Make sure that the temporary declaration survives by merging
       all the loop declarations into the current context.  */
      for (n = 0; n < loop.dimen; n++)
	{
	  gfc_merge_block_scope (&body);
	  body = loop.code[loop.order[n]];
	}
      gfc_merge_block_scope (&body);
    }

  /* Add the post block after the second loop, so that any
     freeing of allocated memory is done at the right time.  */
  gfc_add_block_to_block (&parmse->pre, &loop.pre);

  /**********Copy the temporary back again.*********/

  gfc_init_se (&lse, NULL);
  gfc_init_se (&rse, NULL);

  /* Walk the argument expression.  */
  lss = gfc_walk_expr (expr);
  rse.ss = loop.temp_ss;
  lse.ss = lss;

  /* Initialize the scalarizer.  */
  gfc_init_loopinfo (&loop2);
  gfc_add_ss_to_loop (&loop2, lss);

  /* Calculate the bounds of the scalarization.  */
  gfc_conv_ss_startstride (&loop2);

  /* Setup the scalarizing loops.  */
  gfc_conv_loop_setup (&loop2);

  gfc_copy_loopinfo_to_se (&lse, &loop2);
  gfc_copy_loopinfo_to_se (&rse, &loop2);

  gfc_mark_ss_chain_used (lss, 1);
  gfc_mark_ss_chain_used (loop.temp_ss, 1);

  /* Declare the variable to hold the temporary offset and start the
     scalarized loop body.  */
  offset = gfc_create_var (gfc_array_index_type, NULL);
  gfc_start_scalarized_body (&loop2, &body);

  /* Build the offsets for the temporary from the loop variables.  The
     temporary array has lbounds of zero and strides of one in all
     dimensions, so this is very simple.  The offset is only computed
     outside the innermost loop, so the overall transfer could be
     optimized further.  */
  info = &rse.ss->data.info;

  tmp_index = gfc_index_zero_node;
  for (n = info->dimen - 1; n > 0; n--)
    {
      tree tmp_str;
      tmp = rse.loop->loopvar[n];
      tmp = fold_build2 (MINUS_EXPR, gfc_array_index_type,
			 tmp, rse.loop->from[n]);
      tmp = fold_build2 (PLUS_EXPR, gfc_array_index_type,
			 tmp, tmp_index);

      tmp_str = fold_build2 (MINUS_EXPR, gfc_array_index_type,
			     rse.loop->to[n-1], rse.loop->from[n-1]);
      tmp_str = fold_build2 (PLUS_EXPR, gfc_array_index_type,
			     tmp_str, gfc_index_one_node);

      tmp_index = fold_build2 (MULT_EXPR, gfc_array_index_type,
			       tmp, tmp_str);
    }

  tmp_index = fold_build2 (MINUS_EXPR, gfc_array_index_type,
  			   tmp_index, rse.loop->from[0]);
  gfc_add_modify_expr (&rse.loop->code[0], offset, tmp_index);

  tmp_index = fold_build2 (PLUS_EXPR, gfc_array_index_type,
			   rse.loop->loopvar[0], offset);

  /* Now use the offset for the reference.  */
  tmp = build_fold_indirect_ref (info->data);
  rse.expr = gfc_build_array_ref (tmp, tmp_index);

  if (expr->ts.type == BT_CHARACTER)
    rse.string_length = expr->ts.cl->backend_decl;

  gfc_conv_expr (&lse, expr);

  gcc_assert (lse.ss == gfc_ss_terminator);

  tmp = gfc_trans_scalar_assign (&lse, &rse, expr->ts, false, false);
  gfc_add_expr_to_block (&body, tmp);
  
  /* Generate the copying loops.  */
  gfc_trans_scalarizing_loops (&loop2, &body);

  /* Wrap the whole thing up by adding the second loop to the post-block
     and following it by the post-block of the first loop.  In this way,
     if the temporary needs freeing, it is done after use!  */
  if (intent != INTENT_IN)
    {
      gfc_add_block_to_block (&parmse->post, &loop2.pre);
      gfc_add_block_to_block (&parmse->post, &loop2.post);
    }

  gfc_add_block_to_block (&parmse->post, &loop.post);

  gfc_cleanup_loop (&loop);
  gfc_cleanup_loop (&loop2);

  /* Pass the string length to the argument expression.  */
  if (expr->ts.type == BT_CHARACTER)
    parmse->string_length = expr->ts.cl->backend_decl;

  /* We want either the address for the data or the address of the descriptor,
     depending on the mode of passing array arguments.  */
  if (g77)
    parmse->expr = gfc_conv_descriptor_data_get (parmse->expr);
  else
    parmse->expr = build_fold_addr_expr (parmse->expr);

  return;
}

/* Is true if an array reference is followed by a component or substring
   reference.  */

bool
is_aliased_array (gfc_expr * e)
{
  gfc_ref * ref;
  bool seen_array;

  seen_array = false;	
  for (ref = e->ref; ref; ref = ref->next)
    {
      if (ref->type == REF_ARRAY
	    && ref->u.ar.type != AR_ELEMENT)
	seen_array = true;

      if (seen_array
	    && ref->type != REF_ARRAY)
	return seen_array;
    }
  return false;
}

/* Generate the code for argument list functions.  */

static void
conv_arglist_function (gfc_se *se, gfc_expr *expr, const char *name)
{
  tree type = NULL_TREE;
  /* Pass by value for g77 %VAL(arg), pass the address
     indirectly for %LOC, else by reference.  Thus %REF
     is a "do-nothing" and %LOC is the same as an F95
     pointer.  */
  if (strncmp (name, "%VAL", 4) == 0)
    {
      gfc_conv_expr (se, expr);
      /* %VAL converts argument to default kind.  */
      switch (expr->ts.type)
	{
	  case BT_REAL:
	    type = gfc_get_real_type (gfc_default_real_kind);
	    se->expr = fold_convert (type, se->expr);
	    break;
	  case BT_COMPLEX:
	    type = gfc_get_complex_type (gfc_default_complex_kind);
	    se->expr = fold_convert (type, se->expr);
	    break;
	  case BT_INTEGER:
	    type = gfc_get_int_type (gfc_default_integer_kind);
	    se->expr = fold_convert (type, se->expr);
	    break;
	  case BT_LOGICAL:
	    type = gfc_get_logical_type (gfc_default_logical_kind);
	    se->expr = fold_convert (type, se->expr);
	    break;
	  /* This should have been resolved away.  */
	  case BT_UNKNOWN: case BT_CHARACTER: case BT_DERIVED:
	  case BT_PROCEDURE: case BT_HOLLERITH:
	    gfc_internal_error ("Bad type in conv_arglist_function");
	}
	  
    }
  else if (strncmp (name, "%LOC", 4) == 0)
    {
      gfc_conv_expr_reference (se, expr);
      se->expr = gfc_build_addr_expr (NULL, se->expr);
    }
  else if (strncmp (name, "%REF", 4) == 0)
    gfc_conv_expr_reference (se, expr);
  else
    gfc_error ("Unknown argument list function at %L", &expr->where);
}


/* Generate code for a procedure call.  Note can return se->post != NULL.
   If se->direct_byref is set then se->expr contains the return parameter.
   Return nonzero, if the call has alternate specifiers.  */

int
gfc_conv_function_call (gfc_se * se, gfc_symbol * sym,
			gfc_actual_arglist * arg)
{
  gfc_interface_mapping mapping;
  tree arglist;
  tree retargs;
  tree tmp;
  tree fntype;
  gfc_se parmse;
  gfc_ss *argss;
  gfc_ss_info *info;
  int byref;
  int parm_kind;
  tree type;
  tree var;
  tree len;
  tree stringargs;
  gfc_formal_arglist *formal;
  int has_alternate_specifier = 0;
  bool need_interface_mapping;
  bool callee_alloc;
  gfc_typespec ts;
  gfc_charlen cl;
  gfc_expr *e;
  gfc_symbol *fsym;
  stmtblock_t post;
  enum {MISSING = 0, ELEMENTAL, SCALAR, SCALAR_POINTER, ARRAY};

  arglist = NULL_TREE;
  retargs = NULL_TREE;
  stringargs = NULL_TREE;
  var = NULL_TREE;
  len = NULL_TREE;

  if (se->ss != NULL)
    {
      if (!sym->attr.elemental)
	{
	  gcc_assert (se->ss->type == GFC_SS_FUNCTION);
          if (se->ss->useflags)
            {
              gcc_assert (gfc_return_by_reference (sym)
                      && sym->result->attr.dimension);
              gcc_assert (se->loop != NULL);

              /* Access the previously obtained result.  */
              gfc_conv_tmp_array_ref (se);
              gfc_advance_se_ss_chain (se);
              return 0;
            }
	}
      info = &se->ss->data.info;
    }
  else
    info = NULL;

  gfc_init_block (&post);
  gfc_init_interface_mapping (&mapping);
  need_interface_mapping = ((sym->ts.type == BT_CHARACTER
				  && sym->ts.cl->length
				  && sym->ts.cl->length->expr_type
						!= EXPR_CONSTANT)
			      || sym->attr.dimension);
  formal = sym->formal;
  /* Evaluate the arguments.  */
  for (; arg != NULL; arg = arg->next, formal = formal ? formal->next : NULL)
    {
      e = arg->expr;
      fsym = formal ? formal->sym : NULL;
      parm_kind = MISSING;
      if (e == NULL)
	{

	  if (se->ignore_optional)
	    {
	      /* Some intrinsics have already been resolved to the correct
	         parameters.  */
	      continue;
	    }
	  else if (arg->label)
	    {
              has_alternate_specifier = 1;
              continue;
	    }
	  else
	    {
	      /* Pass a NULL pointer for an absent arg.  */
	      gfc_init_se (&parmse, NULL);
	      parmse.expr = null_pointer_node;
              if (arg->missing_arg_type == BT_CHARACTER)
		parmse.string_length = build_int_cst (gfc_charlen_type_node, 0);
	    }
	}
      else if (se->ss && se->ss->useflags)
	{
	  /* An elemental function inside a scalarized loop.  */
          gfc_init_se (&parmse, se);
          gfc_conv_expr_reference (&parmse, e);
	  parm_kind = ELEMENTAL;
	}
      else
	{
	  /* A scalar or transformational function.  */
	  gfc_init_se (&parmse, NULL);
	  argss = gfc_walk_expr (e);

	  if (argss == gfc_ss_terminator)
            {
	      parm_kind = SCALAR;
	      if (arg->name && arg->name[0] == '%')
		/* Argument list functions %VAL, %LOC and %REF are signalled
		   through arg->name.  */
		conv_arglist_function (&parmse, arg->expr, arg->name);
	      else
		{
		  gfc_conv_expr_reference (&parmse, e);
		  if (fsym && fsym->attr.pointer
			&& e->expr_type != EXPR_NULL)
		    {
		      /* Scalar pointer dummy args require an extra level of
			 indirection. The null pointer already contains
			 this level of indirection.  */
		      parm_kind = SCALAR_POINTER;
		      parmse.expr = build_fold_addr_expr (parmse.expr);
		    }
		}
            }
	  else
	    {
              /* If the procedure requires an explicit interface, the actual
                 argument is passed according to the corresponding formal
                 argument.  If the corresponding formal argument is a POINTER,
                 ALLOCATABLE or assumed shape, we do not use g77's calling
                 convention, and pass the address of the array descriptor
                 instead. Otherwise we use g77's calling convention.  */
	      int f;
	      f = (fsym != NULL)
		  && !(fsym->attr.pointer || fsym->attr.allocatable)
		  && fsym->as->type != AS_ASSUMED_SHAPE;
	      f = f || !sym->attr.always_explicit;

	      if (e->expr_type == EXPR_VARIABLE
		    && is_aliased_array (e))
		/* The actual argument is a component reference to an
		   array of derived types.  In this case, the argument
		   is converted to a temporary, which is passed and then
		   written back after the procedure call.  */
		gfc_conv_aliased_arg (&parmse, e, f,
			fsym ? fsym->attr.intent : INTENT_INOUT);
	      else
	        gfc_conv_array_parameter (&parmse, e, argss, f);

              /* If an ALLOCATABLE dummy argument has INTENT(OUT) and is 
                 allocated on entry, it must be deallocated.  */
              if (fsym && fsym->attr.allocatable
                  && fsym->attr.intent == INTENT_OUT)
                {
                  tmp = build_fold_indirect_ref (parmse.expr);
                  tmp = gfc_trans_dealloc_allocated (tmp);
                  gfc_add_expr_to_block (&se->pre, tmp);
                }

	    } 
	}

      if (fsym)
	{
	  if (e)
	    {
	      /* If an optional argument is itself an optional dummy
		 argument, check its presence and substitute a null
		 if absent.  */
	      if (e->expr_type == EXPR_VARIABLE
		    && e->symtree->n.sym->attr.optional
		    && fsym->attr.optional)
		gfc_conv_missing_dummy (&parmse, e, fsym->ts);

	      /* If an INTENT(OUT) dummy of derived type has a default
		 initializer, it must be (re)initialized here.  */
	      if (fsym->attr.intent == INTENT_OUT
		    && fsym->ts.type == BT_DERIVED
		    && fsym->value)
		{
		  gcc_assert (!fsym->attr.allocatable);
		  tmp = gfc_trans_assignment (e, fsym->value, false);
		  gfc_add_expr_to_block (&se->pre, tmp);
		}

	      /* Obtain the character length of an assumed character
		 length procedure from the typespec.  */
	      if (fsym->ts.type == BT_CHARACTER
		    && parmse.string_length == NULL_TREE
		    && e->ts.type == BT_PROCEDURE
		    && e->symtree->n.sym->ts.type == BT_CHARACTER
		    && e->symtree->n.sym->ts.cl->length != NULL)
		{
		  gfc_conv_const_charlen (e->symtree->n.sym->ts.cl);
		  parmse.string_length
			= e->symtree->n.sym->ts.cl->backend_decl;
		}
	    }

	  if (need_interface_mapping)
	    gfc_add_interface_mapping (&mapping, fsym, &parmse);
	}

      gfc_add_block_to_block (&se->pre, &parmse.pre);
      gfc_add_block_to_block (&post, &parmse.post);

      /* Allocated allocatable components of derived types must be
	 deallocated for INTENT(OUT) dummy arguments and non-variable
         scalars.  Non-variable arrays are dealt with in trans-array.c
         (gfc_conv_array_parameter).  */
      if (e && e->ts.type == BT_DERIVED
	    && e->ts.derived->attr.alloc_comp
	    && ((formal && formal->sym->attr.intent == INTENT_OUT)
		   ||
		(e->expr_type != EXPR_VARIABLE && !e->rank)))
        {
	  int parm_rank;
	  tmp = build_fold_indirect_ref (parmse.expr);
	  parm_rank = e->rank;
	  switch (parm_kind)
	    {
	    case (ELEMENTAL):
	    case (SCALAR):
	      parm_rank = 0;
	      break;

	    case (SCALAR_POINTER):
              tmp = build_fold_indirect_ref (tmp);
	      break;
	    case (ARRAY):
              tmp = parmse.expr;
	      break;
	    }

          tmp = gfc_deallocate_alloc_comp (e->ts.derived, tmp, parm_rank);
	  if (e->expr_type == EXPR_VARIABLE && e->symtree->n.sym->attr.optional)
	    tmp = build3_v (COND_EXPR, gfc_conv_expr_present (e->symtree->n.sym),
			    tmp, build_empty_stmt ());

	  if (e->expr_type != EXPR_VARIABLE)
	    /* Don't deallocate non-variables until they have been used.  */
	    gfc_add_expr_to_block (&se->post, tmp);
	  else 
	    {
	      gcc_assert (formal && formal->sym->attr.intent == INTENT_OUT);
	      gfc_add_expr_to_block (&se->pre, tmp);
	    }
        }

      /* Character strings are passed as two parameters, a length and a
         pointer.  */
      if (parmse.string_length != NULL_TREE)
        stringargs = gfc_chainon_list (stringargs, parmse.string_length);

      arglist = gfc_chainon_list (arglist, parmse.expr);
    }
  gfc_finish_interface_mapping (&mapping, &se->pre, &se->post);

  ts = sym->ts;
  if (ts.type == BT_CHARACTER)
    {
      if (sym->ts.cl->length == NULL)
	{
	  /* Assumed character length results are not allowed by 5.1.1.5 of the
	     standard and are trapped in resolve.c; except in the case of SPREAD
	     (and other intrinsics?) and dummy functions.  In the case of SPREAD,
	     we take the character length of the first argument for the result.
	     For dummies, we have to look through the formal argument list for
	     this function and use the character length found there.*/
	  if (!sym->attr.dummy)
	    cl.backend_decl = TREE_VALUE (stringargs);
	  else
	    {
	      formal = sym->ns->proc_name->formal;
	      for (; formal; formal = formal->next)
		if (strcmp (formal->sym->name, sym->name) == 0)
		  cl.backend_decl = formal->sym->ts.cl->backend_decl;
	    }
        }
        else
        {
	  /* Calculate the length of the returned string.  */
	  gfc_init_se (&parmse, NULL);
	  if (need_interface_mapping)
	    gfc_apply_interface_mapping (&mapping, &parmse, sym->ts.cl->length);
	  else
	    gfc_conv_expr (&parmse, sym->ts.cl->length);
	  gfc_add_block_to_block (&se->pre, &parmse.pre);
	  gfc_add_block_to_block (&se->post, &parmse.post);
	  cl.backend_decl = fold_convert (gfc_charlen_type_node, parmse.expr);
	}

      /* Set up a charlen structure for it.  */
      cl.next = NULL;
      cl.length = NULL;
      ts.cl = &cl;

      len = cl.backend_decl;
    }

  byref = gfc_return_by_reference (sym);
  if (byref)
    {
      if (se->direct_byref)
	retargs = gfc_chainon_list (retargs, se->expr);
      else if (sym->result->attr.dimension)
	{
	  gcc_assert (se->loop && info);

	  /* Set the type of the array.  */
	  tmp = gfc_typenode_for_spec (&ts);
	  info->dimen = se->loop->dimen;

	  /* Evaluate the bounds of the result, if known.  */
	  gfc_set_loop_bounds_from_array_spec (&mapping, se, sym->result->as);

	  /* Create a temporary to store the result.  In case the function
	     returns a pointer, the temporary will be a shallow copy and
	     mustn't be deallocated.  */
	  callee_alloc = sym->attr.allocatable || sym->attr.pointer;
	  gfc_trans_create_temp_array (&se->pre, &se->post, se->loop, info, tmp,
				       false, !sym->attr.pointer, callee_alloc);

	  /* Pass the temporary as the first argument.  */
	  tmp = info->descriptor;
	  tmp = build_fold_addr_expr (tmp);
	  retargs = gfc_chainon_list (retargs, tmp);
	}
      else if (ts.type == BT_CHARACTER)
	{
	  /* Pass the string length.  */
	  type = gfc_get_character_type (ts.kind, ts.cl);
	  type = build_pointer_type (type);

	  /* Return an address to a char[0:len-1]* temporary for
	     character pointers.  */
	  if (sym->attr.pointer || sym->attr.allocatable)
	    {
	      /* Build char[0:len-1] * pstr.  */
	      tmp = fold_build2 (MINUS_EXPR, gfc_charlen_type_node, len,
				 build_int_cst (gfc_charlen_type_node, 1));
	      tmp = build_range_type (gfc_array_index_type,
				      gfc_index_zero_node, tmp);
	      tmp = build_array_type (gfc_character1_type_node, tmp);
	      var = gfc_create_var (build_pointer_type (tmp), "pstr");

	      /* Provide an address expression for the function arguments.  */
	      var = build_fold_addr_expr (var);
	    }
	  else
	    var = gfc_conv_string_tmp (se, type, len);

	  retargs = gfc_chainon_list (retargs, var);
	}
      else
	{
	  gcc_assert (gfc_option.flag_f2c && ts.type == BT_COMPLEX);

	  type = gfc_get_complex_type (ts.kind);
	  var = build_fold_addr_expr (gfc_create_var (type, "cmplx"));
	  retargs = gfc_chainon_list (retargs, var);
	}

      /* Add the string length to the argument list.  */
      if (ts.type == BT_CHARACTER)
	retargs = gfc_chainon_list (retargs, len);
    }
  gfc_free_interface_mapping (&mapping);

  /* Add the return arguments.  */
  arglist = chainon (retargs, arglist);

  /* Add the hidden string length parameters to the arguments.  */
  arglist = chainon (arglist, stringargs);

  /* Generate the actual call.  */
  gfc_conv_function_val (se, sym);

  /* If there are alternate return labels, function type should be
     integer.  Can't modify the type in place though, since it can be shared
     with other functions.  For dummy arguments, the typing is done to
     to this result, even if it has to be repeated for each call.  */
  if (has_alternate_specifier
      && TREE_TYPE (TREE_TYPE (TREE_TYPE (se->expr))) != integer_type_node)
    {
      if (!sym->attr.dummy)
	{
	  TREE_TYPE (sym->backend_decl)
		= build_function_type (integer_type_node,
		      TYPE_ARG_TYPES (TREE_TYPE (sym->backend_decl)));
	  se->expr = build_fold_addr_expr (sym->backend_decl);
	}
      else
	TREE_TYPE (TREE_TYPE (TREE_TYPE (se->expr))) = integer_type_node;
    }

  fntype = TREE_TYPE (TREE_TYPE (se->expr));
  se->expr = build3 (CALL_EXPR, TREE_TYPE (fntype), se->expr,
		     arglist, NULL_TREE);

  /* If we have a pointer function, but we don't want a pointer, e.g.
     something like
        x = f()
     where f is pointer valued, we have to dereference the result.  */
  if (!se->want_pointer && !byref && sym->attr.pointer)
    se->expr = build_fold_indirect_ref (se->expr);

  /* f2c calling conventions require a scalar default real function to
     return a double precision result.  Convert this back to default
     real.  We only care about the cases that can happen in Fortran 77.
  */
  if (gfc_option.flag_f2c && sym->ts.type == BT_REAL
      && sym->ts.kind == gfc_default_real_kind
      && !sym->attr.always_explicit)
    se->expr = fold_convert (gfc_get_real_type (sym->ts.kind), se->expr);

  /* A pure function may still have side-effects - it may modify its
     parameters.  */
  TREE_SIDE_EFFECTS (se->expr) = 1;
#if 0
  if (!sym->attr.pure)
    TREE_SIDE_EFFECTS (se->expr) = 1;
#endif

  if (byref)
    {
      /* Add the function call to the pre chain.  There is no expression.  */
      gfc_add_expr_to_block (&se->pre, se->expr);
      se->expr = NULL_TREE;

      if (!se->direct_byref)
	{
	  if (sym->attr.dimension)
	    {
	      if (flag_bounds_check)
		{
		  /* Check the data pointer hasn't been modified.  This would
		     happen in a function returning a pointer.  */
		  tmp = gfc_conv_descriptor_data_get (info->descriptor);
		  tmp = fold_build2 (NE_EXPR, boolean_type_node,
				     tmp, info->data);
		  gfc_trans_runtime_check (tmp, gfc_msg_fault, &se->pre, NULL);
		}
	      se->expr = info->descriptor;
	      /* Bundle in the string length.  */
	      se->string_length = len;
	    }
	  else if (sym->ts.type == BT_CHARACTER)
	    {
	      /* Dereference for character pointer results.  */
	      if (sym->attr.pointer || sym->attr.allocatable)
		se->expr = build_fold_indirect_ref (var);
	      else
	        se->expr = var;

	      se->string_length = len;
	    }
	  else
	    {
	      gcc_assert (sym->ts.type == BT_COMPLEX && gfc_option.flag_f2c);
	      se->expr = build_fold_indirect_ref (var);
	    }
	}
    }

  /* Follow the function call with the argument post block.  */
  if (byref)
    gfc_add_block_to_block (&se->pre, &post);
  else
    gfc_add_block_to_block (&se->post, &post);

  return has_alternate_specifier;
}


/* Generate code to copy a string.  */

static void
gfc_trans_string_copy (stmtblock_t * block, tree dlength, tree dest,
		       tree slength, tree src)
{
  tree tmp, dlen, slen;
  tree dsc;
  tree ssc;
  tree cond;
  tree cond2;
  tree tmp2;
  tree tmp3;
  tree tmp4;
  stmtblock_t tempblock;

  dlen = fold_convert (size_type_node, gfc_evaluate_now (dlength, block));
  slen = fold_convert (size_type_node, gfc_evaluate_now (slength, block));

  /* Deal with single character specially.  */
  dsc = gfc_to_single_character (dlen, dest);
  ssc = gfc_to_single_character (slen, src);
  if (dsc != NULL_TREE && ssc != NULL_TREE)
    {
      gfc_add_modify_expr (block, dsc, ssc);
      return;
    }

  /* Do nothing if the destination length is zero.  */
  cond = fold_build2 (GT_EXPR, boolean_type_node, dlen,
		      build_int_cst (gfc_charlen_type_node, 0));

  /* The following code was previously in _gfortran_copy_string:

       // The two strings may overlap so we use memmove.
       void
       copy_string (GFC_INTEGER_4 destlen, char * dest,
                    GFC_INTEGER_4 srclen, const char * src)
       {
         if (srclen >= destlen)
           {
             // This will truncate if too long.
             memmove (dest, src, destlen);
           }
         else
           {
             memmove (dest, src, srclen);
             // Pad with spaces.
             memset (&dest[srclen], ' ', destlen - srclen);
           }
       }

     We're now doing it here for better optimization, but the logic
     is the same.  */
  
  /* Truncate string if source is too long.  */
  cond2 = fold_build2 (GE_EXPR, boolean_type_node, slen, dlen);
  tmp2 = gfc_chainon_list (NULL_TREE, dest);
  tmp2 = gfc_chainon_list (tmp2, src);
  tmp2 = gfc_chainon_list (tmp2, dlen);
  tmp2 = build_function_call_expr (built_in_decls[BUILT_IN_MEMMOVE], tmp2);

  /* Else copy and pad with spaces.  */
  tmp3 = gfc_chainon_list (NULL_TREE, dest);
  tmp3 = gfc_chainon_list (tmp3, src);
  tmp3 = gfc_chainon_list (tmp3, slen);
  tmp3 = build_function_call_expr (built_in_decls[BUILT_IN_MEMMOVE], tmp3);

  tmp4 = fold_build2 (PLUS_EXPR, pchar_type_node, dest,
		      fold_convert (pchar_type_node, slen));
  tmp4 = gfc_chainon_list (NULL_TREE, tmp4);
  tmp4 = gfc_chainon_list (tmp4, build_int_cst
				   (gfc_get_int_type (gfc_c_int_kind),
				    lang_hooks.to_target_charset (' ')));
  tmp4 = gfc_chainon_list (tmp4, fold_build2 (MINUS_EXPR, TREE_TYPE(dlen),
					      dlen, slen));
  tmp4 = build_function_call_expr (built_in_decls[BUILT_IN_MEMSET], tmp4);

  gfc_init_block (&tempblock);
  gfc_add_expr_to_block (&tempblock, tmp3);
  gfc_add_expr_to_block (&tempblock, tmp4);
  tmp3 = gfc_finish_block (&tempblock);

  /* The whole copy_string function is there.  */
  tmp = fold_build3 (COND_EXPR, void_type_node, cond2, tmp2, tmp3);
  tmp = fold_build3 (COND_EXPR, void_type_node, cond, tmp, build_empty_stmt ());
  gfc_add_expr_to_block (block, tmp);
}


/* Translate a statement function.
   The value of a statement function reference is obtained by evaluating the
   expression using the values of the actual arguments for the values of the
   corresponding dummy arguments.  */

static void
gfc_conv_statement_function (gfc_se * se, gfc_expr * expr)
{
  gfc_symbol *sym;
  gfc_symbol *fsym;
  gfc_formal_arglist *fargs;
  gfc_actual_arglist *args;
  gfc_se lse;
  gfc_se rse;
  gfc_saved_var *saved_vars;
  tree *temp_vars;
  tree type;
  tree tmp;
  int n;

  sym = expr->symtree->n.sym;
  args = expr->value.function.actual;
  gfc_init_se (&lse, NULL);
  gfc_init_se (&rse, NULL);

  n = 0;
  for (fargs = sym->formal; fargs; fargs = fargs->next)
    n++;
  saved_vars = (gfc_saved_var *)gfc_getmem (n * sizeof (gfc_saved_var));
  temp_vars = (tree *)gfc_getmem (n * sizeof (tree));

  for (fargs = sym->formal, n = 0; fargs; fargs = fargs->next, n++)
    {
      /* Each dummy shall be specified, explicitly or implicitly, to be
         scalar.  */
      gcc_assert (fargs->sym->attr.dimension == 0);
      fsym = fargs->sym;

      /* Create a temporary to hold the value.  */
      type = gfc_typenode_for_spec (&fsym->ts);
      temp_vars[n] = gfc_create_var (type, fsym->name);

      if (fsym->ts.type == BT_CHARACTER)
        {
	  /* Copy string arguments.  */
          tree arglen;

          gcc_assert (fsym->ts.cl && fsym->ts.cl->length
                  && fsym->ts.cl->length->expr_type == EXPR_CONSTANT);

          arglen = TYPE_MAX_VALUE (TYPE_DOMAIN (type));
          tmp = gfc_build_addr_expr (build_pointer_type (type),
				     temp_vars[n]);

          gfc_conv_expr (&rse, args->expr);
          gfc_conv_string_parameter (&rse);
          gfc_add_block_to_block (&se->pre, &lse.pre);
          gfc_add_block_to_block (&se->pre, &rse.pre);

	  gfc_trans_string_copy (&se->pre, arglen, tmp, rse.string_length,
				 rse.expr);
          gfc_add_block_to_block (&se->pre, &lse.post);
          gfc_add_block_to_block (&se->pre, &rse.post);
        }
      else
        {
          /* For everything else, just evaluate the expression.  */
          gfc_conv_expr (&lse, args->expr);

          gfc_add_block_to_block (&se->pre, &lse.pre);
          gfc_add_modify_expr (&se->pre, temp_vars[n], lse.expr);
          gfc_add_block_to_block (&se->pre, &lse.post);
        }

      args = args->next;
    }

  /* Use the temporary variables in place of the real ones.  */
  for (fargs = sym->formal, n = 0; fargs; fargs = fargs->next, n++)
    gfc_shadow_sym (fargs->sym, temp_vars[n], &saved_vars[n]);

  gfc_conv_expr (se, sym->value);

  if (sym->ts.type == BT_CHARACTER)
    {
      gfc_conv_const_charlen (sym->ts.cl);

      /* Force the expression to the correct length.  */
      if (!INTEGER_CST_P (se->string_length)
	  || tree_int_cst_lt (se->string_length,
			      sym->ts.cl->backend_decl))
	{
	  type = gfc_get_character_type (sym->ts.kind, sym->ts.cl);
	  tmp = gfc_create_var (type, sym->name);
	  tmp = gfc_build_addr_expr (build_pointer_type (type), tmp);
	  gfc_trans_string_copy (&se->pre, sym->ts.cl->backend_decl, tmp,
				 se->string_length, se->expr);
	  se->expr = tmp;
	}
      se->string_length = sym->ts.cl->backend_decl;
    }

  /* Restore the original variables.  */
  for (fargs = sym->formal, n = 0; fargs; fargs = fargs->next, n++)
    gfc_restore_sym (fargs->sym, &saved_vars[n]);
  gfc_free (saved_vars);
}


/* Translate a function expression.  */

static void
gfc_conv_function_expr (gfc_se * se, gfc_expr * expr)
{
  gfc_symbol *sym;

  if (expr->value.function.isym)
    {
      gfc_conv_intrinsic_function (se, expr);
      return;
    }

  /* We distinguish statement functions from general functions to improve
     runtime performance.  */
  if (expr->symtree->n.sym->attr.proc == PROC_ST_FUNCTION)
    {
      gfc_conv_statement_function (se, expr);
      return;
    }

  /* expr.value.function.esym is the resolved (specific) function symbol for
     most functions.  However this isn't set for dummy procedures.  */
  sym = expr->value.function.esym;
  if (!sym)
    sym = expr->symtree->n.sym;
  gfc_conv_function_call (se, sym, expr->value.function.actual);
}


static void
gfc_conv_array_constructor_expr (gfc_se * se, gfc_expr * expr)
{
  gcc_assert (se->ss != NULL && se->ss != gfc_ss_terminator);
  gcc_assert (se->ss->expr == expr && se->ss->type == GFC_SS_CONSTRUCTOR);

  gfc_conv_tmp_array_ref (se);
  gfc_advance_se_ss_chain (se);
}


/* Build a static initializer.  EXPR is the expression for the initial value.
   The other parameters describe the variable of the component being 
   initialized. EXPR may be null.  */

tree
gfc_conv_initializer (gfc_expr * expr, gfc_typespec * ts, tree type,
		      bool array, bool pointer)
{
  gfc_se se;

  if (!(expr || pointer))
    return NULL_TREE;

  if (array)
    {
      /* Arrays need special handling.  */
      if (pointer)
	return gfc_build_null_descriptor (type);
      else
	return gfc_conv_array_initializer (type, expr);
    }
  else if (pointer)
    return fold_convert (type, null_pointer_node);
  else
    {
      switch (ts->type)
	{
	case BT_DERIVED:
	  gfc_init_se (&se, NULL);
	  gfc_conv_structure (&se, expr, 1);
	  return se.expr;

	case BT_CHARACTER:
	  return gfc_conv_string_init (ts->cl->backend_decl,expr);

	default:
	  gfc_init_se (&se, NULL);
	  gfc_conv_constant (&se, expr);
	  return se.expr;
	}
    }
}
  
static tree
gfc_trans_subarray_assign (tree dest, gfc_component * cm, gfc_expr * expr)
{
  gfc_se rse;
  gfc_se lse;
  gfc_ss *rss;
  gfc_ss *lss;
  stmtblock_t body;
  stmtblock_t block;
  gfc_loopinfo loop;
  int n;
  tree tmp;

  gfc_start_block (&block);

  /* Initialize the scalarizer.  */
  gfc_init_loopinfo (&loop);

  gfc_init_se (&lse, NULL);
  gfc_init_se (&rse, NULL);

  /* Walk the rhs.  */
  rss = gfc_walk_expr (expr);
  if (rss == gfc_ss_terminator)
    {
      /* The rhs is scalar.  Add a ss for the expression.  */
      rss = gfc_get_ss ();
      rss->next = gfc_ss_terminator;
      rss->type = GFC_SS_SCALAR;
      rss->expr = expr;
    }

  /* Create a SS for the destination.  */
  lss = gfc_get_ss ();
  lss->type = GFC_SS_COMPONENT;
  lss->expr = NULL;
  lss->shape = gfc_get_shape (cm->as->rank);
  lss->next = gfc_ss_terminator;
  lss->data.info.dimen = cm->as->rank;
  lss->data.info.descriptor = dest;
  lss->data.info.data = gfc_conv_array_data (dest);
  lss->data.info.offset = gfc_conv_array_offset (dest);
  for (n = 0; n < cm->as->rank; n++)
    {
      lss->data.info.dim[n] = n;
      lss->data.info.start[n] = gfc_conv_array_lbound (dest, n);
      lss->data.info.stride[n] = gfc_index_one_node;

      mpz_init (lss->shape[n]);
      mpz_sub (lss->shape[n], cm->as->upper[n]->value.integer,
	       cm->as->lower[n]->value.integer);
      mpz_add_ui (lss->shape[n], lss->shape[n], 1);
    }
  
  /* Associate the SS with the loop.  */
  gfc_add_ss_to_loop (&loop, lss);
  gfc_add_ss_to_loop (&loop, rss);

  /* Calculate the bounds of the scalarization.  */
  gfc_conv_ss_startstride (&loop);

  /* Setup the scalarizing loops.  */
  gfc_conv_loop_setup (&loop);

  /* Setup the gfc_se structures.  */
  gfc_copy_loopinfo_to_se (&lse, &loop);
  gfc_copy_loopinfo_to_se (&rse, &loop);

  rse.ss = rss;
  gfc_mark_ss_chain_used (rss, 1);
  lse.ss = lss;
  gfc_mark_ss_chain_used (lss, 1);

  /* Start the scalarized loop body.  */
  gfc_start_scalarized_body (&loop, &body);

  gfc_conv_tmp_array_ref (&lse);
  if (cm->ts.type == BT_CHARACTER)
    lse.string_length = cm->ts.cl->backend_decl;

  gfc_conv_expr (&rse, expr);

  tmp = gfc_trans_scalar_assign (&lse, &rse, cm->ts, true, false);
  gfc_add_expr_to_block (&body, tmp);

  gcc_assert (rse.ss == gfc_ss_terminator);

  /* Generate the copying loops.  */
  gfc_trans_scalarizing_loops (&loop, &body);

  /* Wrap the whole thing up.  */
  gfc_add_block_to_block (&block, &loop.pre);
  gfc_add_block_to_block (&block, &loop.post);

  for (n = 0; n < cm->as->rank; n++)
    mpz_clear (lss->shape[n]);
  gfc_free (lss->shape);

  gfc_cleanup_loop (&loop);

  return gfc_finish_block (&block);
}


/* Assign a single component of a derived type constructor.  */

static tree
gfc_trans_subcomponent_assign (tree dest, gfc_component * cm, gfc_expr * expr)
{
  gfc_se se;
  gfc_se lse;
  gfc_ss *rss;
  stmtblock_t block;
  tree tmp;
  tree offset;
  int n;

  gfc_start_block (&block);

  if (cm->pointer)
    {
      gfc_init_se (&se, NULL);
      /* Pointer component.  */
      if (cm->dimension)
	{
	  /* Array pointer.  */
	  if (expr->expr_type == EXPR_NULL)
	    gfc_conv_descriptor_data_set (&block, dest, null_pointer_node);
	  else
	    {
	      rss = gfc_walk_expr (expr);
	      se.direct_byref = 1;
	      se.expr = dest;
	      gfc_conv_expr_descriptor (&se, expr, rss);
	      gfc_add_block_to_block (&block, &se.pre);
	      gfc_add_block_to_block (&block, &se.post);
	    }
	}
      else
	{
	  /* Scalar pointers.  */
	  se.want_pointer = 1;
	  gfc_conv_expr (&se, expr);
	  gfc_add_block_to_block (&block, &se.pre);
	  gfc_add_modify_expr (&block, dest,
			       fold_convert (TREE_TYPE (dest), se.expr));
	  gfc_add_block_to_block (&block, &se.post);
	}
    }
  else if (cm->dimension)
    {
      if (cm->allocatable && expr->expr_type == EXPR_NULL)
 	gfc_conv_descriptor_data_set (&block, dest, null_pointer_node);
      else if (cm->allocatable)
        {
          tree tmp2;

          gfc_init_se (&se, NULL);
 
	  rss = gfc_walk_expr (expr);
          se.want_pointer = 0;
          gfc_conv_expr_descriptor (&se, expr, rss);
	  gfc_add_block_to_block (&block, &se.pre);

	  tmp = fold_convert (TREE_TYPE (dest), se.expr);
	  gfc_add_modify_expr (&block, dest, tmp);

          if (cm->ts.type == BT_DERIVED && cm->ts.derived->attr.alloc_comp)
	    tmp = gfc_copy_alloc_comp (cm->ts.derived, se.expr, dest,
				       cm->as->rank);
	  else
            tmp = gfc_duplicate_allocatable (dest, se.expr,
					     TREE_TYPE(cm->backend_decl),
					     cm->as->rank);

          gfc_add_expr_to_block (&block, tmp);

          gfc_add_block_to_block (&block, &se.post);
          gfc_conv_descriptor_data_set (&block, se.expr, null_pointer_node);

          /* Shift the lbound and ubound of temporaries to being unity, rather
             than zero, based.  Calculate the offset for all cases.  */
          offset = gfc_conv_descriptor_offset (dest);
          gfc_add_modify_expr (&block, offset, gfc_index_zero_node);
          tmp2 =gfc_create_var (gfc_array_index_type, NULL);
          for (n = 0; n < expr->rank; n++)
            {
              if (expr->expr_type != EXPR_VARIABLE
                  && expr->expr_type != EXPR_CONSTANT)
                {
                  tmp = gfc_conv_descriptor_ubound (dest, gfc_rank_cst[n]);
                  gfc_add_modify_expr (&block, tmp,
                                       fold_build2 (PLUS_EXPR,
				      		    gfc_array_index_type,
                                                    tmp, gfc_index_one_node));
                  tmp = gfc_conv_descriptor_lbound (dest, gfc_rank_cst[n]);
                  gfc_add_modify_expr (&block, tmp, gfc_index_one_node);
                }
              tmp = fold_build2 (MULT_EXPR, gfc_array_index_type,
                                 gfc_conv_descriptor_lbound (dest,
							     gfc_rank_cst[n]),
                                 gfc_conv_descriptor_stride (dest,
							     gfc_rank_cst[n]));
              gfc_add_modify_expr (&block, tmp2, tmp);
              tmp = fold_build2 (MINUS_EXPR, gfc_array_index_type, offset, tmp2);
              gfc_add_modify_expr (&block, offset, tmp);
            }
        }
      else
        {
	  tmp = gfc_trans_subarray_assign (dest, cm, expr);
	  gfc_add_expr_to_block (&block, tmp);
        }
    }
  else if (expr->ts.type == BT_DERIVED)
    {
      if (expr->expr_type != EXPR_STRUCTURE)
	{
	  gfc_init_se (&se, NULL);
	  gfc_conv_expr (&se, expr);
	  gfc_add_modify_expr (&block, dest,
			       fold_convert (TREE_TYPE (dest), se.expr));
	}
      else
	{
	  /* Nested constructors.  */
	  tmp = gfc_trans_structure_assign (dest, expr);
	  gfc_add_expr_to_block (&block, tmp);
	}
    }
  else
    {
      /* Scalar component.  */
      gfc_init_se (&se, NULL);
      gfc_init_se (&lse, NULL);

      gfc_conv_expr (&se, expr);
      if (cm->ts.type == BT_CHARACTER)
	lse.string_length = cm->ts.cl->backend_decl;
      lse.expr = dest;
      tmp = gfc_trans_scalar_assign (&lse, &se, cm->ts, true, false);
      gfc_add_expr_to_block (&block, tmp);
    }
  return gfc_finish_block (&block);
}

/* Assign a derived type constructor to a variable.  */

static tree
gfc_trans_structure_assign (tree dest, gfc_expr * expr)
{
  gfc_constructor *c;
  gfc_component *cm;
  stmtblock_t block;
  tree field;
  tree tmp;

  gfc_start_block (&block);
  cm = expr->ts.derived->components;
  for (c = expr->value.constructor; c; c = c->next, cm = cm->next)
    {
      /* Skip absent members in default initializers.  */
      if (!c->expr)
        continue;

      field = cm->backend_decl;
      tmp = build3 (COMPONENT_REF, TREE_TYPE (field), dest, field, NULL_TREE);
      tmp = gfc_trans_subcomponent_assign (tmp, cm, c->expr);
      gfc_add_expr_to_block (&block, tmp);
    }
  return gfc_finish_block (&block);
}

/* Build an expression for a constructor. If init is nonzero then
   this is part of a static variable initializer.  */

void
gfc_conv_structure (gfc_se * se, gfc_expr * expr, int init)
{
  gfc_constructor *c;
  gfc_component *cm;
  tree val;
  tree type;
  tree tmp;
  VEC(constructor_elt,gc) *v = NULL;

  gcc_assert (se->ss == NULL);
  gcc_assert (expr->expr_type == EXPR_STRUCTURE);
  type = gfc_typenode_for_spec (&expr->ts);

  if (!init)
    {
      /* Create a temporary variable and fill it in.  */
      se->expr = gfc_create_var (type, expr->ts.derived->name);
      tmp = gfc_trans_structure_assign (se->expr, expr);
      gfc_add_expr_to_block (&se->pre, tmp);
      return;
    }

  cm = expr->ts.derived->components;

  for (c = expr->value.constructor; c; c = c->next, cm = cm->next)
    {
      /* Skip absent members in default initializers and allocatable
	 components.  Although the latter have a default initializer
	 of EXPR_NULL,... by default, the static nullify is not needed
	 since this is done every time we come into scope.  */
      if (!c->expr || cm->allocatable)
        continue;

      val = gfc_conv_initializer (c->expr, &cm->ts,
	  TREE_TYPE (cm->backend_decl), cm->dimension, cm->pointer);

      /* Append it to the constructor list.  */
      CONSTRUCTOR_APPEND_ELT (v, cm->backend_decl, val);
    }
  se->expr = build_constructor (type, v);
}


/* Translate a substring expression.  */

static void
gfc_conv_substring_expr (gfc_se * se, gfc_expr * expr)
{
  gfc_ref *ref;

  ref = expr->ref;

  gcc_assert (ref->type == REF_SUBSTRING);

  se->expr = gfc_build_string_const(expr->value.character.length,
                                    expr->value.character.string);
  se->string_length = TYPE_MAX_VALUE (TYPE_DOMAIN (TREE_TYPE (se->expr)));
  TYPE_STRING_FLAG (TREE_TYPE (se->expr))=1;

  gfc_conv_substring(se,ref,expr->ts.kind);
}


/* Entry point for expression translation.  Evaluates a scalar quantity.
   EXPR is the expression to be translated, and SE is the state structure if
   called from within the scalarized.  */

void
gfc_conv_expr (gfc_se * se, gfc_expr * expr)
{
  if (se->ss && se->ss->expr == expr
      && (se->ss->type == GFC_SS_SCALAR || se->ss->type == GFC_SS_REFERENCE))
    {
      /* Substitute a scalar expression evaluated outside the scalarization
         loop.  */
      se->expr = se->ss->data.scalar.expr;
      se->string_length = se->ss->string_length;
      gfc_advance_se_ss_chain (se);
      return;
    }

  switch (expr->expr_type)
    {
    case EXPR_OP:
      gfc_conv_expr_op (se, expr);
      break;

    case EXPR_FUNCTION:
      gfc_conv_function_expr (se, expr);
      break;

    case EXPR_CONSTANT:
      gfc_conv_constant (se, expr);
      break;

    case EXPR_VARIABLE:
      gfc_conv_variable (se, expr);
      break;

    case EXPR_NULL:
      se->expr = null_pointer_node;
      break;

    case EXPR_SUBSTRING:
      gfc_conv_substring_expr (se, expr);
      break;

    case EXPR_STRUCTURE:
      gfc_conv_structure (se, expr, 0);
      break;

    case EXPR_ARRAY:
      gfc_conv_array_constructor_expr (se, expr);
      break;

    default:
      gcc_unreachable ();
      break;
    }
}

/* Like gfc_conv_expr_val, but the value is also suitable for use in the lhs
   of an assignment.  */
void
gfc_conv_expr_lhs (gfc_se * se, gfc_expr * expr)
{
  gfc_conv_expr (se, expr);
  /* All numeric lvalues should have empty post chains.  If not we need to
     figure out a way of rewriting an lvalue so that it has no post chain.  */
  gcc_assert (expr->ts.type == BT_CHARACTER || !se->post.head);
}

/* Like gfc_conv_expr, but the POST block is guaranteed to be empty for
   numeric expressions.  Used for scalar values where inserting cleanup code
   is inconvenient.  */
void
gfc_conv_expr_val (gfc_se * se, gfc_expr * expr)
{
  tree val;

  gcc_assert (expr->ts.type != BT_CHARACTER);
  gfc_conv_expr (se, expr);
  if (se->post.head)
    {
      val = gfc_create_var (TREE_TYPE (se->expr), NULL);
      gfc_add_modify_expr (&se->pre, val, se->expr);
      se->expr = val;
      gfc_add_block_to_block (&se->pre, &se->post);
    }
}

/* Helper to translate and expression and convert it to a particular type.  */
void
gfc_conv_expr_type (gfc_se * se, gfc_expr * expr, tree type)
{
  gfc_conv_expr_val (se, expr);
  se->expr = convert (type, se->expr);
}


/* Converts an expression so that it can be passed by reference.  Scalar
   values only.  */

void
gfc_conv_expr_reference (gfc_se * se, gfc_expr * expr)
{
  tree var;

  if (se->ss && se->ss->expr == expr
      && se->ss->type == GFC_SS_REFERENCE)
    {
      se->expr = se->ss->data.scalar.expr;
      se->string_length = se->ss->string_length;
      gfc_advance_se_ss_chain (se);
      return;
    }

  if (expr->ts.type == BT_CHARACTER)
    {
      gfc_conv_expr (se, expr);
      gfc_conv_string_parameter (se);
      return;
    }

  if (expr->expr_type == EXPR_VARIABLE)
    {
      se->want_pointer = 1;
      gfc_conv_expr (se, expr);
      if (se->post.head)
	{
	  var = gfc_create_var (TREE_TYPE (se->expr), NULL);
	  gfc_add_modify_expr (&se->pre, var, se->expr);
	  gfc_add_block_to_block (&se->pre, &se->post);
	  se->expr = var;
	}
      return;
    }

  gfc_conv_expr (se, expr);

  /* Create a temporary var to hold the value.  */
  if (TREE_CONSTANT (se->expr))
    {
      tree tmp = se->expr;
      STRIP_TYPE_NOPS (tmp);
      var = build_decl (CONST_DECL, NULL, TREE_TYPE (tmp));
      DECL_INITIAL (var) = tmp;
      TREE_STATIC (var) = 1;
      pushdecl (var);
    }
  else
    {
      var = gfc_create_var (TREE_TYPE (se->expr), NULL);
      gfc_add_modify_expr (&se->pre, var, se->expr);
    }
  gfc_add_block_to_block (&se->pre, &se->post);

  /* Take the address of that value.  */
  se->expr = build_fold_addr_expr (var);
}


tree
gfc_trans_pointer_assign (gfc_code * code)
{
  return gfc_trans_pointer_assignment (code->expr, code->expr2);
}


/* Generate code for a pointer assignment.  */

tree
gfc_trans_pointer_assignment (gfc_expr * expr1, gfc_expr * expr2)
{
  gfc_se lse;
  gfc_se rse;
  gfc_ss *lss;
  gfc_ss *rss;
  stmtblock_t block;
  tree desc;
  tree tmp;

  gfc_start_block (&block);

  gfc_init_se (&lse, NULL);

  lss = gfc_walk_expr (expr1);
  rss = gfc_walk_expr (expr2);
  if (lss == gfc_ss_terminator)
    {
      /* Scalar pointers.  */
      lse.want_pointer = 1;
      gfc_conv_expr (&lse, expr1);
      gcc_assert (rss == gfc_ss_terminator);
      gfc_init_se (&rse, NULL);
      rse.want_pointer = 1;
      gfc_conv_expr (&rse, expr2);
      gfc_add_block_to_block (&block, &lse.pre);
      gfc_add_block_to_block (&block, &rse.pre);
      gfc_add_modify_expr (&block, lse.expr,
			   fold_convert (TREE_TYPE (lse.expr), rse.expr));
      gfc_add_block_to_block (&block, &rse.post);
      gfc_add_block_to_block (&block, &lse.post);
    }
  else
    {
      /* Array pointer.  */
      gfc_conv_expr_descriptor (&lse, expr1, lss);
      switch (expr2->expr_type)
	{
	case EXPR_NULL:
	  /* Just set the data pointer to null.  */
	  gfc_conv_descriptor_data_set (&lse.pre, lse.expr, null_pointer_node);
	  break;

	case EXPR_VARIABLE:
	  /* Assign directly to the pointer's descriptor.  */
          lse.direct_byref = 1;
	  gfc_conv_expr_descriptor (&lse, expr2, rss);
	  break;

	default:
	  /* Assign to a temporary descriptor and then copy that
	     temporary to the pointer.  */
	  desc = lse.expr;
	  tmp = gfc_create_var (TREE_TYPE (desc), "ptrtemp");

	  lse.expr = tmp;
	  lse.direct_byref = 1;
	  gfc_conv_expr_descriptor (&lse, expr2, rss);
	  gfc_add_modify_expr (&lse.pre, desc, tmp);
	  break;
        }
      gfc_add_block_to_block (&block, &lse.pre);
      gfc_add_block_to_block (&block, &lse.post);
    }
  return gfc_finish_block (&block);
}


/* Makes sure se is suitable for passing as a function string parameter.  */
/* TODO: Need to check all callers fo this function.  It may be abused.  */

void
gfc_conv_string_parameter (gfc_se * se)
{
  tree type;

  if (TREE_CODE (se->expr) == STRING_CST)
    {
      se->expr = gfc_build_addr_expr (pchar_type_node, se->expr);
      return;
    }

  type = TREE_TYPE (se->expr);
  if (TYPE_STRING_FLAG (type))
    {
      gcc_assert (TREE_CODE (se->expr) != INDIRECT_REF);
      se->expr = gfc_build_addr_expr (pchar_type_node, se->expr);
    }

  gcc_assert (POINTER_TYPE_P (TREE_TYPE (se->expr)));
  gcc_assert (se->string_length
	  && TREE_CODE (TREE_TYPE (se->string_length)) == INTEGER_TYPE);
}


/* Generate code for assignment of scalar variables.  Includes character
   strings and derived types with allocatable components.  */

tree
gfc_trans_scalar_assign (gfc_se * lse, gfc_se * rse, gfc_typespec ts,
			 bool l_is_temp, bool r_is_var)
{
  stmtblock_t block;
  tree tmp;
  tree cond;

  gfc_init_block (&block);

  if (ts.type == BT_CHARACTER)
    {
      gcc_assert (lse->string_length != NULL_TREE
	      && rse->string_length != NULL_TREE);

      gfc_conv_string_parameter (lse);
      gfc_conv_string_parameter (rse);

      gfc_add_block_to_block (&block, &lse->pre);
      gfc_add_block_to_block (&block, &rse->pre);

      gfc_trans_string_copy (&block, lse->string_length, lse->expr,
			     rse->string_length, rse->expr);
    }
  else if (ts.type == BT_DERIVED && ts.derived->attr.alloc_comp)
    {
      cond = NULL_TREE;
	
      /* Are the rhs and the lhs the same?  */
      if (r_is_var)
	{
	  cond = fold_build2 (EQ_EXPR, boolean_type_node,
			      build_fold_addr_expr (lse->expr),
			      build_fold_addr_expr (rse->expr));
	  cond = gfc_evaluate_now (cond, &lse->pre);
	}

      /* Deallocate the lhs allocated components as long as it is not
	 the same as the rhs.  */
      if (!l_is_temp)
	{
	  tmp = gfc_deallocate_alloc_comp (ts.derived, lse->expr, 0);
	  if (r_is_var)
	    tmp = build3_v (COND_EXPR, cond, build_empty_stmt (), tmp);
	  gfc_add_expr_to_block (&lse->pre, tmp);
	}
	
      gfc_add_block_to_block (&block, &lse->pre);
      gfc_add_block_to_block (&block, &rse->pre);

      gfc_add_modify_expr (&block, lse->expr,
			   fold_convert (TREE_TYPE (lse->expr), rse->expr));

      /* Do a deep copy if the rhs is a variable, if it is not the
	 same as the lhs.  */
      if (r_is_var)
	{
	  tmp = gfc_copy_alloc_comp (ts.derived, rse->expr, lse->expr, 0);
	  tmp = build3_v (COND_EXPR, cond, build_empty_stmt (), tmp);
	  gfc_add_expr_to_block (&block, tmp);
	}
    }
  else
    {
      gfc_add_block_to_block (&block, &lse->pre);
      gfc_add_block_to_block (&block, &rse->pre);

      gfc_add_modify_expr (&block, lse->expr,
			   fold_convert (TREE_TYPE (lse->expr), rse->expr));
    }

  gfc_add_block_to_block (&block, &lse->post);
  gfc_add_block_to_block (&block, &rse->post);

  return gfc_finish_block (&block);
}


/* Try to translate array(:) = func (...), where func is a transformational
   array function, without using a temporary.  Returns NULL is this isn't the
   case.  */

static tree
gfc_trans_arrayfunc_assign (gfc_expr * expr1, gfc_expr * expr2)
{
  gfc_se se;
  gfc_ss *ss;
  gfc_ref * ref;
  bool seen_array_ref;

  /* The caller has already checked rank>0 and expr_type == EXPR_FUNCTION.  */
  if (expr2->value.function.isym && !gfc_is_intrinsic_libcall (expr2))
    return NULL;

  /* Elemental functions don't need a temporary anyway.  */
  if (expr2->value.function.esym != NULL
      && expr2->value.function.esym->attr.elemental)
    return NULL;

  /* Fail if EXPR1 can't be expressed as a descriptor.  */
  if (gfc_ref_needs_temporary_p (expr1->ref))
    return NULL;

  /* Functions returning pointers need temporaries.  */
  if (expr2->symtree->n.sym->attr.pointer 
      || expr2->symtree->n.sym->attr.allocatable)
    return NULL;

  /* Character array functions need temporaries unless the
     character lengths are the same.  */
  if (expr2->ts.type == BT_CHARACTER && expr2->rank > 0)
    {
      if (expr1->ts.cl->length == NULL
	    || expr1->ts.cl->length->expr_type != EXPR_CONSTANT)
	return NULL;

      if (expr2->ts.cl->length == NULL
	    || expr2->ts.cl->length->expr_type != EXPR_CONSTANT)
	return NULL;

      if (mpz_cmp (expr1->ts.cl->length->value.integer,
		     expr2->ts.cl->length->value.integer) != 0)
	return NULL;
    }

  /* Check that no LHS component references appear during an array
     reference. This is needed because we do not have the means to
     span any arbitrary stride with an array descriptor. This check
     is not needed for the rhs because the function result has to be
     a complete type.  */
  seen_array_ref = false;
  for (ref = expr1->ref; ref; ref = ref->next)
    {
      if (ref->type == REF_ARRAY)
	seen_array_ref= true;
      else if (ref->type == REF_COMPONENT && seen_array_ref)
	return NULL;
    }

  /* Check for a dependency.  */
  if (gfc_check_fncall_dependency (expr1, INTENT_OUT,
				   expr2->value.function.esym,
				   expr2->value.function.actual))
    return NULL;

  /* The frontend doesn't seem to bother filling in expr->symtree for intrinsic
     functions.  */
  gcc_assert (expr2->value.function.isym
	      || (gfc_return_by_reference (expr2->value.function.esym)
	      && expr2->value.function.esym->result->attr.dimension));

  ss = gfc_walk_expr (expr1);
  gcc_assert (ss != gfc_ss_terminator);
  gfc_init_se (&se, NULL);
  gfc_start_block (&se.pre);
  se.want_pointer = 1;

  gfc_conv_array_parameter (&se, expr1, ss, 0);

  se.direct_byref = 1;
  se.ss = gfc_walk_expr (expr2);
  gcc_assert (se.ss != gfc_ss_terminator);
  gfc_conv_function_expr (&se, expr2);
  gfc_add_block_to_block (&se.pre, &se.post);

  return gfc_finish_block (&se.pre);
}


/* Translate an assignment.  Most of the code is concerned with
   setting up the scalarizer.  */

tree
gfc_trans_assignment (gfc_expr * expr1, gfc_expr * expr2, bool init_flag)
{
  gfc_se lse;
  gfc_se rse;
  gfc_ss *lss;
  gfc_ss *lss_section;
  gfc_ss *rss;
  gfc_loopinfo loop;
  tree tmp;
  stmtblock_t block;
  stmtblock_t body;
  bool l_is_temp;

  /* Special case a single function returning an array.  */
  if (expr2->expr_type == EXPR_FUNCTION && expr2->rank > 0)
    {
      tmp = gfc_trans_arrayfunc_assign (expr1, expr2);
      if (tmp)
	return tmp;
    }

  /* Assignment of the form lhs = rhs.  */
  gfc_start_block (&block);

  gfc_init_se (&lse, NULL);
  gfc_init_se (&rse, NULL);

  /* Walk the lhs.  */
  lss = gfc_walk_expr (expr1);
  rss = NULL;
  if (lss != gfc_ss_terminator)
    {
      /* The assignment needs scalarization.  */
      lss_section = lss;

      /* Find a non-scalar SS from the lhs.  */
      while (lss_section != gfc_ss_terminator
	     && lss_section->type != GFC_SS_SECTION)
	lss_section = lss_section->next;

      gcc_assert (lss_section != gfc_ss_terminator);

      /* Initialize the scalarizer.  */
      gfc_init_loopinfo (&loop);

      /* Walk the rhs.  */
      rss = gfc_walk_expr (expr2);
      if (rss == gfc_ss_terminator)
	{
	  /* The rhs is scalar.  Add a ss for the expression.  */
	  rss = gfc_get_ss ();
	  rss->next = gfc_ss_terminator;
	  rss->type = GFC_SS_SCALAR;
	  rss->expr = expr2;
	}
      /* Associate the SS with the loop.  */
      gfc_add_ss_to_loop (&loop, lss);
      gfc_add_ss_to_loop (&loop, rss);

      /* Calculate the bounds of the scalarization.  */
      gfc_conv_ss_startstride (&loop);
      /* Resolve any data dependencies in the statement.  */
      gfc_conv_resolve_dependencies (&loop, lss, rss);
      /* Setup the scalarizing loops.  */
      gfc_conv_loop_setup (&loop);

      /* Setup the gfc_se structures.  */
      gfc_copy_loopinfo_to_se (&lse, &loop);
      gfc_copy_loopinfo_to_se (&rse, &loop);

      rse.ss = rss;
      gfc_mark_ss_chain_used (rss, 1);
      if (loop.temp_ss == NULL)
	{
	  lse.ss = lss;
	  gfc_mark_ss_chain_used (lss, 1);
	}
      else
	{
	  lse.ss = loop.temp_ss;
	  gfc_mark_ss_chain_used (lss, 3);
	  gfc_mark_ss_chain_used (loop.temp_ss, 3);
	}

      /* Start the scalarized loop body.  */
      gfc_start_scalarized_body (&loop, &body);
    }
  else
    gfc_init_block (&body);

  l_is_temp = (lss != gfc_ss_terminator && loop.temp_ss != NULL);

  /* Translate the expression.  */
  gfc_conv_expr (&rse, expr2);

  if (l_is_temp)
    {
      gfc_conv_tmp_array_ref (&lse);
      gfc_advance_se_ss_chain (&lse);
    }
  else
    gfc_conv_expr (&lse, expr1);

  tmp = gfc_trans_scalar_assign (&lse, &rse, expr1->ts,
				 l_is_temp || init_flag,
				 expr2->expr_type == EXPR_VARIABLE);
  gfc_add_expr_to_block (&body, tmp);

  if (lss == gfc_ss_terminator)
    {
      /* Use the scalar assignment as is.  */
      gfc_add_block_to_block (&block, &body);
    }
  else
    {
      gcc_assert (lse.ss == gfc_ss_terminator
		  && rse.ss == gfc_ss_terminator);

      if (l_is_temp)
	{
	  gfc_trans_scalarized_loop_boundary (&loop, &body);

	  /* We need to copy the temporary to the actual lhs.  */
	  gfc_init_se (&lse, NULL);
	  gfc_init_se (&rse, NULL);
	  gfc_copy_loopinfo_to_se (&lse, &loop);
	  gfc_copy_loopinfo_to_se (&rse, &loop);

	  rse.ss = loop.temp_ss;
	  lse.ss = lss;

	  gfc_conv_tmp_array_ref (&rse);
	  gfc_advance_se_ss_chain (&rse);
	  gfc_conv_expr (&lse, expr1);

	  gcc_assert (lse.ss == gfc_ss_terminator
		      && rse.ss == gfc_ss_terminator);

	  tmp = gfc_trans_scalar_assign (&lse, &rse, expr1->ts,
					 false, false);
	  gfc_add_expr_to_block (&body, tmp);
	}

      /* Generate the copying loops.  */
      gfc_trans_scalarizing_loops (&loop, &body);

      /* Wrap the whole thing up.  */
      gfc_add_block_to_block (&block, &loop.pre);
      gfc_add_block_to_block (&block, &loop.post);

      gfc_cleanup_loop (&loop);
    }

  return gfc_finish_block (&block);
}

tree
gfc_trans_init_assign (gfc_code * code)
{
  return gfc_trans_assignment (code->expr, code->expr2, true);
}

tree
gfc_trans_assign (gfc_code * code)
{
  return gfc_trans_assignment (code->expr, code->expr2, false);
}

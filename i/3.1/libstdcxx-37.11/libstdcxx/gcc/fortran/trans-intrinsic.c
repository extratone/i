/* Intrinsic translation
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007
   Free Software Foundation, Inc.
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

/* trans-intrinsic.c-- generate GENERIC trees for calls to intrinsics.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tree.h"
#include "ggc.h"
#include "toplev.h"
#include "real.h"
#include "tree-gimple.h"
#include "flags.h"
#include "gfortran.h"
#include "arith.h"
#include "intrinsic.h"
#include "trans.h"
#include "trans-const.h"
#include "trans-types.h"
#include "trans-array.h"
#include "defaults.h"
/* Only for gfc_trans_assign and gfc_trans_pointer_assign.  */
#include "trans-stmt.h"

/* This maps fortran intrinsic math functions to external library or GCC
   builtin functions.  */
typedef struct gfc_intrinsic_map_t	GTY(())
{
  /* The explicit enum is required to work around inadequacies in the
     garbage collection/gengtype parsing mechanism.  */
  enum gfc_generic_isym_id id;

  /* Enum value from the "language-independent", aka C-centric, part
     of gcc, or END_BUILTINS of no such value set.  */
  enum built_in_function code_r4;
  enum built_in_function code_r8;
  enum built_in_function code_r10;
  enum built_in_function code_r16;
  enum built_in_function code_c4;
  enum built_in_function code_c8;
  enum built_in_function code_c10;
  enum built_in_function code_c16;

  /* True if the naming pattern is to prepend "c" for complex and
     append "f" for kind=4.  False if the naming pattern is to
     prepend "_gfortran_" and append "[rc](4|8|10|16)".  */
  bool libm_name;

  /* True if a complex version of the function exists.  */
  bool complex_available;

  /* True if the function should be marked const.  */
  bool is_constant;

  /* The base library name of this function.  */
  const char *name;

  /* Cache decls created for the various operand types.  */
  tree real4_decl;
  tree real8_decl;
  tree real10_decl;
  tree real16_decl;
  tree complex4_decl;
  tree complex8_decl;
  tree complex10_decl;
  tree complex16_decl;
}
gfc_intrinsic_map_t;

/* ??? The NARGS==1 hack here is based on the fact that (c99 at least)
   defines complex variants of all of the entries in mathbuiltins.def
   except for atan2.  */
#define DEFINE_MATH_BUILTIN(ID, NAME, ARGTYPE) \
  { GFC_ISYM_ ## ID, BUILT_IN_ ## ID ## F, BUILT_IN_ ## ID, \
    BUILT_IN_ ## ID ## L, BUILT_IN_ ## ID ## L, 0, 0, 0, 0, true, \
    false, true, NAME, NULL_TREE, NULL_TREE, NULL_TREE, NULL_TREE, \
    NULL_TREE, NULL_TREE, NULL_TREE, NULL_TREE},

#define DEFINE_MATH_BUILTIN_C(ID, NAME, ARGTYPE) \
  { GFC_ISYM_ ## ID, BUILT_IN_ ## ID ## F, BUILT_IN_ ## ID, \
    BUILT_IN_ ## ID ## L, BUILT_IN_ ## ID ## L, BUILT_IN_C ## ID ## F, \
    BUILT_IN_C ## ID, BUILT_IN_C ## ID ## L, BUILT_IN_C ## ID ## L, true, \
    true, true, NAME, NULL_TREE, NULL_TREE, NULL_TREE, NULL_TREE, \
    NULL_TREE, NULL_TREE, NULL_TREE, NULL_TREE},

#define LIBM_FUNCTION(ID, NAME, HAVE_COMPLEX) \
  { GFC_ISYM_ ## ID, END_BUILTINS, END_BUILTINS, END_BUILTINS, END_BUILTINS, \
    END_BUILTINS, END_BUILTINS, END_BUILTINS, END_BUILTINS, \
    true, HAVE_COMPLEX, true, NAME, NULL_TREE, NULL_TREE, NULL_TREE, \
    NULL_TREE, NULL_TREE, NULL_TREE, NULL_TREE, NULL_TREE }

#define LIBF_FUNCTION(ID, NAME, HAVE_COMPLEX) \
  { GFC_ISYM_ ## ID, END_BUILTINS, END_BUILTINS, END_BUILTINS, END_BUILTINS, \
    END_BUILTINS, END_BUILTINS, END_BUILTINS, END_BUILTINS, \
    false, HAVE_COMPLEX, true, NAME, NULL_TREE, NULL_TREE, NULL_TREE, \
    NULL_TREE, NULL_TREE, NULL_TREE, NULL_TREE, NULL_TREE }

static GTY(()) gfc_intrinsic_map_t gfc_intrinsic_map[] =
{
  /* Functions built into gcc itself.  */
#include "mathbuiltins.def"

  /* Functions in libm.  */
  /* ??? This does exist as BUILT_IN_SCALBN, but doesn't quite fit the
     pattern for other mathbuiltins.def entries.  At present we have no
     optimizations for this in the common sources.  */
  LIBM_FUNCTION (SCALE, "scalbn", false),

  /* Functions in libgfortran.  */
  LIBF_FUNCTION (FRACTION, "fraction", false),
  LIBF_FUNCTION (NEAREST, "nearest", false),
  LIBF_FUNCTION (RRSPACING, "rrspacing", false),
  LIBF_FUNCTION (SET_EXPONENT, "set_exponent", false),
  LIBF_FUNCTION (SPACING, "spacing", false),

  /* End the list.  */
  LIBF_FUNCTION (NONE, NULL, false)
};
#undef DEFINE_MATH_BUILTIN
#undef DEFINE_MATH_BUILTIN_C
#undef LIBM_FUNCTION
#undef LIBF_FUNCTION

/* Structure for storing components of a floating number to be used by
   elemental functions to manipulate reals.  */
typedef struct
{
  tree arg;     /* Variable tree to view convert to integer.  */
  tree expn;    /* Variable tree to save exponent.  */
  tree frac;    /* Variable tree to save fraction.  */
  tree smask;   /* Constant tree of sign's mask.  */
  tree emask;   /* Constant tree of exponent's mask.  */
  tree fmask;   /* Constant tree of fraction's mask.  */
  tree edigits; /* Constant tree of the number of exponent bits.  */
  tree fdigits; /* Constant tree of the number of fraction bits.  */
  tree f1;      /* Constant tree of the f1 defined in the real model.  */
  tree bias;    /* Constant tree of the bias of exponent in the memory.  */
  tree type;    /* Type tree of arg1.  */
  tree mtype;   /* Type tree of integer type. Kind is that of arg1.  */
}
real_compnt_info;


/* Evaluate the arguments to an intrinsic function.  */

static tree
gfc_conv_intrinsic_function_args (gfc_se * se, gfc_expr * expr)
{
  gfc_actual_arglist *actual;
  gfc_expr *e;
  gfc_intrinsic_arg  *formal;
  gfc_se argse;
  tree args;

  args = NULL_TREE;
  formal = expr->value.function.isym->formal;

  for (actual = expr->value.function.actual; actual; actual = actual->next,
       formal = formal ? formal->next : NULL)
    {
      e = actual->expr;
      /* Skip omitted optional arguments.  */
      if (!e)
	continue;

      /* Evaluate the parameter.  This will substitute scalarized
         references automatically.  */
      gfc_init_se (&argse, se);

      if (e->ts.type == BT_CHARACTER)
	{
	  gfc_conv_expr (&argse, e);
	  gfc_conv_string_parameter (&argse);
	  args = gfc_chainon_list (args, argse.string_length);
	}
      else
        gfc_conv_expr_val (&argse, e);

      /* If an optional argument is itself an optional dummy argument,
	 check its presence and substitute a null if absent.  */
      if (e->expr_type ==EXPR_VARIABLE
	    && e->symtree->n.sym->attr.optional
	    && formal
	    && formal->optional)
	gfc_conv_missing_dummy (&argse, e, formal->ts);

      gfc_add_block_to_block (&se->pre, &argse.pre);
      gfc_add_block_to_block (&se->post, &argse.post);
      args = gfc_chainon_list (args, argse.expr);
    }
  return args;
}


/* Conversions between different types are output by the frontend as
   intrinsic functions.  We implement these directly with inline code.  */

static void
gfc_conv_intrinsic_conversion (gfc_se * se, gfc_expr * expr)
{
  tree type;
  tree arg;

  /* Evaluate the argument.  */
  type = gfc_typenode_for_spec (&expr->ts);
  gcc_assert (expr->value.function.actual->expr);
  arg = gfc_conv_intrinsic_function_args (se, expr);
  arg = TREE_VALUE (arg);

  /* Conversion from complex to non-complex involves taking the real
     component of the value.  */
  if (TREE_CODE (TREE_TYPE (arg)) == COMPLEX_TYPE
      && expr->ts.type != BT_COMPLEX)
    {
      tree artype;

      artype = TREE_TYPE (TREE_TYPE (arg));
      arg = build1 (REALPART_EXPR, artype, arg);
    }

  se->expr = convert (type, arg);
}

/* This is needed because the gcc backend only implements
   FIX_TRUNC_EXPR, which is the same as INT() in Fortran.
   FLOOR(x) = INT(x) <= x ? INT(x) : INT(x) - 1
   Similarly for CEILING.  */

static tree
build_fixbound_expr (stmtblock_t * pblock, tree arg, tree type, int up)
{
  tree tmp;
  tree cond;
  tree argtype;
  tree intval;

  argtype = TREE_TYPE (arg);
  arg = gfc_evaluate_now (arg, pblock);

  intval = convert (type, arg);
  intval = gfc_evaluate_now (intval, pblock);

  tmp = convert (argtype, intval);
  cond = build2 (up ? GE_EXPR : LE_EXPR, boolean_type_node, tmp, arg);

  tmp = build2 (up ? PLUS_EXPR : MINUS_EXPR, type, intval,
		build_int_cst (type, 1));
  tmp = build3 (COND_EXPR, type, cond, intval, tmp);
  return tmp;
}


/* This is needed because the gcc backend only implements FIX_TRUNC_EXPR
   NINT(x) = INT(x + ((x > 0) ? 0.5 : -0.5)).  */

static tree
build_round_expr (stmtblock_t * pblock, tree arg, tree type)
{
  tree tmp;
  tree cond;
  tree neg;
  tree pos;
  tree argtype;
  REAL_VALUE_TYPE r;

  argtype = TREE_TYPE (arg);
  arg = gfc_evaluate_now (arg, pblock);

  real_from_string (&r, "0.5");
  pos = build_real (argtype, r);

  real_from_string (&r, "-0.5");
  neg = build_real (argtype, r);

  tmp = gfc_build_const (argtype, integer_zero_node);
  cond = fold_build2 (GT_EXPR, boolean_type_node, arg, tmp);

  tmp = fold_build3 (COND_EXPR, argtype, cond, pos, neg);
  tmp = fold_build2 (PLUS_EXPR, argtype, arg, tmp);
  return fold_build1 (FIX_TRUNC_EXPR, type, tmp);
}


/* Convert a real to an integer using a specific rounding mode.
   Ideally we would just build the corresponding GENERIC node,
   however the RTL expander only actually supports FIX_TRUNC_EXPR.  */

static tree
build_fix_expr (stmtblock_t * pblock, tree arg, tree type,
               enum tree_code op)
{
  switch (op)
    {
    case FIX_FLOOR_EXPR:
      return build_fixbound_expr (pblock, arg, type, 0);
      break;

    case FIX_CEIL_EXPR:
      return build_fixbound_expr (pblock, arg, type, 1);
      break;

    case FIX_ROUND_EXPR:
      return build_round_expr (pblock, arg, type);

    default:
      return build1 (op, type, arg);
    }
}


/* Round a real value using the specified rounding mode.
   We use a temporary integer of that same kind size as the result.
   Values larger than those that can be represented by this kind are
   unchanged, as they will not be accurate enough to represent the
   rounding.
    huge = HUGE (KIND (a))
    aint (a) = ((a > huge) || (a < -huge)) ? a : (real)(int)a
   */

static void
gfc_conv_intrinsic_aint (gfc_se * se, gfc_expr * expr, enum tree_code op)
{
  tree type;
  tree itype;
  tree arg;
  tree tmp;
  tree cond;
  mpfr_t huge;
  int n;
  int kind;

  kind = expr->ts.kind;

  n = END_BUILTINS;
  /* We have builtin functions for some cases.  */
  switch (op)
    {
    case FIX_ROUND_EXPR:
      switch (kind)
	{
	case 4:
	  n = BUILT_IN_ROUNDF;
	  break;

	case 8:
	  n = BUILT_IN_ROUND;
	  break;

	case 10:
	case 16:
	  n = BUILT_IN_ROUNDL;
	  break;
	}
      break;

    case FIX_TRUNC_EXPR:
      switch (kind)
	{
	case 4:
	  n = BUILT_IN_TRUNCF;
	  break;

	case 8:
	  n = BUILT_IN_TRUNC;
	  break;

	case 10:
	case 16:
	  n = BUILT_IN_TRUNCL;
	  break;
	}
      break;

    default:
      gcc_unreachable ();
    }

  /* Evaluate the argument.  */
  gcc_assert (expr->value.function.actual->expr);
  arg = gfc_conv_intrinsic_function_args (se, expr);

  /* Use a builtin function if one exists.  */
  if (n != END_BUILTINS)
    {
      tmp = built_in_decls[n];
      se->expr = build_function_call_expr (tmp, arg);
      return;
    }

  /* This code is probably redundant, but we'll keep it lying around just
     in case.  */
  type = gfc_typenode_for_spec (&expr->ts);
  arg = TREE_VALUE (arg);
  arg = gfc_evaluate_now (arg, &se->pre);

  /* Test if the value is too large to handle sensibly.  */
  gfc_set_model_kind (kind);
  mpfr_init (huge);
  n = gfc_validate_kind (BT_INTEGER, kind, false);
  mpfr_set_z (huge, gfc_integer_kinds[n].huge, GFC_RND_MODE);
  tmp = gfc_conv_mpfr_to_tree (huge, kind);
  cond = build2 (LT_EXPR, boolean_type_node, arg, tmp);

  mpfr_neg (huge, huge, GFC_RND_MODE);
  tmp = gfc_conv_mpfr_to_tree (huge, kind);
  tmp = build2 (GT_EXPR, boolean_type_node, arg, tmp);
  cond = build2 (TRUTH_AND_EXPR, boolean_type_node, cond, tmp);
  itype = gfc_get_int_type (kind);

  tmp = build_fix_expr (&se->pre, arg, itype, op);
  tmp = convert (type, tmp);
  se->expr = build3 (COND_EXPR, type, cond, tmp, arg);
  mpfr_clear (huge);
}


/* Convert to an integer using the specified rounding mode.  */

static void
gfc_conv_intrinsic_int (gfc_se * se, gfc_expr * expr, int op)
{
  tree type;
  tree arg;

  /* Evaluate the argument.  */
  type = gfc_typenode_for_spec (&expr->ts);
  gcc_assert (expr->value.function.actual->expr);
  arg = gfc_conv_intrinsic_function_args (se, expr);
  arg = TREE_VALUE (arg);

  if (TREE_CODE (TREE_TYPE (arg)) == INTEGER_TYPE)
    {
      /* Conversion to a different integer kind.  */
      se->expr = convert (type, arg);
    }
  else
    {
      /* Conversion from complex to non-complex involves taking the real
         component of the value.  */
      if (TREE_CODE (TREE_TYPE (arg)) == COMPLEX_TYPE
	  && expr->ts.type != BT_COMPLEX)
	{
	  tree artype;

	  artype = TREE_TYPE (TREE_TYPE (arg));
	  arg = build1 (REALPART_EXPR, artype, arg);
	}

      se->expr = build_fix_expr (&se->pre, arg, type, op);
    }
}


/* Get the imaginary component of a value.  */

static void
gfc_conv_intrinsic_imagpart (gfc_se * se, gfc_expr * expr)
{
  tree arg;

  arg = gfc_conv_intrinsic_function_args (se, expr);
  arg = TREE_VALUE (arg);
  se->expr = build1 (IMAGPART_EXPR, TREE_TYPE (TREE_TYPE (arg)), arg);
}


/* Get the complex conjugate of a value.  */

static void
gfc_conv_intrinsic_conjg (gfc_se * se, gfc_expr * expr)
{
  tree arg;

  arg = gfc_conv_intrinsic_function_args (se, expr);
  arg = TREE_VALUE (arg);
  se->expr = build1 (CONJ_EXPR, TREE_TYPE (arg), arg);
}


/* Initialize function decls for library functions.  The external functions
   are created as required.  Builtin functions are added here.  */

void
gfc_build_intrinsic_lib_fndecls (void)
{
  gfc_intrinsic_map_t *m;

  /* Add GCC builtin functions.  */
  for (m = gfc_intrinsic_map; m->id != GFC_ISYM_NONE; m++)
    {
      if (m->code_r4 != END_BUILTINS)
	m->real4_decl = built_in_decls[m->code_r4];
      if (m->code_r8 != END_BUILTINS)
	m->real8_decl = built_in_decls[m->code_r8];
      if (m->code_r10 != END_BUILTINS)
	m->real10_decl = built_in_decls[m->code_r10];
      if (m->code_r16 != END_BUILTINS)
	m->real16_decl = built_in_decls[m->code_r16];
      if (m->code_c4 != END_BUILTINS)
	m->complex4_decl = built_in_decls[m->code_c4];
      if (m->code_c8 != END_BUILTINS)
	m->complex8_decl = built_in_decls[m->code_c8];
      if (m->code_c10 != END_BUILTINS)
	m->complex10_decl = built_in_decls[m->code_c10];
      if (m->code_c16 != END_BUILTINS)
	m->complex16_decl = built_in_decls[m->code_c16];
    }
}


/* Create a fndecl for a simple intrinsic library function.  */

static tree
gfc_get_intrinsic_lib_fndecl (gfc_intrinsic_map_t * m, gfc_expr * expr)
{
  tree type;
  tree argtypes;
  tree fndecl;
  gfc_actual_arglist *actual;
  tree *pdecl;
  gfc_typespec *ts;
  char name[GFC_MAX_SYMBOL_LEN + 3];

  ts = &expr->ts;
  if (ts->type == BT_REAL)
    {
      switch (ts->kind)
	{
	case 4:
	  pdecl = &m->real4_decl;
	  break;
	case 8:
	  pdecl = &m->real8_decl;
	  break;
	case 10:
	  pdecl = &m->real10_decl;
	  break;
	case 16:
	  pdecl = &m->real16_decl;
	  break;
	default:
	  gcc_unreachable ();
	}
    }
  else if (ts->type == BT_COMPLEX)
    {
      gcc_assert (m->complex_available);

      switch (ts->kind)
	{
	case 4:
	  pdecl = &m->complex4_decl;
	  break;
	case 8:
	  pdecl = &m->complex8_decl;
	  break;
	case 10:
	  pdecl = &m->complex10_decl;
	  break;
	case 16:
	  pdecl = &m->complex16_decl;
	  break;
	default:
	  gcc_unreachable ();
	}
    }
  else
    gcc_unreachable ();

  if (*pdecl)
    return *pdecl;

  if (m->libm_name)
    {
      if (ts->kind == 4)
	snprintf (name, sizeof (name), "%s%s%s",
		ts->type == BT_COMPLEX ? "c" : "", m->name, "f");
      else if (ts->kind == 8)
	snprintf (name, sizeof (name), "%s%s",
		ts->type == BT_COMPLEX ? "c" : "", m->name);
      else
	{
	  gcc_assert (ts->kind == 10 || ts->kind == 16);
	  snprintf (name, sizeof (name), "%s%s%s",
		ts->type == BT_COMPLEX ? "c" : "", m->name, "l");
	}
    }
  else
    {
      snprintf (name, sizeof (name), PREFIX ("%s_%c%d"), m->name,
		ts->type == BT_COMPLEX ? 'c' : 'r',
		ts->kind);
    }

  argtypes = NULL_TREE;
  for (actual = expr->value.function.actual; actual; actual = actual->next)
    {
      type = gfc_typenode_for_spec (&actual->expr->ts);
      argtypes = gfc_chainon_list (argtypes, type);
    }
  argtypes = gfc_chainon_list (argtypes, void_type_node);
  type = build_function_type (gfc_typenode_for_spec (ts), argtypes);
  fndecl = build_decl (FUNCTION_DECL, get_identifier (name), type);

  /* Mark the decl as external.  */
  DECL_EXTERNAL (fndecl) = 1;
  TREE_PUBLIC (fndecl) = 1;

  /* Mark it __attribute__((const)), if possible.  */
  TREE_READONLY (fndecl) = m->is_constant;

  rest_of_decl_compilation (fndecl, 1, 0);

  (*pdecl) = fndecl;
  return fndecl;
}


/* Convert an intrinsic function into an external or builtin call.  */

static void
gfc_conv_intrinsic_lib_function (gfc_se * se, gfc_expr * expr)
{
  gfc_intrinsic_map_t *m;
  tree args;
  tree fndecl;
  gfc_generic_isym_id id;

  id = expr->value.function.isym->generic_id;
  /* Find the entry for this function.  */
  for (m = gfc_intrinsic_map; m->id != GFC_ISYM_NONE; m++)
    {
      if (id == m->id)
	break;
    }

  if (m->id == GFC_ISYM_NONE)
    {
      internal_error ("Intrinsic function %s(%d) not recognized",
		      expr->value.function.name, id);
    }

  /* Get the decl and generate the call.  */
  args = gfc_conv_intrinsic_function_args (se, expr);
  fndecl = gfc_get_intrinsic_lib_fndecl (m, expr);
  se->expr = build_function_call_expr (fndecl, args);
}

/* Generate code for EXPONENT(X) intrinsic function.  */

static void
gfc_conv_intrinsic_exponent (gfc_se * se, gfc_expr * expr)
{
  tree args, fndecl;
  gfc_expr *a1;

  args = gfc_conv_intrinsic_function_args (se, expr);

  a1 = expr->value.function.actual->expr;
  switch (a1->ts.kind)
    {
    case 4:
      fndecl = gfor_fndecl_math_exponent4;
      break;
    case 8:
      fndecl = gfor_fndecl_math_exponent8;
      break;
    case 10:
      fndecl = gfor_fndecl_math_exponent10;
      break;
    case 16:
      fndecl = gfor_fndecl_math_exponent16;
      break;
    default:
      gcc_unreachable ();
    }

  se->expr = build_function_call_expr (fndecl, args);
}

/* Evaluate a single upper or lower bound.  */
/* TODO: bound intrinsic generates way too much unnecessary code.  */

static void
gfc_conv_intrinsic_bound (gfc_se * se, gfc_expr * expr, int upper)
{
  gfc_actual_arglist *arg;
  gfc_actual_arglist *arg2;
  tree desc;
  tree type;
  tree bound;
  tree tmp;
  tree cond, cond1, cond2, cond3, cond4, size;
  tree ubound;
  tree lbound;
  gfc_se argse;
  gfc_ss *ss;
  gfc_array_spec * as;
  gfc_ref *ref;

  arg = expr->value.function.actual;
  arg2 = arg->next;

  if (se->ss)
    {
      /* Create an implicit second parameter from the loop variable.  */
      gcc_assert (!arg2->expr);
      gcc_assert (se->loop->dimen == 1);
      gcc_assert (se->ss->expr == expr);
      gfc_advance_se_ss_chain (se);
      bound = se->loop->loopvar[0];
      bound = fold_build2 (MINUS_EXPR, gfc_array_index_type, bound,
			   se->loop->from[0]);
    }
  else
    {
      /* use the passed argument.  */
      gcc_assert (arg->next->expr);
      gfc_init_se (&argse, NULL);
      gfc_conv_expr_type (&argse, arg->next->expr, gfc_array_index_type);
      gfc_add_block_to_block (&se->pre, &argse.pre);
      bound = argse.expr;
      /* Convert from one based to zero based.  */
      bound = fold_build2 (MINUS_EXPR, gfc_array_index_type, bound,
			   gfc_index_one_node);
    }

  /* TODO: don't re-evaluate the descriptor on each iteration.  */
  /* Get a descriptor for the first parameter.  */
  ss = gfc_walk_expr (arg->expr);
  gcc_assert (ss != gfc_ss_terminator);
  gfc_init_se (&argse, NULL);
  gfc_conv_expr_descriptor (&argse, arg->expr, ss);
  gfc_add_block_to_block (&se->pre, &argse.pre);
  gfc_add_block_to_block (&se->post, &argse.post);

  desc = argse.expr;

  if (INTEGER_CST_P (bound))
    {
      int hi, low;

      hi = TREE_INT_CST_HIGH (bound);
      low = TREE_INT_CST_LOW (bound);
      if (hi || low < 0 || low >= GFC_TYPE_ARRAY_RANK (TREE_TYPE (desc)))
	gfc_error ("'dim' argument of %s intrinsic at %L is not a valid "
		   "dimension index", upper ? "UBOUND" : "LBOUND",
		   &expr->where);
    }
  else
    {
      if (flag_bounds_check)
        {
          bound = gfc_evaluate_now (bound, &se->pre);
          cond = fold_build2 (LT_EXPR, boolean_type_node,
			      bound, build_int_cst (TREE_TYPE (bound), 0));
          tmp = gfc_rank_cst[GFC_TYPE_ARRAY_RANK (TREE_TYPE (desc))];
          tmp = fold_build2 (GE_EXPR, boolean_type_node, bound, tmp);
          cond = fold_build2 (TRUTH_ORIF_EXPR, boolean_type_node, cond, tmp);
          gfc_trans_runtime_check (cond, gfc_msg_fault, &se->pre, &expr->where);
        }
    }

  ubound = gfc_conv_descriptor_ubound (desc, bound);
  lbound = gfc_conv_descriptor_lbound (desc, bound);
  
  /* Follow any component references.  */
  if (arg->expr->expr_type == EXPR_VARIABLE
      || arg->expr->expr_type == EXPR_CONSTANT)
    {
      as = arg->expr->symtree->n.sym->as;
      for (ref = arg->expr->ref; ref; ref = ref->next)
	{
	  switch (ref->type)
	    {
	    case REF_COMPONENT:
	      as = ref->u.c.component->as;
	      continue;

	    case REF_SUBSTRING:
	      continue;

	    case REF_ARRAY:
	      {
		switch (ref->u.ar.type)
		  {
		  case AR_ELEMENT:
		  case AR_SECTION:
		  case AR_UNKNOWN:
		    as = NULL;
		    continue;

		  case AR_FULL:
		    break;
		  }
	      }
	    }
	}
    }
  else
    as = NULL;

  /* 13.14.53: Result value for LBOUND

     Case (i): For an array section or for an array expression other than a
               whole array or array structure component, LBOUND(ARRAY, DIM)
               has the value 1.  For a whole array or array structure
               component, LBOUND(ARRAY, DIM) has the value:
                 (a) equal to the lower bound for subscript DIM of ARRAY if
                     dimension DIM of ARRAY does not have extent zero
                     or if ARRAY is an assumed-size array of rank DIM,
              or (b) 1 otherwise.

     13.14.113: Result value for UBOUND

     Case (i): For an array section or for an array expression other than a
               whole array or array structure component, UBOUND(ARRAY, DIM)
               has the value equal to the number of elements in the given
               dimension; otherwise, it has a value equal to the upper bound
               for subscript DIM of ARRAY if dimension DIM of ARRAY does
               not have size zero and has value zero if dimension DIM has
               size zero.  */

  if (as)
    {
      tree stride = gfc_conv_descriptor_stride (desc, bound);

      cond1 = fold_build2 (GE_EXPR, boolean_type_node, ubound, lbound);
      cond2 = fold_build2 (LE_EXPR, boolean_type_node, ubound, lbound);

      cond3 = fold_build2 (GE_EXPR, boolean_type_node, stride,
			   gfc_index_zero_node);
      cond3 = fold_build2 (TRUTH_AND_EXPR, boolean_type_node, cond3, cond1);

      cond4 = fold_build2 (LT_EXPR, boolean_type_node, stride,
			   gfc_index_zero_node);
      cond4 = fold_build2 (TRUTH_AND_EXPR, boolean_type_node, cond4, cond2);

      if (upper)
	{
	  cond = fold_build2 (TRUTH_OR_EXPR, boolean_type_node, cond3, cond4);

	  se->expr = fold_build3 (COND_EXPR, gfc_array_index_type, cond,
				  ubound, gfc_index_zero_node);
	}
      else
	{
	  if (as->type == AS_ASSUMED_SIZE)
	    cond = fold_build2 (EQ_EXPR, boolean_type_node, bound,
				build_int_cst (TREE_TYPE (bound),
					       arg->expr->rank - 1));
	  else
	    cond = boolean_false_node;

	  cond1 = fold_build2 (TRUTH_OR_EXPR, boolean_type_node, cond3, cond4);
	  cond = fold_build2 (TRUTH_OR_EXPR, boolean_type_node, cond, cond1);

	  se->expr = fold_build3 (COND_EXPR, gfc_array_index_type, cond,
				  lbound, gfc_index_one_node);
	}
    }
  else
    {
      if (upper)
        {
	  size = fold_build2 (MINUS_EXPR, gfc_array_index_type, ubound, lbound);
	  se->expr = fold_build2 (PLUS_EXPR, gfc_array_index_type, size,
				  gfc_index_one_node);
	}
      else
	se->expr = gfc_index_one_node;
    }

  type = gfc_typenode_for_spec (&expr->ts);
  se->expr = convert (type, se->expr);
}


static void
gfc_conv_intrinsic_abs (gfc_se * se, gfc_expr * expr)
{
  tree args;
  tree val;
  int n;

  args = gfc_conv_intrinsic_function_args (se, expr);
  gcc_assert (args && TREE_CHAIN (args) == NULL_TREE);
  val = TREE_VALUE (args);

  switch (expr->value.function.actual->expr->ts.type)
    {
    case BT_INTEGER:
    case BT_REAL:
      se->expr = build1 (ABS_EXPR, TREE_TYPE (val), val);
      break;

    case BT_COMPLEX:
      switch (expr->ts.kind)
	{
	case 4:
	  n = BUILT_IN_CABSF;
	  break;
	case 8:
	  n = BUILT_IN_CABS;
	  break;
	case 10:
	case 16:
	  n = BUILT_IN_CABSL;
	  break;
	default:
	  gcc_unreachable ();
	}
      se->expr = build_function_call_expr (built_in_decls[n], args);
      break;

    default:
      gcc_unreachable ();
    }
}


/* Create a complex value from one or two real components.  */

static void
gfc_conv_intrinsic_cmplx (gfc_se * se, gfc_expr * expr, int both)
{
  tree arg;
  tree real;
  tree imag;
  tree type;

  type = gfc_typenode_for_spec (&expr->ts);
  arg = gfc_conv_intrinsic_function_args (se, expr);
  real = convert (TREE_TYPE (type), TREE_VALUE (arg));
  if (both)
    imag = convert (TREE_TYPE (type), TREE_VALUE (TREE_CHAIN (arg)));
  else if (TREE_CODE (TREE_TYPE (TREE_VALUE (arg))) == COMPLEX_TYPE)
    {
      arg = TREE_VALUE (arg);
      imag = build1 (IMAGPART_EXPR, TREE_TYPE (TREE_TYPE (arg)), arg);
      imag = convert (TREE_TYPE (type), imag);
    }
  else
    imag = build_real_from_int_cst (TREE_TYPE (type), integer_zero_node);

  se->expr = fold_build2 (COMPLEX_EXPR, type, real, imag);
}

/* Remainder function MOD(A, P) = A - INT(A / P) * P
                      MODULO(A, P) = A - FLOOR (A / P) * P  */
/* TODO: MOD(x, 0)  */

static void
gfc_conv_intrinsic_mod (gfc_se * se, gfc_expr * expr, int modulo)
{
  tree arg;
  tree arg2;
  tree type;
  tree itype;
  tree tmp;
  tree test;
  tree test2;
  mpfr_t huge;
  int n, ikind;

  arg = gfc_conv_intrinsic_function_args (se, expr);

  switch (expr->ts.type)
    {
    case BT_INTEGER:
      /* Integer case is easy, we've got a builtin op.  */
      arg2 = TREE_VALUE (TREE_CHAIN (arg));
      arg = TREE_VALUE (arg);
      type = TREE_TYPE (arg);

      if (modulo)
       se->expr = build2 (FLOOR_MOD_EXPR, type, arg, arg2);
      else
       se->expr = build2 (TRUNC_MOD_EXPR, type, arg, arg2);
      break;

    case BT_REAL:
      n = END_BUILTINS;
      /* Check if we have a builtin fmod.  */
      switch (expr->ts.kind)
	{
	case 4:
	  n = BUILT_IN_FMODF;
	  break;

	case 8:
	  n = BUILT_IN_FMOD;
	  break;

	case 10:
	case 16:
	  n = BUILT_IN_FMODL;
	  break;

	default:
	  break;
	}

      /* Use it if it exists.  */
      if (n != END_BUILTINS)
	{
	  tmp = built_in_decls[n];
	  se->expr = build_function_call_expr (tmp, arg);
	  if (modulo == 0)
	    return;
	}

      arg2 = TREE_VALUE (TREE_CHAIN (arg));
      arg = TREE_VALUE (arg);
      type = TREE_TYPE (arg);

      arg = gfc_evaluate_now (arg, &se->pre);
      arg2 = gfc_evaluate_now (arg2, &se->pre);

      /* Definition:
	 modulo = arg - floor (arg/arg2) * arg2, so
		= test ? fmod (arg, arg2) : fmod (arg, arg2) + arg2, 
	 where
	  test  = (fmod (arg, arg2) != 0) && ((arg < 0) xor (arg2 < 0))
	 thereby avoiding another division and retaining the accuracy
	 of the builtin function.  */
      if (n != END_BUILTINS && modulo)
	{
	  tree zero = gfc_build_const (type, integer_zero_node);
	  tmp = gfc_evaluate_now (se->expr, &se->pre);
	  test = build2 (LT_EXPR, boolean_type_node, arg, zero);
	  test2 = build2 (LT_EXPR, boolean_type_node, arg2, zero);
	  test2 = build2 (TRUTH_XOR_EXPR, boolean_type_node, test, test2);
	  test = build2 (NE_EXPR, boolean_type_node, tmp, zero);
	  test = build2 (TRUTH_AND_EXPR, boolean_type_node, test, test2);
	  test = gfc_evaluate_now (test, &se->pre);
	  se->expr = build3 (COND_EXPR, type, test,
			     build2 (PLUS_EXPR, type, tmp, arg2), tmp);
	  return;
	}

      /* If we do not have a built_in fmod, the calculation is going to
	 have to be done longhand.  */
      tmp = build2 (RDIV_EXPR, type, arg, arg2);

      /* Test if the value is too large to handle sensibly.  */
      gfc_set_model_kind (expr->ts.kind);
      mpfr_init (huge);
      n = gfc_validate_kind (BT_INTEGER, expr->ts.kind, true);
      ikind = expr->ts.kind;
      if (n < 0)
	{
	  n = gfc_validate_kind (BT_INTEGER, gfc_max_integer_kind, false);
	  ikind = gfc_max_integer_kind;
	}
      mpfr_set_z (huge, gfc_integer_kinds[n].huge, GFC_RND_MODE);
      test = gfc_conv_mpfr_to_tree (huge, expr->ts.kind);
      test2 = build2 (LT_EXPR, boolean_type_node, tmp, test);

      mpfr_neg (huge, huge, GFC_RND_MODE);
      test = gfc_conv_mpfr_to_tree (huge, expr->ts.kind);
      test = build2 (GT_EXPR, boolean_type_node, tmp, test);
      test2 = build2 (TRUTH_AND_EXPR, boolean_type_node, test, test2);

      itype = gfc_get_int_type (ikind);
      if (modulo)
       tmp = build_fix_expr (&se->pre, tmp, itype, FIX_FLOOR_EXPR);
      else
       tmp = build_fix_expr (&se->pre, tmp, itype, FIX_TRUNC_EXPR);
      tmp = convert (type, tmp);
      tmp = build3 (COND_EXPR, type, test2, tmp, arg);
      tmp = build2 (MULT_EXPR, type, tmp, arg2);
      se->expr = build2 (MINUS_EXPR, type, arg, tmp);
      mpfr_clear (huge);
      break;

    default:
      gcc_unreachable ();
    }
}

/* Positive difference DIM (x, y) = ((x - y) < 0) ? 0 : x - y.  */

static void
gfc_conv_intrinsic_dim (gfc_se * se, gfc_expr * expr)
{
  tree arg;
  tree arg2;
  tree val;
  tree tmp;
  tree type;
  tree zero;

  arg = gfc_conv_intrinsic_function_args (se, expr);
  arg2 = TREE_VALUE (TREE_CHAIN (arg));
  arg = TREE_VALUE (arg);
  type = TREE_TYPE (arg);

  val = build2 (MINUS_EXPR, type, arg, arg2);
  val = gfc_evaluate_now (val, &se->pre);

  zero = gfc_build_const (type, integer_zero_node);
  tmp = build2 (LE_EXPR, boolean_type_node, val, zero);
  se->expr = build3 (COND_EXPR, type, tmp, zero, val);
}


/* SIGN(A, B) is absolute value of A times sign of B.
   The real value versions use library functions to ensure the correct
   handling of negative zero.  Integer case implemented as:
   SIGN(A, B) = { tmp = (A ^ B) >> C; (A + tmp) ^ tmp }
  */

static void
gfc_conv_intrinsic_sign (gfc_se * se, gfc_expr * expr)
{
  tree tmp;
  tree arg;
  tree arg2;
  tree type;

  arg = gfc_conv_intrinsic_function_args (se, expr);
  if (expr->ts.type == BT_REAL)
    {
      switch (expr->ts.kind)
	{
	case 4:
	  tmp = built_in_decls[BUILT_IN_COPYSIGNF];
	  break;
	case 8:
	  tmp = built_in_decls[BUILT_IN_COPYSIGN];
	  break;
	case 10:
	case 16:
	  tmp = built_in_decls[BUILT_IN_COPYSIGNL];
	  break;
	default:
	  gcc_unreachable ();
	}
      se->expr = build_function_call_expr (tmp, arg);
      return;
    }

  /* Having excluded floating point types, we know we are now dealing
     with signed integer types.  */
  arg2 = TREE_VALUE (TREE_CHAIN (arg));
  arg = TREE_VALUE (arg);
  type = TREE_TYPE (arg);

  /* Arg is used multiple times below.  */
  arg = gfc_evaluate_now (arg, &se->pre);

  /* Construct (A ^ B) >> 31, which generates a bit mask of all zeros if
     the signs of A and B are the same, and of all ones if they differ.  */
  tmp = fold_build2 (BIT_XOR_EXPR, type, arg, arg2);
  tmp = fold_build2 (RSHIFT_EXPR, type, tmp,
		     build_int_cst (type, TYPE_PRECISION (type) - 1));
  tmp = gfc_evaluate_now (tmp, &se->pre);

  /* Construct (A + tmp) ^ tmp, which is A if tmp is zero, and -A if tmp]
     is all ones (i.e. -1).  */
  se->expr = fold_build2 (BIT_XOR_EXPR, type,
			  fold_build2 (PLUS_EXPR, type, arg, tmp),
			  tmp);
}


/* Test for the presence of an optional argument.  */

static void
gfc_conv_intrinsic_present (gfc_se * se, gfc_expr * expr)
{
  gfc_expr *arg;

  arg = expr->value.function.actual->expr;
  gcc_assert (arg->expr_type == EXPR_VARIABLE);
  se->expr = gfc_conv_expr_present (arg->symtree->n.sym);
  se->expr = convert (gfc_typenode_for_spec (&expr->ts), se->expr);
}


/* Calculate the double precision product of two single precision values.  */

static void
gfc_conv_intrinsic_dprod (gfc_se * se, gfc_expr * expr)
{
  tree arg;
  tree arg2;
  tree type;

  arg = gfc_conv_intrinsic_function_args (se, expr);
  arg2 = TREE_VALUE (TREE_CHAIN (arg));
  arg = TREE_VALUE (arg);

  /* Convert the args to double precision before multiplying.  */
  type = gfc_typenode_for_spec (&expr->ts);
  arg = convert (type, arg);
  arg2 = convert (type, arg2);
  se->expr = build2 (MULT_EXPR, type, arg, arg2);
}


/* Return a length one character string containing an ascii character.  */

static void
gfc_conv_intrinsic_char (gfc_se * se, gfc_expr * expr)
{
  tree arg;
  tree var;
  tree type;

  arg = gfc_conv_intrinsic_function_args (se, expr);
  arg = TREE_VALUE (arg);

  /* We currently don't support character types != 1.  */
  gcc_assert (expr->ts.kind == 1);
  type = gfc_character1_type_node;
  var = gfc_create_var (type, "char");

  arg = convert (type, arg);
  gfc_add_modify_expr (&se->pre, var, arg);
  se->expr = gfc_build_addr_expr (build_pointer_type (type), var);
  se->string_length = integer_one_node;
}


static void
gfc_conv_intrinsic_ctime (gfc_se * se, gfc_expr * expr)
{
  tree var;
  tree len;
  tree tmp;
  tree arglist;
  tree type;
  tree cond;
  tree gfc_int8_type_node = gfc_get_int_type (8);

  type = build_pointer_type (gfc_character1_type_node);
  var = gfc_create_var (type, "pstr");
  len = gfc_create_var (gfc_int8_type_node, "len");

  tmp = gfc_conv_intrinsic_function_args (se, expr);
  arglist = gfc_chainon_list (NULL_TREE, build_fold_addr_expr (var));
  arglist = gfc_chainon_list (arglist, build_fold_addr_expr (len));
  arglist = chainon (arglist, tmp);

  tmp = build_function_call_expr (gfor_fndecl_ctime, arglist);
  gfc_add_expr_to_block (&se->pre, tmp);

  /* Free the temporary afterwards, if necessary.  */
  cond = build2 (GT_EXPR, boolean_type_node, len,
		 build_int_cst (TREE_TYPE (len), 0));
  arglist = gfc_chainon_list (NULL_TREE, var);
  tmp = build_function_call_expr (gfor_fndecl_internal_free, arglist);
  tmp = build3_v (COND_EXPR, cond, tmp, build_empty_stmt ());
  gfc_add_expr_to_block (&se->post, tmp);

  se->expr = var;
  se->string_length = len;
}


static void
gfc_conv_intrinsic_fdate (gfc_se * se, gfc_expr * expr)
{
  tree var;
  tree len;
  tree tmp;
  tree arglist;
  tree type;
  tree cond;
  tree gfc_int4_type_node = gfc_get_int_type (4);

  type = build_pointer_type (gfc_character1_type_node);
  var = gfc_create_var (type, "pstr");
  len = gfc_create_var (gfc_int4_type_node, "len");

  tmp = gfc_conv_intrinsic_function_args (se, expr);
  arglist = gfc_chainon_list (NULL_TREE, build_fold_addr_expr (var));
  arglist = gfc_chainon_list (arglist, build_fold_addr_expr (len));
  arglist = chainon (arglist, tmp);

  tmp = build_function_call_expr (gfor_fndecl_fdate, arglist);
  gfc_add_expr_to_block (&se->pre, tmp);

  /* Free the temporary afterwards, if necessary.  */
  cond = build2 (GT_EXPR, boolean_type_node, len,
		 build_int_cst (TREE_TYPE (len), 0));
  arglist = gfc_chainon_list (NULL_TREE, var);
  tmp = build_function_call_expr (gfor_fndecl_internal_free, arglist);
  tmp = build3_v (COND_EXPR, cond, tmp, build_empty_stmt ());
  gfc_add_expr_to_block (&se->post, tmp);

  se->expr = var;
  se->string_length = len;
}


/* Return a character string containing the tty name.  */

static void
gfc_conv_intrinsic_ttynam (gfc_se * se, gfc_expr * expr)
{
  tree var;
  tree len;
  tree tmp;
  tree arglist;
  tree type;
  tree cond;
  tree gfc_int4_type_node = gfc_get_int_type (4);

  type = build_pointer_type (gfc_character1_type_node);
  var = gfc_create_var (type, "pstr");
  len = gfc_create_var (gfc_int4_type_node, "len");

  tmp = gfc_conv_intrinsic_function_args (se, expr);
  arglist = gfc_chainon_list (NULL_TREE, build_fold_addr_expr (var));
  arglist = gfc_chainon_list (arglist, build_fold_addr_expr (len));
  arglist = chainon (arglist, tmp);

  tmp = build_function_call_expr (gfor_fndecl_ttynam, arglist);
  gfc_add_expr_to_block (&se->pre, tmp);

  /* Free the temporary afterwards, if necessary.  */
  cond = build2 (GT_EXPR, boolean_type_node, len,
		 build_int_cst (TREE_TYPE (len), 0));
  arglist = gfc_chainon_list (NULL_TREE, var);
  tmp = build_function_call_expr (gfor_fndecl_internal_free, arglist);
  tmp = build3_v (COND_EXPR, cond, tmp, build_empty_stmt ());
  gfc_add_expr_to_block (&se->post, tmp);

  se->expr = var;
  se->string_length = len;
}


/* Get the minimum/maximum value of all the parameters.
    minmax (a1, a2, a3, ...)
    {
      if (a2 .op. a1)
        mvar = a2;
      else
        mvar = a1;
      if (a3 .op. mvar)
        mvar = a3;
      ...
      return mvar
    }
 */

/* TODO: Mismatching types can occur when specific names are used.
   These should be handled during resolution.  */
static void
gfc_conv_intrinsic_minmax (gfc_se * se, gfc_expr * expr, int op)
{
  tree limit;
  tree tmp;
  tree mvar;
  tree val;
  tree thencase;
  tree elsecase;
  tree arg;
  tree type;

  arg = gfc_conv_intrinsic_function_args (se, expr);
  type = gfc_typenode_for_spec (&expr->ts);

  limit = TREE_VALUE (arg);
  if (TREE_TYPE (limit) != type)
    limit = convert (type, limit);
  /* Only evaluate the argument once.  */
  if (TREE_CODE (limit) != VAR_DECL && !TREE_CONSTANT (limit))
    limit = gfc_evaluate_now (limit, &se->pre);

  mvar = gfc_create_var (type, "M");
  elsecase = build2_v (MODIFY_EXPR, mvar, limit);
  for (arg = TREE_CHAIN (arg); arg != NULL_TREE; arg = TREE_CHAIN (arg))
    {
      val = TREE_VALUE (arg);
      if (TREE_TYPE (val) != type)
	val = convert (type, val);

      /* Only evaluate the argument once.  */
      if (TREE_CODE (val) != VAR_DECL && !TREE_CONSTANT (val))
        val = gfc_evaluate_now (val, &se->pre);

      thencase = build2_v (MODIFY_EXPR, mvar, convert (type, val));

      tmp = build2 (op, boolean_type_node, val, limit);
      tmp = build3_v (COND_EXPR, tmp, thencase, elsecase);
      gfc_add_expr_to_block (&se->pre, tmp);
      elsecase = build_empty_stmt ();
      limit = mvar;
    }
  se->expr = mvar;
}


/* Create a symbol node for this intrinsic.  The symbol from the frontend
   has the generic name.  */

static gfc_symbol *
gfc_get_symbol_for_expr (gfc_expr * expr)
{
  gfc_symbol *sym;

  /* TODO: Add symbols for intrinsic function to the global namespace.  */
  gcc_assert (strlen (expr->value.function.name) <= GFC_MAX_SYMBOL_LEN - 5);
  sym = gfc_new_symbol (expr->value.function.name, NULL);

  sym->ts = expr->ts;
  sym->attr.external = 1;
  sym->attr.function = 1;
  sym->attr.always_explicit = 1;
  sym->attr.proc = PROC_INTRINSIC;
  sym->attr.flavor = FL_PROCEDURE;
  sym->result = sym;
  if (expr->rank > 0)
    {
      sym->attr.dimension = 1;
      sym->as = gfc_get_array_spec ();
      sym->as->type = AS_ASSUMED_SHAPE;
      sym->as->rank = expr->rank;
    }

  /* TODO: proper argument lists for external intrinsics.  */
  return sym;
}

/* Generate a call to an external intrinsic function.  */
static void
gfc_conv_intrinsic_funcall (gfc_se * se, gfc_expr * expr)
{
  gfc_symbol *sym;

  gcc_assert (!se->ss || se->ss->expr == expr);

  if (se->ss)
    gcc_assert (expr->rank > 0);
  else
    gcc_assert (expr->rank == 0);

  sym = gfc_get_symbol_for_expr (expr);
  gfc_conv_function_call (se, sym, expr->value.function.actual);
  gfc_free (sym);
}

/* ANY and ALL intrinsics. ANY->op == NE_EXPR, ALL->op == EQ_EXPR.
   Implemented as
    any(a)
    {
      forall (i=...)
        if (a[i] != 0)
          return 1
      end forall
      return 0
    }
    all(a)
    {
      forall (i=...)
        if (a[i] == 0)
          return 0
      end forall
      return 1
    }
 */
static void
gfc_conv_intrinsic_anyall (gfc_se * se, gfc_expr * expr, int op)
{
  tree resvar;
  stmtblock_t block;
  stmtblock_t body;
  tree type;
  tree tmp;
  tree found;
  gfc_loopinfo loop;
  gfc_actual_arglist *actual;
  gfc_ss *arrayss;
  gfc_se arrayse;
  tree exit_label;

  if (se->ss)
    {
      gfc_conv_intrinsic_funcall (se, expr);
      return;
    }

  actual = expr->value.function.actual;
  type = gfc_typenode_for_spec (&expr->ts);
  /* Initialize the result.  */
  resvar = gfc_create_var (type, "test");
  if (op == EQ_EXPR)
    tmp = convert (type, boolean_true_node);
  else
    tmp = convert (type, boolean_false_node);
  gfc_add_modify_expr (&se->pre, resvar, tmp);

  /* Walk the arguments.  */
  arrayss = gfc_walk_expr (actual->expr);
  gcc_assert (arrayss != gfc_ss_terminator);

  /* Initialize the scalarizer.  */
  gfc_init_loopinfo (&loop);
  exit_label = gfc_build_label_decl (NULL_TREE);
  TREE_USED (exit_label) = 1;
  gfc_add_ss_to_loop (&loop, arrayss);

  /* Initialize the loop.  */
  gfc_conv_ss_startstride (&loop);
  gfc_conv_loop_setup (&loop);

  gfc_mark_ss_chain_used (arrayss, 1);
  /* Generate the loop body.  */
  gfc_start_scalarized_body (&loop, &body);

  /* If the condition matches then set the return value.  */
  gfc_start_block (&block);
  if (op == EQ_EXPR)
    tmp = convert (type, boolean_false_node);
  else
    tmp = convert (type, boolean_true_node);
  gfc_add_modify_expr (&block, resvar, tmp);

  /* And break out of the loop.  */
  tmp = build1_v (GOTO_EXPR, exit_label);
  gfc_add_expr_to_block (&block, tmp);

  found = gfc_finish_block (&block);

  /* Check this element.  */
  gfc_init_se (&arrayse, NULL);
  gfc_copy_loopinfo_to_se (&arrayse, &loop);
  arrayse.ss = arrayss;
  gfc_conv_expr_val (&arrayse, actual->expr);

  gfc_add_block_to_block (&body, &arrayse.pre);
  tmp = build2 (op, boolean_type_node, arrayse.expr,
		build_int_cst (TREE_TYPE (arrayse.expr), 0));
  tmp = build3_v (COND_EXPR, tmp, found, build_empty_stmt ());
  gfc_add_expr_to_block (&body, tmp);
  gfc_add_block_to_block (&body, &arrayse.post);

  gfc_trans_scalarizing_loops (&loop, &body);

  /* Add the exit label.  */
  tmp = build1_v (LABEL_EXPR, exit_label);
  gfc_add_expr_to_block (&loop.pre, tmp);

  gfc_add_block_to_block (&se->pre, &loop.pre);
  gfc_add_block_to_block (&se->pre, &loop.post);
  gfc_cleanup_loop (&loop);

  se->expr = resvar;
}

/* COUNT(A) = Number of true elements in A.  */
static void
gfc_conv_intrinsic_count (gfc_se * se, gfc_expr * expr)
{
  tree resvar;
  tree type;
  stmtblock_t body;
  tree tmp;
  gfc_loopinfo loop;
  gfc_actual_arglist *actual;
  gfc_ss *arrayss;
  gfc_se arrayse;

  if (se->ss)
    {
      gfc_conv_intrinsic_funcall (se, expr);
      return;
    }

  actual = expr->value.function.actual;

  type = gfc_typenode_for_spec (&expr->ts);
  /* Initialize the result.  */
  resvar = gfc_create_var (type, "count");
  gfc_add_modify_expr (&se->pre, resvar, build_int_cst (type, 0));

  /* Walk the arguments.  */
  arrayss = gfc_walk_expr (actual->expr);
  gcc_assert (arrayss != gfc_ss_terminator);

  /* Initialize the scalarizer.  */
  gfc_init_loopinfo (&loop);
  gfc_add_ss_to_loop (&loop, arrayss);

  /* Initialize the loop.  */
  gfc_conv_ss_startstride (&loop);
  gfc_conv_loop_setup (&loop);

  gfc_mark_ss_chain_used (arrayss, 1);
  /* Generate the loop body.  */
  gfc_start_scalarized_body (&loop, &body);

  tmp = build2 (PLUS_EXPR, TREE_TYPE (resvar), resvar,
		build_int_cst (TREE_TYPE (resvar), 1));
  tmp = build2_v (MODIFY_EXPR, resvar, tmp);

  gfc_init_se (&arrayse, NULL);
  gfc_copy_loopinfo_to_se (&arrayse, &loop);
  arrayse.ss = arrayss;
  gfc_conv_expr_val (&arrayse, actual->expr);
  tmp = build3_v (COND_EXPR, arrayse.expr, tmp, build_empty_stmt ());

  gfc_add_block_to_block (&body, &arrayse.pre);
  gfc_add_expr_to_block (&body, tmp);
  gfc_add_block_to_block (&body, &arrayse.post);

  gfc_trans_scalarizing_loops (&loop, &body);

  gfc_add_block_to_block (&se->pre, &loop.pre);
  gfc_add_block_to_block (&se->pre, &loop.post);
  gfc_cleanup_loop (&loop);

  se->expr = resvar;
}

/* Inline implementation of the sum and product intrinsics.  */
static void
gfc_conv_intrinsic_arith (gfc_se * se, gfc_expr * expr, int op)
{
  tree resvar;
  tree type;
  stmtblock_t body;
  stmtblock_t block;
  tree tmp;
  gfc_loopinfo loop;
  gfc_actual_arglist *actual;
  gfc_ss *arrayss;
  gfc_ss *maskss;
  gfc_se arrayse;
  gfc_se maskse;
  gfc_expr *arrayexpr;
  gfc_expr *maskexpr;

  if (se->ss)
    {
      gfc_conv_intrinsic_funcall (se, expr);
      return;
    }

  type = gfc_typenode_for_spec (&expr->ts);
  /* Initialize the result.  */
  resvar = gfc_create_var (type, "val");
  if (op == PLUS_EXPR)
    tmp = gfc_build_const (type, integer_zero_node);
  else
    tmp = gfc_build_const (type, integer_one_node);

  gfc_add_modify_expr (&se->pre, resvar, tmp);

  /* Walk the arguments.  */
  actual = expr->value.function.actual;
  arrayexpr = actual->expr;
  arrayss = gfc_walk_expr (arrayexpr);
  gcc_assert (arrayss != gfc_ss_terminator);

  actual = actual->next->next;
  gcc_assert (actual);
  maskexpr = actual->expr;
  if (maskexpr && maskexpr->rank != 0)
    {
      maskss = gfc_walk_expr (maskexpr);
      gcc_assert (maskss != gfc_ss_terminator);
    }
  else
    maskss = NULL;

  /* Initialize the scalarizer.  */
  gfc_init_loopinfo (&loop);
  gfc_add_ss_to_loop (&loop, arrayss);
  if (maskss)
    gfc_add_ss_to_loop (&loop, maskss);

  /* Initialize the loop.  */
  gfc_conv_ss_startstride (&loop);
  gfc_conv_loop_setup (&loop);

  gfc_mark_ss_chain_used (arrayss, 1);
  if (maskss)
    gfc_mark_ss_chain_used (maskss, 1);
  /* Generate the loop body.  */
  gfc_start_scalarized_body (&loop, &body);

  /* If we have a mask, only add this element if the mask is set.  */
  if (maskss)
    {
      gfc_init_se (&maskse, NULL);
      gfc_copy_loopinfo_to_se (&maskse, &loop);
      maskse.ss = maskss;
      gfc_conv_expr_val (&maskse, maskexpr);
      gfc_add_block_to_block (&body, &maskse.pre);

      gfc_start_block (&block);
    }
  else
    gfc_init_block (&block);

  /* Do the actual summation/product.  */
  gfc_init_se (&arrayse, NULL);
  gfc_copy_loopinfo_to_se (&arrayse, &loop);
  arrayse.ss = arrayss;
  gfc_conv_expr_val (&arrayse, arrayexpr);
  gfc_add_block_to_block (&block, &arrayse.pre);

  tmp = build2 (op, type, resvar, arrayse.expr);
  gfc_add_modify_expr (&block, resvar, tmp);
  gfc_add_block_to_block (&block, &arrayse.post);

  if (maskss)
    {
      /* We enclose the above in if (mask) {...} .  */
      tmp = gfc_finish_block (&block);

      tmp = build3_v (COND_EXPR, maskse.expr, tmp, build_empty_stmt ());
    }
  else
    tmp = gfc_finish_block (&block);
  gfc_add_expr_to_block (&body, tmp);

  gfc_trans_scalarizing_loops (&loop, &body);

  /* For a scalar mask, enclose the loop in an if statement.  */
  if (maskexpr && maskss == NULL)
    {
      gfc_init_se (&maskse, NULL);
      gfc_conv_expr_val (&maskse, maskexpr);
      gfc_init_block (&block);
      gfc_add_block_to_block (&block, &loop.pre);
      gfc_add_block_to_block (&block, &loop.post);
      tmp = gfc_finish_block (&block);

      tmp = build3_v (COND_EXPR, maskse.expr, tmp, build_empty_stmt ());
      gfc_add_expr_to_block (&block, tmp);
      gfc_add_block_to_block (&se->pre, &block);
    }
  else
    {
      gfc_add_block_to_block (&se->pre, &loop.pre);
      gfc_add_block_to_block (&se->pre, &loop.post);
    }

  gfc_cleanup_loop (&loop);

  se->expr = resvar;
}


/* Inline implementation of the dot_product intrinsic. This function
   is based on gfc_conv_intrinsic_arith (the previous function).  */
static void
gfc_conv_intrinsic_dot_product (gfc_se * se, gfc_expr * expr)
{
  tree resvar;
  tree type;
  stmtblock_t body;
  stmtblock_t block;
  tree tmp;
  gfc_loopinfo loop;
  gfc_actual_arglist *actual;
  gfc_ss *arrayss1, *arrayss2;
  gfc_se arrayse1, arrayse2;
  gfc_expr *arrayexpr1, *arrayexpr2;

  type = gfc_typenode_for_spec (&expr->ts);

  /* Initialize the result.  */
  resvar = gfc_create_var (type, "val");
  if (expr->ts.type == BT_LOGICAL)
    tmp = convert (type, integer_zero_node);
  else
    tmp = gfc_build_const (type, integer_zero_node);

  gfc_add_modify_expr (&se->pre, resvar, tmp);

  /* Walk argument #1.  */
  actual = expr->value.function.actual;
  arrayexpr1 = actual->expr;
  arrayss1 = gfc_walk_expr (arrayexpr1);
  gcc_assert (arrayss1 != gfc_ss_terminator);

  /* Walk argument #2.  */
  actual = actual->next;
  arrayexpr2 = actual->expr;
  arrayss2 = gfc_walk_expr (arrayexpr2);
  gcc_assert (arrayss2 != gfc_ss_terminator);

  /* Initialize the scalarizer.  */
  gfc_init_loopinfo (&loop);
  gfc_add_ss_to_loop (&loop, arrayss1);
  gfc_add_ss_to_loop (&loop, arrayss2);

  /* Initialize the loop.  */
  gfc_conv_ss_startstride (&loop);
  gfc_conv_loop_setup (&loop);

  gfc_mark_ss_chain_used (arrayss1, 1);
  gfc_mark_ss_chain_used (arrayss2, 1);

  /* Generate the loop body.  */
  gfc_start_scalarized_body (&loop, &body);
  gfc_init_block (&block);

  /* Make the tree expression for [conjg(]array1[)].  */
  gfc_init_se (&arrayse1, NULL);
  gfc_copy_loopinfo_to_se (&arrayse1, &loop);
  arrayse1.ss = arrayss1;
  gfc_conv_expr_val (&arrayse1, arrayexpr1);
  if (expr->ts.type == BT_COMPLEX)
    arrayse1.expr = build1 (CONJ_EXPR, type, arrayse1.expr);
  gfc_add_block_to_block (&block, &arrayse1.pre);

  /* Make the tree expression for array2.  */
  gfc_init_se (&arrayse2, NULL);
  gfc_copy_loopinfo_to_se (&arrayse2, &loop);
  arrayse2.ss = arrayss2;
  gfc_conv_expr_val (&arrayse2, arrayexpr2);
  gfc_add_block_to_block (&block, &arrayse2.pre);

  /* Do the actual product and sum.  */
  if (expr->ts.type == BT_LOGICAL)
    {
      tmp = build2 (TRUTH_AND_EXPR, type, arrayse1.expr, arrayse2.expr);
      tmp = build2 (TRUTH_OR_EXPR, type, resvar, tmp);
    }
  else
    {
      tmp = build2 (MULT_EXPR, type, arrayse1.expr, arrayse2.expr);
      tmp = build2 (PLUS_EXPR, type, resvar, tmp);
    }
  gfc_add_modify_expr (&block, resvar, tmp);

  /* Finish up the loop block and the loop.  */
  tmp = gfc_finish_block (&block);
  gfc_add_expr_to_block (&body, tmp);

  gfc_trans_scalarizing_loops (&loop, &body);
  gfc_add_block_to_block (&se->pre, &loop.pre);
  gfc_add_block_to_block (&se->pre, &loop.post);
  gfc_cleanup_loop (&loop);

  se->expr = resvar;
}


static void
gfc_conv_intrinsic_minmaxloc (gfc_se * se, gfc_expr * expr, int op)
{
  stmtblock_t body;
  stmtblock_t block;
  stmtblock_t ifblock;
  stmtblock_t elseblock;
  tree limit;
  tree type;
  tree tmp;
  tree elsetmp;
  tree ifbody;
  gfc_loopinfo loop;
  gfc_actual_arglist *actual;
  gfc_ss *arrayss;
  gfc_ss *maskss;
  gfc_se arrayse;
  gfc_se maskse;
  gfc_expr *arrayexpr;
  gfc_expr *maskexpr;
  tree pos;
  int n;

  if (se->ss)
    {
      gfc_conv_intrinsic_funcall (se, expr);
      return;
    }

  /* Initialize the result.  */
  pos = gfc_create_var (gfc_array_index_type, "pos");
  type = gfc_typenode_for_spec (&expr->ts);

  /* Walk the arguments.  */
  actual = expr->value.function.actual;
  arrayexpr = actual->expr;
  arrayss = gfc_walk_expr (arrayexpr);
  gcc_assert (arrayss != gfc_ss_terminator);

  actual = actual->next->next;
  gcc_assert (actual);
  maskexpr = actual->expr;
  if (maskexpr && maskexpr->rank != 0)
    {
      maskss = gfc_walk_expr (maskexpr);
      gcc_assert (maskss != gfc_ss_terminator);
    }
  else
    maskss = NULL;

  limit = gfc_create_var (gfc_typenode_for_spec (&arrayexpr->ts), "limit");
  n = gfc_validate_kind (arrayexpr->ts.type, arrayexpr->ts.kind, false);
  switch (arrayexpr->ts.type)
    {
    case BT_REAL:
      tmp = gfc_conv_mpfr_to_tree (gfc_real_kinds[n].huge, arrayexpr->ts.kind);
      break;

    case BT_INTEGER:
      tmp = gfc_conv_mpz_to_tree (gfc_integer_kinds[n].huge,
				  arrayexpr->ts.kind);
      break;

    default:
      gcc_unreachable ();
    }

  /* We start with the most negative possible value for MAXLOC, and the most
     positive possible value for MINLOC. The most negative possible value is
     -HUGE for BT_REAL and (-HUGE - 1) for BT_INTEGER; the most positive
     possible value is HUGE in both cases. */
  if (op == GT_EXPR)
    tmp = fold_build1 (NEGATE_EXPR, TREE_TYPE (tmp), tmp);
  gfc_add_modify_expr (&se->pre, limit, tmp);

  if (op == GT_EXPR && expr->ts.type == BT_INTEGER)
    tmp = build2 (MINUS_EXPR, TREE_TYPE (tmp), tmp,
		  build_int_cst (type, 1));

  /* Initialize the scalarizer.  */
  gfc_init_loopinfo (&loop);
  gfc_add_ss_to_loop (&loop, arrayss);
  if (maskss)
    gfc_add_ss_to_loop (&loop, maskss);

  /* Initialize the loop.  */
  gfc_conv_ss_startstride (&loop);
  gfc_conv_loop_setup (&loop);

  gcc_assert (loop.dimen == 1);

  /* Initialize the position to zero, following Fortran 2003.  We are free
     to do this because Fortran 95 allows the result of an entirely false
     mask to be processor dependent.  */
  gfc_add_modify_expr (&loop.pre, pos, gfc_index_zero_node);

  gfc_mark_ss_chain_used (arrayss, 1);
  if (maskss)
    gfc_mark_ss_chain_used (maskss, 1);
  /* Generate the loop body.  */
  gfc_start_scalarized_body (&loop, &body);

  /* If we have a mask, only check this element if the mask is set.  */
  if (maskss)
    {
      gfc_init_se (&maskse, NULL);
      gfc_copy_loopinfo_to_se (&maskse, &loop);
      maskse.ss = maskss;
      gfc_conv_expr_val (&maskse, maskexpr);
      gfc_add_block_to_block (&body, &maskse.pre);

      gfc_start_block (&block);
    }
  else
    gfc_init_block (&block);

  /* Compare with the current limit.  */
  gfc_init_se (&arrayse, NULL);
  gfc_copy_loopinfo_to_se (&arrayse, &loop);
  arrayse.ss = arrayss;
  gfc_conv_expr_val (&arrayse, arrayexpr);
  gfc_add_block_to_block (&block, &arrayse.pre);

  /* We do the following if this is a more extreme value.  */
  gfc_start_block (&ifblock);

  /* Assign the value to the limit...  */
  gfc_add_modify_expr (&ifblock, limit, arrayse.expr);

  /* Remember where we are.  */
  gfc_add_modify_expr (&ifblock, pos, loop.loopvar[0]);

  ifbody = gfc_finish_block (&ifblock);

  /* If it is a more extreme value or pos is still zero.  */
  tmp = build2 (TRUTH_OR_EXPR, boolean_type_node,
		  build2 (op, boolean_type_node, arrayse.expr, limit),
		  build2 (EQ_EXPR, boolean_type_node, pos, gfc_index_zero_node));
  tmp = build3_v (COND_EXPR, tmp, ifbody, build_empty_stmt ());
  gfc_add_expr_to_block (&block, tmp);

  if (maskss)
    {
      /* We enclose the above in if (mask) {...}.  */
      tmp = gfc_finish_block (&block);

      tmp = build3_v (COND_EXPR, maskse.expr, tmp, build_empty_stmt ());
    }
  else
    tmp = gfc_finish_block (&block);
  gfc_add_expr_to_block (&body, tmp);

  gfc_trans_scalarizing_loops (&loop, &body);

  /* For a scalar mask, enclose the loop in an if statement.  */
  if (maskexpr && maskss == NULL)
    {
      gfc_init_se (&maskse, NULL);
      gfc_conv_expr_val (&maskse, maskexpr);
      gfc_init_block (&block);
      gfc_add_block_to_block (&block, &loop.pre);
      gfc_add_block_to_block (&block, &loop.post);
      tmp = gfc_finish_block (&block);

      /* For the else part of the scalar mask, just initialize
	 the pos variable the same way as above.  */

      gfc_init_block (&elseblock);
      gfc_add_modify_expr (&elseblock, pos, gfc_index_zero_node);
      elsetmp = gfc_finish_block (&elseblock);

      tmp = build3_v (COND_EXPR, maskse.expr, tmp, elsetmp);
      gfc_add_expr_to_block (&block, tmp);
      gfc_add_block_to_block (&se->pre, &block);
    }
  else
    {
      gfc_add_block_to_block (&se->pre, &loop.pre);
      gfc_add_block_to_block (&se->pre, &loop.post);
    }
  gfc_cleanup_loop (&loop);

  /* Return a value in the range 1..SIZE(array).  */
  tmp = fold_build2 (MINUS_EXPR, gfc_array_index_type, loop.from[0],
		     gfc_index_one_node);
  tmp = fold_build2 (MINUS_EXPR, gfc_array_index_type, pos, tmp);
  /* And convert to the required type.  */
  se->expr = convert (type, tmp);
}

static void
gfc_conv_intrinsic_minmaxval (gfc_se * se, gfc_expr * expr, int op)
{
  tree limit;
  tree type;
  tree tmp;
  tree ifbody;
  stmtblock_t body;
  stmtblock_t block;
  gfc_loopinfo loop;
  gfc_actual_arglist *actual;
  gfc_ss *arrayss;
  gfc_ss *maskss;
  gfc_se arrayse;
  gfc_se maskse;
  gfc_expr *arrayexpr;
  gfc_expr *maskexpr;
  int n;

  if (se->ss)
    {
      gfc_conv_intrinsic_funcall (se, expr);
      return;
    }

  type = gfc_typenode_for_spec (&expr->ts);
  /* Initialize the result.  */
  limit = gfc_create_var (type, "limit");
  n = gfc_validate_kind (expr->ts.type, expr->ts.kind, false);
  switch (expr->ts.type)
    {
    case BT_REAL:
      tmp = gfc_conv_mpfr_to_tree (gfc_real_kinds[n].huge, expr->ts.kind);
      break;

    case BT_INTEGER:
      tmp = gfc_conv_mpz_to_tree (gfc_integer_kinds[n].huge, expr->ts.kind);
      break;

    default:
      gcc_unreachable ();
    }

  /* We start with the most negative possible value for MAXVAL, and the most
     positive possible value for MINVAL. The most negative possible value is
     -HUGE for BT_REAL and (-HUGE - 1) for BT_INTEGER; the most positive
     possible value is HUGE in both cases. */
  if (op == GT_EXPR)
    tmp = fold_build1 (NEGATE_EXPR, TREE_TYPE (tmp), tmp);

  if (op == GT_EXPR && expr->ts.type == BT_INTEGER)
    tmp = build2 (MINUS_EXPR, TREE_TYPE (tmp), tmp,
		  build_int_cst (type, 1));

  gfc_add_modify_expr (&se->pre, limit, tmp);

  /* Walk the arguments.  */
  actual = expr->value.function.actual;
  arrayexpr = actual->expr;
  arrayss = gfc_walk_expr (arrayexpr);
  gcc_assert (arrayss != gfc_ss_terminator);

  actual = actual->next->next;
  gcc_assert (actual);
  maskexpr = actual->expr;
  if (maskexpr && maskexpr->rank != 0)
    {
      maskss = gfc_walk_expr (maskexpr);
      gcc_assert (maskss != gfc_ss_terminator);
    }
  else
    maskss = NULL;

  /* Initialize the scalarizer.  */
  gfc_init_loopinfo (&loop);
  gfc_add_ss_to_loop (&loop, arrayss);
  if (maskss)
    gfc_add_ss_to_loop (&loop, maskss);

  /* Initialize the loop.  */
  gfc_conv_ss_startstride (&loop);
  gfc_conv_loop_setup (&loop);

  gfc_mark_ss_chain_used (arrayss, 1);
  if (maskss)
    gfc_mark_ss_chain_used (maskss, 1);
  /* Generate the loop body.  */
  gfc_start_scalarized_body (&loop, &body);

  /* If we have a mask, only add this element if the mask is set.  */
  if (maskss)
    {
      gfc_init_se (&maskse, NULL);
      gfc_copy_loopinfo_to_se (&maskse, &loop);
      maskse.ss = maskss;
      gfc_conv_expr_val (&maskse, maskexpr);
      gfc_add_block_to_block (&body, &maskse.pre);

      gfc_start_block (&block);
    }
  else
    gfc_init_block (&block);

  /* Compare with the current limit.  */
  gfc_init_se (&arrayse, NULL);
  gfc_copy_loopinfo_to_se (&arrayse, &loop);
  arrayse.ss = arrayss;
  gfc_conv_expr_val (&arrayse, arrayexpr);
  gfc_add_block_to_block (&block, &arrayse.pre);

  /* Assign the value to the limit...  */
  ifbody = build2_v (MODIFY_EXPR, limit, arrayse.expr);

  /* If it is a more extreme value.  */
  tmp = build2 (op, boolean_type_node, arrayse.expr, limit);
  tmp = build3_v (COND_EXPR, tmp, ifbody, build_empty_stmt ());
  gfc_add_expr_to_block (&block, tmp);
  gfc_add_block_to_block (&block, &arrayse.post);

  tmp = gfc_finish_block (&block);
  if (maskss)
    /* We enclose the above in if (mask) {...}.  */
    tmp = build3_v (COND_EXPR, maskse.expr, tmp, build_empty_stmt ());
  gfc_add_expr_to_block (&body, tmp);

  gfc_trans_scalarizing_loops (&loop, &body);

  /* For a scalar mask, enclose the loop in an if statement.  */
  if (maskexpr && maskss == NULL)
    {
      gfc_init_se (&maskse, NULL);
      gfc_conv_expr_val (&maskse, maskexpr);
      gfc_init_block (&block);
      gfc_add_block_to_block (&block, &loop.pre);
      gfc_add_block_to_block (&block, &loop.post);
      tmp = gfc_finish_block (&block);

      tmp = build3_v (COND_EXPR, maskse.expr, tmp, build_empty_stmt ());
      gfc_add_expr_to_block (&block, tmp);
      gfc_add_block_to_block (&se->pre, &block);
    }
  else
    {
      gfc_add_block_to_block (&se->pre, &loop.pre);
      gfc_add_block_to_block (&se->pre, &loop.post);
    }

  gfc_cleanup_loop (&loop);

  se->expr = limit;
}

/* BTEST (i, pos) = (i & (1 << pos)) != 0.  */
static void
gfc_conv_intrinsic_btest (gfc_se * se, gfc_expr * expr)
{
  tree arg;
  tree arg2;
  tree type;
  tree tmp;

  arg = gfc_conv_intrinsic_function_args (se, expr);
  arg2 = TREE_VALUE (TREE_CHAIN (arg));
  arg = TREE_VALUE (arg);
  type = TREE_TYPE (arg);

  tmp = build2 (LSHIFT_EXPR, type, build_int_cst (type, 1), arg2);
  tmp = build2 (BIT_AND_EXPR, type, arg, tmp);
  tmp = fold_build2 (NE_EXPR, boolean_type_node, tmp,
		     build_int_cst (type, 0));
  type = gfc_typenode_for_spec (&expr->ts);
  se->expr = convert (type, tmp);
}

/* Generate code to perform the specified operation.  */
static void
gfc_conv_intrinsic_bitop (gfc_se * se, gfc_expr * expr, int op)
{
  tree arg;
  tree arg2;
  tree type;

  arg = gfc_conv_intrinsic_function_args (se, expr);
  arg2 = TREE_VALUE (TREE_CHAIN (arg));
  arg = TREE_VALUE (arg);
  type = TREE_TYPE (arg);

  se->expr = fold_build2 (op, type, arg, arg2);
}

/* Bitwise not.  */
static void
gfc_conv_intrinsic_not (gfc_se * se, gfc_expr * expr)
{
  tree arg;

  arg = gfc_conv_intrinsic_function_args (se, expr);
  arg = TREE_VALUE (arg);

  se->expr = build1 (BIT_NOT_EXPR, TREE_TYPE (arg), arg);
}

/* Set or clear a single bit.  */
static void
gfc_conv_intrinsic_singlebitop (gfc_se * se, gfc_expr * expr, int set)
{
  tree arg;
  tree arg2;
  tree type;
  tree tmp;
  int op;

  arg = gfc_conv_intrinsic_function_args (se, expr);
  arg2 = TREE_VALUE (TREE_CHAIN (arg));
  arg = TREE_VALUE (arg);
  type = TREE_TYPE (arg);

  tmp = fold_build2 (LSHIFT_EXPR, type, build_int_cst (type, 1), arg2);
  if (set)
    op = BIT_IOR_EXPR;
  else
    {
      op = BIT_AND_EXPR;
      tmp = fold_build1 (BIT_NOT_EXPR, type, tmp);
    }
  se->expr = fold_build2 (op, type, arg, tmp);
}

/* Extract a sequence of bits.
    IBITS(I, POS, LEN) = (I >> POS) & ~((~0) << LEN).  */
static void
gfc_conv_intrinsic_ibits (gfc_se * se, gfc_expr * expr)
{
  tree arg;
  tree arg2;
  tree arg3;
  tree type;
  tree tmp;
  tree mask;

  arg = gfc_conv_intrinsic_function_args (se, expr);
  arg2 = TREE_CHAIN (arg);
  arg3 = TREE_VALUE (TREE_CHAIN (arg2));
  arg = TREE_VALUE (arg);
  arg2 = TREE_VALUE (arg2);
  type = TREE_TYPE (arg);

  mask = build_int_cst (type, -1);
  mask = build2 (LSHIFT_EXPR, type, mask, arg3);
  mask = build1 (BIT_NOT_EXPR, type, mask);

  tmp = build2 (RSHIFT_EXPR, type, arg, arg2);

  se->expr = fold_build2 (BIT_AND_EXPR, type, tmp, mask);
}

/* RSHIFT (I, SHIFT) = I >> SHIFT
   LSHIFT (I, SHIFT) = I << SHIFT  */
static void
gfc_conv_intrinsic_rlshift (gfc_se * se, gfc_expr * expr, int right_shift)
{
  tree arg;
  tree arg2;

  arg = gfc_conv_intrinsic_function_args (se, expr);
  arg2 = TREE_VALUE (TREE_CHAIN (arg));
  arg = TREE_VALUE (arg);

  se->expr = fold_build2 (right_shift ? RSHIFT_EXPR : LSHIFT_EXPR,
			  TREE_TYPE (arg), arg, arg2);
}

/* ISHFT (I, SHIFT) = (abs (shift) >= BIT_SIZE (i))
                        ? 0
	 	        : ((shift >= 0) ? i << shift : i >> -shift)
   where all shifts are logical shifts.  */
static void
gfc_conv_intrinsic_ishft (gfc_se * se, gfc_expr * expr)
{
  tree arg;
  tree arg2;
  tree type;
  tree utype;
  tree tmp;
  tree width;
  tree num_bits;
  tree cond;
  tree lshift;
  tree rshift;

  arg = gfc_conv_intrinsic_function_args (se, expr);
  arg2 = TREE_VALUE (TREE_CHAIN (arg));
  arg = TREE_VALUE (arg);
  type = TREE_TYPE (arg);
  utype = gfc_unsigned_type (type);

  width = fold_build1 (ABS_EXPR, TREE_TYPE (arg2), arg2);

  /* Left shift if positive.  */
  lshift = fold_build2 (LSHIFT_EXPR, type, arg, width);

  /* Right shift if negative.
     We convert to an unsigned type because we want a logical shift.
     The standard doesn't define the case of shifting negative
     numbers, and we try to be compatible with other compilers, most
     notably g77, here.  */
  rshift = fold_convert (type, build2 (RSHIFT_EXPR, utype, 
				       convert (utype, arg), width));

  tmp = fold_build2 (GE_EXPR, boolean_type_node, arg2,
		     build_int_cst (TREE_TYPE (arg2), 0));
  tmp = fold_build3 (COND_EXPR, type, tmp, lshift, rshift);

  /* The Fortran standard allows shift widths <= BIT_SIZE(I), whereas
     gcc requires a shift width < BIT_SIZE(I), so we have to catch this
     special case.  */
  num_bits = build_int_cst (TREE_TYPE (arg2), TYPE_PRECISION (type));
  cond = fold_build2 (GE_EXPR, boolean_type_node, width, num_bits);

  se->expr = fold_build3 (COND_EXPR, type, cond,
			  build_int_cst (type, 0), tmp);
}

/* Circular shift.  AKA rotate or barrel shift.  */
static void
gfc_conv_intrinsic_ishftc (gfc_se * se, gfc_expr * expr)
{
  tree arg;
  tree arg2;
  tree arg3;
  tree type;
  tree tmp;
  tree lrot;
  tree rrot;
  tree zero;

  arg = gfc_conv_intrinsic_function_args (se, expr);
  arg2 = TREE_CHAIN (arg);
  arg3 = TREE_CHAIN (arg2);
  if (arg3)
    {
      /* Use a library function for the 3 parameter version.  */
      tree int4type = gfc_get_int_type (4);

      type = TREE_TYPE (TREE_VALUE (arg));
      /* We convert the first argument to at least 4 bytes, and
	 convert back afterwards.  This removes the need for library
	 functions for all argument sizes, and function will be
	 aligned to at least 32 bits, so there's no loss.  */
      if (expr->ts.kind < 4)
	{
	  tmp = convert (int4type, TREE_VALUE (arg));
	  TREE_VALUE (arg) = tmp;
	}
      /* Convert the SHIFT and SIZE args to INTEGER*4 otherwise we would
         need loads of library  functions.  They cannot have values >
	 BIT_SIZE (I) so the conversion is safe.  */
      TREE_VALUE (arg2) = convert (int4type, TREE_VALUE (arg2));
      TREE_VALUE (arg3) = convert (int4type, TREE_VALUE (arg3));

      switch (expr->ts.kind)
	{
	case 1:
	case 2:
	case 4:
	  tmp = gfor_fndecl_math_ishftc4;
	  break;
	case 8:
	  tmp = gfor_fndecl_math_ishftc8;
	  break;
	case 16:
	  tmp = gfor_fndecl_math_ishftc16;
	  break;
	default:
	  gcc_unreachable ();
	}
      se->expr = build_function_call_expr (tmp, arg);
      /* Convert the result back to the original type, if we extended
	 the first argument's width above.  */
      if (expr->ts.kind < 4)
	se->expr = convert (type, se->expr);

      return;
    }
  arg = TREE_VALUE (arg);
  arg2 = TREE_VALUE (arg2);
  type = TREE_TYPE (arg);

  /* Rotate left if positive.  */
  lrot = fold_build2 (LROTATE_EXPR, type, arg, arg2);

  /* Rotate right if negative.  */
  tmp = fold_build1 (NEGATE_EXPR, TREE_TYPE (arg2), arg2);
  rrot = fold_build2 (RROTATE_EXPR, type, arg, tmp);

  zero = build_int_cst (TREE_TYPE (arg2), 0);
  tmp = fold_build2 (GT_EXPR, boolean_type_node, arg2, zero);
  rrot = fold_build3 (COND_EXPR, type, tmp, lrot, rrot);

  /* Do nothing if shift == 0.  */
  tmp = fold_build2 (EQ_EXPR, boolean_type_node, arg2, zero);
  se->expr = fold_build3 (COND_EXPR, type, tmp, arg, rrot);
}

/* The length of a character string.  */
static void
gfc_conv_intrinsic_len (gfc_se * se, gfc_expr * expr)
{
  tree len;
  tree type;
  tree decl;
  gfc_symbol *sym;
  gfc_se argse;
  gfc_expr *arg;
  gfc_ss *ss;

  gcc_assert (!se->ss);

  arg = expr->value.function.actual->expr;

  type = gfc_typenode_for_spec (&expr->ts);
  switch (arg->expr_type)
    {
    case EXPR_CONSTANT:
      len = build_int_cst (NULL_TREE, arg->value.character.length);
      break;

    case EXPR_ARRAY:
      /* Obtain the string length from the function used by
         trans-array.c(gfc_trans_array_constructor).  */
      len = NULL_TREE;
      get_array_ctor_strlen (arg->value.constructor, &len);
      break;

    case EXPR_VARIABLE:
      if (arg->ref == NULL
	    || (arg->ref->next == NULL && arg->ref->type == REF_ARRAY))
	{
	  /* This doesn't catch all cases.
	     See http://gcc.gnu.org/ml/fortran/2004-06/msg00165.html
	     and the surrounding thread.  */
	  sym = arg->symtree->n.sym;
	  decl = gfc_get_symbol_decl (sym);
	  if (decl == current_function_decl && sym->attr.function
		&& (sym->result == sym))
	    decl = gfc_get_fake_result_decl (sym, 0);

	  len = sym->ts.cl->backend_decl;
	  gcc_assert (len);
	  break;
	}

      /* Otherwise fall through.  */

    default:
      /* Anybody stupid enough to do this deserves inefficient code.  */
      ss = gfc_walk_expr (arg);
      gfc_init_se (&argse, se);
      if (ss == gfc_ss_terminator)
	gfc_conv_expr (&argse, arg);
      else
	gfc_conv_expr_descriptor (&argse, arg, ss);
      gfc_add_block_to_block (&se->pre, &argse.pre);
      gfc_add_block_to_block (&se->post, &argse.post);
      len = argse.string_length;
      break;
    }
  se->expr = convert (type, len);
}

/* The length of a character string not including trailing blanks.  */
static void
gfc_conv_intrinsic_len_trim (gfc_se * se, gfc_expr * expr)
{
  tree args;
  tree type;

  args = gfc_conv_intrinsic_function_args (se, expr);
  type = gfc_typenode_for_spec (&expr->ts);
  se->expr = build_function_call_expr (gfor_fndecl_string_len_trim, args);
  se->expr = convert (type, se->expr);
}


/* Returns the starting position of a substring within a string.  */

static void
gfc_conv_intrinsic_index (gfc_se * se, gfc_expr * expr)
{
  tree logical4_type_node = gfc_get_logical_type (4);
  tree args;
  tree back;
  tree type;
  tree tmp;

  args = gfc_conv_intrinsic_function_args (se, expr);
  type = gfc_typenode_for_spec (&expr->ts);
  tmp = gfc_advance_chain (args, 3);
  if (TREE_CHAIN (tmp) == NULL_TREE)
    {
      back = tree_cons (NULL_TREE, build_int_cst (logical4_type_node, 0),
			NULL_TREE);
      TREE_CHAIN (tmp) = back;
    }
  else
    {
      back = TREE_CHAIN (tmp);
      TREE_VALUE (back) = convert (logical4_type_node, TREE_VALUE (back));
    }

  se->expr = build_function_call_expr (gfor_fndecl_string_index, args);
  se->expr = convert (type, se->expr);
}

/* The ascii value for a single character.  */
static void
gfc_conv_intrinsic_ichar (gfc_se * se, gfc_expr * expr)
{
  tree arg;
  tree type;

  arg = gfc_conv_intrinsic_function_args (se, expr);
  arg = TREE_VALUE (TREE_CHAIN (arg));
  gcc_assert (POINTER_TYPE_P (TREE_TYPE (arg)));
  arg = build1 (NOP_EXPR, pchar_type_node, arg);
  type = gfc_typenode_for_spec (&expr->ts);

  se->expr = build_fold_indirect_ref (arg);
  se->expr = convert (type, se->expr);
}


/* MERGE (tsource, fsource, mask) = mask ? tsource : fsource.  */

static void
gfc_conv_intrinsic_merge (gfc_se * se, gfc_expr * expr)
{
  tree arg;
  tree tsource;
  tree fsource;
  tree mask;
  tree type;
  tree len;

  arg = gfc_conv_intrinsic_function_args (se, expr);
  if (expr->ts.type != BT_CHARACTER)
    {
      tsource = TREE_VALUE (arg);
      arg = TREE_CHAIN (arg);
      fsource = TREE_VALUE (arg);
      mask = TREE_VALUE (TREE_CHAIN (arg));
    }
  else
    {
      /* We do the same as in the non-character case, but the argument
	 list is different because of the string length arguments. We
	 also have to set the string length for the result.  */
      len = TREE_VALUE (arg);
      arg = TREE_CHAIN (arg);
      tsource = TREE_VALUE (arg);
      arg = TREE_CHAIN (TREE_CHAIN (arg));
      fsource = TREE_VALUE (arg);
      mask = TREE_VALUE (TREE_CHAIN (arg));

      se->string_length = len;
    }
  type = TREE_TYPE (tsource);
  se->expr = fold_build3 (COND_EXPR, type, mask, tsource, fsource);
}


static void
gfc_conv_intrinsic_size (gfc_se * se, gfc_expr * expr)
{
  gfc_actual_arglist *actual;
  tree args;
  tree type;
  tree fndecl;
  gfc_se argse;
  gfc_ss *ss;

  gfc_init_se (&argse, NULL);
  actual = expr->value.function.actual;

  ss = gfc_walk_expr (actual->expr);
  gcc_assert (ss != gfc_ss_terminator);
  argse.want_pointer = 1;
  argse.data_not_needed = 1;
  gfc_conv_expr_descriptor (&argse, actual->expr, ss);
  gfc_add_block_to_block (&se->pre, &argse.pre);
  gfc_add_block_to_block (&se->post, &argse.post);
  args = gfc_chainon_list (NULL_TREE, argse.expr);

  actual = actual->next;
  if (actual->expr)
    {
      gfc_init_se (&argse, NULL);
      gfc_conv_expr_type (&argse, actual->expr, gfc_array_index_type);
      gfc_add_block_to_block (&se->pre, &argse.pre);
      args = gfc_chainon_list (args, argse.expr);
      fndecl = gfor_fndecl_size1;
    }
  else
    fndecl = gfor_fndecl_size0;

  se->expr = build_function_call_expr (fndecl, args);
  type = gfc_typenode_for_spec (&expr->ts);
  se->expr = convert (type, se->expr);
}


/* Intrinsic string comparison functions.  */

  static void
gfc_conv_intrinsic_strcmp (gfc_se * se, gfc_expr * expr, int op)
{
  tree type;
  tree args;
  tree arg2;

  args = gfc_conv_intrinsic_function_args (se, expr);
  arg2 = TREE_CHAIN (TREE_CHAIN (args));

  se->expr = gfc_build_compare_string (TREE_VALUE (args),
		TREE_VALUE (TREE_CHAIN (args)), TREE_VALUE (arg2),
		TREE_VALUE (TREE_CHAIN (arg2)));

  type = gfc_typenode_for_spec (&expr->ts);
  se->expr = fold_build2 (op, type, se->expr,
		     build_int_cst (TREE_TYPE (se->expr), 0));
}

/* Generate a call to the adjustl/adjustr library function.  */
static void
gfc_conv_intrinsic_adjust (gfc_se * se, gfc_expr * expr, tree fndecl)
{
  tree args;
  tree len;
  tree type;
  tree var;
  tree tmp;

  args = gfc_conv_intrinsic_function_args (se, expr);
  len = TREE_VALUE (args);

  type = TREE_TYPE (TREE_VALUE (TREE_CHAIN (args)));
  var = gfc_conv_string_tmp (se, type, len);
  args = tree_cons (NULL_TREE, var, args);

  tmp = build_function_call_expr (fndecl, args);
  gfc_add_expr_to_block (&se->pre, tmp);
  se->expr = var;
  se->string_length = len;
}


/* A helper function for gfc_conv_intrinsic_array_transfer to compute
   the size of tree expressions in bytes.  */
static tree
gfc_size_in_bytes (gfc_se *se, gfc_expr *e)
{
  tree tmp;

  if (e->ts.type == BT_CHARACTER)
    tmp = se->string_length;
  else
    {
      if (e->rank)
	{
	  tmp = gfc_get_element_type (TREE_TYPE (se->expr));
	  tmp = size_in_bytes (tmp);
	}
      else
	tmp = size_in_bytes (TREE_TYPE (TREE_TYPE (se->expr)));
    }

  return fold_convert (gfc_array_index_type, tmp);
}


/* Array transfer statement.
     DEST(1:N) = TRANSFER (SOURCE, MOLD[, SIZE])
   where:
     typeof<DEST> = typeof<MOLD>
   and:
     N = min (sizeof (SOURCE(:)), sizeof (DEST(:)),
	      sizeof (DEST(0) * SIZE).  */

static void
gfc_conv_intrinsic_array_transfer (gfc_se * se, gfc_expr * expr)
{
  tree tmp;
  tree extent;
  tree source;
  tree source_bytes;
  tree dest_word_len;
  tree size_words;
  tree size_bytes;
  tree upper;
  tree lower;
  tree stride;
  tree stmt;
  tree args;
  gfc_actual_arglist *arg;
  gfc_se argse;
  gfc_ss *ss;
  gfc_ss_info *info;
  stmtblock_t block;
  int n;

  gcc_assert (se->loop);
  info = &se->ss->data.info;

  /* Convert SOURCE.  The output from this stage is:-
	source_bytes = length of the source in bytes
	source = pointer to the source data.  */
  arg = expr->value.function.actual;
  gfc_init_se (&argse, NULL);
  ss = gfc_walk_expr (arg->expr);

  source_bytes = gfc_create_var (gfc_array_index_type, NULL);

  /* Obtain the pointer to source and the length of source in bytes.  */
  if (ss == gfc_ss_terminator)
    {
      gfc_conv_expr_reference (&argse, arg->expr);
      source = argse.expr;

      /* Obtain the source word length.  */
      tmp = gfc_size_in_bytes (&argse, arg->expr);
    }
  else
    {
      gfc_init_se (&argse, NULL);
      argse.want_pointer = 0;
      gfc_conv_expr_descriptor (&argse, arg->expr, ss);
      source = gfc_conv_descriptor_data_get (argse.expr);

      /* Repack the source if not a full variable array.  */
      if (!(arg->expr->expr_type == EXPR_VARIABLE
	      && arg->expr->ref->u.ar.type == AR_FULL))
	{
	  tmp = build_fold_addr_expr (argse.expr);
	  tmp = gfc_chainon_list (NULL_TREE, tmp);
	  source = build_function_call_expr (gfor_fndecl_in_pack, tmp);
	  source = gfc_evaluate_now (source, &argse.pre);

	  /* Free the temporary.  */
	  gfc_start_block (&block);
	  tmp = convert (pvoid_type_node, source);
	  tmp = gfc_chainon_list (NULL_TREE, tmp);
	  tmp = build_function_call_expr (gfor_fndecl_internal_free, tmp);
	  gfc_add_expr_to_block (&block, tmp);
	  stmt = gfc_finish_block (&block);

	  /* Clean up if it was repacked.  */
	  gfc_init_block (&block);
	  tmp = gfc_conv_array_data (argse.expr);
	  tmp = build2 (NE_EXPR, boolean_type_node, source, tmp);
	  tmp = build3_v (COND_EXPR, tmp, stmt, build_empty_stmt ());
	  gfc_add_expr_to_block (&block, tmp);
	  gfc_add_block_to_block (&block, &se->post);
	  gfc_init_block (&se->post);
	  gfc_add_block_to_block (&se->post, &block);
	}

      /* Obtain the source word length.  */
      tmp = gfc_size_in_bytes (&argse, arg->expr);

      /* Obtain the size of the array in bytes.  */
      extent = gfc_create_var (gfc_array_index_type, NULL);
      for (n = 0; n < arg->expr->rank; n++)
	{
	  tree idx;
	  idx = gfc_rank_cst[n];
	  gfc_add_modify_expr (&argse.pre, source_bytes, tmp);
	  stride = gfc_conv_descriptor_stride (argse.expr, idx);
	  lower = gfc_conv_descriptor_lbound (argse.expr, idx);
	  upper = gfc_conv_descriptor_ubound (argse.expr, idx);
	  tmp = build2 (MINUS_EXPR, gfc_array_index_type,
			upper, lower);
	  gfc_add_modify_expr (&argse.pre, extent, tmp);
	  tmp = build2 (PLUS_EXPR, gfc_array_index_type,
			extent, gfc_index_one_node);
	  tmp = build2 (MULT_EXPR, gfc_array_index_type,
			tmp, source_bytes);
	}
    }

  gfc_add_modify_expr (&argse.pre, source_bytes, tmp);
  gfc_add_block_to_block (&se->pre, &argse.pre);
  gfc_add_block_to_block (&se->post, &argse.post);

  /* Now convert MOLD.  The sole output is:
	dest_word_len = destination word length in bytes.  */
  arg = arg->next;

  gfc_init_se (&argse, NULL);
  ss = gfc_walk_expr (arg->expr);

  if (ss == gfc_ss_terminator)
    {
      gfc_conv_expr_reference (&argse, arg->expr);

      /* Obtain the source word length.  */
      tmp = gfc_size_in_bytes (&argse, arg->expr);
    }
  else
    {
      gfc_init_se (&argse, NULL);
      argse.want_pointer = 0;
      gfc_conv_expr_descriptor (&argse, arg->expr, ss);

      /* Obtain the source word length.  */
      tmp = gfc_size_in_bytes (&argse, arg->expr);
    }

  dest_word_len = gfc_create_var (gfc_array_index_type, NULL);
  gfc_add_modify_expr (&se->pre, dest_word_len, tmp);

  /* Finally convert SIZE, if it is present.  */
  arg = arg->next;
  size_words = gfc_create_var (gfc_array_index_type, NULL);

  if (arg->expr)
    {
      gfc_init_se (&argse, NULL);
      gfc_conv_expr_reference (&argse, arg->expr);
      tmp = convert (gfc_array_index_type,
			 build_fold_indirect_ref (argse.expr));
      gfc_add_block_to_block (&se->pre, &argse.pre);
      gfc_add_block_to_block (&se->post, &argse.post);
    }
  else
    tmp = NULL_TREE;

  size_bytes = gfc_create_var (gfc_array_index_type, NULL);
  if (tmp != NULL_TREE)
    {
      tmp = build2 (MULT_EXPR, gfc_array_index_type,
		    tmp, dest_word_len);
      tmp = build2 (MIN_EXPR, gfc_array_index_type, tmp, source_bytes);
    }
  else
    tmp = source_bytes;

  gfc_add_modify_expr (&se->pre, size_bytes, tmp);
  gfc_add_modify_expr (&se->pre, size_words,
		       build2 (CEIL_DIV_EXPR, gfc_array_index_type,
			       size_bytes, dest_word_len));

  /* Evaluate the bounds of the result.  If the loop range exists, we have
     to check if it is too large.  If so, we modify loop->to be consistent
     with min(size, size(source)).  Otherwise, size is made consistent with
     the loop range, so that the right number of bytes is transferred.*/
  n = se->loop->order[0];
  if (se->loop->to[n] != NULL_TREE)
    {
      tmp = fold_build2 (MINUS_EXPR, gfc_array_index_type,
			 se->loop->to[n], se->loop->from[n]);
      tmp = build2 (PLUS_EXPR, gfc_array_index_type,
		    tmp, gfc_index_one_node);
      tmp = build2 (MIN_EXPR, gfc_array_index_type,
		    tmp, size_words);
      gfc_add_modify_expr (&se->pre, size_words, tmp);
      gfc_add_modify_expr (&se->pre, size_bytes,
			   build2 (MULT_EXPR, gfc_array_index_type,
			   size_words, dest_word_len));
      upper = build2 (PLUS_EXPR, gfc_array_index_type,
		      size_words, se->loop->from[n]);
      upper = build2 (MINUS_EXPR, gfc_array_index_type,
		      upper, gfc_index_one_node);
    }
  else
    {
      upper = build2 (MINUS_EXPR, gfc_array_index_type,
		      size_words, gfc_index_one_node);
      se->loop->from[n] = gfc_index_zero_node;
    }

  se->loop->to[n] = upper;

  /* Build a destination descriptor, using the pointer, source, as the
     data field.  This is already allocated so set callee_alloc.
     FIXME callee_alloc is not set!  */
 
  tmp = gfc_typenode_for_spec (&expr->ts);
  gfc_trans_create_temp_array (&se->pre, &se->post, se->loop,
			       info, tmp, false, true, false);

  /* Use memcpy to do the transfer.  */
  tmp = gfc_conv_descriptor_data_get (info->descriptor);
  args = gfc_chainon_list (NULL_TREE, tmp);
  tmp = fold_convert (pvoid_type_node, source);
  args = gfc_chainon_list (args, source);
  args = gfc_chainon_list (args, size_bytes);
  tmp = built_in_decls[BUILT_IN_MEMCPY];
  tmp = build_function_call_expr (tmp, args);
  gfc_add_expr_to_block (&se->pre, tmp);

  se->expr = info->descriptor;
  if (expr->ts.type == BT_CHARACTER)
    se->string_length = dest_word_len;
}


/* Scalar transfer statement.
   TRANSFER (source, mold) = memcpy(&tmpdecl, &source, size), tmpdecl.  */

static void
gfc_conv_intrinsic_transfer (gfc_se * se, gfc_expr * expr)
{
  gfc_actual_arglist *arg;
  gfc_se argse;
  tree type;
  tree ptr;
  gfc_ss *ss;
  tree tmpdecl, tmp, args;

  /* Get a pointer to the source.  */
  arg = expr->value.function.actual;
  ss = gfc_walk_expr (arg->expr);
  gfc_init_se (&argse, NULL);
  if (ss == gfc_ss_terminator)
    gfc_conv_expr_reference (&argse, arg->expr);
  else
    gfc_conv_array_parameter (&argse, arg->expr, ss, 1);
  gfc_add_block_to_block (&se->pre, &argse.pre);
  gfc_add_block_to_block (&se->post, &argse.post);
  ptr = argse.expr;

  arg = arg->next;
  type = gfc_typenode_for_spec (&expr->ts);

  if (expr->ts.type == BT_CHARACTER)
    {
      ptr = convert (build_pointer_type (type), ptr);
      gfc_init_se (&argse, NULL);
      gfc_conv_expr (&argse, arg->expr);
      gfc_add_block_to_block (&se->pre, &argse.pre);
      gfc_add_block_to_block (&se->post, &argse.post);
      se->expr = ptr;
      se->string_length = argse.string_length;
    }
  else
    {
      tree moldsize;
      tmpdecl = gfc_create_var (type, "transfer");
      moldsize = size_in_bytes (type);

      /* Use memcpy to do the transfer.  */
      tmp = build1 (ADDR_EXPR, build_pointer_type (type), tmpdecl);
      tmp = fold_convert (pvoid_type_node, tmp);
      args = gfc_chainon_list (NULL_TREE, tmp);
      tmp = fold_convert (pvoid_type_node, ptr);
      args = gfc_chainon_list (args, tmp);
      args = gfc_chainon_list (args, moldsize);
      tmp = built_in_decls[BUILT_IN_MEMCPY];
      tmp = build_function_call_expr (tmp, args);
      gfc_add_expr_to_block (&se->pre, tmp);

      se->expr = tmpdecl;
    }
}


/* Generate code for the ALLOCATED intrinsic.
   Generate inline code that directly check the address of the argument.  */

static void
gfc_conv_allocated (gfc_se *se, gfc_expr *expr)
{
  gfc_actual_arglist *arg1;
  gfc_se arg1se;
  gfc_ss *ss1;
  tree tmp;

  gfc_init_se (&arg1se, NULL);
  arg1 = expr->value.function.actual;
  ss1 = gfc_walk_expr (arg1->expr);
  arg1se.descriptor_only = 1;
  gfc_conv_expr_descriptor (&arg1se, arg1->expr, ss1);

  tmp = gfc_conv_descriptor_data_get (arg1se.expr);
  tmp = build2 (NE_EXPR, boolean_type_node, tmp,
		fold_convert (TREE_TYPE (tmp), null_pointer_node));
  se->expr = convert (gfc_typenode_for_spec (&expr->ts), tmp);
}


/* Generate code for the ASSOCIATED intrinsic.
   If both POINTER and TARGET are arrays, generate a call to library function
   _gfor_associated, and pass descriptors of POINTER and TARGET to it.
   In other cases, generate inline code that directly compare the address of
   POINTER with the address of TARGET.  */

static void
gfc_conv_associated (gfc_se *se, gfc_expr *expr)
{
  gfc_actual_arglist *arg1;
  gfc_actual_arglist *arg2;
  gfc_se arg1se;
  gfc_se arg2se;
  tree tmp2;
  tree tmp;
  tree args, fndecl;
  tree nonzero_charlen;
  tree nonzero_arraylen;
  gfc_ss *ss1, *ss2;

  gfc_init_se (&arg1se, NULL);
  gfc_init_se (&arg2se, NULL);
  arg1 = expr->value.function.actual;
  arg2 = arg1->next;
  ss1 = gfc_walk_expr (arg1->expr);

  if (!arg2->expr)
    {
      /* No optional target.  */
      if (ss1 == gfc_ss_terminator)
        {
          /* A pointer to a scalar.  */
          arg1se.want_pointer = 1;
          gfc_conv_expr (&arg1se, arg1->expr);
          tmp2 = arg1se.expr;
        }
      else
        {
          /* A pointer to an array.  */
          gfc_conv_expr_descriptor (&arg1se, arg1->expr, ss1);
          tmp2 = gfc_conv_descriptor_data_get (arg1se.expr);
        }
      gfc_add_block_to_block (&se->pre, &arg1se.pre);
      gfc_add_block_to_block (&se->post, &arg1se.post);
      tmp = build2 (NE_EXPR, boolean_type_node, tmp2,
		    fold_convert (TREE_TYPE (tmp2), null_pointer_node));
      se->expr = tmp;
    }
  else
    {
      /* An optional target.  */
      ss2 = gfc_walk_expr (arg2->expr);

      nonzero_charlen = NULL_TREE;
      if (arg1->expr->ts.type == BT_CHARACTER)
	nonzero_charlen = build2 (NE_EXPR, boolean_type_node,
				  arg1->expr->ts.cl->backend_decl,
				  integer_zero_node);

      if (ss1 == gfc_ss_terminator)
        {
          /* A pointer to a scalar.  */
          gcc_assert (ss2 == gfc_ss_terminator);
          arg1se.want_pointer = 1;
          gfc_conv_expr (&arg1se, arg1->expr);
          arg2se.want_pointer = 1;
          gfc_conv_expr (&arg2se, arg2->expr);
	  gfc_add_block_to_block (&se->pre, &arg1se.pre);
	  gfc_add_block_to_block (&se->post, &arg1se.post);
          tmp = build2 (EQ_EXPR, boolean_type_node, arg1se.expr, arg2se.expr);
          tmp2 = build2 (NE_EXPR, boolean_type_node, arg1se.expr,
                         null_pointer_node);
          se->expr = build2 (TRUTH_AND_EXPR, boolean_type_node, tmp, tmp2);
        }
      else
        {

	  /* An array pointer of zero length is not associated if target is
	     present.  */
	  arg1se.descriptor_only = 1;
	  gfc_conv_expr_lhs (&arg1se, arg1->expr);
	  tmp = gfc_conv_descriptor_stride (arg1se.expr,
					    gfc_rank_cst[arg1->expr->rank - 1]);
	  nonzero_arraylen = build2 (NE_EXPR, boolean_type_node,
				 tmp, integer_zero_node);

          /* A pointer to an array, call library function _gfor_associated.  */
          gcc_assert (ss2 != gfc_ss_terminator);
          args = NULL_TREE;
          arg1se.want_pointer = 1;
          gfc_conv_expr_descriptor (&arg1se, arg1->expr, ss1);
          args = gfc_chainon_list (args, arg1se.expr);

          arg2se.want_pointer = 1;
          gfc_conv_expr_descriptor (&arg2se, arg2->expr, ss2);
          gfc_add_block_to_block (&se->pre, &arg2se.pre);
          gfc_add_block_to_block (&se->post, &arg2se.post);
          args = gfc_chainon_list (args, arg2se.expr);
          fndecl = gfor_fndecl_associated;
          se->expr = build_function_call_expr (fndecl, args);
	  se->expr = build2 (TRUTH_AND_EXPR, boolean_type_node,
			     se->expr, nonzero_arraylen);

        }

      /* If target is present zero character length pointers cannot
	 be associated.  */
      if (nonzero_charlen != NULL_TREE)
	se->expr = build2 (TRUTH_AND_EXPR, boolean_type_node,
			   se->expr, nonzero_charlen);
    }

  se->expr = convert (gfc_typenode_for_spec (&expr->ts), se->expr);
}


/* Scan a string for any one of the characters in a set of characters.  */

static void
gfc_conv_intrinsic_scan (gfc_se * se, gfc_expr * expr)
{
  tree logical4_type_node = gfc_get_logical_type (4);
  tree args;
  tree back;
  tree type;
  tree tmp;

  args = gfc_conv_intrinsic_function_args (se, expr);
  type = gfc_typenode_for_spec (&expr->ts);
  tmp = gfc_advance_chain (args, 3);
  if (TREE_CHAIN (tmp) == NULL_TREE)
    {
      back = tree_cons (NULL_TREE, build_int_cst (logical4_type_node, 0),
			NULL_TREE);
      TREE_CHAIN (tmp) = back;
    }
  else
    {
      back = TREE_CHAIN (tmp);
      TREE_VALUE (back) = convert (logical4_type_node, TREE_VALUE (back));
    }

  se->expr = build_function_call_expr (gfor_fndecl_string_scan, args);
  se->expr = convert (type, se->expr);
}


/* Verify that a set of characters contains all the characters in a string
   by identifying the position of the first character in a string of
   characters that does not appear in a given set of characters.  */

static void
gfc_conv_intrinsic_verify (gfc_se * se, gfc_expr * expr)
{
  tree logical4_type_node = gfc_get_logical_type (4);
  tree args;
  tree back;
  tree type;
  tree tmp;

  args = gfc_conv_intrinsic_function_args (se, expr);
  type = gfc_typenode_for_spec (&expr->ts);
  tmp = gfc_advance_chain (args, 3);
  if (TREE_CHAIN (tmp) == NULL_TREE)
    {
      back = tree_cons (NULL_TREE, build_int_cst (logical4_type_node, 0),
			NULL_TREE);
      TREE_CHAIN (tmp) = back;
    }
  else
    {
      back = TREE_CHAIN (tmp);
      TREE_VALUE (back) = convert (logical4_type_node, TREE_VALUE (back));
    }

  se->expr = build_function_call_expr (gfor_fndecl_string_verify, args);
  se->expr = convert (type, se->expr);
}


/* Generate code for SELECTED_INT_KIND (R) intrinsic function.  */

static void
gfc_conv_intrinsic_si_kind (gfc_se * se, gfc_expr * expr)
{
  tree args;

  args = gfc_conv_intrinsic_function_args (se, expr);
  args = TREE_VALUE (args);
  args = build_fold_addr_expr (args);
  args = tree_cons (NULL_TREE, args, NULL_TREE);
  se->expr = build_function_call_expr (gfor_fndecl_si_kind, args);
}

/* Generate code for SELECTED_REAL_KIND (P, R) intrinsic function.  */

static void
gfc_conv_intrinsic_sr_kind (gfc_se * se, gfc_expr * expr)
{
  gfc_actual_arglist *actual;
  tree args;
  gfc_se argse;

  args = NULL_TREE;
  for (actual = expr->value.function.actual; actual; actual = actual->next)
    {
      gfc_init_se (&argse, se);

      /* Pass a NULL pointer for an absent arg.  */
      if (actual->expr == NULL)
        argse.expr = null_pointer_node;
      else
        gfc_conv_expr_reference (&argse, actual->expr);

      gfc_add_block_to_block (&se->pre, &argse.pre);
      gfc_add_block_to_block (&se->post, &argse.post);
      args = gfc_chainon_list (args, argse.expr);
    }
  se->expr = build_function_call_expr (gfor_fndecl_sr_kind, args);
}


/* Generate code for TRIM (A) intrinsic function.  */

static void
gfc_conv_intrinsic_trim (gfc_se * se, gfc_expr * expr)
{
  tree gfc_int4_type_node = gfc_get_int_type (4);
  tree var;
  tree len;
  tree addr;
  tree tmp;
  tree arglist;
  tree type;
  tree cond;

  arglist = NULL_TREE;

  type = build_pointer_type (gfc_character1_type_node);
  var = gfc_create_var (type, "pstr");
  addr = gfc_build_addr_expr (ppvoid_type_node, var);
  len = gfc_create_var (gfc_int4_type_node, "len");

  tmp = gfc_conv_intrinsic_function_args (se, expr);
  arglist = gfc_chainon_list (arglist, build_fold_addr_expr (len));
  arglist = gfc_chainon_list (arglist, addr);
  arglist = chainon (arglist, tmp);

  tmp = build_function_call_expr (gfor_fndecl_string_trim, arglist);
  gfc_add_expr_to_block (&se->pre, tmp);

  /* Free the temporary afterwards, if necessary.  */
  cond = build2 (GT_EXPR, boolean_type_node, len,
		 build_int_cst (TREE_TYPE (len), 0));
  arglist = gfc_chainon_list (NULL_TREE, var);
  tmp = build_function_call_expr (gfor_fndecl_internal_free, arglist);
  tmp = build3_v (COND_EXPR, cond, tmp, build_empty_stmt ());
  gfc_add_expr_to_block (&se->post, tmp);

  se->expr = var;
  se->string_length = len;
}


/* Generate code for REPEAT (STRING, NCOPIES) intrinsic function.  */

static void
gfc_conv_intrinsic_repeat (gfc_se * se, gfc_expr * expr)
{
  tree gfc_int4_type_node = gfc_get_int_type (4);
  tree tmp;
  tree len;
  tree args;
  tree arglist;
  tree ncopies;
  tree var;
  tree type;
  tree cond;

  args = gfc_conv_intrinsic_function_args (se, expr);
  len = TREE_VALUE (args);
  tmp = gfc_advance_chain (args, 2);
  ncopies = TREE_VALUE (tmp);

  /* Check that ncopies is not negative.  */
  ncopies = gfc_evaluate_now (ncopies, &se->pre);
  cond = fold_build2 (LT_EXPR, boolean_type_node, ncopies,
		      build_int_cst (TREE_TYPE (ncopies), 0));
  gfc_trans_runtime_check (cond,
			   "Argument NCOPIES of REPEAT intrinsic is negative",
			   &se->pre, &expr->where);

  /* Compute the destination length.  */
  len = fold_build2 (MULT_EXPR, gfc_int4_type_node, len, ncopies);
  type = gfc_get_character_type (expr->ts.kind, expr->ts.cl);
  var = gfc_conv_string_tmp (se, build_pointer_type (type), len);

  /* Create the argument list and generate the function call.  */
  arglist = NULL_TREE;
  arglist = gfc_chainon_list (arglist, var);
  arglist = gfc_chainon_list (arglist, TREE_VALUE (args));
  arglist = gfc_chainon_list (arglist, TREE_VALUE (TREE_CHAIN (args)));
  arglist = gfc_chainon_list (arglist, ncopies);
  tmp = build_function_call_expr (gfor_fndecl_string_repeat, arglist);
  gfc_add_expr_to_block (&se->pre, tmp);

  se->expr = var;
  se->string_length = len;
}


/* Generate code for the IARGC intrinsic.  */

static void
gfc_conv_intrinsic_iargc (gfc_se * se, gfc_expr * expr)
{
  tree tmp;
  tree fndecl;
  tree type;

  /* Call the library function.  This always returns an INTEGER(4).  */
  fndecl = gfor_fndecl_iargc;
  tmp = build_function_call_expr (fndecl, NULL_TREE);

  /* Convert it to the required type.  */
  type = gfc_typenode_for_spec (&expr->ts);
  tmp = fold_convert (type, tmp);

  se->expr = tmp;
}


/* The loc intrinsic returns the address of its argument as
   gfc_index_integer_kind integer.  */

static void
gfc_conv_intrinsic_loc(gfc_se * se, gfc_expr * expr)
{
  tree temp_var;
  gfc_expr *arg_expr;
  gfc_ss *ss;

  gcc_assert (!se->ss);

  arg_expr = expr->value.function.actual->expr;
  ss = gfc_walk_expr (arg_expr);
  if (ss == gfc_ss_terminator)
    gfc_conv_expr_reference (se, arg_expr);
  else
    gfc_conv_array_parameter (se, arg_expr, ss, 1); 
  se->expr= convert (gfc_unsigned_type (long_integer_type_node), 
		     se->expr);
   
  /* Create a temporary variable for loc return value.  Without this, 
     we get an error an ICE in gcc/expr.c(expand_expr_addr_expr_1).  */
  temp_var = gfc_create_var (gfc_unsigned_type (long_integer_type_node), 
			     NULL);
  gfc_add_modify_expr (&se->pre, temp_var, se->expr);
  se->expr = temp_var;
}

/* Generate code for an intrinsic function.  Some map directly to library
   calls, others get special handling.  In some cases the name of the function
   used depends on the type specifiers.  */

void
gfc_conv_intrinsic_function (gfc_se * se, gfc_expr * expr)
{
  gfc_intrinsic_sym *isym;
  const char *name;
  int lib;

  isym = expr->value.function.isym;

  name = &expr->value.function.name[2];

  if (expr->rank > 0 && !expr->inline_noncopying_intrinsic)
    {
      lib = gfc_is_intrinsic_libcall (expr);
      if (lib != 0)
	{
	  if (lib == 1)
	    se->ignore_optional = 1;
	  gfc_conv_intrinsic_funcall (se, expr);
	  return;
	}
    }

  switch (expr->value.function.isym->generic_id)
    {
    case GFC_ISYM_NONE:
      gcc_unreachable ();

    case GFC_ISYM_REPEAT:
      gfc_conv_intrinsic_repeat (se, expr);
      break;

    case GFC_ISYM_TRIM:
      gfc_conv_intrinsic_trim (se, expr);
      break;

    case GFC_ISYM_SI_KIND:
      gfc_conv_intrinsic_si_kind (se, expr);
      break;

    case GFC_ISYM_SR_KIND:
      gfc_conv_intrinsic_sr_kind (se, expr);
      break;

    case GFC_ISYM_EXPONENT:
      gfc_conv_intrinsic_exponent (se, expr);
      break;

    case GFC_ISYM_SCAN:
      gfc_conv_intrinsic_scan (se, expr);
      break;

    case GFC_ISYM_VERIFY:
      gfc_conv_intrinsic_verify (se, expr);
      break;

    case GFC_ISYM_ALLOCATED:
      gfc_conv_allocated (se, expr);
      break;

    case GFC_ISYM_ASSOCIATED:
      gfc_conv_associated(se, expr);
      break;

    case GFC_ISYM_ABS:
      gfc_conv_intrinsic_abs (se, expr);
      break;

    case GFC_ISYM_ADJUSTL:
      gfc_conv_intrinsic_adjust (se, expr, gfor_fndecl_adjustl);
      break;

    case GFC_ISYM_ADJUSTR:
      gfc_conv_intrinsic_adjust (se, expr, gfor_fndecl_adjustr);
      break;

    case GFC_ISYM_AIMAG:
      gfc_conv_intrinsic_imagpart (se, expr);
      break;

    case GFC_ISYM_AINT:
      gfc_conv_intrinsic_aint (se, expr, FIX_TRUNC_EXPR);
      break;

    case GFC_ISYM_ALL:
      gfc_conv_intrinsic_anyall (se, expr, EQ_EXPR);
      break;

    case GFC_ISYM_ANINT:
      gfc_conv_intrinsic_aint (se, expr, FIX_ROUND_EXPR);
      break;

    case GFC_ISYM_AND:
      gfc_conv_intrinsic_bitop (se, expr, BIT_AND_EXPR);
      break;

    case GFC_ISYM_ANY:
      gfc_conv_intrinsic_anyall (se, expr, NE_EXPR);
      break;

    case GFC_ISYM_BTEST:
      gfc_conv_intrinsic_btest (se, expr);
      break;

    case GFC_ISYM_ACHAR:
    case GFC_ISYM_CHAR:
      gfc_conv_intrinsic_char (se, expr);
      break;

    case GFC_ISYM_CONVERSION:
    case GFC_ISYM_REAL:
    case GFC_ISYM_LOGICAL:
    case GFC_ISYM_DBLE:
      gfc_conv_intrinsic_conversion (se, expr);
      break;

      /* Integer conversions are handled separately to make sure we get the
         correct rounding mode.  */
    case GFC_ISYM_INT:
    case GFC_ISYM_INT2:
    case GFC_ISYM_INT8:
    case GFC_ISYM_LONG:
      gfc_conv_intrinsic_int (se, expr, FIX_TRUNC_EXPR);
      break;

    case GFC_ISYM_NINT:
      gfc_conv_intrinsic_int (se, expr, FIX_ROUND_EXPR);
      break;

    case GFC_ISYM_CEILING:
      gfc_conv_intrinsic_int (se, expr, FIX_CEIL_EXPR);
      break;

    case GFC_ISYM_FLOOR:
      gfc_conv_intrinsic_int (se, expr, FIX_FLOOR_EXPR);
      break;

    case GFC_ISYM_MOD:
      gfc_conv_intrinsic_mod (se, expr, 0);
      break;

    case GFC_ISYM_MODULO:
      gfc_conv_intrinsic_mod (se, expr, 1);
      break;

    case GFC_ISYM_CMPLX:
      gfc_conv_intrinsic_cmplx (se, expr, name[5] == '1');
      break;

    case GFC_ISYM_COMMAND_ARGUMENT_COUNT:
      gfc_conv_intrinsic_iargc (se, expr);
      break;

    case GFC_ISYM_COMPLEX:
      gfc_conv_intrinsic_cmplx (se, expr, 1);
      break;

    case GFC_ISYM_CONJG:
      gfc_conv_intrinsic_conjg (se, expr);
      break;

    case GFC_ISYM_COUNT:
      gfc_conv_intrinsic_count (se, expr);
      break;

    case GFC_ISYM_CTIME:
      gfc_conv_intrinsic_ctime (se, expr);
      break;

    case GFC_ISYM_DIM:
      gfc_conv_intrinsic_dim (se, expr);
      break;

    case GFC_ISYM_DOT_PRODUCT:
      gfc_conv_intrinsic_dot_product (se, expr);
      break;

    case GFC_ISYM_DPROD:
      gfc_conv_intrinsic_dprod (se, expr);
      break;

    case GFC_ISYM_FDATE:
      gfc_conv_intrinsic_fdate (se, expr);
      break;

    case GFC_ISYM_IAND:
      gfc_conv_intrinsic_bitop (se, expr, BIT_AND_EXPR);
      break;

    case GFC_ISYM_IBCLR:
      gfc_conv_intrinsic_singlebitop (se, expr, 0);
      break;

    case GFC_ISYM_IBITS:
      gfc_conv_intrinsic_ibits (se, expr);
      break;

    case GFC_ISYM_IBSET:
      gfc_conv_intrinsic_singlebitop (se, expr, 1);
      break;

    case GFC_ISYM_IACHAR:
    case GFC_ISYM_ICHAR:
      /* We assume ASCII character sequence.  */
      gfc_conv_intrinsic_ichar (se, expr);
      break;

    case GFC_ISYM_IARGC:
      gfc_conv_intrinsic_iargc (se, expr);
      break;

    case GFC_ISYM_IEOR:
      gfc_conv_intrinsic_bitop (se, expr, BIT_XOR_EXPR);
      break;

    case GFC_ISYM_INDEX:
      gfc_conv_intrinsic_index (se, expr);
      break;

    case GFC_ISYM_IOR:
      gfc_conv_intrinsic_bitop (se, expr, BIT_IOR_EXPR);
      break;

    case GFC_ISYM_LSHIFT:
      gfc_conv_intrinsic_rlshift (se, expr, 0);
      break;

    case GFC_ISYM_RSHIFT:
      gfc_conv_intrinsic_rlshift (se, expr, 1);
      break;

    case GFC_ISYM_ISHFT:
      gfc_conv_intrinsic_ishft (se, expr);
      break;

    case GFC_ISYM_ISHFTC:
      gfc_conv_intrinsic_ishftc (se, expr);
      break;

    case GFC_ISYM_LBOUND:
      gfc_conv_intrinsic_bound (se, expr, 0);
      break;

    case GFC_ISYM_TRANSPOSE:
      if (se->ss && se->ss->useflags)
	{
	  gfc_conv_tmp_array_ref (se);
	  gfc_advance_se_ss_chain (se);
	}
      else
	gfc_conv_array_transpose (se, expr->value.function.actual->expr);
      break;

    case GFC_ISYM_LEN:
      gfc_conv_intrinsic_len (se, expr);
      break;

    case GFC_ISYM_LEN_TRIM:
      gfc_conv_intrinsic_len_trim (se, expr);
      break;

    case GFC_ISYM_LGE:
      gfc_conv_intrinsic_strcmp (se, expr, GE_EXPR);
      break;

    case GFC_ISYM_LGT:
      gfc_conv_intrinsic_strcmp (se, expr, GT_EXPR);
      break;

    case GFC_ISYM_LLE:
      gfc_conv_intrinsic_strcmp (se, expr, LE_EXPR);
      break;

    case GFC_ISYM_LLT:
      gfc_conv_intrinsic_strcmp (se, expr, LT_EXPR);
      break;

    case GFC_ISYM_MAX:
      gfc_conv_intrinsic_minmax (se, expr, GT_EXPR);
      break;

    case GFC_ISYM_MAXLOC:
      gfc_conv_intrinsic_minmaxloc (se, expr, GT_EXPR);
      break;

    case GFC_ISYM_MAXVAL:
      gfc_conv_intrinsic_minmaxval (se, expr, GT_EXPR);
      break;

    case GFC_ISYM_MERGE:
      gfc_conv_intrinsic_merge (se, expr);
      break;

    case GFC_ISYM_MIN:
      gfc_conv_intrinsic_minmax (se, expr, LT_EXPR);
      break;

    case GFC_ISYM_MINLOC:
      gfc_conv_intrinsic_minmaxloc (se, expr, LT_EXPR);
      break;

    case GFC_ISYM_MINVAL:
      gfc_conv_intrinsic_minmaxval (se, expr, LT_EXPR);
      break;

    case GFC_ISYM_NOT:
      gfc_conv_intrinsic_not (se, expr);
      break;

    case GFC_ISYM_OR:
      gfc_conv_intrinsic_bitop (se, expr, BIT_IOR_EXPR);
      break;

    case GFC_ISYM_PRESENT:
      gfc_conv_intrinsic_present (se, expr);
      break;

    case GFC_ISYM_PRODUCT:
      gfc_conv_intrinsic_arith (se, expr, MULT_EXPR);
      break;

    case GFC_ISYM_SIGN:
      gfc_conv_intrinsic_sign (se, expr);
      break;

    case GFC_ISYM_SIZE:
      gfc_conv_intrinsic_size (se, expr);
      break;

    case GFC_ISYM_SUM:
      gfc_conv_intrinsic_arith (se, expr, PLUS_EXPR);
      break;

    case GFC_ISYM_TRANSFER:
      if (se->ss)
	{
	  if (se->ss->useflags)
	    {
	      /* Access the previously obtained result.  */
	      gfc_conv_tmp_array_ref (se);
	      gfc_advance_se_ss_chain (se);
	      break;
	    }
	  else
	    gfc_conv_intrinsic_array_transfer (se, expr);
	}
      else
	gfc_conv_intrinsic_transfer (se, expr);
      break;

    case GFC_ISYM_TTYNAM:
      gfc_conv_intrinsic_ttynam (se, expr);
      break;

    case GFC_ISYM_UBOUND:
      gfc_conv_intrinsic_bound (se, expr, 1);
      break;

    case GFC_ISYM_XOR:
      gfc_conv_intrinsic_bitop (se, expr, BIT_XOR_EXPR);
      break;

    case GFC_ISYM_LOC:
      gfc_conv_intrinsic_loc (se, expr);
      break;

    case GFC_ISYM_ACCESS:
    case GFC_ISYM_CHDIR:
    case GFC_ISYM_CHMOD:
    case GFC_ISYM_ETIME:
    case GFC_ISYM_FGET:
    case GFC_ISYM_FGETC:
    case GFC_ISYM_FNUM:
    case GFC_ISYM_FPUT:
    case GFC_ISYM_FPUTC:
    case GFC_ISYM_FSTAT:
    case GFC_ISYM_FTELL:
    case GFC_ISYM_GETCWD:
    case GFC_ISYM_GETGID:
    case GFC_ISYM_GETPID:
    case GFC_ISYM_GETUID:
    case GFC_ISYM_HOSTNM:
    case GFC_ISYM_KILL:
    case GFC_ISYM_IERRNO:
    case GFC_ISYM_IRAND:
    case GFC_ISYM_ISATTY:
    case GFC_ISYM_LINK:
    case GFC_ISYM_LSTAT:
    case GFC_ISYM_MALLOC:
    case GFC_ISYM_MATMUL:
    case GFC_ISYM_MCLOCK:
    case GFC_ISYM_MCLOCK8:
    case GFC_ISYM_RAND:
    case GFC_ISYM_RENAME:
    case GFC_ISYM_SECOND:
    case GFC_ISYM_SECNDS:
    case GFC_ISYM_SIGNAL:
    case GFC_ISYM_STAT:
    case GFC_ISYM_SYMLNK:
    case GFC_ISYM_SYSTEM:
    case GFC_ISYM_TIME:
    case GFC_ISYM_TIME8:
    case GFC_ISYM_UMASK:
    case GFC_ISYM_UNLINK:
      gfc_conv_intrinsic_funcall (se, expr);
      break;

    default:
      gfc_conv_intrinsic_lib_function (se, expr);
      break;
    }
}


/* This generates code to execute before entering the scalarization loop.
   Currently does nothing.  */

void
gfc_add_intrinsic_ss_code (gfc_loopinfo * loop ATTRIBUTE_UNUSED, gfc_ss * ss)
{
  switch (ss->expr->value.function.isym->generic_id)
    {
    case GFC_ISYM_UBOUND:
    case GFC_ISYM_LBOUND:
      break;

    default:
      gcc_unreachable ();
    }
}


/* UBOUND and LBOUND intrinsics with one parameter are expanded into code
   inside the scalarization loop.  */

static gfc_ss *
gfc_walk_intrinsic_bound (gfc_ss * ss, gfc_expr * expr)
{
  gfc_ss *newss;

  /* The two argument version returns a scalar.  */
  if (expr->value.function.actual->next->expr)
    return ss;

  newss = gfc_get_ss ();
  newss->type = GFC_SS_INTRINSIC;
  newss->expr = expr;
  newss->next = ss;
  newss->data.info.dimen = 1;

  return newss;
}


/* Walk an intrinsic array libcall.  */

static gfc_ss *
gfc_walk_intrinsic_libfunc (gfc_ss * ss, gfc_expr * expr)
{
  gfc_ss *newss;

  gcc_assert (expr->rank > 0);

  newss = gfc_get_ss ();
  newss->type = GFC_SS_FUNCTION;
  newss->expr = expr;
  newss->next = ss;
  newss->data.info.dimen = expr->rank;

  return newss;
}


/* Returns nonzero if the specified intrinsic function call maps directly to a
   an external library call.  Should only be used for functions that return
   arrays.  */

int
gfc_is_intrinsic_libcall (gfc_expr * expr)
{
  gcc_assert (expr->expr_type == EXPR_FUNCTION && expr->value.function.isym);
  gcc_assert (expr->rank > 0);

  switch (expr->value.function.isym->generic_id)
    {
    case GFC_ISYM_ALL:
    case GFC_ISYM_ANY:
    case GFC_ISYM_COUNT:
    case GFC_ISYM_MATMUL:
    case GFC_ISYM_MAXLOC:
    case GFC_ISYM_MAXVAL:
    case GFC_ISYM_MINLOC:
    case GFC_ISYM_MINVAL:
    case GFC_ISYM_PRODUCT:
    case GFC_ISYM_SUM:
    case GFC_ISYM_SHAPE:
    case GFC_ISYM_SPREAD:
    case GFC_ISYM_TRANSPOSE:
      /* Ignore absent optional parameters.  */
      return 1;

    case GFC_ISYM_RESHAPE:
    case GFC_ISYM_CSHIFT:
    case GFC_ISYM_EOSHIFT:
    case GFC_ISYM_PACK:
    case GFC_ISYM_UNPACK:
      /* Pass absent optional parameters.  */
      return 2;

    default:
      return 0;
    }
}

/* Walk an intrinsic function.  */
gfc_ss *
gfc_walk_intrinsic_function (gfc_ss * ss, gfc_expr * expr,
			     gfc_intrinsic_sym * isym)
{
  gcc_assert (isym);

  if (isym->elemental)
    return gfc_walk_elemental_function_args (ss, expr->value.function.actual, GFC_SS_SCALAR);

  if (expr->rank == 0)
    return ss;

  if (gfc_is_intrinsic_libcall (expr))
    return gfc_walk_intrinsic_libfunc (ss, expr);

  /* Special cases.  */
  switch (isym->generic_id)
    {
    case GFC_ISYM_LBOUND:
    case GFC_ISYM_UBOUND:
      return gfc_walk_intrinsic_bound (ss, expr);

    case GFC_ISYM_TRANSFER:
      return gfc_walk_intrinsic_libfunc (ss, expr);

    default:
      /* This probably meant someone forgot to add an intrinsic to the above
         list(s) when they implemented it, or something's gone horribly wrong.
       */
      gfc_todo_error ("Scalarization of non-elemental intrinsic: %s",
		      expr->value.function.name);
    }
}

#include "gt-fortran-trans-intrinsic.h"

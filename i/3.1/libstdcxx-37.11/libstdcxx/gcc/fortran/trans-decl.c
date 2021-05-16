/* Backend function setup
   Copyright (C) 2002, 2003, 2004, 2005, 2006 Free Software Foundation,
   Inc.
   Contributed by Paul Brook

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

/* trans-decl.c -- Handling of backend function and variable decls, etc */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tree.h"
#include "tree-dump.h"
#include "tree-gimple.h"
#include "ggc.h"
#include "toplev.h"
#include "tm.h"
#include "rtl.h"
#include "target.h"
#include "function.h"
#include "flags.h"
#include "cgraph.h"
#include "gfortran.h"
#include "trans.h"
#include "trans-types.h"
#include "trans-array.h"
#include "trans-const.h"
/* Only for gfc_trans_code.  Shouldn't need to include this.  */
#include "trans-stmt.h"

#define MAX_LABEL_VALUE 99999


/* Holds the result of the function if no result variable specified.  */

static GTY(()) tree current_fake_result_decl;
static GTY(()) tree parent_fake_result_decl;

static GTY(()) tree current_function_return_label;


/* Holds the variable DECLs for the current function.  */

static GTY(()) tree saved_function_decls;
static GTY(()) tree saved_parent_function_decls;


/* The namespace of the module we're currently generating.  Only used while
   outputting decls for module variables.  Do not rely on this being set.  */

static gfc_namespace *module_namespace;


/* List of static constructor functions.  */

tree gfc_static_ctors;


/* Function declarations for builtin library functions.  */

tree gfor_fndecl_internal_malloc;
tree gfor_fndecl_internal_malloc64;
tree gfor_fndecl_internal_realloc;
tree gfor_fndecl_internal_realloc64;
tree gfor_fndecl_internal_free;
tree gfor_fndecl_allocate;
tree gfor_fndecl_allocate64;
tree gfor_fndecl_allocate_array;
tree gfor_fndecl_allocate64_array;
tree gfor_fndecl_deallocate;
tree gfor_fndecl_pause_numeric;
tree gfor_fndecl_pause_string;
tree gfor_fndecl_stop_numeric;
tree gfor_fndecl_stop_string;
tree gfor_fndecl_select_string;
tree gfor_fndecl_runtime_error;
tree gfor_fndecl_set_fpe;
tree gfor_fndecl_set_std;
tree gfor_fndecl_set_convert;
tree gfor_fndecl_set_record_marker;
tree gfor_fndecl_set_max_subrecord_length;
tree gfor_fndecl_ctime;
tree gfor_fndecl_fdate;
tree gfor_fndecl_ttynam;
tree gfor_fndecl_in_pack;
tree gfor_fndecl_in_unpack;
tree gfor_fndecl_associated;


/* Math functions.  Many other math functions are handled in
   trans-intrinsic.c.  */

gfc_powdecl_list gfor_fndecl_math_powi[4][3];
tree gfor_fndecl_math_cpowf;
tree gfor_fndecl_math_cpow;
tree gfor_fndecl_math_cpowl10;
tree gfor_fndecl_math_cpowl16;
tree gfor_fndecl_math_ishftc4;
tree gfor_fndecl_math_ishftc8;
tree gfor_fndecl_math_ishftc16;
tree gfor_fndecl_math_exponent4;
tree gfor_fndecl_math_exponent8;
tree gfor_fndecl_math_exponent10;
tree gfor_fndecl_math_exponent16;


/* String functions.  */

tree gfor_fndecl_compare_string;
tree gfor_fndecl_concat_string;
tree gfor_fndecl_string_len_trim;
tree gfor_fndecl_string_index;
tree gfor_fndecl_string_scan;
tree gfor_fndecl_string_verify;
tree gfor_fndecl_string_trim;
tree gfor_fndecl_string_repeat;
tree gfor_fndecl_adjustl;
tree gfor_fndecl_adjustr;


/* Other misc. runtime library functions.  */

tree gfor_fndecl_size0;
tree gfor_fndecl_size1;
tree gfor_fndecl_iargc;

/* Intrinsic functions implemented in FORTRAN.  */
tree gfor_fndecl_si_kind;
tree gfor_fndecl_sr_kind;


static void
gfc_add_decl_to_parent_function (tree decl)
{
  gcc_assert (decl);
  DECL_CONTEXT (decl) = DECL_CONTEXT (current_function_decl);
  DECL_NONLOCAL (decl) = 1;
  TREE_CHAIN (decl) = saved_parent_function_decls;
  saved_parent_function_decls = decl;
}

void
gfc_add_decl_to_function (tree decl)
{
  gcc_assert (decl);
  TREE_USED (decl) = 1;
  DECL_CONTEXT (decl) = current_function_decl;
  TREE_CHAIN (decl) = saved_function_decls;
  saved_function_decls = decl;
}


/* Build a  backend label declaration.  Set TREE_USED for named labels.
   The context of the label is always the current_function_decl.  All
   labels are marked artificial.  */

tree
gfc_build_label_decl (tree label_id)
{
  /* 2^32 temporaries should be enough.  */
  static unsigned int tmp_num = 1;
  tree label_decl;
  char *label_name;

  if (label_id == NULL_TREE)
    {
      /* Build an internal label name.  */
      ASM_FORMAT_PRIVATE_NAME (label_name, "L", tmp_num++);
      label_id = get_identifier (label_name);
    }
  else
    label_name = NULL;

  /* Build the LABEL_DECL node. Labels have no type.  */
  label_decl = build_decl (LABEL_DECL, label_id, void_type_node);
  DECL_CONTEXT (label_decl) = current_function_decl;
  DECL_MODE (label_decl) = VOIDmode;

  /* We always define the label as used, even if the original source
     file never references the label.  We don't want all kinds of
     spurious warnings for old-style Fortran code with too many
     labels.  */
  TREE_USED (label_decl) = 1;

  DECL_ARTIFICIAL (label_decl) = 1;
  return label_decl;
}


/* Returns the return label for the current function.  */

tree
gfc_get_return_label (void)
{
  char name[GFC_MAX_SYMBOL_LEN + 10];

  if (current_function_return_label)
    return current_function_return_label;

  sprintf (name, "__return_%s",
	   IDENTIFIER_POINTER (DECL_NAME (current_function_decl)));

  current_function_return_label =
    gfc_build_label_decl (get_identifier (name));

  DECL_ARTIFICIAL (current_function_return_label) = 1;

  return current_function_return_label;
}


/* Set the backend source location of a decl.  */

void
gfc_set_decl_location (tree decl, locus * loc)
{
#ifdef USE_MAPPED_LOCATION
  DECL_SOURCE_LOCATION (decl) = loc->lb->location;
#else
  DECL_SOURCE_LINE (decl) = loc->lb->linenum;
  DECL_SOURCE_FILE (decl) = loc->lb->file->filename;
#endif
}


/* Return the backend label declaration for a given label structure,
   or create it if it doesn't exist yet.  */

tree
gfc_get_label_decl (gfc_st_label * lp)
{
  if (lp->backend_decl)
    return lp->backend_decl;
  else
    {
      char label_name[GFC_MAX_SYMBOL_LEN + 1];
      tree label_decl;

      /* Validate the label declaration from the front end.  */
      gcc_assert (lp != NULL && lp->value <= MAX_LABEL_VALUE);

      /* Build a mangled name for the label.  */
      sprintf (label_name, "__label_%.6d", lp->value);

      /* Build the LABEL_DECL node.  */
      label_decl = gfc_build_label_decl (get_identifier (label_name));

      /* Tell the debugger where the label came from.  */
      if (lp->value <= MAX_LABEL_VALUE)	/* An internal label.  */
	gfc_set_decl_location (label_decl, &lp->where);
      else
	DECL_ARTIFICIAL (label_decl) = 1;

      /* Store the label in the label list and return the LABEL_DECL.  */
      lp->backend_decl = label_decl;
      return label_decl;
    }
}


/* Convert a gfc_symbol to an identifier of the same name.  */

static tree
gfc_sym_identifier (gfc_symbol * sym)
{
  return (get_identifier (sym->name));
}


/* Construct mangled name from symbol name.  */

static tree
gfc_sym_mangled_identifier (gfc_symbol * sym)
{
  char name[GFC_MAX_MANGLED_SYMBOL_LEN + 1];

  if (sym->module == NULL)
    return gfc_sym_identifier (sym);
  else
    {
      snprintf (name, sizeof name, "__%s__%s", sym->module, sym->name);
      return get_identifier (name);
    }
}


/* Construct mangled function name from symbol name.  */

static tree
gfc_sym_mangled_function_id (gfc_symbol * sym)
{
  int has_underscore;
  char name[GFC_MAX_MANGLED_SYMBOL_LEN + 1];

  if (sym->module == NULL || sym->attr.proc == PROC_EXTERNAL
      || (sym->module != NULL && (sym->attr.external
	    || sym->attr.if_source == IFSRC_IFBODY)))
    {
      if (strcmp (sym->name, "MAIN__") == 0
	  || sym->attr.proc == PROC_INTRINSIC)
	return get_identifier (sym->name);

      if (gfc_option.flag_underscoring)
	{
	  has_underscore = strchr (sym->name, '_') != 0;
	  if (gfc_option.flag_second_underscore && has_underscore)
	    snprintf (name, sizeof name, "%s__", sym->name);
	  else
	    snprintf (name, sizeof name, "%s_", sym->name);
	  return get_identifier (name);
	}
      else
	return get_identifier (sym->name);
    }
  else
    {
      snprintf (name, sizeof name, "__%s__%s", sym->module, sym->name);
      return get_identifier (name);
    }
}


/* Returns true if a variable of specified size should go on the stack.  */

int
gfc_can_put_var_on_stack (tree size)
{
  unsigned HOST_WIDE_INT low;

  if (!INTEGER_CST_P (size))
    return 0;

  if (gfc_option.flag_max_stack_var_size < 0)
    return 1;

  if (TREE_INT_CST_HIGH (size) != 0)
    return 0;

  low = TREE_INT_CST_LOW (size);
  if (low > (unsigned HOST_WIDE_INT) gfc_option.flag_max_stack_var_size)
    return 0;

/* TODO: Set a per-function stack size limit.  */

  return 1;
}


/* gfc_finish_cray_pointee sets DECL_VALUE_EXPR for a Cray pointee to
   an expression involving its corresponding pointer.  There are
   2 cases; one for variable size arrays, and one for everything else,
   because variable-sized arrays require one fewer level of
   indirection.  */

static void
gfc_finish_cray_pointee (tree decl, gfc_symbol *sym)
{
  tree ptr_decl = gfc_get_symbol_decl (sym->cp_pointer);
  tree value;

  /* Parameters need to be dereferenced.  */
  if (sym->cp_pointer->attr.dummy) 
    ptr_decl = build_fold_indirect_ref (ptr_decl);

  /* Check to see if we're dealing with a variable-sized array.  */
  if (sym->attr.dimension
      && TREE_CODE (TREE_TYPE (decl)) == POINTER_TYPE) 
    {  
      /* These decls will be dereferenced later, so we don't dereference
	 them here.  */
      value = convert (TREE_TYPE (decl), ptr_decl);
    }
  else
    {
      ptr_decl = convert (build_pointer_type (TREE_TYPE (decl)),
			  ptr_decl);
      value = build_fold_indirect_ref (ptr_decl);
    }

  SET_DECL_VALUE_EXPR (decl, value);
  DECL_HAS_VALUE_EXPR_P (decl) = 1;
  GFC_DECL_CRAY_POINTEE (decl) = 1;
  /* This is a fake variable just for debugging purposes.  */
  TREE_ASM_WRITTEN (decl) = 1;
}


/* Finish processing of a declaration and install its initial value.  */

static void
gfc_finish_decl (tree decl, tree init)
{
  if (TREE_CODE (decl) == PARM_DECL)
    gcc_assert (init == NULL_TREE);
  /* Remember that PARM_DECL doesn't have a DECL_INITIAL field per se
     -- it overlaps DECL_ARG_TYPE.  */
  else if (init == NULL_TREE)
    gcc_assert (DECL_INITIAL (decl) == NULL_TREE);
  else
    gcc_assert (DECL_INITIAL (decl) == error_mark_node);

  if (init != NULL_TREE)
    {
      if (TREE_CODE (decl) != TYPE_DECL)
	DECL_INITIAL (decl) = init;
      else
	{
	  /* typedef foo = bar; store the type of bar as the type of foo.  */
	  TREE_TYPE (decl) = TREE_TYPE (init);
	  DECL_INITIAL (decl) = init = 0;
	}
    }

  if (TREE_CODE (decl) == VAR_DECL)
    {
      if (DECL_SIZE (decl) == NULL_TREE
	  && TYPE_SIZE (TREE_TYPE (decl)) != NULL_TREE)
	layout_decl (decl, 0);

      /* A static variable with an incomplete type is an error if it is
         initialized. Also if it is not file scope. Otherwise, let it
         through, but if it is not `extern' then it may cause an error
         message later.  */
      /* An automatic variable with an incomplete type is an error.  */
      if (DECL_SIZE (decl) == NULL_TREE
          && (TREE_STATIC (decl) ? (DECL_INITIAL (decl) != 0
				    || DECL_CONTEXT (decl) != 0)
                                 : !DECL_EXTERNAL (decl)))
	{
	  gfc_fatal_error ("storage size not known");
	}

      if ((DECL_EXTERNAL (decl) || TREE_STATIC (decl))
	  && (DECL_SIZE (decl) != 0)
	  && (TREE_CODE (DECL_SIZE (decl)) != INTEGER_CST))
	{
	  gfc_fatal_error ("storage size not constant");
	}
    }

}


/* Apply symbol attributes to a variable, and add it to the function scope.  */

static void
gfc_finish_var_decl (tree decl, gfc_symbol * sym)
{
  /* TREE_ADDRESSABLE means the address of this variable is actually needed.
     This is the equivalent of the TARGET variables.
     We also need to set this if the variable is passed by reference in a
     CALL statement.  */

  /* Set DECL_VALUE_EXPR for Cray Pointees.  */
  if (sym->attr.cray_pointee)
    gfc_finish_cray_pointee (decl, sym);

  if (sym->attr.target)
    TREE_ADDRESSABLE (decl) = 1;
  /* If it wasn't used we wouldn't be getting it.  */
  TREE_USED (decl) = 1;

  /* Chain this decl to the pending declarations.  Don't do pushdecl()
     because this would add them to the current scope rather than the
     function scope.  */
  if (current_function_decl != NULL_TREE)
    {
      if (sym->ns->proc_name->backend_decl == current_function_decl
          || sym->result == sym)
	gfc_add_decl_to_function (decl);
      else
	gfc_add_decl_to_parent_function (decl);
    }

  if (sym->attr.cray_pointee)
    return;

  /* If a variable is USE associated, it's always external.  */
  if (sym->attr.use_assoc)
    {
      DECL_EXTERNAL (decl) = 1;
      TREE_PUBLIC (decl) = 1;
    }
  else if (sym->module && !sym->attr.result && !sym->attr.dummy)
    {
      /* TODO: Don't set sym->module for result or dummy variables.  */
      gcc_assert (current_function_decl == NULL_TREE || sym->result == sym);
      /* This is the declaration of a module variable.  */
      TREE_PUBLIC (decl) = 1;
      TREE_STATIC (decl) = 1;
    }

  if ((sym->attr.save || sym->attr.data || sym->value)
      && !sym->attr.use_assoc)
    TREE_STATIC (decl) = 1;
  
  /* Keep variables larger than max-stack-var-size off stack.  */
  if (!sym->ns->proc_name->attr.recursive
      && INTEGER_CST_P (DECL_SIZE_UNIT (decl))
      && !gfc_can_put_var_on_stack (DECL_SIZE_UNIT (decl))
	 /* Put variable length auto array pointers always into stack.  */
      && (TREE_CODE (TREE_TYPE (decl)) != POINTER_TYPE
	  || sym->attr.dimension == 0
	  || sym->as->type != AS_EXPLICIT
	  || sym->attr.pointer
	  || sym->attr.allocatable)
      && !DECL_ARTIFICIAL (decl))
    TREE_STATIC (decl) = 1;

  /* Handle threadprivate variables.  */
  if (sym->attr.threadprivate && targetm.have_tls
      && (TREE_STATIC (decl) || DECL_EXTERNAL (decl)))
    DECL_TLS_MODEL (decl) = decl_default_tls_model (decl);
}


/* Allocate the lang-specific part of a decl.  */

void
gfc_allocate_lang_decl (tree decl)
{
  DECL_LANG_SPECIFIC (decl) = (struct lang_decl *)
    ggc_alloc_cleared (sizeof (struct lang_decl));
}

/* Remember a symbol to generate initialization/cleanup code at function
   entry/exit.  */

static void
gfc_defer_symbol_init (gfc_symbol * sym)
{
  gfc_symbol *p;
  gfc_symbol *last;
  gfc_symbol *head;

  /* Don't add a symbol twice.  */
  if (sym->tlink)
    return;

  last = head = sym->ns->proc_name;
  p = last->tlink;

  /* Make sure that setup code for dummy variables which are used in the
     setup of other variables is generated first.  */
  if (sym->attr.dummy)
    {
      /* Find the first dummy arg seen after us, or the first non-dummy arg.
         This is a circular list, so don't go past the head.  */
      while (p != head
             && (!p->attr.dummy || p->dummy_order > sym->dummy_order))
        {
          last = p;
          p = p->tlink;
        }
    }
  /* Insert in between last and p.  */
  last->tlink = sym;
  sym->tlink = p;
}


/* Create an array index type variable with function scope.  */

static tree
create_index_var (const char * pfx, int nest)
{
  tree decl;

  decl = gfc_create_var_np (gfc_array_index_type, pfx);
  if (nest)
    gfc_add_decl_to_parent_function (decl);
  else
    gfc_add_decl_to_function (decl);
  return decl;
}


/* Create variables to hold all the non-constant bits of info for a
   descriptorless array.  Remember these in the lang-specific part of the
   type.  */

static void
gfc_build_qualified_array (tree decl, gfc_symbol * sym)
{
  tree type;
  int dim;
  int nest;

  type = TREE_TYPE (decl);

  /* We just use the descriptor, if there is one.  */
  if (GFC_DESCRIPTOR_TYPE_P (type))
    return;

  gcc_assert (GFC_ARRAY_TYPE_P (type));
  nest = (sym->ns->proc_name->backend_decl != current_function_decl)
	 && !sym->attr.contained;

  for (dim = 0; dim < GFC_TYPE_ARRAY_RANK (type); dim++)
    {
      if (GFC_TYPE_ARRAY_LBOUND (type, dim) == NULL_TREE)
        GFC_TYPE_ARRAY_LBOUND (type, dim) = create_index_var ("lbound", nest);
      /* Don't try to use the unknown bound for assumed shape arrays.  */
      if (GFC_TYPE_ARRAY_UBOUND (type, dim) == NULL_TREE
          && (sym->as->type != AS_ASSUMED_SIZE
              || dim < GFC_TYPE_ARRAY_RANK (type) - 1))
        GFC_TYPE_ARRAY_UBOUND (type, dim) = create_index_var ("ubound", nest);

      if (GFC_TYPE_ARRAY_STRIDE (type, dim) == NULL_TREE)
        GFC_TYPE_ARRAY_STRIDE (type, dim) = create_index_var ("stride", nest);
    }
  if (GFC_TYPE_ARRAY_OFFSET (type) == NULL_TREE)
    {
      GFC_TYPE_ARRAY_OFFSET (type) = gfc_create_var_np (gfc_array_index_type,
							"offset");
      if (nest)
	gfc_add_decl_to_parent_function (GFC_TYPE_ARRAY_OFFSET (type));
      else
	gfc_add_decl_to_function (GFC_TYPE_ARRAY_OFFSET (type));
    }

  if (GFC_TYPE_ARRAY_SIZE (type) == NULL_TREE
      && sym->as->type != AS_ASSUMED_SIZE)
    GFC_TYPE_ARRAY_SIZE (type) = create_index_var ("size", nest);

  if (POINTER_TYPE_P (type))
    {
      gcc_assert (GFC_ARRAY_TYPE_P (TREE_TYPE (type)));
      gcc_assert (TYPE_LANG_SPECIFIC (type)
		  == TYPE_LANG_SPECIFIC (TREE_TYPE (type)));
      type = TREE_TYPE (type);
    }

  if (! COMPLETE_TYPE_P (type) && GFC_TYPE_ARRAY_SIZE (type))
    {
      tree size, range;

      size = build2 (MINUS_EXPR, gfc_array_index_type,
		     GFC_TYPE_ARRAY_SIZE (type), gfc_index_one_node);
      range = build_range_type (gfc_array_index_type, gfc_index_zero_node,
				size);
      TYPE_DOMAIN (type) = range;
      layout_type (type);
    }
}


/* For some dummy arguments we don't use the actual argument directly.
   Instead we create a local decl and use that.  This allows us to perform
   initialization, and construct full type information.  */

static tree
gfc_build_dummy_array_decl (gfc_symbol * sym, tree dummy)
{
  tree decl;
  tree type;
  gfc_array_spec *as;
  char *name;
  int packed;
  int n;
  bool known_size;

  if (sym->attr.pointer || sym->attr.allocatable)
    return dummy;

  /* Add to list of variables if not a fake result variable.  */
  if (sym->attr.result || sym->attr.dummy)
    gfc_defer_symbol_init (sym);

  type = TREE_TYPE (dummy);
  gcc_assert (TREE_CODE (dummy) == PARM_DECL
	  && POINTER_TYPE_P (type));

  /* Do we know the element size?  */
  known_size = sym->ts.type != BT_CHARACTER
	  || INTEGER_CST_P (sym->ts.cl->backend_decl);
  
  if (known_size && !GFC_DESCRIPTOR_TYPE_P (TREE_TYPE (type)))
    {
      /* For descriptorless arrays with known element size the actual
         argument is sufficient.  */
      gcc_assert (GFC_ARRAY_TYPE_P (type));
      gfc_build_qualified_array (dummy, sym);
      return dummy;
    }

  type = TREE_TYPE (type);
  if (GFC_DESCRIPTOR_TYPE_P (type))
    {
      /* Create a descriptorless array pointer.  */
      as = sym->as;
      packed = 0;
      if (!gfc_option.flag_repack_arrays)
	{
	  if (as->type == AS_ASSUMED_SIZE)
	    packed = 2;
	}
      else
	{
	  if (as->type == AS_EXPLICIT)
	    {
	      packed = 2;
	      for (n = 0; n < as->rank; n++)
		{
		  if (!(as->upper[n]
			&& as->lower[n]
			&& as->upper[n]->expr_type == EXPR_CONSTANT
			&& as->lower[n]->expr_type == EXPR_CONSTANT))
		    packed = 1;
		}
	    }
	  else
	    packed = 1;
	}

      type = gfc_typenode_for_spec (&sym->ts);
      type = gfc_get_nodesc_array_type (type, sym->as, packed);
    }
  else
    {
      /* We now have an expression for the element size, so create a fully
	 qualified type.  Reset sym->backend decl or this will just return the
	 old type.  */
      DECL_ARTIFICIAL (sym->backend_decl) = 1;
      sym->backend_decl = NULL_TREE;
      type = gfc_sym_type (sym);
      packed = 2;
    }

  ASM_FORMAT_PRIVATE_NAME (name, IDENTIFIER_POINTER (DECL_NAME (dummy)), 0);
  decl = build_decl (VAR_DECL, get_identifier (name), type);

  DECL_ARTIFICIAL (decl) = 1;
  TREE_PUBLIC (decl) = 0;
  TREE_STATIC (decl) = 0;
  DECL_EXTERNAL (decl) = 0;

  /* We should never get deferred shape arrays here.  We used to because of
     frontend bugs.  */
  gcc_assert (sym->as->type != AS_DEFERRED);

  switch (packed)
    {
    case 1:
      GFC_DECL_PARTIAL_PACKED_ARRAY (decl) = 1;
      break;

    case 2:
      GFC_DECL_PACKED_ARRAY (decl) = 1;
      break;
    }

  gfc_build_qualified_array (decl, sym);

  if (DECL_LANG_SPECIFIC (dummy))
    DECL_LANG_SPECIFIC (decl) = DECL_LANG_SPECIFIC (dummy);
  else
    gfc_allocate_lang_decl (decl);

  GFC_DECL_SAVED_DESCRIPTOR (decl) = dummy;

  if (sym->ns->proc_name->backend_decl == current_function_decl
      || sym->attr.contained)
    gfc_add_decl_to_function (decl);
  else
    gfc_add_decl_to_parent_function (decl);

  return decl;
}


/* Return a constant or a variable to use as a string length.  Does not
   add the decl to the current scope.  */

static tree
gfc_create_string_length (gfc_symbol * sym)
{
  tree length;

  gcc_assert (sym->ts.cl);
  gfc_conv_const_charlen (sym->ts.cl);
  
  if (sym->ts.cl->backend_decl == NULL_TREE)
    {
      char name[GFC_MAX_MANGLED_SYMBOL_LEN + 2];

      /* Also prefix the mangled name.  */
      strcpy (&name[1], sym->name);
      name[0] = '.';
      length = build_decl (VAR_DECL, get_identifier (name),
			   gfc_charlen_type_node);
      DECL_ARTIFICIAL (length) = 1;
      TREE_USED (length) = 1;
      if (sym->ns->proc_name->tlink != NULL)
	gfc_defer_symbol_init (sym);
      sym->ts.cl->backend_decl = length;
    }

  return sym->ts.cl->backend_decl;
}

/* If a variable is assigned a label, we add another two auxiliary
   variables.  */

static void
gfc_add_assign_aux_vars (gfc_symbol * sym)
{
  tree addr;
  tree length;
  tree decl;

  gcc_assert (sym->backend_decl);

  decl = sym->backend_decl;
  gfc_allocate_lang_decl (decl);
  GFC_DECL_ASSIGN (decl) = 1;
  length = build_decl (VAR_DECL, create_tmp_var_name (sym->name),
		       gfc_charlen_type_node);
  addr = build_decl (VAR_DECL, create_tmp_var_name (sym->name),
		     pvoid_type_node);
  gfc_finish_var_decl (length, sym);
  gfc_finish_var_decl (addr, sym);
  /*  STRING_LENGTH is also used as flag. Less than -1 means that
      ASSIGN_ADDR can not be used. Equal -1 means that ASSIGN_ADDR is the
      target label's address. Otherwise, value is the length of a format string
      and ASSIGN_ADDR is its address.  */
  if (TREE_STATIC (length))
    DECL_INITIAL (length) = build_int_cst (NULL_TREE, -2);
  else
    gfc_defer_symbol_init (sym);

  GFC_DECL_STRING_LEN (decl) = length;
  GFC_DECL_ASSIGN_ADDR (decl) = addr;
}

/* Return the decl for a gfc_symbol, create it if it doesn't already
   exist.  */

tree
gfc_get_symbol_decl (gfc_symbol * sym)
{
  tree decl;
  tree length = NULL_TREE;
  int byref;

  gcc_assert (sym->attr.referenced
               || sym->ns->proc_name->attr.if_source == IFSRC_IFBODY);

  if (sym->ns && sym->ns->proc_name->attr.function)
    byref = gfc_return_by_reference (sym->ns->proc_name);
  else
    byref = 0;

  if ((sym->attr.dummy && ! sym->attr.function) || (sym->attr.result && byref))
    {
      /* Return via extra parameter.  */
      if (sym->attr.result && byref
	  && !sym->backend_decl)
	{
	  sym->backend_decl =
	    DECL_ARGUMENTS (sym->ns->proc_name->backend_decl);
	  /* For entry master function skip over the __entry
	     argument.  */
	  if (sym->ns->proc_name->attr.entry_master)
	    sym->backend_decl = TREE_CHAIN (sym->backend_decl);
	}

      /* Dummy variables should already have been created.  */
      gcc_assert (sym->backend_decl);

      /* Create a character length variable.  */
      if (sym->ts.type == BT_CHARACTER)
	{
	  if (sym->ts.cl->backend_decl == NULL_TREE)
	    length = gfc_create_string_length (sym);
	  else
	    length = sym->ts.cl->backend_decl;
	  if (TREE_CODE (length) == VAR_DECL
	      && DECL_CONTEXT (length) == NULL_TREE)
	    {
	      /* Add the string length to the same context as the symbol.  */
	      if (DECL_CONTEXT (sym->backend_decl) == current_function_decl)
	        gfc_add_decl_to_function (length);
	      else
		gfc_add_decl_to_parent_function (length);

	      gcc_assert (DECL_CONTEXT (sym->backend_decl) ==
			    DECL_CONTEXT (length));

	      gfc_defer_symbol_init (sym);
	    }
	}

      /* Use a copy of the descriptor for dummy arrays.  */
      if (sym->attr.dimension && !TREE_USED (sym->backend_decl))
        {
	  decl = gfc_build_dummy_array_decl (sym, sym->backend_decl);
	  /* Prevent the dummy from being detected as unused if it is copied.  */
	  if (sym->backend_decl != NULL && decl != sym->backend_decl)
	    DECL_ARTIFICIAL (sym->backend_decl) = 1;
	  sym->backend_decl = decl;
	}

      TREE_USED (sym->backend_decl) = 1;
      if (sym->attr.assign && GFC_DECL_ASSIGN (sym->backend_decl) == 0)
	{
	  gfc_add_assign_aux_vars (sym);
	}
      return sym->backend_decl;
    }

  if (sym->backend_decl)
    return sym->backend_decl;

  /* Catch function declarations.  Only used for actual parameters.  */
  if (sym->attr.flavor == FL_PROCEDURE)
    {
      decl = gfc_get_extern_function_decl (sym);
      return decl;
    }

  if (sym->attr.intrinsic)
    internal_error ("intrinsic variable which isn't a procedure");

  /* Create string length decl first so that they can be used in the
     type declaration.  */
  if (sym->ts.type == BT_CHARACTER)
    length = gfc_create_string_length (sym);

  /* Create the decl for the variable.  */
  decl = build_decl (VAR_DECL, gfc_sym_identifier (sym), gfc_sym_type (sym));

  gfc_set_decl_location (decl, &sym->declared_at);

  /* Symbols from modules should have their assembler names mangled.
     This is done here rather than in gfc_finish_var_decl because it
     is different for string length variables.  */
  if (sym->module)
    SET_DECL_ASSEMBLER_NAME (decl, gfc_sym_mangled_identifier (sym));

  if (sym->attr.dimension)
    {
      /* Create variables to hold the non-constant bits of array info.  */
      gfc_build_qualified_array (decl, sym);

      /* Remember this variable for allocation/cleanup.  */
      gfc_defer_symbol_init (sym);

      if ((sym->attr.allocatable || !sym->attr.dummy) && !sym->attr.pointer)
	GFC_DECL_PACKED_ARRAY (decl) = 1;
    }

  if (sym->ts.type == BT_DERIVED && sym->ts.derived->attr.alloc_comp)
    gfc_defer_symbol_init (sym);

  gfc_finish_var_decl (decl, sym);

  if (sym->ts.type == BT_CHARACTER)
    {
      /* Character variables need special handling.  */
      gfc_allocate_lang_decl (decl);

      if (TREE_CODE (length) != INTEGER_CST)
	{
	  char name[GFC_MAX_MANGLED_SYMBOL_LEN + 2];

	  if (sym->module)
	    {
	      /* Also prefix the mangled name for symbols from modules.  */
	      strcpy (&name[1], sym->name);
	      name[0] = '.';
	      strcpy (&name[1],
		      IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (length)));
	      SET_DECL_ASSEMBLER_NAME (decl, get_identifier (name));
	    }
	  gfc_finish_var_decl (length, sym);
	  gcc_assert (!sym->value);
	}
    }
  sym->backend_decl = decl;

  if (sym->attr.assign)
    {
      gfc_add_assign_aux_vars (sym);
    }

  if (TREE_STATIC (decl) && !sym->attr.use_assoc)
    {
      /* Add static initializer.  */
      DECL_INITIAL (decl) = gfc_conv_initializer (sym->value, &sym->ts,
	  TREE_TYPE (decl), sym->attr.dimension,
	  sym->attr.pointer || sym->attr.allocatable);
    }

  return decl;
}


/* Substitute a temporary variable in place of the real one.  */

void
gfc_shadow_sym (gfc_symbol * sym, tree decl, gfc_saved_var * save)
{
  save->attr = sym->attr;
  save->decl = sym->backend_decl;

  gfc_clear_attr (&sym->attr);
  sym->attr.referenced = 1;
  sym->attr.flavor = FL_VARIABLE;

  sym->backend_decl = decl;
}


/* Restore the original variable.  */

void
gfc_restore_sym (gfc_symbol * sym, gfc_saved_var * save)
{
  sym->attr = save->attr;
  sym->backend_decl = save->decl;
}


/* Get a basic decl for an external function.  */

tree
gfc_get_extern_function_decl (gfc_symbol * sym)
{
  tree type;
  tree fndecl;
  gfc_expr e;
  gfc_intrinsic_sym *isym;
  gfc_expr argexpr;
  char s[GFC_MAX_SYMBOL_LEN + 13]; /* "f2c_specific" and '\0'.  */
  tree name;
  tree mangled_name;

  if (sym->backend_decl)
    return sym->backend_decl;

  /* We should never be creating external decls for alternate entry points.
     The procedure may be an alternate entry point, but we don't want/need
     to know that.  */
  gcc_assert (!(sym->attr.entry || sym->attr.entry_master));

  if (sym->attr.intrinsic)
    {
      /* Call the resolution function to get the actual name.  This is
         a nasty hack which relies on the resolution functions only looking
	 at the first argument.  We pass NULL for the second argument
	 otherwise things like AINT get confused.  */
      isym = gfc_find_function (sym->name);
      gcc_assert (isym->resolve.f0 != NULL);

      memset (&e, 0, sizeof (e));
      e.expr_type = EXPR_FUNCTION;

      memset (&argexpr, 0, sizeof (argexpr));
      gcc_assert (isym->formal);
      argexpr.ts = isym->formal->ts;

      if (isym->formal->next == NULL)
	isym->resolve.f1 (&e, &argexpr);
      else
	{
	  if (isym->formal->next->next == NULL)
	    isym->resolve.f2 (&e, &argexpr, NULL);
	  else
	    {
	      /* All specific intrinsics take less than 4 arguments.  */
	      gcc_assert (isym->formal->next->next->next == NULL);
	      isym->resolve.f3 (&e, &argexpr, NULL, NULL);
	    }
	}

      if (gfc_option.flag_f2c
	  && ((e.ts.type == BT_REAL && e.ts.kind == gfc_default_real_kind)
	      || e.ts.type == BT_COMPLEX))
	{
	  /* Specific which needs a different implementation if f2c
	     calling conventions are used.  */
	  sprintf (s, "f2c_specific%s", e.value.function.name);
	}
      else
	sprintf (s, "specific%s", e.value.function.name);

      name = get_identifier (s);
      mangled_name = name;
    }
  else
    {
      name = gfc_sym_identifier (sym);
      mangled_name = gfc_sym_mangled_function_id (sym);
    }

  type = gfc_get_function_type (sym);
  fndecl = build_decl (FUNCTION_DECL, name, type);

  SET_DECL_ASSEMBLER_NAME (fndecl, mangled_name);
  /* If the return type is a pointer, avoid alias issues by setting
     DECL_IS_MALLOC to nonzero. This means that the function should be
     treated as if it were a malloc, meaning it returns a pointer that
     is not an alias.  */
  if (POINTER_TYPE_P (type))
    DECL_IS_MALLOC (fndecl) = 1;

  /* Set the context of this decl.  */
  if (0 && sym->ns && sym->ns->proc_name)
    {
      /* TODO: Add external decls to the appropriate scope.  */
      DECL_CONTEXT (fndecl) = sym->ns->proc_name->backend_decl;
    }
  else
    {
      /* Global declaration, e.g. intrinsic subroutine.  */
      DECL_CONTEXT (fndecl) = NULL_TREE;
    }

  DECL_EXTERNAL (fndecl) = 1;

  /* This specifies if a function is globally addressable, i.e. it is
     the opposite of declaring static in C.  */
  TREE_PUBLIC (fndecl) = 1;

  /* Set attributes for PURE functions. A call to PURE function in the
     Fortran 95 sense is both pure and without side effects in the C
     sense.  */
  if (sym->attr.pure || sym->attr.elemental)
    {
      if (sym->attr.function && !gfc_return_by_reference (sym))
	DECL_IS_PURE (fndecl) = 1;
      /* TODO: check if pure SUBROUTINEs don't have INTENT(OUT)
	 parameters and don't use alternate returns (is this
	 allowed?). In that case, calls to them are meaningless, and
	 can be optimized away. See also in build_function_decl().  */
      TREE_SIDE_EFFECTS (fndecl) = 0;
    }

  /* Mark non-returning functions.  */
  if (sym->attr.noreturn)
      TREE_THIS_VOLATILE(fndecl) = 1;

  sym->backend_decl = fndecl;

  if (DECL_CONTEXT (fndecl) == NULL_TREE)
    pushdecl_top_level (fndecl);

  return fndecl;
}


/* Create a declaration for a procedure.  For external functions (in the C
   sense) use gfc_get_extern_function_decl.  HAS_ENTRIES is true if this is
   a master function with alternate entry points.  */

static void
build_function_decl (gfc_symbol * sym)
{
  tree fndecl, type;
  symbol_attribute attr;
  tree result_decl;
  gfc_formal_arglist *f;

  gcc_assert (!sym->backend_decl);
  gcc_assert (!sym->attr.external);

  /* Set the line and filename.  sym->declared_at seems to point to the
     last statement for subroutines, but it'll do for now.  */
  gfc_set_backend_locus (&sym->declared_at);

  /* Allow only one nesting level.  Allow public declarations.  */
  gcc_assert (current_function_decl == NULL_TREE
	  || DECL_CONTEXT (current_function_decl) == NULL_TREE);

  type = gfc_get_function_type (sym);
  fndecl = build_decl (FUNCTION_DECL, gfc_sym_identifier (sym), type);

  /* Perform name mangling if this is a top level or module procedure.  */
  if (current_function_decl == NULL_TREE)
    SET_DECL_ASSEMBLER_NAME (fndecl, gfc_sym_mangled_function_id (sym));

  /* Figure out the return type of the declared function, and build a
     RESULT_DECL for it.  If this is a subroutine with alternate
     returns, build a RESULT_DECL for it.  */
  attr = sym->attr;

  result_decl = NULL_TREE;
  /* TODO: Shouldn't this just be TREE_TYPE (TREE_TYPE (fndecl)).  */
  if (attr.function)
    {
      if (gfc_return_by_reference (sym))
	type = void_type_node;
      else
	{
	  if (sym->result != sym)
	    result_decl = gfc_sym_identifier (sym->result);

	  type = TREE_TYPE (TREE_TYPE (fndecl));
	}
    }
  else
    {
      /* Look for alternate return placeholders.  */
      int has_alternate_returns = 0;
      for (f = sym->formal; f; f = f->next)
	{
	  if (f->sym == NULL)
	    {
	      has_alternate_returns = 1;
	      break;
	    }
	}

      if (has_alternate_returns)
	type = integer_type_node;
      else
	type = void_type_node;
    }

  result_decl = build_decl (RESULT_DECL, result_decl, type);
  DECL_ARTIFICIAL (result_decl) = 1;
  DECL_IGNORED_P (result_decl) = 1;
  DECL_CONTEXT (result_decl) = fndecl;
  DECL_RESULT (fndecl) = result_decl;

  /* Don't call layout_decl for a RESULT_DECL.
     layout_decl (result_decl, 0);  */

  /* If the return type is a pointer, avoid alias issues by setting
     DECL_IS_MALLOC to nonzero. This means that the function should be
     treated as if it were a malloc, meaning it returns a pointer that
     is not an alias.  */
  if (POINTER_TYPE_P (type))
    DECL_IS_MALLOC (fndecl) = 1;

  /* Set up all attributes for the function.  */
  DECL_CONTEXT (fndecl) = current_function_decl;
  DECL_EXTERNAL (fndecl) = 0;

  /* This specifies if a function is globally visible, i.e. it is
     the opposite of declaring static in C.  */
  if (DECL_CONTEXT (fndecl) == NULL_TREE
      && !sym->attr.entry_master)
    TREE_PUBLIC (fndecl) = 1;

  /* TREE_STATIC means the function body is defined here.  */
  TREE_STATIC (fndecl) = 1;

  /* Set attributes for PURE functions. A call to a PURE function in the
     Fortran 95 sense is both pure and without side effects in the C
     sense.  */
  if (attr.pure || attr.elemental)
    {
      /* TODO: check if a pure SUBROUTINE has no INTENT(OUT) arguments
	 including a alternate return. In that case it can also be
	 marked as PURE. See also in gfc_get_extern_function_decl().  */
      if (attr.function && !gfc_return_by_reference (sym))
	DECL_IS_PURE (fndecl) = 1;
      TREE_SIDE_EFFECTS (fndecl) = 0;
    }

  /* Layout the function declaration and put it in the binding level
     of the current function.  */
  pushdecl (fndecl);

  sym->backend_decl = fndecl;
}


/* Create the DECL_ARGUMENTS for a procedure.  */

static void
create_function_arglist (gfc_symbol * sym)
{
  tree fndecl;
  gfc_formal_arglist *f;
  tree typelist, hidden_typelist;
  tree arglist, hidden_arglist;
  tree type;
  tree parm;

  fndecl = sym->backend_decl;

  /* Build formal argument list. Make sure that their TREE_CONTEXT is
     the new FUNCTION_DECL node.  */
  arglist = NULL_TREE;
  hidden_arglist = NULL_TREE;
  typelist = TYPE_ARG_TYPES (TREE_TYPE (fndecl));

  if (sym->attr.entry_master)
    {
      type = TREE_VALUE (typelist);
      parm = build_decl (PARM_DECL, get_identifier ("__entry"), type);
      
      DECL_CONTEXT (parm) = fndecl;
      DECL_ARG_TYPE (parm) = type;
      TREE_READONLY (parm) = 1;
      gfc_finish_decl (parm, NULL_TREE);
      DECL_ARTIFICIAL (parm) = 1;

      arglist = chainon (arglist, parm);
      typelist = TREE_CHAIN (typelist);
    }

  if (gfc_return_by_reference (sym))
    {
      tree type = TREE_VALUE (typelist), length = NULL;

      if (sym->ts.type == BT_CHARACTER)
	{
	  /* Length of character result.  */
	  tree len_type = TREE_VALUE (TREE_CHAIN (typelist));
	  gcc_assert (len_type == gfc_charlen_type_node);

	  length = build_decl (PARM_DECL,
			       get_identifier (".__result"),
			       len_type);
	  if (!sym->ts.cl->length)
	    {
	      sym->ts.cl->backend_decl = length;
	      TREE_USED (length) = 1;
	    }
	  gcc_assert (TREE_CODE (length) == PARM_DECL);
	  DECL_CONTEXT (length) = fndecl;
	  DECL_ARG_TYPE (length) = len_type;
	  TREE_READONLY (length) = 1;
	  DECL_ARTIFICIAL (length) = 1;
	  gfc_finish_decl (length, NULL_TREE);
	  if (sym->ts.cl->backend_decl == NULL
	      || sym->ts.cl->backend_decl == length)
	    {
	      gfc_symbol *arg;
	      tree backend_decl;

	      if (sym->ts.cl->backend_decl == NULL)
		{
		  tree len = build_decl (VAR_DECL,
					 get_identifier ("..__result"),
					 gfc_charlen_type_node);
		  DECL_ARTIFICIAL (len) = 1;
		  TREE_USED (len) = 1;
		  sym->ts.cl->backend_decl = len;
		}

	      /* Make sure PARM_DECL type doesn't point to incomplete type.  */
	      arg = sym->result ? sym->result : sym;
	      backend_decl = arg->backend_decl;
	      /* Temporary clear it, so that gfc_sym_type creates complete
		 type.  */
	      arg->backend_decl = NULL;
	      type = gfc_sym_type (arg);
	      arg->backend_decl = backend_decl;
	      type = build_reference_type (type);
	    }
	}

      parm = build_decl (PARM_DECL, get_identifier ("__result"), type);

      DECL_CONTEXT (parm) = fndecl;
      DECL_ARG_TYPE (parm) = TREE_VALUE (typelist);
      TREE_READONLY (parm) = 1;
      DECL_ARTIFICIAL (parm) = 1;
      gfc_finish_decl (parm, NULL_TREE);

      arglist = chainon (arglist, parm);
      typelist = TREE_CHAIN (typelist);

      if (sym->ts.type == BT_CHARACTER)
	{
	  gfc_allocate_lang_decl (parm);
	  arglist = chainon (arglist, length);
	  typelist = TREE_CHAIN (typelist);
	}
    }

  hidden_typelist = typelist;
  for (f = sym->formal; f; f = f->next)
    if (f->sym != NULL)	/* Ignore alternate returns.  */
      hidden_typelist = TREE_CHAIN (hidden_typelist);

  for (f = sym->formal; f; f = f->next)
    {
      char name[GFC_MAX_SYMBOL_LEN + 2];

      /* Ignore alternate returns.  */
      if (f->sym == NULL)
	continue;

      type = TREE_VALUE (typelist);

      if (f->sym->ts.type == BT_CHARACTER)
	{
	  tree len_type = TREE_VALUE (hidden_typelist);
	  tree length = NULL_TREE;
	  gcc_assert (len_type == gfc_charlen_type_node);

	  strcpy (&name[1], f->sym->name);
	  name[0] = '_';
	  length = build_decl (PARM_DECL, get_identifier (name), len_type);

	  hidden_arglist = chainon (hidden_arglist, length);
	  DECL_CONTEXT (length) = fndecl;
	  DECL_ARTIFICIAL (length) = 1;
	  DECL_ARG_TYPE (length) = len_type;
	  TREE_READONLY (length) = 1;
	  gfc_finish_decl (length, NULL_TREE);

	  /* TODO: Check string lengths when -fbounds-check.  */

	  /* Use the passed value for assumed length variables.  */
	  if (!f->sym->ts.cl->length)
	    {
	      TREE_USED (length) = 1;
	      if (!f->sym->ts.cl->backend_decl)
		f->sym->ts.cl->backend_decl = length;
	      else
		{
		  /* there is already another variable using this
		     gfc_charlen node, build a new one for this variable
		     and chain it into the list of gfc_charlens.
		     This happens for e.g. in the case
		     CHARACTER(*)::c1,c2
		     since CHARACTER declarations on the same line share
		     the same gfc_charlen node.  */
		  gfc_charlen *cl;
	      
		  cl = gfc_get_charlen ();
		  cl->backend_decl = length;
		  cl->next = f->sym->ts.cl->next;
		  f->sym->ts.cl->next = cl;
		  f->sym->ts.cl = cl;
		}
	    }

	  hidden_typelist = TREE_CHAIN (hidden_typelist);

	  if (f->sym->ts.cl->backend_decl == NULL
	      || f->sym->ts.cl->backend_decl == length)
	    {
	      if (f->sym->ts.cl->backend_decl == NULL)
		gfc_create_string_length (f->sym);

	      /* Make sure PARM_DECL type doesn't point to incomplete type.  */
	      if (f->sym->attr.flavor == FL_PROCEDURE)
		type = build_pointer_type (gfc_get_function_type (f->sym));
	      else
		type = gfc_sym_type (f->sym);
	    }
	}

      /* For non-constant length array arguments, make sure they use
	 a different type node from TYPE_ARG_TYPES type.  */
      if (f->sym->attr.dimension
	  && type == TREE_VALUE (typelist)
	  && TREE_CODE (type) == POINTER_TYPE
	  && GFC_ARRAY_TYPE_P (type)
	  && f->sym->as->type != AS_ASSUMED_SIZE
	  && ! COMPLETE_TYPE_P (TREE_TYPE (type)))
	{
	  if (f->sym->attr.flavor == FL_PROCEDURE)
	    type = build_pointer_type (gfc_get_function_type (f->sym));
	  else
	    type = gfc_sym_type (f->sym);
	}

      /* Build a the argument declaration.  */
      parm = build_decl (PARM_DECL, gfc_sym_identifier (f->sym), type);

      /* Fill in arg stuff.  */
      DECL_CONTEXT (parm) = fndecl;
      DECL_ARG_TYPE (parm) = TREE_VALUE (typelist);
      /* All implementation args are read-only.  */
      TREE_READONLY (parm) = 1;

      gfc_finish_decl (parm, NULL_TREE);

      f->sym->backend_decl = parm;

      arglist = chainon (arglist, parm);
      typelist = TREE_CHAIN (typelist);
    }

  /* Add the hidden string length parameters.  */
  arglist = chainon (arglist, hidden_arglist);

  gcc_assert (hidden_typelist == NULL_TREE
              || TREE_VALUE (hidden_typelist) == void_type_node);
  DECL_ARGUMENTS (fndecl) = arglist;
}

/* Convert FNDECL's code to GIMPLE and handle any nested functions.  */

static void
gfc_gimplify_function (tree fndecl)
{
  struct cgraph_node *cgn;

  gimplify_function_tree (fndecl);
  dump_function (TDI_generic, fndecl);

  /* Generate errors for structured block violations.  */
  /* ??? Could be done as part of resolve_labels.  */
  if (flag_openmp)
    diagnose_omp_structured_block_errors (fndecl);

  /* Convert all nested functions to GIMPLE now.  We do things in this order
     so that items like VLA sizes are expanded properly in the context of the
     correct function.  */
  cgn = cgraph_node (fndecl);
  for (cgn = cgn->nested; cgn; cgn = cgn->next_nested)
    gfc_gimplify_function (cgn->decl);
}


/* Do the setup necessary before generating the body of a function.  */

static void
trans_function_start (gfc_symbol * sym)
{
  tree fndecl;

  fndecl = sym->backend_decl;

  /* Let GCC know the current scope is this function.  */
  current_function_decl = fndecl;

  /* Let the world know what we're about to do.  */
  announce_function (fndecl);

  if (DECL_CONTEXT (fndecl) == NULL_TREE)
    {
      /* Create RTL for function declaration.  */
      rest_of_decl_compilation (fndecl, 1, 0);
    }

  /* Create RTL for function definition.  */
  make_decl_rtl (fndecl);

  init_function_start (fndecl);

  /* Even though we're inside a function body, we still don't want to
     call expand_expr to calculate the size of a variable-sized array.
     We haven't necessarily assigned RTL to all variables yet, so it's
     not safe to try to expand expressions involving them.  */
  cfun->x_dont_save_pending_sizes_p = 1;

  /* function.c requires a push at the start of the function.  */
  pushlevel (0);
}

/* Create thunks for alternate entry points.  */

static void
build_entry_thunks (gfc_namespace * ns)
{
  gfc_formal_arglist *formal;
  gfc_formal_arglist *thunk_formal;
  gfc_entry_list *el;
  gfc_symbol *thunk_sym;
  stmtblock_t body;
  tree thunk_fndecl;
  tree args;
  tree string_args;
  tree tmp;
  locus old_loc;

  /* This should always be a toplevel function.  */
  gcc_assert (current_function_decl == NULL_TREE);

  gfc_get_backend_locus (&old_loc);
  for (el = ns->entries; el; el = el->next)
    {
      thunk_sym = el->sym;
      
      build_function_decl (thunk_sym);
      create_function_arglist (thunk_sym);

      trans_function_start (thunk_sym);

      thunk_fndecl = thunk_sym->backend_decl;

      gfc_start_block (&body);

      /* Pass extra parameter identifying this entry point.  */
      tmp = build_int_cst (gfc_array_index_type, el->id);
      args = tree_cons (NULL_TREE, tmp, NULL_TREE);
      string_args = NULL_TREE;

      if (thunk_sym->attr.function)
	{
	  if (gfc_return_by_reference (ns->proc_name))
	    {
	      tree ref = DECL_ARGUMENTS (current_function_decl);
	      args = tree_cons (NULL_TREE, ref, args);
	      if (ns->proc_name->ts.type == BT_CHARACTER)
		args = tree_cons (NULL_TREE, TREE_CHAIN (ref),
				  args);
	    }
	}

      for (formal = ns->proc_name->formal; formal; formal = formal->next)
	{
	  /* Ignore alternate returns.  */
	  if (formal->sym == NULL)
	    continue;

	  /* We don't have a clever way of identifying arguments, so resort to
	     a brute-force search.  */
	  for (thunk_formal = thunk_sym->formal;
	       thunk_formal;
	       thunk_formal = thunk_formal->next)
	    {
	      if (thunk_formal->sym == formal->sym)
		break;
	    }

	  if (thunk_formal)
	    {
	      /* Pass the argument.  */
	      DECL_ARTIFICIAL (thunk_formal->sym->backend_decl) = 1;
	      args = tree_cons (NULL_TREE, thunk_formal->sym->backend_decl,
				args);
	      if (formal->sym->ts.type == BT_CHARACTER)
		{
		  tmp = thunk_formal->sym->ts.cl->backend_decl;
		  string_args = tree_cons (NULL_TREE, tmp, string_args);
		}
	    }
	  else
	    {
	      /* Pass NULL for a missing argument.  */
	      args = tree_cons (NULL_TREE, null_pointer_node, args);
	      if (formal->sym->ts.type == BT_CHARACTER)
		{
		  tmp = build_int_cst (gfc_charlen_type_node, 0);
		  string_args = tree_cons (NULL_TREE, tmp, string_args);
		}
	    }
	}

      /* Call the master function.  */
      args = nreverse (args);
      args = chainon (args, nreverse (string_args));
      tmp = ns->proc_name->backend_decl;
      tmp = build_function_call_expr (tmp, args);
      if (ns->proc_name->attr.mixed_entry_master)
	{
	  tree union_decl, field;
	  tree master_type = TREE_TYPE (ns->proc_name->backend_decl);

	  union_decl = build_decl (VAR_DECL, get_identifier ("__result"),
				   TREE_TYPE (master_type));
	  DECL_ARTIFICIAL (union_decl) = 1;
	  DECL_EXTERNAL (union_decl) = 0;
	  TREE_PUBLIC (union_decl) = 0;
	  TREE_USED (union_decl) = 1;
	  layout_decl (union_decl, 0);
	  pushdecl (union_decl);

	  DECL_CONTEXT (union_decl) = current_function_decl;
	  tmp = build2 (MODIFY_EXPR,
			TREE_TYPE (union_decl),
			union_decl, tmp);
	  gfc_add_expr_to_block (&body, tmp);

	  for (field = TYPE_FIELDS (TREE_TYPE (union_decl));
	       field; field = TREE_CHAIN (field))
	    if (strcmp (IDENTIFIER_POINTER (DECL_NAME (field)),
		thunk_sym->result->name) == 0)
	      break;
	  gcc_assert (field != NULL_TREE);
	  tmp = build3 (COMPONENT_REF, TREE_TYPE (field), union_decl, field,
			NULL_TREE);
	  tmp = build2 (MODIFY_EXPR,
			TREE_TYPE (DECL_RESULT (current_function_decl)),
			DECL_RESULT (current_function_decl), tmp);
	  tmp = build1_v (RETURN_EXPR, tmp);
	}
      else if (TREE_TYPE (DECL_RESULT (current_function_decl))
	       != void_type_node)
	{
	  tmp = build2 (MODIFY_EXPR,
			TREE_TYPE (DECL_RESULT (current_function_decl)),
			DECL_RESULT (current_function_decl), tmp);
	  tmp = build1_v (RETURN_EXPR, tmp);
	}
      gfc_add_expr_to_block (&body, tmp);

      /* Finish off this function and send it for code generation.  */
      DECL_SAVED_TREE (thunk_fndecl) = gfc_finish_block (&body);
      poplevel (1, 0, 1);
      BLOCK_SUPERCONTEXT (DECL_INITIAL (thunk_fndecl)) = thunk_fndecl;

      /* Output the GENERIC tree.  */
      dump_function (TDI_original, thunk_fndecl);

      /* Store the end of the function, so that we get good line number
	 info for the epilogue.  */
      cfun->function_end_locus = input_location;

      /* We're leaving the context of this function, so zap cfun.
	 It's still in DECL_STRUCT_FUNCTION, and we'll restore it in
	 tree_rest_of_compilation.  */
      cfun = NULL;

      current_function_decl = NULL_TREE;

      gfc_gimplify_function (thunk_fndecl);
      cgraph_finalize_function (thunk_fndecl, false);

      /* We share the symbols in the formal argument list with other entry
	 points and the master function.  Clear them so that they are
	 recreated for each function.  */
      for (formal = thunk_sym->formal; formal; formal = formal->next)
	if (formal->sym != NULL)  /* Ignore alternate returns.  */
	  {
	    formal->sym->backend_decl = NULL_TREE;
	    if (formal->sym->ts.type == BT_CHARACTER)
	      formal->sym->ts.cl->backend_decl = NULL_TREE;
	  }

      if (thunk_sym->attr.function)
	{
	  if (thunk_sym->ts.type == BT_CHARACTER)
	    thunk_sym->ts.cl->backend_decl = NULL_TREE;
	  if (thunk_sym->result->ts.type == BT_CHARACTER)
	    thunk_sym->result->ts.cl->backend_decl = NULL_TREE;
	}
    }

  gfc_set_backend_locus (&old_loc);
}


/* Create a decl for a function, and create any thunks for alternate entry
   points.  */

void
gfc_create_function_decl (gfc_namespace * ns)
{
  /* Create a declaration for the master function.  */
  build_function_decl (ns->proc_name);

  /* Compile the entry thunks.  */
  if (ns->entries)
    build_entry_thunks (ns);

  /* Now create the read argument list.  */
  create_function_arglist (ns->proc_name);
}

/* Return the decl used to hold the function return value.  If
   parent_flag is set, the context is the parent_scope.  */

tree
gfc_get_fake_result_decl (gfc_symbol * sym, int parent_flag)
{
  tree decl;
  tree length;
  tree this_fake_result_decl;
  tree this_function_decl;

  char name[GFC_MAX_SYMBOL_LEN + 10];

  if (parent_flag)
    {
      this_fake_result_decl = parent_fake_result_decl;
      this_function_decl = DECL_CONTEXT (current_function_decl);
    }
  else
    {
      this_fake_result_decl = current_fake_result_decl;
      this_function_decl = current_function_decl;
    }

  if (sym
      && sym->ns->proc_name->backend_decl == this_function_decl
      && sym->ns->proc_name->attr.entry_master
      && sym != sym->ns->proc_name)
    {
      tree t = NULL, var;
      if (this_fake_result_decl != NULL)
	for (t = TREE_CHAIN (this_fake_result_decl); t; t = TREE_CHAIN (t))
	  if (strcmp (IDENTIFIER_POINTER (TREE_PURPOSE (t)), sym->name) == 0)
	    break;
      if (t)
	return TREE_VALUE (t);
      decl = gfc_get_fake_result_decl (sym->ns->proc_name, parent_flag);

      if (parent_flag)
	this_fake_result_decl = parent_fake_result_decl;
      else
	this_fake_result_decl = current_fake_result_decl;

      if (decl && sym->ns->proc_name->attr.mixed_entry_master)
	{
	  tree field;

	  for (field = TYPE_FIELDS (TREE_TYPE (decl));
	       field; field = TREE_CHAIN (field))
	    if (strcmp (IDENTIFIER_POINTER (DECL_NAME (field)),
		sym->name) == 0)
	      break;

	  gcc_assert (field != NULL_TREE);
	  decl = build3 (COMPONENT_REF, TREE_TYPE (field), decl, field,
			 NULL_TREE);
	}

      var = create_tmp_var_raw (TREE_TYPE (decl), sym->name);
      if (parent_flag)
	gfc_add_decl_to_parent_function (var);
      else
	gfc_add_decl_to_function (var);

      SET_DECL_VALUE_EXPR (var, decl);
      DECL_HAS_VALUE_EXPR_P (var) = 1;
      GFC_DECL_RESULT (var) = 1;

      TREE_CHAIN (this_fake_result_decl)
	  = tree_cons (get_identifier (sym->name), var,
		       TREE_CHAIN (this_fake_result_decl));
      return var;
    }

  if (this_fake_result_decl != NULL_TREE)
    return TREE_VALUE (this_fake_result_decl);

  /* Only when gfc_get_fake_result_decl is called by gfc_trans_return,
     sym is NULL.  */
  if (!sym)
    return NULL_TREE;

  if (sym->ts.type == BT_CHARACTER)
    {
      if (sym->ts.cl->backend_decl == NULL_TREE)
	length = gfc_create_string_length (sym);
      else
	length = sym->ts.cl->backend_decl;
      if (TREE_CODE (length) == VAR_DECL
	  && DECL_CONTEXT (length) == NULL_TREE)
	gfc_add_decl_to_function (length);
    }

  if (gfc_return_by_reference (sym))
    {
      decl = DECL_ARGUMENTS (this_function_decl);

      if (sym->ns->proc_name->backend_decl == this_function_decl
	  && sym->ns->proc_name->attr.entry_master)
	decl = TREE_CHAIN (decl);

      TREE_USED (decl) = 1;
      if (sym->as)
	decl = gfc_build_dummy_array_decl (sym, decl);
    }
  else
    {
      sprintf (name, "__result_%.20s",
	       IDENTIFIER_POINTER (DECL_NAME (this_function_decl)));

      if (!sym->attr.mixed_entry_master && sym->attr.function)
	decl = build_decl (VAR_DECL, get_identifier (name),
			   gfc_sym_type (sym));
      else
	decl = build_decl (VAR_DECL, get_identifier (name),
			   TREE_TYPE (TREE_TYPE (this_function_decl)));
      DECL_ARTIFICIAL (decl) = 1;
      DECL_EXTERNAL (decl) = 0;
      TREE_PUBLIC (decl) = 0;
      TREE_USED (decl) = 1;
      GFC_DECL_RESULT (decl) = 1;
      TREE_ADDRESSABLE (decl) = 1;

      layout_decl (decl, 0);

      if (parent_flag)
	gfc_add_decl_to_parent_function (decl);
      else
	gfc_add_decl_to_function (decl);
    }

  if (parent_flag)
    parent_fake_result_decl = build_tree_list (NULL, decl);
  else
    current_fake_result_decl = build_tree_list (NULL, decl);

  return decl;
}


/* Builds a function decl.  The remaining parameters are the types of the
   function arguments.  Negative nargs indicates a varargs function.  */

tree
gfc_build_library_function_decl (tree name, tree rettype, int nargs, ...)
{
  tree arglist;
  tree argtype;
  tree fntype;
  tree fndecl;
  va_list p;
  int n;

  /* Library functions must be declared with global scope.  */
  gcc_assert (current_function_decl == NULL_TREE);

  va_start (p, nargs);


  /* Create a list of the argument types.  */
  for (arglist = NULL_TREE, n = abs (nargs); n > 0; n--)
    {
      argtype = va_arg (p, tree);
      arglist = gfc_chainon_list (arglist, argtype);
    }

  if (nargs >= 0)
    {
      /* Terminate the list.  */
      arglist = gfc_chainon_list (arglist, void_type_node);
    }

  /* Build the function type and decl.  */
  fntype = build_function_type (rettype, arglist);
  fndecl = build_decl (FUNCTION_DECL, name, fntype);

  /* Mark this decl as external.  */
  DECL_EXTERNAL (fndecl) = 1;
  TREE_PUBLIC (fndecl) = 1;

  va_end (p);

  pushdecl (fndecl);

  rest_of_decl_compilation (fndecl, 1, 0);

  return fndecl;
}

static void
gfc_build_intrinsic_function_decls (void)
{
  tree gfc_int4_type_node = gfc_get_int_type (4);
  tree gfc_int8_type_node = gfc_get_int_type (8);
  tree gfc_int16_type_node = gfc_get_int_type (16);
  tree gfc_logical4_type_node = gfc_get_logical_type (4);
  tree gfc_real4_type_node = gfc_get_real_type (4);
  tree gfc_real8_type_node = gfc_get_real_type (8);
  tree gfc_real10_type_node = gfc_get_real_type (10);
  tree gfc_real16_type_node = gfc_get_real_type (16);
  tree gfc_complex4_type_node = gfc_get_complex_type (4);
  tree gfc_complex8_type_node = gfc_get_complex_type (8);
  tree gfc_complex10_type_node = gfc_get_complex_type (10);
  tree gfc_complex16_type_node = gfc_get_complex_type (16);
  tree gfc_c_int_type_node = gfc_get_int_type (gfc_c_int_kind);

  /* String functions.  */
  gfor_fndecl_compare_string =
    gfc_build_library_function_decl (get_identifier (PREFIX("compare_string")),
				     gfc_int4_type_node,
				     4,
				     gfc_charlen_type_node, pchar_type_node,
				     gfc_charlen_type_node, pchar_type_node);

  gfor_fndecl_concat_string =
    gfc_build_library_function_decl (get_identifier (PREFIX("concat_string")),
				     void_type_node,
				     6,
				     gfc_charlen_type_node, pchar_type_node,
				     gfc_charlen_type_node, pchar_type_node,
				     gfc_charlen_type_node, pchar_type_node);

  gfor_fndecl_string_len_trim =
    gfc_build_library_function_decl (get_identifier (PREFIX("string_len_trim")),
				     gfc_int4_type_node,
				     2, gfc_charlen_type_node,
				     pchar_type_node);

  gfor_fndecl_string_index =
    gfc_build_library_function_decl (get_identifier (PREFIX("string_index")),
				     gfc_int4_type_node,
				     5, gfc_charlen_type_node, pchar_type_node,
				     gfc_charlen_type_node, pchar_type_node,
                                     gfc_logical4_type_node);

  gfor_fndecl_string_scan =
    gfc_build_library_function_decl (get_identifier (PREFIX("string_scan")),
                                     gfc_int4_type_node,
                                     5, gfc_charlen_type_node, pchar_type_node,
                                     gfc_charlen_type_node, pchar_type_node,
                                     gfc_logical4_type_node);

  gfor_fndecl_string_verify =
    gfc_build_library_function_decl (get_identifier (PREFIX("string_verify")),
                                     gfc_int4_type_node,
                                     5, gfc_charlen_type_node, pchar_type_node,
                                     gfc_charlen_type_node, pchar_type_node,
                                     gfc_logical4_type_node);

  gfor_fndecl_string_trim = 
    gfc_build_library_function_decl (get_identifier (PREFIX("string_trim")),
                                     void_type_node,
                                     4,
                                     build_pointer_type (gfc_charlen_type_node),
                                     ppvoid_type_node,
                                     gfc_charlen_type_node,
                                     pchar_type_node);

  gfor_fndecl_string_repeat =
    gfc_build_library_function_decl (get_identifier (PREFIX("string_repeat")),
                                     void_type_node,
                                     4,
                                     pchar_type_node,
                                     gfc_charlen_type_node,
                                     pchar_type_node,
                                     gfc_int4_type_node);

  gfor_fndecl_ttynam =
    gfc_build_library_function_decl (get_identifier (PREFIX("ttynam")),
                                     void_type_node,
                                     3,
                                     pchar_type_node,
                                     gfc_charlen_type_node,
                                     gfc_c_int_type_node);

  gfor_fndecl_fdate =
    gfc_build_library_function_decl (get_identifier (PREFIX("fdate")),
                                     void_type_node,
                                     2,
                                     pchar_type_node,
                                     gfc_charlen_type_node);

  gfor_fndecl_ctime =
    gfc_build_library_function_decl (get_identifier (PREFIX("ctime")),
                                     void_type_node,
                                     3,
                                     pchar_type_node,
                                     gfc_charlen_type_node,
                                     gfc_int8_type_node);

  gfor_fndecl_adjustl =
    gfc_build_library_function_decl (get_identifier (PREFIX("adjustl")),
				     void_type_node,
				     3,
				     pchar_type_node,
				     gfc_charlen_type_node, pchar_type_node);

  gfor_fndecl_adjustr =
    gfc_build_library_function_decl (get_identifier (PREFIX("adjustr")),
				     void_type_node,
				     3,
				     pchar_type_node,
				     gfc_charlen_type_node, pchar_type_node);

  gfor_fndecl_si_kind =
    gfc_build_library_function_decl (get_identifier ("selected_int_kind"),
                                     gfc_int4_type_node,
                                     1,
                                     pvoid_type_node);

  gfor_fndecl_sr_kind =
    gfc_build_library_function_decl (get_identifier ("selected_real_kind"),
                                     gfc_int4_type_node,
                                     2, pvoid_type_node,
                                     pvoid_type_node);

  /* Power functions.  */
  {
    tree ctype, rtype, itype, jtype;
    int rkind, ikind, jkind;
#define NIKINDS 3
#define NRKINDS 4
    static int ikinds[NIKINDS] = {4, 8, 16};
    static int rkinds[NRKINDS] = {4, 8, 10, 16};
    char name[PREFIX_LEN + 12]; /* _gfortran_pow_?n_?n */

    for (ikind=0; ikind < NIKINDS; ikind++)
      {
	itype = gfc_get_int_type (ikinds[ikind]);

	for (jkind=0; jkind < NIKINDS; jkind++)
	  {
	    jtype = gfc_get_int_type (ikinds[jkind]);
	    if (itype && jtype)
	      {
		sprintf(name, PREFIX("pow_i%d_i%d"), ikinds[ikind],
			ikinds[jkind]);
		gfor_fndecl_math_powi[jkind][ikind].integer =
		  gfc_build_library_function_decl (get_identifier (name),
		    jtype, 2, jtype, itype);
	      }
	  }

	for (rkind = 0; rkind < NRKINDS; rkind ++)
	  {
	    rtype = gfc_get_real_type (rkinds[rkind]);
	    if (rtype && itype)
	      {
		sprintf(name, PREFIX("pow_r%d_i%d"), rkinds[rkind],
			ikinds[ikind]);
		gfor_fndecl_math_powi[rkind][ikind].real =
		  gfc_build_library_function_decl (get_identifier (name),
		    rtype, 2, rtype, itype);
	      }

	    ctype = gfc_get_complex_type (rkinds[rkind]);
	    if (ctype && itype)
	      {
		sprintf(name, PREFIX("pow_c%d_i%d"), rkinds[rkind],
			ikinds[ikind]);
		gfor_fndecl_math_powi[rkind][ikind].cmplx =
		  gfc_build_library_function_decl (get_identifier (name),
		    ctype, 2,ctype, itype);
	      }
	  }
      }
#undef NIKINDS
#undef NRKINDS
  }

  gfor_fndecl_math_cpowf =
    gfc_build_library_function_decl (get_identifier ("cpowf"),
				     gfc_complex4_type_node,
				     1, gfc_complex4_type_node);
  gfor_fndecl_math_cpow =
    gfc_build_library_function_decl (get_identifier ("cpow"),
				     gfc_complex8_type_node,
				     1, gfc_complex8_type_node);
  if (gfc_complex10_type_node)
    gfor_fndecl_math_cpowl10 =
      gfc_build_library_function_decl (get_identifier ("cpowl"),
				       gfc_complex10_type_node, 1,
				       gfc_complex10_type_node);
  if (gfc_complex16_type_node)
    gfor_fndecl_math_cpowl16 =
      gfc_build_library_function_decl (get_identifier ("cpowl"),
				       gfc_complex16_type_node, 1,
				       gfc_complex16_type_node);

  gfor_fndecl_math_ishftc4 =
    gfc_build_library_function_decl (get_identifier (PREFIX("ishftc4")),
				     gfc_int4_type_node,
				     3, gfc_int4_type_node,
				     gfc_int4_type_node, gfc_int4_type_node);
  gfor_fndecl_math_ishftc8 =
    gfc_build_library_function_decl (get_identifier (PREFIX("ishftc8")),
				     gfc_int8_type_node,
				     3, gfc_int8_type_node,
				     gfc_int4_type_node, gfc_int4_type_node);
  if (gfc_int16_type_node)
    gfor_fndecl_math_ishftc16 =
      gfc_build_library_function_decl (get_identifier (PREFIX("ishftc16")),
				       gfc_int16_type_node, 3,
				       gfc_int16_type_node,
				       gfc_int4_type_node,
				       gfc_int4_type_node);

  gfor_fndecl_math_exponent4 =
    gfc_build_library_function_decl (get_identifier (PREFIX("exponent_r4")),
				     gfc_int4_type_node,
				     1, gfc_real4_type_node);
  gfor_fndecl_math_exponent8 =
    gfc_build_library_function_decl (get_identifier (PREFIX("exponent_r8")),
				     gfc_int4_type_node,
				     1, gfc_real8_type_node);
  if (gfc_real10_type_node)
    gfor_fndecl_math_exponent10 =
      gfc_build_library_function_decl (get_identifier (PREFIX("exponent_r10")),
				       gfc_int4_type_node, 1,
				       gfc_real10_type_node);
  if (gfc_real16_type_node)
    gfor_fndecl_math_exponent16 =
      gfc_build_library_function_decl (get_identifier (PREFIX("exponent_r16")),
				       gfc_int4_type_node, 1,
				       gfc_real16_type_node);

  /* Other functions.  */
  gfor_fndecl_size0 =
    gfc_build_library_function_decl (get_identifier (PREFIX("size0")),
				     gfc_array_index_type,
				     1, pvoid_type_node);
  gfor_fndecl_size1 =
    gfc_build_library_function_decl (get_identifier (PREFIX("size1")),
				     gfc_array_index_type,
				     2, pvoid_type_node,
				     gfc_array_index_type);

  gfor_fndecl_iargc =
    gfc_build_library_function_decl (get_identifier (PREFIX ("iargc")),
				     gfc_int4_type_node,
				     0);
}


/* Make prototypes for runtime library functions.  */

void
gfc_build_builtin_function_decls (void)
{
  tree gfc_c_int_type_node = gfc_get_int_type (gfc_c_int_kind);
  tree gfc_int4_type_node = gfc_get_int_type (4);
  tree gfc_int8_type_node = gfc_get_int_type (8);
  tree gfc_logical4_type_node = gfc_get_logical_type (4);
  tree gfc_pint4_type_node = build_pointer_type (gfc_int4_type_node);

  /* Treat these two internal malloc wrappers as malloc.  */
  gfor_fndecl_internal_malloc =
    gfc_build_library_function_decl (get_identifier (PREFIX("internal_malloc")),
				     pvoid_type_node, 1, gfc_int4_type_node);
  DECL_IS_MALLOC (gfor_fndecl_internal_malloc) = 1;

  gfor_fndecl_internal_malloc64 =
    gfc_build_library_function_decl (get_identifier
				     (PREFIX("internal_malloc64")),
				     pvoid_type_node, 1, gfc_int8_type_node);
  DECL_IS_MALLOC (gfor_fndecl_internal_malloc64) = 1;

  gfor_fndecl_internal_realloc =
    gfc_build_library_function_decl (get_identifier
				     (PREFIX("internal_realloc")),
				     pvoid_type_node, 2, pvoid_type_node,
				     gfc_int4_type_node);

  gfor_fndecl_internal_realloc64 =
    gfc_build_library_function_decl (get_identifier
				     (PREFIX("internal_realloc64")),
				     pvoid_type_node, 2, pvoid_type_node,
				     gfc_int8_type_node);

  gfor_fndecl_internal_free =
    gfc_build_library_function_decl (get_identifier (PREFIX("internal_free")),
				     void_type_node, 1, pvoid_type_node);

  gfor_fndecl_allocate =
    gfc_build_library_function_decl (get_identifier (PREFIX("allocate")),
				     pvoid_type_node, 2,
				     gfc_int4_type_node, gfc_pint4_type_node);
  DECL_IS_MALLOC (gfor_fndecl_allocate) = 1;

  gfor_fndecl_allocate64 =
    gfc_build_library_function_decl (get_identifier (PREFIX("allocate64")),
				     pvoid_type_node, 2,
				     gfc_int8_type_node, gfc_pint4_type_node);
  DECL_IS_MALLOC (gfor_fndecl_allocate64) = 1;

  gfor_fndecl_allocate_array =
    gfc_build_library_function_decl (get_identifier (PREFIX("allocate_array")),
				     pvoid_type_node, 3, pvoid_type_node,
				     gfc_int4_type_node, gfc_pint4_type_node);
  DECL_IS_MALLOC (gfor_fndecl_allocate_array) = 1;

  gfor_fndecl_allocate64_array =
    gfc_build_library_function_decl (get_identifier (PREFIX("allocate64_array")),
				     pvoid_type_node, 3, pvoid_type_node,
				     gfc_int8_type_node, gfc_pint4_type_node);
  DECL_IS_MALLOC (gfor_fndecl_allocate64_array) = 1;

  gfor_fndecl_deallocate =
    gfc_build_library_function_decl (get_identifier (PREFIX("deallocate")),
				     void_type_node, 2, pvoid_type_node,
				     gfc_pint4_type_node);

  gfor_fndecl_stop_numeric =
    gfc_build_library_function_decl (get_identifier (PREFIX("stop_numeric")),
				     void_type_node, 1, gfc_int4_type_node);

  /* Stop doesn't return.  */
  TREE_THIS_VOLATILE (gfor_fndecl_stop_numeric) = 1;

  gfor_fndecl_stop_string =
    gfc_build_library_function_decl (get_identifier (PREFIX("stop_string")),
				     void_type_node, 2, pchar_type_node,
                                     gfc_int4_type_node);
  /* Stop doesn't return.  */
  TREE_THIS_VOLATILE (gfor_fndecl_stop_string) = 1;

  gfor_fndecl_pause_numeric =
    gfc_build_library_function_decl (get_identifier (PREFIX("pause_numeric")),
				     void_type_node, 1, gfc_int4_type_node);

  gfor_fndecl_pause_string =
    gfc_build_library_function_decl (get_identifier (PREFIX("pause_string")),
				     void_type_node, 2, pchar_type_node,
                                     gfc_int4_type_node);

  gfor_fndecl_select_string =
    gfc_build_library_function_decl (get_identifier (PREFIX("select_string")),
                                     pvoid_type_node, 0);

  gfor_fndecl_runtime_error =
    gfc_build_library_function_decl (get_identifier (PREFIX("runtime_error")),
				     void_type_node, 1, pchar_type_node);
  /* The runtime_error function does not return.  */
  TREE_THIS_VOLATILE (gfor_fndecl_runtime_error) = 1;

  gfor_fndecl_set_fpe =
    gfc_build_library_function_decl (get_identifier (PREFIX("set_fpe")),
				    void_type_node, 1, gfc_c_int_type_node);

  gfor_fndecl_set_std =
    gfc_build_library_function_decl (get_identifier (PREFIX("set_std")),
				    void_type_node,
				    3,
				    gfc_int4_type_node,
				    gfc_int4_type_node,
				    gfc_int4_type_node);

  gfor_fndecl_set_convert =
    gfc_build_library_function_decl (get_identifier (PREFIX("set_convert")),
				     void_type_node, 1, gfc_c_int_type_node);

  gfor_fndecl_set_record_marker =
    gfc_build_library_function_decl (get_identifier (PREFIX("set_record_marker")),
				     void_type_node, 1, gfc_c_int_type_node);

  gfor_fndecl_set_max_subrecord_length =
    gfc_build_library_function_decl (get_identifier (PREFIX("set_max_subrecord_length")),
				     void_type_node, 1, gfc_c_int_type_node);

  gfor_fndecl_in_pack = gfc_build_library_function_decl (
        get_identifier (PREFIX("internal_pack")),
        pvoid_type_node, 1, pvoid_type_node);

  gfor_fndecl_in_unpack = gfc_build_library_function_decl (
        get_identifier (PREFIX("internal_unpack")),
        pvoid_type_node, 1, pvoid_type_node);

  gfor_fndecl_associated =
    gfc_build_library_function_decl (
                                     get_identifier (PREFIX("associated")),
                                     gfc_logical4_type_node,
                                     2,
                                     ppvoid_type_node,
                                     ppvoid_type_node);

  gfc_build_intrinsic_function_decls ();
  gfc_build_intrinsic_lib_fndecls ();
  gfc_build_io_library_fndecls ();
}


/* Evaluate the length of dummy character variables.  */

static tree
gfc_trans_dummy_character (gfc_symbol *sym, gfc_charlen *cl, tree fnbody)
{
  stmtblock_t body;

  gfc_finish_decl (cl->backend_decl, NULL_TREE);

  gfc_start_block (&body);

  /* Evaluate the string length expression.  */
  gfc_trans_init_string_length (cl, &body);

  gfc_trans_vla_type_sizes (sym, &body);

  gfc_add_expr_to_block (&body, fnbody);
  return gfc_finish_block (&body);
}


/* Allocate and cleanup an automatic character variable.  */

static tree
gfc_trans_auto_character_variable (gfc_symbol * sym, tree fnbody)
{
  stmtblock_t body;
  tree decl;
  tree tmp;

  gcc_assert (sym->backend_decl);
  gcc_assert (sym->ts.cl && sym->ts.cl->length);

  gfc_start_block (&body);

  /* Evaluate the string length expression.  */
  gfc_trans_init_string_length (sym->ts.cl, &body);

  gfc_trans_vla_type_sizes (sym, &body);

  decl = sym->backend_decl;

  /* Emit a DECL_EXPR for this variable, which will cause the
     gimplifier to allocate storage, and all that good stuff.  */
  tmp = build1 (DECL_EXPR, TREE_TYPE (decl), decl);
  gfc_add_expr_to_block (&body, tmp);

  gfc_add_expr_to_block (&body, fnbody);
  return gfc_finish_block (&body);
}

/* Set the initial value of ASSIGN statement auxiliary variable explicitly.  */

static tree
gfc_trans_assign_aux_var (gfc_symbol * sym, tree fnbody)
{
  stmtblock_t body;

  gcc_assert (sym->backend_decl);
  gfc_start_block (&body);

  /* Set the initial value to length. See the comments in
     function gfc_add_assign_aux_vars in this file.  */
  gfc_add_modify_expr (&body, GFC_DECL_STRING_LEN (sym->backend_decl),
		       build_int_cst (NULL_TREE, -2));

  gfc_add_expr_to_block (&body, fnbody);
  return gfc_finish_block (&body);
}

static void
gfc_trans_vla_one_sizepos (tree *tp, stmtblock_t *body)
{
  tree t = *tp, var, val;

  if (t == NULL || t == error_mark_node)
    return;
  if (TREE_CONSTANT (t) || DECL_P (t))
    return;

  if (TREE_CODE (t) == SAVE_EXPR)
    {
      if (SAVE_EXPR_RESOLVED_P (t))
	{
	  *tp = TREE_OPERAND (t, 0);
	  return;
	}
      val = TREE_OPERAND (t, 0);
    }
  else
    val = t;

  var = gfc_create_var_np (TREE_TYPE (t), NULL);
  gfc_add_decl_to_function (var);
  gfc_add_modify_expr (body, var, val);
  if (TREE_CODE (t) == SAVE_EXPR)
    TREE_OPERAND (t, 0) = var;
  *tp = var;
}

static void
gfc_trans_vla_type_sizes_1 (tree type, stmtblock_t *body)
{
  tree t;

  if (type == NULL || type == error_mark_node)
    return;

  type = TYPE_MAIN_VARIANT (type);

  if (TREE_CODE (type) == INTEGER_TYPE)
    {
      gfc_trans_vla_one_sizepos (&TYPE_MIN_VALUE (type), body);
      gfc_trans_vla_one_sizepos (&TYPE_MAX_VALUE (type), body);

      for (t = TYPE_NEXT_VARIANT (type); t; t = TYPE_NEXT_VARIANT (t))
	{
	  TYPE_MIN_VALUE (t) = TYPE_MIN_VALUE (type);
	  TYPE_MAX_VALUE (t) = TYPE_MAX_VALUE (type);
	}
    }
  else if (TREE_CODE (type) == ARRAY_TYPE)
    {
      gfc_trans_vla_type_sizes_1 (TREE_TYPE (type), body);
      gfc_trans_vla_type_sizes_1 (TYPE_DOMAIN (type), body);
      gfc_trans_vla_one_sizepos (&TYPE_SIZE (type), body);
      gfc_trans_vla_one_sizepos (&TYPE_SIZE_UNIT (type), body);

      for (t = TYPE_NEXT_VARIANT (type); t; t = TYPE_NEXT_VARIANT (t))
	{
	  TYPE_SIZE (t) = TYPE_SIZE (type);
	  TYPE_SIZE_UNIT (t) = TYPE_SIZE_UNIT (type);
	}
    }
}

/* Make sure all type sizes and array domains are either constant,
   or variable or parameter decls.  This is a simplified variant
   of gimplify_type_sizes, but we can't use it here, as none of the
   variables in the expressions have been gimplified yet.
   As type sizes and domains for various variable length arrays
   contain VAR_DECLs that are only initialized at gfc_trans_deferred_vars
   time, without this routine gimplify_type_sizes in the middle-end
   could result in the type sizes being gimplified earlier than where
   those variables are initialized.  */

void
gfc_trans_vla_type_sizes (gfc_symbol *sym, stmtblock_t *body)
{
  tree type = TREE_TYPE (sym->backend_decl);

  if (TREE_CODE (type) == FUNCTION_TYPE
      && (sym->attr.function || sym->attr.result || sym->attr.entry))
    {
      if (! current_fake_result_decl)
	return;

      type = TREE_TYPE (TREE_VALUE (current_fake_result_decl));
    }

  while (POINTER_TYPE_P (type))
    type = TREE_TYPE (type);

  if (GFC_DESCRIPTOR_TYPE_P (type))
    {
      tree etype = GFC_TYPE_ARRAY_DATAPTR_TYPE (type);

      while (POINTER_TYPE_P (etype))
	etype = TREE_TYPE (etype);

      gfc_trans_vla_type_sizes_1 (etype, body);
    }

  gfc_trans_vla_type_sizes_1 (type, body);
}


/* Generate function entry and exit code, and add it to the function body.
   This includes:
    Allocation and initialization of array variables.
    Allocation of character string variables.
    Initialization and possibly repacking of dummy arrays.
    Initialization of ASSIGN statement auxiliary variable.  */

static tree
gfc_trans_deferred_vars (gfc_symbol * proc_sym, tree fnbody)
{
  locus loc;
  gfc_symbol *sym;
  gfc_formal_arglist *f;
  stmtblock_t body;
  bool seen_trans_deferred_array = false;

  /* Deal with implicit return variables.  Explicit return variables will
     already have been added.  */
  if (gfc_return_by_reference (proc_sym) && proc_sym->result == proc_sym)
    {
      if (!current_fake_result_decl)
	{
	  gfc_entry_list *el = NULL;
	  if (proc_sym->attr.entry_master)
	    {
	      for (el = proc_sym->ns->entries; el; el = el->next)
		if (el->sym != el->sym->result)
		  break;
	    }
	  if (el == NULL)
	    warning (0, "Function does not return a value");
	}
      else if (proc_sym->as)
	{
	  tree result = TREE_VALUE (current_fake_result_decl);
	  fnbody = gfc_trans_dummy_array_bias (proc_sym, result, fnbody);

	  /* An automatic character length, pointer array result.  */
	  if (proc_sym->ts.type == BT_CHARACTER
		&& TREE_CODE (proc_sym->ts.cl->backend_decl) == VAR_DECL)
	    fnbody = gfc_trans_dummy_character (proc_sym, proc_sym->ts.cl,
						fnbody);
	}
      else if (proc_sym->ts.type == BT_CHARACTER)
	{
	  if (TREE_CODE (proc_sym->ts.cl->backend_decl) == VAR_DECL)
	    fnbody = gfc_trans_dummy_character (proc_sym, proc_sym->ts.cl,
						fnbody);
	}
      else
	gcc_assert (gfc_option.flag_f2c
		    && proc_sym->ts.type == BT_COMPLEX);
    }

  for (sym = proc_sym->tlink; sym != proc_sym; sym = sym->tlink)
    {
      bool sym_has_alloc_comp = (sym->ts.type == BT_DERIVED)
				   && sym->ts.derived->attr.alloc_comp;
      if (sym->attr.dimension)
	{
	  switch (sym->as->type)
	    {
	    case AS_EXPLICIT:
	      if (sym->attr.dummy || sym->attr.result)
		fnbody =
		  gfc_trans_dummy_array_bias (sym, sym->backend_decl, fnbody);
	      else if (sym->attr.pointer || sym->attr.allocatable)
		{
		  if (TREE_STATIC (sym->backend_decl))
		    gfc_trans_static_array_pointer (sym);
		  else
		    {
		      seen_trans_deferred_array = true;
		      fnbody = gfc_trans_deferred_array (sym, fnbody);
		    }
		}
	      else
		{
		  if (sym_has_alloc_comp)
		    {
		      seen_trans_deferred_array = true;
		      fnbody = gfc_trans_deferred_array (sym, fnbody);
		    }

		  gfc_get_backend_locus (&loc);
		  gfc_set_backend_locus (&sym->declared_at);
		  fnbody = gfc_trans_auto_array_allocation (sym->backend_decl,
		      sym, fnbody);
		  gfc_set_backend_locus (&loc);
		}
	      break;

	    case AS_ASSUMED_SIZE:
	      /* Must be a dummy parameter.  */
	      gcc_assert (sym->attr.dummy);

	      /* We should always pass assumed size arrays the g77 way.  */
	      fnbody = gfc_trans_g77_array (sym, fnbody);
              break;

	    case AS_ASSUMED_SHAPE:
	      /* Must be a dummy parameter.  */
	      gcc_assert (sym->attr.dummy);

	      fnbody = gfc_trans_dummy_array_bias (sym, sym->backend_decl,
						   fnbody);
	      break;

	    case AS_DEFERRED:
	      seen_trans_deferred_array = true;
	      fnbody = gfc_trans_deferred_array (sym, fnbody);
	      break;

	    default:
	      gcc_unreachable ();
	    }
	  if (sym_has_alloc_comp && !seen_trans_deferred_array)
	    fnbody = gfc_trans_deferred_array (sym, fnbody);
	}
      else if (sym_has_alloc_comp)
	fnbody = gfc_trans_deferred_array (sym, fnbody);
      else if (sym->ts.type == BT_CHARACTER)
	{
	  gfc_get_backend_locus (&loc);
	  gfc_set_backend_locus (&sym->declared_at);
	  if (sym->attr.dummy || sym->attr.result)
	    fnbody = gfc_trans_dummy_character (sym, sym->ts.cl, fnbody);
	  else
	    fnbody = gfc_trans_auto_character_variable (sym, fnbody);
	  gfc_set_backend_locus (&loc);
	}
      else if (sym->attr.assign)
	{
	  gfc_get_backend_locus (&loc);
	  gfc_set_backend_locus (&sym->declared_at);
	  fnbody = gfc_trans_assign_aux_var (sym, fnbody);
	  gfc_set_backend_locus (&loc);
	}
      else
	gcc_unreachable ();
    }

  gfc_init_block (&body);

  for (f = proc_sym->formal; f; f = f->next)
    if (f->sym && f->sym->tlink == NULL && f->sym->ts.type == BT_CHARACTER)
      {
	gcc_assert (f->sym->ts.cl->backend_decl != NULL);
	if (TREE_CODE (f->sym->ts.cl->backend_decl) == PARM_DECL)
	  gfc_trans_vla_type_sizes (f->sym, &body);
      }

  if (gfc_return_by_reference (proc_sym) && proc_sym->ts.type == BT_CHARACTER
      && current_fake_result_decl != NULL)
    {
      gcc_assert (proc_sym->ts.cl->backend_decl != NULL);
      if (TREE_CODE (proc_sym->ts.cl->backend_decl) == PARM_DECL)
	gfc_trans_vla_type_sizes (proc_sym, &body);
    }

  gfc_add_expr_to_block (&body, fnbody);
  return gfc_finish_block (&body);
}


/* Output an initialized decl for a module variable.  */

static void
gfc_create_module_variable (gfc_symbol * sym)
{
  tree decl;

  /* Module functions with alternate entries are dealt with later and
     would get caught by the next condition.  */
  if (sym->attr.entry)
    return;

  /* Only output symbols from this module.  */
  if (sym->ns != module_namespace)
    {
      /* I don't think this should ever happen.  */
      internal_error ("module symbol %s in wrong namespace", sym->name);
    }

  /* Only output variables and array valued parameters.  */
  if (sym->attr.flavor != FL_VARIABLE
      && (sym->attr.flavor != FL_PARAMETER || sym->attr.dimension == 0))
    return;

  /* Don't generate variables from other modules. Variables from
     COMMONs will already have been generated.  */
  if (sym->attr.use_assoc || sym->attr.in_common)
    return;

  /* Equivalenced variables arrive here after creation.  */
  if (sym->backend_decl
	&& (sym->equiv_built || sym->attr.in_equivalence))
      return;

  if (sym->backend_decl)
    internal_error ("backend decl for module variable %s already exists",
		    sym->name);

  /* We always want module variables to be created.  */
  sym->attr.referenced = 1;
  /* Create the decl.  */
  decl = gfc_get_symbol_decl (sym);

  /* Create the variable.  */
  pushdecl (decl);
  rest_of_decl_compilation (decl, 1, 0);

  /* Also add length of strings.  */
  if (sym->ts.type == BT_CHARACTER)
    {
      tree length;

      length = sym->ts.cl->backend_decl;
      if (!INTEGER_CST_P (length))
        {
          pushdecl (length);
          rest_of_decl_compilation (length, 1, 0);
        }
    }
}


/* Generate all the required code for module variables.  */

void
gfc_generate_module_vars (gfc_namespace * ns)
{
  module_namespace = ns;

  /* Check if the frontend left the namespace in a reasonable state.  */
  gcc_assert (ns->proc_name && !ns->proc_name->tlink);

  /* Generate COMMON blocks.  */
  gfc_trans_common (ns);

  /* Create decls for all the module variables.  */
  gfc_traverse_ns (ns, gfc_create_module_variable);
}

static void
gfc_generate_contained_functions (gfc_namespace * parent)
{
  gfc_namespace *ns;

  /* We create all the prototypes before generating any code.  */
  for (ns = parent->contained; ns; ns = ns->sibling)
    {
      /* Skip namespaces from used modules.  */
      if (ns->parent != parent)
	continue;

      gfc_create_function_decl (ns);
    }

  for (ns = parent->contained; ns; ns = ns->sibling)
    {
      /* Skip namespaces from used modules.  */
      if (ns->parent != parent)
	continue;

      gfc_generate_function_code (ns);
    }
}


/* Drill down through expressions for the array specification bounds and
   character length calling generate_local_decl for all those variables
   that have not already been declared.  */

static void
generate_local_decl (gfc_symbol *);

static void
generate_expr_decls (gfc_symbol *sym, gfc_expr *e)
{
  gfc_actual_arglist *arg;
  gfc_ref *ref;
  int i;

  if (e == NULL)
    return;

  switch (e->expr_type)
    {
    case EXPR_FUNCTION:
      for (arg = e->value.function.actual; arg; arg = arg->next)
	generate_expr_decls (sym, arg->expr);
      break;

    /* If the variable is not the same as the dependent, 'sym', and
       it is not marked as being declared and it is in the same
       namespace as 'sym', add it to the local declarations.  */
    case EXPR_VARIABLE:
      if (sym == e->symtree->n.sym
	    || e->symtree->n.sym->mark
	    || e->symtree->n.sym->ns != sym->ns)
	return;

      generate_local_decl (e->symtree->n.sym);
      break;

    case EXPR_OP:
      generate_expr_decls (sym, e->value.op.op1);
      generate_expr_decls (sym, e->value.op.op2);
      break;

    default:
      break;
    }

  if (e->ref)
    {
      for (ref = e->ref; ref; ref = ref->next)
	{
	  switch (ref->type)
	    {
	    case REF_ARRAY:
	      for (i = 0; i < ref->u.ar.dimen; i++)
		{
		  generate_expr_decls (sym, ref->u.ar.start[i]);
		  generate_expr_decls (sym, ref->u.ar.end[i]);
		  generate_expr_decls (sym, ref->u.ar.stride[i]);
		}
	      break;

	    case REF_SUBSTRING:
	      generate_expr_decls (sym, ref->u.ss.start);
	      generate_expr_decls (sym, ref->u.ss.end);
	      break;

	    case REF_COMPONENT:
	      if (ref->u.c.component->ts.type == BT_CHARACTER
		    && ref->u.c.component->ts.cl->length->expr_type
						!= EXPR_CONSTANT)
		generate_expr_decls (sym, ref->u.c.component->ts.cl->length);

	      if (ref->u.c.component->as)
	        for (i = 0; i < ref->u.c.component->as->rank; i++)
		  {
		    generate_expr_decls (sym, ref->u.c.component->as->lower[i]);
		    generate_expr_decls (sym, ref->u.c.component->as->upper[i]);
		  }
	      break;
	    }
	}
    }
}


/* Check for dependencies in the character length and array spec. */

static void
generate_dependency_declarations (gfc_symbol *sym)
{
  int i;

  if (sym->ts.type == BT_CHARACTER
	&& sym->ts.cl->length->expr_type != EXPR_CONSTANT)
    generate_expr_decls (sym, sym->ts.cl->length);

  if (sym->as && sym->as->rank)
    {
      for (i = 0; i < sym->as->rank; i++)
	{
          generate_expr_decls (sym, sym->as->lower[i]);
          generate_expr_decls (sym, sym->as->upper[i]);
	}
    }
}


/* Generate decls for all local variables.  We do this to ensure correct
   handling of expressions which only appear in the specification of
   other functions.  */

static void
generate_local_decl (gfc_symbol * sym)
{
  if (sym->attr.flavor == FL_VARIABLE)
    {
      /* Check for dependencies in the array specification and string
	length, adding the necessary declarations to the function.  We
	mark the symbol now, as well as in traverse_ns, to prevent
	getting stuck in a circular dependency.  */
      sym->mark = 1;
      if (!sym->attr.dummy && !sym->ns->proc_name->attr.entry_master)
        generate_dependency_declarations (sym);

      if (sym->attr.referenced)
        gfc_get_symbol_decl (sym);
      else if (sym->attr.dummy && warn_unused_parameter)
	gfc_warning ("Unused parameter %s declared at %L", sym->name,
		     &sym->declared_at);
      /* Warn for unused variables, but not if they're inside a common
	 block or are use-associated.  */
      else if (warn_unused_variable
	       && !(sym->attr.in_common || sym->attr.use_assoc))
	gfc_warning ("Unused variable %s declared at %L", sym->name,
		     &sym->declared_at);
      /* For variable length CHARACTER parameters, the PARM_DECL already
	 references the length variable, so force gfc_get_symbol_decl
	 even when not referenced.  If optimize > 0, it will be optimized
	 away anyway.  But do this only after emitting -Wunused-parameter
	 warning if requested.  */
      if (sym->attr.dummy && ! sym->attr.referenced
	  && sym->ts.type == BT_CHARACTER
	  && sym->ts.cl->backend_decl != NULL
	  && TREE_CODE (sym->ts.cl->backend_decl) == VAR_DECL)
	{
	  sym->attr.referenced = 1;
	  gfc_get_symbol_decl (sym);
	}
    }
}

static void
generate_local_vars (gfc_namespace * ns)
{
  gfc_traverse_ns (ns, generate_local_decl);
}


/* Generate a switch statement to jump to the correct entry point.  Also
   creates the label decls for the entry points.  */

static tree
gfc_trans_entry_master_switch (gfc_entry_list * el)
{
  stmtblock_t block;
  tree label;
  tree tmp;
  tree val;

  gfc_init_block (&block);
  for (; el; el = el->next)
    {
      /* Add the case label.  */
      label = gfc_build_label_decl (NULL_TREE);
      val = build_int_cst (gfc_array_index_type, el->id);
      tmp = build3_v (CASE_LABEL_EXPR, val, NULL_TREE, label);
      gfc_add_expr_to_block (&block, tmp);
      
      /* And jump to the actual entry point.  */
      label = gfc_build_label_decl (NULL_TREE);
      tmp = build1_v (GOTO_EXPR, label);
      gfc_add_expr_to_block (&block, tmp);

      /* Save the label decl.  */
      el->label = label;
    }
  tmp = gfc_finish_block (&block);
  /* The first argument selects the entry point.  */
  val = DECL_ARGUMENTS (current_function_decl);
  tmp = build3_v (SWITCH_EXPR, val, tmp, NULL_TREE);
  return tmp;
}


/* Generate code for a function.  */

void
gfc_generate_function_code (gfc_namespace * ns)
{
  tree fndecl;
  tree old_context;
  tree decl;
  tree tmp;
  tree tmp2;
  stmtblock_t block;
  stmtblock_t body;
  tree result;
  gfc_symbol *sym;
  int rank;

  sym = ns->proc_name;

  /* Check that the frontend isn't still using this.  */
  gcc_assert (sym->tlink == NULL);
  sym->tlink = sym;

  /* Create the declaration for functions with global scope.  */
  if (!sym->backend_decl)
    gfc_create_function_decl (ns);

  fndecl = sym->backend_decl;
  old_context = current_function_decl;

  if (old_context)
    {
      push_function_context ();
      saved_parent_function_decls = saved_function_decls;
      saved_function_decls = NULL_TREE;
    }

  trans_function_start (sym);

  gfc_start_block (&block);

  if (ns->entries && ns->proc_name->ts.type == BT_CHARACTER)
    {
      /* Copy length backend_decls to all entry point result
	 symbols.  */
      gfc_entry_list *el;
      tree backend_decl;

      gfc_conv_const_charlen (ns->proc_name->ts.cl);
      backend_decl = ns->proc_name->result->ts.cl->backend_decl;
      for (el = ns->entries; el; el = el->next)
	el->sym->result->ts.cl->backend_decl = backend_decl;
    }

  /* Translate COMMON blocks.  */
  gfc_trans_common (ns);

  /* Null the parent fake result declaration if this namespace is
     a module function or an external procedures.  */
  if ((ns->parent && ns->parent->proc_name->attr.flavor == FL_MODULE)
	|| ns->parent == NULL)
    parent_fake_result_decl = NULL_TREE;

  gfc_generate_contained_functions (ns);

  generate_local_vars (ns);
  
  /* Keep the parent fake result declaration in module functions
     or external procedures.  */
  if ((ns->parent && ns->parent->proc_name->attr.flavor == FL_MODULE)
	|| ns->parent == NULL)
    current_fake_result_decl = parent_fake_result_decl;
  else
    current_fake_result_decl = NULL_TREE;

  current_function_return_label = NULL;

  /* Now generate the code for the body of this function.  */
  gfc_init_block (&body);

  /* If this is the main program, add a call to set_std to set up the
     runtime library Fortran language standard parameters.  */

  if (sym->attr.is_main_program)
    {
      tree arglist, gfc_int4_type_node;

      gfc_int4_type_node = gfc_get_int_type (4);
      arglist = gfc_chainon_list (NULL_TREE,
				  build_int_cst (gfc_int4_type_node,
						 gfc_option.warn_std));
      arglist = gfc_chainon_list (arglist,
				  build_int_cst (gfc_int4_type_node,
						 gfc_option.allow_std));
      arglist = gfc_chainon_list (arglist,
				  build_int_cst (gfc_int4_type_node,
						 pedantic));
      tmp = build_function_call_expr (gfor_fndecl_set_std, arglist);
      gfc_add_expr_to_block (&body, tmp);
    }

  /* If this is the main program and a -ffpe-trap option was provided,
     add a call to set_fpe so that the library will raise a FPE when
     needed.  */
  if (sym->attr.is_main_program && gfc_option.fpe != 0)
    {
      tree arglist, gfc_c_int_type_node;

      gfc_c_int_type_node = gfc_get_int_type (gfc_c_int_kind);
      arglist = gfc_chainon_list (NULL_TREE,
				  build_int_cst (gfc_c_int_type_node,
						 gfc_option.fpe));
      tmp = build_function_call_expr (gfor_fndecl_set_fpe, arglist);
      gfc_add_expr_to_block (&body, tmp);
    }

  /* If this is the main program and an -fconvert option was provided,
     add a call to set_convert.  */

  if (sym->attr.is_main_program && gfc_option.convert != CONVERT_NATIVE)
    {
      tree arglist, gfc_c_int_type_node;

      gfc_c_int_type_node = gfc_get_int_type (gfc_c_int_kind);
      arglist = gfc_chainon_list (NULL_TREE,
				  build_int_cst (gfc_c_int_type_node,
						 gfc_option.convert));
      tmp = build_function_call_expr (gfor_fndecl_set_convert, arglist);
      gfc_add_expr_to_block (&body, tmp);
    }

  /* If this is the main program and an -frecord-marker option was provided,
     add a call to set_record_marker.  */

  if (sym->attr.is_main_program && gfc_option.record_marker != 0)
    {
      tree arglist, gfc_c_int_type_node;

      gfc_c_int_type_node = gfc_get_int_type (gfc_c_int_kind);
      arglist = gfc_chainon_list (NULL_TREE,
				  build_int_cst (gfc_c_int_type_node,
						 gfc_option.record_marker));
      tmp = build_function_call_expr (gfor_fndecl_set_record_marker, arglist);
      gfc_add_expr_to_block (&body, tmp);

    }

  if (sym->attr.is_main_program && gfc_option.max_subrecord_length != 0)
    {
      tree arglist, gfc_c_int_type_node;

      gfc_c_int_type_node = gfc_get_int_type (gfc_c_int_kind);
      arglist = gfc_chainon_list (NULL_TREE,
				  build_int_cst (gfc_c_int_type_node,
						 gfc_option.max_subrecord_length));
      tmp = build_function_call_expr (gfor_fndecl_set_max_subrecord_length, arglist);
      gfc_add_expr_to_block (&body, tmp);
    }

  if (TREE_TYPE (DECL_RESULT (fndecl)) != void_type_node
      && sym->attr.subroutine)
    {
      tree alternate_return;
      alternate_return = gfc_get_fake_result_decl (sym, 0);
      gfc_add_modify_expr (&body, alternate_return, integer_zero_node);
    }

  if (ns->entries)
    {
      /* Jump to the correct entry point.  */
      tmp = gfc_trans_entry_master_switch (ns->entries);
      gfc_add_expr_to_block (&body, tmp);
    }

  tmp = gfc_trans_code (ns->code);
  gfc_add_expr_to_block (&body, tmp);

  /* Add a return label if needed.  */
  if (current_function_return_label)
    {
      tmp = build1_v (LABEL_EXPR, current_function_return_label);
      gfc_add_expr_to_block (&body, tmp);
    }

  tmp = gfc_finish_block (&body);
  /* Add code to create and cleanup arrays.  */
  tmp = gfc_trans_deferred_vars (sym, tmp);

  if (TREE_TYPE (DECL_RESULT (fndecl)) != void_type_node)
    {
      if (sym->attr.subroutine || sym == sym->result)
	{
	  if (current_fake_result_decl != NULL)
	    result = TREE_VALUE (current_fake_result_decl);
	  else
	    result = NULL_TREE;
	  current_fake_result_decl = NULL_TREE;
	}
      else
	result = sym->result->backend_decl;

      if (result != NULL_TREE && sym->attr.function
	    && sym->ts.type == BT_DERIVED
	    && sym->ts.derived->attr.alloc_comp
	    && !sym->attr.pointer)
	{
	  rank = sym->as ? sym->as->rank : 0;
	  tmp2 = gfc_nullify_alloc_comp (sym->ts.derived, result, rank);
	  gfc_add_expr_to_block (&block, tmp2);
	}

     gfc_add_expr_to_block (&block, tmp);

     if (result == NULL_TREE)
	warning (0, "Function return value not set");
      else
	{
	  /* Set the return value to the dummy result variable.  The
	     types may be different for scalar default REAL functions
	     with -ff2c, therefore we have to convert.  */
	  tmp = convert (TREE_TYPE (DECL_RESULT (fndecl)), result);
	  tmp = build2 (MODIFY_EXPR, TREE_TYPE (tmp),
			DECL_RESULT (fndecl), tmp);
	  tmp = build1_v (RETURN_EXPR, tmp);
	  gfc_add_expr_to_block (&block, tmp);
	}
    }
  else
    gfc_add_expr_to_block (&block, tmp);


  /* Add all the decls we created during processing.  */
  decl = saved_function_decls;
  while (decl)
    {
      tree next;

      next = TREE_CHAIN (decl);
      TREE_CHAIN (decl) = NULL_TREE;
      pushdecl (decl);
      decl = next;
    }
  saved_function_decls = NULL_TREE;

  DECL_SAVED_TREE (fndecl) = gfc_finish_block (&block);

  /* Finish off this function and send it for code generation.  */
  poplevel (1, 0, 1);
  BLOCK_SUPERCONTEXT (DECL_INITIAL (fndecl)) = fndecl;

  /* Output the GENERIC tree.  */
  dump_function (TDI_original, fndecl);

  /* Store the end of the function, so that we get good line number
     info for the epilogue.  */
  cfun->function_end_locus = input_location;

  /* We're leaving the context of this function, so zap cfun.
     It's still in DECL_STRUCT_FUNCTION, and we'll restore it in
     tree_rest_of_compilation.  */
  cfun = NULL;

  if (old_context)
    {
      pop_function_context ();
      saved_function_decls = saved_parent_function_decls;
    }
  current_function_decl = old_context;

  if (decl_function_context (fndecl))
    /* Register this function with cgraph just far enough to get it
       added to our parent's nested function list.  */
    (void) cgraph_node (fndecl);
  else
    {
      gfc_gimplify_function (fndecl);
      cgraph_finalize_function (fndecl, false);
    }
}

void
gfc_generate_constructors (void)
{
  gcc_assert (gfc_static_ctors == NULL_TREE);
#if 0
  tree fnname;
  tree type;
  tree fndecl;
  tree decl;
  tree tmp;

  if (gfc_static_ctors == NULL_TREE)
    return;

  fnname = get_file_function_name ('I');
  type = build_function_type (void_type_node,
			      gfc_chainon_list (NULL_TREE, void_type_node));

  fndecl = build_decl (FUNCTION_DECL, fnname, type);
  TREE_PUBLIC (fndecl) = 1;

  decl = build_decl (RESULT_DECL, NULL_TREE, void_type_node);
  DECL_ARTIFICIAL (decl) = 1;
  DECL_IGNORED_P (decl) = 1;
  DECL_CONTEXT (decl) = fndecl;
  DECL_RESULT (fndecl) = decl;

  pushdecl (fndecl);

  current_function_decl = fndecl;

  rest_of_decl_compilation (fndecl, 1, 0);

  make_decl_rtl (fndecl);

  init_function_start (fndecl);

  pushlevel (0);

  for (; gfc_static_ctors; gfc_static_ctors = TREE_CHAIN (gfc_static_ctors))
    {
      tmp =
	build_function_call_expr (TREE_VALUE (gfc_static_ctors), NULL_TREE);
      DECL_SAVED_TREE (fndecl) = build_stmt (EXPR_STMT, tmp);
    }

  poplevel (1, 0, 1);

  BLOCK_SUPERCONTEXT (DECL_INITIAL (fndecl)) = fndecl;

  free_after_parsing (cfun);
  free_after_compilation (cfun);

  tree_rest_of_compilation (fndecl);

  current_function_decl = NULL_TREE;
#endif
}

/* Translates a BLOCK DATA program unit. This means emitting the
   commons contained therein plus their initializations. We also emit
   a globally visible symbol to make sure that each BLOCK DATA program
   unit remains unique.  */

void
gfc_generate_block_data (gfc_namespace * ns)
{
  tree decl;
  tree id;

  /* Tell the backend the source location of the block data.  */
  if (ns->proc_name)
    gfc_set_backend_locus (&ns->proc_name->declared_at);
  else
    gfc_set_backend_locus (&gfc_current_locus);

  /* Process the DATA statements.  */
  gfc_trans_common (ns);

  /* Create a global symbol with the mane of the block data.  This is to
     generate linker errors if the same name is used twice.  It is never
     really used.  */
  if (ns->proc_name)
    id = gfc_sym_mangled_function_id (ns->proc_name);
  else
    id = get_identifier ("__BLOCK_DATA__");

  decl = build_decl (VAR_DECL, id, gfc_array_index_type);
  TREE_PUBLIC (decl) = 1;
  TREE_STATIC (decl) = 1;

  pushdecl (decl);
  rest_of_decl_compilation (decl, 1, 0);
}


#include "gt-fortran-trans-decl.h"

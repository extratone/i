/* Declarations for objc-act.c.
   Copyright (C) 1990, 2000, 2001, 2002, 2003, 2004, 2005
   Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to
the Free Software Foundation, 51 Franklin Street, Fifth Floor,
Boston, MA 02110-1301, USA.  */

#ifndef GCC_OBJC_ACT_H
#define GCC_OBJC_ACT_H

/* For enum gimplify_status */
#include "tree-gimple.h"

/*** Language hooks ***/

bool objc_init (void);
const char *objc_printable_name (tree, int);
tree objc_get_callee_fndecl (tree);
void objc_finish_file (void);
tree objc_fold_obj_type_ref (tree, tree);
enum gimplify_status objc_gimplify_expr (tree *, tree *, tree *);
/* APPLE LOCAL radar 4721858 */
extern void emit_instantiate_pending_templates (location_t *);
/* APPLE LOCAL radar 4291785 */
void objc_detect_field_duplicates (tree);
/* APPLE LOCAL radar 5355344 */
bool cp_objc_protocol_id_list (tree);

/* NB: The remaining public functions are prototyped in c-common.h, for the
   benefit of stub-objc.c and objc-act.c.  */

/* Objective-C structures */

/* APPLE LOCAL radar 4291785, 4548636 */
#define CLASS_LANG_SLOT_ELTS		9
/* APPLE LOCAL ObjC abi v2 - radar 4947311 */
#define PROTOCOL_LANG_SLOT_ELTS		9
#define OBJC_INFO_SLOT_ELTS		2

/* KEYWORD_DECL */
#define KEYWORD_KEY_NAME(DECL) ((DECL)->decl_minimal.name)
#define KEYWORD_ARG_NAME(DECL) ((DECL)->decl_non_common.arguments)
/* APPLE LOCAL radar 4157812 */
#define	KEYWORD_ARG_ATTRIBUTE(DECL) DECL_ATTRIBUTES(DECL)

/* INSTANCE_METHOD_DECL, CLASS_METHOD_DECL */
#define METHOD_SEL_NAME(DECL) ((DECL)->decl_minimal.name)
#define METHOD_SEL_ARGS(DECL) ((DECL)->decl_non_common.arguments)
#define METHOD_ADD_ARGS(DECL) ((DECL)->decl_non_common.result)
#define METHOD_ADD_ARGS_ELLIPSIS_P(DECL) ((DECL)->decl_common.lang_flag_0)
#define METHOD_DEFINITION(DECL) ((DECL)->decl_common.initial)
#define METHOD_ENCODING(DECL) ((DECL)->decl_minimal.context)
/* APPLE LOCAL radar 4359757 */
#define METHOD_CONTEXT(DECL) (DECL_WITH_VIS_CHECK (DECL)->decl_with_vis.assembler_name)
/* APPLE LOCAL radar 3803157 */
#define METHOD_TYPE_ATTRIBUTES(DECL) (DECL_COMMON_CHECK (DECL)->decl_common.abstract_origin)
/* APPLE LOCAL C* property metadata (Radar 4498373) */
#define METHOD_PROPERTY_CONTEXT(DECL) (DECL_COMMON_CHECK (DECL)->decl_common.size_unit)

/* APPLE LOCAL begin C* property (Radar 4436866, 4591909) */
#define PROPERTY_NAME(DECL) ((DECL)->decl_minimal.name)
#define PROPERTY_GETTER_NAME(DECL) ((DECL)->decl_non_common.arguments)
#define PROPERTY_SETTER_NAME(DECL) ((DECL)->decl_non_common.result)
#define PROPERTY_IVAR_NAME(DECL) ((DECL)->decl_common.initial)
#define PROPERTY_READONLY(DECL) ((DECL)->decl_minimal.context)
#define PROPERTY_DYNAMIC(DECL) (DECL_COMMON_CHECK (DECL)->decl_common.abstract_origin)
/* APPLE LOCAL end C* property (Radar 4436866, 4591909) */
/* APPLE LOCAL begin objc new property */
#define PROPERTY_COPY(DECL) (DECL_COMMON_CHECK (DECL)->decl_common.size_unit) 
#define PROPERTY_RETAIN(DECL) (DECL_WITH_VIS_CHECK (DECL)->decl_with_vis.assembler_name)
#define PROPERTY_ASSIGN(DECL) (!property_readonly \
			       && PROPERTY_RETAIN(DECL) == boolean_false_node \
			       && PROPERTY_COPY(DECL) == boolean_false_node)
#define PROPERTY_READWRITE(DECL) (DECL_WITH_VIS_CHECK (DECL)->decl_with_vis.section_name)
/* APPLE LOCAL end objc new property */

/* CLASS_INTERFACE_TYPE, CLASS_IMPLEMENTATION_TYPE,
   CATEGORY_INTERFACE_TYPE, CATEGORY_IMPLEMENTATION_TYPE,
   PROTOCOL_INTERFACE_TYPE */
#define CLASS_NAME(CLASS) ((CLASS)->type.name)
#define CLASS_SUPER_NAME(CLASS) (TYPE_CHECK (CLASS)->type.context)
#define CLASS_IVARS(CLASS) TREE_VEC_ELT (TYPE_LANG_SLOT_1 (CLASS), 0)
#define CLASS_RAW_IVARS(CLASS) TREE_VEC_ELT (TYPE_LANG_SLOT_1 (CLASS), 1)
#define CLASS_NST_METHODS(CLASS) ((CLASS)->type.minval)
#define CLASS_CLS_METHODS(CLASS) ((CLASS)->type.maxval)
/* APPLE LOCAL begin ObjC new abi */
#define CLASS_TYPE(CLASS) ((CLASS)->type.main_variant)
/* APPLE LOCAL end ObjC new abi */
#define CLASS_STATIC_TEMPLATE(CLASS) TREE_VEC_ELT (TYPE_LANG_SLOT_1 (CLASS), 2)
#define CLASS_CATEGORY_LIST(CLASS) TREE_VEC_ELT (TYPE_LANG_SLOT_1 (CLASS), 3)
#define CLASS_PROTOCOL_LIST(CLASS) TREE_VEC_ELT (TYPE_LANG_SLOT_1 (CLASS), 4)
/* APPLE LOCAL radar 4291785 */
#define TOTAL_CLASS_RAW_IVARS(CLASS) TREE_VEC_ELT (TYPE_LANG_SLOT_1 (CLASS), 5)
#define PROTOCOL_NAME(CLASS) ((CLASS)->type.name)
#define PROTOCOL_LIST(CLASS) TREE_VEC_ELT (TYPE_LANG_SLOT_1 (CLASS), 0)
#define PROTOCOL_NST_METHODS(CLASS) ((CLASS)->type.minval)
#define PROTOCOL_CLS_METHODS(CLASS) ((CLASS)->type.maxval)
#define PROTOCOL_FORWARD_DECL(CLASS) TREE_VEC_ELT (TYPE_LANG_SLOT_1 (CLASS), 1)
/* APPLE LOCAL begin ObjC abi v2 */
#define PROTOCOL_V2_FORWARD_DECL(CLASS) TREE_VEC_ELT (TYPE_LANG_SLOT_1 (CLASS), 2) 
#define CLASS_OR_CATEGORY_HAS_LOAD_IMPL(CLASS) TREE_VEC_ELT (TYPE_LANG_SLOT_1 (CLASS), 6)
/* APPLE LOCAL end ObjC abi v2 */
#define PROTOCOL_DEFINED(CLASS) TREE_USED (CLASS)

/* APPLE LOCAL begin C* language */
#define PROTOCOL_OPTIONAL_CLS_METHODS(CLASS) TREE_VEC_ELT (TYPE_LANG_SLOT_1 (CLASS), 3)
#define PROTOCOL_OPTIONAL_NST_METHODS(CLASS) TREE_VEC_ELT (TYPE_LANG_SLOT_1 (CLASS), 4)
/* APPLE LOCAL end C* language */
/* APPLE LOCAL begin C* property (Radar 4436866) */
/* For CATEGORY_INTERFACE_TYPE, CLASS_INTERFACE_TYPE or PROTOCOL_INTERFACE_TYPE */
#define CLASS_PROPERTY_DECL(CLASS) TREE_VEC_ELT (TYPE_LANG_SLOT_1 (CLASS), 7)
/* For CLASS_IMPLEMENTATION_TYPE or CATEGORY_IMPLEMENTATION_TYPE. */
#define IMPL_PROPERTY_DECL(CLASS) TREE_VEC_ELT (TYPE_LANG_SLOT_1 (CLASS), 7)
/* APPLE LOCAL end C* property (Radar 4436866) */
/* APPLE LOCAL begin radar 4548636 */
/* For CLASS_INTERFACE_TYPE only */
#define CLASS_ATTRIBUTES(CLASS) TREE_VEC_ELT (TYPE_LANG_SLOT_1 (CLASS), 8)
/* APPLE LOCAL end radar 4548636 */
/* APPLE LOCAL begin radar 4947311 - protocol attributes */
#define PROTOCOL_ATTRIBUTES(PROTO) TREE_VEC_ELT (TYPE_LANG_SLOT_1 (PROTO), 8)
/* APPLE LOCAL end radar 4947311 - protocol attributes */
/* APPLE LOCAL radar 4695101 */
/* declaration of  PROTOCOL_IMPL_OBJ removed. */

/* ObjC-specific information pertaining to RECORD_TYPEs are stored in
   the LANG_SPECIFIC structures, which may itself need allocating first.  */

/* The following three macros must be overridden (in objcp/objcp-decl.h)
   for Objective-C++.  */
#define TYPE_OBJC_INFO(TYPE) TYPE_LANG_SPECIFIC (TYPE)->objc_info
#define SIZEOF_OBJC_TYPE_LANG_SPECIFIC sizeof (struct lang_type)
#define ALLOC_OBJC_TYPE_LANG_SPECIFIC(NODE)				\
  do {									\
    TYPE_LANG_SPECIFIC (NODE) = GGC_CNEW (struct lang_type);		\
  } while (0)

#define TYPE_HAS_OBJC_INFO(TYPE)				\
	(TYPE_LANG_SPECIFIC (TYPE) && TYPE_OBJC_INFO (TYPE))
#define TYPE_OBJC_INTERFACE(TYPE) TREE_VEC_ELT (TYPE_OBJC_INFO (TYPE), 0)
#define TYPE_OBJC_PROTOCOL_LIST(TYPE) TREE_VEC_ELT (TYPE_OBJC_INFO (TYPE), 1)


#define INIT_TYPE_OBJC_INFO(TYPE)				\
	do							\
	  {							\
	    if (!TYPE_LANG_SPECIFIC (TYPE))			\
	      ALLOC_OBJC_TYPE_LANG_SPECIFIC(TYPE);		\
	    if (!TYPE_OBJC_INFO (TYPE))				\
	      TYPE_OBJC_INFO (TYPE)				\
		= make_tree_vec (OBJC_INFO_SLOT_ELTS);		\
	  }							\
	while (0)
#define DUP_TYPE_OBJC_INFO(DST, SRC)				\
	do							\
	  {							\
	    ALLOC_OBJC_TYPE_LANG_SPECIFIC(DST);			\
	    if (TYPE_LANG_SPECIFIC (SRC))			\
	      memcpy (TYPE_LANG_SPECIFIC (DST),			\
		      TYPE_LANG_SPECIFIC (SRC),			\
		      SIZEOF_OBJC_TYPE_LANG_SPECIFIC);		\
	    TYPE_OBJC_INFO (DST)				\
	      = make_tree_vec (OBJC_INFO_SLOT_ELTS);		\
	  }							\
	while (0)

#define TYPED_OBJECT(TYPE)					\
	(TREE_CODE (TYPE) == RECORD_TYPE			\
	 && TYPE_HAS_OBJC_INFO (TYPE)				\
	 && TYPE_OBJC_INTERFACE (TYPE))
#define OBJC_TYPE_NAME(TYPE) TYPE_NAME(TYPE)
#define OBJC_SET_TYPE_NAME(TYPE, NAME) (TYPE_NAME (TYPE) = NAME)

/* Define the Objective-C or Objective-C++ language-specific tree codes.  */

#define DEFTREECODE(SYM, NAME, TYPE, LENGTH) SYM,
enum objc_tree_code {
#if defined (GCC_CP_TREE_H)
  LAST_BASE_TREE_CODE = LAST_CPLUS_TREE_CODE,
#else 
#if defined (GCC_C_TREE_H)
  LAST_BASE_TREE_CODE = LAST_C_TREE_CODE,
#else
  #error You must include <c-tree.h> or <cp/cp-tree.h> before <objc/objc-act.h>
#endif
#endif
#include "objc-tree.def"
  LAST_OBJC_TREE_CODE
};
#undef DEFTREECODE

/* Hash tables to manage the global pool of method prototypes.  */

typedef struct hashed_entry	*hash;
typedef struct hashed_attribute	*attr;

struct hashed_attribute GTY(())
{
  attr next;
  tree value;
};
struct hashed_entry GTY(())
{
  attr list;
  hash next;
  tree key;
};

extern GTY ((length ("SIZEHASHTABLE"))) hash *nst_method_hash_list;
extern GTY ((length ("SIZEHASHTABLE"))) hash *cls_method_hash_list;
/* APPLE LOCAL begin radar 4359757 */
extern GTY ((length ("SIZEHASHTABLE"))) hash *class_nst_method_hash_list;
extern GTY ((length ("SIZEHASHTABLE"))) hash *class_cls_method_hash_list;
extern GTY ((length ("SIZEHASHTABLE"))) hash *class_names_hash_list;
extern GTY ((length ("SIZEHASHTABLE"))) hash *meth_var_names_hash_list;
extern GTY ((length ("SIZEHASHTABLE"))) hash *meth_var_types_hash_list;
extern GTY ((length ("SIZEHASHTABLE"))) hash *prop_names_attr_hash_list;
extern GTY ((length ("SIZEHASHTABLE"))) hash *sel_ref_hash_list;
/* APPLE LOCAL end radar 4359757 */

/* APPLE LOCAL begin radar 4345837 */
extern GTY ((length ("SIZEHASHTABLE"))) hash *cls_name_hash_list;
extern GTY ((length ("SIZEHASHTABLE"))) hash *als_name_hash_list;
/* APPLE LOCAL end radar 4345837 */

/* APPLE LOCAL radar 4441049 */
extern GTY ((length ("SIZEHASHTABLE"))) hash *ivar_offset_hash_list;

#define SIZEHASHTABLE		257

/* Objective-C/Objective-C++ @implementation list.  */

struct imp_entry GTY(())
{
  struct imp_entry *next;
  tree imp_context;
  tree imp_template;
  tree class_decl;		/* _OBJC_CLASS_<my_name>; */
  tree meta_decl;		/* _OBJC_METACLASS_<my_name>; */
  /* APPLE LOCAL begin ObjC new abi */
  tree class_v2_decl;		/* _OBJC_V2_CLASS_<my_name>; */
  tree meta_v2_decl;		/* _OBJC_V2_METACLASS_<my_name>; */
  /* APPLE LOCAL end ObjC new abi */
  BOOL_BITFIELD has_cxx_cdtors : 1;
};

extern GTY(()) struct imp_entry *imp_list;
extern GTY(()) int imp_count;	/* `@implementation' */
extern GTY(()) int cat_count;	/* `@category' */

extern GTY(()) enum tree_code objc_inherit_code;
extern GTY(()) int objc_public_flag;

/* Objective-C/Objective-C++ global tree enumeration.  */

enum objc_tree_index
{
    OCTI_STATIC_NST,
    OCTI_STATIC_NST_DECL,
    OCTI_SELF_ID,
    OCTI_UCMD_ID,

    OCTI_SELF_DECL,
    OCTI_UMSG_DECL,
    /* APPLE LOCAL radar 4590221 */
    /* code removed */
    OCTI_UMSG_SUPER_DECL,
    OCTI_UMSG_STRET_DECL,
    OCTI_UMSG_SUPER_STRET_DECL,
    OCTI_GET_CLASS_DECL,
    OCTI_GET_MCLASS_DECL,
    OCTI_SUPER_TYPE,
    OCTI_SEL_TYPE,
    OCTI_ID_TYPE,
    OCTI_CLS_TYPE,
    OCTI_NST_TYPE,
    OCTI_PROTO_TYPE,

    /* APPLE LOCAL radar 4345837 */
    /* OCTI_CLS_CHAIN and OCTI_ALIAS_CHAIN are removed removed */
    OCTI_INTF_CHAIN,
    OCTI_PROTO_CHAIN,
    OCTI_IMPL_CHAIN,
    OCTI_CLS_REF_CHAIN,
    OCTI_SEL_REF_CHAIN,
    OCTI_IVAR_CHAIN,
    OCTI_CLS_NAMES_CHAIN,
    OCTI_METH_VAR_NAMES_CHAIN,
    OCTI_METH_VAR_TYPES_CHAIN,

    OCTI_SYMBOLS_DECL,
    OCTI_NST_VAR_DECL,
    OCTI_CLS_VAR_DECL,
    OCTI_NST_METH_DECL,
    OCTI_CLS_METH_DECL,
    OCTI_CLS_DECL,
    OCTI_MCLS_DECL,
    OCTI_SEL_TABLE_DECL,
    OCTI_MODULES_DECL,
    OCTI_GNU_INIT_DECL,

    OCTI_INTF_CTX,
    OCTI_IMPL_CTX,
    OCTI_METH_CTX,
    OCTI_IVAR_CTX,

    OCTI_IMPL_TEMPL,
    OCTI_CLS_TEMPL,
    OCTI_CAT_TEMPL,
    OCTI_UPRIV_REC,
    OCTI_PROTO_TEMPL,
    OCTI_SEL_TEMPL,
    OCTI_UCLS_SUPER_REF,
    OCTI_UUCLS_SUPER_REF,
    OCTI_METH_TEMPL,
    OCTI_IVAR_TEMPL,
    OCTI_METH_LIST_TEMPL,
    OCTI_METH_PROTO_LIST_TEMPL,
    OCTI_IVAR_LIST_TEMPL,
    OCTI_SYMTAB_TEMPL,
    OCTI_MODULE_TEMPL,
    OCTI_SUPER_TEMPL,
    OCTI_OBJ_REF,
    OCTI_CLS_REF,
    OCTI_METH_PROTO_TEMPL,
    OCTI_FUNCTION1_TEMPL,
    OCTI_FUNCTION2_TEMPL,

    OCTI_OBJ_ID,
    OCTI_CLS_ID,
    OCTI_ID_NAME,
    OCTI_CLASS_NAME,
    OCTI_CNST_STR_ID,
    OCTI_CNST_STR_TYPE,
    OCTI_CNST_STR_GLOB_ID,
    OCTI_STRING_CLASS_DECL,
    OCTI_INTERNAL_CNST_STR_TYPE,
    OCTI_SUPER_DECL,
    OCTI_UMSG_NONNIL_DECL,
    OCTI_UMSG_NONNIL_STRET_DECL,
    OCTI_STORAGE_CLS,
    OCTI_EXCEPTION_EXTRACT_DECL,
    OCTI_EXCEPTION_TRY_ENTER_DECL,
    OCTI_EXCEPTION_TRY_EXIT_DECL,
    OCTI_EXCEPTION_MATCH_DECL,
    OCTI_EXCEPTION_THROW_DECL,
    OCTI_SYNC_ENTER_DECL,
    OCTI_SYNC_EXIT_DECL,
    OCTI_SETJMP_DECL,
    OCTI_EXCDATA_TEMPL,
    OCTI_STACK_EXCEPTION_DATA_DECL,
    OCTI_LOCAL_EXCEPTION_DECL,
    OCTI_RETHROW_EXCEPTION_DECL,
    OCTI_EVAL_ONCE_DECL,
    OCTI_CATCH_TYPE,
    OCTI_EXECCLASS_DECL,
    /* APPLE LOCAL radar 4280641 */
    OCTI_UMSG_FPRET_DECL,

    OCTI_ASSIGN_IVAR_DECL,
    /* APPLE LOCAL 4590221 */
     /* removed OCTI_ASSIGN_IVAR_FAST_DECL */
    OCTI_ASSIGN_GLOBAL_DECL,
    OCTI_ASSIGN_STRONGCAST_DECL,
    /* APPLE LOCAL radar 3742561 */
    OCTI_MEMMOVE_DECL,
    /* APPLE LOCAL begin radar 4426814 */
    OCTI_ASSIGN_WEAK_DECL,
    OCTI_READ_WEAK_DECL,
    /* APPLE LOCAL end radar 4426814 */

    /* APPLE LOCAL begin ObjC new abi */
    OCTI_V2_CLS_TEMPL,
    OCTI_V2_CLS_RO_TEMPL,
    OCTI_IMP_TYPE,
    OCTI_V2_CLS_DECL,
    OCTI_V2_MCLS_DECL,
    OCTI_V2_CACHE_DECL,
    OCTI_V2_VTABLE_DECL,
    OCTI_V2_PROTO_TEMPL,
    OCTI_CLASSLIST_REF_CHAIN,
    OCTI_METACLASSLIST_REF_CHAIN,
    /* APPLE LOCAL begin radar 4535676 */
    OCTI_CLASSLIST_SUPER_REF_CHAIN,
    OCTI_METACLASSLIST_SUPER_REF_CHAIN,
    /* APPLE LOCAL end radar 4535676 */
    OCTI_CLASS_LIST_CHAIN,
    /* APPLE LOCAL begin radar 4533974 - ObjC new protocol */
    OCTI_PROTOCOL_LIST_CHAIN,
    OCTI_PROTOCOLLIST_REF_CHAIN,
    /* APPLE LOCAL end radar 4533974 - ObjC new protocol */
    OCTI_CATEGORY_LIST_CHAIN,
    OCTI_NONLAZY_CLASS_LIST_CHAIN,
    OCTI_NONLAZY_CATEGORY_LIST_CHAIN,
    OCTI_MESSAGE_REF_TEMPL,
    OCTI_SUPER_MESSAGE_REF_TEMPL,
    CTI_MESSAGE_SELECTOR_TYPE,
    CTI_SUPER_MESSAGE_SELECTOR_TYPE,
    OCTI_V2_UMSG_FIXUP_DECL,
    /* APPLE LOCAL radar 4557598 */
    OCTI_V2_UMSG_FPRET_FIXUP_DECL, 
    OCTI_V2_UMSG_STRET_FIXUP_DECL,
    OCTI_V2_UMSG_ID_FIXUP_DECL,
    OCTI_V2_UMSG_ID_STRET_FIXUP_DECL,
    OCTI_V2_UMSG_SUPER2_FIXUP_DECL,
    OCTI_V2_UMSG_SUPER2_STRET_FIXUP_DECL,
    OCTI_V2_IMP_TYPE,
    OCTI_V2_SUPER_IMP_TYPE,
    OCTI_V2_MESSAGE_REF_CHAIN,
    OCTI_V2_NST_VAR_DECL,
    OCTI_V2_CLS_VAR_DECL,
    OCTI_V2_IVAR_TEMPL,
    OCTI_V2_IVAR_LIST_TEMPL,
    OCTI_V2_INST_METH_DECL,
    OCTI_V2_CLS_METH_DECL,
    OCTI_V2_IVAR_OFFSET_REF_CHAIN,
    OCTI_V2_CAT_TEMPL,
    /* APPLE LOCAL radar 4533974 - ObjC new protocol */
    /* code removed */
    /* APPLE LOCAL end ObjC new abi */
    /* APPLE LOCAL begin C* language */
    OCTI_FAST_ENUM_STATE_TEMP,
    OCTI_ENUM_MUTATION_DECL,
    /* APPLE LOCAL end C* language */
    /* APPLE LOCAL begin C* property metadata (Radar 4498373) */
    OCTI_V2_PROPERTY_TEMPL,
    OCTI_V2_PROPERTY_DECL,
    OCTI_V2_PROP_NAME_ATTR_CHAIN,
    OCTI_PROP_LIST_TEMPL,
    /* APPLE LOCAL end C* property metadata (Radar 4498373) */
    /* APPLE LOCAL begin radar 4585769 - Objective-C 1.0 extensions */
    OCTI_PROTO_EXT_TEMPL,
    OCTI_PROPERTY_TEMPL,
    OCTI_PROPERTY_LIST_TEMPL,
    OCTI_CLASS_EXT_TEMPL,
    OCTI_CLASS_EXT_DECL,
    OCTI_PROTOCOL_EXT_DECL,
    OCTI_PROTOCOL_OPT_NST_METH_DECL,
    OCTI_PROTOCOL_OPT_CLS_METH_DECL,
    /* APPLE LOCAL end radar 4585769 - Objective-C 1.0 extensions */
    /* APPLE LOCAL begin radar 2848255 */
    OCTI_EHTYPE,
    OCTI_EHTYPE_VTABLE,
    OCTI_BEGIN_CATCH,
    OCTI_END_CATCH,
    OCTI_EHTYPE_ID,
    /* APPLE LOCAL end radar 2848255 */
    /* APPLE LOCAL begin radar 4947014 - objc atomic property */
    OCTI_UMSG_GETATOMICPROPERTY,
    OCTI_UMSG_SETATOMICPROPERTY,
    OCTI_UMSG_COPYATOMICSTRUCT,
    OCTI_UMSG_NSCOPYING,
    /* APPLE LOCAL end radar 4947014 - objc atomic property */
    OCTI_MAX
};

extern GTY(()) tree objc_global_trees[OCTI_MAX];

/* List of classes with list of their static instances.  */
#define objc_static_instances	objc_global_trees[OCTI_STATIC_NST]

/* The declaration of the array administrating the static instances.  */
#define static_instances_decl	objc_global_trees[OCTI_STATIC_NST_DECL]

/* Some commonly used instances of "identifier_node".  */

#define self_id			objc_global_trees[OCTI_SELF_ID]
#define ucmd_id			objc_global_trees[OCTI_UCMD_ID]

#define self_decl		objc_global_trees[OCTI_SELF_DECL]
#define umsg_decl		objc_global_trees[OCTI_UMSG_DECL]
/* APPLE LOCAL radar 4590221 */
/* umsg_fast_decl removed */
#define umsg_super_decl		objc_global_trees[OCTI_UMSG_SUPER_DECL]
#define umsg_stret_decl		objc_global_trees[OCTI_UMSG_STRET_DECL]
#define umsg_super_stret_decl	objc_global_trees[OCTI_UMSG_SUPER_STRET_DECL]
#define objc_get_class_decl	objc_global_trees[OCTI_GET_CLASS_DECL]
#define objc_get_meta_class_decl			\
				objc_global_trees[OCTI_GET_MCLASS_DECL]

#define objc_super_type		objc_global_trees[OCTI_SUPER_TYPE]
/* APPLE LOCAL ObjC new abi */
#define objc_selector_type	objc_global_trees[OCTI_SEL_TYPE]
#define objc_object_type	objc_global_trees[OCTI_ID_TYPE]
#define objc_class_type		objc_global_trees[OCTI_CLS_TYPE]
#define objc_instance_type	objc_global_trees[OCTI_NST_TYPE]
#define objc_protocol_type	objc_global_trees[OCTI_PROTO_TYPE]

/* Type checking macros.  */

#define IS_ID(TYPE)							\
	(TREE_CODE (TYPE) == POINTER_TYPE				\
	 && (TYPE_MAIN_VARIANT (TREE_TYPE (TYPE))			\
	     == TREE_TYPE (objc_object_type)))
#define IS_CLASS(TYPE)							\
	(TREE_CODE (TYPE) == POINTER_TYPE				\
	 && (TYPE_MAIN_VARIANT (TREE_TYPE (TYPE))			\
	     == TREE_TYPE (objc_class_type)))
#define IS_PROTOCOL_QUALIFIED_UNTYPED(TYPE)				\
	((IS_ID (TYPE) || IS_CLASS (TYPE))				\
	 && TYPE_HAS_OBJC_INFO (TREE_TYPE (TYPE))			\
	 && TYPE_OBJC_PROTOCOL_LIST (TREE_TYPE (TYPE)))
#define IS_SUPER(TYPE)							\
	(TREE_CODE (TYPE) == POINTER_TYPE				\
	 && TREE_TYPE (TYPE) == objc_super_template)

/* APPLE LOCAL radar 4345837 */
/* class_chain and alias_chain are removed */
#define interface_chain		objc_global_trees[OCTI_INTF_CHAIN]
#define protocol_chain		objc_global_trees[OCTI_PROTO_CHAIN]
#define implemented_classes	objc_global_trees[OCTI_IMPL_CHAIN]

/* Chains to manage selectors that are referenced and defined in the
   module.  */

#define cls_ref_chain		objc_global_trees[OCTI_CLS_REF_CHAIN]	/* Classes referenced.  */
#define sel_ref_chain		objc_global_trees[OCTI_SEL_REF_CHAIN]	/* Selectors referenced.  */
#define objc_ivar_chain		objc_global_trees[OCTI_IVAR_CHAIN]

/* Chains to manage uniquing of strings.  */

#define class_names_chain	objc_global_trees[OCTI_CLS_NAMES_CHAIN]
#define meth_var_names_chain	objc_global_trees[OCTI_METH_VAR_NAMES_CHAIN]
#define meth_var_types_chain	objc_global_trees[OCTI_METH_VAR_TYPES_CHAIN]


/* Backend data declarations.  */

#define UOBJC_SYMBOLS_decl		objc_global_trees[OCTI_SYMBOLS_DECL]
#define UOBJC_INSTANCE_VARIABLES_decl	objc_global_trees[OCTI_NST_VAR_DECL]
#define UOBJC_CLASS_VARIABLES_decl	objc_global_trees[OCTI_CLS_VAR_DECL]
#define UOBJC_INSTANCE_METHODS_decl	objc_global_trees[OCTI_NST_METH_DECL]
#define UOBJC_CLASS_METHODS_decl	objc_global_trees[OCTI_CLS_METH_DECL]
#define UOBJC_CLASS_decl		objc_global_trees[OCTI_CLS_DECL]
#define UOBJC_METACLASS_decl		objc_global_trees[OCTI_MCLS_DECL]
#define UOBJC_SELECTOR_TABLE_decl	objc_global_trees[OCTI_SEL_TABLE_DECL]
#define UOBJC_MODULES_decl		objc_global_trees[OCTI_MODULES_DECL]
#define GNU_INIT_decl			objc_global_trees[OCTI_GNU_INIT_DECL]

/* The following are used when compiling a class implementation.
   implementation_template will normally be an interface, however if
   none exists this will be equal to objc_implementation_context...it is
   set in start_class.  */

#define objc_interface_context		objc_global_trees[OCTI_INTF_CTX]
#define objc_implementation_context	objc_global_trees[OCTI_IMPL_CTX]
#define objc_method_context		objc_global_trees[OCTI_METH_CTX]
#define objc_ivar_context		objc_global_trees[OCTI_IVAR_CTX]

#define implementation_template	objc_global_trees[OCTI_IMPL_TEMPL]
#define objc_class_template	objc_global_trees[OCTI_CLS_TEMPL]
#define objc_category_template	objc_global_trees[OCTI_CAT_TEMPL]
#define uprivate_record		objc_global_trees[OCTI_UPRIV_REC]
#define objc_protocol_template	objc_global_trees[OCTI_PROTO_TEMPL]
#define objc_selector_template	objc_global_trees[OCTI_SEL_TEMPL]
#define ucls_super_ref		objc_global_trees[OCTI_UCLS_SUPER_REF]
#define uucls_super_ref		objc_global_trees[OCTI_UUCLS_SUPER_REF]

#define umsg_nonnil_decl	objc_global_trees[OCTI_UMSG_NONNIL_DECL]
#define umsg_nonnil_stret_decl	objc_global_trees[OCTI_UMSG_NONNIL_STRET_DECL]
#define objc_storage_class	objc_global_trees[OCTI_STORAGE_CLS]
#define objc_exception_extract_decl		\
				objc_global_trees[OCTI_EXCEPTION_EXTRACT_DECL]
#define objc_exception_try_enter_decl		\
				objc_global_trees[OCTI_EXCEPTION_TRY_ENTER_DECL]
#define objc_exception_try_exit_decl		\
				objc_global_trees[OCTI_EXCEPTION_TRY_EXIT_DECL]
#define objc_exception_match_decl		\
				objc_global_trees[OCTI_EXCEPTION_MATCH_DECL]
#define objc_exception_throw_decl		\
				objc_global_trees[OCTI_EXCEPTION_THROW_DECL]
#define objc_sync_enter_decl	objc_global_trees[OCTI_SYNC_ENTER_DECL]
#define objc_sync_exit_decl	objc_global_trees[OCTI_SYNC_EXIT_DECL]
#define objc_exception_data_template		\
				objc_global_trees[OCTI_EXCDATA_TEMPL]
#define objc_setjmp_decl	objc_global_trees[OCTI_SETJMP_DECL]
#define objc_stack_exception_data		\
				objc_global_trees[OCTI_STACK_EXCEPTION_DATA_DECL]
#define objc_caught_exception	objc_global_trees[OCTI_LOCAL_EXCEPTION_DECL]	
/* APPLE LOCAL radar 4957534 */
#define objc_rethrow_exception_decl objc_global_trees[OCTI_RETHROW_EXCEPTION_DECL]	
#define objc_eval_once		objc_global_trees[OCTI_EVAL_ONCE_DECL]	
#define objc_catch_type		objc_global_trees[OCTI_CATCH_TYPE]

#define execclass_decl		objc_global_trees[OCTI_EXECCLASS_DECL]
/* APPLE LOCAL radar 4280641 */
#define umsg_fpret_decl		objc_global_trees[OCTI_UMSG_FPRET_DECL]

#define objc_assign_ivar_decl	objc_global_trees[OCTI_ASSIGN_IVAR_DECL]
/* APPLE LOCAL mainline */
/* remove objc_assign_ivar_fast_decl */
#define objc_assign_global_decl	objc_global_trees[OCTI_ASSIGN_GLOBAL_DECL]
#define objc_assign_strong_cast_decl		\
				objc_global_trees[OCTI_ASSIGN_STRONGCAST_DECL]
/* APPLE LOCAL radar 3742561 */
#define objc_memmove_collectable_decl objc_global_trees[OCTI_MEMMOVE_DECL]

/* APPLE LOCAL begin radar 4426814 */
#define objc_assign_weak_decl	objc_global_trees[OCTI_ASSIGN_WEAK_DECL]
#define objc_read_weak_decl	objc_global_trees[OCTI_READ_WEAK_DECL]
/* APPLE LOCAL end radar 4426814 */

#define objc_method_template	objc_global_trees[OCTI_METH_TEMPL]
#define objc_ivar_template	objc_global_trees[OCTI_IVAR_TEMPL]
#define objc_method_list_ptr	objc_global_trees[OCTI_METH_LIST_TEMPL]
#define objc_method_proto_list_ptr		\
				objc_global_trees[OCTI_METH_PROTO_LIST_TEMPL]
#define objc_ivar_list_ptr	objc_global_trees[OCTI_IVAR_LIST_TEMPL]
#define objc_symtab_template	objc_global_trees[OCTI_SYMTAB_TEMPL]
#define objc_module_template	objc_global_trees[OCTI_MODULE_TEMPL]
#define objc_super_template	objc_global_trees[OCTI_SUPER_TEMPL]
#define objc_object_reference	objc_global_trees[OCTI_OBJ_REF]
#define objc_class_reference	objc_global_trees[OCTI_CLS_REF]
#define objc_method_prototype_template		\
				objc_global_trees[OCTI_METH_PROTO_TEMPL]
#define function1_template	objc_global_trees[OCTI_FUNCTION1_TEMPL]
#define function2_template	objc_global_trees[OCTI_FUNCTION2_TEMPL]

#define objc_object_id		objc_global_trees[OCTI_OBJ_ID]
#define objc_class_id		objc_global_trees[OCTI_CLS_ID]
#define objc_object_name		objc_global_trees[OCTI_ID_NAME]
#define objc_class_name		objc_global_trees[OCTI_CLASS_NAME]
#define constant_string_id	objc_global_trees[OCTI_CNST_STR_ID]
#define constant_string_type	objc_global_trees[OCTI_CNST_STR_TYPE]
#define constant_string_global_id		\
				objc_global_trees[OCTI_CNST_STR_GLOB_ID]
#define string_class_decl	objc_global_trees[OCTI_STRING_CLASS_DECL]
#define internal_const_str_type	objc_global_trees[OCTI_INTERNAL_CNST_STR_TYPE]
#define UOBJC_SUPER_decl	objc_global_trees[OCTI_SUPER_DECL]

/* APPLE LOCAL begin ObjC new abi */
#define objc_v2_class_template     objc_global_trees[OCTI_V2_CLS_TEMPL]
#define objc_v2_class_ro_template  objc_global_trees[OCTI_V2_CLS_RO_TEMPL]
#define objc_v2_protocol_template  objc_global_trees[OCTI_V2_PROTO_TEMPL]
#define objc_imp_type		       objc_global_trees[OCTI_IMP_TYPE]
#define UOBJC_V2_CLASS_decl        objc_global_trees[OCTI_V2_CLS_DECL]
#define UOBJC_V2_METACLASS_decl    objc_global_trees[OCTI_V2_MCLS_DECL]
#define UOBJC_V2_CACHE_decl	       objc_global_trees[OCTI_V2_CACHE_DECL]
#define UOBJC_V2_VTABLE_decl       objc_global_trees[OCTI_V2_VTABLE_DECL]
#define UOBJC_V2_INSTANCE_VARIABLES_decl  objc_global_trees[OCTI_V2_NST_VAR_DECL]
#define UOBJC_V2_CLASS_VARIABLES_decl	objc_global_trees[OCTI_V2_CLS_VAR_DECL]
#define UOBJC_V2_INSTANCE_METHODS_decl	objc_global_trees[OCTI_V2_INST_METH_DECL]
#define UOBJC_V2_CLASS_METHODS_decl		objc_global_trees[OCTI_V2_CLS_METH_DECL]
#define objc_v2_ivar_template	objc_global_trees[OCTI_V2_IVAR_TEMPL]
/* classes referenced.  */
#define classlist_ref_chain	       objc_global_trees[OCTI_CLASSLIST_REF_CHAIN]
#define metaclasslist_ref_chain	       objc_global_trees[OCTI_METACLASSLIST_REF_CHAIN]
/* APPLE LOCAL begin radar 4535676 */
/* 'super' classes referenced. */
#define classlist_super_ref_chain	       objc_global_trees[OCTI_CLASSLIST_SUPER_REF_CHAIN]
#define metaclasslist_super_ref_chain	       objc_global_trees[OCTI_METACLASSLIST_SUPER_REF_CHAIN]
/* APPLE LOCAL end radar 4535676 */
/* classes @implemented and whose meta-data address must be added to __class_list section. */
#define class_list_chain	objc_global_trees[OCTI_CLASS_LIST_CHAIN]
/* APPLE LOCAL begin radar 4533974 - ObjC new protocol */
/* list of protocols whose protocol_t meta-data need be generated. */
#define protocol_list_chain	objc_global_trees[OCTI_PROTOCOL_LIST_CHAIN]
/* list of protocols in __protocol_refs section reference by a @protocol(MyProto) expression. */
#define protocollist_ref_chain	       objc_global_trees[OCTI_PROTOCOLLIST_REF_CHAIN]
/* APPLE LOCAL end radar 4533974 - ObjC new protocol */
/* categories @implemented and whose meta-data address must be added to __category_list section. */
#define category_list_chain	objc_global_trees[OCTI_CATEGORY_LIST_CHAIN]
#define nonlazy_class_list_chain	objc_global_trees[OCTI_NONLAZY_CLASS_LIST_CHAIN]
#define nonlazy_category_list_chain	objc_global_trees[OCTI_NONLAZY_CATEGORY_LIST_CHAIN]
/* struct message_ref_t */
#define objc_v2_message_ref_template objc_global_trees[OCTI_MESSAGE_REF_TEMPL]      
/* struct super_message_ref_t */
#define objc_v2_super_message_ref_template objc_global_trees[OCTI_SUPER_MESSAGE_REF_TEMPL]      
/* struct message_ref_t* */
#define objc_v2_selector_type      objc_global_trees[CTI_MESSAGE_SELECTOR_TYPE]	
/* struct super_super_message_ref_t */
#define objc_v2_super_selector_type      objc_global_trees[CTI_SUPER_MESSAGE_SELECTOR_TYPE]	
/* objc_msgSend_fixup */
#define umsg_fixup_decl		       objc_global_trees[OCTI_V2_UMSG_FIXUP_DECL]   
/* APPLE LOCAL begin radar 4557598 */
/* objc_msgSend_fpret_fixup_decl */ 
#define umsg_fpret_fixup_decl		objc_global_trees[OCTI_V2_UMSG_FPRET_FIXUP_DECL]
/* APPLE LOCAL end radar 4557598 */
/* objc_msgSend_stret_fixup */
#define umsg_stret_fixup_decl	       objc_global_trees[OCTI_V2_UMSG_STRET_FIXUP_DECL]   
/* objc_msgSendId_fixup */
#define umsg_id_fixup_decl	       objc_global_trees[OCTI_V2_UMSG_ID_FIXUP_DECL]   
/* objc_msgSendId_stret_fixup */
#define umsg_id_stret_fixup_decl       objc_global_trees[OCTI_V2_UMSG_ID_STRET_FIXUP_DECL]   
/* objc_msgSendSuper2_fixup */
#define umsg_id_super2_fixup_decl      objc_global_trees[OCTI_V2_UMSG_SUPER2_FIXUP_DECL]
/* objc_msgSendSuper2_stret_fixup */
#define umsg_id_super2_stret_fixup_decl objc_global_trees[OCTI_V2_UMSG_SUPER2_STRET_FIXUP_DECL]
#define objc_v2_imp_type 		objc_global_trees[OCTI_V2_IMP_TYPE]
#define objc_v2_super_imp_type 	objc_global_trees[OCTI_V2_SUPER_IMP_TYPE]
#define message_ref_chain		objc_global_trees[OCTI_V2_MESSAGE_REF_CHAIN]
#define objc_v2_ivar_list_ptr	objc_global_trees[OCTI_V2_IVAR_LIST_TEMPL]
#define ivar_offset_ref_chain		objc_global_trees[OCTI_V2_IVAR_OFFSET_REF_CHAIN]
#define objc_v2_category_template  	objc_global_trees[OCTI_V2_CAT_TEMPL]
/* APPLE LOCAL radar 4533974 - ObjC new protocol */
/* code removed */
/* APPLE LOCAL end ObjC new abi */
/* APPLE LOCAL begin C* language */
#define objc_fast_enum_state_type	objc_global_trees[OCTI_FAST_ENUM_STATE_TEMP]
#define objc_enum_mutation_decl		objc_global_trees[OCTI_ENUM_MUTATION_DECL]
/* APPLE LOCAL end C* language */
/* APPLE LOCAL begin C* property metadata (Radar 4498373) */
#define objc_v2_property_template objc_global_trees[OCTI_V2_PROPERTY_TEMPL]
#define UOBJC_V2_PROPERTY_decl  objc_global_trees[OCTI_V2_PROPERTY_DECL]
#define prop_names_attr_chain	objc_global_trees[OCTI_V2_PROP_NAME_ATTR_CHAIN]
#define objc_prop_list_ptr	objc_global_trees[OCTI_PROP_LIST_TEMPL]
/* APPLE LOCAL end C* property metadata (Radar 4498373) */
/* APPLE LOCAL begin radar 4585769 - Objective-C 1.0 extensions */
#define objc_protocol_extension_template  objc_global_trees[OCTI_PROTO_EXT_TEMPL]
#define objc_property_template objc_global_trees[OCTI_PROPERTY_TEMPL]
#define objc_property_list_template objc_global_trees[OCTI_PROPERTY_LIST_TEMPL]
#define objc_class_ext_template objc_global_trees[OCTI_CLASS_EXT_TEMPL]
#define UOBJC_CLASS_EXT_decl objc_global_trees[OCTI_CLASS_EXT_DECL]
#define UOBJC_PROTOCOL_EXT_decl objc_global_trees[OCTI_PROTOCOL_EXT_DECL]
#define UOBJC_PROTOCOL_OPT_NST_METHODS_decl objc_global_trees[OCTI_PROTOCOL_OPT_NST_METH_DECL]
#define UOBJC_PROTOCOL_OPT_CLS_METHODS_decl objc_global_trees[OCTI_PROTOCOL_OPT_CLS_METH_DECL]
/* APPLE LOCAL end radar 4585769 - Objective-C 1.0 extensions */
/* APPLE LOCAL begin radar 4947014 - objc atomic property */
#define umsg_GetAtomicProperty		objc_global_trees[OCTI_UMSG_GETATOMICPROPERTY]
#define umsg_SetAtomicProperty		objc_global_trees[OCTI_UMSG_SETATOMICPROPERTY]
#define umsg_CopyAtomicStruct		objc_global_trees[OCTI_UMSG_COPYATOMICSTRUCT]
#define umsg_NSCopying_Type		objc_global_trees[OCTI_UMSG_NSCOPYING]
/* APPLE LOCAL end radar 4947014 - objc atomic property */
/* APPLE LOCAL begin radar 2848255 */
#define UOBJC2_EHTYPE_VTABLE_decl       objc_global_trees[OCTI_EHTYPE_VTABLE]
#define objc2_ehtype_template           objc_global_trees[OCTI_EHTYPE]
#define objc2_begin_catch_decl          objc_global_trees[OCTI_BEGIN_CATCH]
#define objc2_end_catch_decl            objc_global_trees[OCTI_END_CATCH]
#define UOBJC2_EHTYPE_id_decl           objc_global_trees[OCTI_EHTYPE_ID]
/* APPLE LOCAL end radar 2848255 */
/* APPLE LOCAL radar 4951615 */
#define IVAR_PUBLIC_OR_PROTECTED(NODE) (DECL_LANG_FLAG_0 (NODE))
/* APPLE LOCAL begin radar 4965989 */
#define ANONYMOUS_CATEGORY(X) (TREE_CODE (X) == CATEGORY_INTERFACE_TYPE \
                                && CLASS_SUPER_NAME (X) == NULL_TREE)
/* APPLE LOCAL end radar 4965989 */
/* APPLE LOCAL begin radar 4947014 - objc atomic property */
#define ATOMIC_PROPERTY(NODE) (TREE_LANG_FLAG_1 (NODE))
#define IS_ATOMIC(PROPERTY)   (ATOMIC_PROPERTY(PROPERTY))
/* APPLE LOCAL end radar 4947014 - objc atomic property */
/* APPLE LOCAL radar 4653422 */
#define OPTIONAL_PROPERTY(PROPERTY_NODE) (TREE_LANG_FLAG_2 (PROPERTY_NODE))
#endif /* GCC_OBJC_ACT_H */

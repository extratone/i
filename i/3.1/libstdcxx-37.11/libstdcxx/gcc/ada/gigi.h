/****************************************************************************
 *                                                                          *
 *                         GNAT COMPILER COMPONENTS                         *
 *                                                                          *
 *                                 G I G I                                  *
 *                                                                          *
 *                              C Header File                               *
 *                                                                          *
 *          Copyright (C) 1992-2006 Free Software Foundation, Inc.          *
 *                                                                          *
 * GNAT is free software;  you can  redistribute it  and/or modify it under *
 * terms of the  GNU General Public License as published  by the Free Soft- *
 * ware  Foundation;  either version 2,  or (at your option) any later ver- *
 * sion.  GNAT is distributed in the hope that it will be useful, but WITH- *
 * OUT ANY WARRANTY;  without even the  implied warranty of MERCHANTABILITY *
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License *
 * for  more details.  You should have  received  a copy of the GNU General *
 * Public License  distributed with GNAT;  see file COPYING.  If not, write *
 * to  the  Free Software Foundation,  51  Franklin  Street,  Fifth  Floor, *
 * Boston, MA 02110-1301, USA.                                              *
 *                                                                          *
 * As a  special  exception,  if you  link  this file  with other  files to *
 * produce an executable,  this file does not by itself cause the resulting *
 * executable to be covered by the GNU General Public License. This except- *
 * ion does not  however invalidate  any other reasons  why the  executable *
 * file might be covered by the  GNU Public License.                        *
 *                                                                          *
 * GNAT was originally developed  by the GNAT team at  New York University. *
 * Extensive contributions were provided by Ada Core Technologies Inc.      *
 *                                                                          *
 ****************************************************************************/

/* Declare all functions and types used by gigi.  */

/* The largest alignment, in bits, that is needed for using the widest
   move instruction.  */
extern unsigned int largest_move_alignment;

/* Compute the alignment of the largest mode that can be used for copying
   objects.  */
extern void gnat_compute_largest_alignment (void);

/* GNU_TYPE is a type. Determine if it should be passed by reference by
   default.  */
extern bool default_pass_by_ref (tree gnu_type);

/* GNU_TYPE is the type of a subprogram parameter.  Determine from the type
   if it should be passed by reference.  */
extern bool must_pass_by_ref (tree gnu_type);

/* Initialize DUMMY_NODE_TABLE.  */
extern void init_dummy_type (void);

/* Given GNAT_ENTITY, a GNAT defining identifier node, which denotes some Ada
   entity, this routine returns the equivalent GCC tree for that entity
   (an ..._DECL node) and associates the ..._DECL node with the input GNAT
   defining identifier.

   If GNAT_ENTITY is a variable or a constant declaration, GNU_EXPR gives its
   initial value (in GCC tree form). This is optional for variables.
   For renamed entities, GNU_EXPR gives the object being renamed.

   DEFINITION is nonzero if this call is intended for a definition.  This is
   used for separate compilation where it necessary to know whether an
   external declaration or a definition should be created if the GCC equivalent
   was not created previously.  The value of 1 is normally used for a nonzero
   DEFINITION, but a value of 2 is used in special circumstances, defined in
   the code.  */
extern tree gnat_to_gnu_entity (Entity_Id gnat_entity, tree gnu_expr,
                                int definition);

/* Similar, but if the returned value is a COMPONENT_REF, return the
   FIELD_DECL.  */
extern tree gnat_to_gnu_field_decl (Entity_Id gnat_entity);

/* Given GNAT_ENTITY, an entity in the incoming GNAT tree, return a
   GCC type corresponding to that entity.  GNAT_ENTITY is assumed to
   refer to an Ada type.  */
extern tree gnat_to_gnu_type (Entity_Id gnat_entity);

/* Add GNU_STMT to the current BLOCK_STMT node.  */
extern void add_stmt (tree gnu_stmt);

/* Similar, but set the location of GNU_STMT to that of GNAT_NODE.  */
extern void add_stmt_with_node (tree gnu_stmt, Node_Id gnat_node);

/* Set the BLOCK node corresponding to the current code group to GNU_BLOCK.  */
extern void set_block_for_group (tree);

/* Add a declaration statement for GNU_DECL to the current BLOCK_STMT node.
   Get SLOC from GNAT_ENTITY.  */
extern void add_decl_expr (tree gnu_decl, Entity_Id gnat_entity);

/* Given GNAT_ENTITY, elaborate all expressions that are required to
   be elaborated at the point of its definition, but do nothing else.  */
extern void elaborate_entity (Entity_Id gnat_entity);

/* Mark GNAT_ENTITY as going out of scope at this point.  Recursively mark
   any entities on its entity chain similarly.  */
extern void mark_out_of_scope (Entity_Id gnat_entity);

/* Make a dummy type corresponding to GNAT_TYPE.  */
extern tree make_dummy_type (Entity_Id gnat_type);

/* Get the unpadded version of a GNAT type.  */
extern tree get_unpadded_type (Entity_Id gnat_entity);

/* Called when we need to protect a variable object using a save_expr.  */
extern tree maybe_variable (tree gnu_operand);

/* Create a record type that contains a field of TYPE with a starting bit
   position so that it is aligned to ALIGN bits and is SIZE bytes long.  */
extern tree make_aligning_type (tree type, int align, tree size);

/* Ensure that TYPE has SIZE and ALIGN.  Make and return a new padded type
   if needed.  We have already verified that SIZE and TYPE are large enough.

   GNAT_ENTITY and NAME_TRAILER are used to name the resulting record and
   to issue a warning.

   IS_USER_TYPE is true if we must be sure we complete the original type.

   DEFINITION is true if this type is being defined.

   SAME_RM_SIZE is true if the RM_Size of the resulting type is to be
   set to its TYPE_SIZE; otherwise, it's set to the RM_Size of the original
   type.  */
extern tree maybe_pad_type (tree type, tree size, unsigned int align,
                            Entity_Id gnat_entity, const char *name_trailer,
			    bool is_user_type, bool definition,
                            bool same_rm_size);

/* Given a GNU tree and a GNAT list of choices, generate an expression to test
   the value passed against the list of choices.  */
extern tree choices_to_gnu (tree operand, Node_Id choices);

/* Given a type T, a FIELD_DECL F, and a replacement value R,
   return a new type with all size expressions that contain F
   updated by replacing F with R.  This is identical to GCC's
   substitute_in_type except that it knows about TYPE_INDEX_TYPE.  */
extern tree gnat_substitute_in_type (tree t, tree f, tree r);

/* Return the "RM size" of GNU_TYPE.  This is the actual number of bits
   needed to represent the object.  */
extern tree rm_size (tree gnu_type);

/* Given GNU_ID, an IDENTIFIER_NODE containing a name, and SUFFIX, a
   string, return a new IDENTIFIER_NODE that is the concatenation of
   the name in GNU_ID and SUFFIX.  */
extern tree concat_id_with_name (tree gnu_id, const char *suffix);

/* Return the name to be used for GNAT_ENTITY.  If a type, create a
   fully-qualified name, possibly with type information encoding.
   Otherwise, return the name.  */
extern tree get_entity_name (Entity_Id gnat_entity);

/* Return a name for GNAT_ENTITY concatenated with two underscores and
   SUFFIX.  */
extern tree create_concat_name (Entity_Id gnat_entity, const char *suffix);

/* If true, then gigi is being called on an analyzed but unexpanded tree, and
   the only purpose of the call is to properly annotate types with
   representation information.  */
extern bool type_annotate_only;

/* Current file name without path */
extern const char *ref_filename;

/* This is the main program of the back-end.  It sets up all the table
   structures and then generates code.

   ??? Needs parameter descriptions  */

extern void gigi (Node_Id gnat_root, int max_gnat_node, int number_name,
                  struct Node *nodes_ptr, Node_Id *next_node_ptr,
                  Node_Id *prev_node_ptr, struct Elist_Header *elists_ptr,
                  struct Elmt_Item *elmts_ptr,
                  struct String_Entry *strings_ptr,
                  Char_Code *strings_chars_ptr,
                  struct List_Header *list_headers_ptr,
		  Int number_units ATTRIBUTE_UNUSED,
                  char *file_info_ptr ATTRIBUTE_UNUSED,
                  Entity_Id standard_integer,
                  Entity_Id standard_long_long_float,
                  Entity_Id standard_exception_type,
                  Int gigi_operating_mode);

/* GNAT_NODE is the root of some GNAT tree.  Return the root of the
   GCC tree corresponding to that GNAT tree.  Normally, no code is generated;
   we just return an equivalent tree which is used elsewhere to generate
   code.  */
extern tree gnat_to_gnu (Node_Id gnat_node);

/* GNU_STMT is a statement.  We generate code for that statement.  */
extern void gnat_expand_stmt (tree gnu_stmt);

/* ??? missing documentation */
extern int gnat_gimplify_expr (tree *expr_p, tree *pre_p,
                               tree *post_p ATTRIBUTE_UNUSED);

/* Do the processing for the declaration of a GNAT_ENTITY, a type.  If
   a separate Freeze node exists, delay the bulk of the processing.  Otherwise
   make a GCC type for GNAT_ENTITY and set up the correspondence.  */
extern void process_type (Entity_Id gnat_entity);

/* Convert Sloc into *LOCUS (a location_t).  Return true if this Sloc
   corresponds to a source code location and false if it doesn't.  In the
   latter case, we don't update *LOCUS.  We also set the Gigi global variable
   REF_FILENAME to the reference file name as given by sinput (i.e no
   directory).  */
extern bool Sloc_to_locus (Source_Ptr Sloc, location_t *locus);

/* Post an error message.  MSG is the error message, properly annotated.
   NODE is the node at which to post the error and the node to use for the
   "&" substitution.  */
extern void post_error (const char *, Node_Id);

/* Similar, but NODE is the node at which to post the error and ENT
   is the node to use for the "&" substitution.  */
extern void post_error_ne (const char *msg, Node_Id node, Entity_Id ent);

/* Similar, but NODE is the node at which to post the error, ENT is the node
   to use for the "&" substitution, and N is the number to use for the ^.  */
extern void post_error_ne_num (const char *msg, Node_Id node, Entity_Id ent,
                               int n);

/* Similar to post_error_ne_num, but T is a GCC tree representing the number
   to write.  If the tree represents a constant that fits within a
   host integer, the text inside curly brackets in MSG will be output
   (presumably including a '^').  Otherwise that text will not be output
   and the text inside square brackets will be output instead.  */
extern void post_error_ne_tree (const char *msg, Node_Id node, Entity_Id ent,
                                tree t);

/* Similar to post_error_ne_tree, except that NUM is a second
   integer to write in the message.  */
extern void post_error_ne_tree_2 (const char *msg, Node_Id node, Entity_Id ent,
                                  tree t, int num);

/* Protect EXP from multiple evaluation.  This may make a SAVE_EXPR.  */
extern tree protect_multiple_eval (tree exp);

/* Initialize the table that maps GNAT codes to GCC codes for simple
   binary and unary operations.  */
extern void init_code_table (void);

/* Current node being treated, in case gigi_abort or Check_Elaboration_Code
   called.  */
extern Node_Id error_gnat_node;

/* This is equivalent to stabilize_reference in GCC's tree.c, but we know how
   to handle our new nodes and we take extra arguments.

   FORCE says whether to force evaluation of everything,

   SUCCESS we set to true unless we walk through something we don't
   know how to stabilize, or through something which is not an lvalue
   and LVALUES_ONLY is true, in which cases we set to false.  */
extern tree maybe_stabilize_reference (tree ref, bool force, bool lvalues_only,
				       bool *success);

/* Wrapper around maybe_stabilize_reference, for common uses without
   lvalue restrictions and without need to examine the success
   indication.  */

extern tree gnat_stabilize_reference (tree ref, bool force);

/* Highest number in the front-end node table.  */
extern int max_gnat_nodes;

/* If nonzero, pretend we are allocating at global level.  */
extern int force_global;

/* Standard data type sizes.  Most of these are not used.  */

#ifndef CHAR_TYPE_SIZE
#define CHAR_TYPE_SIZE BITS_PER_UNIT
#endif

#ifndef SHORT_TYPE_SIZE
#define SHORT_TYPE_SIZE (BITS_PER_UNIT * MIN ((UNITS_PER_WORD + 1) / 2, 2))
#endif

#ifndef INT_TYPE_SIZE
#define INT_TYPE_SIZE BITS_PER_WORD
#endif

#ifndef LONG_TYPE_SIZE
#define LONG_TYPE_SIZE BITS_PER_WORD
#endif

#ifndef LONG_LONG_TYPE_SIZE
#define LONG_LONG_TYPE_SIZE (BITS_PER_WORD * 2)
#endif

#ifndef FLOAT_TYPE_SIZE
#define FLOAT_TYPE_SIZE BITS_PER_WORD
#endif

#ifndef DOUBLE_TYPE_SIZE
#define DOUBLE_TYPE_SIZE (BITS_PER_WORD * 2)
#endif

#ifndef LONG_DOUBLE_TYPE_SIZE
#define LONG_DOUBLE_TYPE_SIZE (BITS_PER_WORD * 2)
#endif

/* The choice of SIZE_TYPE here is very problematic.  We need a signed
   type whose bit width is Pmode.  Assume "long" is such a type here.  */
#undef SIZE_TYPE
#define SIZE_TYPE "long int"

/* Data structures used to represent attributes.  */

enum attr_type
{
  ATTR_MACHINE_ATTRIBUTE,
  ATTR_LINK_ALIAS,
  ATTR_LINK_SECTION,
  ATTR_LINK_CONSTRUCTOR,
  ATTR_LINK_DESTRUCTOR,
  ATTR_WEAK_EXTERNAL
};

struct attrib
{
  struct attrib *next;
  enum attr_type type;
  tree name;
  tree args;
  Node_Id error_point;
};

/* Table of machine-independent internal attributes.  */
extern const struct attribute_spec gnat_internal_attribute_table[];

/* Define the entries in the standard data array.  */
enum standard_datatypes
{
/* Various standard data types and nodes.  */
  ADT_longest_float_type,
  ADT_void_type_decl,

  /* The type of an exception.  */
  ADT_except_type,

  /* Type declaration node  <==> typedef void *T */
  ADT_ptr_void_type,

  /* Function type declaration -- void T() */
  ADT_void_ftype,

  /* Type declaration node  <==> typedef void *T() */
  ADT_ptr_void_ftype,

  /* A function declaration node for a run-time function for allocating memory.
     Ada allocators cause calls to this function to be generated.   */
  ADT_malloc_decl,

  /* Likewise for freeing memory.  */
  ADT_free_decl,

  /* Types and decls used by our temporary exception mechanism.  See
     init_gigi_decls for details.  */
  ADT_jmpbuf_type,
  ADT_jmpbuf_ptr_type,
  ADT_get_jmpbuf_decl,
  ADT_set_jmpbuf_decl,
  ADT_get_excptr_decl,
  ADT_setjmp_decl,
  ADT_longjmp_decl,
  ADT_update_setjmp_buf_decl,
  ADT_raise_nodefer_decl,
  ADT_begin_handler_decl,
  ADT_end_handler_decl,
  ADT_others_decl,
  ADT_all_others_decl,
  ADT_LAST};

extern GTY(()) tree gnat_std_decls[(int) ADT_LAST];
extern GTY(()) tree gnat_raise_decls[(int) LAST_REASON_CODE + 1];

extern GTY(()) tree static_ctors;
extern GTY(()) tree static_dtors;

#define longest_float_type_node gnat_std_decls[(int) ADT_longest_float_type]
#define void_type_decl_node gnat_std_decls[(int) ADT_void_type_decl]
#define except_type_node gnat_std_decls[(int) ADT_except_type]
#define ptr_void_type_node gnat_std_decls[(int) ADT_ptr_void_type]
#define void_ftype gnat_std_decls[(int) ADT_void_ftype]
#define ptr_void_ftype gnat_std_decls[(int) ADT_ptr_void_ftype]
#define malloc_decl gnat_std_decls[(int) ADT_malloc_decl]
#define free_decl gnat_std_decls[(int) ADT_free_decl]
#define jmpbuf_type gnat_std_decls[(int) ADT_jmpbuf_type]
#define jmpbuf_ptr_type gnat_std_decls[(int) ADT_jmpbuf_ptr_type]
#define get_jmpbuf_decl gnat_std_decls[(int) ADT_get_jmpbuf_decl]
#define set_jmpbuf_decl gnat_std_decls[(int) ADT_set_jmpbuf_decl]
#define get_excptr_decl gnat_std_decls[(int) ADT_get_excptr_decl]
#define setjmp_decl gnat_std_decls[(int) ADT_setjmp_decl]
#define longjmp_decl gnat_std_decls[(int) ADT_longjmp_decl]
#define update_setjmp_buf_decl gnat_std_decls[(int) ADT_update_setjmp_buf_decl]
#define raise_nodefer_decl gnat_std_decls[(int) ADT_raise_nodefer_decl]
#define begin_handler_decl gnat_std_decls[(int) ADT_begin_handler_decl]
#define others_decl gnat_std_decls[(int) ADT_others_decl]
#define all_others_decl gnat_std_decls[(int) ADT_all_others_decl]
#define end_handler_decl gnat_std_decls[(int) ADT_end_handler_decl]

/* Routines expected by the gcc back-end. They must have exactly the same
   prototype and names as below.  */

/* Returns nonzero if we are currently in the global binding level.  */
extern int global_bindings_p (void);

/* Enter and exit a new binding level. */
extern void gnat_pushlevel (void);
extern void gnat_poplevel (void);

/* Set SUPERCONTEXT of the BLOCK for the current binding level to FNDECL
   and point FNDECL to this BLOCK.  */
extern void set_current_block_context (tree fndecl);

/* Set the jmpbuf_decl for the current binding level to DECL.  */
extern void set_block_jmpbuf_decl (tree decl);

/* Get the setjmp_decl, if any, for the current binding level.  */
extern tree get_block_jmpbuf_decl (void);

/* Insert BLOCK at the end of the list of subblocks of the
   current binding level.  This is used when a BIND_EXPR is expanded,
   to handle the BLOCK node inside the BIND_EXPR.  */
extern void insert_block (tree block);

/* Records a ..._DECL node DECL as belonging to the current lexical scope
   and uses GNAT_NODE for location information.  */
extern void gnat_pushdecl (tree decl, Node_Id gnat_node);

extern void gnat_init_stmt_group (void);
extern void gnat_init_decl_processing (void);
extern void init_gigi_decls (tree long_long_float_type, tree exception_type);
extern void gnat_init_gcc_eh (void);

/* Return an integer type with the number of bits of precision given by
   PRECISION.  UNSIGNEDP is nonzero if the type is unsigned; otherwise
   it is a signed type.  */
extern tree gnat_type_for_size (unsigned precision, int unsignedp);

/* Return a data type that has machine mode MODE.  UNSIGNEDP selects
   an unsigned type; otherwise a signed type is returned.  */
extern tree gnat_type_for_mode (enum machine_mode mode, int unsignedp);

/* Return the unsigned version of a TYPE_NODE, a scalar type.  */
extern tree gnat_unsigned_type (tree type_node);

/* Return the signed version of a TYPE_NODE, a scalar type.  */
extern tree gnat_signed_type (tree type_node);

/* Return a type the same as TYPE except unsigned or signed according to
   UNSIGNEDP.  */
extern tree gnat_signed_or_unsigned_type (int unsignedp, tree type);

/* Create an expression whose value is that of EXPR,
   converted to type TYPE.  The TREE_TYPE of the value
   is always TYPE.  This function implements all reasonable
   conversions; callers should filter out those that are
   not permitted by the language being compiled.  */
extern tree convert (tree type, tree expr);

/* Routines created solely for the tree translator's sake. Their prototypes
   can be changed as desired.  */

/* GNAT_ENTITY is a GNAT tree node for a defining identifier.
   GNU_DECL is the GCC tree which is to be associated with
   GNAT_ENTITY. Such gnu tree node is always an ..._DECL node.
   If NO_CHECK is nonzero, the latter check is suppressed.
   If GNU_DECL is zero, a previous association is to be reset.  */
extern void save_gnu_tree (Entity_Id gnat_entity, tree gnu_decl,
                           bool no_check);

/* GNAT_ENTITY is a GNAT tree node for a defining identifier.
   Return the ..._DECL node that was associated with it.  If there is no tree
   node associated with GNAT_ENTITY, abort.  */
extern tree get_gnu_tree (Entity_Id gnat_entity);

/* Return nonzero if a GCC tree has been associated with GNAT_ENTITY.  */
extern bool present_gnu_tree (Entity_Id gnat_entity);

/* Initialize tables for above routines.  */
extern void init_gnat_to_gnu (void);

/* Given a record type (RECORD_TYPE) and a chain of FIELD_DECL
   nodes (FIELDLIST), finish constructing the record or union type.
   If HAS_REP is true, this record has a rep clause; don't call
   layout_type but merely set the size and alignment ourselves.
   If DEFER_DEBUG is true, do not call the debugging routines
   on this type; it will be done later. */
extern void finish_record_type (tree record_type, tree fieldlist,
                                bool has_rep, bool defer_debug);

/*  Output the debug information associated to a record type.  */
extern void write_record_type_debug_info (tree);

/* Returns a FUNCTION_TYPE node. RETURN_TYPE is the type returned by the
   subprogram. If it is void_type_node, then we are dealing with a procedure,
   otherwise we are dealing with a function. PARAM_DECL_LIST is a list of
   PARM_DECL nodes that are the subprogram arguments.  CICO_LIST is the
   copy-in/copy-out list to be stored into TYPE_CI_CO_LIST.
   RETURNS_UNCONSTRAINED is true if the function returns an unconstrained
   object.  RETURNS_BY_REF is true if the function returns by reference.
   RETURNS_WITH_DSP is true if the function is to return with a
   depressed stack pointer.  RETURNS_BY_TARGET_PTR is true if the function
   is to be passed (as its first parameter) the address of the place to copy
   its result.  */
extern tree create_subprog_type (tree return_type, tree param_decl_list,
                                 tree cico_list, bool returns_unconstrained,
                                 bool returns_by_ref, bool returns_with_dsp,
                                 bool returns_by_target_ptr);

/* Return a copy of TYPE, but safe to modify in any way.  */
extern tree copy_type (tree type);

/* Return an INTEGER_TYPE of SIZETYPE with range MIN to MAX and whose
   TYPE_INDEX_TYPE is INDEX.  */
extern tree create_index_type (tree min, tree max, tree index);

/* Return a TYPE_DECL node. TYPE_NAME gives the name of the type (a character
   string) and TYPE is a ..._TYPE node giving its data type.
   ARTIFICIAL_P is true if this is a declaration that was generated
   by the compiler.  DEBUG_INFO_P is true if we need to write debugging
   information about this type.  GNAT_NODE is used for the position of
   the decl.  */
extern tree create_type_decl (tree type_name, tree type,
                              struct attrib *attr_list,
                              bool artificial_p, bool debug_info_p,
			      Node_Id gnat_node);

/* Returns a GCC VAR_DECL node. VAR_NAME gives the name of the variable.
   ASM_NAME is its assembler name (if provided).  TYPE is
   its data type (a GCC ..._TYPE node).  VAR_INIT is the GCC tree for an
   optional initial expression; NULL_TREE if none.

   CONST_FLAG is true if this variable is constant.

   PUBLIC_FLAG is true if this definition is to be made visible outside of
   the current compilation unit. This flag should be set when processing the
   variable definitions in a package specification.  EXTERN_FLAG is nonzero
   when processing an external variable declaration (as opposed to a
   definition: no storage is to be allocated for the variable here).

   STATIC_FLAG is only relevant when not at top level.  In that case
   it indicates whether to always allocate storage to the variable.

   GNAT_NODE is used for the position of the decl.  */
extern tree create_var_decl (tree var_name, tree asm_name, tree type,
                             tree var_init, bool const_flag,
                             bool public_flag, bool extern_flag,
                             bool static_flag,
			     struct attrib *attr_list, Node_Id gnat_node);

/* Given a DECL and ATTR_LIST, apply the listed attributes.  */
extern void process_attributes (tree decl, struct attrib *attr_list);

/* Returns a FIELD_DECL node. FIELD_NAME the field name, FIELD_TYPE is its
   type, and RECORD_TYPE is the type of the parent.  PACKED is nonzero if
   this field is in a record type with a "pragma pack".  If SIZE is nonzero
   it is the specified size for this field.  If POS is nonzero, it is the bit
   position.  If ADDRESSABLE is nonzero, it means we are allowed to take
   the address of this field for aliasing purposes.  */
extern tree create_field_decl (tree field_name, tree field_type,
                               tree record_type, int packed, tree size,
                               tree pos, int addressable);

/* Returns a PARM_DECL node. PARAM_NAME is the name of the parameter,
   PARAM_TYPE is its type.  READONLY is true if the parameter is
   readonly (either an IN parameter or an address of a pass-by-ref
   parameter). */
extern tree create_param_decl (tree param_name, tree param_type,
                               bool readonly);

/* Returns a FUNCTION_DECL node.  SUBPROG_NAME is the name of the subprogram,
   ASM_NAME is its assembler name, SUBPROG_TYPE is its type (a FUNCTION_TYPE
   node), PARAM_DECL_LIST is the list of the subprogram arguments (a list of
   PARM_DECL nodes chained through the TREE_CHAIN field).

   INLINE_FLAG, PUBLIC_FLAG, EXTERN_FLAG, and ATTR_LIST are used to set the
   appropriate fields in the FUNCTION_DECL.  GNAT_NODE gives the location.  */
extern tree create_subprog_decl (tree subprog_name, tree asm_name,
                                 tree subprog_type, tree param_decl_list,
                                 bool inlinee_flag, bool public_flag,
                                 bool extern_flag,
				 struct attrib *attr_list, Node_Id gnat_node);

/* Returns a LABEL_DECL node for LABEL_NAME.  */
extern tree create_label_decl (tree label_name);

/* Set up the framework for generating code for SUBPROG_DECL, a subprogram
   body. This routine needs to be invoked before processing the declarations
   appearing in the subprogram.  */
extern void begin_subprog_body (tree subprog_decl);

/* Finish the definition of the current subprogram and compile it all the way
   to assembler language output.  BODY is the tree corresponding to
   the subprogram.  */
extern void end_subprog_body (tree body);

/* Build a template of type TEMPLATE_TYPE from the array bounds of ARRAY_TYPE.
   EXPR is an expression that we can use to locate any PLACEHOLDER_EXPRs.
   Return a constructor for the template.  */
extern tree build_template (tree template_type, tree array_type, tree expr);

/* Build a VMS descriptor from a Mechanism_Type, which must specify
   a descriptor type, and the GCC type of an object.  Each FIELD_DECL
   in the type contains in its DECL_INITIAL the expression to use when
   a constructor is made for the type.  GNAT_ENTITY is a gnat node used
   to print out an error message if the mechanism cannot be applied to
   an object of that type and also for the name.  */

extern tree build_vms_descriptor (tree type, Mechanism_Type mech,
                                  Entity_Id gnat_entity);

/* Build a type to be used to represent an aliased object whose nominal
   type is an unconstrained array.  This consists of a RECORD_TYPE containing
   a field of TEMPLATE_TYPE and a field of OBJECT_TYPE, which is an
   ARRAY_TYPE.  If ARRAY_TYPE is that of the unconstrained array, this
   is used to represent an arbitrary unconstrained object.  Use NAME
   as the name of the record.  */
extern tree build_unc_object_type (tree template_type, tree object_type,
                                   tree name);

/* Same as build_unc_object_type, but taking a thin or fat pointer type
   instead of the template type. */
extern tree build_unc_object_type_from_ptr (tree thin_fat_ptr_type,
					    tree object_type, tree name);

/* Update anything previously pointing to OLD_TYPE to point to NEW_TYPE.  In
   the normal case this is just two adjustments, but we have more to do
   if NEW is an UNCONSTRAINED_ARRAY_TYPE.  */
extern void update_pointer_to (tree old_type, tree new_type);

/* EXP is an expression for the size of an object.  If this size contains
   discriminant references, replace them with the maximum (if MAX_P) or
   minimum (if !MAX_P) possible value of the discriminant.  */
extern tree max_size (tree exp, bool max_p);

/* Remove all conversions that are done in EXP.  This includes converting
   from a padded type or to a left-justified modular type.  If TRUE_ADDRESS
   is true, always return the address of the containing object even if
   the address is not bit-aligned.  */
extern tree remove_conversions (tree exp, bool true_address);

/* If EXP's type is an UNCONSTRAINED_ARRAY_TYPE, return an expression that
   refers to the underlying array.  If its type has TYPE_CONTAINS_TEMPLATE_P,
   likewise return an expression pointing to the underlying array.  */
extern tree maybe_unconstrained_array (tree exp);

/* Return an expression that does an unchecked conversion of EXPR to TYPE.
   If NOTRUNC_P is true, truncation operations should be suppressed.  */
extern tree unchecked_convert (tree type, tree expr, bool notrunc_p);

/* Prepare expr to be an argument of a TRUTH_NOT_EXPR or other logical
   operation.

   This preparation consists of taking the ordinary
   representation of an expression EXPR and producing a valid tree
   boolean expression describing whether EXPR is nonzero.  We could
   simply always do build_binary_op (NE_EXPR, expr, integer_zero_node, 1),
   but we optimize comparisons, &&, ||, and !.

   The resulting type should always be the same as the input type.
   This function is simpler than the corresponding C version since
   the only possible operands will be things of Boolean type.  */
extern tree gnat_truthvalue_conversion (tree expr);

/* Return the base type of TYPE.  */
extern tree get_base_type (tree type);

/* EXP is a GCC tree representing an address.  See if we can find how
   strictly the object at that address is aligned.   Return that alignment
   strictly the object at that address is aligned.   Return that alignment
   in bits.  If we don't know anything about the alignment, return 0.  */
extern unsigned int known_alignment (tree exp);

/* Return true if VALUE is a multiple of FACTOR. FACTOR must be a power
   of 2. */
extern bool value_factor_p (tree value, HOST_WIDE_INT factor);

/* Make a binary operation of kind OP_CODE.  RESULT_TYPE is the type
   desired for the result.  Usually the operation is to be performed
   in that type.  For MODIFY_EXPR and ARRAY_REF, RESULT_TYPE may be 0
   in which case the type to be used will be derived from the operands.  */
extern tree build_binary_op (enum tree_code op_code, tree retult_type,
                             tree left_operand, tree right_operand);

/* Similar, but make unary operation.   */
extern tree build_unary_op (enum tree_code op_code, tree result_type,
                            tree operand);

/* Similar, but for COND_EXPR.  */
extern tree build_cond_expr (tree result_type, tree condition_operand,
                             tree true_operand, tree false_operand);

/* Similar, but for RETURN_EXPR.  */
extern tree build_return_expr (tree result_decl, tree ret_val);

/* Build a CALL_EXPR to call FUNDECL with one argument, ARG.  Return
   the CALL_EXPR.  */
extern tree build_call_1_expr (tree fundecl, tree arg);

/* Build a CALL_EXPR to call FUNDECL with two argument, ARG1 & ARG2.  Return
   the CALL_EXPR.  */
extern tree build_call_2_expr (tree fundecl, tree arg1, tree arg2);

/* Likewise to call FUNDECL with no arguments.  */
extern tree build_call_0_expr (tree fundecl);

/* Call a function that raises an exception and pass the line number and file
   name, if requested.  MSG says which exception function to call.

   GNAT_NODE is the gnat node conveying the source location for which the
   error should be signaled, or Empty in which case the error is signaled on
   the current ref_file_name/input_line.  */
extern tree build_call_raise (int msg, Node_Id gnat_node);

/* Return a CONSTRUCTOR of TYPE whose list is LIST.  This is not the
   same as build_constructor in the language-independent tree.c.  */
extern tree gnat_build_constructor (tree type, tree list);

/* Return a COMPONENT_REF to access a field that is given by COMPONENT,
   an IDENTIFIER_NODE giving the name of the field, FIELD, a FIELD_DECL,
   for the field, or both.  Don't fold the result if NO_FOLD_P.  */
extern tree build_component_ref (tree record_variable, tree component,
                                 tree field, bool no_fold_p);

/* Build a GCC tree to call an allocation or deallocation function.
   If GNU_OBJ is nonzero, it is an object to deallocate.  Otherwise,
   genrate an allocator.

   GNU_SIZE is the size of the object and ALIGN is the alignment.
   GNAT_PROC, if present is a procedure to call and GNAT_POOL is the
   storage pool to use.  If not preset, malloc and free will be used.  */
extern tree build_call_alloc_dealloc (tree gnu_obj, tree gnu_size,
                                      unsigned align, Entity_Id gnat_proc,
				      Entity_Id gnat_pool, Node_Id gnat_node);

/* Build a GCC tree to correspond to allocating an object of TYPE whose
   initial value if INIT, if INIT is nonzero.  Convert the expression to
   RESULT_TYPE, which must be some type of pointer.  Return the tree.
   GNAT_PROC and GNAT_POOL optionally give the procedure to call and
   the storage pool to use.  GNAT_NODE is used to provide an error
   location for restriction violations messages.  If IGNORE_INIT_TYPE is
   true, ignore the type of INIT for the purpose of determining the size;
   this will cause the maximum size to be allocated if TYPE is of
   self-referential size.  */
extern tree build_allocator (tree type, tree init, tree result_type,
                             Entity_Id gnat_proc, Entity_Id gnat_pool,
                             Node_Id gnat_node, bool);

/* Fill in a VMS descriptor for EXPR and return a constructor for it.
   GNAT_FORMAL is how we find the descriptor record.  */

extern tree fill_vms_descriptor (tree expr, Entity_Id gnat_formal);

/* Indicate that we need to make the address of EXPR_NODE and it therefore
   should not be allocated in a register.  Return true if successful.  */
extern bool gnat_mark_addressable (tree expr_node);

/* Implementation of the builtin_function langhook.  */
extern tree builtin_function (const char *name, tree type, int function_code,
                              enum built_in_class class,
			      const char *library_name, tree attrs);

/* Search the chain of currently reachable declarations for a builtin
   FUNCTION_DECL node corresponding to function NAME (an IDENTIFIER_NODE).
   Return the first node found, if any, or NULL_TREE otherwise.  */
extern tree builtin_decl_for (tree name ATTRIBUTE_UNUSED);

/* This function is called by the front end to enumerate all the supported
   modes for the machine.  We pass a function which is called back with
   the following integer parameters:

   FLOAT_P	nonzero if this represents a floating-point mode
   COMPLEX_P	nonzero is this represents a complex mode
   COUNT	count of number of items, nonzero for vector mode
   PRECISION	number of bits in data representation
   MANTISSA	number of bits in mantissa, if FP and known, else zero.
   SIZE		number of bits used to store data
   ALIGN	number of bits to which mode is aligned.  */
extern void enumerate_modes (void (*f) (int, int, int, int, int, int,
					unsigned int));

/* These are temporary function to deal with recent GCC changes related to
   FP type sizes and precisions.  */
extern int fp_prec_to_size (int prec);
extern int fp_size_to_prec (int size);

/* These functions return the basic data type sizes and related parameters
   about the target machine.  */

extern Pos get_target_bits_per_unit (void);
extern Pos get_target_bits_per_word (void);
extern Pos get_target_char_size (void);
extern Pos get_target_wchar_t_size (void);
extern Pos get_target_short_size (void);
extern Pos get_target_int_size (void);
extern Pos get_target_long_size (void);
extern Pos get_target_long_long_size (void);
extern Pos get_target_float_size (void);
extern Pos get_target_double_size (void);
extern Pos get_target_long_double_size (void);
extern Pos get_target_pointer_size (void);
extern Pos get_target_maximum_alignment (void);
extern Nat get_float_words_be (void);
extern Nat get_words_be (void);
extern Nat get_bytes_be (void);
extern Nat get_bits_be (void);
extern Nat get_strict_alignment (void);

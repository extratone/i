/* Language parser definitions for the GNU compiler for the Java(TM) language.
   Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005
   Free Software Foundation, Inc.
   Contributed by Alexandre Petit-Bianco (apbianco@cygnus.com)

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
Boston, MA 02110-1301, USA.

Java and all Java-based marks are trademarks or registered trademarks
of Sun Microsystems, Inc. in the United States and other countries.
The Free Software Foundation is independent of Sun Microsystems, Inc.  */

#ifndef GCC_JAVA_PARSE_H
#define GCC_JAVA_PARSE_H

#include "lex.h"

/* Extern global variable declarations */
extern int java_error_count;
extern struct obstack temporary_obstack;
extern int quiet_flag;

#ifndef JC1_LITE
/* Function extern to java/ */
extern int int_fits_type_p (tree, tree);
extern tree stabilize_reference (tree);
#endif

/* Macros for verbose debug info  */
#ifdef  VERBOSE_SKELETON
#define RULE( rule ) printf ( "jv_yacc:%d: rule %s\n", lineno, rule )
#else
#define RULE( rule )
#endif

#ifdef VERBOSE_SKELETON
#undef SOURCE_FRONTEND_DEBUG
#define SOURCE_FRONTEND_DEBUG(X)				\
  {if (!quiet_flag) {printf ("* "); printf X; putchar ('\n');} }
#else
#define SOURCE_FRONTEND_DEBUG(X)
#endif

/* Macro for error recovering  */
#ifdef YYDEBUG
#define RECOVERED     					\
  { if (!quiet_flag) {printf ("** Recovered\n");} }
#define DRECOVERED(s) 						\
  { if (!quiet_flag) {printf ("** Recovered (%s)\n", #s);}}
#else
#define RECOVERED
#define DRECOVERED(s)
#endif

#define DRECOVER(s) {yyerrok; DRECOVERED(s);}
#define RECOVER     {yyerrok; RECOVERED;}

#define YYERROR_NOW ctxp->java_error_flag = 1
#define YYNOT_TWICE if (ctxp->prevent_ese != input_line)

/* Accepted modifiers */
#define CLASS_MODIFIERS ACC_PUBLIC|ACC_ABSTRACT|ACC_FINAL|ACC_STRICT
#define FIELD_MODIFIERS ACC_PUBLIC|ACC_PROTECTED|ACC_PRIVATE|ACC_FINAL| \
                        ACC_STATIC|ACC_TRANSIENT|ACC_VOLATILE
#define METHOD_MODIFIERS ACC_PUBLIC|ACC_PROTECTED|ACC_PRIVATE|ACC_ABSTRACT| \
			 ACC_STATIC|ACC_FINAL|ACC_SYNCHRONIZED|ACC_NATIVE| \
			 ACC_STRICT
#define INTERFACE_MODIFIERS ACC_PUBLIC|ACC_ABSTRACT|ACC_STRICT
#define INTERFACE_INNER_MODIFIERS ACC_PUBLIC|ACC_PROTECTED|ACC_ABSTRACT| \
				  ACC_STATIC|ACC_PRIVATE
#define INTERFACE_METHOD_MODIFIERS ACC_PUBLIC|ACC_ABSTRACT
#define INTERFACE_FIELD_MODIFIERS ACC_PUBLIC|ACC_STATIC|ACC_FINAL

/* Getting a modifier WFL */
#define MODIFIER_WFL(M)   (ctxp->modifier_ctx [(M) - PUBLIC_TK])

/* Check on modifiers */
#ifdef USE_MAPPED_LOCATION
#define THIS_MODIFIER_ONLY(f, m, v, count, l)				\
  if ((f) & (m))							\
    {									\
      tree node = MODIFIER_WFL (v);					\
      if (!l)								\
        l = node;							\
      else								\
	{								\
	  expanded_location lloc = expand_location (EXPR_LOCATION (l));	\
	  expanded_location nloc = expand_location (EXPR_LOCATION (node)); \
	  if (nloc.column > lloc.column || nloc.line > lloc.line)	\
	    l = node;							\
	}								\
      count++;								\
    }
#else
#define THIS_MODIFIER_ONLY(f, m, v, count, l)				\
  if ((f) & (m))							\
    {									\
      tree node = MODIFIER_WFL (v);					\
      if ((l)								\
	  && ((EXPR_WFL_COLNO (node) > EXPR_WFL_COLNO (l))		\
	      || (EXPR_WFL_LINENO (node) > EXPR_WFL_LINENO (l))))	\
        l = node;							\
      else if (!(l))							\
        l = node;							\
      count++;								\
    }
#endif

#ifdef ATTRIBUTE_GCC_DIAG
extern void parse_error_context (tree cl, const char *gmsgid, ...) ATTRIBUTE_GCC_DIAG(2,3);
#endif

#define ABSTRACT_CHECK(FLAG, V, CL, S)				\
  if ((FLAG) & (V))						\
    parse_error_context ((CL), "%s method can't be abstract", (S));

#define JCONSTRUCTOR_CHECK(FLAG, V, CL, S)			\
  if ((FLAG) & (V))						\
    parse_error_context ((CL), "Constructor can't be %s", (S));	\
      
/* Misc. */
#define exit_java_complete_class()		\
  {						\
    return;					\
  }

#define CLASS_OR_INTERFACE(decl, s1, s2)			\
   (decl ?							\
    ((get_access_flags_from_decl (TYPE_NAME (TREE_TYPE (decl)))	\
      & ACC_INTERFACE) ?					\
     s2 : s1) : ((s1 [0]=='S'|| s1 [0]=='s') ?			\
		 (s1 [0]=='S' ? "Supertype" : "supertype") :	\
		 (s1 [0] > 'A' ? "Type" : "type")))

#define GET_REAL_TYPE(TYPE) 					\
  (TREE_CODE (TYPE) == TREE_LIST ? TREE_PURPOSE (TYPE) : TYPE)

/* Get TYPE name string, regardless whether TYPE is a class or an
   array. */
#define GET_TYPE_NAME(TYPE)				\
  (TREE_CODE (TYPE_NAME (TYPE)) == IDENTIFIER_NODE ?	\
   IDENTIFIER_POINTER (TYPE_NAME (TYPE)) :		\
   IDENTIFIER_POINTER (DECL_NAME (TYPE_NAME (TYPE))))

/* Pedantic warning on obsolete modifiers. Note: when cl is NULL,
   flags was set artificially, such as for an interface method.  */
#define OBSOLETE_MODIFIER_WARNING(cl, flags, __modifier, arg)                \
  {                                                                          \
    if (flag_redundant && (cl) && ((flags) & (__modifier)))		     \
      parse_warning_context (cl,                                             \
     "Discouraged redundant use of %qs modifier in declaration of %s",      \
			     java_accstring_lookup (__modifier), arg);       \
  }
#define OBSOLETE_MODIFIER_WARNING2(cl, flags, __modifier, arg1, arg2)        \
  {                                                                          \
    if (flag_redundant && (cl) && ((flags) & (__modifier)))		     \
      parse_warning_context (cl,                                             \
     "Discouraged redundant use of %qs modifier in declaration of %s %qs", \
			     java_accstring_lookup (__modifier), arg1, arg2);\
  }

/* Quickly build a temporary pointer on hypothetical type NAME. */
#define BUILD_PTR_FROM_NAME(ptr, name)		\
  do {						\
    ptr = make_node (POINTER_TYPE);		\
    TYPE_NAME (ptr) = name;			\
  } while (0)

#define INCOMPLETE_TYPE_P(NODE)				\
  ((TREE_CODE (NODE) == POINTER_TYPE)			\
   && !TREE_TYPE (NODE) 				\
   && TREE_CODE (TYPE_NAME (NODE)) == IDENTIFIER_NODE)

#ifndef USE_MAPPED_LOCATION
/* Set the EMIT_LINE_NOTE flag of a EXPR_WLF to 1 if debug information
   are requested. Works in the context of a parser rule. */
#define JAVA_MAYBE_GENERATE_DEBUG_INFO(node)		\
  do {if (debug_info_level != DINFO_LEVEL_NONE)	\
      EXPR_WFL_EMIT_LINE_NOTE (node) = 1; } while (0)
#endif

/* Types classification, according to the JLS, section 4.2 */
#define JFLOAT_TYPE_P(TYPE)      (TYPE && TREE_CODE ((TYPE)) == REAL_TYPE)
#define JINTEGRAL_TYPE_P(TYPE)   ((TYPE) 				   \
				  && (TREE_CODE ((TYPE)) == INTEGER_TYPE))
#define JNUMERIC_TYPE_P(TYPE)    ((TYPE)				\
				  && (JFLOAT_TYPE_P ((TYPE))		\
				      || JINTEGRAL_TYPE_P ((TYPE))))
#define JPRIMITIVE_TYPE_P(TYPE)  ((TYPE) 				  \
				  && (JNUMERIC_TYPE_P ((TYPE))		  \
				  || TREE_CODE ((TYPE)) == BOOLEAN_TYPE))

#define JBSC_TYPE_P(TYPE) ((TYPE) && (((TYPE) == byte_type_node)	\
				      || ((TYPE) == short_type_node)	\
				      || ((TYPE) == char_type_node)))

/* Not defined in the LRM */
#define JSTRING_TYPE_P(TYPE) ((TYPE) 					   \
			      && ((TYPE) == string_type_node ||		   \
				  (TREE_CODE (TYPE) == POINTER_TYPE &&	   \
				   TREE_TYPE (TYPE) == string_type_node)))
#define JSTRING_P(NODE) ((NODE)						\
			 && (TREE_CODE (NODE) == STRING_CST		\
			     || IS_CRAFTED_STRING_BUFFER_P (NODE)	\
			     || JSTRING_TYPE_P (TREE_TYPE (NODE))))

#define JREFERENCE_TYPE_P(TYPE) ((TYPE)					      \
				 && (TREE_CODE (TYPE) == RECORD_TYPE 	      \
				     ||	(TREE_CODE (TYPE) == POINTER_TYPE     \
					 &&  TREE_CODE (TREE_TYPE (TYPE)) ==  \
					 RECORD_TYPE)))
#define JNULLP_TYPE_P(TYPE) ((TYPE) && (TREE_CODE (TYPE) == POINTER_TYPE) \
			     && (TYPE) == TREE_TYPE (null_pointer_node))

/* Other predicates */
#define JDECL_P(NODE) (NODE && (TREE_CODE (NODE) == PARM_DECL		\
				|| TREE_CODE (NODE) == VAR_DECL		\
				|| TREE_CODE (NODE) == FIELD_DECL))

#define TYPE_INTERFACE_P(TYPE) 					\
  (CLASS_P (TYPE) && CLASS_INTERFACE (TYPE_NAME (TYPE)))

#define TYPE_CLASS_P(TYPE) (CLASS_P (TYPE) 				\
			    && !CLASS_INTERFACE (TYPE_NAME (TYPE)))

/* Identifier business related to 1.1 language extensions.  */

#define IDENTIFIER_INNER_CLASS_OUTER_FIELD_ACCESS(NODE)	\
  (TREE_CODE (NODE) == IDENTIFIER_NODE &&		\
   IDENTIFIER_LENGTH (NODE) >= 8 &&			\
   IDENTIFIER_POINTER (NODE)[7] != '0')

/* Build the string val$<O> and store it into N. The is used to
   construct the name of inner class hidden fields used to alias outer
   scope local variables.  */
#define MANGLE_OUTER_LOCAL_VARIABLE_NAME(N, O)				\
  {									\
    char *mangled_name;							\
    obstack_grow (&temporary_obstack, "val$", 4);			\
    obstack_grow (&temporary_obstack,					\
		  IDENTIFIER_POINTER ((O)), IDENTIFIER_LENGTH ((O)));	\
    obstack_1grow (&temporary_obstack, '\0');				\
    mangled_name = obstack_finish (&temporary_obstack);			\
    (N) = get_identifier (mangled_name);				\
    obstack_free (&temporary_obstack, mangled_name);			\
  }

/* Build the string parm$<O> and store in into the identifier N. This
   is used to construct the name of hidden parameters used to
   initialize outer scope aliases.  */
#define MANGLE_ALIAS_INITIALIZER_PARAMETER_NAME_ID(N, O)		\
  {									\
    char *mangled_name;							\
    obstack_grow (&temporary_obstack, "parm$", 5);			\
    obstack_grow (&temporary_obstack, 					\
		  IDENTIFIER_POINTER ((O)), IDENTIFIER_LENGTH ((O)));	\
    obstack_1grow (&temporary_obstack, '\0');				\
    mangled_name = obstack_finish (&temporary_obstack);			\
    (N) = get_identifier (mangled_name);				\
    obstack_free (&temporary_obstack, mangled_name);			\
  }

#define MANGLE_ALIAS_INITIALIZER_PARAMETER_NAME_STR(N, S)	\
  {								\
    char *mangled_name;							\
    obstack_grow (&temporary_obstack, "parm$", 5);		\
    obstack_grow (&temporary_obstack, (S), strlen ((S)));	\
    obstack_1grow (&temporary_obstack, '\0');			\
    mangled_name = obstack_finish (&temporary_obstack);			\
    (N) = get_identifier (mangled_name);				\
    obstack_free (&temporary_obstack, mangled_name);			\
  }

/* Skip THIS and artificial parameters found in function decl M and
   assign the result to C. We don't do that for $finit$, since it's
   knowingly called with artificial parms.  */
#define SKIP_THIS_AND_ARTIFICIAL_PARMS(C,M)			\
  {								\
    int i;							\
    (C) = TYPE_ARG_TYPES (TREE_TYPE ((M)));			\
    if (!METHOD_STATIC ((M)))					\
      (C) = TREE_CHAIN (C);					\
    if (DECL_CONSTRUCTOR_P ((M))				\
        && PURE_INNER_CLASS_TYPE_P (DECL_CONTEXT ((M))))	\
      (C) = TREE_CHAIN (C);					\
    if (!DECL_FINIT_P ((M)))					\
      for (i = DECL_FUNCTION_NAP ((M)); i; i--)			\
       (C) = TREE_CHAIN (C);					\
  }

/* Mark final parameters in method M, by comparison of the argument
   list L. This macro is used to set the flag once the method has been
   build.  */
#define MARK_FINAL_PARMS(M, L)						\
  {									\
    tree current = TYPE_ARG_TYPES (TREE_TYPE ((M)));			\
    tree list = (L);							\
    if (!METHOD_STATIC ((M)))						\
      current = TREE_CHAIN (current);					\
    for (; current !=  end_params_node;					\
	 current = TREE_CHAIN (current), list = TREE_CHAIN (list))	\
      ARG_FINAL_P (current) = ARG_FINAL_P (list);			\
    if (current != list)						\
      abort ();								\
  }

/* Reset the ARG_FINAL_P that might have been set in method M args.  */
#define UNMARK_FINAL_PARMS(M)						\
  {									\
    tree current;							\
    for (current = TYPE_ARG_TYPES (TREE_TYPE ((M))); 			\
	 current != end_params_node; current = TREE_CHAIN (current))	\
      ARG_FINAL_P (current) = 0;					\
  }

/* Reverse a crafted parameter list as required.  */
#define CRAFTED_PARAM_LIST_FIXUP(P)		\
  {						\
    if ((P))					\
      {						\
	tree last = (P);			\
	(P) = nreverse (P);			\
	TREE_CHAIN (last) = end_params_node;	\
      }						\
    else					\
      (P) = end_params_node;			\
  }

/* Modes governing the creation of a alias initializer parameter
   lists. AIPL stands for Alias Initializer Parameter List.  */
enum {
  AIPL_FUNCTION_CREATION,	  /* Suitable for artificial method creation */
  AIPL_FUNCTION_DECLARATION,	  /* Suitable for declared methods */
  AIPL_FUNCTION_CTOR_INVOCATION,  /* Invocation of constructors */
  AIPL_FUNCTION_FINIT_INVOCATION  /* Invocation of $finit$ */
};

/* Standard error messages */
#define ERROR_CANT_CONVERT_TO_BOOLEAN(OPERATOR, NODE, TYPE)		\
  parse_error_context ((OPERATOR),					\
    "Incompatible type for %qs. Can't convert %qs to boolean",	\
    operator_string ((NODE)), lang_printable_name ((TYPE),0))

#define ERROR_CANT_CONVERT_TO_NUMERIC(OPERATOR, NODE, TYPE)		\
  parse_error_context ((OPERATOR),					\
      "Incompatible type for %qs. Can't convert %qs to numeric type",	\
      operator_string ((NODE)), lang_printable_name ((TYPE), 0))

#define ERROR_CAST_NEEDED_TO_INTEGRAL(OPERATOR, NODE, TYPE)		\
do {									\
  tree _operator = (OPERATOR), _node = (NODE), _type = (TYPE);		\
  if (JPRIMITIVE_TYPE_P (_type))					\
    parse_error_context (_operator,					\
"Incompatible type for %qs. Explicit cast needed to convert %qs to integral",\
			 operator_string(_node),			\
			 lang_printable_name (_type, 0));		\
  else									\
    parse_error_context (_operator,					\
      "Incompatible type for %qs. Can't convert %qs to integral",	\
			 operator_string(_node),			\
			 lang_printable_name (_type, 0));		\
} while (0)

#define ERROR_VARIABLE_NOT_INITIALIZED(WFL, V)			\
  parse_error_context						\
    ((WFL), "Variable %qs may not have been initialized",	\
     IDENTIFIER_POINTER (V))

/* Definition for loop handling. This is Java's own definition of a
   loop body. See parse.y for documentation. It's valid once you hold
   a loop's body (LOOP_EXPR_BODY) */

/* The loop main block is the one hold the condition and the loop body */
#define LOOP_EXPR_BODY_MAIN_BLOCK(NODE) TREE_OPERAND (NODE, 0)
/* And then there is the loop update block */
#define LOOP_EXPR_BODY_UPDATE_BLOCK(NODE) TREE_OPERAND (NODE, 1)

/* Inside the loop main block, there is the loop condition and the
   loop body. They may be reversed if the loop being described is a
   do-while loop. NOTE: if you use a WFL around the EXIT_EXPR so you
   can issue debug info for it, the EXIT_EXPR will be one operand
   further. */
#define LOOP_EXPR_BODY_CONDITION_EXPR(NODE, R) 			\
  TREE_OPERAND (LOOP_EXPR_BODY_MAIN_BLOCK (NODE), (R ? 1 : 0))

/* Here is the labeled block the loop real body is encapsulated in */
#define LOOP_EXPR_BODY_LABELED_BODY(NODE, R)			\
  TREE_OPERAND (LOOP_EXPR_BODY_MAIN_BLOCK (NODE), (R ? 0 : 1))
/* And here is the loop's real body */
#define LOOP_EXPR_BODY_BODY_EXPR(NODE, R)			\
  LABELED_BLOCK_BODY (LOOP_EXPR_BODY_LABELED_BODY(NODE, R))

#define PUSH_LABELED_BLOCK(B)				\
  {							\
    TREE_CHAIN (B) = ctxp->current_labeled_block;	\
    ctxp->current_labeled_block = (B);			\
  }
#define POP_LABELED_BLOCK() 						\
  ctxp->current_labeled_block = TREE_CHAIN (ctxp->current_labeled_block)

#define PUSH_LOOP(L)				\
  {						\
    TREE_CHAIN (L) = ctxp->current_loop;	\
    ctxp->current_loop = (L);			\
  }
#define POP_LOOP() ctxp->current_loop = TREE_CHAIN (ctxp->current_loop)

#define PUSH_EXCEPTIONS(E)					\
  currently_caught_type_list =					\
    tree_cons (NULL_TREE, (E), currently_caught_type_list);

#define POP_EXCEPTIONS()						\
  currently_caught_type_list = TREE_CHAIN (currently_caught_type_list)

/* Check that we're inside a try block.  */
#define IN_TRY_BLOCK_P()				\
  (currently_caught_type_list 				\
   && ((TREE_VALUE (currently_caught_type_list) !=	\
	DECL_FUNCTION_THROWS (current_function_decl))	\
       || TREE_CHAIN (currently_caught_type_list)))

/* Check that we have exceptions in E.  */
#define EXCEPTIONS_P(E) ((E) ? TREE_VALUE (E) : NULL_TREE)

/* Anonymous array access */
#define ANONYMOUS_ARRAY_BASE_TYPE(N)   TREE_OPERAND ((N), 0)
#define ANONYMOUS_ARRAY_DIMS_SIG(N)    TREE_OPERAND ((N), 1)
#define ANONYMOUS_ARRAY_INITIALIZER(N) TREE_OPERAND ((N), 2)

/* Invocation modes, as returned by invocation_mode (). */
enum {
  INVOKE_STATIC,
  INVOKE_NONVIRTUAL,
  INVOKE_SUPER,
  INVOKE_INTERFACE,
  INVOKE_VIRTUAL
};

/* Unresolved type identifiers handling. When we process the source
   code, we blindly accept an unknown type identifier and try to
   resolve it later. When an unknown type identifier is encountered
   and used, we record in a struct jdep element what the incomplete
   type is and what it should patch. Later, java_complete_class will
   process all classes known to have unresolved type
   dependencies. Within each of these classes, this routine will
   process unresolved type dependencies (JDEP_TO_RESOLVE), patch what
   needs to be patched in the dependent tree node (JDEP_GET_PATCH,
   JDEP_APPLY_PATCH) and perform other actions dictated by the context
   of the patch (JDEP_KIND). The ideas are: we patch only what needs
   to be patched, and with java_complete_class called at the right
   time, we will start processing incomplete function bodies tree
   nodes with everything external to function's bodies already
   completed, it makes things much simpler. */

enum jdep_code {
  JDEP_NO_PATCH,		/* Must be first */
  JDEP_SUPER,			/* Patch the type of one type
				   supertype. Requires some check
				   before it's done */
  JDEP_FIELD,			/* Patch the type of a class field */

  /* JDEP_{METHOD,METHOD_RETURN,METHOD_END} to be kept in order */
  JDEP_METHOD,			/* Mark the beginning of the patching
				   of a method declaration, including
				   it's arguments */
  JDEP_METHOD_RETURN,		/* Mark the beginning of the patching
				   of a method declaration. Arguments
				   aren't patched, only the returned
				   type is */
  JDEP_METHOD_END,		/* Mark the end of the patching of a
				   method declaration. It indicates
				   that it's time to compute and
				   install a new signature */

  JDEP_INTERFACE,		/* Patch the type of a Class/interface
				   extension */
  JDEP_VARIABLE,		/* Patch the type of a variable declaration */
  JDEP_PARM,			/* Patch the type of a parm declaration */
  JDEP_TYPE,			/* Patch a random tree node type,
                                   without the need for any specific
                                   actions */
  JDEP_EXCEPTION,		/* Patch exceptions specified by `throws' */
  JDEP_ANONYMOUS		/* Patch anonymous classes
				   (implementation or extension.) */

};

typedef struct _jdep {
  ENUM_BITFIELD(jdep_code) kind : 8; /* Type of patch */

  unsigned int  flag0 : 1;	/* Some flags */
  tree decl;			/* Tied decl/or WFL */
  tree solv;			/* What to solve */
  tree wfl;			/* Where thing to resolve where found */
  tree misc;			/* Miscellaneous info (optional). */
  tree enclosing;		/* The enclosing (current) class */
  tree *patch;			/* Address of a location to patch */
  struct _jdep *next;		/* Linked list */
} jdep;


#define JDEP_DECL(J)          ((J)->decl)
#define JDEP_DECL_WFL(J)      ((J)->decl)
#define JDEP_KIND(J)          ((J)->kind)
#define JDEP_WFL(J)           ((J)->wfl)
#define JDEP_MISC(J)          ((J)->misc)
#define JDEP_ENCLOSING(J)     ((J)->enclosing)
#define JDEP_CLASS(J)         ((J)->class)
#define JDEP_APPLY_PATCH(J,P) (*(J)->patch = (P))
#define JDEP_GET_PATCH(J)     ((J)->patch)
#define JDEP_CHAIN(J)         ((J)->next)
#define JDEP_TO_RESOLVE(J)    ((J)->solv)
#define JDEP_RESOLVED_DECL(J) ((J)->solv)
#define JDEP_RESOLVED(J, D)   ((J)->solv = D)
#define JDEP_RESOLVED_P(J)    \
	(!(J)->solv || TREE_CODE ((J)->solv) != POINTER_TYPE)

struct jdeplist_s {
  jdep *first;
  jdep *last;
  struct jdeplist_s *next;
};
typedef struct jdeplist_s jdeplist;

#define CLASSD_FIRST(CD) ((CD)->first)
#define CLASSD_LAST(CD)  ((CD)->last)
#define CLASSD_CHAIN(CD) ((CD)->next)

#define JDEP_INSERT(L,J)			\
  {						\
    if (!(L)->first)				\
      (L)->last = (L)->first = (J);		\
    else					\
      {						\
	JDEP_CHAIN ((L)->last) = (J);		\
	(L)->last = (J);			\
      }						\
  }

/* if TYPE can't be resolved, obtain something suitable for its
   resolution (TYPE is saved in SAVE before being changed). and set
   CHAIN to 1. Otherwise, type is set to something usable. CHAIN is
   usually used to determine that a new DEP must be installed on TYPE.
   Note that when compiling java.lang.Object, references to Object are
   java.lang.Object.  */
#define SET_TYPE_FOR_RESOLUTION(TYPE, SAVE, CHAIN)			\
  {									\
    tree _returned_type;						\
    (CHAIN) = 0;							\
    if (TREE_TYPE (GET_CPC ()) == object_type_node			\
	&& TREE_CODE (TYPE) == EXPR_WITH_FILE_LOCATION			\
	&& EXPR_WFL_NODE (TYPE) == unqualified_object_id_node)		\
      (TYPE) = object_type_node;					\
    else								\
      {									\
	if (unresolved_type_p (type, &_returned_type))			\
	  {								\
	    if (_returned_type)						\
	      (TYPE) = _returned_type;					\
	    else							\
	      {								\
	        tree _type;						\
                WFL_STRIP_BRACKET (_type, TYPE);			\
		(SAVE) = (_type);					\
		(TYPE) = obtain_incomplete_type (TYPE);			\
		CHAIN = 1;						\
	      }								\
	  }								\
      }									\
  }

#define WFL_STRIP_BRACKET(TARGET, TYPE)					  \
{									  \
  tree __type = (TYPE);							  \
  if (TYPE && TREE_CODE (TYPE) == EXPR_WITH_FILE_LOCATION)		  \
    {									  \
      tree _node;							  \
      if (build_type_name_from_array_name (EXPR_WFL_NODE (TYPE), &_node)) \
        {								  \
          tree _new = copy_node (TYPE);					  \
          EXPR_WFL_NODE (_new) = _node;				  	  \
          __type = _new;						  \
        }								  \
    }									  \
  (TARGET) = __type;							  \
}

/* If NAME contains one or more trailing []s, NAMELEN will be the
   adjusted to be the index of the last non bracket character in
   NAME. ARRAY_DIMS will contain the number of []s found.  */

#define STRING_STRIP_BRACKETS(NAME, NAMELEN, ARRAY_DIMS)                  \
{									  \
  ARRAY_DIMS = 0;							  \
  while (NAMELEN >= 2 && (NAME)[NAMELEN - 1] == ']')			  \
    {									  \
      NAMELEN -= 2;							  \
      (ARRAY_DIMS)++;							  \
    }									  \
}

/* Promote a type if it won't be registered as a patch */
#define PROMOTE_RECORD_IF_COMPLETE(TYPE, IS_INCOMPLETE)		\
  {								\
    if (!(IS_INCOMPLETE) && TREE_CODE (TYPE) == RECORD_TYPE)	\
      (TYPE) = promote_type (TYPE);				\
  }

/* Insert a DECL in the current block */
#define BLOCK_CHAIN_DECL(NODE)						    \
  {		 							    \
    TREE_CHAIN ((NODE)) = 						    \
      BLOCK_EXPR_DECLS (GET_CURRENT_BLOCK (current_function_decl));         \
    BLOCK_EXPR_DECLS (GET_CURRENT_BLOCK (current_function_decl)) = (NODE);  \
  }

/* Return the current block, either found in the body of the currently
   declared function or in the current static block being defined. */
#define GET_CURRENT_BLOCK(F) ((F) ? DECL_FUNCTION_BODY ((F)) :	\
			     current_static_block)

#ifndef USE_MAPPED_LOCATION
/* Retrieve line/column from a WFL. */
#define EXPR_WFL_GET_LINECOL(V,LINE,COL)	\
  {						\
     (LINE) = (V) >> 12;			\
     (COL) = (V) & 0xfff;			\
   }
#endif

#define EXPR_WFL_QUALIFICATION(WFL) TREE_OPERAND ((WFL), 1)
#define QUAL_WFL(NODE) TREE_PURPOSE (NODE)
#define QUAL_RESOLUTION(NODE) TREE_VALUE (NODE)
#define QUAL_DECL_TYPE(NODE) GET_SKIP_TYPE (NODE)

#define GET_SKIP_TYPE(NODE)				\
  (TREE_CODE (TREE_TYPE (NODE)) == POINTER_TYPE ?	\
   TREE_TYPE (TREE_TYPE (NODE)): TREE_TYPE (NODE))

/* Handy macros for the walk operation */
#define COMPLETE_CHECK_OP(NODE, N)			\
{							\
  TREE_OPERAND ((NODE), (N)) = 				\
    java_complete_tree (TREE_OPERAND ((NODE), (N)));	\
  if (TREE_OPERAND ((NODE), (N)) == error_mark_node)	\
    return error_mark_node;				\
}
#define COMPLETE_CHECK_OP_0(NODE) COMPLETE_CHECK_OP(NODE, 0)
#define COMPLETE_CHECK_OP_1(NODE) COMPLETE_CHECK_OP(NODE, 1)
#define COMPLETE_CHECK_OP_2(NODE) COMPLETE_CHECK_OP(NODE, 2)

/* Building invocations: append(ARG) and StringBuffer(ARG) */
#define BUILD_APPEND(ARG)						      \
  ((JSTRING_TYPE_P (TREE_TYPE (ARG)) || JPRIMITIVE_TYPE_P (TREE_TYPE (ARG)))  \
   ? build_method_invocation (wfl_append,                                     \
			      ARG ? build_tree_list (NULL, (ARG)) : NULL_TREE)\
   : build_method_invocation (wfl_append,                                     \
			      ARG ? build_tree_list (NULL,                    \
						     build1 (CONVERT_EXPR,    \
							     object_type_node,\
							     (ARG)))          \
			      : NULL_TREE))
#define BUILD_STRING_BUFFER(ARG)					      \
  build_new_invocation (wfl_string_buffer, 				      \
			(ARG ? build_tree_list (NULL, (ARG)) : NULL_TREE))

#define BUILD_THROW(WHERE, WHAT)				\
  {								\
    (WHERE) = 							\
      build3 (CALL_EXPR, void_type_node,			\
	      build_address_of (throw_node),			\
	      build_tree_list (NULL_TREE, (WHAT)), NULL_TREE);	\
    TREE_SIDE_EFFECTS ((WHERE)) = 1;				\
  }

/* Set wfl_operator for the most accurate error location */
#ifdef USE_MAPPED_LOCATION
#define SET_WFL_OPERATOR(WHICH, NODE, WFL)		\
  SET_EXPR_LOCATION (WHICH,				\
    (TREE_CODE (WFL) == EXPR_WITH_FILE_LOCATION ?	\
     EXPR_LOCATION (WFL) : EXPR_LOCATION (NODE)))
#else
#define SET_WFL_OPERATOR(WHICH, NODE, WFL)		\
  EXPR_WFL_LINECOL (WHICH) =				\
    (TREE_CODE (WFL) == EXPR_WITH_FILE_LOCATION ?	\
     EXPR_WFL_LINECOL (WFL) : EXPR_WFL_LINECOL (NODE))
#endif

#define PATCH_METHOD_RETURN_ERROR()		\
  {						\
    if (ret_decl)				\
      *ret_decl = NULL_TREE;			\
    return error_mark_node;			\
  }

/* Convenient macro to check. Assumes that CLASS is a CLASS_DECL.  */
#define CHECK_METHODS(CLASS)			\
  {						\
    if (CLASS_INTERFACE ((CLASS)))		\
      java_check_abstract_methods ((CLASS));	\
    else					\
      java_check_regular_methods ((CLASS));	\
  }

#define CLEAR_DEPRECATED  ctxp->deprecated = 0

#define CHECK_DEPRECATED_NO_RESET(DECL)		\
  {						\
    if (ctxp->deprecated)			\
      DECL_DEPRECATED (DECL) = 1;		\
  }

/* Using and reseting the @deprecated tag flag */
#define CHECK_DEPRECATED(DECL)			\
  {						\
    if (ctxp->deprecated)			\
      DECL_DEPRECATED (DECL) = 1;		\
    ctxp->deprecated = 0;			\
  }

/* Register an import */
#define REGISTER_IMPORT(WHOLE, NAME)					\
{									\
  IS_A_SINGLE_IMPORT_CLASSFILE_NAME_P ((NAME)) = 1;			\
  ctxp->import_list = tree_cons ((WHOLE), (NAME), ctxp->import_list);	\
}

/* Macro to access the osb (opening square bracket) count */
#define CURRENT_OSB(C) (C)->osb_number [(C)->osb_depth]

/* Parser context data structure. */
struct parser_ctxt GTY(()) {
  const char *filename;		     /* Current filename */
  location_t file_start_location;
  location_t save_location;
  struct parser_ctxt *next;

  java_lexer * GTY((skip)) lexer; /* Current lexer state */
  char marker_begining;		     /* Marker. Should be a sub-struct */
  int ccb_indent;		     /* Number of unmatched { seen. */
  /* The next two fields are only source_location if USE_MAPPED_LOCATION.
     Otherwise, they are integer line number, but we can't have #ifdefs
     in GTY structures. */
  source_location first_ccb_indent1; /* First { at ident level 1 */
  source_location last_ccb_indent1;  /* Last } at ident level 1 */
  int parser_ccb_indent;	     /* Keep track of {} indent, parser */
  int osb_depth;		     /* Current depth of [ in an expression */
  int osb_limit;		     /* Limit of this depth */
  int * GTY ((skip)) osb_number; /* Keep track of ['s */
  char marker_end;		     /* End marker. Should be a sub-struct */

  /* The flags section */

  /* Indicates a context used for saving the parser status. The
     context must be popped when the status is restored. */
  unsigned saved_data_ctx:1;	
  /* Indicates that a context already contains saved data and that the
     next save operation will require a new context to be created. */
  unsigned saved_data:1;
  /* Report error when true */
  unsigned java_error_flag:1;
  /* @deprecated tag seen */
  unsigned deprecated:1;
  /* Flag to report certain errors (fix this documentation. FIXME) */
  unsigned class_err:1;

  /* This section is used only if we compile jc1 */
  tree modifier_ctx [12];	    /* WFL of modifiers */
  tree class_type;		    /* Current class */
  tree function_decl;	            /* Current function decl, save/restore */

  int prevent_ese;	            /* Prevent expression statement error */

  int formal_parameter_number;	    /* Number of parameters found */
  int interface_number;		    /* # itfs declared to extend an itf def */

  tree package;			    /* Defined package ID */

  /* These two lists won't survive file traversal */
  tree  class_list;		    /* List of classes in a CU */
  jdeplist * GTY((skip)) classd_list; /* Classe dependencies in a CU */
  
  tree  current_parsed_class;	    /* Class currently parsed */
  tree  current_parsed_class_un;    /* Curr. parsed class unqualified name */

  tree non_static_initialized;	    /* List of non static initialized fields */
  tree static_initialized;	    /* List of static non final initialized */
  tree instance_initializers;	    /* List of instance initializers stmts */

  tree import_list;		    /* List of import */
  tree import_demand_list;	    /* List of import on demand */

  tree current_loop;		    /* List of the currently nested 
				       loops/switches */
  tree current_labeled_block;	    /* List of currently nested
				       labeled blocks. */

  int pending_block;		    /* Pending block to close */

  int explicit_constructor_p;	    /* >0 when processing an explicit
				       constructor. This flag is used to trap
				       illegal argument usage during an
				       explicit constructor invocation. */
};

/* A set of macros to push/pop/access the currently parsed class.  */
#define GET_CPC_LIST()     ctxp->current_parsed_class

/* Currently class being parsed is an inner class if an enclosing
   class has been already pushed. This truth value is only valid prior
   an inner class is pushed. After, use FIXME. */
#define CPC_INNER_P() GET_CPC_LIST ()

/* The TYPE_DECL node of the class currently being parsed.  */
#define GET_CPC() TREE_VALUE (GET_CPC_LIST ())

/* Get the currently parsed class unqualified IDENTIFIER_NODE.  */
#define GET_CPC_UN() TREE_PURPOSE (GET_CPC_LIST ())

/* Get a parsed class unqualified IDENTIFIER_NODE from its CPC node.  */
#define GET_CPC_UN_NODE(N) TREE_PURPOSE (N)

/* Get the currently parsed class DECL_TYPE from its CPC node.  */
#define GET_CPC_DECL_NODE(N) TREE_VALUE (N)

/* The currently parsed enclosing currently parsed TREE_LIST node.  */
#define GET_ENCLOSING_CPC() TREE_CHAIN (GET_CPC_LIST ())

/* Get the next enclosing context.  */
#define GET_NEXT_ENCLOSING_CPC(C) TREE_CHAIN (C)

/* The DECL_TYPE node of the enclosing currently parsed
   class. NULL_TREE if the currently parsed class isn't an inner
   class.  */
#define GET_ENCLOSING_CPC_CONTEXT() (GET_ENCLOSING_CPC () ?		      \
                                     TREE_VALUE (GET_ENCLOSING_CPC ()) :      \
				     NULL_TREE)

/* Make sure that innerclass T sits in an appropriate enclosing
   context.  */
#define INNER_ENCLOSING_SCOPE_CHECK(T)					      \
  (INNER_CLASS_TYPE_P ((T)) && !ANONYMOUS_CLASS_P ((T))			      \
   && ((current_this							      \
	/* We have a this and it's not the right one */			      \
	&& (DECL_CONTEXT (TYPE_NAME ((T)))				      \
	    != TYPE_NAME (TREE_TYPE (TREE_TYPE (current_this))))	      \
	&& !inherits_from_p (TREE_TYPE (TREE_TYPE (current_this)),	      \
			     TREE_TYPE (DECL_CONTEXT (TYPE_NAME (T))))	      \
        && !common_enclosing_instance_p (TREE_TYPE (TREE_TYPE (current_this)),\
					(T))                                  \
	&& INNER_CLASS_TYPE_P (TREE_TYPE (TREE_TYPE (current_this)))          \
	&& !inherits_from_p                                                   \
	      (TREE_TYPE (DECL_CONTEXT                                        \
			  (TYPE_NAME (TREE_TYPE (TREE_TYPE (current_this))))),\
	       TREE_TYPE (DECL_CONTEXT (TYPE_NAME (T)))))                     \
       /* We don't have a this, which is OK if the current function is        \
	  static. */                                                          \
       || (!current_this						      \
	   && current_function_decl                                           \
           && ! METHOD_STATIC (current_function_decl))))

/* Push macro. First argument to PUSH_CPC is a DECL_TYPE, second
   argument is the unqualified currently parsed class name.  */
#define PUSH_CPC(C,R) { 					\
                        ctxp->current_parsed_class =		\
		        tree_cons ((R), (C), GET_CPC_LIST ()); 	\
		      }

/* In case of an error, push an error.  */
#define PUSH_ERROR() PUSH_CPC (error_mark_node, error_mark_node)

/* Pop macro. Before we pop, we link the current inner class decl (if any)
   to its enclosing class.  */
#define POP_CPC() {					\
		    link_nested_class_to_enclosing ();	\
		    ctxp->current_parsed_class =	\
		      TREE_CHAIN (GET_CPC_LIST ());	\
		  }

#define DEBUG_CPC()						\
  do								\
    {								\
      tree tmp =  ctxp->current_parsed_class;			\
      while (tmp)						\
	{							\
	  fprintf (stderr, "%s ",				\
		   IDENTIFIER_POINTER (TREE_PURPOSE (tmp)));	\
	  tmp = TREE_CHAIN (tmp);				\
	}							\
    } 								\
  while (0);

/* Access to the various initializer statement lists */
#define CPC_INITIALIZER_LIST(C)          ((C)->non_static_initialized)
#define CPC_STATIC_INITIALIZER_LIST(C)   ((C)->static_initialized)
#define CPC_INSTANCE_INITIALIZER_LIST(C) ((C)->instance_initializers)

/* Access to the various initializer statements */
#define CPC_INITIALIZER_STMT(C) (TREE_PURPOSE (CPC_INITIALIZER_LIST (C)))
#define CPC_STATIC_INITIALIZER_STMT(C) \
  (TREE_PURPOSE (CPC_STATIC_INITIALIZER_LIST (C)))
#define CPC_INSTANCE_INITIALIZER_STMT(C) \
  (TREE_PURPOSE (CPC_INSTANCE_INITIALIZER_LIST (C)))

/* Set various initializer statements */
#define SET_CPC_INITIALIZER_STMT(C,S)			\
  if (CPC_INITIALIZER_LIST (C))				\
    TREE_PURPOSE (CPC_INITIALIZER_LIST (C)) = (S);
#define SET_CPC_STATIC_INITIALIZER_STMT(C,S)			\
  if (CPC_STATIC_INITIALIZER_LIST (C))				\
    TREE_PURPOSE (CPC_STATIC_INITIALIZER_LIST (C)) = (S);
#define SET_CPC_INSTANCE_INITIALIZER_STMT(C,S)			\
  if (CPC_INSTANCE_INITIALIZER_LIST(C))				\
    TREE_PURPOSE (CPC_INSTANCE_INITIALIZER_LIST (C)) = (S);

/* This is used by the lexer to communicate with the parser.  It is
   set on an integer constant if the radix is NOT 10, so that the parser
   can correctly diagnose a numeric overflow.  */
#define JAVA_NOT_RADIX10_FLAG(NODE) TREE_LANG_FLAG_0(NODE)

#ifndef JC1_LITE
void java_complete_class (void);
void java_check_circular_reference (void);
void java_fix_constructors (void);
void java_layout_classes (void);
void java_reorder_fields (void);
tree java_method_add_stmt (tree, tree);
int java_report_errors (void);
extern tree do_resolve_class (tree, tree, tree, tree, tree);
#endif
char *java_get_line_col (const char *, int, int);
extern void reset_report (void);

/* Always in use, no matter what you compile */
void java_push_parser_context (void);
void java_pop_parser_context (int);
void java_init_lex (FILE *, const char *);
extern void java_parser_context_save_global (void);
extern void java_parser_context_restore_global (void);
int yyparse (void);
extern int java_parse (void);
extern void yyerror (const char *)
#ifdef JC1_LITE
ATTRIBUTE_NORETURN
#endif
;
extern void java_expand_classes (void);
extern void java_finish_classes (void);

extern GTY(()) struct parser_ctxt *ctxp;
extern GTY(()) struct parser_ctxt *ctxp_for_generation;
extern GTY(()) struct parser_ctxt *ctxp_for_generation_last;

#endif /* ! GCC_JAVA_PARSE_H */

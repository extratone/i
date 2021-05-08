/* Parser grammar for quick source code scan of Java(TM) language programs.
   Copyright (C) 1998, 1999, 2000, 2002, 2003, 2004, 2005
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

/* This file parses Java source code. Action can be further completed
to achieve a desired behavior. This file isn't part of the Java
language gcc front end.

The grammar conforms to the Java grammar described in "The Java(TM)
Language Specification. J. Gosling, B. Joy, G. Steele. Addison Wesley
1996, ISBN 0-201-63451-1"

Some rules have been modified to support JDK1.1 inner classes
definitions and other extensions.  */

%{
#define JC1_LITE

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "obstack.h"
#include "toplev.h"

extern FILE *finput, *out;
 
 const char *main_input_filename;

/* Obstack for the lexer.  */
struct obstack temporary_obstack;

/* The current parser context.  */
struct parser_ctxt *ctxp;

/* Error and warning counts, because they're used elsewhere  */
int java_error_count;
int java_warning_count;

/* Tweak default rules when necessary.  */
static int absorber;
#define USE_ABSORBER absorber = 0

/* Keep track of the current package name.  */
static const char *package_name;

/* Keep track of whether things have be listed before.  */
static int previous_output;

/* Record modifier uses  */
static int modifier_value;

/* Record (almost) cyclomatic complexity.  */
static int complexity; 

/* Keeps track of number of bracket pairs after a variable declarator
   id.  */
static int bracket_count; 

/* Numbers anonymous classes */
static int anonymous_count;

/* This is used to record the current class context.  */
struct class_context
{
  char *name;
  struct class_context *next;
};

/* The global class context.  */
static struct class_context *current_class_context;

/* A special constant used to represent an anonymous context.  */
static const char *anonymous_context = "ANONYMOUS";

/* Count of method depth.  */
static int method_depth; 

/* Record a method declaration  */
struct method_declarator {
  const char *method_name;
  const char *args;
};
#define NEW_METHOD_DECLARATOR(D,N,A)					     \
{									     \
  (D) = XNEW (struct method_declarator);				     \
  (D)->method_name = (N);						     \
  (D)->args = (A);							     \
}

/* Two actions for this grammar */
static int make_class_name_recursive (struct obstack *stack,
				      struct class_context *ctx);
static char *get_class_name (void);
static void report_class_declaration (const char *);
static void report_main_declaration (struct method_declarator *);
static void push_class_context (const char *);
static void pop_class_context (void);

void report (void); 

#include "lex.h"
#include "parse.h"
%}

%union {
  char *node;
  struct method_declarator *declarator;
  int value;			/* For modifiers */
}

%{
extern int flag_assert;

#include "lex.c"
%}

%pure_parser

/* Things defined here have to match the order of what's in the
   binop_lookup table.  */

%token   PLUS_TK         MINUS_TK        MULT_TK         DIV_TK    REM_TK
%token   LS_TK           SRS_TK          ZRS_TK
%token   AND_TK          XOR_TK          OR_TK
%token   BOOL_AND_TK BOOL_OR_TK 
%token   EQ_TK NEQ_TK GT_TK GTE_TK LT_TK LTE_TK

/* This maps to the same binop_lookup entry than the token above */

%token   PLUS_ASSIGN_TK  MINUS_ASSIGN_TK MULT_ASSIGN_TK DIV_ASSIGN_TK
%token   REM_ASSIGN_TK   
%token   LS_ASSIGN_TK    SRS_ASSIGN_TK   ZRS_ASSIGN_TK
%token   AND_ASSIGN_TK   XOR_ASSIGN_TK   OR_ASSIGN_TK


/* Modifier TOKEN have to be kept in this order. Don't scramble it */

%token   PUBLIC_TK       PRIVATE_TK         PROTECTED_TK
%token   STATIC_TK       FINAL_TK           SYNCHRONIZED_TK
%token   VOLATILE_TK     TRANSIENT_TK       NATIVE_TK
%token   PAD_TK          ABSTRACT_TK        MODIFIER_TK
%token   STRICT_TK

/* Keep those two in order, too */
%token   DECR_TK INCR_TK

/* From now one, things can be in any order */

%token   DEFAULT_TK      IF_TK              THROW_TK
%token   BOOLEAN_TK      DO_TK              IMPLEMENTS_TK
%token   THROWS_TK       BREAK_TK           IMPORT_TK       
%token   ELSE_TK         INSTANCEOF_TK      RETURN_TK
%token   VOID_TK         CATCH_TK           INTERFACE_TK
%token   CASE_TK         EXTENDS_TK         FINALLY_TK
%token   SUPER_TK        WHILE_TK           CLASS_TK
%token   SWITCH_TK       CONST_TK           TRY_TK
%token   FOR_TK          NEW_TK             CONTINUE_TK
%token   GOTO_TK         PACKAGE_TK         THIS_TK
%token   ASSERT_TK

%token   BYTE_TK         SHORT_TK           INT_TK            LONG_TK
%token   CHAR_TK         INTEGRAL_TK

%token   FLOAT_TK        DOUBLE_TK          FP_TK

%token   ID_TK

%token   REL_QM_TK         REL_CL_TK NOT_TK  NEG_TK

%token   ASSIGN_ANY_TK   ASSIGN_TK
%token   OP_TK  CP_TK  OCB_TK  CCB_TK  OSB_TK  CSB_TK  SC_TK  C_TK DOT_TK

%token   STRING_LIT_TK   CHAR_LIT_TK        INT_LIT_TK        FP_LIT_TK
%token   TRUE_TK         FALSE_TK           BOOL_LIT_TK       NULL_TK

%type <node> ID_TK identifier name simple_name qualified_name type
 	     primitive_type reference_type array_type formal_parameter_list
	     formal_parameter class_or_interface_type class_type interface_type
%type <declarator> method_declarator
%type <value>      MODIFIER_TK

%%
/* 19.2 Production from 2.3: The Syntactic Grammar  */
goal:
	compilation_unit
;

/* 19.3 Productions from 3: Lexical structure  */
literal:
	INT_LIT_TK
|	FP_LIT_TK
|	BOOL_LIT_TK
|	CHAR_LIT_TK
|       STRING_LIT_TK
|       NULL_TK
;

/* 19.4 Productions from 4: Types, Values and Variables  */
type:
	primitive_type
|	reference_type
;

primitive_type:
	INTEGRAL_TK
		{
		  /* use preset global here. FIXME */
		  $$ = xstrdup ("int");
		}
|	FP_TK
		{
		  /* use preset global here. FIXME */
		  $$ = xstrdup ("double");
		}
|	BOOLEAN_TK
		{
		  /* use preset global here. FIXME */
		  $$ = xstrdup ("boolean");
		}
;

reference_type:
	class_or_interface_type
|	array_type
;

class_or_interface_type:
	name
;

class_type:
	class_or_interface_type	/* Default rule */
;

interface_type:
	 class_or_interface_type
;

array_type:
	primitive_type dims
		{
	          while (bracket_count-- > 0) 
		    $$ = concat ("[", $1, NULL);
		}
|	name dims
		{
	          while (bracket_count-- > 0) 
		    $$ = concat ("[", $1, NULL);
		}
;

/* 19.5 Productions from 6: Names  */
name:
	simple_name		/* Default rule */
|	qualified_name		/* Default rule */
;

simple_name:
	identifier		/* Default rule */
;

qualified_name:
	name DOT_TK identifier
		{ 
		  $$ = concat ($1, ".", $3, NULL);
		}
;

identifier:
	ID_TK
;

/* 19.6: Production from 7: Packages  */
compilation_unit:
|	package_declaration
|	import_declarations
|	type_declarations
|       package_declaration import_declarations
|       package_declaration type_declarations
|       import_declarations type_declarations
|       package_declaration import_declarations type_declarations
;

import_declarations:
	import_declaration
|	import_declarations import_declaration
;

type_declarations:
	type_declaration
| 	type_declarations type_declaration
;

package_declaration:
	PACKAGE_TK name SC_TK
		{ package_name = $2; }
;

import_declaration:
	single_type_import_declaration
|	type_import_on_demand_declaration
;

single_type_import_declaration:
	IMPORT_TK name SC_TK
;

type_import_on_demand_declaration:
	IMPORT_TK name DOT_TK MULT_TK SC_TK
;

type_declaration:
	class_declaration
|	interface_declaration
|	empty_statement
;

/* 19.7 Shortened from the original:
   modifiers: modifier | modifiers modifier
   modifier: any of public...  */
modifiers:
	MODIFIER_TK
		{ 
		  if ($1 == PUBLIC_TK)
		    modifier_value++;
                  if ($1 == STATIC_TK)
                    modifier_value++;
	          USE_ABSORBER;
		}	
|	modifiers MODIFIER_TK
		{ 
		  if ($2 == PUBLIC_TK)
		    modifier_value++;
                  if ($2 == STATIC_TK)
                    modifier_value++;
		  USE_ABSORBER;
		}	
;

/* 19.8.1 Production from $8.1: Class Declaration */
class_declaration:
	modifiers CLASS_TK identifier super interfaces 
		{ 
		  report_class_declaration($3);
		  modifier_value = 0;
                }
	class_body
|	CLASS_TK identifier super interfaces 
		{ report_class_declaration($2); }
	class_body
;

super:
|	EXTENDS_TK class_type
;

interfaces:
|	IMPLEMENTS_TK interface_type_list
;

interface_type_list:
	interface_type
		{ USE_ABSORBER; }
|	interface_type_list C_TK interface_type
		{ USE_ABSORBER; }
;

class_body:
	OCB_TK CCB_TK
		{ pop_class_context (); }
|	OCB_TK class_body_declarations CCB_TK
		{ pop_class_context (); }
;

class_body_declarations:
	class_body_declaration
|	class_body_declarations class_body_declaration
;

class_body_declaration:
	class_member_declaration
|	static_initializer
|	constructor_declaration
|	block			/* Added, JDK1.1, instance initializer */
;

class_member_declaration:
	field_declaration
|	method_declaration
|	class_declaration	/* Added, JDK1.1 inner classes */
|	interface_declaration	/* Added, JDK1.1 inner classes */
|	empty_statement
;

/* 19.8.2 Productions from 8.3: Field Declarations  */
field_declaration:
	type variable_declarators SC_TK
		{ USE_ABSORBER; }
|	modifiers type variable_declarators SC_TK
		{ modifier_value = 0; }
;

variable_declarators:
	/* Should we use build_decl_list () instead ? FIXME */
	variable_declarator	/* Default rule */
|	variable_declarators C_TK variable_declarator
;

variable_declarator:
	variable_declarator_id
|	variable_declarator_id ASSIGN_TK variable_initializer
;

variable_declarator_id:
	identifier
		{ bracket_count = 0; USE_ABSORBER; }
|	variable_declarator_id OSB_TK CSB_TK
		{ ++bracket_count; }
;

variable_initializer:
	expression
|	array_initializer
;

/* 19.8.3 Productions from 8.4: Method Declarations  */
method_declaration:
	method_header
		{ ++method_depth; }
	method_body
		{ --method_depth; }
;

method_header:	
	type method_declarator throws
		{ USE_ABSORBER; }
|	VOID_TK method_declarator throws
|	modifiers type method_declarator throws
		{ modifier_value = 0; }
|	modifiers VOID_TK method_declarator throws
		{ 
                  report_main_declaration ($3);
		  modifier_value = 0;
		}
;

method_declarator:
	identifier OP_TK CP_TK
		{ 
		  struct method_declarator *d;
		  NEW_METHOD_DECLARATOR (d, $1, NULL);
		  $$ = d;
		}
|	identifier OP_TK formal_parameter_list CP_TK
		{ 
		  struct method_declarator *d;
		  NEW_METHOD_DECLARATOR (d, $1, $3);
		  $$ = d;
		}
|	method_declarator OSB_TK CSB_TK
;

formal_parameter_list:
	formal_parameter
|	formal_parameter_list C_TK formal_parameter
		{
		  $$ = concat ($1, ",", $3, NULL);
		}
;

formal_parameter:
	type variable_declarator_id
		{ 
		  USE_ABSORBER;
		  if (bracket_count)
		    {
		      int i;
		      char *n = XNEWVEC (char, bracket_count + 1 + strlen ($$));
		      for (i = 0; i < bracket_count; ++i)
			n[i] = '[';
		      strcpy (n + bracket_count, $$);
		      $$ = n;
		    }
		  else
		    $$ = $1;
		}
|	modifiers type variable_declarator_id /* Added, JDK1.1 final locals */
		{
		  if (bracket_count)
		    {
		      int i;
		      char *n = XNEWVEC (char, bracket_count + 1 + strlen ($2));
		      for (i = 0; i < bracket_count; ++i)
			n[i] = '[';
		      strcpy (n + bracket_count, $2);
		      $$ = n;
		    }
		  else
		    $$ = $2;
		}
;

throws:
|	THROWS_TK class_type_list
;

class_type_list:
	class_type
		{ USE_ABSORBER; }
|	class_type_list C_TK class_type
		{ USE_ABSORBER; }
;

method_body:
	block
|	SC_TK
;

/* 19.8.4 Productions from 8.5: Static Initializers  */
static_initializer:
	static block
;

static:				/* Test lval.sub_token here */
	MODIFIER_TK
		{ USE_ABSORBER; }
;

/* 19.8.5 Productions from 8.6: Constructor Declarations  */
/* NOTE FOR FURTHER WORK ON CONSTRUCTORS:
   - If a forbidden modifier is found, the error is either the use of
     a forbidden modifier for a constructor OR bogus attempt to declare a
     method without having specified the return type. FIXME */
constructor_declaration:
	constructor_declarator throws constructor_body
|	modifiers constructor_declarator throws constructor_body
		{ modifier_value = 0; }
/* extra SC_TK, FIXME */
|	constructor_declarator throws constructor_body SC_TK
/* extra SC_TK, FIXME */
|	modifiers constructor_declarator throws constructor_body SC_TK
		{ modifier_value = 0; }
/* I'm not happy with the SC_TK addition. It isn't in the grammar and should
   probably be matched by and empty statement. But it doesn't work. FIXME */
;

constructor_declarator:
	simple_name OP_TK CP_TK
		{ USE_ABSORBER; }
|	simple_name OP_TK formal_parameter_list CP_TK
		{ USE_ABSORBER; }
;

constructor_body:
	OCB_TK CCB_TK
|	OCB_TK explicit_constructor_invocation CCB_TK
|	OCB_TK block_statements CCB_TK
|       OCB_TK explicit_constructor_invocation block_statements CCB_TK
;

/* Error recovery for that rule moved down expression_statement: rule.  */
explicit_constructor_invocation:
	this_or_super OP_TK CP_TK SC_TK
|	this_or_super OP_TK argument_list CP_TK SC_TK
        /* Added, JDK1.1 inner classes. Modified because the rule
	   'primary' couldn't work.  */
|	name DOT_TK SUPER_TK OP_TK argument_list CP_TK SC_TK
		{ USE_ABSORBER; }
|	name DOT_TK SUPER_TK OP_TK CP_TK SC_TK
		{ USE_ABSORBER; }
;

this_or_super:			/* Added, simplifies error diagnostics */
	THIS_TK
|	SUPER_TK
;

/* 19.9 Productions from 9: Interfaces  */
/* 19.9.1 Productions from 9.1: Interfaces Declarations  */
interface_declaration:
	INTERFACE_TK identifier
		{ report_class_declaration ($2); modifier_value = 0; }
	interface_body
|	modifiers INTERFACE_TK identifier
		{ report_class_declaration ($3); modifier_value = 0; }
	interface_body
|	INTERFACE_TK identifier extends_interfaces
		{ report_class_declaration ($2); modifier_value = 0; }
	interface_body
|	modifiers INTERFACE_TK identifier extends_interfaces
		{ report_class_declaration ($3); modifier_value = 0; }
	interface_body
;

extends_interfaces:
	EXTENDS_TK interface_type
|	extends_interfaces C_TK interface_type
;

interface_body:
	OCB_TK CCB_TK
		{ pop_class_context (); }
|	OCB_TK interface_member_declarations CCB_TK
		{ pop_class_context (); }
;

interface_member_declarations:
	interface_member_declaration
|	interface_member_declarations interface_member_declaration
;

interface_member_declaration:
	constant_declaration
|	abstract_method_declaration
|	class_declaration	/* Added, JDK1.1 inner classes */
|	interface_declaration	/* Added, JDK1.1 inner classes */
|	empty_statement
;

constant_declaration:
	field_declaration
;

abstract_method_declaration:
	method_header SC_TK
;

/* 19.10 Productions from 10: Arrays  */
array_initializer:
	OCB_TK CCB_TK
|	OCB_TK variable_initializers CCB_TK
|	OCB_TK C_TK CCB_TK
|	OCB_TK variable_initializers C_TK CCB_TK
;

variable_initializers:
	variable_initializer
|	variable_initializers C_TK variable_initializer
;

/* 19.11 Production from 14: Blocks and Statements  */
block:
	OCB_TK CCB_TK
|	OCB_TK block_statements CCB_TK
;

block_statements:
	block_statement
|	block_statements block_statement
;

block_statement:
	local_variable_declaration_statement
|	statement
|	class_declaration	/* Added, JDK1.1 inner classes */
;

local_variable_declaration_statement:
	local_variable_declaration SC_TK /* Can't catch missing ';' here */
;

local_variable_declaration:
	type variable_declarators
		{ USE_ABSORBER; }
|	modifiers type variable_declarators /* Added, JDK1.1 final locals */
		{ modifier_value = 0; }
;

statement:
	statement_without_trailing_substatement
|	labeled_statement
|	if_then_statement
|	if_then_else_statement
|	while_statement
|	for_statement
;

statement_nsi:
	statement_without_trailing_substatement
|	labeled_statement_nsi
|	if_then_else_statement_nsi
|	while_statement_nsi
|	for_statement_nsi
;

statement_without_trailing_substatement:
	block
|	empty_statement
|	expression_statement
|	switch_statement
|	do_statement
|	break_statement
|	continue_statement
|	return_statement
|	synchronized_statement
|	throw_statement
|	try_statement
|	assert_statement
;

empty_statement:
	SC_TK
;

label_decl:
	identifier REL_CL_TK
		{ USE_ABSORBER; }
;

labeled_statement:
	label_decl statement
;

labeled_statement_nsi:
	label_decl statement_nsi
;

/* We concentrate here a bunch of error handling rules that we couldn't write
   earlier, because expression_statement catches a missing ';'.  */
expression_statement:
	statement_expression SC_TK
;

statement_expression: 
	assignment
|	pre_increment_expression
|	pre_decrement_expression
|	post_increment_expression
|	post_decrement_expression
|	method_invocation
|	class_instance_creation_expression
;

if_then_statement:
	IF_TK OP_TK expression CP_TK statement { ++complexity; }
;

if_then_else_statement:
	IF_TK OP_TK expression CP_TK statement_nsi ELSE_TK statement
	{ ++complexity; }
;

if_then_else_statement_nsi:
	IF_TK OP_TK expression CP_TK statement_nsi ELSE_TK statement_nsi
	{ ++complexity; }
;

switch_statement:
	SWITCH_TK OP_TK expression CP_TK switch_block
;

switch_block:
	OCB_TK CCB_TK
|	OCB_TK switch_labels CCB_TK
|	OCB_TK switch_block_statement_groups CCB_TK
|	OCB_TK switch_block_statement_groups switch_labels CCB_TK
;

switch_block_statement_groups: 
	switch_block_statement_group
|	switch_block_statement_groups switch_block_statement_group
;

switch_block_statement_group:
	switch_labels block_statements { ++complexity; }
;


switch_labels:
	switch_label
|	switch_labels switch_label
;

switch_label:
	CASE_TK constant_expression REL_CL_TK
|	DEFAULT_TK REL_CL_TK
;

while_expression:
	WHILE_TK OP_TK expression CP_TK { ++complexity; }
;

while_statement:
	while_expression statement
;

while_statement_nsi:
	while_expression statement_nsi
;

do_statement_begin:
	DO_TK
;

do_statement: 
	do_statement_begin statement WHILE_TK OP_TK expression CP_TK SC_TK
	{ ++complexity; }
;

for_statement:
	for_begin SC_TK expression SC_TK for_update CP_TK statement
|	for_begin SC_TK SC_TK for_update CP_TK statement
;

for_statement_nsi:
	for_begin SC_TK expression SC_TK for_update CP_TK statement_nsi
|	for_begin SC_TK SC_TK for_update CP_TK statement_nsi
;

for_header:
	FOR_TK OP_TK
;

for_begin:
	for_header for_init { ++complexity; }
;
for_init:			/* Can be empty */
|	statement_expression_list
|	local_variable_declaration
;

for_update:			/* Can be empty */
|	statement_expression_list
;

statement_expression_list:
	statement_expression
|	statement_expression_list C_TK statement_expression
;

break_statement:
	BREAK_TK SC_TK
|	BREAK_TK identifier SC_TK
;

/* `continue' with a label is considered for complexity but ordinary
   continue is not.  */
continue_statement:
	CONTINUE_TK SC_TK
	|       CONTINUE_TK identifier SC_TK { ++complexity; }
;

return_statement:
	RETURN_TK SC_TK
|	RETURN_TK expression SC_TK
;

throw_statement:
	THROW_TK expression SC_TK { ++complexity; }
;

assert_statement:
	ASSERT_TK expression REL_CL_TK expression SC_TK
|	ASSERT_TK expression SC_TK
|	ASSERT_TK error
		{yyerror ("Missing term"); RECOVER;}
|	ASSERT_TK expression error
		{yyerror ("';' expected"); RECOVER;}
;
synchronized_statement:
	synchronized OP_TK expression CP_TK block
|	synchronized OP_TK expression CP_TK error
;

synchronized:			/* Test lval.sub_token here */
	MODIFIER_TK
		{ USE_ABSORBER; }
;

try_statement:
	TRY_TK block catches
|	TRY_TK block finally
|	TRY_TK block catches finally
;

catches:
	catch_clause
|	catches catch_clause
;

catch_clause:
	CATCH_TK OP_TK formal_parameter CP_TK block { ++complexity; }
;

finally:
	FINALLY_TK block { ++complexity; }
;

/* 19.12 Production from 15: Expressions  */
primary:
	primary_no_new_array
|	array_creation_expression
;

primary_no_new_array:
	literal
|	THIS_TK
|	OP_TK expression CP_TK
|	class_instance_creation_expression
|	field_access
|	method_invocation
|	array_access
|	type_literals
        /* Added, JDK1.1 inner classes. Documentation is wrong
           referring to a 'ClassName' (class_name) rule that doesn't
           exist. Used name instead.  */
|	name DOT_TK THIS_TK
		{ USE_ABSORBER; }
;

type_literals:
	name DOT_TK CLASS_TK
		{ USE_ABSORBER; }
|	array_type DOT_TK CLASS_TK
		{ USE_ABSORBER; }
|	primitive_type DOT_TK CLASS_TK
		{ USE_ABSORBER; }
|	VOID_TK DOT_TK CLASS_TK
		{ USE_ABSORBER; }
;

class_instance_creation_expression:
	NEW_TK class_type OP_TK argument_list CP_TK
|	NEW_TK class_type OP_TK CP_TK
|	anonymous_class_creation
|	something_dot_new identifier OP_TK CP_TK
|	something_dot_new identifier OP_TK CP_TK class_body
|	something_dot_new identifier OP_TK argument_list CP_TK
|	something_dot_new identifier OP_TK argument_list CP_TK class_body
;

anonymous_class_creation:
	NEW_TK class_type OP_TK CP_TK
		{ report_class_declaration (anonymous_context); }
	class_body         
|	NEW_TK class_type OP_TK argument_list CP_TK
		{ report_class_declaration (anonymous_context); }
	class_body
;

something_dot_new:		/* Added, not part of the specs. */
	name DOT_TK NEW_TK
		{ USE_ABSORBER; }
|	primary DOT_TK NEW_TK
;

argument_list:
	expression
|	argument_list C_TK expression
|	argument_list C_TK error
;

array_creation_expression:
	NEW_TK primitive_type dim_exprs
|	NEW_TK class_or_interface_type dim_exprs
|	NEW_TK primitive_type dim_exprs dims
|	NEW_TK class_or_interface_type dim_exprs dims
        /* Added, JDK1.1 anonymous array. Initial documentation rule
           modified */
|	NEW_TK class_or_interface_type dims array_initializer
|	NEW_TK primitive_type dims array_initializer
;

dim_exprs:
	dim_expr
|	dim_exprs dim_expr
;

dim_expr:
	OSB_TK expression CSB_TK
;

dims:				
	OSB_TK CSB_TK
		{ bracket_count = 1; }
|	dims OSB_TK CSB_TK
		{ bracket_count++; }
;

field_access:
	primary DOT_TK identifier
|	SUPER_TK DOT_TK identifier
;

/* We include method invocation in the complexity measure on the
   theory that most method calls are virtual and therefore involve a
   decision point.  */
method_invocation:
	name OP_TK CP_TK
		{ USE_ABSORBER; ++complexity; }
|	name OP_TK argument_list CP_TK
		{ USE_ABSORBER; ++complexity; }
|	primary DOT_TK identifier OP_TK CP_TK { ++complexity; }
|	primary DOT_TK identifier OP_TK argument_list CP_TK { ++complexity; }
|	SUPER_TK DOT_TK identifier OP_TK CP_TK { ++complexity; }
|	SUPER_TK DOT_TK identifier OP_TK argument_list CP_TK { ++complexity; }
;

array_access:
	name OSB_TK expression CSB_TK
		{ USE_ABSORBER; }
|	primary_no_new_array OSB_TK expression CSB_TK
;

postfix_expression:
	primary
|	name
		{ USE_ABSORBER; }
|	post_increment_expression
|	post_decrement_expression
;

post_increment_expression:
	postfix_expression INCR_TK
;

post_decrement_expression:
	postfix_expression DECR_TK
;

unary_expression:
	pre_increment_expression
|	pre_decrement_expression
|	PLUS_TK unary_expression
|	MINUS_TK unary_expression
|	unary_expression_not_plus_minus
;

pre_increment_expression:
	INCR_TK unary_expression
;

pre_decrement_expression:
	DECR_TK unary_expression
;

unary_expression_not_plus_minus:
	postfix_expression
|	NOT_TK unary_expression
|	NEG_TK unary_expression
|	cast_expression
;

cast_expression:		/* Error handling here is potentially weak */
	OP_TK primitive_type dims CP_TK unary_expression
|	OP_TK primitive_type CP_TK unary_expression
|	OP_TK expression CP_TK unary_expression_not_plus_minus
|	OP_TK name dims CP_TK unary_expression_not_plus_minus
;

multiplicative_expression:
	unary_expression
|	multiplicative_expression MULT_TK unary_expression
|	multiplicative_expression DIV_TK unary_expression
|	multiplicative_expression REM_TK unary_expression
;

additive_expression:
	multiplicative_expression
|	additive_expression PLUS_TK multiplicative_expression
|	additive_expression MINUS_TK multiplicative_expression
;

shift_expression:
	additive_expression
|	shift_expression LS_TK additive_expression
|	shift_expression SRS_TK additive_expression
|	shift_expression ZRS_TK additive_expression
;

relational_expression:
	shift_expression
|	relational_expression LT_TK shift_expression
|	relational_expression GT_TK shift_expression
|	relational_expression LTE_TK shift_expression
|	relational_expression GTE_TK shift_expression
|	relational_expression INSTANCEOF_TK reference_type
;

equality_expression:
	relational_expression
|	equality_expression EQ_TK relational_expression
|	equality_expression NEQ_TK relational_expression
;

and_expression:
	equality_expression
|	and_expression AND_TK equality_expression
;

exclusive_or_expression:
	and_expression
|	exclusive_or_expression XOR_TK and_expression
;

inclusive_or_expression:
	exclusive_or_expression
|	inclusive_or_expression OR_TK exclusive_or_expression
;

conditional_and_expression:
	inclusive_or_expression
|	conditional_and_expression BOOL_AND_TK inclusive_or_expression
	{ ++complexity; }
;

conditional_or_expression:
	conditional_and_expression
|	conditional_or_expression BOOL_OR_TK conditional_and_expression
	{ ++complexity; }
;

conditional_expression:		/* Error handling here is weak */
	conditional_or_expression
|	conditional_or_expression REL_QM_TK expression REL_CL_TK conditional_expression
	{ ++complexity; }
;

assignment_expression:
	conditional_expression
|	assignment
;

assignment:
	left_hand_side assignment_operator assignment_expression
;

left_hand_side:
	name
		{ USE_ABSORBER; }
|	field_access
|	array_access
;

assignment_operator:
	ASSIGN_ANY_TK
|	ASSIGN_TK
;

expression:
	assignment_expression
;

constant_expression:
	expression
;

%%

/* Create a new parser context */

void
java_push_parser_context (void)
{
  struct parser_ctxt *tmp = XCNEW (struct parser_ctxt);

  tmp->next = ctxp;
  ctxp = tmp;
}  

static void
push_class_context (const char *name)
{
  struct class_context *ctx;

  ctx = XNEW (struct class_context);
  ctx->name = (char *) name;
  ctx->next = current_class_context;
  current_class_context = ctx;
}

static void
pop_class_context (void)
{
  struct class_context *ctx;

  if (current_class_context == NULL)
    return;

  ctx = current_class_context->next;
  if (current_class_context->name != anonymous_context)
    free (current_class_context->name);
  free (current_class_context);

  current_class_context = ctx;
  if (current_class_context == NULL)
    anonymous_count = 0;
}

/* Recursively construct the class name.  This is just a helper
   function for get_class_name().  */
static int
make_class_name_recursive (struct obstack *stack, struct class_context *ctx)
{
  if (! ctx)
    return 0;

  make_class_name_recursive (stack, ctx->next);

  /* Replace an anonymous context with the appropriate counter value.  */
  if (ctx->name == anonymous_context)
    {
      char buf[50];
      ++anonymous_count;
      sprintf (buf, "%d", anonymous_count);
      ctx->name = xstrdup (buf);
    }

  obstack_grow (stack, ctx->name, strlen (ctx->name));
  obstack_1grow (stack, '$');

  return ISDIGIT (ctx->name[0]);
}

/* Return a newly allocated string holding the name of the class.  */
static char *
get_class_name (void)
{
  char *result;
  int last_was_digit;
  struct obstack name_stack;

  obstack_init (&name_stack);

  /* Duplicate the logic of parse.y:maybe_make_nested_class_name().  */
  last_was_digit = make_class_name_recursive (&name_stack,
					      current_class_context->next);

  if (! last_was_digit
      && method_depth
      && current_class_context->name != anonymous_context)
    {
      char buf[50];
      ++anonymous_count;
      sprintf (buf, "%d", anonymous_count);
      obstack_grow (&name_stack, buf, strlen (buf));
      obstack_1grow (&name_stack, '$');
    }

  if (current_class_context->name == anonymous_context)
    {
      char buf[50];
      ++anonymous_count;
      sprintf (buf, "%d", anonymous_count);
      current_class_context->name = xstrdup (buf);
      obstack_grow0 (&name_stack, buf, strlen (buf));
    }
  else
    obstack_grow0 (&name_stack, current_class_context->name,
		   strlen (current_class_context->name));

  result = xstrdup (obstack_finish (&name_stack));
  obstack_free (&name_stack, NULL);

  return result;
}

/* Actions defined here */

static void
report_class_declaration (const char * name)
{
  extern int flag_dump_class, flag_list_filename;

  push_class_context (name);
  if (flag_dump_class)
    {
      char *name = get_class_name ();

      if (!previous_output)
	{
	  if (flag_list_filename)
	    fprintf (out, "%s: ", main_input_filename);
	  previous_output = 1;
	}

      if (package_name)
	fprintf (out, "%s.%s ", package_name, name);
      else
	fprintf (out, "%s ", name);

      free (name);
    }
}

static void
report_main_declaration (struct method_declarator *declarator)
{
  extern int flag_find_main;

  if (flag_find_main
      && modifier_value == 2
      && !strcmp (declarator->method_name, "main") 
      && declarator->args 
      && declarator->args [0] == '[' 
      && (! strcmp (declarator->args+1, "String")
	  || ! strcmp (declarator->args + 1, "java.lang.String"))
      && current_class_context)
    {
      if (!previous_output)
	{
	  char *name = get_class_name ();
	  if (package_name)
	    fprintf (out, "%s.%s ", package_name, name);
	  else
	    fprintf (out, "%s", name);
	  free (name);
	  previous_output = 1;
	}
    }
}

void
report (void)
{
  extern int flag_complexity;
  if (flag_complexity)
    fprintf (out, "%s %d\n", main_input_filename, complexity);
}

/* Reset global status used by the report functions.  */

void
reset_report (void)
{
  previous_output = 0;
  package_name = NULL;
  current_class_context = NULL;
  complexity = 0;
}

void
yyerror (const char *msg ATTRIBUTE_UNUSED)
{
  fprintf (stderr, "%s: %s\n", main_input_filename, msg);
  exit (1);
}

#ifdef __XGETTEXT__
/* Depending on the version of Bison used to compile this grammar,
   it may issue generic diagnostics spelled "syntax error" or
   "parse error".  To prevent this from changing the translation
   template randomly, we list all the variants of this particular
   diagnostic here.  Translators: there is no fine distinction
   between diagnostics with "syntax error" in them, and diagnostics
   with "parse error" in them.  It's okay to give them both the same
   translation.  */
const char d1[] = N_("syntax error");
const char d2[] = N_("parse error");
const char d3[] = N_("syntax error; also virtual memory exhausted");
const char d4[] = N_("parse error; also virtual memory exhausted");
const char d5[] = N_("syntax error: cannot back up");
const char d6[] = N_("parse error: cannot back up");
#endif

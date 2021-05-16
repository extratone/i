/* Support routines for manipulating internal types for GDB.
   Copyright 1992, 1993, 1994, 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2003,
   2004 Free Software Foundation, Inc.
   Contributed by Cygnus Support, using pieces from other GDB modules.

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
#include "gdb_string.h"
#include "bfd.h"
#include "symtab.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdbtypes.h"
#include "expression.h"
#include "language.h"
#include "target.h"
#include "value.h"
#include "demangle.h"
#include "complaints.h"
#include "gdbcmd.h"
#include "varobj.h"
#include "wrapper.h"
#include "cp-abi.h"
#include "gdb_assert.h"
#include "exceptions.h"

/* These variables point to the objects
   representing the predefined C data types.  */

struct type *builtin_type_void;
struct type *builtin_type_char;
struct type *builtin_type_true_char;
struct type *builtin_type_short;
struct type *builtin_type_int;
struct type *builtin_type_long;
struct type *builtin_type_long_long;
struct type *builtin_type_signed_char;
struct type *builtin_type_unsigned_char;
struct type *builtin_type_unsigned_short;
struct type *builtin_type_unsigned_int;
struct type *builtin_type_unsigned_long;
struct type *builtin_type_unsigned_long_long;
struct type *builtin_type_float;
struct type *builtin_type_double;
struct type *builtin_type_long_double;
struct type *builtin_type_complex;
struct type *builtin_type_double_complex;
struct type *builtin_type_string;
struct type *builtin_type_int0;
struct type *builtin_type_int8;
struct type *builtin_type_uint8;
struct type *builtin_type_int16;
struct type *builtin_type_uint16;
struct type *builtin_type_int32;
struct type *builtin_type_uint32;
struct type *builtin_type_int64;
struct type *builtin_type_uint64;
struct type *builtin_type_int128;
struct type *builtin_type_uint128;
struct type *builtin_type_bool;

/* 128 bit long vector types */
struct type *builtin_type_v2_double;
struct type *builtin_type_v4_float;
struct type *builtin_type_v2_int64;
struct type *builtin_type_v4_int32;
struct type *builtin_type_v8_int16;
struct type *builtin_type_v16_int8;
/* 64 bit long vector types */
struct type *builtin_type_v2_float;
struct type *builtin_type_v2_int32;
struct type *builtin_type_v4_int16;
struct type *builtin_type_v8_int8;

struct type *builtin_type_v4sf;
struct type *builtin_type_v4si;
struct type *builtin_type_v16qi;
struct type *builtin_type_v8qi;
struct type *builtin_type_v8hi;
struct type *builtin_type_v4hi;
struct type *builtin_type_v2si;
struct type *builtin_type_vec64;
struct type *builtin_type_vec128;
struct type *builtin_type_ieee_single[BFD_ENDIAN_UNKNOWN];
struct type *builtin_type_ieee_single_big;
struct type *builtin_type_ieee_single_little;
struct type *builtin_type_ieee_double[BFD_ENDIAN_UNKNOWN];
struct type *builtin_type_ieee_double_big;
struct type *builtin_type_ieee_double_little;
struct type *builtin_type_ieee_double_littlebyte_bigword;
struct type *builtin_type_i387_ext;
struct type *builtin_type_m68881_ext;
struct type *builtin_type_i960_ext;
struct type *builtin_type_m88110_ext;
struct type *builtin_type_m88110_harris_ext;
struct type *builtin_type_arm_ext[BFD_ENDIAN_UNKNOWN];
struct type *builtin_type_arm_ext_big;
struct type *builtin_type_arm_ext_littlebyte_bigword;
struct type *builtin_type_ia64_spill[BFD_ENDIAN_UNKNOWN];
struct type *builtin_type_ia64_spill_big;
struct type *builtin_type_ia64_spill_little;
struct type *builtin_type_ia64_quad[BFD_ENDIAN_UNKNOWN];
struct type *builtin_type_ia64_quad_big;
struct type *builtin_type_ia64_quad_little;
struct type *builtin_type_void_data_ptr;
struct type *builtin_type_void_func_ptr;
struct type *builtin_type_CORE_ADDR;
struct type *builtin_type_bfd_vma;
struct type *builtin_type_voidptrfuncptr;

int opaque_type_resolution = 1;

/* APPLE LOCAL: referenced by get_array_bounds */
int use_stride = 1;

static void
show_opaque_type_resolution (struct ui_file *file, int from_tty,
			     struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("\
Resolution of opaque struct/class/union types (if set before loading symbols) is %s.\n"),
		    value);
}

int overload_debug = 0;
static void
show_overload_debug (struct ui_file *file, int from_tty,
		     struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("Debugging of C++ overloading is %s.\n"), value);
}

struct extra
  {
    char str[128];
    int len;
  };				/* maximum extension is 128! FIXME */

static void print_bit_vector (B_TYPE *, int);
static void print_arg_types (struct field *, int, int);
static void dump_fn_fieldlists (struct type *, int);
static void print_cplus_stuff (struct type *, int);
static void virtual_base_list_aux (struct type *dclass);


/* Alloc a new type structure and fill it with some defaults.  If
   OBJFILE is non-NULL, then allocate the space for the type structure
   in that objfile's objfile_obstack.  Otherwise allocate the new type structure
   by xmalloc () (for permanent types).  */

struct type *
alloc_type (struct objfile *objfile)
{
  struct type *type;

  /* Alloc the structure and start off with all fields zeroed. */

  if (objfile == NULL)
    {
      type = xmalloc (sizeof (struct type));
      memset (type, 0, sizeof (struct type));
      TYPE_MAIN_TYPE (type) = xmalloc (sizeof (struct main_type));
    }
  else
    {
      type = obstack_alloc (&objfile->objfile_obstack,
			    sizeof (struct type));
      memset (type, 0, sizeof (struct type));
      TYPE_MAIN_TYPE (type) = obstack_alloc (&objfile->objfile_obstack,
					     sizeof (struct main_type));
      OBJSTAT (objfile, n_types++);
    }
  memset (TYPE_MAIN_TYPE (type), 0, sizeof (struct main_type));

  /* Initialize the fields that might not be zero. */

  TYPE_CODE (type) = TYPE_CODE_UNDEF;
  TYPE_BYTE_ORDER (type) = BFD_ENDIAN_UNKNOWN;
  TYPE_OBJFILE (type) = objfile;
  TYPE_VPTR_FIELDNO (type) = -1;
  TYPE_CHAIN (type) = type;	/* Chain back to itself.  */

  return (type);
}

/* Alloc a new type instance structure, fill it with some defaults,
   and point it at OLDTYPE.  Allocate the new type instance from the
   same place as OLDTYPE.  */

static struct type *
alloc_type_instance (struct type *oldtype)
{
  struct type *type;

  /* Allocate the structure.  */

  if (TYPE_OBJFILE (oldtype) == NULL)
    {
      type = xmalloc (sizeof (struct type));
      memset (type, 0, sizeof (struct type));
    }
  else
    {
      type = obstack_alloc (&TYPE_OBJFILE (oldtype)->objfile_obstack,
			    sizeof (struct type));
      memset (type, 0, sizeof (struct type));
    }
  TYPE_MAIN_TYPE (type) = TYPE_MAIN_TYPE (oldtype);

  TYPE_CHAIN (type) = type;	/* Chain back to itself for now.  */

  return (type);
}

/* Clear all remnants of the previous type at TYPE, in preparation for
   replacing it with something else.  */
static void
smash_type (struct type *type)
{
  memset (TYPE_MAIN_TYPE (type), 0, sizeof (struct main_type));

  /* APPLE LOCAL - Deleting the rings is wrong?  Just because you are
     changing the type it doesn't mean you don't want the other
     elements on the chain to lose their connection to whatever this
     is going to become...  */
#if 0
  /* For now, delete the rings.  */
  TYPE_CHAIN (type) = type;
#endif

  /* For now, leave the pointer/reference types alone.  */
}

/* Lookup a pointer to a type TYPE.  TYPEPTR, if nonzero, points
   to a pointer to memory where the pointer type should be stored.
   If *TYPEPTR is zero, update it to point to the pointer type we return.
   We allocate new memory if needed.  */

struct type *
make_pointer_type (struct type *type, struct type **typeptr)
{
  struct type *ntype;	/* New type */
  struct objfile *objfile;

  ntype = TYPE_POINTER_TYPE (type);

  if (ntype)
    {
      if (typeptr == 0)
	return ntype;		/* Don't care about alloc, and have new type.  */
      else if (*typeptr == 0)
	{
	  *typeptr = ntype;	/* Tracking alloc, and we have new type.  */
	  return ntype;
	}
    }

  if (typeptr == 0 || *typeptr == 0)	/* We'll need to allocate one.  */
    {
      ntype = alloc_type (TYPE_OBJFILE (type));
      if (typeptr)
	*typeptr = ntype;
    }
  else
    /* We have storage, but need to reset it.  */
    {
      ntype = *typeptr;
      objfile = TYPE_OBJFILE (ntype);
      smash_type (ntype);
      TYPE_OBJFILE (ntype) = objfile;
    }

  TYPE_TARGET_TYPE (ntype) = type;
  TYPE_POINTER_TYPE (type) = ntype;

  /* FIXME!  Assume the machine has only one representation for pointers!  */

  TYPE_LENGTH (ntype) = TARGET_PTR_BIT / TARGET_CHAR_BIT;
  TYPE_CODE (ntype) = TYPE_CODE_PTR;

  /* Mark pointers as unsigned.  The target converts between pointers
     and addresses (CORE_ADDRs) using POINTER_TO_ADDRESS() and
     ADDRESS_TO_POINTER(). */
  TYPE_FLAGS (ntype) |= TYPE_FLAG_UNSIGNED;

  if (!TYPE_POINTER_TYPE (type))	/* Remember it, if don't have one.  */
    TYPE_POINTER_TYPE (type) = ntype;

  return ntype;
}

/* Given a type TYPE, return a type of pointers to that type.
   May need to construct such a type if this is the first use.  */

struct type *
lookup_pointer_type (struct type *type)
{
  return make_pointer_type (type, (struct type **) 0);
}

/* Lookup a C++ `reference' to a type TYPE.  TYPEPTR, if nonzero, points
   to a pointer to memory where the reference type should be stored.
   If *TYPEPTR is zero, update it to point to the reference type we return.
   We allocate new memory if needed.  */

struct type *
make_reference_type (struct type *type, struct type **typeptr)
{
  struct type *ntype;	/* New type */
  struct objfile *objfile;

  ntype = TYPE_REFERENCE_TYPE (type);

  if (ntype)
    {
      if (typeptr == 0)
	return ntype;		/* Don't care about alloc, and have new type.  */
      else if (*typeptr == 0)
	{
	  *typeptr = ntype;	/* Tracking alloc, and we have new type.  */
	  return ntype;
	}
    }

  if (typeptr == 0 || *typeptr == 0)	/* We'll need to allocate one.  */
    {
      ntype = alloc_type (TYPE_OBJFILE (type));
      if (typeptr)
	*typeptr = ntype;
    }
  else
    /* We have storage, but need to reset it.  */
    {
      ntype = *typeptr;
      objfile = TYPE_OBJFILE (ntype);
      smash_type (ntype);
      TYPE_OBJFILE (ntype) = objfile;
    }

  TYPE_TARGET_TYPE (ntype) = type;
  TYPE_REFERENCE_TYPE (type) = ntype;

  /* FIXME!  Assume the machine has only one representation for references,
     and that it matches the (only) representation for pointers!  */

  TYPE_LENGTH (ntype) = TARGET_PTR_BIT / TARGET_CHAR_BIT;
  TYPE_CODE (ntype) = TYPE_CODE_REF;

  if (!TYPE_REFERENCE_TYPE (type))	/* Remember it, if don't have one.  */
    TYPE_REFERENCE_TYPE (type) = ntype;

  return ntype;
}

/* Same as above, but caller doesn't care about memory allocation details.  */

struct type *
lookup_reference_type (struct type *type)
{
  return make_reference_type (type, (struct type **) 0);
}

/* Lookup a function type that returns type TYPE.  TYPEPTR, if nonzero, points
   to a pointer to memory where the function type should be stored.
   If *TYPEPTR is zero, update it to point to the function type we return.
   We allocate new memory if needed.  */

struct type *
make_function_type (struct type *type, struct type **typeptr)
{
  struct type *ntype;	/* New type */
  struct objfile *objfile;

  if (typeptr == 0 || *typeptr == 0)	/* We'll need to allocate one.  */
    {
      ntype = alloc_type (TYPE_OBJFILE (type));
      if (typeptr)
	*typeptr = ntype;
    }
  else
    /* We have storage, but need to reset it.  */
    {
      ntype = *typeptr;
      objfile = TYPE_OBJFILE (ntype);
      smash_type (ntype);
      TYPE_OBJFILE (ntype) = objfile;
    }

  TYPE_TARGET_TYPE (ntype) = type;

  TYPE_LENGTH (ntype) = 1;
  TYPE_CODE (ntype) = TYPE_CODE_FUNC;

  return ntype;
}


/* Given a type TYPE, return a type of functions that return that type.
   May need to construct such a type if this is the first use.  */

struct type *
lookup_function_type (struct type *type)
{
  return make_function_type (type, (struct type **) 0);
}

/* Identify address space identifier by name --
   return the integer flag defined in gdbtypes.h.  */
extern int
address_space_name_to_int (char *space_identifier)
{
  struct gdbarch *gdbarch = current_gdbarch;
  int type_flags;
  /* Check for known address space delimiters. */
  if (!strcmp (space_identifier, "code"))
    return TYPE_FLAG_CODE_SPACE;
  else if (!strcmp (space_identifier, "data"))
    return TYPE_FLAG_DATA_SPACE;
  else if (gdbarch_address_class_name_to_type_flags_p (gdbarch)
           && gdbarch_address_class_name_to_type_flags (gdbarch,
							space_identifier,
							&type_flags))
    return type_flags;
  else
    error (_("Unknown address space specifier: \"%s\""), space_identifier);
}

/* Identify address space identifier by integer flag as defined in 
   gdbtypes.h -- return the string version of the adress space name. */

const char *
address_space_int_to_name (int space_flag)
{
  struct gdbarch *gdbarch = current_gdbarch;
  if (space_flag & TYPE_FLAG_CODE_SPACE)
    return "code";
  else if (space_flag & TYPE_FLAG_DATA_SPACE)
    return "data";
  else if ((space_flag & TYPE_FLAG_ADDRESS_CLASS_ALL)
           && gdbarch_address_class_type_flags_to_name_p (gdbarch))
    return gdbarch_address_class_type_flags_to_name (gdbarch, space_flag);
  else
    return NULL;
}

/* Create a new type with instance flags NEW_FLAGS, based on TYPE.

   If STORAGE is non-NULL, create the new type instance there.
   STORAGE must be in the same obstack as TYPE.  */

static struct type *
make_qualified_type (struct type *type, int new_flags,
		     struct type *storage)
{
  struct type *ntype;
  struct type *elem;

  ntype = type;
  do {
    if (TYPE_INSTANCE_FLAGS (ntype) == new_flags)
      return ntype;
    ntype = TYPE_CHAIN (ntype);
  } while (ntype != type);

  /* Create a new type instance.  */
  if (storage == NULL)
    ntype = alloc_type_instance (type);
  else
    {
      /* If STORAGE was provided, it had better be in the same objfile as
	 TYPE.  Otherwise, we can't link it into TYPE's cv chain: if one
	 objfile is freed and the other kept, we'd have dangling
	 pointers.  */
      gdb_assert (TYPE_OBJFILE (type) == TYPE_OBJFILE (storage));

      ntype = storage;
      /* APPLE LOCAL: This will get done down below, so it's not
	 necessary here.  */
#if 0
      TYPE_MAIN_TYPE (ntype) = TYPE_MAIN_TYPE (type);
#endif
      /* APPLE LOCAL: Don't do this, if NTYPE already
	 has been linked to another type in this chain, 
	 then you won't set the type in the chained type
	 correctly, AND you will blow away the chain
	 unnecessarily.  */
#if 0
      TYPE_CHAIN (ntype) = ntype;
#endif
    }

  /* APPLE LOCAL: I don't think this is right either.  If you 
     get here because STORAGE is NULL, then these are NULL already,
     alloc_type_instance sees to that.
     If you get passed in a type that already has it's pointer type
     filled in, why would you want to break that relationship?
     For instance, suppose you had:

     LSYM :t(0,5)=*(0,6)
     LSYM :t(0,7)=k(0,4)
     LSYM :t(0,6)=B(0,4)

     You would get in here with type the type for (0,4).  STORAGE
     would be the type struct you made for (0,6) which would
     have a pointer to (0,5) as its pointer type.  If you set the
     pointer type to NULL, (0,6) would be in the "TYPE_TARGET_TYPE
     of (0,5), but the TYPE_POINTER_TYPE of (0,6) would be unknown...  */
#if 0
  /* Pointers or references to the original type are not relevant to
     the new type.  */
  TYPE_POINTER_TYPE (ntype) = (struct type *) 0;
  TYPE_REFERENCE_TYPE (ntype) = (struct type *) 0;
#endif

  /* Chain the new qualified type to the old type.  */
  /* APPLE LOCAL: Be careful to splice any of the types that
     are already in ntype into the chain, and fix up their
     MAIN_TYPEs as well.  */

  /* First propagate the new type to all the elements on the old
     chain.  This leaves elem pointing at the element in NTYPE's chain
     right before ntype.  Note, I know that I am re-assigning the
     MAIN_TYPE of the first element again.  But not doing that makes
     the code really awkward.  */

  elem = ntype;
  while (1)
    {
      TYPE_INSTANCE_FLAGS (elem) |= new_flags;
      TYPE_LENGTH (elem) = TYPE_LENGTH (type);
      TYPE_MAIN_TYPE (elem) = TYPE_MAIN_TYPE (type);
      if (TYPE_CHAIN (elem) == ntype)
	break;
      else
	elem = TYPE_CHAIN (elem);
    }

  /* Now splice the chains together.  We are inserting NTYPE and its
     chain elements between TYPE and TYPE_CHAIN (type)...  */
  TYPE_CHAIN (elem) = TYPE_CHAIN (type);
  TYPE_CHAIN (type) = ntype;
  /* END APPLE LOCAL */

  return ntype;
}

/* Make an address-space-delimited variant of a type -- a type that
   is identical to the one supplied except that it has an address
   space attribute attached to it (such as "code" or "data").

   The space attributes "code" and "data" are for Harvard architectures.
   The address space attributes are for architectures which have
   alternately sized pointers or pointers with alternate representations.  */

struct type *
make_type_with_address_space (struct type *type, int space_flag)
{
  int new_flags = ((TYPE_INSTANCE_FLAGS (type)
		    & ~(TYPE_FLAG_CODE_SPACE | TYPE_FLAG_DATA_SPACE
		        | TYPE_FLAG_ADDRESS_CLASS_ALL))
		   | space_flag);

  return make_qualified_type (type, new_flags, NULL);
}

/* Make a "c-v" variant of a type -- a type that is identical to the
   one supplied except that it may have const or volatile attributes
   CNST is a flag for setting the const attribute
   VOLTL is a flag for setting the volatile attribute
   TYPE is the base type whose variant we are creating.

   If TYPEPTR and *TYPEPTR are non-zero, then *TYPEPTR points to
   storage to hold the new qualified type; *TYPEPTR and TYPE must be
   in the same objfile.  Otherwise, allocate fresh memory for the new
   type whereever TYPE lives.  If TYPEPTR is non-zero, set it to the
   new type we construct.  */
struct type *
make_cv_type (int cnst, int voltl, struct type *type, struct type **typeptr)
{
  struct type *ntype;	/* New type */

  int new_flags = (TYPE_INSTANCE_FLAGS (type)
		   & ~(TYPE_FLAG_CONST | TYPE_FLAG_VOLATILE));

  /* APPLE LOCAL */
  if (TYPE_CODE (type) == TYPE_CODE_ERROR)
    {
      if (typeptr != NULL)
      *typeptr = builtin_type_error;
      return builtin_type_error;
    }
  if (cnst)
    new_flags |= TYPE_FLAG_CONST;

  if (voltl)
    new_flags |= TYPE_FLAG_VOLATILE;

  if (typeptr && *typeptr != NULL)
    {

      /* APPLE LOCAL if we somehow ended up with a type code error
         over at *TYPEPTR, return a type error and in the hopes 
         that we can continue operation instead of asserting out
         and stopping the debugger.  */

      if (TYPE_CODE (*typeptr) == TYPE_CODE_ERROR)
        {
          return builtin_type_error;
        }

      /* TYPE and *TYPEPTR must be in the same objfile.  We can't have
	 a C-V variant chain that threads across objfiles: if one
	 objfile gets freed, then the other has a broken C-V chain.

	 This code used to try to copy over the main type from TYPE to
	 *TYPEPTR if they were in different objfiles, but that's
	 wrong, too: TYPE may have a field list or member function
	 lists, which refer to types of their own, etc. etc.  The
	 whole shebang would need to be copied over recursively; you
	 can't have inter-objfile pointers.  The only thing to do is
	 to leave stub types as stub types, and look them up afresh by
	 name each time you encounter them.  */

      /* APPLE LOCAL: Added a check for TYPE_IS_OPAQUE here, since we
	 can get called with an opaque type as well as a stub type here,
	 and the former may be in another objfile just like the latter.  */

      /* APPLE LOCAL: Instead of asserting out and exiting the debugger,
         return an error type.  */
#if 0
      gdb_assert (TYPE_OBJFILE (*typeptr) == TYPE_OBJFILE (type)
		  || (TYPE_STUB (*typeptr) || TYPE_IS_OPAQUE (*typeptr)));
#else
      if (TYPE_OBJFILE (*typeptr) != TYPE_OBJFILE (type) 
          && !TYPE_STUB (*typeptr) && !TYPE_IS_OPAQUE (*typeptr))
        return builtin_type_error;
#endif
    }
  
  ntype = make_qualified_type (type, new_flags, typeptr ? *typeptr : NULL);

  if (typeptr != NULL)
    *typeptr = ntype;

  return ntype;
}

/* Replace the contents of ntype with the type *type.  This changes the
   contents, rather than the pointer for TYPE_MAIN_TYPE (ntype); thus
   the changes are propogated to all types in the TYPE_CHAIN.

   In order to build recursive types, it's inevitable that we'll need
   to update types in place --- but this sort of indiscriminate
   smashing is ugly, and needs to be replaced with something more
   controlled.  TYPE_MAIN_TYPE is a step in this direction; it's not
   clear if more steps are needed.  */
void
replace_type (struct type *ntype, struct type *type)
{
  struct type *chain;

  /* These two types had better be in the same objfile.  Otherwise,
     the assignment of one type's main type structure to the other
     will produce a type with references to objects (names; field
     lists; etc.) allocated on an objfile other than its own.  */
  gdb_assert (TYPE_OBJFILE (ntype) == TYPE_OBJFILE (ntype));

  *TYPE_MAIN_TYPE (ntype) = *TYPE_MAIN_TYPE (type);

  /* The type length is not a part of the main type.  Update it for each
     type on the variant chain.  */
  chain = ntype;
  do {
    /* Assert that this element of the chain has no address-class bits
       set in its flags.  Such type variants might have type lengths
       which are supposed to be different from the non-address-class
       variants.  This assertion shouldn't ever be triggered because
       symbol readers which do construct address-class variants don't
       call replace_type().  */
    gdb_assert (TYPE_ADDRESS_CLASS_ALL (chain) == 0);

    TYPE_LENGTH (ntype) = TYPE_LENGTH (type);
    chain = TYPE_CHAIN (chain);
  } while (ntype != chain);

  /* Assert that the two types have equivalent instance qualifiers.
     This should be true for at least all of our debug readers.  */
  gdb_assert (TYPE_INSTANCE_FLAGS (ntype) == TYPE_INSTANCE_FLAGS (type));
}

/* Implement direct support for MEMBER_TYPE in GNU C++.
   May need to construct such a type if this is the first use.
   The TYPE is the type of the member.  The DOMAIN is the type
   of the aggregate that the member belongs to.  */

struct type *
lookup_member_type (struct type *type, struct type *domain)
{
  struct type *mtype;

  mtype = alloc_type (TYPE_OBJFILE (type));
  smash_to_member_type (mtype, domain, type);
  return (mtype);
}

/* Allocate a stub method whose return type is TYPE.  
   This apparently happens for speed of symbol reading, since parsing
   out the arguments to the method is cpu-intensive, the way we are doing
   it.  So, we will fill in arguments later.
   This always returns a fresh type.   */

struct type *
allocate_stub_method (struct type *type)
{
  struct type *mtype;

  mtype = init_type (TYPE_CODE_METHOD, 1, TYPE_FLAG_STUB, NULL,
		     TYPE_OBJFILE (type));
  TYPE_TARGET_TYPE (mtype) = type;
  /*  _DOMAIN_TYPE (mtype) = unknown yet */
  return (mtype);
}

/* Create a range type using either a blank type supplied in RESULT_TYPE,
   or creating a new type, inheriting the objfile from INDEX_TYPE.

   Indices will be of type INDEX_TYPE, and will range from LOW_BOUND to
   HIGH_BOUND, inclusive.

   FIXME:  Maybe we should check the TYPE_CODE of RESULT_TYPE to make
   sure it is TYPE_CODE_UNDEF before we bash it into a range type?

   APPLE LOCAL: Add a third field, to store the stride used for array
   types, defaulting to 1 ('normal').  We could make a new constructor
   specifically for range types used as array bounds, and save a bit
   of memory, but I'm not sure it's worth the extra complexity. */

struct type *
create_range_type (struct type *result_type, struct type *index_type,
		   int low_bound, int high_bound)
{
  if (result_type == NULL)
    {
      result_type = alloc_type (TYPE_OBJFILE (index_type));
    }
  TYPE_CODE (result_type) = TYPE_CODE_RANGE;
  TYPE_TARGET_TYPE (result_type) = index_type;
  if (TYPE_STUB (index_type))
    TYPE_FLAGS (result_type) |= TYPE_FLAG_TARGET_STUB;
  else
    TYPE_LENGTH (result_type) = TYPE_LENGTH (check_typedef (index_type));
  TYPE_NFIELDS (result_type) = 3;
  TYPE_FIELDS (result_type) = (struct field *)
    TYPE_ALLOC (result_type, 3 * sizeof (struct field));
  memset (TYPE_FIELDS (result_type), 0, 3 * sizeof (struct field));

  TYPE_FIELD_BITPOS (result_type, 0) = low_bound;
  TYPE_FIELD_BITPOS (result_type, 1) = high_bound;
  TYPE_FIELD_BITPOS (result_type, 2) = 1;

  TYPE_FIELD_TYPE (result_type, 0) = builtin_type_int; /* FIXME */
  TYPE_FIELD_TYPE (result_type, 1) = builtin_type_int; /* FIXME */
  TYPE_FIELD_TYPE (result_type, 2) = builtin_type_int; /* FIXME */

  if (low_bound >= 0)
    TYPE_FLAGS (result_type) |= TYPE_FLAG_UNSIGNED;

  return (result_type);
}

/* Set *LOWP and *HIGHP to the lower and upper bounds of discrete type TYPE.
   Return 1 if type is a range type, 0 if it is discrete (and bounds
   will fit in LONGEST), or -1 otherwise. */

int
get_discrete_bounds (struct type *type, LONGEST *lowp, LONGEST *highp)
{
  CHECK_TYPEDEF (type);
  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_RANGE:
      *lowp = TYPE_LOW_BOUND (type);
      *highp = TYPE_HIGH_BOUND (type);
      return 1;
    case TYPE_CODE_ENUM:
      if (TYPE_NFIELDS (type) > 0)
	{
	  /* The enums may not be sorted by value, so search all
	     entries */
	  int i;

	  *lowp = *highp = TYPE_FIELD_BITPOS (type, 0);
	  for (i = 0; i < TYPE_NFIELDS (type); i++)
	    {
	      if (TYPE_FIELD_BITPOS (type, i) < *lowp)
		*lowp = TYPE_FIELD_BITPOS (type, i);
	      if (TYPE_FIELD_BITPOS (type, i) > *highp)
		*highp = TYPE_FIELD_BITPOS (type, i);
	    }

	  /* Set unsigned indicator if warranted. */
	  if (*lowp >= 0)
	    {
	      TYPE_FLAGS (type) |= TYPE_FLAG_UNSIGNED;
	    }
	}
      else
	{
	  *lowp = 0;
	  *highp = -1;
	}
      return 0;
    case TYPE_CODE_BOOL:
      *lowp = 0;
      *highp = 1;
      return 0;
    case TYPE_CODE_INT:
      if (TYPE_LENGTH (type) > sizeof (LONGEST))	/* Too big */
	return -1;
      if (!TYPE_UNSIGNED (type))
	{
	  *lowp = -(1 << (TYPE_LENGTH (type) * TARGET_CHAR_BIT - 1));
	  *highp = -*lowp - 1;
	  return 0;
	}
      /* ... fall through for unsigned ints ... */
    case TYPE_CODE_CHAR:
      *lowp = 0;
      /* This round-about calculation is to avoid shifting by
         TYPE_LENGTH (type) * TARGET_CHAR_BIT, which will not work
         if TYPE_LENGTH (type) == sizeof (LONGEST). */
      *highp = 1 << (TYPE_LENGTH (type) * TARGET_CHAR_BIT - 1);
      *highp = (*highp - 1) | *highp;
      return 0;
    default:
      return -1;
    }
}

/* APPLE LOCAL: A special case of get_discrete_bounds.  Set *LOWP and
   *HIGHP to the lower and upper bounds of discrete type TYPE, and
   *STRIDE to the array stride.  Return 1 if type is a range type;
   otherwise generate an internal error (the type should already have
   been checked before this function was called).  If the range type
   has the stride set to 'default', choose a value based on the
   array type and the byte ordering of the platform. */

int
get_array_bounds (struct type *array, LONGEST *lowp, LONGEST *highp, LONGEST *stride)
{
  struct type *range = TYPE_INDEX_TYPE (array);

  CHECK_TYPEDEF (range);
  if (TYPE_CODE (range) != TYPE_CODE_RANGE)
    internal_error (__FILE__, __LINE__, "invalid type code");
  if (TYPE_STRIDE (range) == 0)
    internal_error (__FILE__, __LINE__, "range type did not set stride");

  *lowp = TYPE_LOW_BOUND (range);
  *highp = TYPE_HIGH_BOUND (range);

  if (use_stride)
    *stride = TYPE_STRIDE (range);
  else
    *stride = 1;

  return 1;
}

/* APPLE LOCAL: We might be asked to build an array type before we
   know the type of its elements.  If that's true, then the code in
   create_array_type will set the length to 0.  That's unfortunate...
   So we put the array on a completion list, and fix it up in this
   function, which gets called from end_symtab.  This is basically a
   copy of add_undefined_type and cleanup_undefined_types from
   stabsread.c.  */

static struct type **undef_arrays;
static int undef_arrays_allocated;
static int undef_arrays_length;

static void
add_undefined_array (struct type *type)
{
  if (undef_arrays_length == undef_arrays_allocated)
    {
      undef_arrays_allocated *= 2;
      undef_arrays = (struct type **)
	xrealloc ((char *) undef_arrays,
		  undef_arrays_allocated * sizeof (struct type *));
    }
  undef_arrays[undef_arrays_length++] = type;
}

/* This runs through the arrays types on the undef_arrays list and
   recalculates their length.  The manipulation of the type field
   comes from create_array_type.  */

void
cleanup_undefined_arrays (void)
{
  struct type **type;

  for (type = undef_arrays; type < undef_arrays + undef_arrays_length; type++)
    {
      struct type *range_type, *element_type;
      LONGEST low_bound, high_bound;
      if (TYPE_CODE (*type) != TYPE_CODE_ARRAY)
	{
	  warning ("Got a non-array type in cleanup_undefined_arrays");
	  continue;
	}
      element_type = check_typedef(TYPE_TARGET_TYPE (*type));
      range_type = TYPE_FIELD_TYPE (*type, 0);
      if (element_type == NULL)
	{
	  warning ("Got null element type for cleanup_undefined_arrays.");
	  continue;
	}
      else if (range_type == NULL)
	{
	  warning ("Got null range type for cleanup_undefined_arrays.");
	  continue;
	}
      if (get_discrete_bounds (range_type, &low_bound, &high_bound) < 0)
	low_bound = high_bound = 0;
      CHECK_TYPEDEF (element_type);
      TYPE_LENGTH (*type) =
	TYPE_LENGTH (element_type) * (high_bound - low_bound + 1);
      /* Also turn off the STUB flag.  */
      TYPE_FLAGS (*type) &= ~TYPE_FLAG_TARGET_STUB;
    }
  undef_arrays_length = 0;
}

/* END APPLE LOCAL */

/* Create an array type using either a blank type supplied in RESULT_TYPE,
   or creating a new type, inheriting the objfile from RANGE_TYPE.

   Elements will be of type ELEMENT_TYPE, the indices will be of type
   RANGE_TYPE.

   FIXME:  Maybe we should check the TYPE_CODE of RESULT_TYPE to make
   sure it is TYPE_CODE_UNDEF before we bash it into an array type? */

struct type *
create_array_type (struct type *result_type, struct type *element_type,
		   struct type *range_type)
{
  LONGEST low_bound, high_bound;

  if (result_type == NULL)
    {
      result_type = alloc_type (TYPE_OBJFILE (range_type));
    }
  TYPE_CODE (result_type) = TYPE_CODE_ARRAY;
  TYPE_TARGET_TYPE (result_type) = element_type;
  if (get_discrete_bounds (range_type, &low_bound, &high_bound) < 0)
    low_bound = high_bound = 0;
  CHECK_TYPEDEF (element_type);
  /* APPLE LOCAL: If the element type is undefined, then we are going
     to set the length to zero here.  Add this to the list of undefined
     arrays, then in end_symtab we'll come back and see if we know the
     element type then.  
     Another possibility is the element type is an array type that
     WAS undef'ed, but has now been defined.  Since we haven't done
     the cleanup_undefined_arrays yet, the length won't be set on 
     this yet.  So we need to add array's of this array to the 
     undefined arrays, and do it on cleanup.  */

  if (TYPE_CODE (element_type) == TYPE_CODE_UNDEF
      || (TYPE_CODE (element_type) == TYPE_CODE_ARRAY
	  && TYPE_LENGTH (element_type) == 0))
      add_undefined_array (result_type);

  TYPE_LENGTH (result_type) =
    TYPE_LENGTH (element_type) * (high_bound - low_bound + 1);
  TYPE_NFIELDS (result_type) = 1;
  TYPE_FIELDS (result_type) =
    (struct field *) TYPE_ALLOC (result_type, sizeof (struct field));
  memset (TYPE_FIELDS (result_type), 0, sizeof (struct field));
  TYPE_FIELD_TYPE (result_type, 0) = range_type;
  TYPE_VPTR_FIELDNO (result_type) = -1;

  /* TYPE_FLAG_TARGET_STUB will take care of zero length arrays */
  if (TYPE_LENGTH (result_type) == 0)
    TYPE_FLAGS (result_type) |= TYPE_FLAG_TARGET_STUB;

  return (result_type);
}

/* Create a string type using either a blank type supplied in RESULT_TYPE,
   or creating a new type.  String types are similar enough to array of
   char types that we can use create_array_type to build the basic type
   and then bash it into a string type.

   For fixed length strings, the range type contains 0 as the lower
   bound and the length of the string minus one as the upper bound.

   FIXME:  Maybe we should check the TYPE_CODE of RESULT_TYPE to make
   sure it is TYPE_CODE_UNDEF before we bash it into a string type? */

struct type *
create_string_type (struct type *result_type, struct type *range_type)
{
  struct type *string_char_type;
      
  string_char_type = language_string_char_type (current_language,
						current_gdbarch);
  result_type = create_array_type (result_type,
				   string_char_type,
				   range_type);
  TYPE_CODE (result_type) = TYPE_CODE_STRING;
  return (result_type);
}

struct type *
create_set_type (struct type *result_type, struct type *domain_type)
{
  LONGEST low_bound, high_bound, bit_length;
  if (result_type == NULL)
    {
      result_type = alloc_type (TYPE_OBJFILE (domain_type));
    }
  TYPE_CODE (result_type) = TYPE_CODE_SET;
  TYPE_NFIELDS (result_type) = 1;
  TYPE_FIELDS (result_type) = (struct field *)
    TYPE_ALLOC (result_type, 1 * sizeof (struct field));
  memset (TYPE_FIELDS (result_type), 0, sizeof (struct field));

  if (!TYPE_STUB (domain_type))
    {
      if (get_discrete_bounds (domain_type, &low_bound, &high_bound) < 0)
	low_bound = high_bound = 0;
      bit_length = high_bound - low_bound + 1;
      TYPE_LENGTH (result_type)
	= (bit_length + TARGET_CHAR_BIT - 1) / TARGET_CHAR_BIT;
    }
  TYPE_FIELD_TYPE (result_type, 0) = domain_type;

  if (low_bound >= 0)
    TYPE_FLAGS (result_type) |= TYPE_FLAG_UNSIGNED;

  return (result_type);
}

/* Construct and return a type of the form:
	struct NAME { ELT_TYPE ELT_NAME[N]; }
   We use these types for SIMD registers.  For example, the type of
   the SSE registers on the late x86-family processors is:
	struct __builtin_v4sf { float f[4]; }
   built by the function call:
	init_simd_type ("__builtin_v4sf", builtin_type_float, "f", 4)
   The type returned is a permanent type, allocated using malloc; it
   doesn't live in any objfile's obstack.  */
static struct type *
init_simd_type (char *name,
		struct type *elt_type,
		char *elt_name,
		int n)
{
  struct type *simd_type;
  struct type *array_type;
  
  simd_type = init_composite_type (name, TYPE_CODE_STRUCT);
  array_type = create_array_type (0, elt_type,
				  create_range_type (0, builtin_type_int,
						     0, n-1));
  append_composite_type_field (simd_type, elt_name, array_type);
  return simd_type;
}

static struct type *
init_vector_type (struct type *elt_type, int n)
{
  struct type *array_type;
 
  array_type = create_array_type (0, elt_type,
				  create_range_type (0, builtin_type_int,
						     0, n-1));
  TYPE_FLAGS (array_type) |= TYPE_FLAG_VECTOR;
  return array_type;
}

static struct type *
build_builtin_type_vec64 (void)
{
  /* Construct a type for the 64 bit registers.  The type we're
     building is this: */
#if 0
  union __gdb_builtin_type_vec64
  {
    int64_t uint64;
    float v2_float[2];
    int32_t v2_int32[2];
    int16_t v4_int16[4];
    int8_t v8_int8[8];
  };
#endif

  struct type *t;

  t = init_composite_type ("__gdb_builtin_type_vec64", TYPE_CODE_UNION);
  append_composite_type_field (t, "uint64", builtin_type_int64);
  append_composite_type_field (t, "v2_float", builtin_type_v2_float);
  append_composite_type_field (t, "v2_int32", builtin_type_v2_int32);
  append_composite_type_field (t, "v4_int16", builtin_type_v4_int16);
  append_composite_type_field (t, "v8_int8", builtin_type_v8_int8);

  TYPE_FLAGS (t) |= TYPE_FLAG_VECTOR;
  TYPE_NAME (t) = "builtin_type_vec64";
  return t;
}

static struct type *
build_builtin_type_vec128 (void)
{
  /* Construct a type for the 128 bit registers.  The type we're
     building is this: */
#if 0
 union __gdb_builtin_type_vec128 
  {
    int128_t uint128;
    float v4_float[4];
    int32_t v4_int32[4];
    int16_t v8_int16[8];
    int8_t v16_int8[16];
  };
#endif

  struct type *t;

  t = init_composite_type ("__gdb_builtin_type_vec128", TYPE_CODE_UNION);
  append_composite_type_field (t, "uint128", builtin_type_int128);
  append_composite_type_field (t, "v4_float", builtin_type_v4_float);
  append_composite_type_field (t, "v4_int32", builtin_type_v4_int32);
  append_composite_type_field (t, "v8_int16", builtin_type_v8_int16);
  append_composite_type_field (t, "v16_int8", builtin_type_v16_int8);

  TYPE_FLAGS (t) |= TYPE_FLAG_VECTOR;
  TYPE_NAME (t) = "builtin_type_vec128";
  return t;
}

/* Smash TYPE to be a type of members of DOMAIN with type TO_TYPE. 
   A MEMBER is a wierd thing -- it amounts to a typed offset into
   a struct, e.g. "an int at offset 8".  A MEMBER TYPE doesn't
   include the offset (that's the value of the MEMBER itself), but does
   include the structure type into which it points (for some reason).

   When "smashing" the type, we preserve the objfile that the
   old type pointed to, since we aren't changing where the type is actually
   allocated.  */

void
smash_to_member_type (struct type *type, struct type *domain,
		      struct type *to_type)
{
  struct objfile *objfile;

  objfile = TYPE_OBJFILE (type);

  smash_type (type);
  TYPE_OBJFILE (type) = objfile;
  TYPE_TARGET_TYPE (type) = to_type;
  TYPE_DOMAIN_TYPE (type) = domain;
  TYPE_LENGTH (type) = 1;	/* In practice, this is never needed.  */
  TYPE_CODE (type) = TYPE_CODE_MEMBER;
}

/* Smash TYPE to be a type of method of DOMAIN with type TO_TYPE.
   METHOD just means `function that gets an extra "this" argument'.

   When "smashing" the type, we preserve the objfile that the
   old type pointed to, since we aren't changing where the type is actually
   allocated.  */

void
smash_to_method_type (struct type *type, struct type *domain,
		      struct type *to_type, struct field *args,
		      int nargs, int varargs)
{
  struct objfile *objfile;

  objfile = TYPE_OBJFILE (type);

  smash_type (type);
  TYPE_OBJFILE (type) = objfile;
  TYPE_TARGET_TYPE (type) = to_type;
  TYPE_DOMAIN_TYPE (type) = domain;
  TYPE_FIELDS (type) = args;
  TYPE_NFIELDS (type) = nargs;
  if (varargs)
    TYPE_FLAGS (type) |= TYPE_FLAG_VARARGS;
  TYPE_LENGTH (type) = 1;	/* In practice, this is never needed.  */
  TYPE_CODE (type) = TYPE_CODE_METHOD;
}

/* Return a typename for a struct/union/enum type without "struct ",
   "union ", or "enum ".  If the type has a NULL name, return NULL.  */

char *
type_name_no_tag (const struct type *type)
{
  if (TYPE_TAG_NAME (type) != NULL)
    return TYPE_TAG_NAME (type);

  /* Is there code which expects this to return the name if there is no
     tag name?  My guess is that this is mainly used for C++ in cases where
     the two will always be the same.  */
  return TYPE_NAME (type);
}

/* Lookup a typedef or primitive type named NAME,
   visible in lexical block BLOCK.
   If NOERR is nonzero, return zero if NAME is not suitably defined.  */

struct type *
lookup_typename (char *name, struct block *block, int noerr)
{
  struct symbol *sym;
  struct type *tmp;

  sym = lookup_symbol (name, block, VAR_DOMAIN, 0, (struct symtab **) NULL);
  if (sym == NULL || SYMBOL_CLASS (sym) != LOC_TYPEDEF)
    {
      tmp = language_lookup_primitive_type_by_name (current_language,
						    current_gdbarch,
						    name);
      if (tmp)
	{
	  return (tmp);
	}
      else if (!tmp && noerr)
	{
	  return (NULL);
	}
      else
	{
	  error (_("No type named %s."), name);
	}
    }
  return (SYMBOL_TYPE (sym));
}

struct type *
lookup_unsigned_typename (char *name)
{
  char *uns = alloca (strlen (name) + 10);

  strcpy (uns, "unsigned ");
  strcpy (uns + 9, name);
  return (lookup_typename (uns, (struct block *) NULL, 0));
}

struct type *
lookup_signed_typename (char *name)
{
  struct type *t;
  char *uns = alloca (strlen (name) + 8);

  strcpy (uns, "signed ");
  strcpy (uns + 7, name);
  t = lookup_typename (uns, (struct block *) NULL, 1);
  /* If we don't find "signed FOO" just try again with plain "FOO". */
  if (t != NULL)
    return t;
  return lookup_typename (name, (struct block *) NULL, 0);
}

/* Lookup a structure type named "struct NAME",
   visible in lexical block BLOCK.  */

struct type *
lookup_struct (char *name, struct block *block)
{
  struct symbol *sym;

  sym = lookup_symbol (name, block, STRUCT_DOMAIN, 0,
		       (struct symtab **) NULL);

  if (sym == NULL)
    {
      error (_("No struct type named %s."), name);
    }
  if (TYPE_CODE (SYMBOL_TYPE (sym)) != TYPE_CODE_STRUCT)
    {
      error (_("This context has class, union or enum %s, not a struct."), name);
    }
  return (SYMBOL_TYPE (sym));
}

/* Lookup a structure type named "struct NAME",
   visible in lexical block BLOCK, but return NULL instead of
   causing an error if it isn't found. */

struct type *
lookup_struct_no_error (name, block)
     char *name;
     struct block *block;
{
  register struct symbol *sym;

  sym = lookup_symbol (name, block, STRUCT_DOMAIN, 0,
		       (struct symtab **) NULL);

  if (sym == NULL)
    {
      return (NULL);
    }
  if (TYPE_CODE (SYMBOL_TYPE (sym)) != TYPE_CODE_STRUCT)
    {
      return (NULL);
    }
  return (SYMBOL_TYPE (sym));
}

/* Lookup a union type named "union NAME",
   visible in lexical block BLOCK.  */

struct type *
lookup_union (char *name, struct block *block)
{
  struct symbol *sym;
  struct type *t;

  sym = lookup_symbol (name, block, STRUCT_DOMAIN, 0,
		       (struct symtab **) NULL);

  if (sym == NULL)
    error (_("No union type named %s."), name);

  t = SYMBOL_TYPE (sym);

  if (TYPE_CODE (t) == TYPE_CODE_UNION)
    return (t);

  /* C++ unions may come out with TYPE_CODE_CLASS, but we look at
   * a further "declared_type" field to discover it is really a union.
   */
  if (HAVE_CPLUS_STRUCT (t))
    if (TYPE_DECLARED_TYPE (t) == DECLARED_TYPE_UNION)
      return (t);

  /* If we get here, it's not a union */
  error (_("This context has class, struct or enum %s, not a union."), name);
}


/* Lookup an enum type named "enum NAME",
   visible in lexical block BLOCK.  */

struct type *
lookup_enum (char *name, struct block *block)
{
  struct symbol *sym;

  sym = lookup_symbol (name, block, STRUCT_DOMAIN, 0,
		       (struct symtab **) NULL);
  if (sym == NULL)
    {
      error (_("No enum type named %s."), name);
    }
  if (TYPE_CODE (SYMBOL_TYPE (sym)) != TYPE_CODE_ENUM)
    {
      error (_("This context has class, struct or union %s, not an enum."), name);
    }
  return (SYMBOL_TYPE (sym));
}

/* Lookup a template type named "template NAME<TYPE>",
   visible in lexical block BLOCK.  */

struct type *
lookup_template_type (char *name, struct type *type, struct block *block)
{
  struct symbol *sym;
  char *nam = (char *) alloca (strlen (name) + strlen (TYPE_NAME (type)) + 4);
  strcpy (nam, name);
  strcat (nam, "<");
  strcat (nam, TYPE_NAME (type));
  strcat (nam, " >");		/* FIXME, extra space still introduced in gcc? */

  sym = lookup_symbol (nam, block, VAR_DOMAIN, 0, (struct symtab **) NULL);

  if (sym == NULL)
    {
      error (_("No template type named %s."), name);
    }
  if (TYPE_CODE (SYMBOL_TYPE (sym)) != TYPE_CODE_STRUCT)
    {
      error (_("This context has class, union or enum %s, not a struct."), name);
    }
  return (SYMBOL_TYPE (sym));
}

/* Given a type TYPE, lookup the type of the component of type named NAME.  

   TYPE can be either a struct or union, or a pointer or reference to a struct or
   union.  If it is a pointer or reference, its target type is automatically used.
   Thus '.' and '->' are interchangable, as specified for the definitions of the
   expression element types STRUCTOP_STRUCT and STRUCTOP_PTR.

   If NOERR is nonzero, return zero if NAME is not suitably defined.
   If NAME is the name of a baseclass type, return that type.  */

struct type *
lookup_struct_elt_type (struct type *type, char *name, int noerr)
{
  int i;
  char *type_for_printing;

  for (;;)
    {
      CHECK_TYPEDEF (type);
      if (TYPE_CODE (type) != TYPE_CODE_PTR
	  && TYPE_CODE (type) != TYPE_CODE_REF)
	break;
      type = TYPE_TARGET_TYPE (type);
    }

  if (TYPE_CODE (type) != TYPE_CODE_STRUCT &&
      TYPE_CODE (type) != TYPE_CODE_UNION)
    {
      target_terminal_ours ();
      gdb_flush (gdb_stdout);
      type_for_printing = type_sprint (type, "", -1);
      make_cleanup (xfree, type_for_printing);
      error ("Type %s is not a structure or union type.", type_for_printing);
    }

#if 0
  /* FIXME:  This change put in by Michael seems incorrect for the case where
     the structure tag name is the same as the member name.  I.E. when doing
     "ptype bell->bar" for "struct foo { int bar; int foo; } bell;"
     Disabled by fnf. */
  {
    char *typename;

    typename = type_name_no_tag (type);
    if (typename != NULL && strcmp (typename, name) == 0)
      return type;
  }
#endif

  for (i = TYPE_NFIELDS (type) - 1; i >= TYPE_N_BASECLASSES (type); i--)
    {
      char *t_field_name = TYPE_FIELD_NAME (type, i);

      if (t_field_name && (strcmp_iw (t_field_name, name) == 0))
	{
	  return TYPE_FIELD_TYPE (type, i);
	}
    }

  /* OK, it's not in this class.  Recursively check the baseclasses.  */
  for (i = TYPE_N_BASECLASSES (type) - 1; i >= 0; i--)
    {
      struct type *t;

      t = lookup_struct_elt_type (TYPE_BASECLASS (type, i), name, noerr);
      if (t != NULL)
	{
	  return t;
	}
    }

  if (noerr)
    {
      return NULL;
    }

  target_terminal_ours ();
  gdb_flush (gdb_stdout);

  type_for_printing = type_sprint (type, "", -1); 
  make_cleanup (xfree, type_for_printing);
  error ("Type %s has no component named %s.", type_for_printing, name);

  return (struct type *) -1;	/* For lint */
}

/* If possible, make the vptr_fieldno and vptr_basetype fields of TYPE
   valid.  Callers should be aware that in some cases (for example,
   the type or one of its baseclasses is a stub type and we are
   debugging a .o file), this function will not be able to find the virtual
   function table pointer, and vptr_fieldno will remain -1 and vptr_basetype
   will remain NULL.  */

void
fill_in_vptr_fieldno (struct type *type)
{
  CHECK_TYPEDEF (type);

  if (TYPE_VPTR_FIELDNO (type) < 0)
    {
      int i;

      /* We must start at zero in case the first (and only) baseclass is
         virtual (and hence we cannot share the table pointer).  */
      for (i = 0; i < TYPE_N_BASECLASSES (type); i++)
	{
	  struct type *baseclass = check_typedef (TYPE_BASECLASS (type, i));
	  fill_in_vptr_fieldno (baseclass);
	  if (TYPE_VPTR_FIELDNO (baseclass) >= 0)
	    {
	      TYPE_VPTR_FIELDNO (type) = TYPE_VPTR_FIELDNO (baseclass);
	      TYPE_VPTR_BASETYPE (type) = TYPE_VPTR_BASETYPE (baseclass);
	      break;
	    }
	}
    }
}

/* Find the method and field indices for the destructor in class type T.
   Return 1 if the destructor was found, otherwise, return 0.  */

int
get_destructor_fn_field (struct type *t, int *method_indexp, int *field_indexp)
{
  int i;

  for (i = 0; i < TYPE_NFN_FIELDS (t); i++)
    {
      int j;
      struct fn_field *f = TYPE_FN_FIELDLIST1 (t, i);

      for (j = 0; j < TYPE_FN_FIELDLIST_LENGTH (t, i); j++)
	{
	  if (is_destructor_name (TYPE_FN_FIELD_PHYSNAME (f, j)) != 0)
	    {
	      *method_indexp = i;
	      *field_indexp = j;
	      return 1;
	    }
	}
    }
  return 0;
}

static void
stub_noname_complaint (void)
{
  complaint (&symfile_complaints, _("stub type has NULL name"));
}

/* Added by Bryan Boreham, Kewill, Sun Sep 17 18:07:17 1989.

   If this is a stubbed struct (i.e. declared as struct foo *), see if
   we can find a full definition in some other file. If so, copy this
   definition, so we can use it in future.  There used to be a comment (but
   not any code) that if we don't find a full definition, we'd set a flag
   so we don't spend time in the future checking the same type.  That would
   be a mistake, though--we might load in more symbols which contain a
   full definition for the type.

   This used to be coded as a macro, but I don't think it is called 
   often enough to merit such treatment.  */

/* Find the real type of TYPE.  This function returns the real type, after
   removing all layers of typedefs and completing opaque or stub types.
   Completion changes the TYPE argument, but stripping of typedefs does
   not.  */

struct type *
check_typedef (struct type *type)
{
  struct type *orig_type = type;
  int is_const, is_volatile;

  while (TYPE_CODE (type) == TYPE_CODE_TYPEDEF)
    {
      if (TYPE_TARGET_TYPE (type) == type)
	{
	  /* FIXME: I have found places where the TYPE_TARGET_TYPE of a
	     typedef is the same as the type, it comes up when you do:

	     typedef struct foo foo

	     but not always.  

	     We need to figure out why this is happening, but for now,
	     this makes the error benign... */
	  break;
	}

      if (!TYPE_TARGET_TYPE (type))
	{
	  char *name;
	  struct symbol *sym;

	  /* It is dangerous to call lookup_symbol if we are currently
	     reading a symtab.  Infinite recursion is one danger. */
	  if (currently_reading_symtab)
	    return type;

	  name = type_name_no_tag (type);
	  /* FIXME: shouldn't we separately check the TYPE_NAME and the
	     TYPE_TAG_NAME, and look in STRUCT_DOMAIN and/or VAR_DOMAIN
	     as appropriate?  (this code was written before TYPE_NAME and
	     TYPE_TAG_NAME were separate).  */
	  if (name == NULL)
	    {
	      stub_noname_complaint ();
	      return type;
	    }
	  sym = lookup_symbol (name, 0, STRUCT_DOMAIN, 0,
			       (struct symtab **) NULL);
	  if (sym)
	    TYPE_TARGET_TYPE (type) = SYMBOL_TYPE (sym);
	  else
	    TYPE_TARGET_TYPE (type) = alloc_type (NULL);	/* TYPE_CODE_UNDEF */
	}
      type = TYPE_TARGET_TYPE (type);
    }

  is_const = TYPE_CONST (type);
  is_volatile = TYPE_VOLATILE (type);

  /* If this is a struct/class/union with no fields, then check whether a
     full definition exists somewhere else.  This is for systems where a
     type definition with no fields is issued for such types, instead of
     identifying them as stub types in the first place */

  if (TYPE_IS_OPAQUE (type) && opaque_type_resolution && !currently_reading_symtab)
    {
      char *name = type_name_no_tag (type);
      struct type *newtype;
      if (name == NULL)
	{
	  stub_noname_complaint ();
	  return type;
	}
      newtype = lookup_transparent_type (name);

      if (newtype)
	{
	  /* If the resolved type and the stub are in the same objfile,
	     then replace the stub type with the real deal.  But if
	     they're in separate objfiles, leave the stub alone; we'll
	     just look up the transparent type every time we call
	     check_typedef.  We can't create pointers between types
	     allocated to different objfiles, since they may have
	     different lifetimes.  Trying to copy NEWTYPE over to TYPE's
	     objfile is pointless, too, since you'll have to move over any
	     other types NEWTYPE refers to, which could be an unbounded
	     amount of stuff.  */
	  if (TYPE_OBJFILE (newtype) == TYPE_OBJFILE (type))
	    make_cv_type (is_const, is_volatile, newtype, &type);
	  else
	    type = newtype;
	}
    }
  /* Otherwise, rely on the stub flag being set for opaque/stubbed types */
  else if (TYPE_STUB (type) && !currently_reading_symtab)
    {
      char *name = type_name_no_tag (type);
      /* FIXME: shouldn't we separately check the TYPE_NAME and the
         TYPE_TAG_NAME, and look in STRUCT_DOMAIN and/or VAR_DOMAIN
         as appropriate?  (this code was written before TYPE_NAME and
         TYPE_TAG_NAME were separate).  */
      struct symbol *sym;
      if (name == NULL)
	{
	  stub_noname_complaint ();
	  return type;
	}
      sym = lookup_symbol (name, 0, STRUCT_DOMAIN, 0, (struct symtab **) NULL);
      if (sym)
	make_cv_type (is_const, is_volatile, SYMBOL_TYPE (sym), &type);
    }

  if (TYPE_TARGET_STUB (type))
    {
      struct type *range_type;
      struct type *target_type = check_typedef (TYPE_TARGET_TYPE (type));

      if (TYPE_STUB (target_type) || TYPE_TARGET_STUB (target_type))
	{
	}
      else if (TYPE_CODE (type) == TYPE_CODE_ARRAY
	       && TYPE_NFIELDS (type) == 1
	       && (TYPE_CODE (range_type = TYPE_FIELD_TYPE (type, 0))
		   == TYPE_CODE_RANGE))
	{
	  /* Now recompute the length of the array type, based on its
	     number of elements and the target type's length.  */
	  TYPE_LENGTH (type) =
	    ((TYPE_FIELD_BITPOS (range_type, 1)
	      - TYPE_FIELD_BITPOS (range_type, 0)
	      + 1)
	     * TYPE_LENGTH (target_type));
	  TYPE_FLAGS (type) &= ~TYPE_FLAG_TARGET_STUB;
	}
      else if (TYPE_CODE (type) == TYPE_CODE_RANGE)
	{
	  TYPE_LENGTH (type) = TYPE_LENGTH (target_type);
	  TYPE_FLAGS (type) &= ~TYPE_FLAG_TARGET_STUB;
	}
    }

  /* FIXME - if you have a typedef for a struct where the typedef & the
  struct have the same name, the target_type of the TYPEDEF's type points
  back to the type itself.  This is wrong, and causes grief in many places
  in gdb.  We need to figure out why this is happening, but for now, let's
  protect against it here... */

  if (TYPE_TARGET_TYPE (type) == type)
    {
      /* TYPE_CODE_UNDEF */
      TYPE_TARGET_TYPE (type) = alloc_type (NULL);    
    }
        
  /* Cache TYPE_LENGTH for future use.  Only change it if it actually
     needs changing, in case the symbol information is in a read-only
     section. */
  if (TYPE_LENGTH (orig_type) != TYPE_LENGTH (type))
    TYPE_LENGTH (orig_type) = TYPE_LENGTH (type);

  return type;
}

/* Parse a type expression in the string [P..P+LENGTH).  If an error occurs,
   silently return builtin_type_void. */

static struct type *
safe_parse_type (char *p, int length)
{
  struct ui_file *saved_gdb_stderr;
  struct type *type;

  /* Suppress error messages. */
  saved_gdb_stderr = gdb_stderr;
  gdb_stderr = ui_file_new ();

  /* Call parse_and_eval_type() without fear of longjmp()s. */
  if (!gdb_parse_and_eval_type (p, length, &type))
    type = builtin_type_void;

  /* Stop suppressing error messages. */
  ui_file_delete (gdb_stderr);
  gdb_stderr = saved_gdb_stderr;

  return type;
}

/* Ugly hack to convert method stubs into method types.

   He ain't kiddin'.  This demangles the name of the method into a string
   including argument types, parses out each argument type, generates
   a string casting a zero to that type, evaluates the string, and stuffs
   the resulting type into an argtype vector!!!  Then it knows the type
   of the whole function (including argument types for overloading),
   which info used to be in the stab's but was removed to hack back
   the space required for them.  */

static void
check_stub_method (struct type *type, int method_id, int signature_id)
{
  struct fn_field *f;
  char *mangled_name = gdb_mangle_name (type, method_id, signature_id);
  char *demangled_name = cplus_demangle (mangled_name,
					 DMGL_PARAMS | DMGL_ANSI);
  char *argtypetext, *p;
  int depth = 0, argcount = 1;
  struct field *argtypes;
  struct type *mtype;

  /* Make sure we got back a function string that we can use.  */
  if (demangled_name)
    p = strchr (demangled_name, '(');
  else
    p = NULL;

  if (demangled_name == NULL || p == NULL)
    error (_("Internal: Cannot demangle mangled name `%s'."), mangled_name);

  /* Now, read in the parameters that define this type.  */
  p += 1;
  argtypetext = p;
  while (*p)
    {
      if (*p == '(' || *p == '<')
	{
	  depth += 1;
	}
      else if (*p == ')' || *p == '>')
	{
	  depth -= 1;
	}
      else if (*p == ',' && depth == 0)
	{
	  argcount += 1;
	}

      p += 1;
    }

  /* If we read one argument and it was ``void'', don't count it.  */
  if (strncmp (argtypetext, "(void)", 6) == 0)
    argcount -= 1;

  /* We need one extra slot, for the THIS pointer.  */

  argtypes = (struct field *)
    TYPE_ALLOC (type, (argcount + 1) * sizeof (struct field));
  p = argtypetext;

  /* Add THIS pointer for non-static methods.  */
  f = TYPE_FN_FIELDLIST1 (type, method_id);
  if (TYPE_FN_FIELD_STATIC_P (f, signature_id))
    argcount = 0;
  else
    {
      argtypes[0].type = lookup_pointer_type (type);
      argcount = 1;
    }

  if (*p != ')')		/* () means no args, skip while */
    {
      depth = 0;
      while (*p)
	{
	  if (depth <= 0 && (*p == ',' || *p == ')'))
	    {
	      /* Avoid parsing of ellipsis, they will be handled below.
	         Also avoid ``void'' as above.  */
	      if (strncmp (argtypetext, "...", p - argtypetext) != 0
		  && strncmp (argtypetext, "void", p - argtypetext) != 0)
		{
		  argtypes[argcount].type =
		    safe_parse_type (argtypetext, p - argtypetext);
		  argcount += 1;
		}
	      argtypetext = p + 1;
	    }

	  if (*p == '(' || *p == '<')
	    {
	      depth += 1;
	    }
	  else if (*p == ')' || *p == '>')
	    {
	      depth -= 1;
	    }

	  p += 1;
	}
    }

  TYPE_FN_FIELD_PHYSNAME (f, signature_id) = mangled_name;

  /* Now update the old "stub" type into a real type.  */
  mtype = TYPE_FN_FIELD_TYPE (f, signature_id);
  TYPE_DOMAIN_TYPE (mtype) = type;
  TYPE_FIELDS (mtype) = argtypes;
  TYPE_NFIELDS (mtype) = argcount;
  TYPE_FLAGS (mtype) &= ~TYPE_FLAG_STUB;
  TYPE_FN_FIELD_STUB (f, signature_id) = 0;
  if (p[-2] == '.')
    TYPE_FLAGS (mtype) |= TYPE_FLAG_VARARGS;

  xfree (demangled_name);
}

/* This is the external interface to check_stub_method, above.  This function
   unstubs all of the signatures for TYPE's METHOD_ID method name.  After
   calling this function TYPE_FN_FIELD_STUB will be cleared for each signature
   and TYPE_FN_FIELDLIST_NAME will be correct.

   This function unfortunately can not die until stabs do.  */

void
check_stub_method_group (struct type *type, int method_id)
{
  int len = TYPE_FN_FIELDLIST_LENGTH (type, method_id);
  struct fn_field *f = TYPE_FN_FIELDLIST1 (type, method_id);
  int j, found_stub = 0;

  for (j = 0; j < len; j++)
    if (TYPE_FN_FIELD_STUB (f, j))
      {
	found_stub = 1;
	check_stub_method (type, method_id, j);
      }

  /* GNU v3 methods with incorrect names were corrected when we read in
     type information, because it was cheaper to do it then.  The only GNU v2
     methods with incorrect method names are operators and destructors;
     destructors were also corrected when we read in type information.

     Therefore the only thing we need to handle here are v2 operator
     names.  */
  if (found_stub && strncmp (TYPE_FN_FIELD_PHYSNAME (f, 0), "_Z", 2) != 0)
    {
      int ret;
      char dem_opname[256];

      ret = cplus_demangle_opname (TYPE_FN_FIELDLIST_NAME (type, method_id),
				   dem_opname, DMGL_ANSI);
      if (!ret)
	ret = cplus_demangle_opname (TYPE_FN_FIELDLIST_NAME (type, method_id),
				     dem_opname, 0);
      if (ret)
	{
	  TYPE_FN_FIELDLIST_NAME (type, method_id) =
	    TYPE_ALLOC (type, strlen (dem_opname) + 1);
	  strcpy (TYPE_FN_FIELDLIST_NAME (type, method_id), dem_opname);
	}
    }
}

/* APPLE LOCAL: Not const because we assign random
   junk to it via the NONULL macros over in gdbtypes.h */
struct cplus_struct_type cplus_struct_default;

void
allocate_cplus_struct_type (struct type *type)
{
  if (!HAVE_CPLUS_STRUCT (type))
    {
      TYPE_CPLUS_SPECIFIC (type) = (struct cplus_struct_type *)
	TYPE_ALLOC (type, sizeof (struct cplus_struct_type));
      *(TYPE_CPLUS_SPECIFIC (type)) = cplus_struct_default;
    }
}

/* Helper function to initialize the standard scalar types.

   If NAME is non-NULL and OBJFILE is non-NULL, then we make a copy
   of the string pointed to by name in the objfile_obstack for that objfile,
   and initialize the type name to that copy.  There are places (mipsread.c
   in particular, where init_type is called with a NULL value for NAME). */

struct type *
init_type (enum type_code code, int length, int flags, char *name,
	   struct objfile *objfile)
{
  struct type *type;

  type = alloc_type (objfile);
  TYPE_CODE (type) = code;
  TYPE_LENGTH (type) = length;
  TYPE_FLAGS (type) |= flags;
  if ((name != NULL) && (objfile != NULL))
    {
      TYPE_NAME (type) =
	obsavestring (name, strlen (name), &objfile->objfile_obstack);
    }
  else
    {
      TYPE_NAME (type) = name;
    }

  /* C++ fancies.  */

  if (name && strcmp (name, "char") == 0)
    TYPE_FLAGS (type) |= TYPE_FLAG_NOSIGN;

  /* APPLE LOCAL: gcc 3.3 uses 8 byte long doubles, but
     gcc 4.0 uses 16 byte long doubles.  But there is only one
     TARGET_LONG_DOUBLE_BIT & TARGET_LONG_DOUBLE_FORMAT available.
     So we have to patch another one into the format here.  */
  if (code == TYPE_CODE_FLT && name && strcmp (name, "long double") == 0)
    {
      if (length * TARGET_CHAR_BIT != TARGET_LONG_DOUBLE_BIT)
	{
	  TYPE_FLOATFORMAT(type) = gdbarch_long_double_format (current_gdbarch); 
	}
    }

  /* END APPLE LOCAL */

  if (code == TYPE_CODE_STRUCT || code == TYPE_CODE_UNION
      || code == TYPE_CODE_NAMESPACE)
    {
      INIT_CPLUS_SPECIFIC (type);
    }
  return (type);
}

/* Helper function.  Create an empty composite type.  */

struct type *
init_composite_type (char *name, enum type_code code)
{
  struct type *t;
  gdb_assert (code == TYPE_CODE_STRUCT
	      || code == TYPE_CODE_UNION);
  t = init_type (code, 0, 0, NULL, NULL);
  TYPE_TAG_NAME (t) = name;
  return t;
}

/* Helper function.  Append a field to a composite type.  */

void
append_composite_type_field (struct type *t, char *name, struct type *field)
{
  struct field *f;
  TYPE_NFIELDS (t) = TYPE_NFIELDS (t) + 1;
  TYPE_FIELDS (t) = xrealloc (TYPE_FIELDS (t),
			      sizeof (struct field) * TYPE_NFIELDS (t));
  f = &(TYPE_FIELDS (t)[TYPE_NFIELDS (t) - 1]);
  memset (f, 0, sizeof f[0]);
  FIELD_TYPE (f[0]) = field;
  FIELD_NAME (f[0]) = name;
  if (TYPE_CODE (t) == TYPE_CODE_UNION)
    {
      if (TYPE_LENGTH (t) < TYPE_LENGTH (field))
	TYPE_LENGTH (t) = TYPE_LENGTH (field);
    }
  else if (TYPE_CODE (t) == TYPE_CODE_STRUCT)
    {
      TYPE_LENGTH (t) = TYPE_LENGTH (t) + TYPE_LENGTH (field);
      if (TYPE_NFIELDS (t) > 1)
	{
	  FIELD_BITPOS (f[0]) = (FIELD_BITPOS (f[-1])
				 + TYPE_LENGTH (field) * TARGET_CHAR_BIT);
	}
    }
}

/* Look up a fundamental type for the specified objfile.
   May need to construct such a type if this is the first use.

   Some object file formats (ELF, COFF, etc) do not define fundamental
   types such as "int" or "double".  Others (stabs for example), do
   define fundamental types.

   For the formats which don't provide fundamental types, gdb can create
   such types, using defaults reasonable for the current language and
   the current target machine.

   NOTE:  This routine is obsolescent.  Each debugging format reader
   should manage it's own fundamental types, either creating them from
   suitable defaults or reading them from the debugging information,
   whichever is appropriate.  The DWARF reader has already been
   fixed to do this.  Once the other readers are fixed, this routine
   will go away.  Also note that fundamental types should be managed
   on a compilation unit basis in a multi-language environment, not
   on a linkage unit basis as is done here. */


struct type *
lookup_fundamental_type (struct objfile *objfile, int typeid)
{
  struct type **typep;
  int nbytes;

  if (typeid < 0 || typeid >= FT_NUM_MEMBERS)
    {
      error (_("internal error - invalid fundamental type id %d"), typeid);
    }

  /* If this is the first time we need a fundamental type for this objfile
     then we need to initialize the vector of type pointers. */

  if (objfile->fundamental_types == NULL)
    {
      nbytes = FT_NUM_MEMBERS * sizeof (struct type *);
      objfile->fundamental_types = (struct type **)
	obstack_alloc (&objfile->objfile_obstack, nbytes);
      memset ((char *) objfile->fundamental_types, 0, nbytes);
      OBJSTAT (objfile, n_types += FT_NUM_MEMBERS);
    }

  /* Look for this particular type in the fundamental type vector.  If one is
     not found, create and install one appropriate for the current language. */

  typep = objfile->fundamental_types + typeid;
  if (*typep == NULL)
    {
      *typep = create_fundamental_type (objfile, typeid);
    }

  return (*typep);
}

int
can_dereference (struct type *t)
{
  /* FIXME: Should we return true for references as well as pointers?  */
  CHECK_TYPEDEF (t);
  return
    (t != NULL
     && TYPE_CODE (t) == TYPE_CODE_PTR
     && TYPE_CODE (TYPE_TARGET_TYPE (t)) != TYPE_CODE_VOID);
}

int
is_integral_type (struct type *t)
{
  CHECK_TYPEDEF (t);
  return
    ((t != NULL)
     && ((TYPE_CODE (t) == TYPE_CODE_INT)
	 || (TYPE_CODE (t) == TYPE_CODE_ENUM)
	 || (TYPE_CODE (t) == TYPE_CODE_CHAR)
	 || (TYPE_CODE (t) == TYPE_CODE_RANGE)
	 || (TYPE_CODE (t) == TYPE_CODE_BOOL)));
}

/* Check whether BASE is an ancestor or base class or DCLASS 
   Return 1 if so, and 0 if not.
   Note: callers may want to check for identity of the types before
   calling this function -- identical types are considered to satisfy
   the ancestor relationship even if they're identical */

int
is_ancestor (struct type *base, struct type *dclass)
{
  int i;

  CHECK_TYPEDEF (base);
  CHECK_TYPEDEF (dclass);

  if (base == dclass)
    return 1;
  if (TYPE_NAME (base) && TYPE_NAME (dclass) &&
      !strcmp (TYPE_NAME (base), TYPE_NAME (dclass)))
    return 1;

  for (i = 0; i < TYPE_N_BASECLASSES (dclass); i++)
    if (is_ancestor (base, TYPE_BASECLASS (dclass, i)))
      return 1;

  return 0;
}

/* APPLE LOCAL: Like is_ancestor except that we only have the name of
   BASE, not the type.  */
int
is_ancestor_by_name (const char *base, struct type *dclass)
{
  int i;

  CHECK_TYPEDEF (dclass);

  if (TYPE_NAME (dclass) &&
      !strcmp (base, TYPE_NAME (dclass)))
    return 1;

  for (i = 0; i < TYPE_N_BASECLASSES (dclass); i++)
    if (is_ancestor_by_name (base, TYPE_BASECLASS (dclass, i)))
      return 1;

  return 0;

}

/* See whether DCLASS has a virtual table.  This routine is aimed at
   the HP/Taligent ANSI C++ runtime model, and may not work with other
   runtime models.  Return 1 => Yes, 0 => No.  */

int
has_vtable (struct type *dclass)
{
  /* In the HP ANSI C++ runtime model, a class has a vtable only if it
     has virtual functions or virtual bases.  */

  int i;

  if (TYPE_CODE (dclass) != TYPE_CODE_CLASS)
    return 0;

  /* First check for the presence of virtual bases */
  if (TYPE_FIELD_VIRTUAL_BITS (dclass))
    for (i = 0; i < TYPE_N_BASECLASSES (dclass); i++)
      if (B_TST (TYPE_FIELD_VIRTUAL_BITS (dclass), i))
	return 1;

  /* Next check for virtual functions */
  if (TYPE_FN_FIELDLISTS (dclass))
    for (i = 0; i < TYPE_NFN_FIELDS (dclass); i++)
      if (TYPE_FN_FIELD_VIRTUAL_P (TYPE_FN_FIELDLIST1 (dclass, i), 0))
	return 1;

  /* Recurse on non-virtual bases to see if any of them needs a vtable */
  if (TYPE_FIELD_VIRTUAL_BITS (dclass))
    for (i = 0; i < TYPE_N_BASECLASSES (dclass); i++)
      if ((!B_TST (TYPE_FIELD_VIRTUAL_BITS (dclass), i)) &&
	  (has_vtable (TYPE_FIELD_TYPE (dclass, i))))
	return 1;

  /* Well, maybe we don't need a virtual table */
  return 0;
}

/* Return a pointer to the "primary base class" of DCLASS.

   A NULL return indicates that DCLASS has no primary base, or that it
   couldn't be found (insufficient information).

   This routine is aimed at the HP/Taligent ANSI C++ runtime model,
   and may not work with other runtime models.  */

struct type *
primary_base_class (struct type *dclass)
{
  /* In HP ANSI C++'s runtime model, a "primary base class" of a class
     is the first directly inherited, non-virtual base class that
     requires a virtual table */

  int i;

  if (TYPE_CODE (dclass) != TYPE_CODE_CLASS)
    return NULL;

  for (i = 0; i < TYPE_N_BASECLASSES (dclass); i++)
    if (!TYPE_FIELD_VIRTUAL (dclass, i) &&
	has_vtable (TYPE_FIELD_TYPE (dclass, i)))
      return TYPE_FIELD_TYPE (dclass, i);

  return NULL;
}

/* Global manipulated by virtual_base_list[_aux]() */

static struct vbase *current_vbase_list = NULL;

/* Return a pointer to a null-terminated list of struct vbase
   items. The vbasetype pointer of each item in the list points to the
   type information for a virtual base of the argument DCLASS.

   Helper function for virtual_base_list(). 
   Note: the list goes backward, right-to-left. virtual_base_list()
   copies the items out in reverse order.  */

static void
virtual_base_list_aux (struct type *dclass)
{
  struct vbase *tmp_vbase;
  int i;

  if (TYPE_CODE (dclass) != TYPE_CODE_CLASS)
    return;

  for (i = 0; i < TYPE_N_BASECLASSES (dclass); i++)
    {
      /* Recurse on this ancestor, first */
      virtual_base_list_aux (TYPE_FIELD_TYPE (dclass, i));

      /* If this current base is itself virtual, add it to the list */
      if (BASETYPE_VIA_VIRTUAL (dclass, i))
	{
	  struct type *basetype = TYPE_FIELD_TYPE (dclass, i);

	  /* Check if base already recorded */
	  tmp_vbase = current_vbase_list;
	  while (tmp_vbase)
	    {
	      if (tmp_vbase->vbasetype == basetype)
		break;		/* found it */
	      tmp_vbase = tmp_vbase->next;
	    }

	  if (!tmp_vbase)	/* normal exit from loop */
	    {
	      /* Allocate new item for this virtual base */
	      tmp_vbase = (struct vbase *) xmalloc (sizeof (struct vbase));

	      /* Stick it on at the end of the list */
	      tmp_vbase->vbasetype = basetype;
	      tmp_vbase->next = current_vbase_list;
	      current_vbase_list = tmp_vbase;
	    }
	}			/* if virtual */
    }				/* for loop over bases */
}


/* Compute the list of virtual bases in the right order.  Virtual
   bases are laid out in the object's memory area in order of their
   occurrence in a depth-first, left-to-right search through the
   ancestors.

   Argument DCLASS is the type whose virtual bases are required.
   Return value is the address of a null-terminated array of pointers
   to struct type items.

   This routine is aimed at the HP/Taligent ANSI C++ runtime model,
   and may not work with other runtime models.

   This routine merely hands off the argument to virtual_base_list_aux()
   and then copies the result into an array to save space.  */

struct type **
virtual_base_list (struct type *dclass)
{
  struct vbase *tmp_vbase;
  struct vbase *tmp_vbase_2;
  int i;
  int count;
  struct type **vbase_array;

  current_vbase_list = NULL;
  virtual_base_list_aux (dclass);

  for (i = 0, tmp_vbase = current_vbase_list; tmp_vbase != NULL; i++, tmp_vbase = tmp_vbase->next)
    /* no body */ ;

  count = i;

  vbase_array = (struct type **) xmalloc ((count + 1) * sizeof (struct type *));

  for (i = count - 1, tmp_vbase = current_vbase_list; i >= 0; i--, tmp_vbase = tmp_vbase->next)
    vbase_array[i] = tmp_vbase->vbasetype;

  /* Get rid of constructed chain */
  tmp_vbase_2 = tmp_vbase = current_vbase_list;
  while (tmp_vbase)
    {
      tmp_vbase = tmp_vbase->next;
      xfree (tmp_vbase_2);
      tmp_vbase_2 = tmp_vbase;
    }

  vbase_array[count] = NULL;
  return vbase_array;
}

/* Return the length of the virtual base list of the type DCLASS.  */

int
virtual_base_list_length (struct type *dclass)
{
  int i;
  struct vbase *tmp_vbase;

  current_vbase_list = NULL;
  virtual_base_list_aux (dclass);

  for (i = 0, tmp_vbase = current_vbase_list; tmp_vbase != NULL; i++, tmp_vbase = tmp_vbase->next)
    /* no body */ ;
  return i;
}

/* Return the number of elements of the virtual base list of the type
   DCLASS, ignoring those appearing in the primary base (and its
   primary base, recursively).  */

int
virtual_base_list_length_skip_primaries (struct type *dclass)
{
  int i;
  struct vbase *tmp_vbase;
  struct type *primary;

  primary = TYPE_RUNTIME_PTR (dclass) ? TYPE_PRIMARY_BASE (dclass) : NULL;

  if (!primary)
    return virtual_base_list_length (dclass);

  current_vbase_list = NULL;
  virtual_base_list_aux (dclass);

  for (i = 0, tmp_vbase = current_vbase_list; tmp_vbase != NULL; tmp_vbase = tmp_vbase->next)
    {
      if (virtual_base_index (tmp_vbase->vbasetype, primary) >= 0)
	continue;
      i++;
    }
  return i;
}


/* Return the index (position) of type BASE, which is a virtual base
   class of DCLASS, in the latter's virtual base list.  A return of -1
   indicates "not found" or a problem.  */

int
virtual_base_index (struct type *base, struct type *dclass)
{
  struct type *vbase;
  int i;

  if ((TYPE_CODE (dclass) != TYPE_CODE_CLASS) ||
      (TYPE_CODE (base) != TYPE_CODE_CLASS))
    return -1;

  i = 0;
  vbase = virtual_base_list (dclass)[0];
  while (vbase)
    {
      if (vbase == base)
	break;
      vbase = virtual_base_list (dclass)[++i];
    }

  return vbase ? i : -1;
}



/* Return the index (position) of type BASE, which is a virtual base
   class of DCLASS, in the latter's virtual base list. Skip over all
   bases that may appear in the virtual base list of the primary base
   class of DCLASS (recursively).  A return of -1 indicates "not
   found" or a problem.  */

int
virtual_base_index_skip_primaries (struct type *base, struct type *dclass)
{
  struct type *vbase;
  int i, j;
  struct type *primary;

  if ((TYPE_CODE (dclass) != TYPE_CODE_CLASS) ||
      (TYPE_CODE (base) != TYPE_CODE_CLASS))
    return -1;

  primary = TYPE_RUNTIME_PTR (dclass) ? TYPE_PRIMARY_BASE (dclass) : NULL;

  j = -1;
  i = 0;
  vbase = virtual_base_list (dclass)[0];
  while (vbase)
    {
      if (!primary || (virtual_base_index_skip_primaries (vbase, primary) < 0))
	j++;
      if (vbase == base)
	break;
      vbase = virtual_base_list (dclass)[++i];
    }

  return vbase ? j : -1;
}

/* Return position of a derived class DCLASS in the list of
 * primary bases starting with the remotest ancestor.
 * Position returned is 0-based. */

int
class_index_in_primary_list (struct type *dclass)
{
  struct type *pbc;		/* primary base class */

  /* Simply recurse on primary base */
  pbc = TYPE_PRIMARY_BASE (dclass);
  if (pbc)
    return 1 + class_index_in_primary_list (pbc);
  else
    return 0;
}

/* Return a count of the number of virtual functions a type has.
 * This includes all the virtual functions it inherits from its
 * base classes too.
 */

/* pai: FIXME This doesn't do the right thing: count redefined virtual
 * functions only once (latest redefinition)
 */

int
count_virtual_fns (struct type *dclass)
{
  int fn, oi;			/* function and overloaded instance indices */
  int vfuncs;			/* count to return */

  /* recurse on bases that can share virtual table */
  struct type *pbc = primary_base_class (dclass);
  if (pbc)
    vfuncs = count_virtual_fns (pbc);
  else
    vfuncs = 0;

  for (fn = 0; fn < TYPE_NFN_FIELDS (dclass); fn++)
    for (oi = 0; oi < TYPE_FN_FIELDLIST_LENGTH (dclass, fn); oi++)
      if (TYPE_FN_FIELD_VIRTUAL_P (TYPE_FN_FIELDLIST1 (dclass, fn), oi))
	vfuncs++;

  return vfuncs;
}



/* Functions for overload resolution begin here */

/* Compare two badness vectors A and B and return the result.
 * 0 => A and B are identical
 * 1 => A and B are incomparable
 * 2 => A is better than B
 * 3 => A is worse than B */

int
compare_badness (struct badness_vector *a, struct badness_vector *b)
{
  int i;
  int tmp;
  short found_pos = 0;		/* any positives in c? */
  short found_neg = 0;		/* any negatives in c? */

  /* differing lengths => incomparable */
  if (a->length != b->length)
    return 1;

  /* Subtract b from a */
  for (i = 0; i < a->length; i++)
    {
      tmp = a->rank[i] - b->rank[i];
      if (tmp > 0)
	found_pos = 1;
      else if (tmp < 0)
	found_neg = 1;
    }

  if (found_pos)
    {
      if (found_neg)
	return 1;		/* incomparable */
      else
	return 3;		/* A > B */
    }
  else
    /* no positives */
    {
      if (found_neg)
	return 2;		/* A < B */
      else
	return 0;		/* A == B */
    }
}

/* Rank a function by comparing its parameter types (PARMS, length NPARMS),
 * to the types of an argument list (ARGS, length NARGS).
 * Return a pointer to a badness vector. This has NARGS + 1 entries. */

struct badness_vector *
rank_function (struct type **parms, int nparms, struct type **args, int nargs)
{
  int i;
  struct badness_vector *bv;
  int min_len = nparms < nargs ? nparms : nargs;

  bv = xmalloc (sizeof (struct badness_vector));
  bv->length = nargs + 1;	/* add 1 for the length-match rank */
  bv->rank = xmalloc ((nargs + 1) * sizeof (int));

  /* First compare the lengths of the supplied lists.
   * If there is a mismatch, set it to a high value. */

  /* pai/1997-06-03 FIXME: when we have debug info about default
   * arguments and ellipsis parameter lists, we should consider those
   * and rank the length-match more finely. */

  LENGTH_MATCH (bv) = (nargs != nparms) ? LENGTH_MISMATCH_BADNESS : 0;

  /* Now rank all the parameters of the candidate function */
  for (i = 1; i <= min_len; i++)
    bv->rank[i] = rank_one_type (parms[i-1], args[i-1]);

  /* If more arguments than parameters, add dummy entries */
  for (i = min_len + 1; i <= nargs; i++)
    bv->rank[i] = TOO_FEW_PARAMS_BADNESS;

  return bv;
}

/* Compare the names of two integer types, assuming that any sign
   qualifiers have been checked already.  We do it this way because
   there may be an "int" in the name of one of the types.  */

static int
integer_types_same_name_p (const char *first, const char *second)
{
  int first_p, second_p;

  /* If both are shorts, return 1; if neither is a short, keep checking.  */
  first_p = (strstr (first, "short") != NULL);
  second_p = (strstr (second, "short") != NULL);
  if (first_p && second_p)
    return 1;
  if (first_p || second_p)
    return 0;

  /* Likewise for long.  */
  first_p = (strstr (first, "long") != NULL);
  second_p = (strstr (second, "long") != NULL);
  if (first_p && second_p)
    return 1;
  if (first_p || second_p)
    return 0;

  /* Likewise for char.  */
  first_p = (strstr (first, "char") != NULL);
  second_p = (strstr (second, "char") != NULL);
  if (first_p && second_p)
    return 1;
  if (first_p || second_p)
    return 0;

  /* They must both be ints.  */
  return 1;
}

/* Compare one type (PARM) for compatibility with another (ARG).
 * PARM is intended to be the parameter type of a function; and
 * ARG is the supplied argument's type.  This function tests if
 * the latter can be converted to the former.
 *
 * Return 0 if they are identical types;
 * Otherwise, return an integer which corresponds to how compatible
 * PARM is to ARG. The higher the return value, the worse the match.
 * Generally the "bad" conversions are all uniformly assigned a 100 */

int
rank_one_type (struct type *parm, struct type *arg)
{
  /* Identical type pointers */
  /* However, this still doesn't catch all cases of same type for arg
   * and param. The reason is that builtin types are different from
   * the same ones constructed from the object. */
  if (parm == arg)
    return 0;

  /* Resolve typedefs */
  if (TYPE_CODE (parm) == TYPE_CODE_TYPEDEF)
    parm = check_typedef (parm);
  if (TYPE_CODE (arg) == TYPE_CODE_TYPEDEF)
    arg = check_typedef (arg);

  /*
     Well, damnit, if the names are exactly the same,
     i'll say they are exactly the same. This happens when we generate
     method stubs. The types won't point to the same address, but they
     really are the same.
  */

  if (TYPE_NAME (parm) && TYPE_NAME (arg) &&
      !strcmp (TYPE_NAME (parm), TYPE_NAME (arg)))
      return 0;

  /* Check if identical after resolving typedefs */
  if (parm == arg)
    return 0;

  /* See through references, since we can almost make non-references
     references. */
  if (TYPE_CODE (arg) == TYPE_CODE_REF)
    return (rank_one_type (parm, TYPE_TARGET_TYPE (arg))
	    + REFERENCE_CONVERSION_BADNESS);
  if (TYPE_CODE (parm) == TYPE_CODE_REF)
    return (rank_one_type (TYPE_TARGET_TYPE (parm), arg)
	    + REFERENCE_CONVERSION_BADNESS);
  if (overload_debug)
  /* Debugging only. */
    fprintf_filtered (gdb_stderr,"------ Arg is %s [%d], parm is %s [%d]\n",
        TYPE_NAME (arg), TYPE_CODE (arg), TYPE_NAME (parm), TYPE_CODE (parm));

  /* x -> y means arg of type x being supplied for parameter of type y */

  switch (TYPE_CODE (parm))
    {
    case TYPE_CODE_PTR:
      switch (TYPE_CODE (arg))
	{
	case TYPE_CODE_PTR:
	  if (TYPE_CODE (TYPE_TARGET_TYPE (parm)) == TYPE_CODE_VOID)
	    return VOID_PTR_CONVERSION_BADNESS;
	  else
	    return rank_one_type (TYPE_TARGET_TYPE (parm), TYPE_TARGET_TYPE (arg));
	case TYPE_CODE_ARRAY:
	  return rank_one_type (TYPE_TARGET_TYPE (parm), TYPE_TARGET_TYPE (arg));
	case TYPE_CODE_FUNC:
	  return rank_one_type (TYPE_TARGET_TYPE (parm), arg);
	case TYPE_CODE_INT:
	case TYPE_CODE_ENUM:
	case TYPE_CODE_CHAR:
	case TYPE_CODE_RANGE:
	case TYPE_CODE_BOOL:
	  return POINTER_CONVERSION_BADNESS;
	default:
	  return INCOMPATIBLE_TYPE_BADNESS;
	}
    case TYPE_CODE_ARRAY:
      switch (TYPE_CODE (arg))
	{
	case TYPE_CODE_PTR:
	case TYPE_CODE_ARRAY:
	  return rank_one_type (TYPE_TARGET_TYPE (parm), TYPE_TARGET_TYPE (arg));
	default:
	  return INCOMPATIBLE_TYPE_BADNESS;
	}
    case TYPE_CODE_FUNC:
      switch (TYPE_CODE (arg))
	{
	case TYPE_CODE_PTR:	/* funcptr -> func */
	  return rank_one_type (parm, TYPE_TARGET_TYPE (arg));
	default:
	  return INCOMPATIBLE_TYPE_BADNESS;
	}
    case TYPE_CODE_INT:
      switch (TYPE_CODE (arg))
	{
	case TYPE_CODE_INT:
	  if (TYPE_LENGTH (arg) == TYPE_LENGTH (parm))
	    {
	      /* Deal with signed, unsigned, and plain chars and
	         signed and unsigned ints */
	      if (TYPE_NOSIGN (parm))
		{
		  /* This case only for character types */
		  if (TYPE_NOSIGN (arg))	/* plain char -> plain char */
		    return 0;
		  else
		    return INTEGER_CONVERSION_BADNESS;	/* signed/unsigned char -> plain char */
		}
	      else if (TYPE_UNSIGNED (parm))
		{
		  if (TYPE_UNSIGNED (arg))
		    {
		      /* unsigned int -> unsigned int, or unsigned long -> unsigned long */
		      if (integer_types_same_name_p (TYPE_NAME (parm), TYPE_NAME (arg)))
			return 0;
		      else if (integer_types_same_name_p (TYPE_NAME (arg), "int")
			       && integer_types_same_name_p (TYPE_NAME (parm), "long"))
			return INTEGER_PROMOTION_BADNESS;	/* unsigned int -> unsigned long */
		      else
			return INTEGER_CONVERSION_BADNESS;	/* unsigned long -> unsigned int */
		    }
		  else
		    {
		      if (integer_types_same_name_p (TYPE_NAME (arg), "long")
			  && integer_types_same_name_p (TYPE_NAME (parm), "int"))
			return INTEGER_CONVERSION_BADNESS;	/* signed long -> unsigned int */
		      else
			return INTEGER_CONVERSION_BADNESS;	/* signed int/long -> unsigned int/long */
		    }
		}
	      else if (!TYPE_NOSIGN (arg) && !TYPE_UNSIGNED (arg))
		{
		  if (integer_types_same_name_p (TYPE_NAME (parm), TYPE_NAME (arg)))
		    return 0;
		  else if (integer_types_same_name_p (TYPE_NAME (arg), "int")
			   && integer_types_same_name_p (TYPE_NAME (parm), "long"))
		    return INTEGER_PROMOTION_BADNESS;
		  else
		    return INTEGER_CONVERSION_BADNESS;
		}
	      else
		return INTEGER_CONVERSION_BADNESS;
	    }
	  else if (TYPE_LENGTH (arg) < TYPE_LENGTH (parm))
	    return INTEGER_PROMOTION_BADNESS;
	  else
	    return INTEGER_CONVERSION_BADNESS;
	case TYPE_CODE_ENUM:
	case TYPE_CODE_CHAR:
	case TYPE_CODE_RANGE:
	case TYPE_CODE_BOOL:
	  return INTEGER_PROMOTION_BADNESS;
	case TYPE_CODE_FLT:
	  return INT_FLOAT_CONVERSION_BADNESS;
	case TYPE_CODE_PTR:
	  return NS_POINTER_CONVERSION_BADNESS;
	default:
	  return INCOMPATIBLE_TYPE_BADNESS;
	}
      break;
    case TYPE_CODE_ENUM:
      switch (TYPE_CODE (arg))
	{
	case TYPE_CODE_INT:
	case TYPE_CODE_CHAR:
	case TYPE_CODE_RANGE:
	case TYPE_CODE_BOOL:
	case TYPE_CODE_ENUM:
	  return INTEGER_CONVERSION_BADNESS;
	case TYPE_CODE_FLT:
	  return INT_FLOAT_CONVERSION_BADNESS;
	default:
	  return INCOMPATIBLE_TYPE_BADNESS;
	}
      break;
    case TYPE_CODE_CHAR:
      switch (TYPE_CODE (arg))
	{
	case TYPE_CODE_RANGE:
	case TYPE_CODE_BOOL:
	case TYPE_CODE_ENUM:
	  return INTEGER_CONVERSION_BADNESS;
	case TYPE_CODE_FLT:
	  return INT_FLOAT_CONVERSION_BADNESS;
	case TYPE_CODE_INT:
	  if (TYPE_LENGTH (arg) > TYPE_LENGTH (parm))
	    return INTEGER_CONVERSION_BADNESS;
	  else if (TYPE_LENGTH (arg) < TYPE_LENGTH (parm))
	    return INTEGER_PROMOTION_BADNESS;
	  /* >>> !! else fall through !! <<< */
	case TYPE_CODE_CHAR:
	  /* Deal with signed, unsigned, and plain chars for C++
	     and with int cases falling through from previous case */
	  if (TYPE_NOSIGN (parm))
	    {
	      if (TYPE_NOSIGN (arg))
		return 0;
	      else
		return INTEGER_CONVERSION_BADNESS;
	    }
	  else if (TYPE_UNSIGNED (parm))
	    {
	      if (TYPE_UNSIGNED (arg))
		return 0;
	      else
		return INTEGER_PROMOTION_BADNESS;
	    }
	  else if (!TYPE_NOSIGN (arg) && !TYPE_UNSIGNED (arg))
	    return 0;
	  else
	    return INTEGER_CONVERSION_BADNESS;
	default:
	  return INCOMPATIBLE_TYPE_BADNESS;
	}
      break;
    case TYPE_CODE_RANGE:
      switch (TYPE_CODE (arg))
	{
	case TYPE_CODE_INT:
	case TYPE_CODE_CHAR:
	case TYPE_CODE_RANGE:
	case TYPE_CODE_BOOL:
	case TYPE_CODE_ENUM:
	  return INTEGER_CONVERSION_BADNESS;
	case TYPE_CODE_FLT:
	  return INT_FLOAT_CONVERSION_BADNESS;
	default:
	  return INCOMPATIBLE_TYPE_BADNESS;
	}
      break;
    case TYPE_CODE_BOOL:
      switch (TYPE_CODE (arg))
	{
	case TYPE_CODE_INT:
	case TYPE_CODE_CHAR:
	case TYPE_CODE_RANGE:
	case TYPE_CODE_ENUM:
	case TYPE_CODE_FLT:
	case TYPE_CODE_PTR:
	  return BOOLEAN_CONVERSION_BADNESS;
	case TYPE_CODE_BOOL:
	  return 0;
	default:
	  return INCOMPATIBLE_TYPE_BADNESS;
	}
      break;
    case TYPE_CODE_FLT:
      switch (TYPE_CODE (arg))
	{
	case TYPE_CODE_FLT:
	  if (TYPE_LENGTH (arg) < TYPE_LENGTH (parm))
	    return FLOAT_PROMOTION_BADNESS;
	  else if (TYPE_LENGTH (arg) == TYPE_LENGTH (parm))
	    return 0;
	  else
	    return FLOAT_CONVERSION_BADNESS;
	case TYPE_CODE_INT:
	case TYPE_CODE_BOOL:
	case TYPE_CODE_ENUM:
	case TYPE_CODE_RANGE:
	case TYPE_CODE_CHAR:
	  return INT_FLOAT_CONVERSION_BADNESS;
	default:
	  return INCOMPATIBLE_TYPE_BADNESS;
	}
      break;
    case TYPE_CODE_COMPLEX:
      switch (TYPE_CODE (arg))
	{			/* Strictly not needed for C++, but... */
	case TYPE_CODE_FLT:
	  return FLOAT_PROMOTION_BADNESS;
	case TYPE_CODE_COMPLEX:
	  return 0;
	default:
	  return INCOMPATIBLE_TYPE_BADNESS;
	}
      break;
    case TYPE_CODE_STRUCT:
      /* currently same as TYPE_CODE_CLASS */
      switch (TYPE_CODE (arg))
	{
	case TYPE_CODE_STRUCT:
	  /* Check for derivation */
	  if (is_ancestor (parm, arg))
	    return BASE_CONVERSION_BADNESS;
	  /* else fall through */
	default:
	  return INCOMPATIBLE_TYPE_BADNESS;
	}
      break;
    case TYPE_CODE_UNION:
      switch (TYPE_CODE (arg))
	{
	case TYPE_CODE_UNION:
	default:
	  return INCOMPATIBLE_TYPE_BADNESS;
	}
      break;
    case TYPE_CODE_MEMBER:
      switch (TYPE_CODE (arg))
	{
	default:
	  return INCOMPATIBLE_TYPE_BADNESS;
	}
      break;
    case TYPE_CODE_METHOD:
      switch (TYPE_CODE (arg))
	{

	default:
	  return INCOMPATIBLE_TYPE_BADNESS;
	}
      break;
    case TYPE_CODE_REF:
      switch (TYPE_CODE (arg))
	{

	default:
	  return INCOMPATIBLE_TYPE_BADNESS;
	}

      break;
    case TYPE_CODE_SET:
      switch (TYPE_CODE (arg))
	{
	  /* Not in C++ */
	case TYPE_CODE_SET:
	  return rank_one_type (TYPE_FIELD_TYPE (parm, 0), TYPE_FIELD_TYPE (arg, 0));
	default:
	  return INCOMPATIBLE_TYPE_BADNESS;
	}
      break;
    case TYPE_CODE_VOID:
    default:
      return INCOMPATIBLE_TYPE_BADNESS;
    }				/* switch (TYPE_CODE (arg)) */
}


/* End of functions for overload resolution */

static void
print_bit_vector (B_TYPE *bits, int nbits)
{
  int bitno;

  for (bitno = 0; bitno < nbits; bitno++)
    {
      if ((bitno % 8) == 0)
	{
	  puts_filtered (" ");
	}
      if (B_TST (bits, bitno))
	printf_filtered (("1"));
      else
	printf_filtered (("0"));
    }
}

/* Note the first arg should be the "this" pointer, we may not want to
   include it since we may get into a infinitely recursive situation.  */

static void
print_arg_types (struct field *args, int nargs, int spaces)
{
  if (args != NULL)
    {
      int i;

      for (i = 0; i < nargs; i++)
	recursive_dump_type (args[i].type, spaces + 2);
    }
}

static void
dump_fn_fieldlists (struct type *type, int spaces)
{
  int method_idx;
  int overload_idx;
  struct fn_field *f;

  printfi_filtered (spaces, "fn_fieldlists ");
  gdb_print_host_address (TYPE_FN_FIELDLISTS (type), gdb_stdout);
  printf_filtered ("\n");
  for (method_idx = 0; method_idx < TYPE_NFN_FIELDS (type); method_idx++)
    {
      f = TYPE_FN_FIELDLIST1 (type, method_idx);
      printfi_filtered (spaces + 2, "[%d] name '%s' (",
			method_idx,
			TYPE_FN_FIELDLIST_NAME (type, method_idx));
      gdb_print_host_address (TYPE_FN_FIELDLIST_NAME (type, method_idx),
			      gdb_stdout);
      printf_filtered (_(") length %d\n"),
		       TYPE_FN_FIELDLIST_LENGTH (type, method_idx));
      for (overload_idx = 0;
	   overload_idx < TYPE_FN_FIELDLIST_LENGTH (type, method_idx);
	   overload_idx++)
	{
	  printfi_filtered (spaces + 4, "[%d] physname '%s' (",
			    overload_idx,
			    TYPE_FN_FIELD_PHYSNAME (f, overload_idx));
	  gdb_print_host_address (TYPE_FN_FIELD_PHYSNAME (f, overload_idx),
				  gdb_stdout);
	  printf_filtered (")\n");
	  printfi_filtered (spaces + 8, "type ");
	  gdb_print_host_address (TYPE_FN_FIELD_TYPE (f, overload_idx), gdb_stdout);
	  printf_filtered ("\n");

	  recursive_dump_type (TYPE_FN_FIELD_TYPE (f, overload_idx),
			       spaces + 8 + 2);

	  printfi_filtered (spaces + 8, "args ");
	  gdb_print_host_address (TYPE_FN_FIELD_ARGS (f, overload_idx), gdb_stdout);
	  printf_filtered ("\n");

	  print_arg_types (TYPE_FN_FIELD_ARGS (f, overload_idx),
			   TYPE_NFIELDS (TYPE_FN_FIELD_TYPE (f, overload_idx)),
			   spaces);
	  printfi_filtered (spaces + 8, "fcontext ");
	  gdb_print_host_address (TYPE_FN_FIELD_FCONTEXT (f, overload_idx),
				  gdb_stdout);
	  printf_filtered ("\n");

	  printfi_filtered (spaces + 8, "is_const %d\n",
			    TYPE_FN_FIELD_CONST (f, overload_idx));
	  printfi_filtered (spaces + 8, "is_volatile %d\n",
			    TYPE_FN_FIELD_VOLATILE (f, overload_idx));
	  printfi_filtered (spaces + 8, "is_private %d\n",
			    TYPE_FN_FIELD_PRIVATE (f, overload_idx));
	  printfi_filtered (spaces + 8, "is_protected %d\n",
			    TYPE_FN_FIELD_PROTECTED (f, overload_idx));
	  printfi_filtered (spaces + 8, "is_stub %d\n",
			    TYPE_FN_FIELD_STUB (f, overload_idx));
	  printfi_filtered (spaces + 8, "voffset %u\n",
			    TYPE_FN_FIELD_VOFFSET (f, overload_idx));
	}
    }
}

static void
print_cplus_stuff (struct type *type, int spaces)
{
  printfi_filtered (spaces, "n_baseclasses %d\n",
		    TYPE_N_BASECLASSES (type));
  printfi_filtered (spaces, "nfn_fields %d\n",
		    TYPE_NFN_FIELDS (type));
  printfi_filtered (spaces, "nfn_fields_total %d\n",
		    TYPE_NFN_FIELDS_TOTAL (type));
  if (TYPE_N_BASECLASSES (type) > 0)
    {
      printfi_filtered (spaces, "virtual_field_bits (%d bits at *",
			TYPE_N_BASECLASSES (type));
      gdb_print_host_address (TYPE_FIELD_VIRTUAL_BITS (type), gdb_stdout);
      printf_filtered (")");

      print_bit_vector (TYPE_FIELD_VIRTUAL_BITS (type),
			TYPE_N_BASECLASSES (type));
      puts_filtered ("\n");
    }
  if (TYPE_NFIELDS (type) > 0)
    {
      if (TYPE_FIELD_PRIVATE_BITS (type) != NULL)
	{
	  printfi_filtered (spaces, "private_field_bits (%d bits at *",
			    TYPE_NFIELDS (type));
	  gdb_print_host_address (TYPE_FIELD_PRIVATE_BITS (type), gdb_stdout);
	  printf_filtered (")");
	  print_bit_vector (TYPE_FIELD_PRIVATE_BITS (type),
			    TYPE_NFIELDS (type));
	  puts_filtered ("\n");
	}
      if (TYPE_FIELD_PROTECTED_BITS (type) != NULL)
	{
	  printfi_filtered (spaces, "protected_field_bits (%d bits at *",
			    TYPE_NFIELDS (type));
	  gdb_print_host_address (TYPE_FIELD_PROTECTED_BITS (type), gdb_stdout);
	  printf_filtered (")");
	  print_bit_vector (TYPE_FIELD_PROTECTED_BITS (type),
			    TYPE_NFIELDS (type));
	  puts_filtered ("\n");
	}
    }
  if (TYPE_NFN_FIELDS (type) > 0)
    {
      dump_fn_fieldlists (type, spaces);
    }
}

static void
print_bound_type (int bt)
{
  switch (bt)
    {
    case BOUND_CANNOT_BE_DETERMINED:
      printf_filtered ("(BOUND_CANNOT_BE_DETERMINED)");
      break;
    case BOUND_BY_REF_ON_STACK:
      printf_filtered ("(BOUND_BY_REF_ON_STACK)");
      break;
    case BOUND_BY_VALUE_ON_STACK:
      printf_filtered ("(BOUND_BY_VALUE_ON_STACK)");
      break;
    case BOUND_BY_REF_IN_REG:
      printf_filtered ("(BOUND_BY_REF_IN_REG)");
      break;
    case BOUND_BY_VALUE_IN_REG:
      printf_filtered ("(BOUND_BY_VALUE_IN_REG)");
      break;
    case BOUND_SIMPLE:
      printf_filtered ("(BOUND_SIMPLE)");
      break;
    default:
      printf_filtered (_("(unknown bound type)"));
      break;
    }
}

static struct obstack dont_print_type_obstack;

const char *type_code_name (int code)
{
  switch (code)
    {
    case TYPE_CODE_UNDEF:
      return "TYPE_CODE_UNDEF";
    case TYPE_CODE_PTR:
      return "TYPE_CODE_PTR";
    case TYPE_CODE_ARRAY:
      return "TYPE_CODE_ARRAY";
    case TYPE_CODE_STRUCT:
      return "TYPE_CODE_STRUCT";
    case TYPE_CODE_UNION:
      return "TYPE_CODE_UNION";
    case TYPE_CODE_ENUM:
      return "TYPE_CODE_ENUM";
    case TYPE_CODE_FUNC:
      return "TYPE_CODE_FUNC";
    case TYPE_CODE_INT:
      return "TYPE_CODE_INT";
    case TYPE_CODE_FLT:
      return "TYPE_CODE_FLT";
    case TYPE_CODE_VOID:
      return "TYPE_CODE_VOID";
    case TYPE_CODE_SET:
      return "TYPE_CODE_SET";
    case TYPE_CODE_RANGE:
      return "TYPE_CODE_RANGE";
    case TYPE_CODE_STRING:
      return "TYPE_CODE_STRING";
    case TYPE_CODE_BITSTRING:
      return "TYPE_CODE_BITSTRING";
    case TYPE_CODE_ERROR:
      return "TYPE_CODE_ERROR";
    case TYPE_CODE_MEMBER:
      return "TYPE_CODE_MEMBER";
    case TYPE_CODE_METHOD:
      return "TYPE_CODE_METHOD";
    case TYPE_CODE_REF:
      return "TYPE_CODE_REF";
    case TYPE_CODE_CHAR:
      return "TYPE_CODE_CHAR";
    case TYPE_CODE_BOOL:
      return "TYPE_CODE_BOOL";
    case TYPE_CODE_COMPLEX:
      return "TYPE_CODE_COMPLEX";
    case TYPE_CODE_TYPEDEF:
      return "TYPE_CODE_TYPEDEF";
    case TYPE_CODE_TEMPLATE:
      return "TYPE_CODE_TEMPLATE";
    case TYPE_CODE_TEMPLATE_ARG:
      return "TYPE_CODE_TEMPLATE_ARG";
    case TYPE_CODE_NAMESPACE:
      return "TYPE_CODE_NAMESPACE";
    default:
      return "UNKNOWN TYPE CODE";
    }
}


void
recursive_dump_type (struct type *type, int spaces)
{
  int idx;

  if (spaces == 0)
    obstack_begin (&dont_print_type_obstack, 0);

  if (TYPE_NFIELDS (type) > 0
      || (TYPE_CPLUS_SPECIFIC (type) && TYPE_NFN_FIELDS (type) > 0))
    {
      struct type **first_dont_print
      = (struct type **) obstack_base (&dont_print_type_obstack);

      int i = (struct type **) obstack_next_free (&dont_print_type_obstack)
      - first_dont_print;

      while (--i >= 0)
	{
	  if (type == first_dont_print[i])
	    {
	      printfi_filtered (spaces, "type node ");
	      gdb_print_host_address (type, gdb_stdout);
	      printf_filtered (_(" <same as already seen type>\n"));
	      return;
	    }
	}

      obstack_ptr_grow (&dont_print_type_obstack, type);
    }

  printfi_filtered (spaces, "type node ");
  gdb_print_host_address (type, gdb_stdout);
  printf_filtered ("\n");
  printfi_filtered (spaces, "name '%s' (",
		    TYPE_NAME (type) ? TYPE_NAME (type) : "<NULL>");
  gdb_print_host_address (TYPE_NAME (type), gdb_stdout);
  printf_filtered (")\n");
  printfi_filtered (spaces, "tagname '%s' (",
		    TYPE_TAG_NAME (type) ? TYPE_TAG_NAME (type) : "<NULL>");
  gdb_print_host_address (TYPE_TAG_NAME (type), gdb_stdout);
  printf_filtered (")\n");
  printfi_filtered (spaces, "code 0x%x (%s)", TYPE_CODE (type),
		    type_code_name (TYPE_CODE (type)));
  puts_filtered ("\n");
  printfi_filtered (spaces, "length %d\n", TYPE_LENGTH (type));
  printfi_filtered (spaces, "upper_bound_type 0x%x ",
		    TYPE_ARRAY_UPPER_BOUND_TYPE (type));
  print_bound_type (TYPE_ARRAY_UPPER_BOUND_TYPE (type));
  puts_filtered ("\n");
  printfi_filtered (spaces, "lower_bound_type 0x%x ",
		    TYPE_ARRAY_LOWER_BOUND_TYPE (type));
  print_bound_type (TYPE_ARRAY_LOWER_BOUND_TYPE (type));
  puts_filtered ("\n");
  printfi_filtered (spaces, "objfile ");
  gdb_print_host_address (TYPE_OBJFILE (type), gdb_stdout);
  printf_filtered ("\n");
  printfi_filtered (spaces, "target_type ");
  gdb_print_host_address (TYPE_TARGET_TYPE (type), gdb_stdout);
  printf_filtered ("\n");
  if (TYPE_TARGET_TYPE (type) != NULL)
    {
      recursive_dump_type (TYPE_TARGET_TYPE (type), spaces + 2);
    }
  printfi_filtered (spaces, "pointer_type ");
  gdb_print_host_address (TYPE_POINTER_TYPE (type), gdb_stdout);
  printf_filtered ("\n");
  printfi_filtered (spaces, "reference_type ");
  gdb_print_host_address (TYPE_REFERENCE_TYPE (type), gdb_stdout);
  printf_filtered ("\n");
  printfi_filtered (spaces, "type_chain ");
  gdb_print_host_address (TYPE_CHAIN (type), gdb_stdout);
  printf_filtered ("\n");
  printfi_filtered (spaces, "instance_flags 0x%x", TYPE_INSTANCE_FLAGS (type));
  if (TYPE_CONST (type))
    {
      puts_filtered (" TYPE_FLAG_CONST");
    }
  if (TYPE_VOLATILE (type))
    {
      puts_filtered (" TYPE_FLAG_VOLATILE");
    }
  if (TYPE_CODE_SPACE (type))
    {
      puts_filtered (" TYPE_FLAG_CODE_SPACE");
    }
  if (TYPE_DATA_SPACE (type))
    {
      puts_filtered (" TYPE_FLAG_DATA_SPACE");
    }
  if (TYPE_ADDRESS_CLASS_1 (type))
    {
      puts_filtered (" TYPE_FLAG_ADDRESS_CLASS_1");
    }
  if (TYPE_ADDRESS_CLASS_2 (type))
    {
      puts_filtered (" TYPE_FLAG_ADDRESS_CLASS_2");
    }
  puts_filtered ("\n");
  printfi_filtered (spaces, "flags 0x%x", TYPE_FLAGS (type));
  if (TYPE_UNSIGNED (type))
    {
      puts_filtered (" TYPE_FLAG_UNSIGNED");
    }
  if (TYPE_NOSIGN (type))
    {
      puts_filtered (" TYPE_FLAG_NOSIGN");
    }
  if (TYPE_STUB (type))
    {
      puts_filtered (" TYPE_FLAG_STUB");
    }
  if (TYPE_TARGET_STUB (type))
    {
      puts_filtered (" TYPE_FLAG_TARGET_STUB");
    }
  if (TYPE_STATIC (type))
    {
      puts_filtered (" TYPE_FLAG_STATIC");
    }
  if (TYPE_PROTOTYPED (type))
    {
      puts_filtered (" TYPE_FLAG_PROTOTYPED");
    }
  if (TYPE_INCOMPLETE (type))
    {
      puts_filtered (" TYPE_FLAG_INCOMPLETE");
    }
  if (TYPE_VARARGS (type))
    {
      puts_filtered (" TYPE_FLAG_VARARGS");
    }
  /* This is used for things like AltiVec registers on ppc.  Gcc emits
     an attribute for the array type, which tells whether or not we
     have a vector, instead of a regular array.  */
  if (TYPE_VECTOR (type))
    {
      puts_filtered (" TYPE_FLAG_VECTOR");
    }
  puts_filtered ("\n");
  printfi_filtered (spaces, "nfields %d ", TYPE_NFIELDS (type));
  gdb_print_host_address (TYPE_FIELDS (type), gdb_stdout);
  puts_filtered ("\n");
  for (idx = 0; idx < TYPE_NFIELDS (type); idx++)
    {
      printfi_filtered (spaces + 2,
			"[%d] bitpos %d bitsize %d type ",
			idx, TYPE_FIELD_BITPOS (type, idx),
			TYPE_FIELD_BITSIZE (type, idx));
      gdb_print_host_address (TYPE_FIELD_TYPE (type, idx), gdb_stdout);
      printf_filtered (" name '%s' (",
		       TYPE_FIELD_NAME (type, idx) != NULL
		       ? TYPE_FIELD_NAME (type, idx)
		       : "<NULL>");
      gdb_print_host_address (TYPE_FIELD_NAME (type, idx), gdb_stdout);
      printf_filtered (")\n");
      if (TYPE_FIELD_TYPE (type, idx) != NULL)
	{
	  recursive_dump_type (TYPE_FIELD_TYPE (type, idx), spaces + 4);
	}
    }
  printfi_filtered (spaces, "vptr_basetype ");
  gdb_print_host_address (TYPE_VPTR_BASETYPE (type), gdb_stdout);
  puts_filtered ("\n");
  if (TYPE_VPTR_BASETYPE (type) != NULL)
    {
      recursive_dump_type (TYPE_VPTR_BASETYPE (type), spaces + 2);
    }
  printfi_filtered (spaces, "vptr_fieldno %d\n", TYPE_VPTR_FIELDNO (type));
  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_STRUCT:
      printfi_filtered (spaces, "cplus_stuff ");
      gdb_print_host_address (TYPE_CPLUS_SPECIFIC (type), gdb_stdout);
      puts_filtered ("\n");
      print_cplus_stuff (type, spaces);
      break;

    case TYPE_CODE_FLT:
      printfi_filtered (spaces, "floatformat ");
      if (TYPE_FLOATFORMAT (type) == NULL
	  || TYPE_FLOATFORMAT (type)->name == NULL)
	puts_filtered ("(null)");
      else
	puts_filtered (TYPE_FLOATFORMAT (type)->name);
      puts_filtered ("\n");
      break;

    default:
      /* We have to pick one of the union types to be able print and test
         the value.  Pick cplus_struct_type, even though we know it isn't
         any particular one. */
      printfi_filtered (spaces, "type_specific ");
      gdb_print_host_address (TYPE_CPLUS_SPECIFIC (type), gdb_stdout);
      if (TYPE_CPLUS_SPECIFIC (type) != NULL)
	{
	  printf_filtered (_(" (unknown data form)"));
	}
      printf_filtered ("\n");
      break;

    }
  if (spaces == 0)
    obstack_free (&dont_print_type_obstack, NULL);
}

static void build_gdbtypes (void);
static void
build_gdbtypes (void)
{
  builtin_type_void =
    init_type (TYPE_CODE_VOID, 1,
	       0,
	       "void", (struct objfile *) NULL);
  builtin_type_char =
    init_type (TYPE_CODE_INT, TARGET_CHAR_BIT / TARGET_CHAR_BIT,
	       (TYPE_FLAG_NOSIGN
                | (TARGET_CHAR_SIGNED ? 0 : TYPE_FLAG_UNSIGNED)),
	       "char", (struct objfile *) NULL);
  builtin_type_true_char =
    init_type (TYPE_CODE_CHAR, TARGET_CHAR_BIT / TARGET_CHAR_BIT,
	       0,
	       "true character", (struct objfile *) NULL);
  builtin_type_signed_char =
    init_type (TYPE_CODE_INT, TARGET_CHAR_BIT / TARGET_CHAR_BIT,
	       0,
	       "signed char", (struct objfile *) NULL);
  builtin_type_unsigned_char =
    init_type (TYPE_CODE_INT, TARGET_CHAR_BIT / TARGET_CHAR_BIT,
	       TYPE_FLAG_UNSIGNED,
	       "unsigned char", (struct objfile *) NULL);
  builtin_type_short =
    init_type (TYPE_CODE_INT, TARGET_SHORT_BIT / TARGET_CHAR_BIT,
	       0,
	       "short", (struct objfile *) NULL);
  builtin_type_unsigned_short =
    init_type (TYPE_CODE_INT, TARGET_SHORT_BIT / TARGET_CHAR_BIT,
	       TYPE_FLAG_UNSIGNED,
	       "unsigned short", (struct objfile *) NULL);
  builtin_type_int =
    init_type (TYPE_CODE_INT, TARGET_INT_BIT / TARGET_CHAR_BIT,
	       0,
	       "int", (struct objfile *) NULL);
  builtin_type_unsigned_int =
    init_type (TYPE_CODE_INT, TARGET_INT_BIT / TARGET_CHAR_BIT,
	       TYPE_FLAG_UNSIGNED,
	       "unsigned int", (struct objfile *) NULL);
  builtin_type_long =
    init_type (TYPE_CODE_INT, TARGET_LONG_BIT / TARGET_CHAR_BIT,
	       0,
	       "long", (struct objfile *) NULL);
  builtin_type_unsigned_long =
    init_type (TYPE_CODE_INT, TARGET_LONG_BIT / TARGET_CHAR_BIT,
	       TYPE_FLAG_UNSIGNED,
	       "unsigned long", (struct objfile *) NULL);
  builtin_type_long_long =
    init_type (TYPE_CODE_INT, TARGET_LONG_LONG_BIT / TARGET_CHAR_BIT,
	       0,
	       "long long", (struct objfile *) NULL);
  builtin_type_unsigned_long_long =
    init_type (TYPE_CODE_INT, TARGET_LONG_LONG_BIT / TARGET_CHAR_BIT,
	       TYPE_FLAG_UNSIGNED,
	       "unsigned long long", (struct objfile *) NULL);
  builtin_type_float =
    init_type (TYPE_CODE_FLT, TARGET_FLOAT_BIT / TARGET_CHAR_BIT,
	       0,
	       "float", (struct objfile *) NULL);
/* vinschen@redhat.com 2002-02-08:
   The below lines are disabled since they are doing the wrong
   thing for non-multiarch targets.  They are setting the correct
   type of floats for the target but while on multiarch targets
   this is done everytime the architecture changes, it's done on
   non-multiarch targets only on startup, leaving the wrong values
   in even if the architecture changes (eg. from big-endian to
   little-endian).  */
#if 0
  TYPE_FLOATFORMAT (builtin_type_float) = TARGET_FLOAT_FORMAT;
#endif
  builtin_type_double =
    init_type (TYPE_CODE_FLT, TARGET_DOUBLE_BIT / TARGET_CHAR_BIT,
	       0,
	       "double", (struct objfile *) NULL);
#if 0
  TYPE_FLOATFORMAT (builtin_type_double) = TARGET_DOUBLE_FORMAT;
#endif
  builtin_type_long_double =
    init_type (TYPE_CODE_FLT, TARGET_LONG_DOUBLE_BIT / TARGET_CHAR_BIT,
	       0,
	       "long double", (struct objfile *) NULL);
#if 0
  TYPE_FLOATFORMAT (builtin_type_long_double) = TARGET_LONG_DOUBLE_FORMAT;
#endif
  builtin_type_complex =
    init_type (TYPE_CODE_COMPLEX, 2 * TARGET_FLOAT_BIT / TARGET_CHAR_BIT,
	       0,
	       "complex", (struct objfile *) NULL);
  TYPE_TARGET_TYPE (builtin_type_complex) = builtin_type_float;
  builtin_type_double_complex =
    init_type (TYPE_CODE_COMPLEX, 2 * TARGET_DOUBLE_BIT / TARGET_CHAR_BIT,
	       0,
	       "double complex", (struct objfile *) NULL);
  TYPE_TARGET_TYPE (builtin_type_double_complex) = builtin_type_double;
  builtin_type_string =
    init_type (TYPE_CODE_STRING, TARGET_CHAR_BIT / TARGET_CHAR_BIT,
	       0,
	       "string", (struct objfile *) NULL);
  builtin_type_bool =
    init_type (TYPE_CODE_BOOL, TARGET_CHAR_BIT / TARGET_CHAR_BIT,
	       0,
	       "bool", (struct objfile *) NULL);

  /* Add user knob for controlling resolution of opaque types */
  add_setshow_boolean_cmd ("opaque-type-resolution", class_support,
			   &opaque_type_resolution, _("\
Set resolution of opaque struct/class/union types (if set before loading symbols)."), _("\
Show resolution of opaque struct/class/union types (if set before loading symbols)."), NULL,
			   NULL,
			   show_opaque_type_resolution,
			   &setlist, &showlist);
  opaque_type_resolution = 1;

  /* Build SIMD types.  */
  builtin_type_v4sf
    = init_simd_type ("__builtin_v4sf", builtin_type_float, "f", 4);
  builtin_type_v4si
    = init_simd_type ("__builtin_v4si", builtin_type_int32, "f", 4);
  builtin_type_v16qi
    = init_simd_type ("__builtin_v16qi", builtin_type_int8, "f", 16);
  builtin_type_v8qi
    = init_simd_type ("__builtin_v8qi", builtin_type_int8, "f", 8);
  builtin_type_v8hi
    = init_simd_type ("__builtin_v8hi", builtin_type_int16, "f", 8);
  builtin_type_v4hi
    = init_simd_type ("__builtin_v4hi", builtin_type_int16, "f", 4);
  builtin_type_v2si
    = init_simd_type ("__builtin_v2si", builtin_type_int32, "f", 2);

  /* 128 bit vectors.  */
  builtin_type_v2_double = init_vector_type (builtin_type_double, 2);
  builtin_type_v4_float = init_vector_type (builtin_type_float, 4);
  builtin_type_v2_int64 = init_vector_type (builtin_type_int64, 2);
  builtin_type_v4_int32 = init_vector_type (builtin_type_int32, 4);
  builtin_type_v8_int16 = init_vector_type (builtin_type_int16, 8);
  builtin_type_v16_int8 = init_vector_type (builtin_type_int8, 16);
  /* 64 bit vectors.  */
  builtin_type_v2_float = init_vector_type (builtin_type_float, 2);
  builtin_type_v2_int32 = init_vector_type (builtin_type_int32, 2);
  builtin_type_v4_int16 = init_vector_type (builtin_type_int16, 4);
  builtin_type_v8_int8 = init_vector_type (builtin_type_int8, 8);

  /* Vector types.  */
  builtin_type_vec64 = build_builtin_type_vec64 ();
  builtin_type_vec128 = build_builtin_type_vec128 ();

  /* Pointer/Address types. */

  /* NOTE: on some targets, addresses and pointers are not necessarily
     the same --- for example, on the D10V, pointers are 16 bits long,
     but addresses are 32 bits long.  See doc/gdbint.texinfo,
     ``Pointers Are Not Always Addresses''.

     The upshot is:
     - gdb's `struct type' always describes the target's
       representation.
     - gdb's `struct value' objects should always hold values in
       target form.
     - gdb's CORE_ADDR values are addresses in the unified virtual
       address space that the assembler and linker work with.  Thus,
       since target_read_memory takes a CORE_ADDR as an argument, it
       can access any memory on the target, even if the processor has
       separate code and data address spaces.

     So, for example:
     - If v is a value holding a D10V code pointer, its contents are
       in target form: a big-endian address left-shifted two bits.
     - If p is a D10V pointer type, TYPE_LENGTH (p) == 2, just as
       sizeof (void *) == 2 on the target.

     In this context, builtin_type_CORE_ADDR is a bit odd: it's a
     target type for a value the target will never see.  It's only
     used to hold the values of (typeless) linker symbols, which are
     indeed in the unified virtual address space.  */
  builtin_type_void_data_ptr = make_pointer_type (builtin_type_void, NULL);
  builtin_type_void_func_ptr
    = lookup_pointer_type (lookup_function_type (builtin_type_void));
  builtin_type_CORE_ADDR =
    init_type (TYPE_CODE_INT, TARGET_ADDR_BIT / 8,
	       TYPE_FLAG_UNSIGNED,
	       "__CORE_ADDR", (struct objfile *) NULL);
  builtin_type_bfd_vma =
    init_type (TYPE_CODE_INT, TARGET_BFD_VMA_BIT / 8,
	       TYPE_FLAG_UNSIGNED,
	       "__bfd_vma", (struct objfile *) NULL);
  builtin_type_voidptrfuncptr =
    lookup_pointer_type (lookup_function_type (lookup_pointer_type (builtin_type_void)));
}

static struct gdbarch_data *gdbtypes_data;

const struct builtin_type *
builtin_type (struct gdbarch *gdbarch)
{
  return gdbarch_data (gdbarch, gdbtypes_data);
}


static struct type *
build_flt (int bit, char *name, const struct floatformat *floatformat)
{
  struct type *t;
  if (bit <= 0 || floatformat == NULL)
    {
      gdb_assert (builtin_type_error != NULL);
      return builtin_type_error;
    }
  t = init_type (TYPE_CODE_FLT, bit / TARGET_CHAR_BIT,
		 0, name, (struct objfile *) NULL);
  TYPE_FLOATFORMAT (t) = floatformat;
  return t;
}

static struct type *
build_complex (int bit, char *name, struct type *target_type)
{
  struct type *t;
  if (bit <= 0 || target_type == builtin_type_error)
    {
      gdb_assert (builtin_type_error != NULL);
      return builtin_type_error;
    }
  t = init_type (TYPE_CODE_COMPLEX, 2 * bit / TARGET_CHAR_BIT,
		 0, name, (struct objfile *) NULL);
  TYPE_TARGET_TYPE (t) = target_type;
  return t;
}

static void *
gdbtypes_post_init (struct gdbarch *gdbarch)
{
  struct builtin_type *builtin_type
    = GDBARCH_OBSTACK_ZALLOC (gdbarch, struct builtin_type);

  builtin_type->builtin_void =
    init_type (TYPE_CODE_VOID, 1,
	       0,
	       "void", (struct objfile *) NULL);
  builtin_type->builtin_char =
    init_type (TYPE_CODE_INT, TARGET_CHAR_BIT / TARGET_CHAR_BIT,
	       (TYPE_FLAG_NOSIGN
                | (TARGET_CHAR_SIGNED ? 0 : TYPE_FLAG_UNSIGNED)),
	       "char", (struct objfile *) NULL);
  builtin_type->builtin_true_char =
    init_type (TYPE_CODE_CHAR, TARGET_CHAR_BIT / TARGET_CHAR_BIT,
	       0,
	       "true character", (struct objfile *) NULL);
  builtin_type->builtin_signed_char =
    init_type (TYPE_CODE_INT, TARGET_CHAR_BIT / TARGET_CHAR_BIT,
	       0,
	       "signed char", (struct objfile *) NULL);
  builtin_type->builtin_unsigned_char =
    init_type (TYPE_CODE_INT, TARGET_CHAR_BIT / TARGET_CHAR_BIT,
	       TYPE_FLAG_UNSIGNED,
	       "unsigned char", (struct objfile *) NULL);
  builtin_type->builtin_short =
    init_type (TYPE_CODE_INT, TARGET_SHORT_BIT / TARGET_CHAR_BIT,
	       0,
	       "short", (struct objfile *) NULL);
  builtin_type->builtin_unsigned_short =
    init_type (TYPE_CODE_INT, TARGET_SHORT_BIT / TARGET_CHAR_BIT,
	       TYPE_FLAG_UNSIGNED,
	       "unsigned short", (struct objfile *) NULL);
  builtin_type->builtin_int =
    init_type (TYPE_CODE_INT, TARGET_INT_BIT / TARGET_CHAR_BIT,
	       0,
	       "int", (struct objfile *) NULL);
  builtin_type->builtin_unsigned_int =
    init_type (TYPE_CODE_INT, TARGET_INT_BIT / TARGET_CHAR_BIT,
	       TYPE_FLAG_UNSIGNED,
	       "unsigned int", (struct objfile *) NULL);
  builtin_type->builtin_long =
    init_type (TYPE_CODE_INT, TARGET_LONG_BIT / TARGET_CHAR_BIT,
	       0,
	       "long", (struct objfile *) NULL);
  builtin_type->builtin_unsigned_long =
    init_type (TYPE_CODE_INT, TARGET_LONG_BIT / TARGET_CHAR_BIT,
	       TYPE_FLAG_UNSIGNED,
	       "unsigned long", (struct objfile *) NULL);
  builtin_type->builtin_long_long =
    init_type (TYPE_CODE_INT, TARGET_LONG_LONG_BIT / TARGET_CHAR_BIT,
	       0,
	       "long long", (struct objfile *) NULL);
  builtin_type->builtin_unsigned_long_long =
    init_type (TYPE_CODE_INT, TARGET_LONG_LONG_BIT / TARGET_CHAR_BIT,
	       TYPE_FLAG_UNSIGNED,
	       "unsigned long long", (struct objfile *) NULL);
  builtin_type->builtin_float
    = build_flt (gdbarch_float_bit (gdbarch), "float",
		 gdbarch_float_format (gdbarch));
  builtin_type->builtin_double
    = build_flt (gdbarch_double_bit (gdbarch), "double",
		 gdbarch_double_format (gdbarch));
  builtin_type->builtin_long_double
    = build_flt (gdbarch_long_double_bit (gdbarch), "long double",
		 gdbarch_long_double_format (gdbarch));
  builtin_type->builtin_complex
    = build_complex (gdbarch_float_bit (gdbarch), "complex",
		     builtin_type->builtin_float);
  builtin_type->builtin_double_complex
    = build_complex (gdbarch_double_bit (gdbarch), "double complex",
		     builtin_type->builtin_double);
  builtin_type->builtin_string =
    init_type (TYPE_CODE_STRING, TARGET_CHAR_BIT / TARGET_CHAR_BIT,
	       0,
	       "string", (struct objfile *) NULL);
  builtin_type->builtin_bool =
    init_type (TYPE_CODE_BOOL, TARGET_CHAR_BIT / TARGET_CHAR_BIT,
	       0,
	       "bool", (struct objfile *) NULL);

  /* Pointer/Address types. */

  /* NOTE: on some targets, addresses and pointers are not necessarily
     the same --- for example, on the D10V, pointers are 16 bits long,
     but addresses are 32 bits long.  See doc/gdbint.texinfo,
     ``Pointers Are Not Always Addresses''.

     The upshot is:
     - gdb's `struct type' always describes the target's
       representation.
     - gdb's `struct value' objects should always hold values in
       target form.
     - gdb's CORE_ADDR values are addresses in the unified virtual
       address space that the assembler and linker work with.  Thus,
       since target_read_memory takes a CORE_ADDR as an argument, it
       can access any memory on the target, even if the processor has
       separate code and data address spaces.

     So, for example:
     - If v is a value holding a D10V code pointer, its contents are
       in target form: a big-endian address left-shifted two bits.
     - If p is a D10V pointer type, TYPE_LENGTH (p) == 2, just as
       sizeof (void *) == 2 on the target.

     In this context, builtin_type->CORE_ADDR is a bit odd: it's a
     target type for a value the target will never see.  It's only
     used to hold the values of (typeless) linker symbols, which are
     indeed in the unified virtual address space.  */
  builtin_type->builtin_data_ptr
    = make_pointer_type (builtin_type->builtin_void, NULL);
  builtin_type->builtin_func_ptr
    = lookup_pointer_type (lookup_function_type (builtin_type->builtin_void));
  builtin_type->builtin_core_addr =
    init_type (TYPE_CODE_INT, TARGET_ADDR_BIT / 8,
	       TYPE_FLAG_UNSIGNED,
	       "__CORE_ADDR", (struct objfile *) NULL);

  return builtin_type;
}


/* APPLE LOCAL BEGIN: Helper functions for easily building bitfield
   built in types. 

   Build a builtin enum type named NAME that represents a single integer
   bitfield that is SIZE bytes long. FLAGS from the TYPE_FLAG_xxx definitions
   can be OR'ed together to modify the enumeration. This can be used to
   create internal enumeration types for use with the build_builtin_bitfield ()
   function. NAME is the name of the built in type and should start with 
   "__gdb_builtin_type" followed by the type name. ENUMS is an array of 
   NUM_ENUMS gdbtypes_enum_info structures that describe each enumeration. 
   This function will copy NAME and gdbtypes_enum_info.name strings -- creating
   a potential memory leak -- but since builtin types need to permanently be
   around and are typically created in the initialization functions, the leak 
   should be small and contained.
   
   Sample code:
    struct type *enum_type;
    static struct gdbtypes_enum_info enums[] = {
      {"one",	1 },
      {"two",	2 },
      {"three",	3 },
      {"six",	6 }
    };
    uint32_t num_enums = sizeof (enums)/sizeof (enums[0]);
    enum_type build_builtin_enum ("__gdb_builtin_type_small_numbers", 4, 
				  TYPE_FLAG_UNSIGNED, enums, num_enums);
   */
struct type *
build_builtin_enum (const char *name, uint32_t size, int flags, 
		    struct gdbtypes_enum_info *enums, uint32_t num_enums)
{
  int i;
  struct type *t = init_type (TYPE_CODE_ENUM, size, flags, xstrdup (name), 
			      (struct objfile *) NULL);
  TYPE_NFIELDS (t) = num_enums;
  const int fields_size = sizeof (struct field) * TYPE_NFIELDS (t);
  TYPE_FIELDS (t) = xmalloc (fields_size);
  memset (TYPE_FIELDS (t), 0, fields_size);
  for (i = 0; i < num_enums; i++)
    {
      TYPE_FIELD_NAME (t, i) = xstrdup (enums[i].name);
      TYPE_FIELD_BITPOS (t, i) = enums[i].value;  
    }
  gdb_assert (i == TYPE_NFIELDS (t));
  TYPE_LENGTH (t) = size;
  return t;
}

/* Helper function that builds a builtin bitfield type named NAME that  
   represents a single integer bitfield that is SIZE bytes long. This can 
   be used as a register's type to display a register's contents as a 
   structure containing bitfields. NAME is the name of the built in type 
   and should start with "__gdb_builtin_type" followed by the type name. 
   BITFIELDS is an array of NUM_BITFIELDS gdbtypes_bitfield_info structures 
   that describe each bitfield in the type. 
   This function will copy NAME and gdbtypes_bitfield_info.name strings -- 
   creating a potential memory leak -- but since builtin types need to 
   permanently be around and are typically created in the initialization 
   functions, the leak should be small and contained.
   Sample code:
  
    type * bitfield_type;
    struct gdbtypes_bitfield_info bitfields[] = {
      {"bit31",  builtin_type_uint32,  31, 31 },
      {"num",   enum_type,             15,  8 }, 
      {"nibble", builtin_type_uint32,   3,  0 }
    };
    uint32_t num_bitfields = sizeof (bitfields)/sizeof (bitfields[0]);
    bitfield_type = build_builtin_bitfield ("__gdb_builtin_type_test", 4, 
					   bitfields, num_bitfields);
*/
struct type *
build_builtin_bitfield (const char *name, uint32_t size, 
			struct gdbtypes_bitfield_info *bitfields, 
			uint32_t num_bitfields)
{
  struct type *t;
  t = init_composite_type (xstrdup (name), TYPE_CODE_STRUCT);
  TYPE_NAME (t) = name;
  TYPE_NFIELDS (t) = num_bitfields;
  const int fields_size = sizeof (struct field) * TYPE_NFIELDS (t);
  TYPE_FIELDS (t) = xmalloc (fields_size);
  memset (TYPE_FIELDS (t), 0, fields_size);
  int i = 0;

  for (i = 0; i < num_bitfields; i++)
    {
      TYPE_FIELD_NAME (t, i) = xstrdup (bitfields[i].name);
      TYPE_FIELD_TYPE (t, i) = bitfields[i].type;
      TYPE_FIELD_BITSIZE (t, i) = bitfields[i].msbit - bitfields[i].lsbit + 1;
      TYPE_FIELD_BITPOS (t, i) = bitfields[i].lsbit;  
    }
  gdb_assert (i == TYPE_NFIELDS (t));
  TYPE_LENGTH (t) = size;
  return t;
}

/* APPLE LOCAL: 
   This returns the "dynamic type" of the generic Apple "Block"
   structure.  Unlike the rtti get dynamic type functions, this returns
   the dynamic type which is of the same type as the type passed in, so
   if we get a TYPE_CODE_STRUCT we will return a TYPE_CODE_STRUCT for the
   dynamic type, if we get a TYPE_CODE_PTR, we will return a pointer.

   FIXME, at this point, we should have some extensible runtime
   module that provides dynamic type callouts.  But for now just add 'em by 
   hand.  */

struct type *
get_closure_dynamic_type (struct value *in_value)
{
  struct type *deref;
  struct type *dynamic_type = NULL;
  struct type *base_type = check_typedef (value_type (in_value));

  if (TYPE_CODE (base_type) == TYPE_CODE_PTR
      || TYPE_CODE (base_type) == TYPE_CODE_REF)
    deref = TYPE_TARGET_TYPE (base_type);
  else if (TYPE_CODE (base_type) == TYPE_CODE_STRUCT)
    deref = base_type;
  else
    return NULL;

  if (deref
      && ((TYPE_FLAGS (deref) & TYPE_FLAG_APPLE_CLOSURE) != 0))
    {
      /* This is the Apple Closure.  It has a field "FuncPtr" whose
	 value is the implementation function for this closure.  And the
	 first argument of that function is has the real type for this
	 block pointer structure.  */
      struct value *funcPtr_val;
      struct gdb_exception e;

      TRY_CATCH (e, RETURN_MASK_ALL)
	{
	  funcPtr_val = value_struct_elt (&in_value, NULL, "FuncPtr", NULL, "");
	  
	  if (funcPtr_val != NULL)
	    {
	      CORE_ADDR func_addr;
	      struct symbol *func_sym;
	      func_addr = value_as_address (funcPtr_val);
	      func_sym = find_pc_function (func_addr);
	      if (func_sym != NULL)
		{
		  /* Look for the field called "_self".  That's
		     the type pointer for the real type.  */
		  struct type *func_type;
		  func_type = SYMBOL_TYPE (func_sym);
		  if (TYPE_NFIELDS (func_type) > 0)
		    {
		      dynamic_type = TYPE_FIELD_TYPE (func_type, 0);
		      if (TYPE_CODE (dynamic_type) != TYPE_CODE_PTR)
			dynamic_type = NULL;
		      else
			{
			  if (TYPE_CODE (base_type) == TYPE_CODE_STRUCT)
			    dynamic_type = TYPE_TARGET_TYPE (dynamic_type);
			  else if (TYPE_CODE (base_type) == TYPE_CODE_REF)
			    {
			      dynamic_type = make_reference_type (dynamic_type, NULL);
			    }
			}
		    }
		}
	    }
	}
    }

  return dynamic_type;
}
/* END APPLE LOCAL  */


extern void _initialize_gdbtypes (void);
void
_initialize_gdbtypes (void)
{
  builtin_type_int0 =
    init_type (TYPE_CODE_INT, 0 / 8,
	       0,
	       "int0_t", (struct objfile *) NULL);
  builtin_type_int8 =
    init_type (TYPE_CODE_INT, 8 / 8,
	       0,
	       "int8_t", (struct objfile *) NULL);
  builtin_type_uint8 =
    init_type (TYPE_CODE_INT, 8 / 8,
	       TYPE_FLAG_UNSIGNED,
	       "uint8_t", (struct objfile *) NULL);
  builtin_type_int16 =
    init_type (TYPE_CODE_INT, 16 / 8,
	       0,
	       "int16_t", (struct objfile *) NULL);
  builtin_type_uint16 =
    init_type (TYPE_CODE_INT, 16 / 8,
	       TYPE_FLAG_UNSIGNED,
	       "uint16_t", (struct objfile *) NULL);
  builtin_type_int32 =
    init_type (TYPE_CODE_INT, 32 / 8,
	       0,
	       "int32_t", (struct objfile *) NULL);
  builtin_type_uint32 =
    init_type (TYPE_CODE_INT, 32 / 8,
	       TYPE_FLAG_UNSIGNED,
	       "uint32_t", (struct objfile *) NULL);
  builtin_type_int64 =
    init_type (TYPE_CODE_INT, 64 / 8,
	       0,
	       "int64_t", (struct objfile *) NULL);
  builtin_type_uint64 =
    init_type (TYPE_CODE_INT, 64 / 8,
	       TYPE_FLAG_UNSIGNED,
	       "uint64_t", (struct objfile *) NULL);
  builtin_type_int128 =
    init_type (TYPE_CODE_INT, 128 / 8,
	       0,
	       "int128_t", (struct objfile *) NULL);
  builtin_type_uint128 =
    init_type (TYPE_CODE_INT, 128 / 8,
	       TYPE_FLAG_UNSIGNED,
	       "uint128_t", (struct objfile *) NULL);

  build_gdbtypes ();

  gdbtypes_data = gdbarch_data_register_post_init (gdbtypes_post_init);

  /* FIXME - For the moment, handle types by swapping them in and out.
     Should be using the per-architecture data-pointer and a large
     struct. */
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_void);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_char);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_short);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_int);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_long);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_long_long);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_signed_char);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_unsigned_char);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_unsigned_short);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_unsigned_int);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_unsigned_long);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_unsigned_long_long);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_float);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_double);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_long_double);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_complex);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_double_complex);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_string);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_v4sf);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_v4si);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_v16qi);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_v8qi);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_v8hi);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_v4hi);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_v2si);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_v2_double);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_v4_float);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_v2_int64);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_v4_int32);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_v8_int16);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_v16_int8);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_v2_float);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_v2_int32);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_v8_int8);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_v4_int16);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_vec128);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_void_data_ptr);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_void_func_ptr);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_CORE_ADDR);
  DEPRECATED_REGISTER_GDBARCH_SWAP (builtin_type_bfd_vma);
  deprecated_register_gdbarch_swap (NULL, 0, build_gdbtypes);

  /* Note: These types do not need to be swapped - they are target
     neutral.  */
  builtin_type_ieee_single_big =
    init_type (TYPE_CODE_FLT, floatformat_ieee_single_big.totalsize / 8,
	       0, "builtin_type_ieee_single_big", NULL);
  TYPE_FLOATFORMAT (builtin_type_ieee_single_big) = &floatformat_ieee_single_big;
  builtin_type_ieee_single_little =
    init_type (TYPE_CODE_FLT, floatformat_ieee_single_little.totalsize / 8,
	       0, "builtin_type_ieee_single_little", NULL);
  TYPE_FLOATFORMAT (builtin_type_ieee_single_little) = &floatformat_ieee_single_little;
  builtin_type_ieee_single[BFD_ENDIAN_BIG]
    = build_flt (floatformat_ieee_single_big.totalsize,
		 "builtin_type_ieee_single_big",
		 &floatformat_ieee_single_big);
  builtin_type_ieee_single[BFD_ENDIAN_LITTLE]
    = build_flt (floatformat_ieee_single_little.totalsize,
		 "builtin_type_ieee_single_little",
		 &floatformat_ieee_single_little);
  builtin_type_ieee_double_big =
    init_type (TYPE_CODE_FLT, floatformat_ieee_double_big.totalsize / 8,
	       0, "builtin_type_ieee_double_big", NULL);
  TYPE_FLOATFORMAT (builtin_type_ieee_double_big) = &floatformat_ieee_double_big;
  builtin_type_ieee_double_little =
    init_type (TYPE_CODE_FLT, floatformat_ieee_double_little.totalsize / 8,
	       0, "builtin_type_ieee_double_little", NULL);
  TYPE_FLOATFORMAT (builtin_type_ieee_double_little) = &floatformat_ieee_double_little;
  builtin_type_ieee_double[BFD_ENDIAN_BIG]
    = build_flt (floatformat_ieee_double_big.totalsize,
		 "builtin_type_ieee_double_big",
		 &floatformat_ieee_double_big);
  builtin_type_ieee_double[BFD_ENDIAN_LITTLE]
    = build_flt (floatformat_ieee_double_little.totalsize,
		 "builtin_type_ieee_double_little",
		 &floatformat_ieee_double_little);
  builtin_type_ieee_double_littlebyte_bigword =
    init_type (TYPE_CODE_FLT, floatformat_ieee_double_littlebyte_bigword.totalsize / 8,
	       0, "builtin_type_ieee_double_littlebyte_bigword", NULL);
  TYPE_FLOATFORMAT (builtin_type_ieee_double_littlebyte_bigword) = &floatformat_ieee_double_littlebyte_bigword;
  builtin_type_i387_ext =
    init_type (TYPE_CODE_FLT, floatformat_i387_ext.totalsize / 8,
	       0, "builtin_type_i387_ext", NULL);
  TYPE_FLOATFORMAT (builtin_type_i387_ext) = &floatformat_i387_ext;
  builtin_type_m68881_ext =
    init_type (TYPE_CODE_FLT, floatformat_m68881_ext.totalsize / 8,
	       0, "builtin_type_m68881_ext", NULL);
  TYPE_FLOATFORMAT (builtin_type_m68881_ext) = &floatformat_m68881_ext;
  builtin_type_i960_ext =
    init_type (TYPE_CODE_FLT, floatformat_i960_ext.totalsize / 8,
	       0, "builtin_type_i960_ext", NULL);
  TYPE_FLOATFORMAT (builtin_type_i960_ext) = &floatformat_i960_ext;
  builtin_type_m88110_ext =
    init_type (TYPE_CODE_FLT, floatformat_m88110_ext.totalsize / 8,
	       0, "builtin_type_m88110_ext", NULL);
  TYPE_FLOATFORMAT (builtin_type_m88110_ext) = &floatformat_m88110_ext;
  builtin_type_m88110_harris_ext =
    init_type (TYPE_CODE_FLT, floatformat_m88110_harris_ext.totalsize / 8,
	       0, "builtin_type_m88110_harris_ext", NULL);
  TYPE_FLOATFORMAT (builtin_type_m88110_harris_ext) = &floatformat_m88110_harris_ext;
  builtin_type_arm_ext_big =
    init_type (TYPE_CODE_FLT, floatformat_arm_ext_big.totalsize / 8,
	       0, "builtin_type_arm_ext_big", NULL);
  TYPE_FLOATFORMAT (builtin_type_arm_ext_big) = &floatformat_arm_ext_big;
  builtin_type_arm_ext_littlebyte_bigword =
    init_type (TYPE_CODE_FLT, floatformat_arm_ext_littlebyte_bigword.totalsize / 8,
	       0, "builtin_type_arm_ext_littlebyte_bigword", NULL);
  TYPE_FLOATFORMAT (builtin_type_arm_ext_littlebyte_bigword) = &floatformat_arm_ext_littlebyte_bigword;
  builtin_type_arm_ext[BFD_ENDIAN_BIG]
    = build_flt (floatformat_arm_ext_big.totalsize,
		 "builtin_type_arm_ext_big",
		 &floatformat_arm_ext_big);
  builtin_type_arm_ext[BFD_ENDIAN_LITTLE]
    = build_flt (floatformat_arm_ext_littlebyte_bigword.totalsize,
		 "builtin_type_arm_ext_littlebyte_bigword",
		 &floatformat_arm_ext_littlebyte_bigword);
  builtin_type_ia64_spill_big =
    init_type (TYPE_CODE_FLT, floatformat_ia64_spill_big.totalsize / 8,
	       0, "builtin_type_ia64_spill_big", NULL);
  TYPE_FLOATFORMAT (builtin_type_ia64_spill_big) = &floatformat_ia64_spill_big;
  builtin_type_ia64_spill_little =
    init_type (TYPE_CODE_FLT, floatformat_ia64_spill_little.totalsize / 8,
	       0, "builtin_type_ia64_spill_little", NULL);
  TYPE_FLOATFORMAT (builtin_type_ia64_spill_little) = &floatformat_ia64_spill_little;
  builtin_type_ia64_spill[BFD_ENDIAN_BIG]
    = build_flt (floatformat_ia64_spill_big.totalsize,
		 "builtin_type_ia64_spill_big",
		 &floatformat_ia64_spill_big);
  builtin_type_ia64_spill[BFD_ENDIAN_LITTLE]
    = build_flt (floatformat_ia64_spill_little.totalsize,
		 "builtin_type_ia64_spill_little",
		 &floatformat_ia64_spill_little);
  builtin_type_ia64_quad_big =
    init_type (TYPE_CODE_FLT, floatformat_ia64_quad_big.totalsize / 8,
	       0, "builtin_type_ia64_quad_big", NULL);
  TYPE_FLOATFORMAT (builtin_type_ia64_quad_big) = &floatformat_ia64_quad_big;
  builtin_type_ia64_quad_little =
    init_type (TYPE_CODE_FLT, floatformat_ia64_quad_little.totalsize / 8,
	       0, "builtin_type_ia64_quad_little", NULL);
  TYPE_FLOATFORMAT (builtin_type_ia64_quad_little) = &floatformat_ia64_quad_little;
  builtin_type_ia64_quad[BFD_ENDIAN_BIG]
    = build_flt (floatformat_ia64_quad_big.totalsize,
		 "builtin_type_ia64_quad_big",
		 &floatformat_ia64_quad_big);
  builtin_type_ia64_quad[BFD_ENDIAN_LITTLE]
    = build_flt (floatformat_ia64_quad_little.totalsize,
		 "builtin_type_ia64_quad_little",
		 &floatformat_ia64_quad_little);

  add_setshow_zinteger_cmd ("overload", no_class, &overload_debug, _("\
Set debugging of C++ overloading."), _("\
Show debugging of C++ overloading."), _("\
When enabled, ranking of the functions is displayed."),
			    NULL,
			    show_overload_debug,
			    &setdebuglist, &showdebuglist);
  /* APPLE LOCAL begin array stride */
  add_setshow_zinteger_cmd ("use-array-stride", no_class,
			    &use_stride, _("\
Set if GDB should honor the 'stride' parameter of array types."), _("\
Show if GDB should honor the 'stride' parameter of array types."), NULL,
			    NULL, NULL,
			    &setlist, &showlist);
  /* APPLE LOCAL end array stride */

  /* APPLE LOCAL: This list holds the arrays whose element type was unknown
     when the array type was created.  */
  undef_arrays_allocated = 20;
  undef_arrays_length = 0;
  undef_arrays = (struct type **)
    xmalloc (undef_arrays_allocated * sizeof (struct type *));

}

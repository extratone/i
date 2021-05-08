/* Support routines for building symbol tables in GDB's internal format.
   Copyright 1986, 1987, 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995,
   1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004
   Free Software Foundation, Inc.

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

/* This module provides subroutines used for creating and adding to
   the symbol table.  These routines are called from various symbol-
   file-reading routines.

   Routines to support specific debugging information formats (stabs,
   DWARF, etc) belong somewhere else. */

#include "defs.h"
#include "bfd.h"
#include "gdb_obstack.h"
#include "symtab.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdbtypes.h"
#include "gdb_assert.h"
#include "complaints.h"
#include "gdb_string.h"
#include "expression.h"		/* For "enum exp_opcode" used by... */
#include "bcache.h"
#include "filenames.h"		/* For DOSish file names */
#include "macrotab.h"
#include "demangle.h"		/* Needed by SYMBOL_INIT_DEMANGLED_NAME.  */
#include "block.h"
#include "cp-support.h"
#include "dictionary.h"
#include "inlining.h"

/* Ask buildsym.h to define the vars it normally declares `extern'.  */
#define	EXTERN
/**/
#include "buildsym.h"		/* Our own declarations */
#undef	EXTERN

/* For cleanup_undefined_types and finish_global_stabs (somewhat
   questionable--see comment where we call them).  */

#include "stabsread.h"

/* List of free `struct pending' structures for reuse.  */

static struct pending *free_pendings;

/* Non-zero if symtab has line number info.  This prevents an
   otherwise empty symtab from being tossed.  */

static int have_line_numbers;

/* APPLE LOCAL make compare_line_numbers extern */


/* Initial sizes of data structures.  These are realloc'd larger if
   needed, and realloc'd down to the size actually used, when
   completed.  */

#define	INITIAL_CONTEXT_STACK_SIZE	10
#define	INITIAL_LINE_VECTOR_LENGTH	1000


/* maintain the lists of symbols and blocks */

/* Add a pending list to free_pendings. */
void
add_free_pendings (struct pending *list)
{
  struct pending *link = list;

  if (list)
    {
      while (link->next) link = link->next;
      link->next = free_pendings;
      free_pendings = list;
    }
}
      
/* Add a symbol to one of the lists of symbols.  While we're at it, if
   we're in the C++ case and don't have full namespace debugging info,
   check to see if it references an anonymous namespace; if so, add an
   appropriate using directive.  */

void
add_symbol_to_list (struct symbol *symbol, struct pending **listhead)
{
  struct pending *link;

  /* If this is an alias for another symbol, don't add it.  */
  if (symbol->ginfo.name && symbol->ginfo.name[0] == '#')
    return;

  /* We keep PENDINGSIZE symbols in each link of the list. If we
     don't have a link with room in it, add a new link.  */
  if (*listhead == NULL || (*listhead)->nsyms == PENDINGSIZE)
    {
      if (free_pendings)
	{
	  link = free_pendings;
	  free_pendings = link->next;
	}
      else
	{
	  link = (struct pending *) xmalloc (sizeof (struct pending));
	}

      link->next = *listhead;
      *listhead = link;
      link->nsyms = 0;
    }

  (*listhead)->symbol[(*listhead)->nsyms++] = symbol;

  /* Check to see if we might need to look for a mention of anonymous
     namespaces.  */
  
  /* APPLE LOCAL begin Objective-C++ */
  if (SYMBOL_LANGUAGE (symbol) == language_cplus 
      || SYMBOL_LANGUAGE (symbol) == language_objcplus)
    /* APPLE LOCAL end Objective-C++ */
    cp_scan_for_anonymous_namespaces (symbol);

  /* APPLE LOCAL fix-and-continue */
  SYMBOL_OBSOLETED (symbol) = 0;
}

/* Find a symbol named NAME on a LIST.  NAME need not be
   '\0'-terminated; LENGTH is the length of the name.  */

struct symbol *
find_symbol_in_list (struct pending *list, char *name, int length)
{
  int j;
  char *pp;

  while (list != NULL)
    {
      for (j = list->nsyms; --j >= 0;)
	{
	  pp = DEPRECATED_SYMBOL_NAME (list->symbol[j]);
	  if (*pp == *name && strncmp (pp, name, length) == 0 &&
	      pp[length] == '\0')
	    {
	      return (list->symbol[j]);
	    }
	}
      list = list->next;
    }
  return (NULL);
}

/* At end of reading syms, or in case of quit, really free as many
   `struct pending's as we can easily find. */

void
really_free_pendings (void *dummy)
{
  struct pending *next, *next1;

  for (next = free_pendings; next; next = next1)
    {
      next1 = next->next;
      xfree ((void *) next);
    }
  free_pendings = NULL;

  free_pending_blocks ();

  for (next = file_symbols; next != NULL; next = next1)
    {
      next1 = next->next;
      xfree ((void *) next);
    }
  file_symbols = NULL;

  for (next = global_symbols; next != NULL; next = next1)
    {
      next1 = next->next;
      xfree ((void *) next);
    }
  global_symbols = NULL;

  if (pending_macros)
    free_macro_table (pending_macros);
}

/* This function is called to discard any pending blocks. */

void
free_pending_blocks (void)
{
#if 0				/* Now we make the links in the
				   objfile_obstack, so don't free
				   them.  */
  struct pending_block *bnext, *bnext1;

  for (bnext = pending_blocks; bnext; bnext = bnext1)
    {
      bnext1 = bnext->next;
      xfree ((void *) bnext);
    }
#endif
  pending_blocks = NULL;
}

/* Take one of the lists of symbols and make a block from it.  Keep
   the order the symbols have in the list (reversed from the input
   file).  Put the block on the list of pending blocks.  */

/* APPLE LOCAL address ranges: Add a new parameter, RANGES, for the
   case in which the block is contained within multiple non-contiguous
   ranges of addresses.  EITHER the block should have a single START
   and END value, OR it should have a RANGES value.  If RANGES is
   non-null, then START and END will be the starting and ending
   address of the first range in the range list.  For those debug
   formats or architectures which do not support multiple
   non-contiguous ranges of addressess (or in the case where a block
   is all contained within a single contiguous address range), RANGES
   should just be NULL.  */

void
finish_block (struct symbol *symbol, struct pending **listhead,
	      struct pending_block *old_blocks, 
	      CORE_ADDR start, CORE_ADDR end, 
	      /* APPLE LOCAL begin address ranges  */
	      struct address_range_list *ranges, struct objfile *objfile)
	      /* APPLE LOCAL end address ranges  */
{
  struct pending *next, *next1;
  struct block *block;
  struct pending_block *pblock;
  struct pending_block *opblock;
  int i;

  block = allocate_block (&objfile->objfile_obstack);

  if (symbol)
    {
      BLOCK_DICT (block) = dict_create_linear (&objfile->objfile_obstack,
					       *listhead);
    }
  else
    {
      BLOCK_DICT (block) = dict_create_hashed (&objfile->objfile_obstack,
					       *listhead);
    }

  /* APPLE LOCAL begin address ranges  */
  if (ranges)
    {
      /* Figure out the min and max address for this block from the ranges
         information.  */
      for (i=0; i<ranges->nelts; i++)
	{
	  if (ranges->ranges[i].startaddr < start)
	    start = ranges->ranges[i].startaddr;
	  if (ranges->ranges[i].endaddr > end)
	    end = ranges->ranges[i].endaddr;
	}
    }
  /* APPLE LOCAL end address ranges  */

  BLOCK_START (block) = start;
  BLOCK_END (block) = end;
  /* APPLE LOCAL begin address ranges  */
  BLOCK_RANGES (block) = ranges;
  /* APPLE LOCAL end address ranges  */
  /* Superblock filled in when containing block is made */
  BLOCK_SUPERBLOCK (block) = NULL;
  BLOCK_NAMESPACE (block) = NULL;

  BLOCK_GCC_COMPILED (block) = processing_gcc_compilation;

  /* Put the block in as the value of the symbol that names it.  */

  if (symbol)
    {
      struct type *ftype = SYMBOL_TYPE (symbol);
      struct dict_iterator iter;
      SYMBOL_BLOCK_VALUE (symbol) = block;
      BLOCK_FUNCTION (block) = symbol;

      if (TYPE_NFIELDS (ftype) <= 0)
	{
	  /* No parameter type information is recorded with the
	     function's type.  Set that from the type of the
	     parameter symbols. */
	  int nparams = 0, iparams;
	  struct symbol *sym;
	  ALL_BLOCK_SYMBOLS (block, iter, sym)
	    {
	      switch (SYMBOL_CLASS (sym))
		{
		case LOC_ARG:
		case LOC_REF_ARG:
		case LOC_REGPARM:
		case LOC_REGPARM_ADDR:
		case LOC_BASEREG_ARG:
		case LOC_LOCAL_ARG:
		case LOC_COMPUTED_ARG:
		  nparams++;
		  break;
		case LOC_UNDEF:
		case LOC_CONST:
		case LOC_STATIC:
		case LOC_INDIRECT:
		case LOC_REGISTER:
		case LOC_LOCAL:
		case LOC_TYPEDEF:
		case LOC_LABEL:
		case LOC_BLOCK:
		case LOC_CONST_BYTES:
		case LOC_BASEREG:
		case LOC_UNRESOLVED:
		case LOC_OPTIMIZED_OUT:
		case LOC_COMPUTED:
		default:
		  break;
		}
	    }
	  if (nparams > 0)
	    {
	      TYPE_NFIELDS (ftype) = nparams;
	      TYPE_FIELDS (ftype) = (struct field *)
		TYPE_ALLOC (ftype, nparams * sizeof (struct field));
	      /* APPLE LOCAL ??? */
	      memset (TYPE_FIELDS (ftype), 0, sizeof (struct field) * nparams);

	      iparams = 0;
	      ALL_BLOCK_SYMBOLS (block, iter, sym)
		{
		  if (iparams == nparams)
		    break;

		  switch (SYMBOL_CLASS (sym))
		    {
		    case LOC_ARG:
		    case LOC_REF_ARG:
		    case LOC_REGPARM:
		    case LOC_REGPARM_ADDR:
		    case LOC_BASEREG_ARG:
		    case LOC_LOCAL_ARG:
		    case LOC_COMPUTED_ARG:
		      TYPE_FIELD_TYPE (ftype, iparams) = SYMBOL_TYPE (sym);
		      TYPE_FIELD_ARTIFICIAL (ftype, iparams) = 0;
		      iparams++;
		      break;
		    case LOC_UNDEF:
		    case LOC_CONST:
		    case LOC_STATIC:
		    case LOC_INDIRECT:
		    case LOC_REGISTER:
		    case LOC_LOCAL:
		    case LOC_TYPEDEF:
		    case LOC_LABEL:
		    case LOC_BLOCK:
		    case LOC_CONST_BYTES:
		    case LOC_BASEREG:
		    case LOC_UNRESOLVED:
		    case LOC_OPTIMIZED_OUT:
		    case LOC_COMPUTED:
		    default:
		      break;
		    }
		}
	    }
	}

      /* If we're in the C++ case, set the block's scope.  */
      /* APPLE LOCAL begin Objective-C++ */
      if (SYMBOL_LANGUAGE (symbol) == language_cplus
	  || SYMBOL_LANGUAGE (symbol) == language_objcplus)
	/* APPLE LOCAL end Objective-C++ */
	{
	  cp_set_block_scope (symbol, block, &objfile->objfile_obstack);
	}
    }
  else
    {
      BLOCK_FUNCTION (block) = NULL;
    }

  /* Now "free" the links of the list, and empty the list.  */

  for (next = *listhead; next; next = next1)
    {
      next1 = next->next;
      next->next = free_pendings;
      free_pendings = next;
    }
  *listhead = NULL;

#if 1
  /* Check to be sure that the blocks have an end address that is
     greater than starting address */

  /* APPLE LOCAL begin address ranges  */
  if (!BLOCK_RANGES (block)
      && BLOCK_END (block) < BLOCK_START (block))
  /* APPLE LOCAL end address ranges  */
    {
      if (symbol)
	{
	  complaint (&symfile_complaints,
		     _("block end address less than block start address in %s (patched it)"),
		     SYMBOL_PRINT_NAME (symbol));
	}
      else
	{
	  complaint (&symfile_complaints,
		     _("block end address 0x%s less than block start address 0x%s (patched it)"),
		     paddr_nz (BLOCK_END (block)), paddr_nz (BLOCK_START (block)));
	}
      /* Better than nothing */
      BLOCK_END (block) = BLOCK_START (block);
    }
  /* APPLE LOCAL begin address ranges  */
  else if (BLOCK_RANGES (block))
    {
      int i;
      for (i = 0; i < BLOCK_RANGES (block)->nelts; i++)
	if (BLOCK_RANGE_END (block, i) < BLOCK_RANGE_START (block, i))
	  {
	    if (symbol)
	      {
		complaint (&symfile_complaints,
		     _("block end address less than block start address in %s (patched it)"),
		     SYMBOL_PRINT_NAME (symbol));
	      }
	    else
	      {
	  complaint (&symfile_complaints,
		     _("block end address 0x%s less than block start address 0x%s (patched it)"),
		     paddr_nz (BLOCK_RANGE_END (block, i)), 
		     paddr_nz (BLOCK_RANGE_START (block, i)));
	      }
	    /* Better than nothing */
	    BLOCK_RANGE_END (block, i) = BLOCK_RANGE_START (block, i);
	  }
    }
  /* APPLE LOCAL end address ranges  */
#endif

  /* Install this block as the superblock of all blocks made since the
     start of this scope that don't have superblocks yet.  */

  opblock = NULL;
  for (pblock = pending_blocks; 
       pblock && pblock != old_blocks; 
       pblock = pblock->next)
    {
      if (BLOCK_SUPERBLOCK (pblock->block) == NULL)
	{
#if 1
	  /* Check to be sure the blocks are nested as we receive
	     them. If the compiler/assembler/linker work, this just
	     burns a small amount of time.  */
	  /* APPLE LOCAL begin address ranges  */
	  if (!contained_in (pblock->block, block))
	  /* APPLE LOCAL end address ranges  */
	    {
	      if (symbol)
		{
		  complaint (&symfile_complaints,
			     _("inner block not inside outer block in %s"),
			     SYMBOL_PRINT_NAME (symbol));
		}
	      else
		{
                  /* APPLE LOCAL: If you've got a function and you know
                     its name, clap your hands.  Er, I mean, print it.  */
                  if (pblock->block->function 
                      && pblock->block->function->ginfo.name)
		  complaint (&symfile_complaints,
			     _("inner block (0x%s-0x%s '%s') not inside outer block (0x%s-0x%s)"),
			     paddr_nz (BLOCK_START (pblock->block)),
			     paddr_nz (BLOCK_END (pblock->block)),
                             pblock->block->function->ginfo.name,
			     paddr_nz (BLOCK_START (block)),
			     paddr_nz (BLOCK_END (block)));
                  else
		  complaint (&symfile_complaints,
			     _("inner block (0x%s-0x%s) not inside outer block (0x%s-0x%s)"),
			     paddr_nz (BLOCK_START (pblock->block)),
			     paddr_nz (BLOCK_END (pblock->block)),
			     paddr_nz (BLOCK_START (block)),
			     paddr_nz (BLOCK_END (block)));
		}
	      /* APPLE LOCAL begin address ranges  */
	      /* We're trying to fit all the pending blocks into a
		 super block.  In general, we get all the pending blocks
		 by looking at the indididual function entries, so their
		 addresses are quite likely to be right.  But we got the
		 super block start and end by looking at something like
		 the psymtab range, which is less likely to be accurate.
		 So trust the pending blocks, and if they lie outside
		 the super block fix up the super block ranges to fit
		 them.  */
	      if (!BLOCK_RANGES (pblock->block) && !BLOCK_RANGES (block))
		{
		  if (BLOCK_START (pblock->block) < BLOCK_START (block))
		    BLOCK_START (block) = BLOCK_START (pblock->block);
		  if (BLOCK_END (pblock->block) > BLOCK_END (block))
		    BLOCK_END (block) = BLOCK_END (pblock->block);
		}
	      /* APPLE LOCAL end address ranges  */
	    }
#endif
	  BLOCK_SUPERBLOCK (pblock->block) = block;
	}
      opblock = pblock;
    }

  record_pending_block (objfile, block, opblock);
}


/* Record BLOCK on the list of all blocks in the file.  Put it after
   OPBLOCK, or at the beginning if opblock is NULL.  This puts the
   block in the list after all its subblocks.

   Allocate the pending block struct in the objfile_obstack to save
   time.  This wastes a little space.  FIXME: Is it worth it?  */

void
record_pending_block (struct objfile *objfile, struct block *block,
		      struct pending_block *opblock)
{
  struct pending_block *pblock;

  pblock = (struct pending_block *)
    obstack_alloc (&objfile->objfile_obstack, sizeof (struct pending_block));
  pblock->block = block;
  if (opblock)
    {
      pblock->next = opblock->next;
      opblock->next = pblock;
    }
  else
    {
      pblock->next = pending_blocks;
      pending_blocks = pblock;
    }
}

/* APPLE LOCAL begin sort objfile blocks */
static int
compare_blocks (const void *v1, const void *v2)
{
  const struct block *const *b1 = v1;
  const struct block *const *b2 = v2;

  if ((*b1)->startaddr < (*b2)->startaddr)
    return -1;
  else if ((*b1)->startaddr > (*b2)->startaddr)
    return 1;
  else if ((*b1)->endaddr < (*b2)->endaddr)
    return 1;
  else if ((*b1)->endaddr > (*b2)->endaddr)
    return -1;
  else
    return 0;
}
/* APPLE LOCAL end sort objfile blocks */

static struct blockvector *
make_blockvector (struct objfile *objfile)
{
  struct pending_block *next;
  struct blockvector *blockvector;
  int i;

  /* Count the length of the list of blocks.  */

  for (next = pending_blocks, i = 0; next; next = next->next, i++)
    {;
    }

  blockvector = (struct blockvector *)
    obstack_alloc (&objfile->objfile_obstack,
		   (sizeof (struct blockvector)
		    + (i - 1) * sizeof (struct block *)));

  /* Copy the blocks into the blockvector. This is done in reverse
     order, which happens to put the blocks into the proper order
     (ascending starting address). finish_block has hair to insert
     each block into the list after its subblocks in order to make
     sure this is true.  */

  BLOCKVECTOR_NBLOCKS (blockvector) = i;
  for (next = pending_blocks; next; next = next->next)
    {
      BLOCKVECTOR_BLOCK (blockvector, --i) = next->block;
    }

#if 0				/* Now we make the links in the
				   obstack, so don't free them.  */
  /* Now free the links of the list, and empty the list.  */

  for (next = pending_blocks; next; next = next1)
    {
      next1 = next->next;
      xfree (next);
    }
#endif
  pending_blocks = NULL;

  /* APPLE LOCAL begin sort objfile blocks */
  if (objfile->flags & OBJF_REORDERED)
    {
      if (BLOCKVECTOR_NBLOCKS (blockvector) > 2)
	qsort (&blockvector->block[2],
	       BLOCKVECTOR_NBLOCKS (blockvector) - 2,
	       sizeof (struct block *),
	       compare_blocks);
    }
  /* APPLE LOCAL end sort objfile blocks */

#if 1				/* FIXME, shut this off after a while
				   to speed up symbol reading.  */
  /* Some compilers output blocks in the wrong order, but we depend on
     their being in the right order so we can binary search. Check the
     order and moan about it.  FIXME.  */
  if (BLOCKVECTOR_NBLOCKS (blockvector) > 1)
    {
      for (i = 1; i < BLOCKVECTOR_NBLOCKS (blockvector); i++)
	{
	  /* APPLE LOCAL begin address ranges  */
	  CORE_ADDR start1;
	  CORE_ADDR start2;

	  start1 = BLOCK_LOWEST_PC (BLOCKVECTOR_BLOCK (blockvector, i - 1));
	  start2 = BLOCK_LOWEST_PC (BLOCKVECTOR_BLOCK (blockvector, i));

	  if (start1 > start2)
	    complaint (&symfile_complaints, _("block at %s out of order"),
		       hex_string ((LONGEST) start2));
	  /* APPLE LOCAL end address ranges  */
	}
    }
#endif

  return (blockvector);
}

/* Start recording information about source code that came from an
   included (or otherwise merged-in) source file with a different
   name.  NAME is the name of the file (cannot be NULL), DIRNAME is
   the directory in which it resides (or NULL if not known).  */

void
start_subfile (char *name, char *dirname)
{
  struct subfile *subfile;

  /* See if this subfile is already known as a subfile of the current
     main source file.  */

  for (subfile = subfiles; subfile; subfile = subfile->next)
    {
      if (FILENAME_CMP (subfile->name, name) == 0)
	{
	  current_subfile = subfile;
	  return;
	}
    }

  /* This subfile is not known.  Add an entry for it. Make an entry
     for this subfile in the list of all subfiles of the current main
     source file.  */

  subfile = (struct subfile *) xmalloc (sizeof (struct subfile));
  memset ((char *) subfile, 0, sizeof (struct subfile));
  subfile->next = subfiles;
  subfiles = subfile;
  current_subfile = subfile;

  /* Save its name and compilation directory name */
  subfile->name = (name == NULL) ? NULL : savestring (name, strlen (name));
  subfile->dirname =
    (dirname == NULL) ? NULL : savestring (dirname, strlen (dirname));

  /* Initialize line-number recording for this subfile.  */
  subfile->line_vector = NULL;

  /* Default the source language to whatever can be deduced from the
     filename.  If nothing can be deduced (such as for a C/C++ include
     file with a ".h" extension), then inherit whatever language the
     previous subfile had.  This kludgery is necessary because there
     is no standard way in some object formats to record the source
     language.  Also, when symtabs are allocated we try to deduce a
     language then as well, but it is too late for us to use that
     information while reading symbols, since symtabs aren't allocated
     until after all the symbols have been processed for a given
     source file. */

  subfile->language = deduce_language_from_filename (subfile->name);
  if (subfile->language == language_unknown &&
      subfile->next != NULL)
    {
      subfile->language = subfile->next->language;
    }

  /* Initialize the debug format string to NULL.  We may supply it
     later via a call to record_debugformat. */
  subfile->debugformat = NULL;

  /* Similarly for the producer.  */
  subfile->producer = NULL;

  /* If the filename of this subfile ends in .C, then change the
     language of any pending subfiles from C to C++.  We also accept
     any other C++ suffixes accepted by deduce_language_from_filename.  */
  /* Likewise for f2c.  */

  if (subfile->name)
    {
      struct subfile *s;
      enum language sublang = deduce_language_from_filename (subfile->name);

      /* APPLE LOCAL Objective-C++ */
      if (sublang == language_cplus || sublang == language_objcplus || sublang == language_fortran)
	for (s = subfiles; s != NULL; s = s->next)
	  if (s->language == language_c)
	    s->language = sublang;
    }

  /* And patch up this file if necessary.  */
  if (subfile->language == language_c
      && subfile->next != NULL
      && (subfile->next->language == language_cplus
	  /* APPLE LOCAL Objective-C++ */
	  || subfile->next->language == language_objcplus
	  || subfile->next->language == language_fortran))
    {
      subfile->language = subfile->next->language;
    }
}

/* For stabs readers, the first N_SO symbol is assumed to be the
   source file name, and the subfile struct is initialized using that
   assumption.  If another N_SO symbol is later seen, immediately
   following the first one, then the first one is assumed to be the
   directory name and the second one is really the source file name.

   So we have to patch up the subfile struct by moving the old name
   value to dirname and remembering the new name.  Some sanity
   checking is performed to ensure that the state of the subfile
   struct is reasonable and that the old name we are assuming to be a
   directory name actually is (by checking for a trailing '/'). */

void
patch_subfile_names (struct subfile *subfile, char *name)
{
  if (subfile != NULL && subfile->dirname == NULL && subfile->name != NULL
      && subfile->name[strlen (subfile->name) - 1] == '/')
    {
      subfile->dirname = subfile->name;
      subfile->name = savestring (name, strlen (name));
      last_source_file = name;

      /* Default the source language to whatever can be deduced from
         the filename.  If nothing can be deduced (such as for a C/C++
         include file with a ".h" extension), then inherit whatever
         language the previous subfile had.  This kludgery is
         necessary because there is no standard way in some object
         formats to record the source language.  Also, when symtabs
         are allocated we try to deduce a language then as well, but
         it is too late for us to use that information while reading
         symbols, since symtabs aren't allocated until after all the
         symbols have been processed for a given source file. */

      subfile->language = deduce_language_from_filename (subfile->name);
      if (subfile->language == language_unknown &&
	  subfile->next != NULL)
	{
	  subfile->language = subfile->next->language;
	}
    }
}

/* Handle the N_BINCL and N_EINCL symbol types that act like N_SOL for
   switching source files (different subfiles, as we call them) within
   one object file, but using a stack rather than in an arbitrary
   order.  */

void
push_subfile (void)
{
  struct subfile_stack *tem
  = (struct subfile_stack *) xmalloc (sizeof (struct subfile_stack));

  tem->next = subfile_stack;
  subfile_stack = tem;
  if (current_subfile == NULL || current_subfile->name == NULL)
    {
      internal_error (__FILE__, __LINE__, _("failed internal consistency check"));
    }
  tem->name = current_subfile->name;
}

char *
pop_subfile (void)
{
  char *name;
  struct subfile_stack *link = subfile_stack;

  if (link == NULL)
    {
      internal_error (__FILE__, __LINE__, _("failed internal consistency check"));
    }
  name = link->name;
  subfile_stack = link->next;
  xfree ((void *) link);
  return (name);
}

/* Add a linetable entry for line number LINE and address PC to the
   line vector for SUBFILE.  */
/* APPLE LOCAL begin subroutine inlining  */
/* The ENTRY_TYPE parameter indicates the type of line table entry
   (see definition of enum line_table_entry_type).  END_PC wil be zero
   for most entries, but some entries that need to specify an ending
   address (such as entries for inlined subroutines) will have a
   non-zero value in END_PC.  */
void
record_line (struct subfile *subfile, int line, CORE_ADDR pc, CORE_ADDR end_pc,
	     enum line_table_entry_type  entry_type)
/* APPLE LOCAL end subroutine inlining  */
{
  struct linetable_entry *e;
  /* Ignore the dummy line number in libg.o */

  if (dwarf2_debug_inlined_stepping)
    {
      if (entry_type != NORMAL_LT_ENTRY)
	{
	  if (entry_type == INLINED_CALL_SITE_LT_ENTRY)
	    fprintf_unfiltered (gdb_stdout, "     INLINED CALL SITE,  ");
	  else
	    fprintf_unfiltered (gdb_stdout, "     INLINED SUBROUTINE, ");
	  fprintf_unfiltered (gdb_stdout, "0x%x  - 0x%x, ", 
			      (unsigned int) pc, (unsigned int) end_pc);
	  fprintf_unfiltered (gdb_stdout, "%s:%d\n", subfile->name, line);
	}
    }

  if (line == 0xffff)
    {
      return;
    }

  /* Make sure line vector exists and is big enough.  */
  if (!subfile->line_vector)
    {
      subfile->line_vector_length = INITIAL_LINE_VECTOR_LENGTH;
      subfile->line_vector = (struct linetable *)
	xmalloc (sizeof (struct linetable)
	   + subfile->line_vector_length * sizeof (struct linetable_entry));
      subfile->line_vector->nitems = 0;
      /* APPLE LOCAL codewarrior support */
      subfile->line_vector->lines_are_chars = 0;
      have_line_numbers = 1;
    }

  if (subfile->line_vector->nitems + 1 >= subfile->line_vector_length)
    {
      subfile->line_vector_length *= 2;
      subfile->line_vector = (struct linetable *)
	xrealloc ((char *) subfile->line_vector,
		  (sizeof (struct linetable)
		   + (subfile->line_vector_length
		      * sizeof (struct linetable_entry))));
    }

  e = subfile->line_vector->item + subfile->line_vector->nitems++;
  e->line = line;
  e->pc = ADDR_BITS_REMOVE(pc);
  /* APPLE LOCAL begin subroutine inlining  */
  e->end_pc = ADDR_BITS_REMOVE(end_pc);
  e->entry_type = entry_type;
  /* APPLE LOCAL end subroutine inlining  */
}

/* Needed in order to sort line tables from IBM xcoff files.  Sigh!  */

/* APPLE LOCAL make compare_line_numbers extern */
int
compare_line_numbers (const void *ln1p, const void *ln2p)
{
  struct linetable_entry *ln1 = (struct linetable_entry *) ln1p;
  struct linetable_entry *ln2 = (struct linetable_entry *) ln2p;

  /* Note: this code does not assume that CORE_ADDRs can fit in ints.
     Please keep it that way.  */
  if (ln1->pc < ln2->pc)
    return -1;

  if (ln1->pc > ln2->pc)
    return 1;

  /* If pc equal, sort by line.  I'm not sure whether this is optimum
     behavior (see comment at struct linetable in symtab.h).  */
  return ln1->line - ln2->line;
}

/* Start a new symtab for a new source file.  Called, for example,
   when a stabs symbol of type N_SO is seen, or when a DWARF
   TAG_compile_unit DIE is seen.  It indicates the start of data for
   one original source file.  */

void
start_symtab (char *name, char *dirname, CORE_ADDR start_addr)
{

  last_source_file = name;
  last_source_start_addr = start_addr;
  file_symbols = NULL;
  global_symbols = NULL;
  within_function = 0;
  have_line_numbers = 0;

  /* Context stack is initially empty.  Allocate first one with room
     for 10 levels; reuse it forever afterward.  */
  if (context_stack == NULL)
    {
      context_stack_size = INITIAL_CONTEXT_STACK_SIZE;
      context_stack = (struct context_stack *)
	xmalloc (context_stack_size * sizeof (struct context_stack));
    }
  context_stack_depth = 0;

  /* Set up support for C++ namespace support, in case we need it.  */

  cp_initialize_namespace ();

  /* Initialize the list of sub source files with one entry for this
     file (the top-level source file).  */

  subfiles = NULL;
  current_subfile = NULL;
  start_subfile (name, dirname);
}

/* Finish the symbol definitions for one main source file, close off
   all the lexical contexts for that file (creating struct block's for
   them), then make the struct symtab for that file and put it in the
   list of all such.

   END_ADDR is the address of the end of the file's text.  SECTION is
   the section number (in objfile->section_offsets) of the blockvector
   and linetable.

   Note that it is possible for end_symtab() to return NULL.  In
   particular, for the DWARF case at least, it will return NULL when
   it finds a compilation unit that has exactly one DIE, a
   TAG_compile_unit DIE.  This can happen when we link in an object
   file that was compiled from an empty source file.  Returning NULL
   is probably not the correct thing to do, because then gdb will
   never know about this empty file (FIXME). */

struct symtab *
end_symtab (CORE_ADDR end_addr, struct objfile *objfile, int section)
{
  struct symtab *symtab = NULL;
  struct blockvector *blockvector;
  struct subfile *subfile;
  struct context_stack *cstk;
  struct subfile *nextsub;

  /* Finish the lexical context of the last function in the file; pop
     the context stack.  */

  if (context_stack_depth > 0)
    {
      cstk = pop_context ();
      /* Make a block for the local symbols within.  */
      /* APPLE LOCAL begin address ranges  */
      finish_block (cstk->name, &local_symbols, cstk->old_blocks,
		    cstk->start_addr, end_addr, NULL, objfile);
      /* APPLE LOCAL end address ranges  */

      if (context_stack_depth > 0)
	{
	  /* This is said to happen with SCO.  The old coffread.c
	     code simply emptied the context stack, so we do the
	     same.  FIXME: Find out why it is happening.  This is not
	     believed to happen in most cases (even for coffread.c);
	     it used to be an abort().  */
	  complaint (&symfile_complaints,
	             _("Context stack not empty in end_symtab"));
	  context_stack_depth = 0;
	}
    }

  /* APPLE LOCAL replaced by sort in make_blockvector */

  /* Cleanup any undefined types that have been left hanging around
     (this needs to be done before the finish_blocks so that
     file_symbols is still good).

     Both cleanup_undefined_types and finish_global_stabs are stabs
     specific, but harmless for other symbol readers, since on gdb
     startup or when finished reading stabs, the state is set so these
     are no-ops.  FIXME: Is this handled right in case of QUIT?  Can
     we make this cleaner?  */

  cleanup_undefined_types ();

  /* APPLE LOCAL: Some fields may not have gotten the "packed" 
     set right because their type was not known yet.  This cleans that up.  */
  cleanup_undefined_fields ();
  cleanup_undefined_arrays ();

  finish_global_stabs (objfile);

  if (pending_blocks == NULL
      && file_symbols == NULL
      && global_symbols == NULL
      && have_line_numbers == 0
      && pending_macros == NULL)
    {
      /* Ignore symtabs that have no functions with real debugging
         info.  */
      blockvector = NULL;
    }
  else
    {
      /* Define the STATIC_BLOCK & GLOBAL_BLOCK, and build the
         blockvector.  */
      /* APPLE LOCAL begin address ranges  */
      finish_block (0, &file_symbols, 0, last_source_start_addr, end_addr,
		    NULL, objfile);
      finish_block (0, &global_symbols, 0, last_source_start_addr, end_addr,
		    NULL, objfile);
      /* APPLE LOCAL end address ranges  */
      blockvector = make_blockvector (objfile);
      cp_finalize_namespace (BLOCKVECTOR_BLOCK (blockvector, STATIC_BLOCK),
			     &objfile->objfile_obstack);
    }

#ifndef PROCESS_LINENUMBER_HOOK
#define PROCESS_LINENUMBER_HOOK()
#endif
  PROCESS_LINENUMBER_HOOK ();	/* Needed for xcoff. */

  /* Now create the symtab objects proper, one for each subfile.  */
  /* (The main file is the last one on the chain.)  */

  for (subfile = subfiles; subfile; subfile = nextsub)
    {
      int linetablesize = 0;
      symtab = NULL;

      /* If we have blocks of symbols, make a symtab. Otherwise, just
         ignore this file and any line number info in it.  */
      if (blockvector)
	{
	  if (subfile->line_vector)
	    {
	      linetablesize = sizeof (struct linetable) +
	        subfile->line_vector->nitems * sizeof (struct linetable_entry);
#if 0
	      /* I think this is artifact from before it went on the
	         obstack. I doubt we'll need the memory between now
	         and when we free it later in this function.  */
	      /* First, shrink the linetable to make more memory.  */
	      subfile->line_vector = (struct linetable *)
		xrealloc ((char *) subfile->line_vector, linetablesize);
#endif

	      /* Like the pending blocks, the line table may be
	         scrambled in reordered executables.  Sort it if
	         OBJF_REORDERED is true.  */
	      if (objfile->flags & OBJF_REORDERED)
		qsort (subfile->line_vector->item,
		       subfile->line_vector->nitems,
		     sizeof (struct linetable_entry), compare_line_numbers);
	    }

	  /* Now, allocate a symbol table.  */
	  symtab = allocate_symtab (subfile->name, objfile);

	  /* Fill in its components.  */
	  symtab->blockvector = blockvector;
          symtab->macro_table = pending_macros;
	  if (subfile->line_vector)
	    {
	      /* Reallocate the line table on the symbol obstack */
	      symtab->linetable = (struct linetable *)
		obstack_alloc (&objfile->objfile_obstack, linetablesize);
	      memcpy (symtab->linetable, subfile->line_vector, linetablesize);
	    }
	  else
	    {
	      symtab->linetable = NULL;
	    }
	  symtab->block_line_section = section;
	  if (subfile->dirname)
	    {
	      /* Reallocate the dirname on the symbol obstack */
	      symtab->dirname = (char *)
		obstack_alloc (&objfile->objfile_obstack,
			       strlen (subfile->dirname) + 1);
	      strcpy (symtab->dirname, subfile->dirname);
	    }
	  else
	    {
	      symtab->dirname = NULL;
	    }
	  symtab->free_code = free_linetable;
	  symtab->free_func = NULL;

	  /* Use whatever language we have been using for this
	     subfile, not the one that was deduced in allocate_symtab
	     from the filename.  We already did our own deducing when
	     we created the subfile, and we may have altered our
	     opinion of what language it is from things we found in
	     the symbols. */
	  symtab->language = subfile->language;

	  /* Save the debug format string (if any) in the symtab */
	  if (subfile->debugformat != NULL)
	    {
	      symtab->debugformat = obsavestring (subfile->debugformat,
					      strlen (subfile->debugformat),
						  &objfile->objfile_obstack);
	    }

          /* Similarly for the producer.  */
          if (subfile->producer != NULL)
            symtab->producer = obsavestring (subfile->producer,
                                             strlen (subfile->producer),
                                             &objfile->objfile_obstack);

	  /* All symtabs for the main file and the subfiles share a
	     blockvector, so we need to clear primary for everything
	     but the main file.  */

	  symtab->primary = 0;
	}
      if (subfile->name != NULL)
	{
	  xfree ((void *) subfile->name);
	}
      if (subfile->dirname != NULL)
	{
	  xfree ((void *) subfile->dirname);
	}
      if (subfile->line_vector != NULL)
	{
	  xfree ((void *) subfile->line_vector);
	}
      if (subfile->debugformat != NULL)
	{
	  xfree ((void *) subfile->debugformat);
	}
      if (subfile->producer != NULL)
        xfree (subfile->producer);

      nextsub = subfile->next;
      xfree ((void *) subfile);
    }

  /* Set this for the main source file.  */
  if (symtab)
    {
      symtab->primary = 1;
    }

  last_source_file = NULL;
  current_subfile = NULL;
  pending_macros = NULL;

  return symtab;
}

/* Push a context block.  Args are an identifying nesting level
   (checkable when you pop it), and the starting PC address of this
   context.  */

struct context_stack *
push_context (int desc, CORE_ADDR valu)
{
  struct context_stack *new;

  if (context_stack_depth == context_stack_size)
    {
      context_stack_size *= 2;
      context_stack = (struct context_stack *)
	xrealloc ((char *) context_stack,
		  (context_stack_size * sizeof (struct context_stack)));
    }

  new = &context_stack[context_stack_depth++];
  new->depth = desc;
  new->locals = local_symbols;
  new->params = param_symbols;
  new->old_blocks = pending_blocks;
  new->start_addr = valu;
  new->name = NULL;

  local_symbols = NULL;
  param_symbols = NULL;

  return new;
}

/* Pop a context block.  Returns the address of the context block just
   popped. */

struct context_stack *
pop_context (void)
{
  gdb_assert (context_stack_depth > 0);
  return (&context_stack[--context_stack_depth]);
}



/* Compute a small integer hash code for the given name. */

int
hashname (char *name)
{
    return (hash(name,strlen(name)) % HASHSIZE);
}


void
record_debugformat (char *format)
{
  current_subfile->debugformat = savestring (format, strlen (format));
}

void
record_producer (const char *producer)
{
  /* The producer is not always provided in the debugging info.
     Do nothing if PRODUCER is NULL.  */
  if (producer == NULL)
    return;

  current_subfile->producer = savestring (producer, strlen (producer));
}

/* Merge the first symbol list SRCLIST into the second symbol list
   TARGETLIST by repeated calls to add_symbol_to_list().  This
   procedure "frees" each link of SRCLIST by adding it to the
   free_pendings list.  Caller must set SRCLIST to a null list after
   calling this function.

   Void return. */

void
merge_symbol_lists (struct pending **srclist, struct pending **targetlist)
{
  int i;

  if (!srclist || !*srclist)
    return;

  /* Merge in elements from current link.  */
  for (i = 0; i < (*srclist)->nsyms; i++)
    add_symbol_to_list ((*srclist)->symbol[i], targetlist);

  /* Recurse on next.  */
  merge_symbol_lists (&(*srclist)->next, targetlist);

  /* "Free" the current link.  */
  (*srclist)->next = free_pendings;
  free_pendings = (*srclist);
}

/* Initialize anything that needs initializing when starting to read a
   fresh piece of a symbol file, e.g. reading in the stuff
   corresponding to a psymtab.  */

void
buildsym_init (void)
{
  free_pendings = NULL;
  file_symbols = NULL;
  global_symbols = NULL;
  pending_blocks = NULL;
  pending_macros = NULL;
}

/* Initialize anything that needs initializing when a completely new
   symbol file is specified (not just adding some symbols from another
   file, e.g. a shared library).  */

void
buildsym_new_init (void)
{
  buildsym_init ();
}

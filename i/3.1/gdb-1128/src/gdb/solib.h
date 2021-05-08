/* Shared library declarations for GDB, the GNU Debugger.
   
   Copyright 1992, 1993, 1995, 1998, 1999, 2000, 2001, 2003, 2005 
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

#ifndef SOLIB_H
#define SOLIB_H

/* Forward decl's for prototypes */
struct so_list;
struct target_ops;

/* Called when we free all symtabs, to free the shared library information
   as well. */

extern void clear_solib (void);

/* Called to add symbols from a shared library to gdb's symbol table. */

/* APPLE LOCAL return a value */
extern int solib_add (char *, int, struct target_ops *, int);
extern int solib_read_symbols (struct so_list *, int);

/* Function to be called when the inferior starts up, to discover the
   names of shared libraries that are dynamically linked, the base
   addresses to which they are linked, and sufficient information to
   read in their symbols at a later time.  */

extern void solib_create_inferior_hook (void);

/* If ADDR lies in a shared library, return its name.  */

extern char *solib_address (CORE_ADDR);

/* Return 1 if PC lies in the dynamic symbol resolution code of the
   run time loader.  */

extern int in_solib_dynsym_resolve_code (CORE_ADDR);

/* Discard symbols that were auto-loaded from shared libraries. */

extern void no_shared_libraries (char *ignored, int from_tty);

#endif /* SOLIB_H */

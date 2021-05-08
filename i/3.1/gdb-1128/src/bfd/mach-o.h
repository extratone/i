/* Mach-O support for BFD.
   Copyright 1999, 2000, 2001, 2002, 2003, 2005
   Free Software Foundation, Inc.

   This file is part of BFD, the Binary File Descriptor library.

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
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#ifndef _BFD_MACH_O_H_
#define _BFD_MACH_O_H_

#include "bfd.h"

#define BFD_MACH_O_N_STAB  0xe0	/* If any of these bits set, a symbolic debugging entry.  */
#define BFD_MACH_O_N_PEXT  0x10	/* Private external symbol bit.  */
#define BFD_MACH_O_N_TYPE  0x0e	/* Mask for the type bits.  */
#define BFD_MACH_O_N_EXT   0x01	/* External symbol bit, set for external symbols.  */
#define BFD_MACH_O_N_UNDF  0x00	/* Undefined, n_sect == NO_SECT.  */
#define BFD_MACH_O_N_ABS   0x02	/* Absolute, n_sect == NO_SECT.  */
#define BFD_MACH_O_N_SECT  0x0e	/* Defined in section number n_sect.  */
#define BFD_MACH_O_N_PBUD  0x0c /* Prebound undefined (defined in a dylib).  */
#define BFD_MACH_O_N_INDR  0x0a	/* Indirect.  */

typedef enum bfd_mach_o_ppc_thread_flavour
{
  BFD_MACH_O_PPC_THREAD_STATE = 1,
  BFD_MACH_O_PPC_FLOAT_STATE = 2,
  BFD_MACH_O_PPC_EXCEPTION_STATE = 3,
  BFD_MACH_O_PPC_VECTOR_STATE = 4
  /* APPLE LOCAL 64-bit */
  , BFD_MACH_O_PPC_THREAD_STATE_64 = 5
}
bfd_mach_o_ppc_thread_flavour;

typedef enum bfd_mach_o_i386_thread_flavour
{
    BFD_MACH_O_i386_THREAD_STATE = 1,
    BFD_MACH_O_i386_FLOAT_STATE = 2,
    BFD_MACH_O_i386_EXCEPTION_STATE = 3,
    /* APPLE LOCAL begin x86_64 */
    BFD_MACH_O_x86_THREAD_STATE64 = 4,
    BFD_MACH_O_x86_FLOAT_STATE64 = 5,
    BFD_MACH_O_x86_EXCEPTION_STATE64 = 6,
    BFD_MACH_O_x86_THREAD_STATE = 7,
    BFD_MACH_O_x86_FLOAT_STATE = 8,
    BFD_MACH_O_x86_EXCEPTION_STATE = 9,
    BFD_MACH_O_i386_THREAD_STATE_NONE = 10,
    /* APPLE LOCAL end x86_64 */
}
bfd_mach_o_i386_thread_flavour;

typedef enum bfd_mach_o_arm_thread_flavour
  {
    BFD_MACH_O_ARM_THREAD_STATE = 1,
    BFD_MACH_O_ARM_VFP_STATE = 2,
    BFD_MACH_O_ARM_EXCEPTION_STATE = 3,
    BFD_MACH_O_ARM_THREAD_STATE_NONE = 4
  }
bfd_mach_o_arm_thread_flavour;

#define BFD_MACH_O_LC_REQ_DYLD 0x80000000

typedef enum bfd_mach_o_load_command_type
{
  BFD_MACH_O_LC_SEGMENT = 0x1,		/* File segment to be mapped.  */
  BFD_MACH_O_LC_SYMTAB = 0x2,		/* Link-edit stab symbol table info (obsolete).  */
  BFD_MACH_O_LC_SYMSEG = 0x3,		/* Link-edit gdb symbol table info.  */
  BFD_MACH_O_LC_THREAD = 0x4,		/* Thread.  */
  BFD_MACH_O_LC_UNIXTHREAD = 0x5,	/* UNIX thread (includes a stack).  */
  BFD_MACH_O_LC_LOADFVMLIB = 0x6,	/* Load a fixed VM shared library.  */
  BFD_MACH_O_LC_IDFVMLIB = 0x7,		/* Fixed VM shared library id.  */
  BFD_MACH_O_LC_IDENT = 0x8,		/* Object identification information (obsolete).  */
  BFD_MACH_O_LC_FVMFILE = 0x9,		/* Fixed VM file inclusion.  */
  BFD_MACH_O_LC_PREPAGE = 0xa,		/* Prepage command (internal use).  */
  BFD_MACH_O_LC_DYSYMTAB = 0xb,		/* Dynamic link-edit symbol table info.  */
  BFD_MACH_O_LC_LOAD_DYLIB = 0xc,	/* Load a dynamically linked shared library.  */
  BFD_MACH_O_LC_ID_DYLIB = 0xd,		/* Dynamically linked shared lib identification.  */
  BFD_MACH_O_LC_LOAD_DYLINKER = 0xe,	/* Load a dynamic linker.  */
  BFD_MACH_O_LC_ID_DYLINKER = 0xf,	/* Dynamic linker identification.  */
  BFD_MACH_O_LC_PREBOUND_DYLIB = 0x10,	/* Modules prebound for a dynamically.  */
  BFD_MACH_O_LC_ROUTINES = 0x11,	/* Image routines.  */
  BFD_MACH_O_LC_SUB_FRAMEWORK = 0x12,	/* Sub framework.  */
  BFD_MACH_O_LC_SUB_UMBRELLA = 0x13,	/* Sub umbrella.  */
  BFD_MACH_O_LC_SUB_CLIENT = 0x14,	/* Sub client.  */
  BFD_MACH_O_LC_SUB_LIBRARY = 0x15,   	/* Sub library.  */
  BFD_MACH_O_LC_TWOLEVEL_HINTS = 0x16,	/* Two-level namespace lookup hints.  */
  BFD_MACH_O_LC_PREBIND_CKSUM = 0x17, 	/* Prebind checksum.  */
  /* Load a dynamically linked shared library that is allowed to be
       missing (weak).  */
  BFD_MACH_O_LC_LOAD_WEAK_DYLIB = 0x18 | BFD_MACH_O_LC_REQ_DYLD,
  /* APPLE LOCAL 64-bit */
  BFD_MACH_O_LC_SEGMENT_64 = 0x19,	/* 64-bit segment of this file to be 
                                           mapped.  */
  BFD_MACH_O_LC_ROUTINES_64 = 0x1a,      /* Address of the dyld init routine 
                                            in a dylib.  */
  BFD_MACH_O_LC_UUID = 0x1b,             /* 128-bit UUID of the executable.  */
  BFD_MACH_O_LC_RPATH = 0x1c | BFD_MACH_O_LC_REQ_DYLD,  
  BFD_MACH_O_LC_CODE_SIGNATURE = 0x1d,   
  BFD_MACH_O_LC_SEGMENT_SPLIT_INFO = 0x1e, 
  BFD_MACH_O_LC_REEXPORT_DYLIB = 0x1f | BFD_MACH_O_LC_REQ_DYLD,
  BFD_MACH_O_LC_LAZY_LOAD_DYLIB = 0x20,  /* delay load of dylib until first use */
  BFD_MACH_O_LC_ENCRYPTION_INFO = 0x21,  /* encrypted segment information */
  BFD_MACH_O_LC_DYLD_INFO = 0x22,        /* compressed dyld information */
  BFD_MACH_O_LC_DYLD_INFO_ONLY = 0x22 | BFD_MACH_O_LC_REQ_DYLD  /* compressed dyld information only */
}
bfd_mach_o_load_command_type;

#define BFD_MACH_O_CPU_IS64BIT 0x1000000

typedef enum bfd_mach_o_cpu_type
{
  BFD_MACH_O_CPU_TYPE_VAX = 1,
  BFD_MACH_O_CPU_TYPE_MC680x0 = 6,
  BFD_MACH_O_CPU_TYPE_I386 = 7,
  BFD_MACH_O_CPU_TYPE_MIPS = 8,
  BFD_MACH_O_CPU_TYPE_MC98000 = 10,
  BFD_MACH_O_CPU_TYPE_HPPA = 11,
  BFD_MACH_O_CPU_TYPE_ARM = 12,
  BFD_MACH_O_CPU_TYPE_MC88000 = 13,
  BFD_MACH_O_CPU_TYPE_SPARC = 14,
  BFD_MACH_O_CPU_TYPE_I860 = 15,
  BFD_MACH_O_CPU_TYPE_ALPHA = 16,
  BFD_MACH_O_CPU_TYPE_POWERPC = 18
  /* APPLE LOCAL 64-bit */
  , BFD_MACH_O_CPU_TYPE_POWERPC_64 = (18 | BFD_MACH_O_CPU_IS64BIT)
  /* APPLE LOCAL x86_64 */
  , BFD_MACH_O_CPU_TYPE_X86_64 = (BFD_MACH_O_CPU_TYPE_I386 | BFD_MACH_O_CPU_IS64BIT)
}
bfd_mach_o_cpu_type;

typedef enum bfd_mach_o_cpu_subtype
  {
    BFD_MACH_O_CPU_SUBTYPE_POWERPC_ALL = 0,
    BFD_MACH_O_CPU_SUBTYPE_ARM_4T = 5,
    BFD_MACH_O_CPU_SUBTYPE_ARM_6 = 6,
    BFD_MACH_O_CPU_SUBTYPE_ARM_7 = 9,
    BFD_MACH_O_CPU_SUBTYPE_POWERPC_970 = 100
  }
bfd_mach_o_cpu_subtype;

typedef enum bfd_mach_o_filetype
{
  BFD_MACH_O_MH_OBJECT = 1,
  BFD_MACH_O_MH_EXECUTE = 2,
  BFD_MACH_O_MH_FVMLIB = 3,
  BFD_MACH_O_MH_CORE = 4,
  BFD_MACH_O_MH_PRELOAD = 5,
  BFD_MACH_O_MH_DYLIB = 6,
  BFD_MACH_O_MH_DYLINKER = 7,
  BFD_MACH_O_MH_BUNDLE = 8,
  BFD_MACH_O_MH_DYLIB_STUB = 9,
  BFD_MACH_O_MH_DSYM = 10,
  BFD_MACH_O_MH_BUNDLE_KEXT = 11
}
bfd_mach_o_filetype;

/* Constants for the type of a section.  */

typedef enum bfd_mach_o_section_type
{
  /* Regular section.  */
  BFD_MACH_O_S_REGULAR = 0x0,

  /* Zero fill on demand section.  */
  BFD_MACH_O_S_ZEROFILL = 0x1,

  /* Section with only literal C strings.  */
  BFD_MACH_O_S_CSTRING_LITERALS = 0x2,

  /* Section with only 4 byte literals.  */
  BFD_MACH_O_S_4BYTE_LITERALS = 0x3,

  /* Section with only 8 byte literals.  */
  BFD_MACH_O_S_8BYTE_LITERALS = 0x4,

  /* Section with only pointers to literals.  */
  BFD_MACH_O_S_LITERAL_POINTERS = 0x5,

  /* For the two types of symbol pointers sections and the symbol stubs
     section they have indirect symbol table entries.  For each of the
     entries in the section the indirect symbol table entries, in
     corresponding order in the indirect symbol table, start at the index
     stored in the reserved1 field of the section structure.  Since the
     indirect symbol table entries correspond to the entries in the
     section the number of indirect symbol table entries is inferred from
     the size of the section divided by the size of the entries in the
     section.  For symbol pointers sections the size of the entries in
     the section is 4 bytes and for symbol stubs sections the byte size
     of the stubs is stored in the reserved2 field of the section
     structure.  */

  /* Section with only non-lazy symbol pointers.  */
  BFD_MACH_O_S_NON_LAZY_SYMBOL_POINTERS = 0x6,

  /* Section with only lazy symbol pointers.  */
  BFD_MACH_O_S_LAZY_SYMBOL_POINTERS = 0x7,

  /* Section with only symbol stubs, byte size of stub in the reserved2 field.  */
  BFD_MACH_O_S_SYMBOL_STUBS = 0x8,

  /* Section with only function pointers for initialization.  */
  BFD_MACH_O_S_MOD_INIT_FUNC_POINTERS = 0x9
  /* APPLE LOCAL begin Mach-O */
  /* Section with only function pointers for termination. */
  , BFD_MACH_O_S_MOD_TERM_FUNC_POINTERS = 0xa,
  /* Section contains symbols that are to be coalesced. */
  BFD_MACH_O_S_COALESCED = 0xb,
  /* zero fill on demand section (that can be larger than 4 gigabytes) */
  BFD_MACH_O_S_GB_ZEROFILL = 0xd,
  /* a debug section. */
  BFD_MACH_O_S_ATTR_DEBUG = 0x02000000, 
  /* APPLE LOCAL end Mach-O */
}
bfd_mach_o_section_type;

#define BFD_MACH_O_SECTION_TYPE_MASK 0x000000ff
#define BFD_MACH_O_SECTION_ATTRIBUTES_MASK 0xffffff00

typedef struct bfd_mach_o_header
{
  unsigned long magic;
  unsigned long cputype;
  unsigned long cpusubtype;
  unsigned long filetype;
  unsigned long ncmds;
  unsigned long sizeofcmds;
  unsigned long flags;
  unsigned int reserved;
  unsigned int version;
  enum bfd_endian byteorder;
}
bfd_mach_o_header;

typedef struct bfd_mach_o_section
{
  asection *bfdsection;
  char sectname[16 + 1];
  char segname[16 + 1];
  bfd_vma addr;
  bfd_vma size;
  bfd_vma offset;
  unsigned long align;
  bfd_vma reloff;
  unsigned long nreloc;
  unsigned long flags;
  unsigned long reserved1;
  unsigned long reserved2;
  unsigned long reserved3;
}
bfd_mach_o_section;

typedef struct bfd_mach_o_segment_command
{
  char segname[16];
  bfd_vma vmaddr;
  bfd_vma vmsize;
  bfd_vma fileoff;
  bfd_vma filesize;
  unsigned long maxprot;
  unsigned long initprot;
  unsigned long nsects;
  unsigned long flags;
  bfd_mach_o_section *sections;
  asection *segment;
}
bfd_mach_o_segment_command;

typedef struct bfd_mach_o_symtab_command
{
  unsigned long symoff;
  unsigned long nsyms;
  unsigned long stroff;
  unsigned long strsize;
  asymbol *symbols;
  char *strtab;
  asection *stabs_segment;
  asection *stabstr_segment;
}
bfd_mach_o_symtab_command;

/* This is the second set of the symbolic information which is used to support
   the data structures for the dynamically link editor.

   The original set of symbolic information in the symtab_command which contains
   the symbol and string tables must also be present when this load command is
   present.  When this load command is present the symbol table is organized
   into three groups of symbols:
       local symbols (static and debugging symbols) - grouped by module
       defined external symbols - grouped by module (sorted by name if not lib)
       undefined external symbols (sorted by name)
   In this load command there are offsets and counts to each of the three groups
   of symbols.

   This load command contains the offsets and sizes of the following new
   symbolic information tables:
       table of contents
       module table
       reference symbol table
       indirect symbol table
   The first three tables above (the table of contents, module table and
   reference symbol table) are only present if the file is a dynamically linked
   shared library.  For executable and object modules, which are files
   containing only one module, the information that would be in these three
   tables is determined as follows:
       table of contents - the defined external symbols are sorted by name
       module table - the file contains only one module so everything in the
                      file is part of the module.
       reference symbol table - is the defined and undefined external symbols

   For dynamically linked shared library files this load command also contains
   offsets and sizes to the pool of relocation entries for all sections
   separated into two groups:
       external relocation entries
       local relocation entries
   For executable and object modules the relocation entries continue to hang
   off the section structures.  */

typedef struct bfd_mach_o_dysymtab_command
{
  /* The symbols indicated by symoff and nsyms of the LC_SYMTAB load command
     are grouped into the following three groups:
       local symbols (further grouped by the module they are from)
       defined external symbols (further grouped by the module they are from)
       undefined symbols

     The local symbols are used only for debugging.  The dynamic binding
     process may have to use them to indicate to the debugger the local
     symbols for a module that is being bound.

     The last two groups are used by the dynamic binding process to do the
     binding (indirectly through the module table and the reference symbol
     table when this is a dynamically linked shared library file).  */

  unsigned long ilocalsym;    /* Index to local symbols.  */
  unsigned long nlocalsym;    /* Number of local symbols.  */
  unsigned long iextdefsym;   /* Index to externally defined symbols.  */
  unsigned long nextdefsym;   /* Number of externally defined symbols.  */
  unsigned long iundefsym;    /* Index to undefined symbols.  */
  unsigned long nundefsym;    /* Number of undefined symbols.  */

  /* For the dynamic binding process to find which module a symbol
     is defined in the table of contents is used (analogous to the ranlib
     structure in an archive) which maps defined external symbols to modules
     they are defined in.  This exists only in a dynamically linked shared
     library file.  For executable and object modules the defined external
     symbols are sorted by name and are used as the table of contents.  */

  unsigned long tocoff;       /* File offset to table of contents.  */
  unsigned long ntoc;         /* Number of entries in table of contents.  */

  /* To support dynamic binding of "modules" (whole object files) the symbol
     table must reflect the modules that the file was created from.  This is
     done by having a module table that has indexes and counts into the merged
     tables for each module.  The module structure that these two entries
     refer to is described below.  This exists only in a dynamically linked
     shared library file.  For executable and object modules the file only
     contains one module so everything in the file belongs to the module.  */

  unsigned long modtaboff;    /* File offset to module table.  */
  unsigned long nmodtab;      /* Number of module table entries.  */

  /* To support dynamic module binding the module structure for each module
     indicates the external references (defined and undefined) each module
     makes.  For each module there is an offset and a count into the
     reference symbol table for the symbols that the module references.
     This exists only in a dynamically linked shared library file.  For
     executable and object modules the defined external symbols and the
     undefined external symbols indicate the external references.  */

  unsigned long extrefsymoff;  /* Offset to referenced symbol table.  */
  unsigned long nextrefsyms;   /* Number of referenced symbol table entries.  */

  /* The sections that contain "symbol pointers" and "routine stubs" have
     indexes and (implied counts based on the size of the section and fixed
     size of the entry) into the "indirect symbol" table for each pointer
     and stub.  For every section of these two types the index into the
     indirect symbol table is stored in the section header in the field
     reserved1.  An indirect symbol table entry is simply a 32bit index into
     the symbol table to the symbol that the pointer or stub is referring to.
     The indirect symbol table is ordered to match the entries in the section.  */

  unsigned long indirectsymoff; /* File offset to the indirect symbol table.  */
  unsigned long nindirectsyms;  /* Number of indirect symbol table entries.  */

  /* To support relocating an individual module in a library file quickly the
     external relocation entries for each module in the library need to be
     accessed efficiently.  Since the relocation entries can't be accessed
     through the section headers for a library file they are separated into
     groups of local and external entries further grouped by module.  In this
     case the presents of this load command whose extreloff, nextrel,
     locreloff and nlocrel fields are non-zero indicates that the relocation
     entries of non-merged sections are not referenced through the section
     structures (and the reloff and nreloc fields in the section headers are
     set to zero).

     Since the relocation entries are not accessed through the section headers
     this requires the r_address field to be something other than a section
     offset to identify the item to be relocated.  In this case r_address is
     set to the offset from the vmaddr of the first LC_SEGMENT command.

     The relocation entries are grouped by module and the module table
     entries have indexes and counts into them for the group of external
     relocation entries for that the module.

     For sections that are merged across modules there must not be any
     remaining external relocation entries for them (for merged sections
     remaining relocation entries must be local).  */

  unsigned long extreloff;    /* Offset to external relocation entries.  */
  unsigned long nextrel;      /* Number of external relocation entries.  */

  /* All the local relocation entries are grouped together (they are not
     grouped by their module since they are only used if the object is moved
     from it statically link edited address).  */

  unsigned long locreloff;    /* Offset to local relocation entries.  */
  unsigned long nlocrel;      /* Number of local relocation entries.  */
}
bfd_mach_o_dysymtab_command;

/* An indirect symbol table entry is simply a 32bit index into the symbol table
   to the symbol that the pointer or stub is refering to.  Unless it is for a
   non-lazy symbol pointer section for a defined symbol which strip(1) as
   removed.  In which case it has the value INDIRECT_SYMBOL_LOCAL.  If the
   symbol was also absolute INDIRECT_SYMBOL_ABS is or'ed with that.  */

#define INDIRECT_SYMBOL_LOCAL 0x80000000
#define INDIRECT_SYMBOL_ABS   0x40000000

typedef struct bfd_mach_o_thread_flavour
{
  unsigned long flavour;
  bfd_vma offset;
  unsigned long size;
}
bfd_mach_o_thread_flavour;

typedef struct bfd_mach_o_thread_command
{
  unsigned long nflavours;
  bfd_mach_o_thread_flavour *flavours;
  asection *section;
}
bfd_mach_o_thread_command;

typedef struct bfd_mach_o_dylinker_command
{
  unsigned long cmd;                   /* LC_ID_DYLIB or LC_LOAD_DYLIB.  */
  unsigned long cmdsize;               /* Includes pathname string.  */
  unsigned long name_offset;           /* Offset to library's path name.  */
  unsigned long name_len;              /* Offset to library's path name.  */
  asection *section;
}
bfd_mach_o_dylinker_command;

typedef struct bfd_mach_o_dylib_command
{
  unsigned long cmd;                   /* LC_ID_DYLIB or LC_LOAD_DYLIB.  */
  unsigned long cmdsize;               /* Includes pathname string.  */
  unsigned long name_offset;           /* Offset to library's path name.  */
  unsigned long name_len;              /* Offset to library's path name.  */
  unsigned long timestamp;	       /* Library's build time stamp.  */
  unsigned long current_version;       /* Library's current version number.  */
  unsigned long compatibility_version; /* Library's compatibility vers number.  */
  asection *section;
}
bfd_mach_o_dylib_command;

typedef struct bfd_mach_o_prebound_dylib_command
{
  unsigned long cmd;                 /* LC_PREBOUND_DYLIB.  */
  unsigned long cmdsize;             /* Includes strings.  */
  unsigned long name;                /* Library's path name.  */
  unsigned long nmodules;            /* Number of modules in library.  */
  unsigned long linked_modules;      /* Bit vector of linked modules.  */
  asection *section;
}
bfd_mach_o_prebound_dylib_command;

typedef struct bfd_mach_o_load_command
{
  bfd_mach_o_load_command_type type;
  unsigned int type_required;
  bfd_vma offset;
  bfd_vma len;
  union
  {
    bfd_mach_o_segment_command segment;
    bfd_mach_o_symtab_command symtab;
    bfd_mach_o_dysymtab_command dysymtab;
    bfd_mach_o_thread_command thread;
    bfd_mach_o_dylib_command dylib;
    bfd_mach_o_dylinker_command dylinker;
    bfd_mach_o_prebound_dylib_command prebound_dylib;
  }
  command;
}
bfd_mach_o_load_command;

typedef struct mach_o_data_struct
{
  bfd_mach_o_header header;
  bfd_mach_o_load_command *commands;
  unsigned long nsymbols;
  asymbol *symbols;
  unsigned long nsects;
  bfd_mach_o_section **sections;
  bfd *ibfd;
  unsigned char uuid[16];
  int scanning_load_cmds;
  int encrypted;
}
mach_o_data_struct;

typedef struct mach_o_data_struct bfd_mach_o_data_struct;

/* APPLE LOCAL  Mach-O */
unsigned int bfd_mach_o_version (bfd *);
int bfd_mach_o_stub_library (bfd *);
bfd_boolean bfd_mach_o_encrypted_binary (bfd *);
/* APPLE LOCAL shared cache  */
bfd_boolean bfd_mach_o_in_shared_cached_memory (bfd *);

bfd_boolean        bfd_mach_o_valid  (bfd *);
int                bfd_mach_o_scan_read_symtab_symbol        (bfd *, bfd_mach_o_symtab_command *, asymbol *, unsigned long);
int                bfd_mach_o_scan_read_symtab_strtab        (bfd *, bfd_mach_o_symtab_command *);
int                bfd_mach_o_scan_read_symtab_symbols       (bfd *, bfd_mach_o_symtab_command *);
int                bfd_mach_o_scan_read_dysymtab_symbol      (bfd *, bfd_mach_o_dysymtab_command *, bfd_mach_o_symtab_command *, asymbol *, unsigned long);
int                bfd_mach_o_scan_start_address             (bfd *);
int                bfd_mach_o_scan                           (bfd *, bfd_mach_o_header *, bfd_mach_o_data_struct *);
bfd_boolean        bfd_mach_o_mkobject                       (bfd *);
const bfd_target * bfd_mach_o_object_p                       (bfd *);
const bfd_target * bfd_mach_o_core_p                         (bfd *);
const bfd_target * bfd_mach_o_archive_p                      (bfd *);
bfd *              bfd_mach_o_openr_next_archived_file       (bfd *, bfd *);
int                bfd_mach_o_lookup_section                 (bfd *, asection *, bfd_mach_o_load_command **, bfd_mach_o_section **);
int                bfd_mach_o_lookup_command                 (bfd *, bfd_mach_o_load_command_type, bfd_mach_o_load_command **);
unsigned long      bfd_mach_o_stack_addr                     (enum bfd_mach_o_cpu_type);
bfd_boolean        bfd_mach_o_core_fetch_environment         (bfd *, unsigned char **, bfd_size_type *);
char *             bfd_mach_o_core_file_failing_command      (bfd *);
int                bfd_mach_o_core_file_failing_signal       (bfd *);
bfd_boolean        bfd_mach_o_core_file_matches_executable_p (bfd *, bfd *);
bfd_boolean        bfd_mach_o_get_uuid                       (bfd *, unsigned char* buf, unsigned long buf_len);
unsigned int       bfd_mach_o_flavour_from_string            (unsigned long cputype, const char* s);

extern const bfd_target mach_o_be_vec;
extern const bfd_target mach_o_le_vec;
extern const bfd_target mach_o_fat_vec;

#endif /* _BFD_MACH_O_H_ */

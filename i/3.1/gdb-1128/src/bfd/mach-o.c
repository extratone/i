/* Mach-O support for BFD.
   Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005
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

#include "mach-o.h"
#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "libiberty.h"
#include <ctype.h>

#ifndef BFD_IO_FUNCS
#define BFD_IO_FUNCS 0
#endif

#define bfd_mach_o_mkarchive                          _bfd_noarchive_mkarchive
#define bfd_mach_o_read_ar_hdr                        _bfd_noarchive_read_ar_hdr
#define bfd_mach_o_slurp_armap                        _bfd_noarchive_slurp_armap
#define bfd_mach_o_slurp_extended_name_table          _bfd_noarchive_slurp_extended_name_table
#define bfd_mach_o_construct_extended_name_table      _bfd_noarchive_construct_extended_name_table
#define bfd_mach_o_truncate_arname                    _bfd_noarchive_truncate_arname
#define bfd_mach_o_write_armap                        _bfd_noarchive_write_armap
#define bfd_mach_o_get_elt_at_index                   _bfd_noarchive_get_elt_at_index
#define bfd_mach_o_generic_stat_arch_elt              _bfd_noarchive_generic_stat_arch_elt
#define bfd_mach_o_update_armap_timestamp             _bfd_noarchive_update_armap_timestamp
#define bfd_mach_o_close_and_cleanup                  _bfd_generic_close_and_cleanup
#define bfd_mach_o_new_section_hook                   _bfd_generic_new_section_hook
#define bfd_mach_o_get_section_contents_in_window     _bfd_generic_get_section_contents_in_window
#define bfd_mach_o_get_section_contents_in_window_with_mode _bfd_generic_get_section_contents_in_window_with_mode
#define bfd_mach_o_bfd_is_target_special_symbol       ((bfd_boolean (*) (bfd *, asymbol *)) bfd_false)
#define bfd_mach_o_bfd_is_local_label_name            _bfd_nosymbols_bfd_is_local_label_name
#define bfd_mach_o_get_lineno                         _bfd_nosymbols_get_lineno
#define bfd_mach_o_find_nearest_line                  _bfd_nosymbols_find_nearest_line
#define bfd_mach_o_find_inliner_info                  _bfd_nosymbols_find_inliner_info
#define bfd_mach_o_bfd_make_debug_symbol              _bfd_nosymbols_bfd_make_debug_symbol
#define bfd_mach_o_read_minisymbols                   _bfd_generic_read_minisymbols
#define bfd_mach_o_minisymbol_to_symbol               _bfd_generic_minisymbol_to_symbol
#define bfd_mach_o_get_reloc_upper_bound              _bfd_norelocs_get_reloc_upper_bound
#define bfd_mach_o_canonicalize_reloc                 _bfd_norelocs_canonicalize_reloc
#define bfd_mach_o_bfd_reloc_type_lookup              _bfd_norelocs_bfd_reloc_type_lookup
#define bfd_mach_o_bfd_get_relocated_section_contents bfd_generic_get_relocated_section_contents
#define bfd_mach_o_bfd_relax_section                  bfd_generic_relax_section
#define bfd_mach_o_bfd_link_hash_table_create         _bfd_generic_link_hash_table_create
#define bfd_mach_o_bfd_link_hash_table_free           _bfd_generic_link_hash_table_free
#define bfd_mach_o_bfd_link_add_symbols               _bfd_generic_link_add_symbols
#define bfd_mach_o_bfd_link_just_syms                 _bfd_generic_link_just_syms
#define bfd_mach_o_bfd_final_link                     _bfd_generic_final_link
#define bfd_mach_o_bfd_link_split_section             _bfd_generic_link_split_section
#define bfd_mach_o_set_arch_mach                      bfd_default_set_arch_mach
#define bfd_mach_o_bfd_merge_private_bfd_data         _bfd_generic_bfd_merge_private_bfd_data
#define bfd_mach_o_bfd_set_private_flags              _bfd_generic_bfd_set_private_flags
#define bfd_mach_o_bfd_print_private_bfd_data         _bfd_generic_bfd_print_private_bfd_data
#define bfd_mach_o_get_section_contents               _bfd_generic_get_section_contents
#define bfd_mach_o_set_section_contents               _bfd_generic_set_section_contents
#define bfd_mach_o_bfd_gc_sections                    bfd_generic_gc_sections
#define bfd_mach_o_bfd_merge_sections                 bfd_generic_merge_sections
#define bfd_mach_o_bfd_is_group_section               bfd_generic_is_group_section
#define bfd_mach_o_bfd_discard_group                  bfd_generic_discard_group
#define bfd_mach_o_section_already_linked             _bfd_generic_section_already_linked
#define bfd_mach_o_bfd_copy_private_header_data       _bfd_generic_bfd_copy_private_header_data


/* The flags field of a section structure is separated into two parts a section
   type and section attributes.  The section types are mutually exclusive (it
   can only have one type) but the section attributes are not (it may have more
   than one attribute).  */

#define SECTION_TYPE             0x000000ff     /* 256 section types.  */
#define SECTION_ATTRIBUTES       0xffffff00     /*  24 section attributes.  */

/* Constants for the section attributes part of the flags field of a section
   structure.  */

#define SECTION_ATTRIBUTES_USR   0xff000000     /* User-settable attributes.  */
#define S_ATTR_PURE_INSTRUCTIONS 0x80000000     /* Section contains only true machine instructions.  */
#define SECTION_ATTRIBUTES_SYS   0x00ffff00     /* System setable attributes.  */
#define S_ATTR_SOME_INSTRUCTIONS 0x00000400     /* Section contains some machine instructions.  */
#define S_ATTR_EXT_RELOC         0x00000200     /* Section has external relocation entries.  */
#define S_ATTR_LOC_RELOC         0x00000100     /* Section has local relocation entries.  */

#define N_STAB 0xe0
#define N_TYPE 0x1e
#define N_EXT  0x01

#define N_UNDF 0x0
#define N_ABS  0x2
#define N_TEXT 0x4
#define N_DATA 0x6
#define N_BSS 0x8
#define N_SECT 0xe
#define N_INDR 0xa

unsigned int 
bfd_mach_o_version (abfd)
     bfd *abfd;
{
  bfd_mach_o_data_struct *mdata = NULL;

  BFD_ASSERT (bfd_mach_o_valid (abfd));
  mdata = abfd->tdata.mach_o_data;

  return mdata->header.version;
}

/* APPLE LOCAL shared cache begin
   If a mach image is in the shared cache is being read straight from memory
   or from the shared cache file itself, the msbit of FLAGS will be set to
   1.  */
bfd_boolean
bfd_mach_o_in_shared_cached_memory (bfd *abfd)
{
  bfd_mach_o_data_struct *mdata = NULL;
  BFD_ASSERT (bfd_mach_o_valid (abfd));
  mdata = abfd->tdata.mach_o_data;
  if (mdata->header.flags & 0x80000000)
    return 1;
  return 0;
}
/* APPLE LOCAL shared cache end  */

bfd_boolean
bfd_mach_o_valid (bfd *abfd)
{
  if (abfd == NULL || abfd->xvec == NULL)
    return 0;

  if (! ((abfd->xvec == &mach_o_be_vec)
	 || (abfd->xvec == &mach_o_le_vec)
	 || (abfd->xvec == &mach_o_fat_vec)))
    return 0;

  if (abfd->tdata.mach_o_data == NULL)
    return 0;
  return 1;
}

/* Copy any private info we understand from the input symbol
   to the output symbol.  */

static bfd_boolean
bfd_mach_o_bfd_copy_private_symbol_data (bfd *ibfd ATTRIBUTE_UNUSED,
					 asymbol *isymbol ATTRIBUTE_UNUSED,
					 bfd *obfd ATTRIBUTE_UNUSED,
					 asymbol *osymbol ATTRIBUTE_UNUSED)
{
  return TRUE;
}

/* Copy any private info we understand from the input section
   to the output section.  */

static bfd_boolean
bfd_mach_o_bfd_copy_private_section_data (bfd *ibfd ATTRIBUTE_UNUSED,
					  asection *isection ATTRIBUTE_UNUSED,
					  bfd *obfd ATTRIBUTE_UNUSED,
					  asection *osection ATTRIBUTE_UNUSED)
{
  return TRUE;
}

/* Copy any private info we understand from the input bfd
   to the output bfd.  */

static bfd_boolean
bfd_mach_o_bfd_copy_private_bfd_data (bfd *ibfd, bfd *obfd)
{
  BFD_ASSERT (bfd_mach_o_valid (ibfd));
  BFD_ASSERT (bfd_mach_o_valid (obfd));

  obfd->tdata.mach_o_data = ibfd->tdata.mach_o_data;
  obfd->tdata.mach_o_data->ibfd = ibfd;
  return TRUE;
}

static long
bfd_mach_o_count_symbols (bfd *abfd)
{
  bfd_mach_o_data_struct *mdata = NULL;
  long nsyms = 0;
  unsigned long i;

  BFD_ASSERT (bfd_mach_o_valid (abfd));
  mdata = abfd->tdata.mach_o_data;

  for (i = 0; i < mdata->header.ncmds; i++)
    if (mdata->commands[i].type == BFD_MACH_O_LC_SYMTAB)
      {
	bfd_mach_o_symtab_command *sym = &mdata->commands[i].command.symtab;
	nsyms += sym->nsyms;
      }

  return nsyms;
}

static long
bfd_mach_o_get_symtab_upper_bound (bfd *abfd)
{
  long nsyms = bfd_mach_o_count_symbols (abfd);

  if (nsyms < 0)
    return nsyms;

  return ((nsyms + 1) * sizeof (asymbol *));
}

static long
bfd_mach_o_canonicalize_symtab (bfd *abfd, asymbol **alocation)
{
  bfd_mach_o_data_struct *mdata = abfd->tdata.mach_o_data;
  long nsyms = bfd_mach_o_count_symbols (abfd);
  asymbol **csym = alocation;
  unsigned long i, j;

  if (nsyms < 0)
    return nsyms;

  for (i = 0; i < mdata->header.ncmds; i++)
    {
      if (mdata->commands[i].type == BFD_MACH_O_LC_SYMTAB)
	{
	  bfd_mach_o_symtab_command *sym = &mdata->commands[i].command.symtab;

	  if (bfd_mach_o_scan_read_symtab_symbols (abfd, &mdata->commands[i].command.symtab) != 0)
	    {
	      fprintf (stderr, "bfd_mach_o_canonicalize_symtab: unable to load symbols for section %lu\n", i);
	      return 0;
	    }

	  BFD_ASSERT (sym->symbols != NULL);

	  for (j = 0; j < sym->nsyms; j++)
	    {
	      BFD_ASSERT (csym < (alocation + nsyms));
	      *csym++ = &sym->symbols[j];
	    }
	}
    }

  *csym++ = NULL;

  return nsyms;
}

#define BFD_GETB16(addr) ((addr[0] << 8) | addr[1])
#define BFD_GETB32(addr) ((((((unsigned long) addr[0] << 8) | addr[1]) << 8) | addr[2]) << 8 | addr[3])
#define BFD_GETB64(addr) ((((((((((unsigned long long) addr[0] << 8) | addr[1]) << 8) | addr[2]) << 8 | addr[3]) << 8 | addr[4]) << 8 | addr[5]) << 8 | addr[6]) << 8 | addr[7])
#define BFD_GETL16(addr) ((addr[1] << 8) | addr[0])
#define BFD_GETL32(addr) ((((((unsigned long) addr[3] << 8) | addr[2]) << 8) | addr[1]) << 8 | addr[0])
#define BFD_GETL64(addr) ((((((((((unsigned long long) addr[7] << 8) | addr[6]) << 8) | addr[5]) << 8 | addr[4]) << 8 | addr[3]) << 8 | addr[2]) << 8 | addr[1]) << 8 | addr[0])

unsigned char macosx_symbol_types[256];

static unsigned char
bfd_mach_o_symbol_type_base (unsigned char macho_type)
{
  unsigned char mtype = macho_type;
  unsigned char ntype = 0;

  if (macho_type & BFD_MACH_O_N_STAB)
    {
      return macho_type;
    }

  if (mtype & BFD_MACH_O_N_PEXT)
    {
      mtype &= ~BFD_MACH_O_N_PEXT;
      ntype |= N_EXT;
    }

  if (mtype & BFD_MACH_O_N_EXT)
    {
      mtype &= ~BFD_MACH_O_N_EXT;
      ntype |= N_EXT;
    }

  switch (mtype & BFD_MACH_O_N_TYPE)
    {
    case BFD_MACH_O_N_SECT:
      /* should add section here */
      break;

    case BFD_MACH_O_N_PBUD:
      ntype |= N_UNDF;
      break;

    case BFD_MACH_O_N_ABS:
      ntype |= N_ABS;
      break;

    case BFD_MACH_O_N_UNDF:
      ntype |= N_UNDF;
      break;

    case BFD_MACH_O_N_INDR:
      /* complain (&unsupported_indirect_symtype_complaint, local_hex_string (macho_type)); */
      return macho_type;

    default:
      /* complain (&unknown_macho_symtype_complaint, local_hex_string (macho_type)); */
      return macho_type;
    }
  mtype &= ~BFD_MACH_O_N_TYPE;

  BFD_ASSERT (mtype == 0);

  return ntype;
}

static void
bfd_mach_o_symbol_types_init (void)
{
  unsigned int i;
  for (i = 0; i < 256; i++)
    {
      macosx_symbol_types[i] = bfd_mach_o_symbol_type_base (i);
    }
}

static unsigned char
bfd_mach_o_symbol_type (bfd *abfd, unsigned char macho_type, unsigned char macho_sect)
{
  static int init = 0;
  
  if (! init)
    bfd_mach_o_symbol_types_init ();
  init = 1;
  
  unsigned char ntype = macosx_symbol_types[macho_type];

  /* If the symbol refers to a section, modify ntype based on the value of macho_sect. */

  if ((macho_type & BFD_MACH_O_N_TYPE) == BFD_MACH_O_N_SECT)
    {
      if (macho_sect == 1)
        {
          /* Section 1 is always the text segment. */
          ntype |= N_TEXT;
        }

      else if ((macho_sect > 0)
               && (macho_sect <= abfd->tdata.mach_o_data->nsects))
        {
          const bfd_mach_o_section *sect =
            abfd->tdata.mach_o_data->sections[macho_sect - 1];

          if (sect == NULL)
            {
              /* complain (&unknown_macho_section_complaint, local_hex_string (macho_sect)); */
            }
          else if ((sect->segname != NULL)
                   && (strcmp (sect->segname, "__DATA") == 0))
            {
              if ((sect->sectname != NULL)
                  && (strcmp (sect->sectname, "__bss") == 0))
                ntype |= N_BSS;
              else
                ntype |= N_DATA;
            }
          else if ((sect->segname != NULL)
                   && (strcmp (sect->segname, "__TEXT") == 0))
            {
              ntype |= N_TEXT;
            }
          else
            {
              /* complain (&unknown_macho_section_complaint, local_hex_string (macho_sect)); */
              ntype |= N_DATA;
            }
        }

      else
        {
          /* complain (&unknown_macho_section_complaint, local_hex_string (macho_sect)); */
          ntype |= N_DATA;
        }
    }

  /* All modifications are done; return the computed type code. */

  return ntype;
}

static void
bfd_mach_o_get_symbol_info (bfd *abfd,
			    asymbol *symbol,
			    symbol_info *ret)
{
  unsigned int type, section, desc;

  type = (symbol->udata.i >> 24) & 0xff;
  section = (symbol->udata.i >> 16) & 0xff;
  desc = (symbol->udata.i >> 0) & 0xffff;
      
  bfd_symbol_info (symbol, ret);

  if (type & BFD_MACH_O_N_STAB)
    {
      int type_code;
      static char buf[10];

      type_code = bfd_mach_o_symbol_type (abfd, type, section);

      const char *stab_name = bfd_get_stab_name (type_code);
      if (stab_name == NULL)
	{
	  sprintf (buf, "(%d)", type_code);
	  stab_name = buf;
	}

      ret->type = '-';
      ret->stab_type = type_code;
      ret->stab_other = 0;
      ret->stab_desc = desc;
      ret->stab_name = stab_name;
    }
}

static void
bfd_mach_o_print_symbol (bfd *abfd,
			 PTR afile,
			 asymbol *symbol,
			 bfd_print_symbol_type how)
{
  FILE *file = (FILE *) afile;

  switch (how)
    {
    case bfd_print_symbol_name:
      fprintf (file, "%s", symbol->name);
      break;
    default:
      bfd_print_symbol_vandf (abfd, (PTR) file, symbol);
      fprintf (file, " %-5s %s", symbol->section->name, symbol->name);
    }
}

static void
bfd_mach_o_convert_architecture (bfd_mach_o_cpu_type mtype,
				 bfd_mach_o_cpu_subtype msubtype ATTRIBUTE_UNUSED,
				 enum bfd_architecture *type,
				 unsigned long *subtype)
{
  *subtype = bfd_arch_unknown;

  switch (mtype)
    {
    case BFD_MACH_O_CPU_TYPE_VAX: *type = bfd_arch_vax; break;
    case BFD_MACH_O_CPU_TYPE_MC680x0: *type = bfd_arch_m68k; break;
    case BFD_MACH_O_CPU_TYPE_I386:
      *type = bfd_arch_i386;
      *subtype = bfd_mach_i386_i386;
      break;
    /* APPLE LOCAL begin x86_64 */
    case BFD_MACH_O_CPU_TYPE_X86_64:
      *type = bfd_arch_i386;
      *subtype = bfd_mach_x86_64;
      break;
    /* APPLE LOCAL end x86_64 */
    case BFD_MACH_O_CPU_TYPE_MIPS: *type = bfd_arch_mips; break;
    case BFD_MACH_O_CPU_TYPE_MC98000: *type = bfd_arch_m98k; break;
    case BFD_MACH_O_CPU_TYPE_HPPA: *type = bfd_arch_hppa; break;
    case BFD_MACH_O_CPU_TYPE_ARM: 
      *type = bfd_arch_arm;
      if (msubtype == BFD_MACH_O_CPU_SUBTYPE_ARM_4T)
	*subtype = bfd_mach_arm_4T;
      else if (msubtype == BFD_MACH_O_CPU_SUBTYPE_ARM_6)
	*subtype = bfd_mach_arm_6;
      else if (msubtype == BFD_MACH_O_CPU_SUBTYPE_ARM_7)
	*subtype = bfd_mach_arm_7;
      break;
    case BFD_MACH_O_CPU_TYPE_MC88000: *type = bfd_arch_m88k; break;
    case BFD_MACH_O_CPU_TYPE_SPARC:
      *type = bfd_arch_sparc; 
      *subtype = bfd_mach_sparc; 
      break;
    case BFD_MACH_O_CPU_TYPE_I860: *type = bfd_arch_i860; break;
    case BFD_MACH_O_CPU_TYPE_ALPHA: *type = bfd_arch_alpha; break;
    case BFD_MACH_O_CPU_TYPE_POWERPC:
      *type = bfd_arch_powerpc;
      *subtype = bfd_mach_ppc; 
      break;
    case BFD_MACH_O_CPU_TYPE_POWERPC_64:
      *type = bfd_arch_powerpc;
      *subtype = bfd_mach_ppc64; 
      break;
    default:
      *type = bfd_arch_unknown;
      break;
    }
}

static int
bfd_mach_o_write_header (bfd *abfd, bfd_mach_o_header *header)
{
  unsigned char buf[32];
  unsigned int size;

  size = (header->version == 2) ? 32 : 28;

  bfd_h_put_32 (abfd, header->magic, buf + 0);
  bfd_h_put_32 (abfd, header->cputype, buf + 4);
  bfd_h_put_32 (abfd, header->cpusubtype, buf + 8);
  bfd_h_put_32 (abfd, header->filetype, buf + 12);
  bfd_h_put_32 (abfd, header->ncmds, buf + 16);
  bfd_h_put_32 (abfd, header->sizeofcmds, buf + 20);
  bfd_h_put_32 (abfd, header->flags, buf + 24);

  if (header->version == 2)
    bfd_h_put_32 (abfd, header->reserved, buf + 28);

  bfd_seek (abfd, 0, SEEK_SET);
  if (bfd_bwrite ((PTR) buf, size, abfd) != size)
    return -1;

  return 0;
}

static int
bfd_mach_o_scan_write_thread (bfd *abfd, bfd_mach_o_load_command *command)
{
  bfd_mach_o_thread_command *cmd = &command->command.thread;
  unsigned int i;
  unsigned char buf[8];
  bfd_vma offset;
  unsigned int nflavours;

  BFD_ASSERT ((command->type == BFD_MACH_O_LC_THREAD)
	      || (command->type == BFD_MACH_O_LC_UNIXTHREAD));

  offset = 8;
  nflavours = 0;
  for (i = 0; i < cmd->nflavours; i++)
    {
      BFD_ASSERT ((cmd->flavours[i].size % 4) == 0);
      BFD_ASSERT (cmd->flavours[i].offset == (command->offset + offset + 8));

      bfd_h_put_32 (abfd, cmd->flavours[i].flavour, buf);
      bfd_h_put_32 (abfd, (cmd->flavours[i].size / 4), buf + 4);

      bfd_seek (abfd, command->offset + offset, SEEK_SET);
      if (bfd_bwrite ((PTR) buf, 8, abfd) != 8)
	return -1;

      offset += cmd->flavours[i].size + 8;
    }

  return 0;
}

static int
bfd_mach_o_scan_write_section_32 (bfd *abfd,
				  bfd_mach_o_section *section,
				  bfd_vma offset)
{
  unsigned char buf[68];

  memcpy (buf, section->sectname, 16);
  memcpy (buf + 16, section->segname, 16);
  bfd_h_put_32 (abfd, section->addr, buf + 32);
  bfd_h_put_32 (abfd, section->size, buf + 36);
  bfd_h_put_32 (abfd, section->offset, buf + 40);
  bfd_h_put_32 (abfd, section->align, buf + 44);
  bfd_h_put_32 (abfd, section->reloff, buf + 48);
  bfd_h_put_32 (abfd, section->nreloc, buf + 52);
  bfd_h_put_32 (abfd, section->flags, buf + 56);
  bfd_h_put_32 (abfd, section->reserved1, buf + 60);
  bfd_h_put_32 (abfd, section->reserved2, buf + 64);

  bfd_seek (abfd, offset, SEEK_SET);
  if (bfd_bwrite ((PTR) buf, 68, abfd) != 68)
    return -1;

  return 0;
}

static int
bfd_mach_o_scan_write_section_64 (bfd *abfd,
				  bfd_mach_o_section *section,
				  bfd_vma offset)
{
  unsigned char buf[80];

  memcpy (buf, section->sectname, 16);
  memcpy (buf + 16, section->segname, 16);
  bfd_h_put_64 (abfd, section->addr, buf + 32);
  bfd_h_put_64 (abfd, section->size, buf + 40);
  bfd_h_put_32 (abfd, section->offset, buf + 48);
  bfd_h_put_32 (abfd, section->align, buf + 52);
  bfd_h_put_32 (abfd, section->reloff, buf + 56);
  bfd_h_put_32 (abfd, section->nreloc, buf + 60);
  bfd_h_put_32 (abfd, section->flags, buf + 64);
  bfd_h_put_32 (abfd, section->reserved1, buf + 68);
  bfd_h_put_32 (abfd, section->reserved2, buf + 72);
  bfd_h_put_32 (abfd, section->reserved3, buf + 76);

  bfd_seek (abfd, offset, SEEK_SET);
  if (bfd_bwrite ((PTR) buf, 80, abfd) != 80)
    return -1;

  return 0;
}

static int
bfd_mach_o_scan_write_section (bfd *abfd, 
			       bfd_mach_o_section *section,
			       bfd_vma offset,
			       unsigned int wide)
{
  if (wide)
    return bfd_mach_o_scan_write_section_64 (abfd, section, offset);
  else
    return bfd_mach_o_scan_write_section_32 (abfd, section, offset);
}

static int
bfd_mach_o_scan_write_segment (bfd *abfd,
			       bfd_mach_o_load_command *command,
			       unsigned int wide)
{
  unsigned char buf[64];
  bfd_mach_o_segment_command *seg = &command->command.segment;
  unsigned long i;

  if (wide)
    {
      BFD_ASSERT (command->type == BFD_MACH_O_LC_SEGMENT_64);
      
      memcpy (buf, seg->segname, 16);

      bfd_h_put_64 (abfd, seg->vmaddr, buf + 16);
      bfd_h_put_64 (abfd, seg->vmsize, buf + 24);
      bfd_h_put_64 (abfd, seg->fileoff, buf + 32);
      bfd_h_put_64 (abfd, seg->filesize, buf + 40);
      bfd_h_put_32 (abfd, seg->maxprot, buf + 48);
      bfd_h_put_32 (abfd, seg->initprot, buf + 52);
      bfd_h_put_32 (abfd, seg->nsects, buf + 56);
      bfd_h_put_32 (abfd, seg->flags, buf + 60);

      bfd_seek (abfd, command->offset + 8, SEEK_SET);
      if (bfd_bwrite ((PTR) buf, 64, abfd) != 64)
	return -1;
    }
  else
    {
      BFD_ASSERT (command->type == BFD_MACH_O_LC_SEGMENT);
      
      memcpy (buf, seg->segname, 16);

      bfd_h_put_32 (abfd, seg->vmaddr, buf + 16);
      bfd_h_put_32 (abfd, seg->vmsize, buf + 20);
      bfd_h_put_32 (abfd, seg->fileoff, buf + 24);
      bfd_h_put_32 (abfd, seg->filesize, buf + 28);
      bfd_h_put_32 (abfd, seg->maxprot, buf + 32);
      bfd_h_put_32 (abfd, seg->initprot, buf + 36);
      bfd_h_put_32 (abfd, seg->nsects, buf + 40);
      bfd_h_put_32 (abfd, seg->flags, buf + 44);

      bfd_seek (abfd, command->offset + 8, SEEK_SET);
      if (bfd_bwrite ((PTR) buf, 48, abfd) != 48)
	return -1;
    }

  {
    char buf[1024];
    bfd_vma nbytes = seg->filesize;
    bfd_vma curoff = seg->fileoff;

    while (nbytes > 0)
      {
	bfd_vma thiswrite = nbytes;

	if (thiswrite > 1024)
	  thiswrite = 1024;

	bfd_seek (abfd, curoff, SEEK_SET);
	if (bfd_bread ((PTR) buf, thiswrite, abfd) != thiswrite)
	  return -1;

	bfd_seek (abfd, curoff, SEEK_SET);
	if (bfd_bwrite ((PTR) buf, thiswrite, abfd) != thiswrite)
	  return -1;

	nbytes -= thiswrite;
	curoff += thiswrite;
      }
  }

  for (i = 0; i < seg->nsects; i++)
    {
      bfd_vma segoff;
      if (wide)
	segoff = command->offset + 64 + 8 + (i * 80);
      else
	segoff = command->offset + 48 + 8 + (i * 68);
      
      if (bfd_mach_o_scan_write_section
	  (abfd, &seg->sections[i], segoff, wide) != 0)
	return -1;
    }

  return 0;
}

static int
bfd_mach_o_scan_write_segment_32 (bfd *abfd, bfd_mach_o_load_command *command)
{
  return bfd_mach_o_scan_write_segment (abfd, command, 0);
}

static int
bfd_mach_o_scan_write_segment_64 (bfd *abfd, bfd_mach_o_load_command *command)
{
  return bfd_mach_o_scan_write_segment (abfd, command, 1);
}

static int
bfd_mach_o_scan_write_symtab_symbols (bfd *abfd, bfd_mach_o_load_command *command)
{
  bfd_mach_o_symtab_command *sym = &command->command.symtab;
  asymbol *s = NULL;
  unsigned long i;

  for (i = 0; i < sym->nsyms; i++)
    {
      unsigned char buf[12];
      bfd_vma symoff = sym->symoff + (i * 12);
      unsigned char ntype = 0;
      unsigned char nsect = 0;
      short ndesc = 0;

      s = &sym->symbols[i];

      /* Don't set this from the symbol information; use stored values.  */
#if 0
      if (s->flags & BSF_GLOBAL)
	ntype |= N_EXT;
      if (s->flags & BSF_DEBUGGING)
	ntype |= N_STAB;

      if (s->section == bfd_und_section_ptr)
	ntype |= N_UNDF;
      else if (s->section == bfd_abs_section_ptr)
	ntype |= N_ABS;
      else
	ntype |= N_SECT;
#endif

      /* Instead just set from the stored values.  */
      ntype = (s->udata.i >> 24) & 0xff;
      nsect = (s->udata.i >> 16) & 0xff;
      ndesc = s->udata.i & 0xffff;

      bfd_h_put_32 (abfd, s->name - sym->strtab, buf);
      bfd_h_put_8 (abfd, ntype, buf + 4);
      bfd_h_put_8 (abfd, nsect, buf + 5);
      bfd_h_put_16 (abfd, ndesc, buf + 6);
      bfd_h_put_32 (abfd, s->section->vma + s->value, buf + 8);

      bfd_seek (abfd, symoff, SEEK_SET);
      if (bfd_bwrite ((PTR) buf, 12, abfd) != 12)
	{
	  fprintf (stderr, "bfd_mach_o_scan_write_symtab_symbols: unable to write %d bytes at %lu\n",
		   12, (unsigned long) symoff);
	  return -1;
	}
    }

  return 0;
}

static int
bfd_mach_o_scan_write_symtab (bfd *abfd, bfd_mach_o_load_command *command)
{
  bfd_mach_o_symtab_command *seg = &command->command.symtab;
  unsigned char buf[16];

  BFD_ASSERT (command->type == BFD_MACH_O_LC_SYMTAB);

  bfd_h_put_32 (abfd, seg->symoff, buf);
  bfd_h_put_32 (abfd, seg->nsyms, buf + 4);
  bfd_h_put_32 (abfd, seg->stroff, buf + 8);
  bfd_h_put_32 (abfd, seg->strsize, buf + 12);

  bfd_seek (abfd, command->offset + 8, SEEK_SET);
  if (bfd_bwrite ((PTR) buf, 16, abfd) != 16)
    return -1;

  if (bfd_mach_o_scan_write_symtab_symbols (abfd, command) != 0)
    return -1;

  return 0;
}

static bfd_boolean
bfd_mach_o_write_contents (bfd *abfd)
{
  unsigned int i;
  asection *s;

  bfd_mach_o_data_struct *mdata = abfd->tdata.mach_o_data;

  /* Write data sections first in case they overlap header data to be
     written later.  */

  for (s = abfd->sections; s != (asection *) NULL; s = s->next)
    ;

#if 0
  for (i = 0; i < mdata->header.ncmds; i++)
    {
      bfd_mach_o_load_command *cur = &mdata->commands[i];
      if (cur->type != BFD_MACH_O_LC_SEGMENT)
	break;

      {
	bfd_mach_o_segment_command *seg = &cur->command.segment;
	char buf[1024];
	bfd_vma nbytes = seg->filesize;
	bfd_vma curoff = seg->fileoff;

	while (nbytes > 0)
	  {
	    bfd_vma thisread = nbytes;

	    if (thisread > 1024)
	      thisread = 1024;

	    bfd_seek (abfd, curoff, SEEK_SET);
	    if (bfd_bread ((PTR) buf, thisread, abfd) != thisread)
	      return FALSE;

	    bfd_seek (abfd, curoff, SEEK_SET);
	    if (bfd_bwrite ((PTR) buf, thisread, abfd) != thisread)
	      return FALSE;

	    nbytes -= thisread;
	    curoff += thisread;
	  }
      }
  }
#endif

  /* Now write header information.  */
  if (bfd_mach_o_write_header (abfd, &mdata->header) != 0)
    return FALSE;

  for (i = 0; i < mdata->header.ncmds; i++)
    {
      unsigned char buf[8];
      bfd_mach_o_load_command *cur = &mdata->commands[i];
      unsigned long typeflag;

      typeflag = cur->type_required ? cur->type & BFD_MACH_O_LC_REQ_DYLD : cur->type;

      bfd_h_put_32 (abfd, typeflag, buf);
      bfd_h_put_32 (abfd, cur->len, buf + 4);

      bfd_seek (abfd, cur->offset, SEEK_SET);
      if (bfd_bwrite ((PTR) buf, 8, abfd) != 8)
	return FALSE;

      switch (cur->type)
	{
	case BFD_MACH_O_LC_SEGMENT:
	  if (bfd_mach_o_scan_write_segment_32 (abfd, cur) != 0)
	    return FALSE;
	  break;
	case BFD_MACH_O_LC_SEGMENT_64:
	  if (bfd_mach_o_scan_write_segment_64 (abfd, cur) != 0)
	    return FALSE;
	  break;
	case BFD_MACH_O_LC_SYMTAB:
	  if (bfd_mach_o_scan_write_symtab (abfd, cur) != 0)
	    return FALSE;
	  break;
	case BFD_MACH_O_LC_SYMSEG:
	  break;
	case BFD_MACH_O_LC_THREAD:
	case BFD_MACH_O_LC_UNIXTHREAD:
	  if (bfd_mach_o_scan_write_thread (abfd, cur) != 0)
	    return FALSE;
	  break;
	case BFD_MACH_O_LC_LOADFVMLIB:
	case BFD_MACH_O_LC_IDFVMLIB:
	case BFD_MACH_O_LC_IDENT:
	case BFD_MACH_O_LC_FVMFILE:
	case BFD_MACH_O_LC_PREPAGE:
	case BFD_MACH_O_LC_DYSYMTAB:
	case BFD_MACH_O_LC_LOAD_DYLIB:
	case BFD_MACH_O_LC_LOAD_WEAK_DYLIB:
	case BFD_MACH_O_LC_REEXPORT_DYLIB:
	case BFD_MACH_O_LC_ID_DYLIB:
	case BFD_MACH_O_LC_LOAD_DYLINKER:
	case BFD_MACH_O_LC_ID_DYLINKER:
	case BFD_MACH_O_LC_PREBOUND_DYLIB:
	case BFD_MACH_O_LC_ROUTINES:
	case BFD_MACH_O_LC_SUB_FRAMEWORK:
	case BFD_MACH_O_LC_LAZY_LOAD_DYLIB:
	case BFD_MACH_O_LC_ENCRYPTION_INFO:
	case BFD_MACH_O_LC_DYLD_INFO:
	case BFD_MACH_O_LC_DYLD_INFO_ONLY:
	  break;
	default:
	  fprintf (stderr,
		   "unable to write unknown load command 0x%lx\n",
		   (long) cur->type);
	  return FALSE;
	}
    }

  return TRUE;
}

static int
bfd_mach_o_sizeof_headers (bfd *a ATTRIBUTE_UNUSED,
			   bfd_boolean b ATTRIBUTE_UNUSED)
{
  return 0;
}

/* Make an empty symbol.  This is required only because
   bfd_make_section_anyway wants to create a symbol for the section.  */

static asymbol *
bfd_mach_o_make_empty_symbol (bfd *abfd)
{
  asymbol *new;

  new = (asymbol *) bfd_zalloc (abfd, sizeof (asymbol));
  if (new == NULL)
    return new;
  new->the_bfd = abfd;
  return new;
}

static int
bfd_mach_o_read_header (bfd *abfd, bfd_mach_o_header *header)
{
  unsigned char buf[32];
  unsigned int size;
  bfd_vma (* get32) PARAMS ((const void *)) = NULL;

  bfd_seek (abfd, 0, SEEK_SET);
  if (bfd_bread ((PTR) buf, 4, abfd) != 4)
    return -1;

  if (bfd_getb32 (buf) == 0xfeedface)
    {
      header->byteorder = BFD_ENDIAN_BIG;
      header->magic = 0xfeedface;
      header->version = 1;
      get32 = bfd_getb32;
    }
  else if (bfd_getl32 (buf) == 0xfeedface)
    {
      header->byteorder = BFD_ENDIAN_LITTLE;
      header->magic = 0xfeedface;
      header->version = 1;
      get32 = bfd_getl32;
    }
  else if (bfd_getb32 (buf) == 0xfeedfacf)
    {
      header->byteorder = BFD_ENDIAN_BIG;
      header->magic = 0xfeedfacf;
      header->version = 2;
      get32 = bfd_getb32;
    }
  else if (bfd_getl32 (buf) == 0xfeedfacf)
    {
      header->byteorder = BFD_ENDIAN_LITTLE;
      header->magic = 0xfeedfacf;
      header->version = 2;
      get32 = bfd_getl32;
    }
  else
    {
      header->byteorder = BFD_ENDIAN_UNKNOWN;
      return -1;
    }

  size = (header->version == 2) ? 32 : 28;

  bfd_seek (abfd, 0, SEEK_SET);
  if (bfd_bread ((PTR) buf, size, abfd) != size)
    return -1;

  header->cputype = (*get32) (buf + 4);
  header->cpusubtype = (*get32) (buf + 8);
  header->filetype = (*get32) (buf + 12);
  header->ncmds = (*get32) (buf + 16);
  header->sizeofcmds = (*get32) (buf + 20);
  header->flags = (*get32) (buf + 24);
  
  if (header->version == 2)
    header->reserved = (*get32) (buf + 28);

  return 0;
}

static asection *
bfd_mach_o_make_bfd_section (bfd *abfd, bfd_mach_o_section *section)
{
  asection *bfdsec;
  char *sname;

  const char *prefix = "LC_SEGMENT";
  unsigned int snamelen;

  snamelen = strlen (prefix) + 1
    + strlen (section->segname) + 1
    + strlen (section->sectname) + 1;

  sname = (char *) bfd_alloc (abfd, snamelen);
  if (sname == NULL)
    return NULL;
  sprintf (sname, "%s.%s.%s", prefix, section->segname, section->sectname);

  bfdsec = bfd_make_section_anyway (abfd, sname);
  if (bfdsec == NULL)
    return NULL;

  bfdsec->vma = section->addr;
  bfdsec->lma = section->addr;
  bfdsec->size = section->size;
  bfdsec->filepos = section->offset;
  bfdsec->alignment_power = section->align;
  bfdsec->segment_mark = 0;

  if ((section->flags & BFD_MACH_O_SECTION_TYPE_MASK) == BFD_MACH_O_S_ZEROFILL 
      || (section->flags & BFD_MACH_O_SECTION_TYPE_MASK) == BFD_MACH_O_S_GB_ZEROFILL)
    {
      bfdsec->flags = SEC_ALLOC;
    }
  else if ((section->flags & BFD_MACH_O_SECTION_TYPE_MASK) == BFD_MACH_O_S_ATTR_DEBUG 
           || strcmp (section->segname, "__DWARF") == 0)
    {
      bfdsec->flags = SEC_HAS_CONTENTS;
    }
  else
    {
      bfdsec->flags = SEC_HAS_CONTENTS | SEC_LOAD | SEC_ALLOC | SEC_CODE;
    }

  /* The __TEXT.__text segment is always readonly. */
  if (strcmp (section->segname, "__TEXT") == 0
      && (section->sectname[0] == '\0' || strcmp (section->sectname, "__text") == 0))
    bfdsec->flags |= SEC_READONLY;

  return bfdsec;
}

static int
bfd_mach_o_scan_read_section_32 (bfd *abfd,
				 bfd_mach_o_section *section,
				 bfd_vma offset)
{
  unsigned char buf[68];

  bfd_seek (abfd, offset, SEEK_SET);
  if (bfd_bread ((PTR) buf, 68, abfd) != 68)
    return -1;

  memcpy (section->sectname, buf, 16);
  section->sectname[16] = '\0';
  memcpy (section->segname, buf + 16, 16);
  section->segname[16] = '\0';
  section->addr = bfd_h_get_32 (abfd, buf + 32);
  section->size = bfd_h_get_32 (abfd, buf + 36);
  section->offset = bfd_h_get_32 (abfd, buf + 40);
  section->align = bfd_h_get_32 (abfd, buf + 44);
  section->reloff = bfd_h_get_32 (abfd, buf + 48);
  section->nreloc = bfd_h_get_32 (abfd, buf + 52);
  section->flags = bfd_h_get_32 (abfd, buf + 56);
  section->reserved1 = bfd_h_get_32 (abfd, buf + 60);
  section->reserved2 = bfd_h_get_32 (abfd, buf + 64);
  section->reserved3 = 0;
  section->bfdsection = bfd_mach_o_make_bfd_section (abfd, section);

  if (section->bfdsection == NULL)
    return -1;

  return 0;
}

static int
bfd_mach_o_scan_read_section_64 (bfd *abfd,
				 bfd_mach_o_section *section,
				 bfd_vma offset)
{
  unsigned char buf[80];

  bfd_seek (abfd, offset, SEEK_SET);
  if (bfd_bread ((PTR) buf, 80, abfd) != 80)
    return -1;

  memcpy (section->sectname, buf, 16);
  section->sectname[16] = '\0';
  memcpy (section->segname, buf + 16, 16);
  section->segname[16] = '\0';
  section->addr = bfd_h_get_64 (abfd, buf + 32);
  section->size = bfd_h_get_64 (abfd, buf + 40);
  section->offset = bfd_h_get_32 (abfd, buf + 48);
  section->align = bfd_h_get_32 (abfd, buf + 52);
  section->reloff = bfd_h_get_32 (abfd, buf + 56);
  section->nreloc = bfd_h_get_32 (abfd, buf + 60);
  section->flags = bfd_h_get_32 (abfd, buf + 64);
  section->reserved1 = bfd_h_get_32 (abfd, buf + 68);
  section->reserved2 = bfd_h_get_32 (abfd, buf + 72);
  section->reserved3 = bfd_h_get_32 (abfd, buf + 76);
  section->bfdsection = bfd_mach_o_make_bfd_section (abfd, section);

  if (section->bfdsection == NULL)
    return -1;

  return 0;
}

#define ARM_THREAD_STATE_STR	"ARM_THREAD_STATE"
#define ARM_VFP_STATE_STR	"ARM_VFP_STATE"
#define ARM_EXCEPTION_STATE_STR	"ARM_EXCEPTION_STATE"

static const char *
bfd_mach_o_arm_flavour_string (unsigned int flavour)
{
  switch ((int) flavour)
    {
    case BFD_MACH_O_ARM_THREAD_STATE: return ARM_THREAD_STATE_STR;
    case BFD_MACH_O_ARM_VFP_STATE: return ARM_VFP_STATE_STR;
    case BFD_MACH_O_ARM_EXCEPTION_STATE: return ARM_EXCEPTION_STATE_STR;
    default: return "UNKNOWN";
    }
}

static unsigned int
bfd_mach_o_arm_flavour_from_string(const char* s)
{
  if (strcmp(s, ARM_THREAD_STATE_STR) == 0)
    return BFD_MACH_O_ARM_THREAD_STATE;
  else if (strcmp(s, ARM_VFP_STATE_STR) == 0)
    return BFD_MACH_O_ARM_VFP_STATE;
  else if (strcmp(s, ARM_EXCEPTION_STATE_STR) == 0)
    return BFD_MACH_O_ARM_EXCEPTION_STATE;
  return 0;
}


static int
bfd_mach_o_scan_read_section (bfd *abfd,
			      bfd_mach_o_section *section,
			      bfd_vma offset,
			      unsigned int wide)
{
  if (wide)
    return bfd_mach_o_scan_read_section_64 (abfd, section, offset);
  else
    return bfd_mach_o_scan_read_section_32 (abfd, section, offset);
}

int
bfd_mach_o_scan_read_symtab_symbol (bfd *abfd,
				    bfd_mach_o_symtab_command *sym,
				    asymbol *s,
				    unsigned long i)
{
  bfd_mach_o_data_struct *mdata = abfd->tdata.mach_o_data;
  unsigned int wide = (mdata->header.version == 2);
  unsigned int symwidth = wide ? 16 : 12;
  bfd_vma symoff = sym->symoff + (i * symwidth);
  unsigned char buf[16];
  unsigned char type = -1;
  unsigned char section = -1;
  short desc = -1;
  symvalue value = -1;
  unsigned long stroff = -1;
  unsigned int symtype = -1;

  BFD_ASSERT (sym->strtab != NULL);

  bfd_seek (abfd, symoff, SEEK_SET);
  if (bfd_bread ((PTR) buf, symwidth, abfd) != symwidth)
    {
      fprintf (stderr, "bfd_mach_o_scan_read_symtab_symbol: unable to read %d bytes at %lu\n",
	       symwidth, (unsigned long) symoff);
      return -1;
    }

  stroff = bfd_h_get_32 (abfd, buf);
  type = bfd_h_get_8 (abfd, buf + 4);
  symtype = (type & 0x0e);
  section = bfd_h_get_8 (abfd, buf + 5) - 1;
  desc = bfd_h_get_16 (abfd, buf + 6);

  if (wide)
    value = bfd_h_get_64 (abfd, buf + 8);
  else
    value = bfd_h_get_32 (abfd, buf + 8);
    
  if (stroff >= sym->strsize)
    {
      fprintf (stderr, "bfd_mach_o_scan_read_symtab_symbol: symbol name out of range (%lu >= %lu)\n",
	       (unsigned long) stroff, (unsigned long) sym->strsize);
      return -1;
    }

  s->the_bfd = abfd;
  s->name = sym->strtab + stroff;
  s->value = value;
  s->udata.i = (type << 24) | (section << 16) | desc;
  s->flags = 0x0;

  if (type & BFD_MACH_O_N_STAB)
    {
      s->flags |= BSF_DEBUGGING;
      s->section = bfd_und_section_ptr;
    }
  else
    {
      if (type & BFD_MACH_O_N_PEXT)
	{
	  type &= ~BFD_MACH_O_N_PEXT;
	  s->flags |= BSF_GLOBAL;
	}

      if (type & BFD_MACH_O_N_EXT)
	{
	  type &= ~BFD_MACH_O_N_EXT;
	  s->flags |= BSF_GLOBAL;
	}

      switch (symtype)
	{
	case BFD_MACH_O_N_UNDF:
	  s->section = bfd_und_section_ptr;
	  break;
	case BFD_MACH_O_N_PBUD:
	  s->section = bfd_und_section_ptr;
	  break;
	case BFD_MACH_O_N_ABS:
	  s->section = bfd_abs_section_ptr;
	  break;
	case BFD_MACH_O_N_SECT:
	  if ((section > 0) && (section <= mdata->nsects))
	    {
	      s->section = mdata->sections[section]->bfdsection;
	      s->value = s->value - mdata->sections[section]->addr;
	    }
	  else
	    {
	      /* Mach-O uses 0 to mean "no section"; not an error.  */
	      if (section != 0)
		{
		  fprintf (stderr, "bfd_mach_o_scan_read_symtab_symbol: "
			   "symbol \"%s\" specified invalid section %d (max %lu): setting to undefined\n",
			   s->name, section, mdata->nsects);
		}
	      s->section = bfd_und_section_ptr;
	    }
	  break;
	case BFD_MACH_O_N_INDR:
	  fprintf (stderr, "bfd_mach_o_scan_read_symtab_symbol: "
		   "symbol \"%s\" is unsupported 'indirect' reference: setting to undefined\n",
		   s->name);
	  s->section = bfd_und_section_ptr;
	  break;
	default:
	  fprintf (stderr, "bfd_mach_o_scan_read_symtab_symbol: "
		   "symbol \"%s\" specified invalid type field 0x%x: setting to undefined\n",
		   s->name, symtype);
	  s->section = bfd_und_section_ptr;
	  break;
	}
    }

  return 0;
}

int
bfd_mach_o_scan_read_symtab_strtab (bfd *abfd,
				    bfd_mach_o_symtab_command *sym)
{
  BFD_ASSERT (sym->strtab == NULL);

  if (abfd->flags & BFD_IN_MEMORY)
    {
      struct bfd_in_memory *b;

      b = (struct bfd_in_memory *) abfd->iostream;

      if ((sym->stroff + sym->strsize) > b->size)
	{
	  bfd_set_error (bfd_error_file_truncated);
	  return -1;
	}
      sym->strtab = (char *) b->buffer + sym->stroff;
      return 0;
    }

  sym->strtab = bfd_alloc (abfd, sym->strsize);
  if (sym->strtab == NULL)
    return -1;

  bfd_seek (abfd, sym->stroff, SEEK_SET);
  if (bfd_bread ((PTR) sym->strtab, sym->strsize, abfd) != sym->strsize)
    {
      fprintf (stderr, "bfd_mach_o_scan_read_symtab_strtab: unable to read %lu bytes at %lu\n",
	       sym->strsize, sym->stroff);
      return -1;
    }

  return 0;
}

int
bfd_mach_o_scan_read_symtab_symbols (bfd *abfd,
				     bfd_mach_o_symtab_command *sym)
{
  unsigned long i;
  int ret;

  BFD_ASSERT (sym->symbols == NULL);
  sym->symbols = bfd_alloc (abfd, sym->nsyms * sizeof (asymbol));

  if (sym->symbols == NULL)
    {
      fprintf (stderr, "bfd_mach_o_scan_read_symtab_symbols: unable to allocate memory for symbols\n");
      return -1;
    }

  ret = bfd_mach_o_scan_read_symtab_strtab (abfd, sym);
  if (ret != 0)
    return ret;

  for (i = 0; i < sym->nsyms; i++)
    {
      ret = bfd_mach_o_scan_read_symtab_symbol (abfd, sym, &sym->symbols[i], i);
      if (ret != 0)
	return ret;
    }

  return 0;
}

int
bfd_mach_o_scan_read_dysymtab_symbol (bfd *abfd,
				      bfd_mach_o_dysymtab_command *dysym,
				      bfd_mach_o_symtab_command *sym,
				      asymbol *s,
				      unsigned long i)
{
  unsigned long isymoff = dysym->indirectsymoff + (i * 4);
  unsigned long symindex;
  unsigned char buf[4];

  BFD_ASSERT (i < dysym->nindirectsyms);

  bfd_seek (abfd, isymoff, SEEK_SET);
  if (bfd_bread ((PTR) buf, 4, abfd) != 4)
    {
      fprintf (stderr, "bfd_mach_o_scan_read_dysymtab_symbol: unable to read %lu bytes at %lu\n",
	       (unsigned long) 4, isymoff);
      return -1;
    }
  symindex = bfd_h_get_32 (abfd, buf);
  /* Strip off the INDIRECT_SYMBOL_LOCAL and INDIRECT_SYMBOL_ABS flags, if
     they were present.  */
  symindex = symindex & 
               ((unsigned long) ~(INDIRECT_SYMBOL_LOCAL | INDIRECT_SYMBOL_ABS));


  return bfd_mach_o_scan_read_symtab_symbol (abfd, sym, s, symindex);
}

#define i386_THREAD_STATE_STR	  "i386_THREAD_STATE"
#define i386_FLOAT_STATE_STR	  "i386_FLOAT_STATE"
#define i386_EXCEPTION_STATE_STR  "i386_EXCEPTION_STATE"
#define x86_THREAD_STATE64_STR	  "x86_THREAD_STATE64"
#define x86_FLOAT_STATE64_STR	  "x86_FLOAT_STATE64"
#define x86_EXCEPTION_STATE64_STR "x86_EXCEPTION_STATE64"
#define x86_THREAD_STATE_STR	  "x86_THREAD_STATE"
#define x86_FLOAT_STATE_STR	  "x86_FLOAT_STATE"
#define x86_EXCEPTION_STATE_STR   "x86_EXCEPTION_STATE"

static const char *
bfd_mach_o_i386_flavour_string (unsigned int flavour)
{
  switch ((int) flavour)
    {
    case BFD_MACH_O_i386_THREAD_STATE: return i386_THREAD_STATE_STR;
    case BFD_MACH_O_i386_FLOAT_STATE: return i386_FLOAT_STATE_STR;
    case BFD_MACH_O_i386_EXCEPTION_STATE: return i386_EXCEPTION_STATE_STR;
    /* APPLE LOCAL begin x86_64 */
    case BFD_MACH_O_x86_THREAD_STATE64: return x86_THREAD_STATE64_STR;
    case BFD_MACH_O_x86_FLOAT_STATE64: return x86_FLOAT_STATE64_STR;
    case BFD_MACH_O_x86_EXCEPTION_STATE64: return x86_EXCEPTION_STATE64_STR;
    case BFD_MACH_O_x86_THREAD_STATE: return x86_THREAD_STATE_STR;
    case BFD_MACH_O_x86_FLOAT_STATE: return x86_FLOAT_STATE_STR;
    case BFD_MACH_O_x86_EXCEPTION_STATE: return x86_EXCEPTION_STATE_STR;
    /* APPLE LOCAL end x86_64 */
    case BFD_MACH_O_i386_THREAD_STATE_NONE: return "THREAD_STATE_NONE";
    default: return "UNKNOWN";
    }
}
static unsigned int
bfd_mach_o_i386_flavour_from_string (const char * s)
{
  if (strcmp(s, i386_THREAD_STATE_STR) == 0)
    return BFD_MACH_O_i386_THREAD_STATE;
  else if (strcmp(s, i386_FLOAT_STATE_STR) == 0)
    return BFD_MACH_O_i386_FLOAT_STATE;
  else if (strcmp(s, i386_EXCEPTION_STATE_STR) == 0)
    return BFD_MACH_O_i386_EXCEPTION_STATE;
  else if (strcmp(s, x86_THREAD_STATE64_STR) == 0)
    return BFD_MACH_O_x86_THREAD_STATE64;
  else if (strcmp(s, x86_FLOAT_STATE64_STR) == 0)
    return BFD_MACH_O_x86_FLOAT_STATE64;
  else if (strcmp(s, x86_EXCEPTION_STATE64_STR) == 0)
    return BFD_MACH_O_x86_EXCEPTION_STATE64;
  else if (strcmp(s, x86_THREAD_STATE_STR) == 0)
    return BFD_MACH_O_x86_THREAD_STATE;
  else if (strcmp(s, x86_FLOAT_STATE_STR) == 0)
    return BFD_MACH_O_x86_FLOAT_STATE;
  else if (strcmp(s, x86_EXCEPTION_STATE_STR) == 0)
    return BFD_MACH_O_x86_EXCEPTION_STATE;
  return 0;
}

#define PPC_THREAD_STATE_STR	"PPC_THREAD_STATE"
#define PPC_FLOAT_STATE_STR	"PPC_FLOAT_STATE"
#define PPC_EXCEPTION_STATE_STR	"PPC_EXCEPTION_STATE"
#define PPC_VECTOR_STATE_STR	"PPC_VECTOR_STATE"
#define PPC_THREAD_STATE_64_STR	"PPC_THREAD_STATE_64"
static const char *
bfd_mach_o_ppc_flavour_string (unsigned int flavour)
{
  switch ((int) flavour)
    {
    case BFD_MACH_O_PPC_THREAD_STATE: return PPC_THREAD_STATE_STR;
    case BFD_MACH_O_PPC_FLOAT_STATE: return PPC_FLOAT_STATE_STR;
    case BFD_MACH_O_PPC_EXCEPTION_STATE: return PPC_EXCEPTION_STATE_STR;
    case BFD_MACH_O_PPC_VECTOR_STATE: return PPC_VECTOR_STATE_STR;
    case BFD_MACH_O_PPC_THREAD_STATE_64: return PPC_THREAD_STATE_64_STR;
    default: return "UNKNOWN";
    }
}

static unsigned int
bfd_mach_o_ppc_flavour_from_string (const char* s)
{
  if (strcmp(s, PPC_THREAD_STATE_STR) == 0)
    return BFD_MACH_O_PPC_THREAD_STATE;
  else if (strcmp(s, PPC_FLOAT_STATE_STR) == 0)
    return BFD_MACH_O_PPC_FLOAT_STATE;
  else if (strcmp(s, PPC_EXCEPTION_STATE_STR) == 0)
    return BFD_MACH_O_PPC_EXCEPTION_STATE;
  else if (strcmp(s, PPC_VECTOR_STATE_STR) == 0)
    return BFD_MACH_O_PPC_VECTOR_STATE;
  else if (strcmp(s, PPC_THREAD_STATE_64_STR) == 0)
    return BFD_MACH_O_PPC_THREAD_STATE_64;
  return 0;
}

unsigned int
bfd_mach_o_flavour_from_string(unsigned long cputype, const char* s)
{
  unsigned int flavour = 0;
  if (s)
    {
      switch (cputype)
	{
	case BFD_MACH_O_CPU_TYPE_POWERPC:
	case BFD_MACH_O_CPU_TYPE_POWERPC_64:
	  flavour = bfd_mach_o_ppc_flavour_from_string (s);
	  break;
	case BFD_MACH_O_CPU_TYPE_I386:
	case BFD_MACH_O_CPU_TYPE_X86_64:
	  flavour = bfd_mach_o_i386_flavour_from_string (s);
	  break;
	case BFD_MACH_O_CPU_TYPE_ARM:
	  flavour = bfd_mach_o_arm_flavour_from_string (s);
	  break;
	default:
	  break;
	}
    }
  return flavour;
}


static int
bfd_mach_o_scan_read_dylinker (bfd *abfd,
			       bfd_mach_o_load_command *command)
{
  bfd_mach_o_dylinker_command *cmd = &command->command.dylinker;
  unsigned char buf[4];
  unsigned int nameoff;
  asection *bfdsec;
  char *sname;
  const char *prefix;

  BFD_ASSERT ((command->type == BFD_MACH_O_LC_ID_DYLINKER)
	      || (command->type == BFD_MACH_O_LC_LOAD_DYLINKER));

  bfd_seek (abfd, command->offset + 8, SEEK_SET);
  if (bfd_bread ((PTR) buf, 4, abfd) != 4)
    return -1;

  nameoff = bfd_h_get_32 (abfd, buf + 0);

  cmd->name_offset = command->offset + nameoff;
  cmd->name_len = command->len - nameoff;

  if (command->type == BFD_MACH_O_LC_LOAD_DYLINKER)
    prefix = "LC_LOAD_DYLINKER";
  else if (command->type == BFD_MACH_O_LC_ID_DYLINKER)
    prefix = "LC_ID_DYLINKER";
  else
    abort ();

  sname = (char *) bfd_alloc (abfd, strlen (prefix) + 1);
  if (sname == NULL)
    return -1;
  strcpy (sname, prefix);

  bfdsec = bfd_make_section_anyway (abfd, sname);
  if (bfdsec == NULL)
    return -1;

  bfdsec->vma = 0;
  bfdsec->lma = 0;
  bfdsec->size = command->len - 8;
  bfdsec->filepos = command->offset + 8;
  bfdsec->alignment_power = 0;
  bfdsec->flags = SEC_HAS_CONTENTS;

  cmd->section = bfdsec;

  return 0;
}

static int
bfd_mach_o_scan_read_dylib (bfd *abfd, bfd_mach_o_load_command *command)
{
  bfd_mach_o_dylib_command *cmd = &command->command.dylib;
  unsigned char buf[16];
  unsigned int nameoff;
  asection *bfdsec;
  char *sname;
  const char *prefix;

  BFD_ASSERT ((command->type == BFD_MACH_O_LC_ID_DYLIB)
	      || (command->type == BFD_MACH_O_LC_LOAD_DYLIB)
	      || (command->type == BFD_MACH_O_LC_REEXPORT_DYLIB)
	      || (command->type == BFD_MACH_O_LC_LOAD_WEAK_DYLIB));

  bfd_seek (abfd, command->offset + 8, SEEK_SET);
  if (bfd_bread ((PTR) buf, 16, abfd) != 16)
    return -1;

  nameoff = bfd_h_get_32 (abfd, buf + 0);
  cmd->timestamp = bfd_h_get_32 (abfd, buf + 4);
  cmd->current_version = bfd_h_get_32 (abfd, buf + 8);
  cmd->compatibility_version = bfd_h_get_32 (abfd, buf + 12);

  cmd->name_offset = command->offset + nameoff;
  cmd->name_len = command->len - nameoff;

  if (command->type == BFD_MACH_O_LC_LOAD_DYLIB)
    prefix = "LC_LOAD_DYLIB";
  else if (command->type == BFD_MACH_O_LC_LOAD_WEAK_DYLIB)
    prefix = "LC_LOAD_WEAK_DYLIB";
  else if (command->type == BFD_MACH_O_LC_REEXPORT_DYLIB)
    prefix = "LC_REEXPORT_DYLIB";
  else if (command->type == BFD_MACH_O_LC_ID_DYLIB)
    prefix = "LC_ID_DYLIB";
  else
    abort ();

  sname = (char *) bfd_alloc (abfd, strlen (prefix) + 1);
  if (sname == NULL)
    return -1;
  strcpy (sname, prefix);

  bfdsec = bfd_make_section_anyway (abfd, sname);
  if (bfdsec == NULL)
    return -1;

  bfdsec->vma = 0;
  bfdsec->lma = 0;
  bfdsec->size = command->len - 8;
  bfdsec->filepos = command->offset + 8;
  bfdsec->alignment_power = 0;
  bfdsec->flags = SEC_HAS_CONTENTS;

  cmd->section = bfdsec;

  return 0;
}

static int
bfd_mach_o_scan_read_prebound_dylib (bfd *abfd ATTRIBUTE_UNUSED,
				     bfd_mach_o_load_command *command ATTRIBUTE_UNUSED)
{
  /* bfd_mach_o_prebound_dylib_command *cmd = &command->command.prebound_dylib; */

  BFD_ASSERT (command->type == BFD_MACH_O_LC_PREBOUND_DYLIB);
  return 0;
}

static int
bfd_mach_o_scan_read_thread (bfd *abfd, bfd_mach_o_load_command *command)
{
  bfd_mach_o_data_struct *mdata = NULL;
  bfd_mach_o_thread_command *cmd = &command->command.thread;
  unsigned char buf[8];
  bfd_vma offset;
  unsigned int nflavours;
  unsigned int i;

  BFD_ASSERT ((command->type == BFD_MACH_O_LC_THREAD)
	      || (command->type == BFD_MACH_O_LC_UNIXTHREAD));

  BFD_ASSERT (bfd_mach_o_valid (abfd));
  mdata = abfd->tdata.mach_o_data;

  offset = 8;
  nflavours = 0;
  while (offset != command->len)
    {
      if (offset >= command->len)
	return -1;

      bfd_seek (abfd, command->offset + offset, SEEK_SET);

      if (bfd_bread ((PTR) buf, 8, abfd) != 8)
	return -1;

      offset += 8 + bfd_h_get_32 (abfd, buf + 4) * 4;
      nflavours++;
    }

  cmd->flavours =
    ((bfd_mach_o_thread_flavour *)
     bfd_alloc (abfd, nflavours * sizeof (bfd_mach_o_thread_flavour)));
  if (cmd->flavours == NULL)
    return -1;
  cmd->nflavours = nflavours;

  offset = 8;
  nflavours = 0;
  while (offset != command->len)
    {
      if (offset >= command->len)
	return -1;

      if (nflavours >= cmd->nflavours)
	return -1;

      bfd_seek (abfd, command->offset + offset, SEEK_SET);

      if (bfd_bread ((PTR) buf, 8, abfd) != 8)
	return -1;

      cmd->flavours[nflavours].flavour = bfd_h_get_32 (abfd, buf);
      cmd->flavours[nflavours].offset = command->offset + offset + 8;
      cmd->flavours[nflavours].size = bfd_h_get_32 (abfd, buf + 4) * 4;
      offset += cmd->flavours[nflavours].size + 8;
      nflavours++;
    }

  for (i = 0; i < nflavours; i++)
    {
      asection *bfdsec;
      unsigned int snamelen;
      char *sname;
      const char *flavourstr;
      const char *prefix = "LC_THREAD";
      unsigned int j = 0;

      switch (mdata->header.cputype)
	{
	case BFD_MACH_O_CPU_TYPE_POWERPC:
	case BFD_MACH_O_CPU_TYPE_POWERPC_64:
	  flavourstr = bfd_mach_o_ppc_flavour_string (cmd->flavours[i].flavour);
	  break;
	case BFD_MACH_O_CPU_TYPE_I386:
	case BFD_MACH_O_CPU_TYPE_X86_64:
	  flavourstr = bfd_mach_o_i386_flavour_string (cmd->flavours[i].flavour);
	  break;
	case BFD_MACH_O_CPU_TYPE_ARM:
	  flavourstr = bfd_mach_o_arm_flavour_string (cmd->flavours[i].flavour);
	  break;
	default:
	  flavourstr = "UNKNOWN_ARCHITECTURE";
	  break;
	}

      snamelen = strlen (prefix) + 1 + 20 + 1 + strlen (flavourstr) + 1;
      sname = (char *) bfd_alloc (abfd, snamelen);
      if (sname == NULL)
	return -1;

      for (;;)
	{
	  sprintf (sname, "%s.%s.%u", prefix, flavourstr, j);
	  if (bfd_get_section_by_name (abfd, sname) == NULL)
	    break;
	  j++;
	}

      bfdsec = bfd_make_section (abfd, sname);

      bfdsec->vma = 0;
      bfdsec->lma = 0;
      bfdsec->size = cmd->flavours[i].size;
      bfdsec->filepos = cmd->flavours[i].offset;
      bfdsec->alignment_power = 0x0;
      bfdsec->flags = SEC_HAS_CONTENTS;

      cmd->section = bfdsec;
    }

  return 0;
}

static int
bfd_mach_o_scan_read_dysymtab (bfd *abfd, bfd_mach_o_load_command *command)
{
  bfd_mach_o_dysymtab_command *seg = &command->command.dysymtab;
  unsigned char buf[72];
  const char *prefix;
  char *sname;
  struct bfd_section *stabs_pseudo_section;
  asection *bfdsec;
  int nlist_size;
  bfd_boolean in_mem_shared_cache;
  
  in_mem_shared_cache = bfd_mach_o_in_shared_cached_memory (abfd);

  BFD_ASSERT (command->type == BFD_MACH_O_LC_DYSYMTAB);

  bfd_seek (abfd, command->offset + 8, SEEK_SET);
  if (bfd_bread ((PTR) buf, 72, abfd) != 72)
    return -1;

  seg->ilocalsym = bfd_h_get_32 (abfd, buf + 0);
  seg->nlocalsym = bfd_h_get_32 (abfd, buf + 4);
  seg->iextdefsym = bfd_h_get_32 (abfd, buf + 8);
  seg->nextdefsym = bfd_h_get_32 (abfd, buf + 12);
  seg->iundefsym = bfd_h_get_32 (abfd, buf + 16);
  seg->nundefsym = bfd_h_get_32 (abfd, buf + 20);
  seg->tocoff = bfd_h_get_32 (abfd, buf + 24);
  seg->ntoc = bfd_h_get_32 (abfd, buf + 28);
  seg->modtaboff = bfd_h_get_32 (abfd, buf + 32);
  seg->nmodtab = bfd_h_get_32 (abfd, buf + 36);
  seg->extrefsymoff = bfd_h_get_32 (abfd, buf + 40);
  seg->nextrefsyms = bfd_h_get_32 (abfd, buf + 44);
  seg->indirectsymoff = bfd_h_get_32 (abfd, buf + 48);
  seg->nindirectsyms = bfd_h_get_32 (abfd, buf + 52);
  seg->extreloff = bfd_h_get_32 (abfd, buf + 56);
  seg->nextrel = bfd_h_get_32 (abfd, buf + 60);
  seg->locreloff = bfd_h_get_32 (abfd, buf + 64);
  seg->nlocrel = bfd_h_get_32 (abfd, buf + 68);

  /* Create a fake section to indicate the start & length of the 
     "local" stabs -- the nlist records that are not externally
     visible from this compilation unit.  
     The MacOS X static link editor helpfully puts the nlist records
     in this order:

        local nlist records
        externally defined nlist records
        undefined symbols

     This is especially useful if we want to minimize the amount of
     minsyms we create initially.  */

  /* APPLE LOCAL shared cache
     If we are parsing an in memory mach image that is in the shared
     cache, we need not check for continuity in the dysymtab load commands
     since we know they aren't contiguous. These kinds of mach images all 
     share one big symbol and string table, and the dysymtab load command 
     tells which symbols belong to each image. We create a 
     "LC_DYSYMTAB.localstabs" for these files that won't get used,
     will contain no entries, or one bogus entry created by dyld. We also
     create a "LC_DYSYMTAB.nonlocalstabs" section that contains all of the 
     extdef symbols. If we need to parse the undef symbols, we may need to
     create a "LC_DYSYMTAB.undefstabs" section that we can check for since
     items in the shared cache do not have contiguous EXTDEF and UNDEF 
     symbols.  */
     
  if (in_mem_shared_cache == 0)
    {
      /* First, check that our assumption about the ordering is correct.
	 ilocalsym should always be less than (come before) 
	 iextdefsym and iundefsym.  */
      if (seg->ilocalsym >= seg->iextdefsym || seg->ilocalsym >= seg->iundefsym)
	return 0;

      /* Check that the three types - local, external, undefined - are contiguous
	 and start at offset 0 like they're supposed to. */

      if (seg->ilocalsym != 0 || seg->nlocalsym != seg->iextdefsym ||
	  seg->nlocalsym + seg->nextdefsym != seg->iundefsym)
	return 0;
    }

  /* nlist's are different sizes for 32 bit & 64 bit PPC...  */

  nlist_size =   (bfd_mach_o_version (abfd) > 1) ? 16 : 12;
  /* Now create the fake sections.  */

  stabs_pseudo_section = bfd_get_section_by_name (abfd, "LC_SYMTAB.stabs");
  if (stabs_pseudo_section == NULL)
    return 0;

  /* APPLE LOCAL shared cache - only create the fake localstabs section if
     we have some local symbols.  */
  if (seg->nlocalsym > 0)
    {
      prefix = "LC_DYSYMTAB.localstabs";
      
      sname = (char *) bfd_alloc (abfd, strlen (prefix) + 1);
      if (sname == NULL)
	return -1;
      strcpy (sname, prefix);

      bfdsec = bfd_make_section_anyway (abfd, sname);
      if (bfdsec == NULL)
	return -1;

      bfdsec->vma = 0;
      bfdsec->lma = 0;
      bfdsec->size = seg->nlocalsym * nlist_size;
      bfdsec->filepos = stabs_pseudo_section->filepos + (seg->ilocalsym * nlist_size);
      bfdsec->alignment_power = 0;
      bfdsec->flags = SEC_HAS_CONTENTS;
    }
  
  /* APPLE LOCAL shared cache - only create the fake localstabs section if
     we have some local symbols.  */
  if (seg->nextdefsym > 0 || seg->nundefsym > 0)
    {
      unsigned long num_nonlocalstabs;
      if (in_mem_shared_cache)
	{
	  /* In the memory based shared cache mach-o images we just want
	     the EXT symbols since the UNDEF symbols do not immediately 
	     follow the EXT symbols.  */
	  num_nonlocalstabs = seg->nextdefsym;
	}
      else
	{
	  /* For all others the EXT and UNDEF symbols are contiguous.  */
	  num_nonlocalstabs = seg->nextdefsym + seg->nundefsym;
	}

      if (num_nonlocalstabs > 0)
	{
	  prefix = "LC_DYSYMTAB.nonlocalstabs";
	  
	  sname = (char *) bfd_alloc (abfd, strlen (prefix) + 1);
	  if (sname == NULL)
	    return -1;
	  strcpy (sname, prefix);

	  bfdsec = bfd_make_section_anyway (abfd, sname);
	  if (bfdsec == NULL)
	    return -1;

	  bfdsec->vma = 0;
	  bfdsec->lma = 0;
	  bfdsec->size = (num_nonlocalstabs * nlist_size);
	  bfdsec->filepos =  stabs_pseudo_section->filepos + 
			     (seg->iextdefsym * nlist_size);
	  bfdsec->alignment_power = 0;
	  bfdsec->flags = SEC_HAS_CONTENTS;
	}
    }
  return 0;
}

static int
bfd_mach_o_scan_read_symtab (bfd *abfd, bfd_mach_o_load_command *command)
{
  bfd_mach_o_symtab_command *seg = &command->command.symtab;
  unsigned char buf[16];
  asection *bfdsec;
  char *sname;
  const char *prefix = "LC_SYMTAB.stabs";
  int nlist_size = (bfd_mach_o_version (abfd) > 1) ? 16 : 12;

  BFD_ASSERT (command->type == BFD_MACH_O_LC_SYMTAB);

  bfd_seek (abfd, command->offset + 8, SEEK_SET);
  if (bfd_bread ((PTR) buf, 16, abfd) != 16)
    return -1;

  seg->symoff = bfd_h_get_32 (abfd, buf);
  seg->nsyms = bfd_h_get_32 (abfd, buf + 4);
  seg->stroff = bfd_h_get_32 (abfd, buf + 8);
  seg->strsize = bfd_h_get_32 (abfd, buf + 12);
  seg->symbols = NULL;
  seg->strtab = NULL;

  sname = (char *) bfd_alloc (abfd, strlen (prefix) + 1);
  if (sname == NULL)
    return -1;
  strcpy (sname, prefix);

  bfdsec = bfd_make_section_anyway (abfd, sname);
  if (bfdsec == NULL)
    return -1;

  bfdsec->vma = 0;
  bfdsec->lma = 0;
  bfdsec->size = seg->nsyms * nlist_size;
  bfdsec->filepos = seg->symoff;
  bfdsec->alignment_power = 0;
  bfdsec->flags = SEC_HAS_CONTENTS;

  seg->stabs_segment = bfdsec;

  prefix = "LC_SYMTAB.stabstr";
  sname = (char *) bfd_alloc (abfd, strlen (prefix) + 1);
  if (sname == NULL)
    return -1;
  strcpy (sname, prefix);

  bfdsec = bfd_make_section_anyway (abfd, sname);
  if (bfdsec == NULL)
    return -1;

  bfdsec->vma = 0;
  bfdsec->lma = 0;
  bfdsec->size = seg->strsize;
  bfdsec->filepos = seg->stroff;
  bfdsec->alignment_power = 0;
  bfdsec->flags = SEC_HAS_CONTENTS;

  seg->stabstr_segment = bfdsec;

  return 0;
}

static int
bfd_mach_o_scan_read_segment (bfd *abfd,
			      bfd_mach_o_load_command *command,
			      unsigned int wide)
{
  unsigned char buf[64];
  bfd_mach_o_segment_command *seg = &command->command.segment;
  unsigned long i;
  asection *bfdsec;
  char *sname;
  const char *prefix = "LC_SEGMENT";
  unsigned int snamelen;

  if (wide)
    {
      BFD_ASSERT (command->type == BFD_MACH_O_LC_SEGMENT_64);

      bfd_seek (abfd, command->offset + 8, SEEK_SET);
      if (bfd_bread ((PTR) buf, 64, abfd) != 64)
	return -1;

      memcpy (seg->segname, buf, 16);

      seg->vmaddr = bfd_h_get_64 (abfd, buf + 16);
      seg->vmsize = bfd_h_get_64 (abfd, buf + 24);
      seg->fileoff = bfd_h_get_64 (abfd, buf + 32);
      seg->filesize = bfd_h_get_64 (abfd, buf + 40);
      seg->maxprot = bfd_h_get_32 (abfd, buf + 48);
      seg->initprot = bfd_h_get_32 (abfd, buf + 52);
      seg->nsects = bfd_h_get_32 (abfd, buf + 56);
      seg->flags = bfd_h_get_32 (abfd, buf + 60);
    }
  else
    {
      BFD_ASSERT (command->type == BFD_MACH_O_LC_SEGMENT);

      bfd_seek (abfd, command->offset + 8, SEEK_SET);
      if (bfd_bread ((PTR) buf, 48, abfd) != 48)
	return -1;

      memcpy (seg->segname, buf, 16);

      seg->vmaddr = bfd_h_get_32 (abfd, buf + 16);
      seg->vmsize = bfd_h_get_32 (abfd, buf + 20);
      seg->fileoff = bfd_h_get_32 (abfd, buf + 24);
      seg->filesize = bfd_h_get_32 (abfd, buf +  28);
      seg->maxprot = bfd_h_get_32 (abfd, buf + 32);
      seg->initprot = bfd_h_get_32 (abfd, buf + 36);
      seg->nsects = bfd_h_get_32 (abfd, buf + 40);
      seg->flags = bfd_h_get_32 (abfd, buf + 44);
    }

  snamelen = strlen (prefix) + 1 + strlen (seg->segname) + 1;
  sname = (char *) bfd_alloc (abfd, snamelen);
  if (sname == NULL)
    return -1;
  sprintf (sname, "%s.%s", prefix, seg->segname);

  bfdsec = bfd_make_section_anyway (abfd, sname);
  if (bfdsec == NULL)
    return -1;

  bfdsec->vma = seg->vmaddr;
  bfdsec->lma = seg->vmaddr;
  bfdsec->size = seg->filesize;
  bfdsec->filepos = seg->fileoff;
  bfdsec->alignment_power = 0x0;
  bfdsec->flags = SEC_HAS_CONTENTS | SEC_LOAD | SEC_ALLOC | SEC_CODE;
  bfdsec->segment_mark = 1;

  seg->segment = bfdsec;

  if (seg->nsects != 0)
    {
      seg->sections =
	((bfd_mach_o_section *)
	 bfd_alloc (abfd, seg->nsects * sizeof (bfd_mach_o_section)));
      if (seg->sections == NULL)
	return -1;

      for (i = 0; i < seg->nsects; i++)
	{
	  bfd_vma segoff;
	  if (wide)
	    segoff = command->offset + 64 + 8 + (i * 80);
	  else
	    segoff = command->offset + 48 + 8 + (i * 68);

	  if (bfd_mach_o_scan_read_section
	      (abfd, &seg->sections[i], segoff, wide) != 0)
	    return -1;
	}
    }

  return 0;
}

static int
bfd_mach_o_scan_read_segment_32 (bfd *abfd, bfd_mach_o_load_command *command)
{
  return bfd_mach_o_scan_read_segment (abfd, command, 0);
}

static int
bfd_mach_o_scan_read_segment_64 (bfd *abfd, bfd_mach_o_load_command *command)
{
  return bfd_mach_o_scan_read_segment (abfd, command, 1);
}

static int
bfd_mach_o_scan_read_command (bfd *abfd, bfd_mach_o_load_command *command)
{
  unsigned char buf[8];

  bfd_seek (abfd, command->offset, SEEK_SET);
  if (bfd_bread ((PTR) buf, 8, abfd) != 8)
    return -1;

  command->type = (bfd_h_get_32 (abfd, buf));
  command->type_required = (bfd_h_get_32 (abfd, buf) & BFD_MACH_O_LC_REQ_DYLD
			    ? 1 : 0);
  command->len = bfd_h_get_32 (abfd, buf + 4);

  switch (command->type)
    {
    case BFD_MACH_O_LC_SEGMENT:
      if (bfd_mach_o_scan_read_segment_32 (abfd, command) != 0)
	return -1;
      break;
    case BFD_MACH_O_LC_SEGMENT_64:
      if (bfd_mach_o_scan_read_segment_64 (abfd, command) != 0)
	return -1;
      break;
    case BFD_MACH_O_LC_SYMTAB:
      if (bfd_mach_o_scan_read_symtab (abfd, command) != 0)
	return -1;
      break;
    case BFD_MACH_O_LC_SYMSEG:
      break;
    case BFD_MACH_O_LC_THREAD:
    case BFD_MACH_O_LC_UNIXTHREAD:
      if (bfd_mach_o_scan_read_thread (abfd, command) != 0)
	return -1;
      break;
    case BFD_MACH_O_LC_LOAD_DYLINKER:
    case BFD_MACH_O_LC_ID_DYLINKER:
      if (bfd_mach_o_scan_read_dylinker (abfd, command) != 0)
	return -1;
      break;
    case BFD_MACH_O_LC_LOAD_DYLIB:
    case BFD_MACH_O_LC_ID_DYLIB:
    case BFD_MACH_O_LC_LOAD_WEAK_DYLIB:
    case BFD_MACH_O_LC_REEXPORT_DYLIB:
      if (bfd_mach_o_scan_read_dylib (abfd, command) != 0)
	return -1;
      break;
    case BFD_MACH_O_LC_PREBOUND_DYLIB:
      if (bfd_mach_o_scan_read_prebound_dylib (abfd, command) != 0)
	return -1;
      break;
    case BFD_MACH_O_LC_LOADFVMLIB:
    case BFD_MACH_O_LC_IDFVMLIB:
    case BFD_MACH_O_LC_IDENT:
    case BFD_MACH_O_LC_FVMFILE:
    case BFD_MACH_O_LC_PREPAGE:
    case BFD_MACH_O_LC_ROUTINES:
    case BFD_MACH_O_LC_SUB_FRAMEWORK:
      break;
    case BFD_MACH_O_LC_DYSYMTAB:
      if (bfd_mach_o_scan_read_dysymtab (abfd, command) != 0)
	return -1;
      break;
    case BFD_MACH_O_LC_UUID:
      {
	bfd_mach_o_data_struct *mdata = abfd->tdata.mach_o_data;
	if (bfd_bread ((PTR) &mdata->uuid[0], 
                       sizeof (mdata->uuid), abfd) != sizeof (mdata->uuid))
	  return -1;
      }
      break;

    case BFD_MACH_O_LC_SUB_UMBRELLA:
    case BFD_MACH_O_LC_SUB_CLIENT:
    case BFD_MACH_O_LC_SUB_LIBRARY:
    case BFD_MACH_O_LC_TWOLEVEL_HINTS:
    case BFD_MACH_O_LC_PREBIND_CKSUM:
    case BFD_MACH_O_LC_ROUTINES_64:
    case BFD_MACH_O_LC_RPATH:
    case BFD_MACH_O_LC_CODE_SIGNATURE:
    case BFD_MACH_O_LC_SEGMENT_SPLIT_INFO:
    case BFD_MACH_O_LC_LAZY_LOAD_DYLIB:
    case BFD_MACH_O_LC_DYLD_INFO:
    case BFD_MACH_O_LC_DYLD_INFO_ONLY:
      break;
    case BFD_MACH_O_LC_ENCRYPTION_INFO:
      {
	char cryptid_buf[4];

	bfd_seek (abfd, command->offset + 16, SEEK_SET);
	if (bfd_bread ((PTR) cryptid_buf, 4, abfd) != 4)
	  return -1;
	
	abfd->tdata.mach_o_data->encrypted = (bfd_h_get_32 (abfd, cryptid_buf));
      }
      break;
    default:
      fprintf (stderr, "unable to read unknown load command 0x%lx\n",
	       (unsigned long) command->type);
      break;
    }

  return 0;
}

static void
bfd_mach_o_flatten_sections (bfd *abfd)
{
  bfd_mach_o_data_struct *mdata = abfd->tdata.mach_o_data;
  long csect = 0;
  unsigned long i, j;

  mdata->nsects = 0;

  for (i = 0; i < mdata->header.ncmds; i++)
    {
      if (mdata->commands[i].type == BFD_MACH_O_LC_SEGMENT
	  || mdata->commands[i].type == BFD_MACH_O_LC_SEGMENT_64)
	{
	  bfd_mach_o_segment_command *seg;

	  seg = &mdata->commands[i].command.segment;
	  mdata->nsects += seg->nsects;
	}
    }

  mdata->sections = bfd_alloc (abfd,
			       mdata->nsects * sizeof (bfd_mach_o_section *));
  csect = 0;

  for (i = 0; i < mdata->header.ncmds; i++)
    {
      if (mdata->commands[i].type == BFD_MACH_O_LC_SEGMENT
	  || mdata->commands[i].type == BFD_MACH_O_LC_SEGMENT_64)
	{
	  bfd_mach_o_segment_command *seg;

	  seg = &mdata->commands[i].command.segment;
	  BFD_ASSERT (csect + seg->nsects <= mdata->nsects);

	  for (j = 0; j < seg->nsects; j++)
	    mdata->sections[csect++] = &seg->sections[j];
	}
    }
}

int
bfd_mach_o_scan_start_address (bfd *abfd)
{
  bfd_mach_o_data_struct *mdata = abfd->tdata.mach_o_data;
  bfd_mach_o_thread_command *cmd = NULL;
  unsigned long i;

  /* dyld for instance DOES have LC_UNIXTHREAD commands - the kernel
     needs this to load dyld and let it go - which in turn loads the
     binary...  But this isn't what we mean by the start address.  
     You can't have a start address if you aren't an executable...  */

  if (mdata->header.filetype != BFD_MACH_O_MH_EXECUTE)
    return 0;

  for (i = 0; i < mdata->header.ncmds; i++)
    {
      if ((mdata->commands[i].type == BFD_MACH_O_LC_THREAD) ||
	  (mdata->commands[i].type == BFD_MACH_O_LC_UNIXTHREAD))
	{
	  if (cmd == NULL)
	    cmd = &mdata->commands[i].command.thread;
	  else
	    return 0;
	}
    }

  if (cmd == NULL)
    return 0;

  for (i = 0; i < cmd->nflavours; i++)
    {
      if ((mdata->header.cputype == BFD_MACH_O_CPU_TYPE_I386)
	  && (cmd->flavours[i].flavour
	      == (unsigned long) BFD_MACH_O_i386_THREAD_STATE))
	{
	  unsigned char buf[4];

	  bfd_seek (abfd, cmd->flavours[i].offset + 40, SEEK_SET);

	  if (bfd_bread (buf, 4, abfd) != 4)
	    return -1;

	  abfd->start_address = bfd_h_get_32 (abfd, buf);
	}
      else if ((mdata->header.cputype == BFD_MACH_O_CPU_TYPE_POWERPC)
	       && (cmd->flavours[i].flavour == BFD_MACH_O_PPC_THREAD_STATE))
	{
	  unsigned char buf[4];

	  bfd_seek (abfd, cmd->flavours[i].offset + 0, SEEK_SET);

	  if (bfd_bread (buf, 4, abfd) != 4)
	    return -1;

	  abfd->start_address = bfd_h_get_32 (abfd, buf);
	}
      else if ((mdata->header.cputype == BFD_MACH_O_CPU_TYPE_POWERPC_64)
               && (cmd->flavours[i].flavour == BFD_MACH_O_PPC_THREAD_STATE_64))
        {
          unsigned char buf[8];

          bfd_seek (abfd, cmd->flavours[i].offset + 0, SEEK_SET);

          if (bfd_bread (buf, 8, abfd) != 8)
            return -1;

          abfd->start_address = bfd_h_get_64 (abfd, buf);
        }
      else if ((mdata->header.cputype == BFD_MACH_O_CPU_TYPE_ARM)
               && (cmd->flavours[i].flavour == BFD_MACH_O_ARM_THREAD_STATE))
        {
          unsigned char buf[8];
	  
          bfd_seek (abfd, cmd->flavours[i].offset + 60, SEEK_SET);
	  
          if (bfd_bread (buf, 4, abfd) != 4)
            return -1;
	  
          abfd->start_address = bfd_h_get_32 (abfd, buf);
        }
      
      /* APPLE LOCAL begin x86_64 */
      else if ((mdata->header.cputype == BFD_MACH_O_CPU_TYPE_X86_64)
               && (cmd->flavours[i].flavour == BFD_MACH_O_x86_THREAD_STATE64))
        {
          unsigned char buf[8];

          bfd_seek (abfd, cmd->flavours[i].offset + (16 * 8), SEEK_SET);

          if (bfd_bread (buf, 8, abfd) != 8)
            return -1;

          abfd->start_address = bfd_h_get_64 (abfd, buf);
        }
      /* APPLE LOCAL end x86_64 */

    }

  return 0;
}

int
bfd_mach_o_scan (bfd *abfd,
		 bfd_mach_o_header *header,
		 bfd_mach_o_data_struct *mdata)
{
  unsigned int i;
  enum bfd_architecture cputype;
  unsigned long cpusubtype;

  unsigned int hdrsize;

  hdrsize = (header->version == 2) ? 32 : 28;

  mdata->header = *header;
  mdata->symbols = NULL;
  mdata->scanning_load_cmds = 1;

  abfd->flags = (abfd->xvec->object_flags
		 | (abfd->flags & (BFD_IN_MEMORY | BFD_IO_FUNCS)));
  abfd->tdata.mach_o_data = mdata;

  bfd_mach_o_convert_architecture (header->cputype, header->cpusubtype,
				   &cputype, &cpusubtype);
  if (cputype == bfd_arch_unknown)
    {
      fprintf (stderr, "bfd_mach_o_scan: unknown architecture 0x%lx/0x%lx\n",
	       header->cputype, header->cpusubtype);
      return -1;
    }

  bfd_set_arch_mach (abfd, cputype, cpusubtype);

  /* obj_symbol_entry_size (abfd) = (header->version == 2) ? 16 : 12; */

  if (header->ncmds != 0)
    {
      /* Use zalloc so we set all the "type" fields to 0 - we use that
        to indicate that we have not read the command data for that
        command in yet.  */
      mdata->commands =
	((bfd_mach_o_load_command *)
	 bfd_zalloc (abfd, header->ncmds * sizeof (bfd_mach_o_load_command)));
      if (mdata->commands == NULL)
	return -1;

      for (i = 0; i < header->ncmds; i++)
	{
	  bfd_mach_o_load_command *cur = &mdata->commands[i];

	  if (i == 0)
	    cur->offset = hdrsize;
	  else
	    {
	      bfd_mach_o_load_command *prev = &mdata->commands[i - 1];
	      cur->offset = prev->offset + prev->len;
	    }

	  if (bfd_mach_o_scan_read_command (abfd, cur) < 0)
	    return -1;
	}
    }

  if (bfd_mach_o_scan_start_address (abfd) < 0)
    {
#if 0
      fprintf (stderr, "bfd_mach_o_scan: unable to scan start address: %s\n",
	       bfd_errmsg (bfd_get_error ()));
      abfd->tdata.mach_o_data = NULL;
      return -1;
#endif
    }

  bfd_mach_o_flatten_sections (abfd);
  mdata->scanning_load_cmds = 0;

  return 0;
}

bfd_boolean
bfd_mach_o_mkobject (bfd *abfd)
{
  bfd_mach_o_data_struct *mdata = NULL;

  mdata = ((bfd_mach_o_data_struct *)
	   bfd_alloc (abfd, sizeof (bfd_mach_o_data_struct)));
  if (mdata == NULL)
    return FALSE;
  abfd->tdata.mach_o_data = mdata;

  mdata->header.magic = 0;
  mdata->header.cputype = 0;
  mdata->header.cpusubtype = 0;
  mdata->header.filetype = 0;
  mdata->header.ncmds = 0;
  mdata->header.sizeofcmds = 0;
  mdata->header.flags = 0;
  mdata->header.byteorder = BFD_ENDIAN_UNKNOWN;
  mdata->commands = NULL;
  mdata->nsymbols = 0;
  mdata->symbols = NULL;
  mdata->nsects = 0;
  mdata->sections = NULL;
  mdata->ibfd = NULL;
  mdata->encrypted = 0;

  return TRUE;
}

const bfd_target *
bfd_mach_o_object_p (bfd *abfd)
{
  struct bfd_preserve preserve;
  bfd_mach_o_header header;

  preserve.marker = NULL;
  if (bfd_mach_o_read_header (abfd, &header) != 0)
    goto wrong;

  if (! (header.byteorder == BFD_ENDIAN_BIG
	 || header.byteorder == BFD_ENDIAN_LITTLE))
    {
      fprintf (stderr, "unknown header byte-order value 0x%lx\n",
	       (long) header.byteorder);
      goto wrong;
    }

  if (! ((header.byteorder == BFD_ENDIAN_BIG
	  && abfd->xvec->byteorder == BFD_ENDIAN_BIG
	  && abfd->xvec->header_byteorder == BFD_ENDIAN_BIG)
	 || (header.byteorder == BFD_ENDIAN_LITTLE
	     && abfd->xvec->byteorder == BFD_ENDIAN_LITTLE
	     && abfd->xvec->header_byteorder == BFD_ENDIAN_LITTLE)))
    goto wrong;

  preserve.marker = bfd_zalloc (abfd, sizeof (bfd_mach_o_data_struct));
  if (preserve.marker == NULL
      || !bfd_preserve_save (abfd, &preserve))
    goto fail;

  if (bfd_mach_o_scan (abfd, &header,
		       (bfd_mach_o_data_struct *) preserve.marker) != 0)
    goto wrong;

  bfd_preserve_finish (abfd, &preserve);
  return abfd->xvec;

 wrong:
  bfd_set_error (bfd_error_wrong_format);

 fail:
  if (preserve.marker != NULL)
    bfd_preserve_restore (abfd, &preserve);
  return NULL;
}

const bfd_target *
bfd_mach_o_core_p (bfd *abfd)
{
  struct bfd_preserve preserve;
  bfd_mach_o_header header;

  preserve.marker = NULL;
  if (bfd_mach_o_read_header (abfd, &header) != 0)
    goto wrong;

  if (! (header.byteorder == BFD_ENDIAN_BIG
	 || header.byteorder == BFD_ENDIAN_LITTLE))
    {
      fprintf (stderr, "unknown header byte-order value 0x%lx\n",
	       (long) header.byteorder);
      abort ();
    }

  if (! ((header.byteorder == BFD_ENDIAN_BIG
	  && abfd->xvec->byteorder == BFD_ENDIAN_BIG
	  && abfd->xvec->header_byteorder == BFD_ENDIAN_BIG)
	 || (header.byteorder == BFD_ENDIAN_LITTLE
	     && abfd->xvec->byteorder == BFD_ENDIAN_LITTLE
	     && abfd->xvec->header_byteorder == BFD_ENDIAN_LITTLE)))
    goto wrong;

  if (header.filetype != BFD_MACH_O_MH_CORE)
    goto wrong;

  preserve.marker = bfd_zalloc (abfd, sizeof (bfd_mach_o_data_struct));
  if (preserve.marker == NULL
      || !bfd_preserve_save (abfd, &preserve))
    goto fail;

  if (bfd_mach_o_scan (abfd, &header,
		       (bfd_mach_o_data_struct *) preserve.marker) != 0)
    goto wrong;

  bfd_preserve_finish (abfd, &preserve);
  return abfd->xvec;

 wrong:
  bfd_set_error (bfd_error_wrong_format);

 fail:
  if (preserve.marker != NULL)
    bfd_preserve_restore (abfd, &preserve);
  return NULL;
}

/* APPLE LOCAL: Return 1 if the bfd is a stub library -- that is, it has had
   its text stripped away and will cause gdb all sorts of problems if it tries
   to read it.  */

int
bfd_mach_o_stub_library (bfd *abfd)
{
  bfd_mach_o_header header;
  if (bfd_mach_o_read_header (abfd, &header) != 0)
    return 0;

  if (header.filetype == BFD_MACH_O_MH_DYLIB_STUB)
    return 1;

  return 0;
}

typedef struct mach_o_fat_archentry
{
  unsigned long cputype;
  unsigned long cpusubtype;
  unsigned long offset;
  unsigned long size;
  unsigned long align;
  bfd *abfd;
} mach_o_fat_archentry;

typedef struct mach_o_fat_data_struct
{
  unsigned long magic;
  unsigned long nfat_arch;
  mach_o_fat_archentry *archentries;
} mach_o_fat_data_struct;

/* This function (and the way it is called) are for "archives", which
   might lead you to think of ranlib style .a files, but this is really
   to detect whether the bfd is a (MachO) fat file or not. */

const bfd_target *
bfd_mach_o_archive_p (bfd *abfd)
{
  mach_o_fat_data_struct *adata = NULL;
  unsigned char buf[20];
  unsigned long i;

  bfd_seek (abfd, 0, SEEK_SET);
  if (bfd_bread ((PTR) buf, 8, abfd) != 8)
    goto error;

  adata = (mach_o_fat_data_struct *)
    bfd_alloc (abfd, sizeof (mach_o_fat_data_struct));
  if (adata == NULL)
    goto error;

  adata->magic = bfd_getb32 (buf);
  adata->nfat_arch = bfd_getb32 (buf + 4);
  if (adata->magic != 0xcafebabe)
    goto error;

  adata->archentries = (mach_o_fat_archentry *)
    bfd_alloc (abfd, adata->nfat_arch * sizeof (mach_o_fat_archentry));
  if (adata->archentries == NULL)
    goto error;

  for (i = 0; i < adata->nfat_arch; i++)
    {
      bfd_seek (abfd, 8 + 20 * i, SEEK_SET);

      if (bfd_bread ((PTR) buf, 20, abfd) != 20)
	goto error;
      adata->archentries[i].cputype = bfd_getb32 (buf);
      adata->archentries[i].cpusubtype = bfd_getb32 (buf + 4);
      adata->archentries[i].offset = bfd_getb32 (buf + 8);
      adata->archentries[i].size = bfd_getb32 (buf + 12);
      adata->archentries[i].align = bfd_getb32 (buf + 16);
      adata->archentries[i].abfd = NULL;
    }

  abfd->tdata.mach_o_fat_data = adata;
  return abfd->xvec;

 error:
  if (adata != NULL)
    bfd_release (abfd, adata);
  bfd_set_error (bfd_error_wrong_format);
  return NULL;
}

bfd *
bfd_mach_o_openr_next_archived_file (bfd *archive, bfd *prev)
{
  mach_o_fat_data_struct *adata;
  mach_o_fat_archentry *entry = NULL;
  unsigned long i;

  adata = (mach_o_fat_data_struct *) archive->tdata.mach_o_fat_data;
  BFD_ASSERT (adata != NULL);

  /* Find index of previous entry.  */
  if (prev == NULL)
    i = 0;	/* Start at first one.  */
  else
    {
      for (i = 0; i < adata->nfat_arch; i++)
	{
	  if (adata->archentries[i].abfd == prev)
	    break;
	}

      if (i == adata->nfat_arch)
	{
	  /* Not found.  */
	  bfd_set_error (bfd_error_bad_value);
	  return NULL;
	}
    i++;	/* Get next entry.  */
  }

  if (i >= adata->nfat_arch)
    {
      bfd_set_error (bfd_error_no_more_archived_files);
      return NULL;
    }

  entry = &adata->archentries[i];
  if (entry->abfd == NULL)
    {
      bfd *nbfd = _bfd_new_bfd_contained_in (archive);
      char *s = NULL;

      if (nbfd == NULL)
	return NULL;

      nbfd->origin = entry->offset;
      s = bfd_malloc (strlen (archive->filename) + 1);
      if (s == NULL)
	return NULL;
      strcpy (s, archive->filename);
      nbfd->filename = s;
      nbfd->iostream = NULL;
      entry->abfd = nbfd;
#ifdef BFD_TRACK_OPEN_CLOSE
  printf ("Opening 0x%lx from FAT archive 0x%lx: \"%s\"\n", (unsigned long) entry->abfd,
	  (unsigned long) archive, entry->abfd->filename);
#endif
    }
#ifdef BFD_TRACK_OPEN_CLOSE
  else
    printf ("Opening 0x%lx from FAT archive cache 0x%lx: \"%s\"\n", 
	    (unsigned long) entry->abfd,
	    (unsigned long) archive, entry->abfd->filename);
#endif

  return entry->abfd;
}

int
bfd_mach_o_lookup_section (bfd *abfd,
			   asection *section,
			   bfd_mach_o_load_command **mcommand,
			   bfd_mach_o_section **msection)
{
  struct mach_o_data_struct *md = abfd->tdata.mach_o_data;
  unsigned int i, j, num;

  bfd_mach_o_load_command *ncmd = NULL;
  bfd_mach_o_section *nsect = NULL;

  BFD_ASSERT (mcommand != NULL);
  BFD_ASSERT (msection != NULL);

  num = 0;
  for (i = 0; i < md->header.ncmds; i++)
    {
      struct bfd_mach_o_load_command *cmd = &md->commands[i];
      struct bfd_mach_o_segment_command *seg = NULL;

      if (cmd->type != BFD_MACH_O_LC_SEGMENT
	  && cmd->type != BFD_MACH_O_LC_SEGMENT_64)
	continue;
      seg = &cmd->command.segment;

      if (seg->segment == section)
	{
	  if (num == 0)
	    ncmd = cmd;
	  num++;
	}

      for (j = 0; j < seg->nsects; j++)
	{
	  struct bfd_mach_o_section *sect = &seg->sections[j];

	  if (sect->bfdsection == section)
	    {
	      if (num == 0)
		nsect = sect;
	      num++;
	    }
	}
    }

  *mcommand = ncmd;
  *msection = nsect;
  return num;
}

int
bfd_mach_o_lookup_command (bfd *abfd,
			   bfd_mach_o_load_command_type type,
			   bfd_mach_o_load_command **mcommand)
{
  struct mach_o_data_struct *md = NULL;
  bfd_mach_o_load_command *ncmd = NULL;
  unsigned int i, num;

  md = abfd->tdata.mach_o_data;

  BFD_ASSERT (md != NULL);
  BFD_ASSERT (mcommand != NULL);

  num = 0;
  for (i = 0; i < md->header.ncmds; i++)
    {
      struct bfd_mach_o_load_command *cmd = &md->commands[i];

      if (cmd->type != type)
	continue;

      if (num == 0)
	ncmd = cmd;
      num++;
    }

  *mcommand = ncmd;
  return num;
}

unsigned long
bfd_mach_o_stack_addr (enum bfd_mach_o_cpu_type type)
{
  switch (type)
    {
    case BFD_MACH_O_CPU_TYPE_MC680x0:
      return 0x04000000;
    case BFD_MACH_O_CPU_TYPE_MC88000:
      return 0xffffe000;
    case BFD_MACH_O_CPU_TYPE_POWERPC:
      return 0xc0000000;
    case BFD_MACH_O_CPU_TYPE_I386:
      return 0xc0000000;
    case BFD_MACH_O_CPU_TYPE_ARM:
      return 0x40000000;
    case BFD_MACH_O_CPU_TYPE_SPARC:
      return 0xf0000000;
    case BFD_MACH_O_CPU_TYPE_I860:
      return 0;
    case BFD_MACH_O_CPU_TYPE_HPPA:
      return 0xc0000000 - 0x04000000;
    default:
      return 0;
    }
}

/* Look for a valid environment string in the data bounded by BUF and
   LEN.  A "valid environment string" means that it's got a NULL at
   the end, and four NULLS at the beginning.  Store the resulting
   string in RBUF and RLEN, without allocating any new memory.

   If we can't find the end of the environment string (there are no
   null characters), return -1.  If we found the end of the
   environment string, but not the beginning, return -2.  Otherwise
   return 0, and store the pointer into BUF and length in RBUF and
   RLEN. */

static int
bfd_mach_o_core_parse_environment (bfd *abfd ATTRIBUTE_UNUSED,
				   unsigned char *buf,
				   bfd_size_type len,
				   unsigned char **rbuf,
				   bfd_size_type *rlen)
{
  unsigned char *end, *start;

  /* Find the first non-NULL character in the string. */
  end = buf + len - 1;
  while ((end > buf) && (*end == '\0'))
    end--;
  if ((end <= buf) && (*end == '\0'))
    return -1;
  end++;

  /* Back up until we find four consecutive NULL characters (note they
     need not be 4-byte-aligned). */
  for (start = end; start >= (buf + 4); start--)
    {
      if ((start[-1] == '\0') 
	  && (start[-2] == '\0')
	  && (start[-3] == '\0')
	  && (start[-4] == '\0'))
	{
	  break;
	}
    }
  if (start < (buf + 4))
    return -2;

  *rlen = (end - start);
  *rbuf = start;

  return 0;
}

bfd_boolean
bfd_mach_o_core_fetch_environment (bfd *abfd,
				   unsigned char **rbuf,
				   bfd_size_type *rlen)
{
  bfd_mach_o_data_struct *mdata = abfd->tdata.mach_o_data;
  unsigned long stackaddr = bfd_mach_o_stack_addr (mdata->header.cputype);
  unsigned int i = 0;

  for (i = 0; i < mdata->header.ncmds; i++)
    {
      bfd_mach_o_load_command *cur = &mdata->commands[i];
      bfd_mach_o_segment_command *seg = NULL;

      if (cur->type != BFD_MACH_O_LC_SEGMENT
	  && cur->type != BFD_MACH_O_LC_SEGMENT_64)
	continue;

      seg = &cur->command.segment;

      if ((seg->vmaddr + seg->vmsize) == stackaddr)
	{
	  bfd_size_type start = seg->fileoff;
	  bfd_size_type end = seg->fileoff + seg->filesize;
	  unsigned char *buf = bfd_malloc (1024 * 1);
	  bfd_size_type size = 1024 * 1;

	  for (;;)
	    {
	      bfd_size_type nread = 0;
	      int ret;

	      unsigned char *nrbuf;
	      bfd_size_type nrlen;

	      if (size > (end - start))
		size = (end - start);

	      buf = bfd_realloc (buf, size);

	      bfd_seek (abfd, end - size, SEEK_SET);
	      nread = bfd_bread (buf, size, abfd);

	      if (nread != size)
		return FALSE;

	      ret = bfd_mach_o_core_parse_environment
		(abfd, buf, size, &nrbuf, &nrlen);

	      if (ret == 0)
		{
		  *rbuf = bfd_malloc (nrlen);
		  memcpy (*rbuf, nrbuf, nrlen);
		  *rlen = nrlen;
		  free (buf);
		  return TRUE;
		}

	      if (size == (end - start))
		{
		  free (buf);
		  return FALSE;
		}
	      
	      size *= 2;
	    }
	}
    }
  /* If we get here, it means we didn't find
     the segment before the stack. */
  return FALSE;
}

char *
bfd_mach_o_core_file_failing_command (bfd *abfd)
{
  unsigned char *buf = NULL;
  bfd_size_type len = 0;
  char *p;
  int ret = -1;

  ret = bfd_mach_o_core_fetch_environment (abfd, &buf, &len);
  if (ret <= 0)
    return NULL;

  p = xstrdup ((char *) buf);
  free (buf);

  return p;
}

int
bfd_mach_o_core_file_failing_signal (bfd *abfd ATTRIBUTE_UNUSED)
{
  return 0;
}

bfd_boolean
bfd_mach_o_core_file_matches_executable_p (bfd *core_bfd, bfd *exec_bfd)
{
  if (core_bfd->tdata.mach_o_data->header.cputype == exec_bfd->tdata.mach_o_data->header.cputype)
    return TRUE;
  else
    return FALSE;
}

bfd_boolean        
bfd_mach_o_get_uuid (bfd *abfd, unsigned char *buf, unsigned long buf_len)
{
  if (bfd_get_flavour (abfd) == bfd_target_mach_o_flavour)
    {
      unsigned i;
      bfd_mach_o_data_struct *mdata = abfd->tdata.mach_o_data;
      const unsigned k_uuid_size = sizeof (mdata->uuid);
      /* Return true if we have any non-zero uuid bytes.  */
      for (i = 0; i < k_uuid_size; ++i)
	{
	  if (mdata->uuid[i] != 0)
	    {
	      memcpy (buf, 
		      &mdata->uuid[0], 
		      buf_len > k_uuid_size ? k_uuid_size : buf_len);
	      return TRUE;
	    }
	}
    }
  return FALSE;
}

/* Return TRUE if ABFD is an encrypted binary.  In this case, gdb
   won't want to look at the contents of the binary on disk, but 
   rather read it from memory.  */

bfd_boolean
bfd_mach_o_encrypted_binary (bfd *abfd)
{
  if (abfd->tdata.mach_o_data->encrypted == 0)
    return FALSE;
  else
    return TRUE;
}

/* Add free_cached_info functions so we can actually close the
   bfd's that we opened when looking through archives.  Since we
   leave the debug info in the .a files, gdb ends up accessing .a
   files quite a lot.  Leaking them is very bad.  */

/* The thin version closes all the member bfd's.  */
static bfd_boolean
mach_o_bfd_thin_free_cached_info (bfd *input)
{
  if (bfd_check_format (input, bfd_archive))
    bfd_archive_free_cached_info (input);
  return TRUE;
}

/* The fat version calls free_cached_info on all the member
   archives, and then closes them.  */
static bfd_boolean
mach_o_bfd_fat_free_cached_info (bfd *input)
{
  if (bfd_check_format (input, bfd_archive))
    {
      unsigned int i;
      mach_o_fat_data_struct *adata 
	= (mach_o_fat_data_struct *) input->tdata.mach_o_fat_data;
      for (i = 0; i < adata->nfat_arch; i++)
	{
	  if (adata->archentries[i].abfd != NULL)
	    {
	      bfd_free_cached_info (adata->archentries[i].abfd);
	      bfd_close (adata->archentries[i].abfd);
	      adata->archentries[i].abfd = NULL;
	    }
	}
    }
  return TRUE;

}

#define bfd_mach_o_bfd_free_cached_info mach_o_bfd_thin_free_cached_info 

#define TARGET_NAME 		mach_o_be_vec
#define TARGET_STRING     	"mach-o-be"
#define TARGET_BIG_ENDIAN 	1
#define TARGET_ARCHIVE 		0

#include "mach-o-target.c"

#undef TARGET_NAME
#undef TARGET_STRING
#undef TARGET_BIG_ENDIAN
#undef TARGET_ARCHIVE

#define TARGET_NAME 		mach_o_le_vec
#define TARGET_STRING 		"mach-o-le"
#define TARGET_BIG_ENDIAN 	0
#define TARGET_ARCHIVE 		0

#include "mach-o-target.c"

#undef TARGET_NAME
#undef TARGET_STRING
#undef TARGET_BIG_ENDIAN
#undef TARGET_ARCHIVE

#define TARGET_NAME 		mach_o_fat_vec
#define TARGET_STRING 		"mach-o-fat"
#define TARGET_BIG_ENDIAN 	1
#define TARGET_ARCHIVE 		1

#undef bfd_mach_o_bfd_free_cached_info
#define bfd_mach_o_bfd_free_cached_info               mach_o_bfd_fat_free_cached_info

#include "mach-o-target.c"

#undef TARGET_NAME
#undef TARGET_STRING
#undef TARGET_BIG_ENDIAN
#undef TARGET_ARCHIVE

/* ELF linker support.
   Copyright 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003
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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* ELF linker code.  */

#include "safe-ctype.h"

static bfd_boolean elf_link_add_object_symbols (bfd *, struct bfd_link_info *);
static bfd_boolean elf_finalize_dynstr (bfd *, struct bfd_link_info *);
static bfd_boolean elf_collect_hash_codes (struct elf_link_hash_entry *,
					   void *);
static bfd_boolean elf_section_ignore_discarded_relocs (asection *);

/* Given an ELF BFD, add symbols to the global hash table as
   appropriate.  */

bfd_boolean
elf_bfd_link_add_symbols (bfd *abfd, struct bfd_link_info *info)
{
  switch (bfd_get_format (abfd))
    {
    case bfd_object:
      return elf_link_add_object_symbols (abfd, info);
    case bfd_archive:
      return _bfd_elf_link_add_archive_symbols (abfd, info);
    default:
      bfd_set_error (bfd_error_wrong_format);
      return FALSE;
    }
}

/* Sort symbol by value and section.  */
static int
sort_symbol (const void *arg1, const void *arg2)
{
  const struct elf_link_hash_entry *h1
    = *(const struct elf_link_hash_entry **) arg1;
  const struct elf_link_hash_entry *h2
    = *(const struct elf_link_hash_entry **) arg2;
  bfd_signed_vma vdiff = h1->root.u.def.value - h2->root.u.def.value;

  if (vdiff)
    return vdiff > 0 ? 1 : -1;
  else
    {
      long sdiff = h1->root.u.def.section - h2->root.u.def.section;
      if (sdiff)
	return sdiff > 0 ? 1 : -1;
      else
	return 0;
    }
}

/* Add symbols from an ELF object file to the linker hash table.  */

static bfd_boolean
elf_link_add_object_symbols (bfd *abfd, struct bfd_link_info *info)
{
  bfd_boolean (*add_symbol_hook)
    (bfd *, struct bfd_link_info *, const Elf_Internal_Sym *,
     const char **, flagword *, asection **, bfd_vma *);
  bfd_boolean (*check_relocs)
    (bfd *, struct bfd_link_info *, asection *, const Elf_Internal_Rela *);
  bfd_boolean collect;
  Elf_Internal_Shdr *hdr;
  bfd_size_type symcount;
  bfd_size_type extsymcount;
  bfd_size_type extsymoff;
  struct elf_link_hash_entry **sym_hash;
  bfd_boolean dynamic;
  Elf_External_Versym *extversym = NULL;
  Elf_External_Versym *ever;
  struct elf_link_hash_entry *weaks;
  struct elf_link_hash_entry **nondeflt_vers = NULL;
  bfd_size_type nondeflt_vers_cnt = 0;
  Elf_Internal_Sym *isymbuf = NULL;
  Elf_Internal_Sym *isym;
  Elf_Internal_Sym *isymend;
  const struct elf_backend_data *bed;
  bfd_boolean dt_needed;
  struct elf_link_hash_table * hash_table;
  bfd_size_type amt;

  hash_table = elf_hash_table (info);

  bed = get_elf_backend_data (abfd);
  add_symbol_hook = bed->elf_add_symbol_hook;
  collect = bed->collect;

  if ((abfd->flags & DYNAMIC) == 0)
    dynamic = FALSE;
  else
    {
      dynamic = TRUE;

      /* You can't use -r against a dynamic object.  Also, there's no
	 hope of using a dynamic object which does not exactly match
	 the format of the output file.  */
      if (info->relocatable
	  || !is_elf_hash_table (hash_table)
	  || hash_table->root.creator != abfd->xvec)
	{
	  bfd_set_error (bfd_error_invalid_operation);
	  goto error_return;
	}
    }

  /* As a GNU extension, any input sections which are named
     .gnu.warning.SYMBOL are treated as warning symbols for the given
     symbol.  This differs from .gnu.warning sections, which generate
     warnings when they are included in an output file.  */
  if (info->executable)
    {
      asection *s;

      for (s = abfd->sections; s != NULL; s = s->next)
	{
	  const char *name;

	  name = bfd_get_section_name (abfd, s);
	  if (strncmp (name, ".gnu.warning.", sizeof ".gnu.warning." - 1) == 0)
	    {
	      char *msg;
	      bfd_size_type sz;
	      bfd_size_type prefix_len;
	      const char * gnu_warning_prefix = _("warning: ");

	      name += sizeof ".gnu.warning." - 1;

	      /* If this is a shared object, then look up the symbol
		 in the hash table.  If it is there, and it is already
		 been defined, then we will not be using the entry
		 from this shared object, so we don't need to warn.
		 FIXME: If we see the definition in a regular object
		 later on, we will warn, but we shouldn't.  The only
		 fix is to keep track of what warnings we are supposed
		 to emit, and then handle them all at the end of the
		 link.  */
	      if (dynamic)
		{
		  struct elf_link_hash_entry *h;

		  h = elf_link_hash_lookup (hash_table, name,
					    FALSE, FALSE, TRUE);

		  /* FIXME: What about bfd_link_hash_common?  */
		  if (h != NULL
		      && (h->root.type == bfd_link_hash_defined
			  || h->root.type == bfd_link_hash_defweak))
		    {
		      /* We don't want to issue this warning.  Clobber
			 the section size so that the warning does not
			 get copied into the output file.  */
		      s->_raw_size = 0;
		      continue;
		    }
		}

	      sz = bfd_section_size (abfd, s);
	      prefix_len = strlen (gnu_warning_prefix);
	      msg = bfd_alloc (abfd, prefix_len + sz + 1);
	      if (msg == NULL)
		goto error_return;

	      strcpy (msg, gnu_warning_prefix);
	      if (! bfd_get_section_contents (abfd, s, msg + prefix_len, 0, sz))
		goto error_return;

	      msg[prefix_len + sz] = '\0';

	      if (! (_bfd_generic_link_add_one_symbol
		     (info, abfd, name, BSF_WARNING, s, 0, msg,
		      FALSE, collect, NULL)))
		goto error_return;

	      if (! info->relocatable)
		{
		  /* Clobber the section size so that the warning does
		     not get copied into the output file.  */
		  s->_raw_size = 0;
		}
	    }
	}
    }

  dt_needed = FALSE;
  if (! dynamic)
    {
      /* If we are creating a shared library, create all the dynamic
	 sections immediately.  We need to attach them to something,
	 so we attach them to this BFD, provided it is the right
	 format.  FIXME: If there are no input BFD's of the same
	 format as the output, we can't make a shared library.  */
      if (info->shared
	  && is_elf_hash_table (hash_table)
	  && hash_table->root.creator == abfd->xvec
	  && ! hash_table->dynamic_sections_created)
	{
	  if (! _bfd_elf_link_create_dynamic_sections (abfd, info))
	    goto error_return;
	}
    }
  else if (!is_elf_hash_table (hash_table))
    goto error_return;
  else
    {
      asection *s;
      bfd_boolean add_needed;
      const char *name;
      bfd_size_type oldsize;
      bfd_size_type strindex;
      struct bfd_link_needed_list *rpath = NULL, *runpath = NULL;

      /* ld --just-symbols and dynamic objects don't mix very well.
	 Test for --just-symbols by looking at info set up by
	 _bfd_elf_link_just_syms.  */
      if ((s = abfd->sections) != NULL
	  && s->sec_info_type == ELF_INFO_TYPE_JUST_SYMS)
	goto error_return;

      /* Find the name to use in a DT_NEEDED entry that refers to this
	 object.  If the object has a DT_SONAME entry, we use it.
	 Otherwise, if the generic linker stuck something in
	 elf_dt_name, we use that.  Otherwise, we just use the file
	 name.  If the generic linker put a null string into
	 elf_dt_name, we don't make a DT_NEEDED entry at all, even if
	 there is a DT_SONAME entry.  */
      add_needed = TRUE;
      name = bfd_get_filename (abfd);
      if (elf_dt_name (abfd) != NULL)
	{
	  name = elf_dt_name (abfd);
	  if (*name == '\0')
	    {
	      if (elf_dt_soname (abfd) != NULL)
		dt_needed = TRUE;

	      add_needed = FALSE;
	    }
	}
      s = bfd_get_section_by_name (abfd, ".dynamic");
      if (s != NULL)
	{
	  Elf_External_Dyn *dynbuf = NULL;
	  Elf_External_Dyn *extdyn;
	  Elf_External_Dyn *extdynend;
	  int elfsec;
	  unsigned long shlink;

	  dynbuf = bfd_malloc (s->_raw_size);
	  if (dynbuf == NULL)
	    goto error_return;

	  if (! bfd_get_section_contents (abfd, s, dynbuf, 0, s->_raw_size))
	    goto error_free_dyn;

	  elfsec = _bfd_elf_section_from_bfd_section (abfd, s);
	  if (elfsec == -1)
	    goto error_free_dyn;
	  shlink = elf_elfsections (abfd)[elfsec]->sh_link;

	  extdyn = dynbuf;
	  extdynend = extdyn + s->_raw_size / sizeof (Elf_External_Dyn);
	  for (; extdyn < extdynend; extdyn++)
	    {
	      Elf_Internal_Dyn dyn;

	      elf_swap_dyn_in (abfd, extdyn, &dyn);
	      if (dyn.d_tag == DT_SONAME)
		{
		  unsigned int tagv = dyn.d_un.d_val;
		  name = bfd_elf_string_from_elf_section (abfd, shlink, tagv);
		  if (name == NULL)
		    goto error_free_dyn;
		}
	      if (dyn.d_tag == DT_NEEDED)
		{
		  struct bfd_link_needed_list *n, **pn;
		  char *fnm, *anm;
		  unsigned int tagv = dyn.d_un.d_val;

		  amt = sizeof (struct bfd_link_needed_list);
		  n = bfd_alloc (abfd, amt);
		  fnm = bfd_elf_string_from_elf_section (abfd, shlink, tagv);
		  if (n == NULL || fnm == NULL)
		    goto error_free_dyn;
		  amt = strlen (fnm) + 1;
		  anm = bfd_alloc (abfd, amt);
		  if (anm == NULL)
		    goto error_free_dyn;
		  memcpy (anm, fnm, amt);
		  n->name = anm;
		  n->by = abfd;
		  n->next = NULL;
		  for (pn = & hash_table->needed;
		       *pn != NULL;
		       pn = &(*pn)->next)
		    ;
		  *pn = n;
		}
	      if (dyn.d_tag == DT_RUNPATH)
		{
		  struct bfd_link_needed_list *n, **pn;
		  char *fnm, *anm;
		  unsigned int tagv = dyn.d_un.d_val;

		  amt = sizeof (struct bfd_link_needed_list);
		  n = bfd_alloc (abfd, amt);
		  fnm = bfd_elf_string_from_elf_section (abfd, shlink, tagv);
		  if (n == NULL || fnm == NULL)
		    goto error_free_dyn;
		  amt = strlen (fnm) + 1;
		  anm = bfd_alloc (abfd, amt);
		  if (anm == NULL)
		    goto error_free_dyn;
		  memcpy (anm, fnm, amt);
		  n->name = anm;
		  n->by = abfd;
		  n->next = NULL;
		  for (pn = & runpath;
		       *pn != NULL;
		       pn = &(*pn)->next)
		    ;
		  *pn = n;
		}
	      /* Ignore DT_RPATH if we have seen DT_RUNPATH.  */
	      if (!runpath && dyn.d_tag == DT_RPATH)
		{
		  struct bfd_link_needed_list *n, **pn;
		  char *fnm, *anm;
		  unsigned int tagv = dyn.d_un.d_val;

		  amt = sizeof (struct bfd_link_needed_list);
		  n = bfd_alloc (abfd, amt);
		  fnm = bfd_elf_string_from_elf_section (abfd, shlink, tagv);
		  if (n == NULL || fnm == NULL)
		    goto error_free_dyn;
		  amt = strlen (fnm) + 1;
		  anm = bfd_alloc (abfd, amt);
		  if (anm == NULL)
		    {
		    error_free_dyn:
		      free (dynbuf);
		      goto error_return;
		    }
		  memcpy (anm, fnm, amt);
		  n->name = anm;
		  n->by = abfd;
		  n->next = NULL;
		  for (pn = & rpath;
		       *pn != NULL;
		       pn = &(*pn)->next)
		    ;
		  *pn = n;
		}
	    }

	  free (dynbuf);
	}

      /* DT_RUNPATH overrides DT_RPATH.  Do _NOT_ bfd_release, as that
	 frees all more recently bfd_alloc'd blocks as well.  */
      if (runpath)
	rpath = runpath;

      if (rpath)
	{
	  struct bfd_link_needed_list **pn;
	  for (pn = & hash_table->runpath;
	       *pn != NULL;
	       pn = &(*pn)->next)
	    ;
	  *pn = rpath;
	}

      /* We do not want to include any of the sections in a dynamic
	 object in the output file.  We hack by simply clobbering the
	 list of sections in the BFD.  This could be handled more
	 cleanly by, say, a new section flag; the existing
	 SEC_NEVER_LOAD flag is not the one we want, because that one
	 still implies that the section takes up space in the output
	 file.  */
      bfd_section_list_clear (abfd);

      /* If this is the first dynamic object found in the link, create
	 the special sections required for dynamic linking.  */
      if (! _bfd_elf_link_create_dynamic_sections (abfd, info))
	goto error_return;

      if (add_needed)
	{
	  /* Add a DT_NEEDED entry for this dynamic object.  */
	  oldsize = _bfd_elf_strtab_size (hash_table->dynstr);
	  strindex = _bfd_elf_strtab_add (hash_table->dynstr, name, FALSE);
	  if (strindex == (bfd_size_type) -1)
	    goto error_return;

	  if (oldsize == _bfd_elf_strtab_size (hash_table->dynstr))
	    {
	      asection *sdyn;
	      Elf_External_Dyn *dyncon, *dynconend;

	      /* The hash table size did not change, which means that
		 the dynamic object name was already entered.  If we
		 have already included this dynamic object in the
		 link, just ignore it.  There is no reason to include
		 a particular dynamic object more than once.  */
	      sdyn = bfd_get_section_by_name (hash_table->dynobj, ".dynamic");
	      BFD_ASSERT (sdyn != NULL);

	      dyncon = (Elf_External_Dyn *) sdyn->contents;
	      dynconend = (Elf_External_Dyn *) (sdyn->contents +
						sdyn->_raw_size);
	      for (; dyncon < dynconend; dyncon++)
		{
		  Elf_Internal_Dyn dyn;

		  elf_swap_dyn_in (hash_table->dynobj, dyncon, & dyn);
		  if (dyn.d_tag == DT_NEEDED
		      && dyn.d_un.d_val == strindex)
		    {
		      _bfd_elf_strtab_delref (hash_table->dynstr, strindex);
		      return TRUE;
		    }
		}
	    }

	  if (! elf_add_dynamic_entry (info, DT_NEEDED, strindex))
	    goto error_return;
	}

      /* Save the SONAME, if there is one, because sometimes the
	 linker emulation code will need to know it.  */
      if (*name == '\0')
	name = basename (bfd_get_filename (abfd));
      elf_dt_name (abfd) = name;
    }

  /* If this is a dynamic object, we always link against the .dynsym
     symbol table, not the .symtab symbol table.  The dynamic linker
     will only see the .dynsym symbol table, so there is no reason to
     look at .symtab for a dynamic object.  */

  if (! dynamic || elf_dynsymtab (abfd) == 0)
    hdr = &elf_tdata (abfd)->symtab_hdr;
  else
    hdr = &elf_tdata (abfd)->dynsymtab_hdr;

  symcount = hdr->sh_size / sizeof (Elf_External_Sym);

  /* The sh_info field of the symtab header tells us where the
     external symbols start.  We don't care about the local symbols at
     this point.  */
  if (elf_bad_symtab (abfd))
    {
      extsymcount = symcount;
      extsymoff = 0;
    }
  else
    {
      extsymcount = symcount - hdr->sh_info;
      extsymoff = hdr->sh_info;
    }

  sym_hash = NULL;
  if (extsymcount != 0)
    {
      isymbuf = bfd_elf_get_elf_syms (abfd, hdr, extsymcount, extsymoff,
				      NULL, NULL, NULL);
      if (isymbuf == NULL)
	goto error_return;

      /* We store a pointer to the hash table entry for each external
	 symbol.  */
      amt = extsymcount * sizeof (struct elf_link_hash_entry *);
      sym_hash = bfd_alloc (abfd, amt);
      if (sym_hash == NULL)
	goto error_free_sym;
      elf_sym_hashes (abfd) = sym_hash;
    }

  if (dynamic)
    {
      /* Read in any version definitions.  */
      if (! _bfd_elf_slurp_version_tables (abfd))
	goto error_free_sym;

      /* Read in the symbol versions, but don't bother to convert them
	 to internal format.  */
      if (elf_dynversym (abfd) != 0)
	{
	  Elf_Internal_Shdr *versymhdr;

	  versymhdr = &elf_tdata (abfd)->dynversym_hdr;
	  extversym = bfd_malloc (versymhdr->sh_size);
	  if (extversym == NULL)
	    goto error_free_sym;
	  amt = versymhdr->sh_size;
	  if (bfd_seek (abfd, versymhdr->sh_offset, SEEK_SET) != 0
	      || bfd_bread (extversym, amt, abfd) != amt)
	    goto error_free_vers;
	}
    }

  weaks = NULL;

  ever = extversym != NULL ? extversym + extsymoff : NULL;
  for (isym = isymbuf, isymend = isymbuf + extsymcount;
       isym < isymend;
       isym++, sym_hash++, ever = (ever != NULL ? ever + 1 : NULL))
    {
      int bind;
      bfd_vma value;
      asection *sec;
      flagword flags;
      const char *name;
      struct elf_link_hash_entry *h;
      bfd_boolean definition;
      bfd_boolean size_change_ok;
      bfd_boolean type_change_ok;
      bfd_boolean new_weakdef;
      bfd_boolean override;
      unsigned int old_alignment;
      bfd *old_bfd;

      override = FALSE;

      flags = BSF_NO_FLAGS;
      sec = NULL;
      value = isym->st_value;
      *sym_hash = NULL;

      bind = ELF_ST_BIND (isym->st_info);
      if (bind == STB_LOCAL)
	{
	  /* This should be impossible, since ELF requires that all
	     global symbols follow all local symbols, and that sh_info
	     point to the first global symbol.  Unfortunately, Irix 5
	     screws this up.  */
	  continue;
	}
      else if (bind == STB_GLOBAL)
	{
	  if (isym->st_shndx != SHN_UNDEF
	      && isym->st_shndx != SHN_COMMON)
	    flags = BSF_GLOBAL;
	}
      else if (bind == STB_WEAK)
	flags = BSF_WEAK;
      else
	{
	  /* Leave it up to the processor backend.  */
	}

      if (isym->st_shndx == SHN_UNDEF)
	sec = bfd_und_section_ptr;
      else if (isym->st_shndx < SHN_LORESERVE || isym->st_shndx > SHN_HIRESERVE)
	{
	  sec = section_from_elf_index (abfd, isym->st_shndx);
	  if (sec == NULL)
	    sec = bfd_abs_section_ptr;
	  else if ((abfd->flags & (EXEC_P | DYNAMIC)) != 0)
	    value -= sec->vma;
	}
      else if (isym->st_shndx == SHN_ABS)
	sec = bfd_abs_section_ptr;
      else if (isym->st_shndx == SHN_COMMON)
	{
	  sec = bfd_com_section_ptr;
	  /* What ELF calls the size we call the value.  What ELF
	     calls the value we call the alignment.  */
	  value = isym->st_size;
	}
      else
	{
	  /* Leave it up to the processor backend.  */
	}

      name = bfd_elf_string_from_elf_section (abfd, hdr->sh_link,
					      isym->st_name);
      if (name == NULL)
	goto error_free_vers;

      if (isym->st_shndx == SHN_COMMON
	  && ELF_ST_TYPE (isym->st_info) == STT_TLS)
	{
	  asection *tcomm = bfd_get_section_by_name (abfd, ".tcommon");

	  if (tcomm == NULL)
	    {
	      tcomm = bfd_make_section (abfd, ".tcommon");
	      if (tcomm == NULL
		  || !bfd_set_section_flags (abfd, tcomm, (SEC_ALLOC
							   | SEC_IS_COMMON
							   | SEC_LINKER_CREATED
							   | SEC_THREAD_LOCAL)))
		goto error_free_vers;
	    }
	  sec = tcomm;
	}
      else if (add_symbol_hook)
	{
	  if (! (*add_symbol_hook) (abfd, info, isym, &name, &flags, &sec,
				    &value))
	    goto error_free_vers;

	  /* The hook function sets the name to NULL if this symbol
	     should be skipped for some reason.  */
	  if (name == NULL)
	    continue;
	}

      /* Sanity check that all possibilities were handled.  */
      if (sec == NULL)
	{
	  bfd_set_error (bfd_error_bad_value);
	  goto error_free_vers;
	}

      if (bfd_is_und_section (sec)
	  || bfd_is_com_section (sec))
	definition = FALSE;
      else
	definition = TRUE;

      size_change_ok = FALSE;
      type_change_ok = get_elf_backend_data (abfd)->type_change_ok;
      old_alignment = 0;
      old_bfd = NULL;

      if (is_elf_hash_table (hash_table))
	{
	  Elf_Internal_Versym iver;
	  unsigned int vernum = 0;
	  bfd_boolean skip;

	  if (ever != NULL)
	    {
	      _bfd_elf_swap_versym_in (abfd, ever, &iver);
	      vernum = iver.vs_vers & VERSYM_VERSION;

	      /* If this is a hidden symbol, or if it is not version
		 1, we append the version name to the symbol name.
		 However, we do not modify a non-hidden absolute
		 symbol, because it might be the version symbol
		 itself.  FIXME: What if it isn't?  */
	      if ((iver.vs_vers & VERSYM_HIDDEN) != 0
		  || (vernum > 1 && ! bfd_is_abs_section (sec)))
		{
		  const char *verstr;
		  size_t namelen, verlen, newlen;
		  char *newname, *p;

		  if (isym->st_shndx != SHN_UNDEF)
		    {
		      if (vernum > elf_tdata (abfd)->dynverdef_hdr.sh_info)
			{
			  (*_bfd_error_handler)
			    (_("%s: %s: invalid version %u (max %d)"),
			     bfd_archive_filename (abfd), name, vernum,
			     elf_tdata (abfd)->dynverdef_hdr.sh_info);
			  bfd_set_error (bfd_error_bad_value);
			  goto error_free_vers;
			}
		      else if (vernum > 1)
			verstr =
			  elf_tdata (abfd)->verdef[vernum - 1].vd_nodename;
		      else
			verstr = "";
		    }
		  else
		    {
		      /* We cannot simply test for the number of
			 entries in the VERNEED section since the
			 numbers for the needed versions do not start
			 at 0.  */
		      Elf_Internal_Verneed *t;

		      verstr = NULL;
		      for (t = elf_tdata (abfd)->verref;
			   t != NULL;
			   t = t->vn_nextref)
			{
			  Elf_Internal_Vernaux *a;

			  for (a = t->vn_auxptr; a != NULL; a = a->vna_nextptr)
			    {
			      if (a->vna_other == vernum)
				{
				  verstr = a->vna_nodename;
				  break;
				}
			    }
			  if (a != NULL)
			    break;
			}
		      if (verstr == NULL)
			{
			  (*_bfd_error_handler)
			    (_("%s: %s: invalid needed version %d"),
			     bfd_archive_filename (abfd), name, vernum);
			  bfd_set_error (bfd_error_bad_value);
			  goto error_free_vers;
			}
		    }

		  namelen = strlen (name);
		  verlen = strlen (verstr);
		  newlen = namelen + verlen + 2;
		  if ((iver.vs_vers & VERSYM_HIDDEN) == 0
		      && isym->st_shndx != SHN_UNDEF)
		    ++newlen;

		  newname = bfd_alloc (abfd, newlen);
		  if (newname == NULL)
		    goto error_free_vers;
		  memcpy (newname, name, namelen);
		  p = newname + namelen;
		  *p++ = ELF_VER_CHR;
		  /* If this is a defined non-hidden version symbol,
		     we add another @ to the name.  This indicates the
		     default version of the symbol.  */
		  if ((iver.vs_vers & VERSYM_HIDDEN) == 0
		      && isym->st_shndx != SHN_UNDEF)
		    *p++ = ELF_VER_CHR;
		  memcpy (p, verstr, verlen + 1);

		  name = newname;
		}
	    }

	  if (!_bfd_elf_merge_symbol (abfd, info, name, isym, &sec, &value,
				      sym_hash, &skip, &override,
				      &type_change_ok, &size_change_ok,
				      dt_needed))
	    goto error_free_vers;

	  if (skip)
	    continue;

	  if (override)
	    definition = FALSE;

	  h = *sym_hash;
	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;

	  /* Remember the old alignment if this is a common symbol, so
	     that we don't reduce the alignment later on.  We can't
	     check later, because _bfd_generic_link_add_one_symbol
	     will set a default for the alignment which we want to
	     override. We also remember the old bfd where the existing
	     definition comes from.  */
	  switch (h->root.type)
	    {
	    default:
	      break;

	    case bfd_link_hash_defined:
	    case bfd_link_hash_defweak:
	      old_bfd = h->root.u.def.section->owner;
	      break;
	    
	    case bfd_link_hash_common:
	      old_bfd = h->root.u.c.p->section->owner;
	      old_alignment = h->root.u.c.p->alignment_power;
	      break;
	    }

	  if (elf_tdata (abfd)->verdef != NULL
	      && ! override
	      && vernum > 1
	      && definition)
	    h->verinfo.verdef = &elf_tdata (abfd)->verdef[vernum - 1];
	}

      if (! (_bfd_generic_link_add_one_symbol
	     (info, abfd, name, flags, sec, value, NULL, FALSE, collect,
	      (struct bfd_link_hash_entry **) sym_hash)))
	goto error_free_vers;

      h = *sym_hash;
      while (h->root.type == bfd_link_hash_indirect
	     || h->root.type == bfd_link_hash_warning)
	h = (struct elf_link_hash_entry *) h->root.u.i.link;
      *sym_hash = h;

      new_weakdef = FALSE;
      if (dynamic
	  && definition
	  && (flags & BSF_WEAK) != 0
	  && ELF_ST_TYPE (isym->st_info) != STT_FUNC
	  && is_elf_hash_table (hash_table)
	  && h->weakdef == NULL)
	{
	  /* Keep a list of all weak defined non function symbols from
	     a dynamic object, using the weakdef field.  Later in this
	     function we will set the weakdef field to the correct
	     value.  We only put non-function symbols from dynamic
	     objects on this list, because that happens to be the only
	     time we need to know the normal symbol corresponding to a
	     weak symbol, and the information is time consuming to
	     figure out.  If the weakdef field is not already NULL,
	     then this symbol was already defined by some previous
	     dynamic object, and we will be using that previous
	     definition anyhow.  */

	  h->weakdef = weaks;
	  weaks = h;
	  new_weakdef = TRUE;
	}

      /* Set the alignment of a common symbol.  */
      if (isym->st_shndx == SHN_COMMON
	  && h->root.type == bfd_link_hash_common)
	{
	  unsigned int align;

	  align = bfd_log2 (isym->st_value);
	  if (align > old_alignment
	      /* Permit an alignment power of zero if an alignment of one
		 is specified and no other alignments have been specified.  */
	      || (isym->st_value == 1 && old_alignment == 0))
	    h->root.u.c.p->alignment_power = align;
	  else
	    h->root.u.c.p->alignment_power = old_alignment;
	}

      if (is_elf_hash_table (hash_table))
	{
	  int old_flags;
	  bfd_boolean dynsym;
	  int new_flag;

	  /* Check the alignment when a common symbol is involved. This
	     can change when a common symbol is overridden by a normal
	     definition or a common symbol is ignored due to the old
	     normal definition. We need to make sure the maximum
	     alignment is maintained.  */
	  if ((old_alignment || isym->st_shndx == SHN_COMMON)
	      && h->root.type != bfd_link_hash_common)
	    {
	      unsigned int common_align;
	      unsigned int normal_align;
	      unsigned int symbol_align;
	      bfd *normal_bfd;
	      bfd *common_bfd;

	      symbol_align = ffs (h->root.u.def.value) - 1;
	      if (h->root.u.def.section->owner != NULL
		  && (h->root.u.def.section->owner->flags & DYNAMIC) == 0)
		{
		  normal_align = h->root.u.def.section->alignment_power;
		  if (normal_align > symbol_align)
		    normal_align = symbol_align;
		}
	      else
		normal_align = symbol_align;

	      if (old_alignment)
		{
		  common_align = old_alignment;
		  common_bfd = old_bfd;
		  normal_bfd = abfd;
		}
	      else
		{
		  common_align = bfd_log2 (isym->st_value);
		  common_bfd = abfd;
		  normal_bfd = old_bfd;
		}

	      if (normal_align < common_align)
		(*_bfd_error_handler)
		  (_("Warning: alignment %u of symbol `%s' in %s is smaller than %u in %s"),
		   1 << normal_align,
		   name,
		   bfd_archive_filename (normal_bfd),
		   1 << common_align,
		   bfd_archive_filename (common_bfd));
	    }

	  /* Remember the symbol size and type.  */
	  if (isym->st_size != 0
	      && (definition || h->size == 0))
	    {
	      if (h->size != 0 && h->size != isym->st_size && ! size_change_ok)
		(*_bfd_error_handler)
		  (_("Warning: size of symbol `%s' changed from %lu in %s to %lu in %s"),
		   name, (unsigned long) h->size,
		   bfd_archive_filename (old_bfd),
		   (unsigned long) isym->st_size,
		   bfd_archive_filename (abfd));

	      h->size = isym->st_size;
	    }

	  /* If this is a common symbol, then we always want H->SIZE
	     to be the size of the common symbol.  The code just above
	     won't fix the size if a common symbol becomes larger.  We
	     don't warn about a size change here, because that is
	     covered by --warn-common.  */
	  if (h->root.type == bfd_link_hash_common)
	    h->size = h->root.u.c.size;

	  if (ELF_ST_TYPE (isym->st_info) != STT_NOTYPE
	      && (definition || h->type == STT_NOTYPE))
	    {
	      if (h->type != STT_NOTYPE
		  && h->type != ELF_ST_TYPE (isym->st_info)
		  && ! type_change_ok)
		(*_bfd_error_handler)
		  (_("Warning: type of symbol `%s' changed from %d to %d in %s"),
		   name, h->type, ELF_ST_TYPE (isym->st_info),
		   bfd_archive_filename (abfd));

	      h->type = ELF_ST_TYPE (isym->st_info);
	    }

	  /* If st_other has a processor-specific meaning, specific
	     code might be needed here. We never merge the visibility
	     attribute with the one from a dynamic object.  */
	  if (bed->elf_backend_merge_symbol_attribute)
	    (*bed->elf_backend_merge_symbol_attribute) (h, isym, definition,
							dynamic);

	  if (isym->st_other != 0 && !dynamic)
	    {
	      unsigned char hvis, symvis, other, nvis;

	      /* Take the balance of OTHER from the definition.  */
	      other = (definition ? isym->st_other : h->other);
	      other &= ~ ELF_ST_VISIBILITY (-1);

	      /* Combine visibilities, using the most constraining one.  */
	      hvis   = ELF_ST_VISIBILITY (h->other);
	      symvis = ELF_ST_VISIBILITY (isym->st_other);
	      if (! hvis)
		nvis = symvis;
	      else if (! symvis)
		nvis = hvis;
	      else
		nvis = hvis < symvis ? hvis : symvis;

	      h->other = other | nvis;
	    }

	  /* Set a flag in the hash table entry indicating the type of
	     reference or definition we just found.  Keep a count of
	     the number of dynamic symbols we find.  A dynamic symbol
	     is one which is referenced or defined by both a regular
	     object and a shared object.  */
	  old_flags = h->elf_link_hash_flags;
	  dynsym = FALSE;
	  if (! dynamic)
	    {
	      if (! definition)
		{
		  new_flag = ELF_LINK_HASH_REF_REGULAR;
		  if (bind != STB_WEAK)
		    new_flag |= ELF_LINK_HASH_REF_REGULAR_NONWEAK;
		}
	      else
		new_flag = ELF_LINK_HASH_DEF_REGULAR;
	      if (! info->executable
		  || (old_flags & (ELF_LINK_HASH_DEF_DYNAMIC
				   | ELF_LINK_HASH_REF_DYNAMIC)) != 0)
		dynsym = TRUE;
	    }
	  else
	    {
	      if (! definition)
		new_flag = ELF_LINK_HASH_REF_DYNAMIC;
	      else
		new_flag = ELF_LINK_HASH_DEF_DYNAMIC;
	      if ((old_flags & (ELF_LINK_HASH_DEF_REGULAR
				| ELF_LINK_HASH_REF_REGULAR)) != 0
		  || (h->weakdef != NULL
		      && ! new_weakdef
		      && h->weakdef->dynindx != -1))
		dynsym = TRUE;
	    }

	  h->elf_link_hash_flags |= new_flag;

	  /* Check to see if we need to add an indirect symbol for
	     the default name.  */
	  if (definition || h->root.type == bfd_link_hash_common)
	    if (!_bfd_elf_add_default_symbol (abfd, info, h, name, isym,
					      &sec, &value, &dynsym,
					      override, dt_needed))
	      goto error_free_vers;

	  if (definition && !dynamic)
	    {
	      char *p = strchr (name, ELF_VER_CHR);
	      if (p != NULL && p[1] != ELF_VER_CHR)
		{
		  /* Queue non-default versions so that .symver x, x@FOO
		     aliases can be checked.  */
		  if (! nondeflt_vers)
		    {
		      amt = (isymend - isym + 1)
			    * sizeof (struct elf_link_hash_entry *);
		      nondeflt_vers = bfd_malloc (amt);
		    }
		  nondeflt_vers [nondeflt_vers_cnt++] = h;
		}
	    }

	  if (dynsym && h->dynindx == -1)
	    {
	      if (! _bfd_elf_link_record_dynamic_symbol (info, h))
		goto error_free_vers;
	      if (h->weakdef != NULL
		  && ! new_weakdef
		  && h->weakdef->dynindx == -1)
		{
		  if (! _bfd_elf_link_record_dynamic_symbol (info, h->weakdef))
		    goto error_free_vers;
		}
	    }
	  else if (dynsym && h->dynindx != -1)
	    /* If the symbol already has a dynamic index, but
	       visibility says it should not be visible, turn it into
	       a local symbol.  */
	    switch (ELF_ST_VISIBILITY (h->other))
	      {
	      case STV_INTERNAL:
	      case STV_HIDDEN:
		(*bed->elf_backend_hide_symbol) (info, h, TRUE);
		break;
	      }

	  if (dt_needed && definition
	      && (h->elf_link_hash_flags
		  & ELF_LINK_HASH_REF_REGULAR) != 0)
	    {
	      bfd_size_type oldsize;
	      bfd_size_type strindex;

	      /* The symbol from a DT_NEEDED object is referenced from
		 the regular object to create a dynamic executable. We
		 have to make sure there is a DT_NEEDED entry for it.  */

	      dt_needed = FALSE;
	      oldsize = _bfd_elf_strtab_size (hash_table->dynstr);
	      strindex = _bfd_elf_strtab_add (hash_table->dynstr,
					      elf_dt_soname (abfd), FALSE);
	      if (strindex == (bfd_size_type) -1)
		goto error_free_vers;

	      if (oldsize == _bfd_elf_strtab_size (hash_table->dynstr))
		{
		  asection *sdyn;
		  Elf_External_Dyn *dyncon, *dynconend;

		  sdyn = bfd_get_section_by_name (hash_table->dynobj,
						  ".dynamic");
		  BFD_ASSERT (sdyn != NULL);

		  dyncon = (Elf_External_Dyn *) sdyn->contents;
		  dynconend = (Elf_External_Dyn *) (sdyn->contents +
						    sdyn->_raw_size);
		  for (; dyncon < dynconend; dyncon++)
		    {
		      Elf_Internal_Dyn dyn;

		      elf_swap_dyn_in (hash_table->dynobj,
				       dyncon, &dyn);
		      BFD_ASSERT (dyn.d_tag != DT_NEEDED ||
				  dyn.d_un.d_val != strindex);
		    }
		}

	      if (! elf_add_dynamic_entry (info, DT_NEEDED, strindex))
		goto error_free_vers;
	    }
	}
    }

  /* Now that all the symbols from this input file are created, handle
     .symver foo, foo@BAR such that any relocs against foo become foo@BAR.  */
  if (nondeflt_vers != NULL)
    {
      bfd_size_type cnt, symidx;

      for (cnt = 0; cnt < nondeflt_vers_cnt; ++cnt)
	{
	  struct elf_link_hash_entry *h = nondeflt_vers[cnt], *hi;
	  char *shortname, *p;

	  p = strchr (h->root.root.string, ELF_VER_CHR);
	  if (p == NULL
	      || (h->root.type != bfd_link_hash_defined
		  && h->root.type != bfd_link_hash_defweak))
	    continue;

	  amt = p - h->root.root.string;
	  shortname = bfd_malloc (amt + 1);
	  memcpy (shortname, h->root.root.string, amt);
	  shortname[amt] = '\0';

	  hi = (struct elf_link_hash_entry *)
	       bfd_link_hash_lookup (&hash_table->root, shortname,
				     FALSE, FALSE, FALSE);
	  if (hi != NULL
	      && hi->root.type == h->root.type
	      && hi->root.u.def.value == h->root.u.def.value
	      && hi->root.u.def.section == h->root.u.def.section)
	    {
	      (*bed->elf_backend_hide_symbol) (info, hi, TRUE);
	      hi->root.type = bfd_link_hash_indirect;
	      hi->root.u.i.link = (struct bfd_link_hash_entry *) h;
	      (*bed->elf_backend_copy_indirect_symbol) (bed, h, hi);
	      sym_hash = elf_sym_hashes (abfd);
	      if (sym_hash)
		for (symidx = 0; symidx < extsymcount; ++symidx)
		  if (sym_hash[symidx] == hi)
		    {
		      sym_hash[symidx] = h;
		      break;
		    }
	    }
	  free (shortname);
	}
      free (nondeflt_vers);
      nondeflt_vers = NULL;
    }

  if (extversym != NULL)
    {
      free (extversym);
      extversym = NULL;
    }

  if (isymbuf != NULL)
    free (isymbuf);
  isymbuf = NULL;

  /* Now set the weakdefs field correctly for all the weak defined
     symbols we found.  The only way to do this is to search all the
     symbols.  Since we only need the information for non functions in
     dynamic objects, that's the only time we actually put anything on
     the list WEAKS.  We need this information so that if a regular
     object refers to a symbol defined weakly in a dynamic object, the
     real symbol in the dynamic object is also put in the dynamic
     symbols; we also must arrange for both symbols to point to the
     same memory location.  We could handle the general case of symbol
     aliasing, but a general symbol alias can only be generated in
     assembler code, handling it correctly would be very time
     consuming, and other ELF linkers don't handle general aliasing
     either.  */
  if (weaks != NULL)
    {
      struct elf_link_hash_entry **hpp;
      struct elf_link_hash_entry **hppend;
      struct elf_link_hash_entry **sorted_sym_hash;
      struct elf_link_hash_entry *h;
      size_t sym_count;

      /* Since we have to search the whole symbol list for each weak
	 defined symbol, search time for N weak defined symbols will be
	 O(N^2). Binary search will cut it down to O(NlogN).  */
      amt = extsymcount * sizeof (struct elf_link_hash_entry *);
      sorted_sym_hash = bfd_malloc (amt);
      if (sorted_sym_hash == NULL)
	goto error_return;
      sym_hash = sorted_sym_hash;
      hpp = elf_sym_hashes (abfd);
      hppend = hpp + extsymcount;
      sym_count = 0;
      for (; hpp < hppend; hpp++)
	{
	  h = *hpp;
	  if (h != NULL
	      && h->root.type == bfd_link_hash_defined
	      && h->type != STT_FUNC)
	    {
	      *sym_hash = h;
	      sym_hash++;
	      sym_count++;
	    }
	}

      qsort (sorted_sym_hash, sym_count,
	     sizeof (struct elf_link_hash_entry *),
	     sort_symbol);

      while (weaks != NULL)
	{
	  struct elf_link_hash_entry *hlook;
	  asection *slook;
	  bfd_vma vlook;
	  long ilook;
	  size_t i, j, idx;

	  hlook = weaks;
	  weaks = hlook->weakdef;
	  hlook->weakdef = NULL;

	  BFD_ASSERT (hlook->root.type == bfd_link_hash_defined
		      || hlook->root.type == bfd_link_hash_defweak
		      || hlook->root.type == bfd_link_hash_common
		      || hlook->root.type == bfd_link_hash_indirect);
	  slook = hlook->root.u.def.section;
	  vlook = hlook->root.u.def.value;

	  ilook = -1;
	  i = 0;
	  j = sym_count;
	  while (i < j)
	    {
	      bfd_signed_vma vdiff;
	      idx = (i + j) / 2;
	      h = sorted_sym_hash [idx];
	      vdiff = vlook - h->root.u.def.value;
	      if (vdiff < 0)
		j = idx;
	      else if (vdiff > 0)
		i = idx + 1;
	      else
		{
		  long sdiff = slook - h->root.u.def.section;
		  if (sdiff < 0)
		    j = idx;
		  else if (sdiff > 0)
		    i = idx + 1;
		  else
		    {
		      ilook = idx;
		      break;
		    }
		}
	    }

	  /* We didn't find a value/section match.  */
	  if (ilook == -1)
	    continue;

	  for (i = ilook; i < sym_count; i++)
	    {
	      h = sorted_sym_hash [i];

	      /* Stop if value or section doesn't match.  */
	      if (h->root.u.def.value != vlook
		  || h->root.u.def.section != slook)
		break;
	      else if (h != hlook)
		{
		  hlook->weakdef = h;

		  /* If the weak definition is in the list of dynamic
		     symbols, make sure the real definition is put
		     there as well.  */
		  if (hlook->dynindx != -1 && h->dynindx == -1)
		    {
		      if (! _bfd_elf_link_record_dynamic_symbol (info,
								 h))
			goto error_return;
		    }

		  /* If the real definition is in the list of dynamic
		     symbols, make sure the weak definition is put
		     there as well.  If we don't do this, then the
		     dynamic loader might not merge the entries for the
		     real definition and the weak definition.  */
		  if (h->dynindx != -1 && hlook->dynindx == -1)
		    {
		      if (! _bfd_elf_link_record_dynamic_symbol (info,
								 hlook))
			goto error_return;
		    }
		  break;
		}
	    }
	}

      free (sorted_sym_hash);
    }

  /* If this object is the same format as the output object, and it is
     not a shared library, then let the backend look through the
     relocs.

     This is required to build global offset table entries and to
     arrange for dynamic relocs.  It is not required for the
     particular common case of linking non PIC code, even when linking
     against shared libraries, but unfortunately there is no way of
     knowing whether an object file has been compiled PIC or not.
     Looking through the relocs is not particularly time consuming.
     The problem is that we must either (1) keep the relocs in memory,
     which causes the linker to require additional runtime memory or
     (2) read the relocs twice from the input file, which wastes time.
     This would be a good case for using mmap.

     I have no idea how to handle linking PIC code into a file of a
     different format.  It probably can't be done.  */
  check_relocs = get_elf_backend_data (abfd)->check_relocs;
  if (! dynamic
      && is_elf_hash_table (hash_table)
      && hash_table->root.creator == abfd->xvec
      && check_relocs != NULL)
    {
      asection *o;

      for (o = abfd->sections; o != NULL; o = o->next)
	{
	  Elf_Internal_Rela *internal_relocs;
	  bfd_boolean ok;

	  if ((o->flags & SEC_RELOC) == 0
	      || o->reloc_count == 0
	      || ((info->strip == strip_all || info->strip == strip_debugger)
		  && (o->flags & SEC_DEBUGGING) != 0)
	      || bfd_is_abs_section (o->output_section))
	    continue;

	  internal_relocs = _bfd_elf_link_read_relocs (abfd, o, NULL, NULL,
						       info->keep_memory);
	  if (internal_relocs == NULL)
	    goto error_return;

	  ok = (*check_relocs) (abfd, info, o, internal_relocs);

	  if (elf_section_data (o)->relocs != internal_relocs)
	    free (internal_relocs);

	  if (! ok)
	    goto error_return;
	}
    }

  /* If this is a non-traditional link, try to optimize the handling
     of the .stab/.stabstr sections.  */
  if (! dynamic
      && ! info->traditional_format
      && is_elf_hash_table (hash_table)
      && (info->strip != strip_all && info->strip != strip_debugger))
    {
      asection *stabstr;
      
      stabstr = bfd_get_section_by_name (abfd, ".stabstr");
      if (stabstr != NULL)
	{
	  bfd_size_type string_offset = 0;
	  asection *stab;

	  for (stab = abfd->sections; stab; stab = stab->next)
	    if (strncmp (".stab", stab->name, 5) == 0
		&& (!stab->name[5] ||
		    (stab->name[5] == '.' && ISDIGIT (stab->name[6])))
		&& (stab->flags & SEC_MERGE) == 0
		&& !bfd_is_abs_section (stab->output_section))
	      {
		struct bfd_elf_section_data *secdata;
		
		secdata = elf_section_data (stab);
		if (! _bfd_link_section_stabs (abfd,
					       & hash_table->stab_info,
					       stab, stabstr,
					       &secdata->sec_info,
					       &string_offset))
		  goto error_return;
		if (secdata->sec_info)
		  stab->sec_info_type = ELF_INFO_TYPE_STABS;
	    }
	}
    }

  if (! info->relocatable
      && ! dynamic
      && is_elf_hash_table (hash_table))
    {
      asection *s;

      for (s = abfd->sections; s != NULL; s = s->next)
	if ((s->flags & SEC_MERGE) != 0
	    && !bfd_is_abs_section (s->output_section))
	  {
	    struct bfd_elf_section_data *secdata;

	    secdata = elf_section_data (s);
	    if (! _bfd_merge_section (abfd,
				      & hash_table->merge_info,
				      s, &secdata->sec_info))
	      goto error_return;
	    else if (secdata->sec_info)
	      s->sec_info_type = ELF_INFO_TYPE_MERGE;
	  }
    }

  if (is_elf_hash_table (hash_table))
    {
      /* Add this bfd to the loaded list.  */
      struct elf_link_loaded_list *n;

      n = bfd_alloc (abfd, sizeof (struct elf_link_loaded_list));
      if (n == NULL)
	goto error_return;
      n->abfd = abfd;
      n->next = hash_table->loaded;
      hash_table->loaded = n;
    }

  return TRUE;

 error_free_vers:
  if (nondeflt_vers != NULL)
    free (nondeflt_vers);
  if (extversym != NULL)
    free (extversym);
 error_free_sym:
  if (isymbuf != NULL)
    free (isymbuf);
 error_return:
  return FALSE;
}

/* Add an entry to the .dynamic table.  */

bfd_boolean
elf_add_dynamic_entry (struct bfd_link_info *info, bfd_vma tag, bfd_vma val)
{
  Elf_Internal_Dyn dyn;
  bfd *dynobj;
  asection *s;
  bfd_size_type newsize;
  bfd_byte *newcontents;

  if (! is_elf_hash_table (info->hash))
    return FALSE;

  dynobj = elf_hash_table (info)->dynobj;

  s = bfd_get_section_by_name (dynobj, ".dynamic");
  BFD_ASSERT (s != NULL);

  newsize = s->_raw_size + sizeof (Elf_External_Dyn);
  newcontents = bfd_realloc (s->contents, newsize);
  if (newcontents == NULL)
    return FALSE;

  dyn.d_tag = tag;
  dyn.d_un.d_val = val;
  elf_swap_dyn_out (dynobj, &dyn,
		    (Elf_External_Dyn *) (newcontents + s->_raw_size));

  s->_raw_size = newsize;
  s->contents = newcontents;

  return TRUE;
}

/* Array used to determine the number of hash table buckets to use
   based on the number of symbols there are.  If there are fewer than
   3 symbols we use 1 bucket, fewer than 17 symbols we use 3 buckets,
   fewer than 37 we use 17 buckets, and so forth.  We never use more
   than 32771 buckets.  */

static const size_t elf_buckets[] =
{
  1, 3, 17, 37, 67, 97, 131, 197, 263, 521, 1031, 2053, 4099, 8209,
  16411, 32771, 0
};

/* Compute bucket count for hashing table.  We do not use a static set
   of possible tables sizes anymore.  Instead we determine for all
   possible reasonable sizes of the table the outcome (i.e., the
   number of collisions etc) and choose the best solution.  The
   weighting functions are not too simple to allow the table to grow
   without bounds.  Instead one of the weighting factors is the size.
   Therefore the result is always a good payoff between few collisions
   (= short chain lengths) and table size.  */
static size_t
compute_bucket_count (struct bfd_link_info *info)
{
  size_t dynsymcount = elf_hash_table (info)->dynsymcount;
  size_t best_size = 0;
  unsigned long int *hashcodes;
  unsigned long int *hashcodesp;
  unsigned long int i;
  bfd_size_type amt;

  /* Compute the hash values for all exported symbols.  At the same
     time store the values in an array so that we could use them for
     optimizations.  */
  amt = dynsymcount;
  amt *= sizeof (unsigned long int);
  hashcodes = bfd_malloc (amt);
  if (hashcodes == NULL)
    return 0;
  hashcodesp = hashcodes;

  /* Put all hash values in HASHCODES.  */
  elf_link_hash_traverse (elf_hash_table (info),
			  elf_collect_hash_codes, &hashcodesp);

  /* We have a problem here.  The following code to optimize the table
     size requires an integer type with more the 32 bits.  If
     BFD_HOST_U_64_BIT is set we know about such a type.  */
#ifdef BFD_HOST_U_64_BIT
  if (info->optimize)
    {
      unsigned long int nsyms = hashcodesp - hashcodes;
      size_t minsize;
      size_t maxsize;
      BFD_HOST_U_64_BIT best_chlen = ~((BFD_HOST_U_64_BIT) 0);
      unsigned long int *counts ;

      /* Possible optimization parameters: if we have NSYMS symbols we say
	 that the hashing table must at least have NSYMS/4 and at most
	 2*NSYMS buckets.  */
      minsize = nsyms / 4;
      if (minsize == 0)
	minsize = 1;
      best_size = maxsize = nsyms * 2;

      /* Create array where we count the collisions in.  We must use bfd_malloc
	 since the size could be large.  */
      amt = maxsize;
      amt *= sizeof (unsigned long int);
      counts = bfd_malloc (amt);
      if (counts == NULL)
	{
	  free (hashcodes);
	  return 0;
	}

      /* Compute the "optimal" size for the hash table.  The criteria is a
	 minimal chain length.  The minor criteria is (of course) the size
	 of the table.  */
      for (i = minsize; i < maxsize; ++i)
	{
	  /* Walk through the array of hashcodes and count the collisions.  */
	  BFD_HOST_U_64_BIT max;
	  unsigned long int j;
	  unsigned long int fact;

	  memset (counts, '\0', i * sizeof (unsigned long int));

	  /* Determine how often each hash bucket is used.  */
	  for (j = 0; j < nsyms; ++j)
	    ++counts[hashcodes[j] % i];

	  /* For the weight function we need some information about the
	     pagesize on the target.  This is information need not be 100%
	     accurate.  Since this information is not available (so far) we
	     define it here to a reasonable default value.  If it is crucial
	     to have a better value some day simply define this value.  */
# ifndef BFD_TARGET_PAGESIZE
#  define BFD_TARGET_PAGESIZE	(4096)
# endif

	  /* We in any case need 2 + NSYMS entries for the size values and
	     the chains.  */
	  max = (2 + nsyms) * (ARCH_SIZE / 8);

# if 1
	  /* Variant 1: optimize for short chains.  We add the squares
	     of all the chain lengths (which favors many small chain
	     over a few long chains).  */
	  for (j = 0; j < i; ++j)
	    max += counts[j] * counts[j];

	  /* This adds penalties for the overall size of the table.  */
	  fact = i / (BFD_TARGET_PAGESIZE / (ARCH_SIZE / 8)) + 1;
	  max *= fact * fact;
# else
	  /* Variant 2: Optimize a lot more for small table.  Here we
	     also add squares of the size but we also add penalties for
	     empty slots (the +1 term).  */
	  for (j = 0; j < i; ++j)
	    max += (1 + counts[j]) * (1 + counts[j]);

	  /* The overall size of the table is considered, but not as
	     strong as in variant 1, where it is squared.  */
	  fact = i / (BFD_TARGET_PAGESIZE / (ARCH_SIZE / 8)) + 1;
	  max *= fact;
# endif

	  /* Compare with current best results.  */
	  if (max < best_chlen)
	    {
	      best_chlen = max;
	      best_size = i;
	    }
	}

      free (counts);
    }
  else
#endif /* defined (BFD_HOST_U_64_BIT) */
    {
      /* This is the fallback solution if no 64bit type is available or if we
	 are not supposed to spend much time on optimizations.  We select the
	 bucket count using a fixed set of numbers.  */
      for (i = 0; elf_buckets[i] != 0; i++)
	{
	  best_size = elf_buckets[i];
	  if (dynsymcount < elf_buckets[i + 1])
	    break;
	}
    }

  /* Free the arrays we needed.  */
  free (hashcodes);

  return best_size;
}

/* Set up the sizes and contents of the ELF dynamic sections.  This is
   called by the ELF linker emulation before_allocation routine.  We
   must set the sizes of the sections before the linker sets the
   addresses of the various sections.  */

bfd_boolean
NAME(bfd_elf,size_dynamic_sections) (bfd *output_bfd,
				     const char *soname,
				     const char *rpath,
				     const char *filter_shlib,
				     const char * const *auxiliary_filters,
				     struct bfd_link_info *info,
				     asection **sinterpptr,
				     struct bfd_elf_version_tree *verdefs)
{
  bfd_size_type soname_indx;
  bfd *dynobj;
  const struct elf_backend_data *bed;
  struct elf_assign_sym_version_info asvinfo;

  *sinterpptr = NULL;

  soname_indx = (bfd_size_type) -1;

  if (!is_elf_hash_table (info->hash))
    return TRUE;

  if (info->execstack)
    elf_tdata (output_bfd)->stack_flags = PF_R | PF_W | PF_X;
  else if (info->noexecstack)
    elf_tdata (output_bfd)->stack_flags = PF_R | PF_W;
  else
    {
      bfd *inputobj;
      asection *notesec = NULL;
      int exec = 0;

      for (inputobj = info->input_bfds;
	   inputobj;
	   inputobj = inputobj->link_next)
	{
	  asection *s;

	  if (inputobj->flags & DYNAMIC)
	    continue;
	  s = bfd_get_section_by_name (inputobj, ".note.GNU-stack");
	  if (s)
	    {
	      if (s->flags & SEC_CODE)
		exec = PF_X;
	      notesec = s;
	    }
	  else
	    exec = PF_X;
	}
      if (notesec)
	{
	  elf_tdata (output_bfd)->stack_flags = PF_R | PF_W | exec;
	  if (exec && info->relocatable
	      && notesec->output_section != bfd_abs_section_ptr)
	    notesec->output_section->flags |= SEC_CODE;
	}
    }

  /* Any syms created from now on start with -1 in
     got.refcount/offset and plt.refcount/offset.  */
  elf_hash_table (info)->init_refcount = elf_hash_table (info)->init_offset;

  /* The backend may have to create some sections regardless of whether
     we're dynamic or not.  */
  bed = get_elf_backend_data (output_bfd);
  if (bed->elf_backend_always_size_sections
      && ! (*bed->elf_backend_always_size_sections) (output_bfd, info))
    return FALSE;

  dynobj = elf_hash_table (info)->dynobj;

  /* If there were no dynamic objects in the link, there is nothing to
     do here.  */
  if (dynobj == NULL)
    return TRUE;

  if (! _bfd_elf_maybe_strip_eh_frame_hdr (info))
    return FALSE;

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      struct elf_info_failed eif;
      struct elf_link_hash_entry *h;
      asection *dynstr;
      struct bfd_elf_version_tree *t;
      struct bfd_elf_version_expr *d;
      bfd_boolean all_defined;

      *sinterpptr = bfd_get_section_by_name (dynobj, ".interp");
      BFD_ASSERT (*sinterpptr != NULL || !info->executable);

      if (soname != NULL)
	{
	  soname_indx = _bfd_elf_strtab_add (elf_hash_table (info)->dynstr,
					     soname, TRUE);
	  if (soname_indx == (bfd_size_type) -1
	      || ! elf_add_dynamic_entry (info, DT_SONAME, soname_indx))
	    return FALSE;
	}

      if (info->symbolic)
	{
	  if (! elf_add_dynamic_entry (info, DT_SYMBOLIC, 0))
	    return FALSE;
	  info->flags |= DF_SYMBOLIC;
	}

      if (rpath != NULL)
	{
	  bfd_size_type indx;

	  indx = _bfd_elf_strtab_add (elf_hash_table (info)->dynstr, rpath,
				      TRUE);
	  if (info->new_dtags)
	    _bfd_elf_strtab_addref (elf_hash_table (info)->dynstr, indx);
	  if (indx == (bfd_size_type) -1
	      || ! elf_add_dynamic_entry (info, DT_RPATH, indx)
	      || (info->new_dtags
		  && ! elf_add_dynamic_entry (info, DT_RUNPATH, indx)))
	    return FALSE;
	}

      if (filter_shlib != NULL)
	{
	  bfd_size_type indx;

	  indx = _bfd_elf_strtab_add (elf_hash_table (info)->dynstr,
				      filter_shlib, TRUE);
	  if (indx == (bfd_size_type) -1
	      || ! elf_add_dynamic_entry (info, DT_FILTER, indx))
	    return FALSE;
	}

      if (auxiliary_filters != NULL)
	{
	  const char * const *p;

	  for (p = auxiliary_filters; *p != NULL; p++)
	    {
	      bfd_size_type indx;

	      indx = _bfd_elf_strtab_add (elf_hash_table (info)->dynstr,
					  *p, TRUE);
	      if (indx == (bfd_size_type) -1
		  || ! elf_add_dynamic_entry (info, DT_AUXILIARY, indx))
		return FALSE;
	    }
	}

      eif.info = info;
      eif.verdefs = verdefs;
      eif.failed = FALSE;

      /* If we are supposed to export all symbols into the dynamic symbol
	 table (this is not the normal case), then do so.  */
      if (info->export_dynamic)
	{
	  elf_link_hash_traverse (elf_hash_table (info),
				  _bfd_elf_export_symbol,
				  &eif);
	  if (eif.failed)
	    return FALSE;
	}

      /* Make all global versions with definition.  */
      for (t = verdefs; t != NULL; t = t->next)
	for (d = t->globals.list; d != NULL; d = d->next)
	  if (!d->symver && d->symbol)
	    {
	      const char *verstr, *name;
	      size_t namelen, verlen, newlen;
	      char *newname, *p;
	      struct elf_link_hash_entry *newh;

	      name = d->symbol;
	      namelen = strlen (name);
	      verstr = t->name;
	      verlen = strlen (verstr);
	      newlen = namelen + verlen + 3;

	      newname = bfd_malloc (newlen);
	      if (newname == NULL)
		return FALSE;
	      memcpy (newname, name, namelen);

	      /* Check the hidden versioned definition.  */
	      p = newname + namelen;
	      *p++ = ELF_VER_CHR;
	      memcpy (p, verstr, verlen + 1);
	      newh = elf_link_hash_lookup (elf_hash_table (info),
					   newname, FALSE, FALSE,
					   FALSE);
	      if (newh == NULL
		  || (newh->root.type != bfd_link_hash_defined
		      && newh->root.type != bfd_link_hash_defweak))
		{
		  /* Check the default versioned definition.  */
		  *p++ = ELF_VER_CHR;
		  memcpy (p, verstr, verlen + 1);
		  newh = elf_link_hash_lookup (elf_hash_table (info),
					       newname, FALSE, FALSE,
					       FALSE);
		}
	      free (newname);

	      /* Mark this version if there is a definition and it is
		 not defined in a shared object.  */
	      if (newh != NULL
		  && ((newh->elf_link_hash_flags
		       & ELF_LINK_HASH_DEF_DYNAMIC) == 0)
		  && (newh->root.type == bfd_link_hash_defined
		      || newh->root.type == bfd_link_hash_defweak))
		d->symver = 1;
	    }

      /* Attach all the symbols to their version information.  */
      asvinfo.output_bfd = output_bfd;
      asvinfo.info = info;
      asvinfo.verdefs = verdefs;
      asvinfo.failed = FALSE;

      elf_link_hash_traverse (elf_hash_table (info),
			      _bfd_elf_link_assign_sym_version,
			      &asvinfo);
      if (asvinfo.failed)
	return FALSE;

      if (!info->allow_undefined_version)
	{
	  /* Check if all global versions have a definition.  */
	  all_defined = TRUE;
	  for (t = verdefs; t != NULL; t = t->next)
	    for (d = t->globals.list; d != NULL; d = d->next)
	      if (!d->symver && !d->script)
		{
		  (*_bfd_error_handler)
		    (_("%s: undefined version: %s"),
		     d->pattern, t->name);
		  all_defined = FALSE;
		}

	  if (!all_defined)
	    {
	      bfd_set_error (bfd_error_bad_value);
	      return FALSE;
	    }
	}

      /* Find all symbols which were defined in a dynamic object and make
	 the backend pick a reasonable value for them.  */
      elf_link_hash_traverse (elf_hash_table (info),
			      _bfd_elf_adjust_dynamic_symbol,
			      &eif);
      if (eif.failed)
	return FALSE;

      /* Add some entries to the .dynamic section.  We fill in some of the
	 values later, in elf_bfd_final_link, but we must add the entries
	 now so that we know the final size of the .dynamic section.  */

      /* If there are initialization and/or finalization functions to
	 call then add the corresponding DT_INIT/DT_FINI entries.  */
      h = (info->init_function
	   ? elf_link_hash_lookup (elf_hash_table (info),
				   info->init_function, FALSE,
				   FALSE, FALSE)
	   : NULL);
      if (h != NULL
	  && (h->elf_link_hash_flags & (ELF_LINK_HASH_REF_REGULAR
					| ELF_LINK_HASH_DEF_REGULAR)) != 0)
	{
	  if (! elf_add_dynamic_entry (info, DT_INIT, 0))
	    return FALSE;
	}
      h = (info->fini_function
	   ? elf_link_hash_lookup (elf_hash_table (info),
				   info->fini_function, FALSE,
				   FALSE, FALSE)
	   : NULL);
      if (h != NULL
	  && (h->elf_link_hash_flags & (ELF_LINK_HASH_REF_REGULAR
					| ELF_LINK_HASH_DEF_REGULAR)) != 0)
	{
	  if (! elf_add_dynamic_entry (info, DT_FINI, 0))
	    return FALSE;
	}

      if (bfd_get_section_by_name (output_bfd, ".preinit_array") != NULL)
	{
	  /* DT_PREINIT_ARRAY is not allowed in shared library.  */
	  if (! info->executable)
	    {
	      bfd *sub;
	      asection *o;

	      for (sub = info->input_bfds; sub != NULL;
		   sub = sub->link_next)
		for (o = sub->sections; o != NULL; o = o->next)
		  if (elf_section_data (o)->this_hdr.sh_type
		      == SHT_PREINIT_ARRAY)
		    {
		      (*_bfd_error_handler)
			(_("%s: .preinit_array section is not allowed in DSO"),
			 bfd_archive_filename (sub));
		      break;
		    }

	      bfd_set_error (bfd_error_nonrepresentable_section);
	      return FALSE;
	    }

	  if (!elf_add_dynamic_entry (info, DT_PREINIT_ARRAY, 0)
	      || !elf_add_dynamic_entry (info, DT_PREINIT_ARRAYSZ, 0))
	    return FALSE;
	}
      if (bfd_get_section_by_name (output_bfd, ".init_array") != NULL)
	{
	  if (!elf_add_dynamic_entry (info, DT_INIT_ARRAY, 0)
	      || !elf_add_dynamic_entry (info, DT_INIT_ARRAYSZ, 0))
	    return FALSE;
	}
      if (bfd_get_section_by_name (output_bfd, ".fini_array") != NULL)
	{
	  if (!elf_add_dynamic_entry (info, DT_FINI_ARRAY, 0)
	      || !elf_add_dynamic_entry (info, DT_FINI_ARRAYSZ, 0))
	    return FALSE;
	}

      dynstr = bfd_get_section_by_name (dynobj, ".dynstr");
      /* If .dynstr is excluded from the link, we don't want any of
	 these tags.  Strictly, we should be checking each section
	 individually;  This quick check covers for the case where
	 someone does a /DISCARD/ : { *(*) }.  */
      if (dynstr != NULL && dynstr->output_section != bfd_abs_section_ptr)
	{
	  bfd_size_type strsize;

	  strsize = _bfd_elf_strtab_size (elf_hash_table (info)->dynstr);
	  if (! elf_add_dynamic_entry (info, DT_HASH, 0)
	      || ! elf_add_dynamic_entry (info, DT_STRTAB, 0)
	      || ! elf_add_dynamic_entry (info, DT_SYMTAB, 0)
	      || ! elf_add_dynamic_entry (info, DT_STRSZ, strsize)
	      || ! elf_add_dynamic_entry (info, DT_SYMENT,
					  sizeof (Elf_External_Sym)))
	    return FALSE;
	}
    }

  /* The backend must work out the sizes of all the other dynamic
     sections.  */
  if (bed->elf_backend_size_dynamic_sections
      && ! (*bed->elf_backend_size_dynamic_sections) (output_bfd, info))
    return FALSE;

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      bfd_size_type dynsymcount;
      asection *s;
      size_t bucketcount = 0;
      size_t hash_entry_size;
      unsigned int dtagcount;

      /* Set up the version definition section.  */
      s = bfd_get_section_by_name (dynobj, ".gnu.version_d");
      BFD_ASSERT (s != NULL);

      /* We may have created additional version definitions if we are
	 just linking a regular application.  */
      verdefs = asvinfo.verdefs;

      /* Skip anonymous version tag.  */
      if (verdefs != NULL && verdefs->vernum == 0)
	verdefs = verdefs->next;

      if (verdefs == NULL)
	_bfd_strip_section_from_output (info, s);
      else
	{
	  unsigned int cdefs;
	  bfd_size_type size;
	  struct bfd_elf_version_tree *t;
	  bfd_byte *p;
	  Elf_Internal_Verdef def;
	  Elf_Internal_Verdaux defaux;

	  cdefs = 0;
	  size = 0;

	  /* Make space for the base version.  */
	  size += sizeof (Elf_External_Verdef);
	  size += sizeof (Elf_External_Verdaux);
	  ++cdefs;

	  for (t = verdefs; t != NULL; t = t->next)
	    {
	      struct bfd_elf_version_deps *n;

	      size += sizeof (Elf_External_Verdef);
	      size += sizeof (Elf_External_Verdaux);
	      ++cdefs;

	      for (n = t->deps; n != NULL; n = n->next)
		size += sizeof (Elf_External_Verdaux);
	    }

	  s->_raw_size = size;
	  s->contents = bfd_alloc (output_bfd, s->_raw_size);
	  if (s->contents == NULL && s->_raw_size != 0)
	    return FALSE;

	  /* Fill in the version definition section.  */

	  p = s->contents;

	  def.vd_version = VER_DEF_CURRENT;
	  def.vd_flags = VER_FLG_BASE;
	  def.vd_ndx = 1;
	  def.vd_cnt = 1;
	  def.vd_aux = sizeof (Elf_External_Verdef);
	  def.vd_next = (sizeof (Elf_External_Verdef)
			 + sizeof (Elf_External_Verdaux));

	  if (soname_indx != (bfd_size_type) -1)
	    {
	      _bfd_elf_strtab_addref (elf_hash_table (info)->dynstr,
				      soname_indx);
	      def.vd_hash = bfd_elf_hash (soname);
	      defaux.vda_name = soname_indx;
	    }
	  else
	    {
	      const char *name;
	      bfd_size_type indx;

	      name = basename (output_bfd->filename);
	      def.vd_hash = bfd_elf_hash (name);
	      indx = _bfd_elf_strtab_add (elf_hash_table (info)->dynstr,
					  name, FALSE);
	      if (indx == (bfd_size_type) -1)
		return FALSE;
	      defaux.vda_name = indx;
	    }
	  defaux.vda_next = 0;

	  _bfd_elf_swap_verdef_out (output_bfd, &def,
				    (Elf_External_Verdef *) p);
	  p += sizeof (Elf_External_Verdef);
	  _bfd_elf_swap_verdaux_out (output_bfd, &defaux,
				     (Elf_External_Verdaux *) p);
	  p += sizeof (Elf_External_Verdaux);

	  for (t = verdefs; t != NULL; t = t->next)
	    {
	      unsigned int cdeps;
	      struct bfd_elf_version_deps *n;
	      struct elf_link_hash_entry *h;
	      struct bfd_link_hash_entry *bh;

	      cdeps = 0;
	      for (n = t->deps; n != NULL; n = n->next)
		++cdeps;

	      /* Add a symbol representing this version.  */
	      bh = NULL;
	      if (! (_bfd_generic_link_add_one_symbol
		     (info, dynobj, t->name, BSF_GLOBAL, bfd_abs_section_ptr,
		      0, NULL, FALSE,
		      get_elf_backend_data (dynobj)->collect, &bh)))
		return FALSE;
	      h = (struct elf_link_hash_entry *) bh;
	      h->elf_link_hash_flags &= ~ ELF_LINK_NON_ELF;
	      h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
	      h->type = STT_OBJECT;
	      h->verinfo.vertree = t;

	      if (! _bfd_elf_link_record_dynamic_symbol (info, h))
		return FALSE;

	      def.vd_version = VER_DEF_CURRENT;
	      def.vd_flags = 0;
	      if (t->globals.list == NULL && t->locals.list == NULL && ! t->used)
		def.vd_flags |= VER_FLG_WEAK;
	      def.vd_ndx = t->vernum + 1;
	      def.vd_cnt = cdeps + 1;
	      def.vd_hash = bfd_elf_hash (t->name);
	      def.vd_aux = sizeof (Elf_External_Verdef);
	      if (t->next != NULL)
		def.vd_next = (sizeof (Elf_External_Verdef)
			       + (cdeps + 1) * sizeof (Elf_External_Verdaux));
	      else
		def.vd_next = 0;

	      _bfd_elf_swap_verdef_out (output_bfd, &def,
					(Elf_External_Verdef *) p);
	      p += sizeof (Elf_External_Verdef);

	      defaux.vda_name = h->dynstr_index;
	      _bfd_elf_strtab_addref (elf_hash_table (info)->dynstr,
				      h->dynstr_index);
	      if (t->deps == NULL)
		defaux.vda_next = 0;
	      else
		defaux.vda_next = sizeof (Elf_External_Verdaux);
	      t->name_indx = defaux.vda_name;

	      _bfd_elf_swap_verdaux_out (output_bfd, &defaux,
					 (Elf_External_Verdaux *) p);
	      p += sizeof (Elf_External_Verdaux);

	      for (n = t->deps; n != NULL; n = n->next)
		{
		  if (n->version_needed == NULL)
		    {
		      /* This can happen if there was an error in the
			 version script.  */
		      defaux.vda_name = 0;
		    }
		  else
		    {
		      defaux.vda_name = n->version_needed->name_indx;
		      _bfd_elf_strtab_addref (elf_hash_table (info)->dynstr,
					      defaux.vda_name);
		    }
		  if (n->next == NULL)
		    defaux.vda_next = 0;
		  else
		    defaux.vda_next = sizeof (Elf_External_Verdaux);

		  _bfd_elf_swap_verdaux_out (output_bfd, &defaux,
					     (Elf_External_Verdaux *) p);
		  p += sizeof (Elf_External_Verdaux);
		}
	    }

	  if (! elf_add_dynamic_entry (info, DT_VERDEF, 0)
	      || ! elf_add_dynamic_entry (info, DT_VERDEFNUM, cdefs))
	    return FALSE;

	  elf_tdata (output_bfd)->cverdefs = cdefs;
	}

      if ((info->new_dtags && info->flags) || (info->flags & DF_STATIC_TLS))
	{
	  if (! elf_add_dynamic_entry (info, DT_FLAGS, info->flags))
	    return FALSE;
	}
      else if (info->flags & DF_BIND_NOW)
	{
	  if (! elf_add_dynamic_entry (info, DT_BIND_NOW, 0))
	    return FALSE;
	}

      if (info->flags_1)
	{
	  if (info->executable)
	    info->flags_1 &= ~ (DF_1_INITFIRST
				| DF_1_NODELETE
				| DF_1_NOOPEN);
	  if (! elf_add_dynamic_entry (info, DT_FLAGS_1, info->flags_1))
	    return FALSE;
	}

      /* Work out the size of the version reference section.  */

      s = bfd_get_section_by_name (dynobj, ".gnu.version_r");
      BFD_ASSERT (s != NULL);
      {
	struct elf_find_verdep_info sinfo;

	sinfo.output_bfd = output_bfd;
	sinfo.info = info;
	sinfo.vers = elf_tdata (output_bfd)->cverdefs;
	if (sinfo.vers == 0)
	  sinfo.vers = 1;
	sinfo.failed = FALSE;

	elf_link_hash_traverse (elf_hash_table (info),
				_bfd_elf_link_find_version_dependencies,
				&sinfo);

	if (elf_tdata (output_bfd)->verref == NULL)
	  _bfd_strip_section_from_output (info, s);
	else
	  {
	    Elf_Internal_Verneed *t;
	    unsigned int size;
	    unsigned int crefs;
	    bfd_byte *p;

	    /* Build the version definition section.  */
	    size = 0;
	    crefs = 0;
	    for (t = elf_tdata (output_bfd)->verref;
		 t != NULL;
		 t = t->vn_nextref)
	      {
		Elf_Internal_Vernaux *a;

		size += sizeof (Elf_External_Verneed);
		++crefs;
		for (a = t->vn_auxptr; a != NULL; a = a->vna_nextptr)
		  size += sizeof (Elf_External_Vernaux);
	      }

	    s->_raw_size = size;
	    s->contents = bfd_alloc (output_bfd, s->_raw_size);
	    if (s->contents == NULL)
	      return FALSE;

	    p = s->contents;
	    for (t = elf_tdata (output_bfd)->verref;
		 t != NULL;
		 t = t->vn_nextref)
	      {
		unsigned int caux;
		Elf_Internal_Vernaux *a;
		bfd_size_type indx;

		caux = 0;
		for (a = t->vn_auxptr; a != NULL; a = a->vna_nextptr)
		  ++caux;

		t->vn_version = VER_NEED_CURRENT;
		t->vn_cnt = caux;
		indx = _bfd_elf_strtab_add (elf_hash_table (info)->dynstr,
					    elf_dt_name (t->vn_bfd) != NULL
					    ? elf_dt_name (t->vn_bfd)
					    : basename (t->vn_bfd->filename),
					    FALSE);
		if (indx == (bfd_size_type) -1)
		  return FALSE;
		t->vn_file = indx;
		t->vn_aux = sizeof (Elf_External_Verneed);
		if (t->vn_nextref == NULL)
		  t->vn_next = 0;
		else
		  t->vn_next = (sizeof (Elf_External_Verneed)
				+ caux * sizeof (Elf_External_Vernaux));

		_bfd_elf_swap_verneed_out (output_bfd, t,
					   (Elf_External_Verneed *) p);
		p += sizeof (Elf_External_Verneed);

		for (a = t->vn_auxptr; a != NULL; a = a->vna_nextptr)
		  {
		    a->vna_hash = bfd_elf_hash (a->vna_nodename);
		    indx = _bfd_elf_strtab_add (elf_hash_table (info)->dynstr,
						a->vna_nodename, FALSE);
		    if (indx == (bfd_size_type) -1)
		      return FALSE;
		    a->vna_name = indx;
		    if (a->vna_nextptr == NULL)
		      a->vna_next = 0;
		    else
		      a->vna_next = sizeof (Elf_External_Vernaux);

		    _bfd_elf_swap_vernaux_out (output_bfd, a,
					       (Elf_External_Vernaux *) p);
		    p += sizeof (Elf_External_Vernaux);
		  }
	      }

	    if (! elf_add_dynamic_entry (info, DT_VERNEED, 0)
		|| ! elf_add_dynamic_entry (info, DT_VERNEEDNUM, crefs))
	      return FALSE;

	    elf_tdata (output_bfd)->cverrefs = crefs;
	  }
      }

      /* Assign dynsym indicies.  In a shared library we generate a
	 section symbol for each output section, which come first.
	 Next come all of the back-end allocated local dynamic syms,
	 followed by the rest of the global symbols.  */

      dynsymcount = _bfd_elf_link_renumber_dynsyms (output_bfd, info);

      /* Work out the size of the symbol version section.  */
      s = bfd_get_section_by_name (dynobj, ".gnu.version");
      BFD_ASSERT (s != NULL);
      if (dynsymcount == 0
	  || (verdefs == NULL && elf_tdata (output_bfd)->verref == NULL))
	{
	  _bfd_strip_section_from_output (info, s);
	  /* The DYNSYMCOUNT might have changed if we were going to
	     output a dynamic symbol table entry for S.  */
	  dynsymcount = _bfd_elf_link_renumber_dynsyms (output_bfd, info);
	}
      else
	{
	  s->_raw_size = dynsymcount * sizeof (Elf_External_Versym);
	  s->contents = bfd_zalloc (output_bfd, s->_raw_size);
	  if (s->contents == NULL)
	    return FALSE;

	  if (! elf_add_dynamic_entry (info, DT_VERSYM, 0))
	    return FALSE;
	}

      /* Set the size of the .dynsym and .hash sections.  We counted
	 the number of dynamic symbols in elf_link_add_object_symbols.
	 We will build the contents of .dynsym and .hash when we build
	 the final symbol table, because until then we do not know the
	 correct value to give the symbols.  We built the .dynstr
	 section as we went along in elf_link_add_object_symbols.  */
      s = bfd_get_section_by_name (dynobj, ".dynsym");
      BFD_ASSERT (s != NULL);
      s->_raw_size = dynsymcount * sizeof (Elf_External_Sym);
      s->contents = bfd_alloc (output_bfd, s->_raw_size);
      if (s->contents == NULL && s->_raw_size != 0)
	return FALSE;

      if (dynsymcount != 0)
	{
	  Elf_Internal_Sym isym;

	  /* The first entry in .dynsym is a dummy symbol.  */
	  isym.st_value = 0;
	  isym.st_size = 0;
	  isym.st_name = 0;
	  isym.st_info = 0;
	  isym.st_other = 0;
	  isym.st_shndx = 0;
	  elf_swap_symbol_out (output_bfd, &isym, s->contents, 0);
	}

      /* Compute the size of the hashing table.  As a side effect this
	 computes the hash values for all the names we export.  */
      bucketcount = compute_bucket_count (info);

      s = bfd_get_section_by_name (dynobj, ".hash");
      BFD_ASSERT (s != NULL);
      hash_entry_size = elf_section_data (s)->this_hdr.sh_entsize;
      s->_raw_size = ((2 + bucketcount + dynsymcount) * hash_entry_size);
      s->contents = bfd_zalloc (output_bfd, s->_raw_size);
      if (s->contents == NULL)
	return FALSE;

      bfd_put (8 * hash_entry_size, output_bfd, bucketcount, s->contents);
      bfd_put (8 * hash_entry_size, output_bfd, dynsymcount,
	       s->contents + hash_entry_size);

      elf_hash_table (info)->bucketcount = bucketcount;

      s = bfd_get_section_by_name (dynobj, ".dynstr");
      BFD_ASSERT (s != NULL);

      elf_finalize_dynstr (output_bfd, info);

      s->_raw_size = _bfd_elf_strtab_size (elf_hash_table (info)->dynstr);

      for (dtagcount = 0; dtagcount <= info->spare_dynamic_tags; ++dtagcount)
	if (! elf_add_dynamic_entry (info, DT_NULL, 0))
	  return FALSE;
    }

  return TRUE;
}

/* This function is used to adjust offsets into .dynstr for
   dynamic symbols.  This is called via elf_link_hash_traverse.  */

static bfd_boolean
elf_adjust_dynstr_offsets (struct elf_link_hash_entry *h, void *data)
{
  struct elf_strtab_hash *dynstr = data;

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  if (h->dynindx != -1)
    h->dynstr_index = _bfd_elf_strtab_offset (dynstr, h->dynstr_index);
  return TRUE;
}

/* Assign string offsets in .dynstr, update all structures referencing
   them.  */

static bfd_boolean
elf_finalize_dynstr (bfd *output_bfd, struct bfd_link_info *info)
{
  struct elf_link_local_dynamic_entry *entry;
  struct elf_strtab_hash *dynstr = elf_hash_table (info)->dynstr;
  bfd *dynobj = elf_hash_table (info)->dynobj;
  asection *sdyn;
  bfd_size_type size;
  Elf_External_Dyn *dyncon, *dynconend;

  _bfd_elf_strtab_finalize (dynstr);
  size = _bfd_elf_strtab_size (dynstr);

  /* Update all .dynamic entries referencing .dynstr strings.  */
  sdyn = bfd_get_section_by_name (dynobj, ".dynamic");
  BFD_ASSERT (sdyn != NULL);

  dyncon = (Elf_External_Dyn *) sdyn->contents;
  dynconend = (Elf_External_Dyn *) (sdyn->contents +
				    sdyn->_raw_size);
  for (; dyncon < dynconend; dyncon++)
    {
      Elf_Internal_Dyn dyn;

      elf_swap_dyn_in (dynobj, dyncon, & dyn);
      switch (dyn.d_tag)
	{
	case DT_STRSZ:
	  dyn.d_un.d_val = size;
	  elf_swap_dyn_out (dynobj, & dyn, dyncon);
	  break;
	case DT_NEEDED:
	case DT_SONAME:
	case DT_RPATH:
	case DT_RUNPATH:
	case DT_FILTER:
	case DT_AUXILIARY:
	  dyn.d_un.d_val = _bfd_elf_strtab_offset (dynstr, dyn.d_un.d_val);
	  elf_swap_dyn_out (dynobj, & dyn, dyncon);
	  break;
	default:
	  break;
	}
    }

  /* Now update local dynamic symbols.  */
  for (entry = elf_hash_table (info)->dynlocal; entry ; entry = entry->next)
    entry->isym.st_name = _bfd_elf_strtab_offset (dynstr,
						  entry->isym.st_name);

  /* And the rest of dynamic symbols.  */
  elf_link_hash_traverse (elf_hash_table (info),
			  elf_adjust_dynstr_offsets, dynstr);

  /* Adjust version definitions.  */
  if (elf_tdata (output_bfd)->cverdefs)
    {
      asection *s;
      bfd_byte *p;
      bfd_size_type i;
      Elf_Internal_Verdef def;
      Elf_Internal_Verdaux defaux;

      s = bfd_get_section_by_name (dynobj, ".gnu.version_d");
      p = (bfd_byte *) s->contents;
      do
	{
	  _bfd_elf_swap_verdef_in (output_bfd, (Elf_External_Verdef *) p,
				   &def);
	  p += sizeof (Elf_External_Verdef);
	  for (i = 0; i < def.vd_cnt; ++i)
	    {
	      _bfd_elf_swap_verdaux_in (output_bfd,
					(Elf_External_Verdaux *) p, &defaux);
	      defaux.vda_name = _bfd_elf_strtab_offset (dynstr,
							defaux.vda_name);
	      _bfd_elf_swap_verdaux_out (output_bfd,
					 &defaux, (Elf_External_Verdaux *) p);
	      p += sizeof (Elf_External_Verdaux);
	    }
	}
      while (def.vd_next);
    }

  /* Adjust version references.  */
  if (elf_tdata (output_bfd)->verref)
    {
      asection *s;
      bfd_byte *p;
      bfd_size_type i;
      Elf_Internal_Verneed need;
      Elf_Internal_Vernaux needaux;

      s = bfd_get_section_by_name (dynobj, ".gnu.version_r");
      p = (bfd_byte *) s->contents;
      do
	{
	  _bfd_elf_swap_verneed_in (output_bfd, (Elf_External_Verneed *) p,
				    &need);
	  need.vn_file = _bfd_elf_strtab_offset (dynstr, need.vn_file);
	  _bfd_elf_swap_verneed_out (output_bfd, &need,
				     (Elf_External_Verneed *) p);
	  p += sizeof (Elf_External_Verneed);
	  for (i = 0; i < need.vn_cnt; ++i)
	    {
	      _bfd_elf_swap_vernaux_in (output_bfd,
					(Elf_External_Vernaux *) p, &needaux);
	      needaux.vna_name = _bfd_elf_strtab_offset (dynstr,
							 needaux.vna_name);
	      _bfd_elf_swap_vernaux_out (output_bfd,
					 &needaux,
					 (Elf_External_Vernaux *) p);
	      p += sizeof (Elf_External_Vernaux);
	    }
	}
      while (need.vn_next);
    }

  return TRUE;
}

/* Final phase of ELF linker.  */

/* A structure we use to avoid passing large numbers of arguments.  */

struct elf_final_link_info
{
  /* General link information.  */
  struct bfd_link_info *info;
  /* Output BFD.  */
  bfd *output_bfd;
  /* Symbol string table.  */
  struct bfd_strtab_hash *symstrtab;
  /* .dynsym section.  */
  asection *dynsym_sec;
  /* .hash section.  */
  asection *hash_sec;
  /* symbol version section (.gnu.version).  */
  asection *symver_sec;
  /* Buffer large enough to hold contents of any section.  */
  bfd_byte *contents;
  /* Buffer large enough to hold external relocs of any section.  */
  void *external_relocs;
  /* Buffer large enough to hold internal relocs of any section.  */
  Elf_Internal_Rela *internal_relocs;
  /* Buffer large enough to hold external local symbols of any input
     BFD.  */
  Elf_External_Sym *external_syms;
  /* And a buffer for symbol section indices.  */
  Elf_External_Sym_Shndx *locsym_shndx;
  /* Buffer large enough to hold internal local symbols of any input
     BFD.  */
  Elf_Internal_Sym *internal_syms;
  /* Array large enough to hold a symbol index for each local symbol
     of any input BFD.  */
  long *indices;
  /* Array large enough to hold a section pointer for each local
     symbol of any input BFD.  */
  asection **sections;
  /* Buffer to hold swapped out symbols.  */
  Elf_External_Sym *symbuf;
  /* And one for symbol section indices.  */
  Elf_External_Sym_Shndx *symshndxbuf;
  /* Number of swapped out symbols in buffer.  */
  size_t symbuf_count;
  /* Number of symbols which fit in symbuf.  */
  size_t symbuf_size;
  /* And same for symshndxbuf.  */
  size_t shndxbuf_size;
};

static bfd_boolean elf_link_output_sym
  (struct elf_final_link_info *, const char *, Elf_Internal_Sym *, asection *,
   struct elf_link_hash_entry *);
static bfd_boolean elf_link_flush_output_syms
  (struct elf_final_link_info *);
static bfd_boolean elf_link_output_extsym
  (struct elf_link_hash_entry *, void *);
static bfd_boolean elf_link_input_bfd
  (struct elf_final_link_info *, bfd *);
static bfd_boolean elf_reloc_link_order
  (bfd *, struct bfd_link_info *, asection *, struct bfd_link_order *);

/* This struct is used to pass information to elf_link_output_extsym.  */

struct elf_outext_info
{
  bfd_boolean failed;
  bfd_boolean localsyms;
  struct elf_final_link_info *finfo;
};

/* When performing a relocatable link, the input relocations are
   preserved.  But, if they reference global symbols, the indices
   referenced must be updated.  Update all the relocations in
   REL_HDR (there are COUNT of them), using the data in REL_HASH.  */

static void
elf_link_adjust_relocs (bfd *abfd,
			Elf_Internal_Shdr *rel_hdr,
			unsigned int count,
			struct elf_link_hash_entry **rel_hash)
{
  unsigned int i;
  const struct elf_backend_data *bed = get_elf_backend_data (abfd);
  bfd_byte *erela;
  void (*swap_in) (bfd *, const bfd_byte *, Elf_Internal_Rela *);
  void (*swap_out) (bfd *, const Elf_Internal_Rela *, bfd_byte *);

  if (rel_hdr->sh_entsize == sizeof (Elf_External_Rel))
    {
      swap_in = bed->s->swap_reloc_in;
      swap_out = bed->s->swap_reloc_out;
    }
  else if (rel_hdr->sh_entsize == sizeof (Elf_External_Rela))
    {
      swap_in = bed->s->swap_reloca_in;
      swap_out = bed->s->swap_reloca_out;
    }
  else
    abort ();

  if (bed->s->int_rels_per_ext_rel > MAX_INT_RELS_PER_EXT_REL)
    abort ();

  erela = rel_hdr->contents;
  for (i = 0; i < count; i++, rel_hash++, erela += rel_hdr->sh_entsize)
    {
      Elf_Internal_Rela irela[MAX_INT_RELS_PER_EXT_REL];
      unsigned int j;

      if (*rel_hash == NULL)
	continue;

      BFD_ASSERT ((*rel_hash)->indx >= 0);

      (*swap_in) (abfd, erela, irela);
      for (j = 0; j < bed->s->int_rels_per_ext_rel; j++)
	irela[j].r_info = ELF_R_INFO ((*rel_hash)->indx,
				      ELF_R_TYPE (irela[j].r_info));
      (*swap_out) (abfd, irela, erela);
    }
}

struct elf_link_sort_rela
{
  bfd_vma offset;
  enum elf_reloc_type_class type;
  /* We use this as an array of size int_rels_per_ext_rel.  */
  Elf_Internal_Rela rela[1];
};

static int
elf_link_sort_cmp1 (const void *A, const void *B)
{
  const struct elf_link_sort_rela *a = A;
  const struct elf_link_sort_rela *b = B;
  int relativea, relativeb;

  relativea = a->type == reloc_class_relative;
  relativeb = b->type == reloc_class_relative;

  if (relativea < relativeb)
    return 1;
  if (relativea > relativeb)
    return -1;
  if (ELF_R_SYM (a->rela->r_info) < ELF_R_SYM (b->rela->r_info))
    return -1;
  if (ELF_R_SYM (a->rela->r_info) > ELF_R_SYM (b->rela->r_info))
    return 1;
  if (a->rela->r_offset < b->rela->r_offset)
    return -1;
  if (a->rela->r_offset > b->rela->r_offset)
    return 1;
  return 0;
}

static int
elf_link_sort_cmp2 (const void *A, const void *B)
{
  const struct elf_link_sort_rela *a = A;
  const struct elf_link_sort_rela *b = B;
  int copya, copyb;

  if (a->offset < b->offset)
    return -1;
  if (a->offset > b->offset)
    return 1;
  copya = (a->type == reloc_class_copy) * 2 + (a->type == reloc_class_plt);
  copyb = (b->type == reloc_class_copy) * 2 + (b->type == reloc_class_plt);
  if (copya < copyb)
    return -1;
  if (copya > copyb)
    return 1;
  if (a->rela->r_offset < b->rela->r_offset)
    return -1;
  if (a->rela->r_offset > b->rela->r_offset)
    return 1;
  return 0;
}

static size_t
elf_link_sort_relocs (bfd *abfd, struct bfd_link_info *info, asection **psec)
{
  asection *reldyn;
  bfd_size_type count, size;
  size_t i, ret, sort_elt, ext_size;
  bfd_byte *sort, *s_non_relative, *p;
  struct elf_link_sort_rela *sq;
  const struct elf_backend_data *bed = get_elf_backend_data (abfd);
  int i2e = bed->s->int_rels_per_ext_rel;
  void (*swap_in) (bfd *, const bfd_byte *, Elf_Internal_Rela *);
  void (*swap_out) (bfd *, const Elf_Internal_Rela *, bfd_byte *);
  struct bfd_link_order *lo;

  reldyn = bfd_get_section_by_name (abfd, ".rela.dyn");
  if (reldyn == NULL || reldyn->_raw_size == 0)
    {
      reldyn = bfd_get_section_by_name (abfd, ".rel.dyn");
      if (reldyn == NULL || reldyn->_raw_size == 0)
	return 0;
      ext_size = sizeof (Elf_External_Rel);
      swap_in = bed->s->swap_reloc_in;
      swap_out = bed->s->swap_reloc_out;
    }
  else
    {
      ext_size = sizeof (Elf_External_Rela);
      swap_in = bed->s->swap_reloca_in;
      swap_out = bed->s->swap_reloca_out;
    }
  count = reldyn->_raw_size / ext_size;

  size = 0;
  for (lo = reldyn->link_order_head; lo != NULL; lo = lo->next)
    if (lo->type == bfd_indirect_link_order)
      {
	asection *o = lo->u.indirect.section;
	size += o->_raw_size;
      }

  if (size != reldyn->_raw_size)
    return 0;

  sort_elt = (sizeof (struct elf_link_sort_rela)
	      + (i2e - 1) * sizeof (Elf_Internal_Rela));
  sort = bfd_zmalloc (sort_elt * count);
  if (sort == NULL)
    {
      (*info->callbacks->warning)
	(info, _("Not enough memory to sort relocations"), 0, abfd, 0, 0);
      return 0;
    }

  for (lo = reldyn->link_order_head; lo != NULL; lo = lo->next)
    if (lo->type == bfd_indirect_link_order)
      {
	bfd_byte *erel, *erelend;
	asection *o = lo->u.indirect.section;

	erel = o->contents;
	erelend = o->contents + o->_raw_size;
	p = sort + o->output_offset / ext_size * sort_elt;
	while (erel < erelend)
	  {
	    struct elf_link_sort_rela *s = (struct elf_link_sort_rela *) p;
	    (*swap_in) (abfd, erel, s->rela);
	    s->type = (*bed->elf_backend_reloc_type_class) (s->rela);
	    p += sort_elt;
	    erel += ext_size;
	  }
      }

  qsort (sort, count, sort_elt, elf_link_sort_cmp1);

  for (i = 0, p = sort; i < count; i++, p += sort_elt)
    {
      struct elf_link_sort_rela *s = (struct elf_link_sort_rela *) p;
      if (s->type != reloc_class_relative)
	break;
    }
  ret = i;
  s_non_relative = p;

  sq = (struct elf_link_sort_rela *) s_non_relative;
  for (; i < count; i++, p += sort_elt)
    {
      struct elf_link_sort_rela *sp = (struct elf_link_sort_rela *) p;
      if (ELF_R_SYM (sp->rela->r_info) != ELF_R_SYM (sq->rela->r_info))
	sq = sp;
      sp->offset = sq->rela->r_offset;
    }

  qsort (s_non_relative, count - ret, sort_elt, elf_link_sort_cmp2);

  for (lo = reldyn->link_order_head; lo != NULL; lo = lo->next)
    if (lo->type == bfd_indirect_link_order)
      {
	bfd_byte *erel, *erelend;
	asection *o = lo->u.indirect.section;

	erel = o->contents;
	erelend = o->contents + o->_raw_size;
	p = sort + o->output_offset / ext_size * sort_elt;
	while (erel < erelend)
	  {
	    struct elf_link_sort_rela *s = (struct elf_link_sort_rela *) p;
	    (*swap_out) (abfd, s->rela, erel);
	    p += sort_elt;
	    erel += ext_size;
	  }
      }

  free (sort);
  *psec = reldyn;
  return ret;
}

/* Do the final step of an ELF link.  */

bfd_boolean
elf_bfd_final_link (bfd *abfd, struct bfd_link_info *info)
{
  bfd_boolean dynamic;
  bfd_boolean emit_relocs;
  bfd *dynobj;
  struct elf_final_link_info finfo;
  register asection *o;
  register struct bfd_link_order *p;
  register bfd *sub;
  bfd_size_type max_contents_size;
  bfd_size_type max_external_reloc_size;
  bfd_size_type max_internal_reloc_count;
  bfd_size_type max_sym_count;
  bfd_size_type max_sym_shndx_count;
  file_ptr off;
  Elf_Internal_Sym elfsym;
  unsigned int i;
  Elf_Internal_Shdr *symtab_hdr;
  Elf_Internal_Shdr *symtab_shndx_hdr;
  Elf_Internal_Shdr *symstrtab_hdr;
  const struct elf_backend_data *bed = get_elf_backend_data (abfd);
  struct elf_outext_info eoinfo;
  bfd_boolean merged;
  size_t relativecount = 0;
  asection *reldyn = 0;
  bfd_size_type amt;

  if (! is_elf_hash_table (info->hash))
    return FALSE;

  if (info->shared)
    abfd->flags |= DYNAMIC;

  dynamic = elf_hash_table (info)->dynamic_sections_created;
  dynobj = elf_hash_table (info)->dynobj;

  emit_relocs = (info->relocatable
		 || info->emitrelocations
		 || bed->elf_backend_emit_relocs);

  finfo.info = info;
  finfo.output_bfd = abfd;
  finfo.symstrtab = elf_stringtab_init ();
  if (finfo.symstrtab == NULL)
    return FALSE;

  if (! dynamic)
    {
      finfo.dynsym_sec = NULL;
      finfo.hash_sec = NULL;
      finfo.symver_sec = NULL;
    }
  else
    {
      finfo.dynsym_sec = bfd_get_section_by_name (dynobj, ".dynsym");
      finfo.hash_sec = bfd_get_section_by_name (dynobj, ".hash");
      BFD_ASSERT (finfo.dynsym_sec != NULL && finfo.hash_sec != NULL);
      finfo.symver_sec = bfd_get_section_by_name (dynobj, ".gnu.version");
      /* Note that it is OK if symver_sec is NULL.  */
    }

  finfo.contents = NULL;
  finfo.external_relocs = NULL;
  finfo.internal_relocs = NULL;
  finfo.external_syms = NULL;
  finfo.locsym_shndx = NULL;
  finfo.internal_syms = NULL;
  finfo.indices = NULL;
  finfo.sections = NULL;
  finfo.symbuf = NULL;
  finfo.symshndxbuf = NULL;
  finfo.symbuf_count = 0;
  finfo.shndxbuf_size = 0;

  /* Count up the number of relocations we will output for each output
     section, so that we know the sizes of the reloc sections.  We
     also figure out some maximum sizes.  */
  max_contents_size = 0;
  max_external_reloc_size = 0;
  max_internal_reloc_count = 0;
  max_sym_count = 0;
  max_sym_shndx_count = 0;
  merged = FALSE;
  for (o = abfd->sections; o != NULL; o = o->next)
    {
      struct bfd_elf_section_data *esdo = elf_section_data (o);
      o->reloc_count = 0;

      for (p = o->link_order_head; p != NULL; p = p->next)
	{
	  unsigned int reloc_count = 0;
	  struct bfd_elf_section_data *esdi = NULL;
	  unsigned int *rel_count1;

	  if (p->type == bfd_section_reloc_link_order
	      || p->type == bfd_symbol_reloc_link_order)
	    reloc_count = 1;
	  else if (p->type == bfd_indirect_link_order)
	    {
	      asection *sec;

	      sec = p->u.indirect.section;
	      esdi = elf_section_data (sec);

	      /* Mark all sections which are to be included in the
		 link.  This will normally be every section.  We need
		 to do this so that we can identify any sections which
		 the linker has decided to not include.  */
	      sec->linker_mark = TRUE;

	      if (sec->flags & SEC_MERGE)
		merged = TRUE;

	      if (info->relocatable || info->emitrelocations)
		reloc_count = sec->reloc_count;
	      else if (bed->elf_backend_count_relocs)
		{
		  Elf_Internal_Rela * relocs;

		  relocs = _bfd_elf_link_read_relocs (abfd, sec, NULL, NULL,
						      info->keep_memory);

		  reloc_count = (*bed->elf_backend_count_relocs) (sec, relocs);

		  if (elf_section_data (o)->relocs != relocs)
		    free (relocs);
		}

	      if (sec->_raw_size > max_contents_size)
		max_contents_size = sec->_raw_size;
	      if (sec->_cooked_size > max_contents_size)
		max_contents_size = sec->_cooked_size;

	      /* We are interested in just local symbols, not all
		 symbols.  */
	      if (bfd_get_flavour (sec->owner) == bfd_target_elf_flavour
		  && (sec->owner->flags & DYNAMIC) == 0)
		{
		  size_t sym_count;

		  if (elf_bad_symtab (sec->owner))
		    sym_count = (elf_tdata (sec->owner)->symtab_hdr.sh_size
				 / sizeof (Elf_External_Sym));
		  else
		    sym_count = elf_tdata (sec->owner)->symtab_hdr.sh_info;

		  if (sym_count > max_sym_count)
		    max_sym_count = sym_count;

		  if (sym_count > max_sym_shndx_count
		      && elf_symtab_shndx (sec->owner) != 0)
		    max_sym_shndx_count = sym_count;

		  if ((sec->flags & SEC_RELOC) != 0)
		    {
		      size_t ext_size;

		      ext_size = elf_section_data (sec)->rel_hdr.sh_size;
		      if (ext_size > max_external_reloc_size)
			max_external_reloc_size = ext_size;
		      if (sec->reloc_count > max_internal_reloc_count)
			max_internal_reloc_count = sec->reloc_count;
		    }
		}
	    }

	  if (reloc_count == 0)
	    continue;

	  o->reloc_count += reloc_count;

	  /* MIPS may have a mix of REL and RELA relocs on sections.
	     To support this curious ABI we keep reloc counts in
	     elf_section_data too.  We must be careful to add the
	     relocations from the input section to the right output
	     count.  FIXME: Get rid of one count.  We have
	     o->reloc_count == esdo->rel_count + esdo->rel_count2.  */
	  rel_count1 = &esdo->rel_count;
	  if (esdi != NULL)
	    {
	      bfd_boolean same_size;
	      bfd_size_type entsize1;

	      entsize1 = esdi->rel_hdr.sh_entsize;
	      BFD_ASSERT (entsize1 == sizeof (Elf_External_Rel)
			  || entsize1 == sizeof (Elf_External_Rela));
	      same_size = (!o->use_rela_p
			   == (entsize1 == sizeof (Elf_External_Rel)));

	      if (!same_size)
		rel_count1 = &esdo->rel_count2;

	      if (esdi->rel_hdr2 != NULL)
		{
		  bfd_size_type entsize2 = esdi->rel_hdr2->sh_entsize;
		  unsigned int alt_count;
		  unsigned int *rel_count2;

		  BFD_ASSERT (entsize2 != entsize1
			      && (entsize2 == sizeof (Elf_External_Rel)
				  || entsize2 == sizeof (Elf_External_Rela)));

		  rel_count2 = &esdo->rel_count2;
		  if (!same_size)
		    rel_count2 = &esdo->rel_count;

		  /* The following is probably too simplistic if the
		     backend counts output relocs unusually.  */
		  BFD_ASSERT (bed->elf_backend_count_relocs == NULL);
		  alt_count = NUM_SHDR_ENTRIES (esdi->rel_hdr2);
		  *rel_count2 += alt_count;
		  reloc_count -= alt_count;
		}
	    }
	  *rel_count1 += reloc_count;
	}

      if (o->reloc_count > 0)
	o->flags |= SEC_RELOC;
      else
	{
	  /* Explicitly clear the SEC_RELOC flag.  The linker tends to
	     set it (this is probably a bug) and if it is set
	     assign_section_numbers will create a reloc section.  */
	  o->flags &=~ SEC_RELOC;
	}

      /* If the SEC_ALLOC flag is not set, force the section VMA to
	 zero.  This is done in elf_fake_sections as well, but forcing
	 the VMA to 0 here will ensure that relocs against these
	 sections are handled correctly.  */
      if ((o->flags & SEC_ALLOC) == 0
	  && ! o->user_set_vma)
	o->vma = 0;
    }

  if (! info->relocatable && merged)
    elf_link_hash_traverse (elf_hash_table (info),
			    _bfd_elf_link_sec_merge_syms, abfd);

  /* Figure out the file positions for everything but the symbol table
     and the relocs.  We set symcount to force assign_section_numbers
     to create a symbol table.  */
  bfd_get_symcount (abfd) = info->strip == strip_all ? 0 : 1;
  BFD_ASSERT (! abfd->output_has_begun);
  if (! _bfd_elf_compute_section_file_positions (abfd, info))
    goto error_return;

  /* That created the reloc sections.  Set their sizes, and assign
     them file positions, and allocate some buffers.  */
  for (o = abfd->sections; o != NULL; o = o->next)
    {
      if ((o->flags & SEC_RELOC) != 0)
	{
	  if (!(_bfd_elf_link_size_reloc_section
		(abfd, &elf_section_data (o)->rel_hdr, o)))
	    goto error_return;

	  if (elf_section_data (o)->rel_hdr2
	      && !(_bfd_elf_link_size_reloc_section
		   (abfd, elf_section_data (o)->rel_hdr2, o)))
	    goto error_return;
	}

      /* Now, reset REL_COUNT and REL_COUNT2 so that we can use them
	 to count upwards while actually outputting the relocations.  */
      elf_section_data (o)->rel_count = 0;
      elf_section_data (o)->rel_count2 = 0;
    }

  _bfd_elf_assign_file_positions_for_relocs (abfd);

  /* We have now assigned file positions for all the sections except
     .symtab and .strtab.  We start the .symtab section at the current
     file position, and write directly to it.  We build the .strtab
     section in memory.  */
  bfd_get_symcount (abfd) = 0;
  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  /* sh_name is set in prep_headers.  */
  symtab_hdr->sh_type = SHT_SYMTAB;
  /* sh_flags, sh_addr and sh_size all start off zero.  */
  symtab_hdr->sh_entsize = sizeof (Elf_External_Sym);
  /* sh_link is set in assign_section_numbers.  */
  /* sh_info is set below.  */
  /* sh_offset is set just below.  */
  symtab_hdr->sh_addralign = 1 << bed->s->log_file_align;

  off = elf_tdata (abfd)->next_file_pos;
  off = _bfd_elf_assign_file_position_for_section (symtab_hdr, off, TRUE);

  /* Note that at this point elf_tdata (abfd)->next_file_pos is
     incorrect.  We do not yet know the size of the .symtab section.
     We correct next_file_pos below, after we do know the size.  */

  /* Allocate a buffer to hold swapped out symbols.  This is to avoid
     continuously seeking to the right position in the file.  */
  if (! info->keep_memory || max_sym_count < 20)
    finfo.symbuf_size = 20;
  else
    finfo.symbuf_size = max_sym_count;
  amt = finfo.symbuf_size;
  amt *= sizeof (Elf_External_Sym);
  finfo.symbuf = bfd_malloc (amt);
  if (finfo.symbuf == NULL)
    goto error_return;
  if (elf_numsections (abfd) > SHN_LORESERVE)
    {
      /* Wild guess at number of output symbols.  realloc'd as needed.  */
      amt = 2 * max_sym_count + elf_numsections (abfd) + 1000;
      finfo.shndxbuf_size = amt;
      amt *= sizeof (Elf_External_Sym_Shndx);
      finfo.symshndxbuf = bfd_zmalloc (amt);
      if (finfo.symshndxbuf == NULL)
	goto error_return;
    }

  /* Start writing out the symbol table.  The first symbol is always a
     dummy symbol.  */
  if (info->strip != strip_all
      || emit_relocs)
    {
      elfsym.st_value = 0;
      elfsym.st_size = 0;
      elfsym.st_info = 0;
      elfsym.st_other = 0;
      elfsym.st_shndx = SHN_UNDEF;
      if (! elf_link_output_sym (&finfo, NULL, &elfsym, bfd_und_section_ptr,
				 NULL))
	goto error_return;
    }

#if 0
  /* Some standard ELF linkers do this, but we don't because it causes
     bootstrap comparison failures.  */
  /* Output a file symbol for the output file as the second symbol.
     We output this even if we are discarding local symbols, although
     I'm not sure if this is correct.  */
  elfsym.st_value = 0;
  elfsym.st_size = 0;
  elfsym.st_info = ELF_ST_INFO (STB_LOCAL, STT_FILE);
  elfsym.st_other = 0;
  elfsym.st_shndx = SHN_ABS;
  if (! elf_link_output_sym (&finfo, bfd_get_filename (abfd),
			     &elfsym, bfd_abs_section_ptr, NULL))
    goto error_return;
#endif

  /* Output a symbol for each section.  We output these even if we are
     discarding local symbols, since they are used for relocs.  These
     symbols have no names.  We store the index of each one in the
     index field of the section, so that we can find it again when
     outputting relocs.  */
  if (info->strip != strip_all
      || emit_relocs)
    {
      elfsym.st_size = 0;
      elfsym.st_info = ELF_ST_INFO (STB_LOCAL, STT_SECTION);
      elfsym.st_other = 0;
      for (i = 1; i < elf_numsections (abfd); i++)
	{
	  o = section_from_elf_index (abfd, i);
	  if (o != NULL)
	    o->target_index = bfd_get_symcount (abfd);
	  elfsym.st_shndx = i;
	  if (info->relocatable || o == NULL)
	    elfsym.st_value = 0;
	  else
	    elfsym.st_value = o->vma;
	  if (! elf_link_output_sym (&finfo, NULL, &elfsym, o, NULL))
	    goto error_return;
	  if (i == SHN_LORESERVE - 1)
	    i += SHN_HIRESERVE + 1 - SHN_LORESERVE;
	}
    }

  /* Allocate some memory to hold information read in from the input
     files.  */
  if (max_contents_size != 0)
    {
      finfo.contents = bfd_malloc (max_contents_size);
      if (finfo.contents == NULL)
	goto error_return;
    }

  if (max_external_reloc_size != 0)
    {
      finfo.external_relocs = bfd_malloc (max_external_reloc_size);
      if (finfo.external_relocs == NULL)
	goto error_return;
    }

  if (max_internal_reloc_count != 0)
    {
      amt = max_internal_reloc_count * bed->s->int_rels_per_ext_rel;
      amt *= sizeof (Elf_Internal_Rela);
      finfo.internal_relocs = bfd_malloc (amt);
      if (finfo.internal_relocs == NULL)
	goto error_return;
    }

  if (max_sym_count != 0)
    {
      amt = max_sym_count * sizeof (Elf_External_Sym);
      finfo.external_syms = bfd_malloc (amt);
      if (finfo.external_syms == NULL)
	goto error_return;

      amt = max_sym_count * sizeof (Elf_Internal_Sym);
      finfo.internal_syms = bfd_malloc (amt);
      if (finfo.internal_syms == NULL)
	goto error_return;

      amt = max_sym_count * sizeof (long);
      finfo.indices = bfd_malloc (amt);
      if (finfo.indices == NULL)
	goto error_return;

      amt = max_sym_count * sizeof (asection *);
      finfo.sections = bfd_malloc (amt);
      if (finfo.sections == NULL)
	goto error_return;
    }

  if (max_sym_shndx_count != 0)
    {
      amt = max_sym_shndx_count * sizeof (Elf_External_Sym_Shndx);
      finfo.locsym_shndx = bfd_malloc (amt);
      if (finfo.locsym_shndx == NULL)
	goto error_return;
    }

  if (elf_hash_table (info)->tls_sec)
    {
      bfd_vma base, end = 0;
      asection *sec;

      for (sec = elf_hash_table (info)->tls_sec;
	   sec && (sec->flags & SEC_THREAD_LOCAL);
	   sec = sec->next)
	{
	  bfd_vma size = sec->_raw_size;

	  if (size == 0 && (sec->flags & SEC_HAS_CONTENTS) == 0)
	    {
	      struct bfd_link_order *o;

	      for (o = sec->link_order_head; o != NULL; o = o->next)
		if (size < o->offset + o->size)
		  size = o->offset + o->size;
	    }
	  end = sec->vma + size;
	}
      base = elf_hash_table (info)->tls_sec->vma;
      end = align_power (end, elf_hash_table (info)->tls_sec->alignment_power);
      elf_hash_table (info)->tls_size = end - base;
    }

  /* Since ELF permits relocations to be against local symbols, we
     must have the local symbols available when we do the relocations.
     Since we would rather only read the local symbols once, and we
     would rather not keep them in memory, we handle all the
     relocations for a single input file at the same time.

     Unfortunately, there is no way to know the total number of local
     symbols until we have seen all of them, and the local symbol
     indices precede the global symbol indices.  This means that when
     we are generating relocatable output, and we see a reloc against
     a global symbol, we can not know the symbol index until we have
     finished examining all the local symbols to see which ones we are
     going to output.  To deal with this, we keep the relocations in
     memory, and don't output them until the end of the link.  This is
     an unfortunate waste of memory, but I don't see a good way around
     it.  Fortunately, it only happens when performing a relocatable
     link, which is not the common case.  FIXME: If keep_memory is set
     we could write the relocs out and then read them again; I don't
     know how bad the memory loss will be.  */

  for (sub = info->input_bfds; sub != NULL; sub = sub->link_next)
    sub->output_has_begun = FALSE;
  for (o = abfd->sections; o != NULL; o = o->next)
    {
      for (p = o->link_order_head; p != NULL; p = p->next)
	{
	  if (p->type == bfd_indirect_link_order
	      && (bfd_get_flavour ((sub = p->u.indirect.section->owner))
		  == bfd_target_elf_flavour)
	      && elf_elfheader (sub)->e_ident[EI_CLASS] == bed->s->elfclass)
	    {
	      if (! sub->output_has_begun)
		{
		  if (! elf_link_input_bfd (&finfo, sub))
		    goto error_return;
		  sub->output_has_begun = TRUE;
		}
	    }
	  else if (p->type == bfd_section_reloc_link_order
		   || p->type == bfd_symbol_reloc_link_order)
	    {
	      if (! elf_reloc_link_order (abfd, info, o, p))
		goto error_return;
	    }
	  else
	    {
	      if (! _bfd_default_link_order (abfd, info, o, p))
		goto error_return;
	    }
	}
    }

  /* Output any global symbols that got converted to local in a
     version script or due to symbol visibility.  We do this in a
     separate step since ELF requires all local symbols to appear
     prior to any global symbols.  FIXME: We should only do this if
     some global symbols were, in fact, converted to become local.
     FIXME: Will this work correctly with the Irix 5 linker?  */
  eoinfo.failed = FALSE;
  eoinfo.finfo = &finfo;
  eoinfo.localsyms = TRUE;
  elf_link_hash_traverse (elf_hash_table (info), elf_link_output_extsym,
			  &eoinfo);
  if (eoinfo.failed)
    return FALSE;

  /* That wrote out all the local symbols.  Finish up the symbol table
     with the global symbols. Even if we want to strip everything we
     can, we still need to deal with those global symbols that got
     converted to local in a version script.  */

  /* The sh_info field records the index of the first non local symbol.  */
  symtab_hdr->sh_info = bfd_get_symcount (abfd);

  if (dynamic
      && finfo.dynsym_sec->output_section != bfd_abs_section_ptr)
    {
      Elf_Internal_Sym sym;
      Elf_External_Sym *dynsym =
	(Elf_External_Sym *) finfo.dynsym_sec->contents;
      long last_local = 0;

      /* Write out the section symbols for the output sections.  */
      if (info->shared)
	{
	  asection *s;

	  sym.st_size = 0;
	  sym.st_name = 0;
	  sym.st_info = ELF_ST_INFO (STB_LOCAL, STT_SECTION);
	  sym.st_other = 0;

	  for (s = abfd->sections; s != NULL; s = s->next)
	    {
	      int indx;
	      Elf_External_Sym *dest;

	      indx = elf_section_data (s)->this_idx;
	      BFD_ASSERT (indx > 0);
	      sym.st_shndx = indx;
	      sym.st_value = s->vma;
	      dest = dynsym + elf_section_data (s)->dynindx;
	      elf_swap_symbol_out (abfd, &sym, dest, 0);
	    }

	  last_local = bfd_count_sections (abfd);
	}

      /* Write out the local dynsyms.  */
      if (elf_hash_table (info)->dynlocal)
	{
	  struct elf_link_local_dynamic_entry *e;
	  for (e = elf_hash_table (info)->dynlocal; e ; e = e->next)
	    {
	      asection *s;
	      Elf_External_Sym *dest;

	      sym.st_size = e->isym.st_size;
	      sym.st_other = e->isym.st_other;

	      /* Copy the internal symbol as is.
		 Note that we saved a word of storage and overwrote
		 the original st_name with the dynstr_index.  */
	      sym = e->isym;

	      if (e->isym.st_shndx != SHN_UNDEF
		  && (e->isym.st_shndx < SHN_LORESERVE
		      || e->isym.st_shndx > SHN_HIRESERVE))
		{
		  s = bfd_section_from_elf_index (e->input_bfd,
						  e->isym.st_shndx);

		  sym.st_shndx =
		    elf_section_data (s->output_section)->this_idx;
		  sym.st_value = (s->output_section->vma
				  + s->output_offset
				  + e->isym.st_value);
		}

	      if (last_local < e->dynindx)
		last_local = e->dynindx;

	      dest = dynsym + e->dynindx;
	      elf_swap_symbol_out (abfd, &sym, dest, 0);
	    }
	}

      elf_section_data (finfo.dynsym_sec->output_section)->this_hdr.sh_info =
	last_local + 1;
    }

  /* We get the global symbols from the hash table.  */
  eoinfo.failed = FALSE;
  eoinfo.localsyms = FALSE;
  eoinfo.finfo = &finfo;
  elf_link_hash_traverse (elf_hash_table (info), elf_link_output_extsym,
			  &eoinfo);
  if (eoinfo.failed)
    return FALSE;

  /* If backend needs to output some symbols not present in the hash
     table, do it now.  */
  if (bed->elf_backend_output_arch_syms)
    {
      typedef bfd_boolean (*out_sym_func)
	(void *, const char *, Elf_Internal_Sym *, asection *,
	 struct elf_link_hash_entry *);

      if (! ((*bed->elf_backend_output_arch_syms)
	     (abfd, info, &finfo, (out_sym_func) elf_link_output_sym)))
	return FALSE;
    }

  /* Flush all symbols to the file.  */
  if (! elf_link_flush_output_syms (&finfo))
    return FALSE;

  /* Now we know the size of the symtab section.  */
  off += symtab_hdr->sh_size;

  symtab_shndx_hdr = &elf_tdata (abfd)->symtab_shndx_hdr;
  if (symtab_shndx_hdr->sh_name != 0)
    {
      symtab_shndx_hdr->sh_type = SHT_SYMTAB_SHNDX;
      symtab_shndx_hdr->sh_entsize = sizeof (Elf_External_Sym_Shndx);
      symtab_shndx_hdr->sh_addralign = sizeof (Elf_External_Sym_Shndx);
      amt = bfd_get_symcount (abfd) * sizeof (Elf_External_Sym_Shndx);
      symtab_shndx_hdr->sh_size = amt;

      off = _bfd_elf_assign_file_position_for_section (symtab_shndx_hdr,
						       off, TRUE);

      if (bfd_seek (abfd, symtab_shndx_hdr->sh_offset, SEEK_SET) != 0
	  || (bfd_bwrite (finfo.symshndxbuf, amt, abfd) != amt))
	return FALSE;
    }


  /* Finish up and write out the symbol string table (.strtab)
     section.  */
  symstrtab_hdr = &elf_tdata (abfd)->strtab_hdr;
  /* sh_name was set in prep_headers.  */
  symstrtab_hdr->sh_type = SHT_STRTAB;
  symstrtab_hdr->sh_flags = 0;
  symstrtab_hdr->sh_addr = 0;
  symstrtab_hdr->sh_size = _bfd_stringtab_size (finfo.symstrtab);
  symstrtab_hdr->sh_entsize = 0;
  symstrtab_hdr->sh_link = 0;
  symstrtab_hdr->sh_info = 0;
  /* sh_offset is set just below.  */
  symstrtab_hdr->sh_addralign = 1;

  off = _bfd_elf_assign_file_position_for_section (symstrtab_hdr, off, TRUE);
  elf_tdata (abfd)->next_file_pos = off;

  if (bfd_get_symcount (abfd) > 0)
    {
      if (bfd_seek (abfd, symstrtab_hdr->sh_offset, SEEK_SET) != 0
	  || ! _bfd_stringtab_emit (abfd, finfo.symstrtab))
	return FALSE;
    }

  /* Adjust the relocs to have the correct symbol indices.  */
  for (o = abfd->sections; o != NULL; o = o->next)
    {
      if ((o->flags & SEC_RELOC) == 0)
	continue;

      elf_link_adjust_relocs (abfd, &elf_section_data (o)->rel_hdr,
			      elf_section_data (o)->rel_count,
			      elf_section_data (o)->rel_hashes);
      if (elf_section_data (o)->rel_hdr2 != NULL)
	elf_link_adjust_relocs (abfd, elf_section_data (o)->rel_hdr2,
				elf_section_data (o)->rel_count2,
				(elf_section_data (o)->rel_hashes
				 + elf_section_data (o)->rel_count));

      /* Set the reloc_count field to 0 to prevent write_relocs from
	 trying to swap the relocs out itself.  */
      o->reloc_count = 0;
    }

  if (dynamic && info->combreloc && dynobj != NULL)
    relativecount = elf_link_sort_relocs (abfd, info, &reldyn);

  /* If we are linking against a dynamic object, or generating a
     shared library, finish up the dynamic linking information.  */
  if (dynamic)
    {
      Elf_External_Dyn *dyncon, *dynconend;

      /* Fix up .dynamic entries.  */
      o = bfd_get_section_by_name (dynobj, ".dynamic");
      BFD_ASSERT (o != NULL);

      dyncon = (Elf_External_Dyn *) o->contents;
      dynconend = (Elf_External_Dyn *) (o->contents + o->_raw_size);
      for (; dyncon < dynconend; dyncon++)
	{
	  Elf_Internal_Dyn dyn;
	  const char *name;
	  unsigned int type;

	  elf_swap_dyn_in (dynobj, dyncon, &dyn);

	  switch (dyn.d_tag)
	    {
	    default:
	      break;
	    case DT_NULL:
	      if (relativecount > 0 && dyncon + 1 < dynconend)
		{
		  switch (elf_section_data (reldyn)->this_hdr.sh_type)
		    {
		    case SHT_REL: dyn.d_tag = DT_RELCOUNT; break;
		    case SHT_RELA: dyn.d_tag = DT_RELACOUNT; break;
		    default: break;
		    }
		  if (dyn.d_tag != DT_NULL)
		    {
		      dyn.d_un.d_val = relativecount;
		      elf_swap_dyn_out (dynobj, &dyn, dyncon);
		      relativecount = 0;
		    }
		}
	      break;
	    case DT_INIT:
	      name = info->init_function;
	      goto get_sym;
	    case DT_FINI:
	      name = info->fini_function;
	    get_sym:
	      {
		struct elf_link_hash_entry *h;

		h = elf_link_hash_lookup (elf_hash_table (info), name,
					  FALSE, FALSE, TRUE);
		if (h != NULL
		    && (h->root.type == bfd_link_hash_defined
			|| h->root.type == bfd_link_hash_defweak))
		  {
		    dyn.d_un.d_val = h->root.u.def.value;
		    o = h->root.u.def.section;
		    if (o->output_section != NULL)
		      dyn.d_un.d_val += (o->output_section->vma
					 + o->output_offset);
		    else
		      {
			/* The symbol is imported from another shared
			   library and does not apply to this one.  */
			dyn.d_un.d_val = 0;
		      }

		    elf_swap_dyn_out (dynobj, &dyn, dyncon);
		  }
	      }
	      break;

	    case DT_PREINIT_ARRAYSZ:
	      name = ".preinit_array";
	      goto get_size;
	    case DT_INIT_ARRAYSZ:
	      name = ".init_array";
	      goto get_size;
	    case DT_FINI_ARRAYSZ:
	      name = ".fini_array";
	    get_size:
	      o = bfd_get_section_by_name (abfd, name);
	      if (o == NULL)
		{
		  (*_bfd_error_handler)
		    (_("%s: could not find output section %s"),
		     bfd_get_filename (abfd), name);
		  goto error_return;
		}
	      if (o->_raw_size == 0)
		(*_bfd_error_handler)
		  (_("warning: %s section has zero size"), name);
	      dyn.d_un.d_val = o->_raw_size;
	      elf_swap_dyn_out (dynobj, &dyn, dyncon);
	      break;

	    case DT_PREINIT_ARRAY:
	      name = ".preinit_array";
	      goto get_vma;
	    case DT_INIT_ARRAY:
	      name = ".init_array";
	      goto get_vma;
	    case DT_FINI_ARRAY:
	      name = ".fini_array";
	      goto get_vma;

	    case DT_HASH:
	      name = ".hash";
	      goto get_vma;
	    case DT_STRTAB:
	      name = ".dynstr";
	      goto get_vma;
	    case DT_SYMTAB:
	      name = ".dynsym";
	      goto get_vma;
	    case DT_VERDEF:
	      name = ".gnu.version_d";
	      goto get_vma;
	    case DT_VERNEED:
	      name = ".gnu.version_r";
	      goto get_vma;
	    case DT_VERSYM:
	      name = ".gnu.version";
	    get_vma:
	      o = bfd_get_section_by_name (abfd, name);
	      if (o == NULL)
		{
		  (*_bfd_error_handler)
		    (_("%s: could not find output section %s"),
		     bfd_get_filename (abfd), name);
		  goto error_return;
		}
	      dyn.d_un.d_ptr = o->vma;
	      elf_swap_dyn_out (dynobj, &dyn, dyncon);
	      break;

	    case DT_REL:
	    case DT_RELA:
	    case DT_RELSZ:
	    case DT_RELASZ:
	      if (dyn.d_tag == DT_REL || dyn.d_tag == DT_RELSZ)
		type = SHT_REL;
	      else
		type = SHT_RELA;
	      dyn.d_un.d_val = 0;
	      for (i = 1; i < elf_numsections (abfd); i++)
		{
		  Elf_Internal_Shdr *hdr;

		  hdr = elf_elfsections (abfd)[i];
		  if (hdr->sh_type == type
		      && (hdr->sh_flags & SHF_ALLOC) != 0)
		    {
		      if (dyn.d_tag == DT_RELSZ || dyn.d_tag == DT_RELASZ)
			dyn.d_un.d_val += hdr->sh_size;
		      else
			{
			  if (dyn.d_un.d_val == 0
			      || hdr->sh_addr < dyn.d_un.d_val)
			    dyn.d_un.d_val = hdr->sh_addr;
			}
		    }
		}
	      elf_swap_dyn_out (dynobj, &dyn, dyncon);
	      break;
	    }
	}
    }

  /* If we have created any dynamic sections, then output them.  */
  if (dynobj != NULL)
    {
      if (! (*bed->elf_backend_finish_dynamic_sections) (abfd, info))
	goto error_return;

      for (o = dynobj->sections; o != NULL; o = o->next)
	{
	  if ((o->flags & SEC_HAS_CONTENTS) == 0
	      || o->_raw_size == 0
	      || o->output_section == bfd_abs_section_ptr)
	    continue;
	  if ((o->flags & SEC_LINKER_CREATED) == 0)
	    {
	      /* At this point, we are only interested in sections
		 created by _bfd_elf_link_create_dynamic_sections.  */
	      continue;
	    }
	  if ((elf_section_data (o->output_section)->this_hdr.sh_type
	       != SHT_STRTAB)
	      || strcmp (bfd_get_section_name (abfd, o), ".dynstr") != 0)
	    {
	      if (! bfd_set_section_contents (abfd, o->output_section,
					      o->contents,
					      (file_ptr) o->output_offset,
					      o->_raw_size))
		goto error_return;
	    }
	  else
	    {
	      /* The contents of the .dynstr section are actually in a
		 stringtab.  */
	      off = elf_section_data (o->output_section)->this_hdr.sh_offset;
	      if (bfd_seek (abfd, off, SEEK_SET) != 0
		  || ! _bfd_elf_strtab_emit (abfd,
					     elf_hash_table (info)->dynstr))
		goto error_return;
	    }
	}
    }

  if (info->relocatable)
    {
      bfd_boolean failed = FALSE;

      bfd_map_over_sections (abfd, bfd_elf_set_group_contents, &failed);
      if (failed)
	goto error_return;
    }

  /* If we have optimized stabs strings, output them.  */
  if (elf_hash_table (info)->stab_info != NULL)
    {
      if (! _bfd_write_stab_strings (abfd, &elf_hash_table (info)->stab_info))
	goto error_return;
    }

  if (info->eh_frame_hdr)
    {
      if (! _bfd_elf_write_section_eh_frame_hdr (abfd, info))
	goto error_return;
    }

  if (finfo.symstrtab != NULL)
    _bfd_stringtab_free (finfo.symstrtab);
  if (finfo.contents != NULL)
    free (finfo.contents);
  if (finfo.external_relocs != NULL)
    free (finfo.external_relocs);
  if (finfo.internal_relocs != NULL)
    free (finfo.internal_relocs);
  if (finfo.external_syms != NULL)
    free (finfo.external_syms);
  if (finfo.locsym_shndx != NULL)
    free (finfo.locsym_shndx);
  if (finfo.internal_syms != NULL)
    free (finfo.internal_syms);
  if (finfo.indices != NULL)
    free (finfo.indices);
  if (finfo.sections != NULL)
    free (finfo.sections);
  if (finfo.symbuf != NULL)
    free (finfo.symbuf);
  if (finfo.symshndxbuf != NULL)
    free (finfo.symshndxbuf);
  for (o = abfd->sections; o != NULL; o = o->next)
    {
      if ((o->flags & SEC_RELOC) != 0
	  && elf_section_data (o)->rel_hashes != NULL)
	free (elf_section_data (o)->rel_hashes);
    }

  elf_tdata (abfd)->linker = TRUE;

  return TRUE;

 error_return:
  if (finfo.symstrtab != NULL)
    _bfd_stringtab_free (finfo.symstrtab);
  if (finfo.contents != NULL)
    free (finfo.contents);
  if (finfo.external_relocs != NULL)
    free (finfo.external_relocs);
  if (finfo.internal_relocs != NULL)
    free (finfo.internal_relocs);
  if (finfo.external_syms != NULL)
    free (finfo.external_syms);
  if (finfo.locsym_shndx != NULL)
    free (finfo.locsym_shndx);
  if (finfo.internal_syms != NULL)
    free (finfo.internal_syms);
  if (finfo.indices != NULL)
    free (finfo.indices);
  if (finfo.sections != NULL)
    free (finfo.sections);
  if (finfo.symbuf != NULL)
    free (finfo.symbuf);
  if (finfo.symshndxbuf != NULL)
    free (finfo.symshndxbuf);
  for (o = abfd->sections; o != NULL; o = o->next)
    {
      if ((o->flags & SEC_RELOC) != 0
	  && elf_section_data (o)->rel_hashes != NULL)
	free (elf_section_data (o)->rel_hashes);
    }

  return FALSE;
}

/* Add a symbol to the output symbol table.  */

static bfd_boolean
elf_link_output_sym (struct elf_final_link_info *finfo,
		     const char *name,
		     Elf_Internal_Sym *elfsym,
		     asection *input_sec,
		     struct elf_link_hash_entry *h)
{
  Elf_External_Sym *dest;
  Elf_External_Sym_Shndx *destshndx;
  bfd_boolean (*output_symbol_hook)
    (struct bfd_link_info *, const char *, Elf_Internal_Sym *, asection *,
     struct elf_link_hash_entry *);

  output_symbol_hook = get_elf_backend_data (finfo->output_bfd)->
    elf_backend_link_output_symbol_hook;
  if (output_symbol_hook != NULL)
    {
      if (! (*output_symbol_hook) (finfo->info, name, elfsym, input_sec, h))
	return FALSE;
    }

  if (name == NULL || *name == '\0')
    elfsym->st_name = 0;
  else if (input_sec->flags & SEC_EXCLUDE)
    elfsym->st_name = 0;
  else
    {
      elfsym->st_name = (unsigned long) _bfd_stringtab_add (finfo->symstrtab,
							    name, TRUE, FALSE);
      if (elfsym->st_name == (unsigned long) -1)
	return FALSE;
    }

  if (finfo->symbuf_count >= finfo->symbuf_size)
    {
      if (! elf_link_flush_output_syms (finfo))
	return FALSE;
    }

  dest = finfo->symbuf + finfo->symbuf_count;
  destshndx = finfo->symshndxbuf;
  if (destshndx != NULL)
    {
      if (bfd_get_symcount (finfo->output_bfd) >= finfo->shndxbuf_size)
	{
	  bfd_size_type amt;

	  amt = finfo->shndxbuf_size * sizeof (Elf_External_Sym_Shndx);
	  finfo->symshndxbuf = destshndx = bfd_realloc (destshndx, amt * 2);
	  if (destshndx == NULL)
	    return FALSE;
	  memset ((char *) destshndx + amt, 0, amt);
	  finfo->shndxbuf_size *= 2;
	}
      destshndx += bfd_get_symcount (finfo->output_bfd);
    }

  elf_swap_symbol_out (finfo->output_bfd, elfsym, dest, destshndx);
  finfo->symbuf_count += 1;
  bfd_get_symcount (finfo->output_bfd) += 1;

  return TRUE;
}

/* Flush the output symbols to the file.  */

static bfd_boolean
elf_link_flush_output_syms (struct elf_final_link_info *finfo)
{
  if (finfo->symbuf_count > 0)
    {
      Elf_Internal_Shdr *hdr;
      file_ptr pos;
      bfd_size_type amt;

      hdr = &elf_tdata (finfo->output_bfd)->symtab_hdr;
      pos = hdr->sh_offset + hdr->sh_size;
      amt = finfo->symbuf_count * sizeof (Elf_External_Sym);
      if (bfd_seek (finfo->output_bfd, pos, SEEK_SET) != 0
	  || bfd_bwrite (finfo->symbuf, amt, finfo->output_bfd) != amt)
	return FALSE;

      hdr->sh_size += amt;
      finfo->symbuf_count = 0;
    }

  return TRUE;
}

/* For DSOs loaded in via a DT_NEEDED entry, emulate ld.so in
   allowing an unsatisfied unversioned symbol in the DSO to match a
   versioned symbol that would normally require an explicit version.
   We also handle the case that a DSO references a hidden symbol
   which may be satisfied by a versioned symbol in another DSO.  */

static bfd_boolean
elf_link_check_versioned_symbol (struct bfd_link_info *info,
				 struct elf_link_hash_entry *h)
{
  bfd *abfd;
  struct elf_link_loaded_list *loaded;

  if (!is_elf_hash_table (info->hash))
    return FALSE;

  switch (h->root.type)
    {
    default:
      abfd = NULL;
      break;

    case bfd_link_hash_undefined:
    case bfd_link_hash_undefweak:
      abfd = h->root.u.undef.abfd;
      if ((abfd->flags & DYNAMIC) == 0 || elf_dt_soname (abfd) == NULL)
	return FALSE;
      break;

    case bfd_link_hash_defined:
    case bfd_link_hash_defweak:
      abfd = h->root.u.def.section->owner;
      break;

    case bfd_link_hash_common:
      abfd = h->root.u.c.p->section->owner;
      break;
    }
  BFD_ASSERT (abfd != NULL);

  for (loaded = elf_hash_table (info)->loaded;
       loaded != NULL;
       loaded = loaded->next)
    {
      bfd *input;
      Elf_Internal_Shdr *hdr;
      bfd_size_type symcount;
      bfd_size_type extsymcount;
      bfd_size_type extsymoff;
      Elf_Internal_Shdr *versymhdr;
      Elf_Internal_Sym *isym;
      Elf_Internal_Sym *isymend;
      Elf_Internal_Sym *isymbuf;
      Elf_External_Versym *ever;
      Elf_External_Versym *extversym;

      input = loaded->abfd;

      /* We check each DSO for a possible hidden versioned definition.  */
      if (input == abfd
	  || (input->flags & DYNAMIC) == 0
	  || elf_dynversym (input) == 0)
	continue;

      hdr = &elf_tdata (input)->dynsymtab_hdr;

      symcount = hdr->sh_size / sizeof (Elf_External_Sym);
      if (elf_bad_symtab (input))
	{
	  extsymcount = symcount;
	  extsymoff = 0;
	}
      else
	{
	  extsymcount = symcount - hdr->sh_info;
	  extsymoff = hdr->sh_info;
	}

      if (extsymcount == 0)
	continue;

      isymbuf = bfd_elf_get_elf_syms (input, hdr, extsymcount, extsymoff,
				      NULL, NULL, NULL);
      if (isymbuf == NULL)
	return FALSE;

      /* Read in any version definitions.  */
      versymhdr = &elf_tdata (input)->dynversym_hdr;
      extversym = bfd_malloc (versymhdr->sh_size);
      if (extversym == NULL)
	goto error_ret;

      if (bfd_seek (input, versymhdr->sh_offset, SEEK_SET) != 0
	  || (bfd_bread (extversym, versymhdr->sh_size, input)
	      != versymhdr->sh_size))
	{
	  free (extversym);
	error_ret:
	  free (isymbuf);
	  return FALSE;
	}

      ever = extversym + extsymoff;
      isymend = isymbuf + extsymcount;
      for (isym = isymbuf; isym < isymend; isym++, ever++)
	{
	  const char *name;
	  Elf_Internal_Versym iver;
	  unsigned short version_index;

	  if (ELF_ST_BIND (isym->st_info) == STB_LOCAL
	      || isym->st_shndx == SHN_UNDEF)
	    continue;

	  name = bfd_elf_string_from_elf_section (input,
						  hdr->sh_link,
						  isym->st_name);
	  if (strcmp (name, h->root.root.string) != 0)
	    continue;

	  _bfd_elf_swap_versym_in (input, ever, &iver);

	  if ((iver.vs_vers & VERSYM_HIDDEN) == 0)
	    {
	      /* If we have a non-hidden versioned sym, then it should
		 have provided a definition for the undefined sym.  */
	      abort ();
	    }

	  version_index = iver.vs_vers & VERSYM_VERSION;
	  if (version_index == 1 || version_index == 2)
	    {
	      /* This is the base or first version.  We can use it.  */
	      free (extversym);
	      free (isymbuf);
	      return TRUE;
	    }
	}

      free (extversym);
      free (isymbuf);
    }

  return FALSE;
}

/* Add an external symbol to the symbol table.  This is called from
   the hash table traversal routine.  When generating a shared object,
   we go through the symbol table twice.  The first time we output
   anything that might have been forced to local scope in a version
   script.  The second time we output the symbols that are still
   global symbols.  */

static bfd_boolean
elf_link_output_extsym (struct elf_link_hash_entry *h, void *data)
{
  struct elf_outext_info *eoinfo = data;
  struct elf_final_link_info *finfo = eoinfo->finfo;
  bfd_boolean strip;
  Elf_Internal_Sym sym;
  asection *input_sec;

  if (h->root.type == bfd_link_hash_warning)
    {
      h = (struct elf_link_hash_entry *) h->root.u.i.link;
      if (h->root.type == bfd_link_hash_new)
	return TRUE;
    }

  /* Decide whether to output this symbol in this pass.  */
  if (eoinfo->localsyms)
    {
      if ((h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) == 0)
	return TRUE;
    }
  else
    {
      if ((h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) != 0)
	return TRUE;
    }

  /* If we have an undefined symbol reference here then it must have
     come from a shared library that is being linked in.  (Undefined
     references in regular files have already been handled).  If we
     are reporting errors for this situation then do so now.  */
  if (h->root.type == bfd_link_hash_undefined
      && (h->elf_link_hash_flags & ELF_LINK_HASH_REF_DYNAMIC) != 0
      && (h->elf_link_hash_flags & ELF_LINK_HASH_REF_REGULAR) == 0
      && ! elf_link_check_versioned_symbol (finfo->info, h)
      && finfo->info->unresolved_syms_in_shared_libs != RM_IGNORE)
    {
      if (! ((*finfo->info->callbacks->undefined_symbol)
	     (finfo->info, h->root.root.string, h->root.u.undef.abfd,
	      NULL, 0, finfo->info->unresolved_syms_in_shared_libs == RM_GENERATE_ERROR)))
	{
	  eoinfo->failed = TRUE;
	  return FALSE;
	}
    }

  /* We should also warn if a forced local symbol is referenced from
     shared libraries.  */
  if (! finfo->info->relocatable
      && (! finfo->info->shared)
      && (h->elf_link_hash_flags
	  & (ELF_LINK_FORCED_LOCAL | ELF_LINK_HASH_REF_DYNAMIC | ELF_LINK_DYNAMIC_DEF | ELF_LINK_DYNAMIC_WEAK))
	 == (ELF_LINK_FORCED_LOCAL | ELF_LINK_HASH_REF_DYNAMIC)
      && ! elf_link_check_versioned_symbol (finfo->info, h))
    {
      (*_bfd_error_handler)
	(_("%s: %s symbol `%s' in %s is referenced by DSO"),
	 bfd_get_filename (finfo->output_bfd),
	 ELF_ST_VISIBILITY (h->other) == STV_INTERNAL
	 ? "internal"
	 : ELF_ST_VISIBILITY (h->other) == STV_HIDDEN
	   ? "hidden" : "local",
	 h->root.root.string,
	 bfd_archive_filename (h->root.u.def.section->owner));
      eoinfo->failed = TRUE;
      return FALSE;
    }

  /* We don't want to output symbols that have never been mentioned by
     a regular file, or that we have been told to strip.  However, if
     h->indx is set to -2, the symbol is used by a reloc and we must
     output it.  */
  if (h->indx == -2)
    strip = FALSE;
  else if (((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) != 0
	    || (h->elf_link_hash_flags & ELF_LINK_HASH_REF_DYNAMIC) != 0)
	   && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0
	   && (h->elf_link_hash_flags & ELF_LINK_HASH_REF_REGULAR) == 0)
    strip = TRUE;
  else if (finfo->info->strip == strip_all)
    strip = TRUE;
  else if (finfo->info->strip == strip_some
	   && bfd_hash_lookup (finfo->info->keep_hash,
			       h->root.root.string, FALSE, FALSE) == NULL)
    strip = TRUE;
  else if (finfo->info->strip_discarded
	   && (h->root.type == bfd_link_hash_defined
	       || h->root.type == bfd_link_hash_defweak)
	   && elf_discarded_section (h->root.u.def.section))
    strip = TRUE;
  else
    strip = FALSE;

  /* If we're stripping it, and it's not a dynamic symbol, there's
     nothing else to do unless it is a forced local symbol.  */
  if (strip
      && h->dynindx == -1
      && (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) == 0)
    return TRUE;

  sym.st_value = 0;
  sym.st_size = h->size;
  sym.st_other = h->other;
  if ((h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) != 0)
    sym.st_info = ELF_ST_INFO (STB_LOCAL, h->type);
  else if (h->root.type == bfd_link_hash_undefweak
	   || h->root.type == bfd_link_hash_defweak)
    sym.st_info = ELF_ST_INFO (STB_WEAK, h->type);
  else
    sym.st_info = ELF_ST_INFO (STB_GLOBAL, h->type);

  switch (h->root.type)
    {
    default:
    case bfd_link_hash_new:
    case bfd_link_hash_warning:
      abort ();
      return FALSE;

    case bfd_link_hash_undefined:
    case bfd_link_hash_undefweak:
      input_sec = bfd_und_section_ptr;
      sym.st_shndx = SHN_UNDEF;
      break;

    case bfd_link_hash_defined:
    case bfd_link_hash_defweak:
      {
	input_sec = h->root.u.def.section;
	if (input_sec->output_section != NULL)
	  {
	    sym.st_shndx =
	      _bfd_elf_section_from_bfd_section (finfo->output_bfd,
						 input_sec->output_section);
	    if (sym.st_shndx == SHN_BAD)
	      {
		(*_bfd_error_handler)
		  (_("%s: could not find output section %s for input section %s"),
		   bfd_get_filename (finfo->output_bfd),
		   input_sec->output_section->name,
		   input_sec->name);
		eoinfo->failed = TRUE;
		return FALSE;
	      }

	    /* ELF symbols in relocatable files are section relative,
	       but in nonrelocatable files they are virtual
	       addresses.  */
	    sym.st_value = h->root.u.def.value + input_sec->output_offset;
	    if (! finfo->info->relocatable)
	      {
		sym.st_value += input_sec->output_section->vma;
		if (h->type == STT_TLS)
		  {
		    /* STT_TLS symbols are relative to PT_TLS segment
		       base.  */
		    BFD_ASSERT (elf_hash_table (finfo->info)->tls_sec != NULL);
		    sym.st_value -= elf_hash_table (finfo->info)->tls_sec->vma;
		  }
	      }
	  }
	else
	  {
	    BFD_ASSERT (input_sec->owner == NULL
			|| (input_sec->owner->flags & DYNAMIC) != 0);
	    sym.st_shndx = SHN_UNDEF;
	    input_sec = bfd_und_section_ptr;
	  }
      }
      break;

    case bfd_link_hash_common:
      input_sec = h->root.u.c.p->section;
      sym.st_shndx = SHN_COMMON;
      sym.st_value = 1 << h->root.u.c.p->alignment_power;
      break;

    case bfd_link_hash_indirect:
      /* These symbols are created by symbol versioning.  They point
	 to the decorated version of the name.  For example, if the
	 symbol foo@@GNU_1.2 is the default, which should be used when
	 foo is used with no version, then we add an indirect symbol
	 foo which points to foo@@GNU_1.2.  We ignore these symbols,
	 since the indirected symbol is already in the hash table.  */
      return TRUE;
    }

  /* Give the processor backend a chance to tweak the symbol value,
     and also to finish up anything that needs to be done for this
     symbol.  FIXME: Not calling elf_backend_finish_dynamic_symbol for
     forced local syms when non-shared is due to a historical quirk.  */
  if ((h->dynindx != -1
       || (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) != 0)
      && ((finfo->info->shared
	   && (ELF_ST_VISIBILITY (h->other) == STV_DEFAULT
	       || h->root.type != bfd_link_hash_undefweak))
	  || (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) == 0)
      && elf_hash_table (finfo->info)->dynamic_sections_created)
    {
      const struct elf_backend_data *bed;

      bed = get_elf_backend_data (finfo->output_bfd);
      if (! ((*bed->elf_backend_finish_dynamic_symbol)
	     (finfo->output_bfd, finfo->info, h, &sym)))
	{
	  eoinfo->failed = TRUE;
	  return FALSE;
	}
    }

  /* If we are marking the symbol as undefined, and there are no
     non-weak references to this symbol from a regular object, then
     mark the symbol as weak undefined; if there are non-weak
     references, mark the symbol as strong.  We can't do this earlier,
     because it might not be marked as undefined until the
     finish_dynamic_symbol routine gets through with it.  */
  if (sym.st_shndx == SHN_UNDEF
      && (h->elf_link_hash_flags & ELF_LINK_HASH_REF_REGULAR) != 0
      && (ELF_ST_BIND (sym.st_info) == STB_GLOBAL
	  || ELF_ST_BIND (sym.st_info) == STB_WEAK))
    {
      int bindtype;

      if ((h->elf_link_hash_flags & ELF_LINK_HASH_REF_REGULAR_NONWEAK) != 0)
	bindtype = STB_GLOBAL;
      else
	bindtype = STB_WEAK;
      sym.st_info = ELF_ST_INFO (bindtype, ELF_ST_TYPE (sym.st_info));
    }

  /* If a non-weak symbol with non-default visibility is not defined
     locally, it is a fatal error.  */
  if (! finfo->info->relocatable
      && ELF_ST_VISIBILITY (sym.st_other) != STV_DEFAULT
      && ELF_ST_BIND (sym.st_info) != STB_WEAK
      && h->root.type == bfd_link_hash_undefined
      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
    {
      (*_bfd_error_handler)
	(_("%s: %s symbol `%s' isn't defined"),
	  bfd_get_filename (finfo->output_bfd),
	  ELF_ST_VISIBILITY (sym.st_other) == STV_PROTECTED
	  ? "protected"
	  : ELF_ST_VISIBILITY (sym.st_other) == STV_INTERNAL
	    ? "internal" : "hidden",
	  h->root.root.string);
      eoinfo->failed = TRUE;
      return FALSE;
    }

  /* If this symbol should be put in the .dynsym section, then put it
     there now.  We already know the symbol index.  We also fill in
     the entry in the .hash section.  */
  if (h->dynindx != -1
      && elf_hash_table (finfo->info)->dynamic_sections_created)
    {
      size_t bucketcount;
      size_t bucket;
      size_t hash_entry_size;
      bfd_byte *bucketpos;
      bfd_vma chain;
      Elf_External_Sym *esym;

      sym.st_name = h->dynstr_index;
      esym = (Elf_External_Sym *) finfo->dynsym_sec->contents + h->dynindx;
      elf_swap_symbol_out (finfo->output_bfd, &sym, esym, 0);

      bucketcount = elf_hash_table (finfo->info)->bucketcount;
      bucket = h->elf_hash_value % bucketcount;
      hash_entry_size
	= elf_section_data (finfo->hash_sec)->this_hdr.sh_entsize;
      bucketpos = ((bfd_byte *) finfo->hash_sec->contents
		   + (bucket + 2) * hash_entry_size);
      chain = bfd_get (8 * hash_entry_size, finfo->output_bfd, bucketpos);
      bfd_put (8 * hash_entry_size, finfo->output_bfd, h->dynindx, bucketpos);
      bfd_put (8 * hash_entry_size, finfo->output_bfd, chain,
	       ((bfd_byte *) finfo->hash_sec->contents
		+ (bucketcount + 2 + h->dynindx) * hash_entry_size));

      if (finfo->symver_sec != NULL && finfo->symver_sec->contents != NULL)
	{
	  Elf_Internal_Versym iversym;
	  Elf_External_Versym *eversym;

	  if ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
	    {
	      if (h->verinfo.verdef == NULL)
		iversym.vs_vers = 0;
	      else
		iversym.vs_vers = h->verinfo.verdef->vd_exp_refno + 1;
	    }
	  else
	    {
	      if (h->verinfo.vertree == NULL)
		iversym.vs_vers = 1;
	      else
		iversym.vs_vers = h->verinfo.vertree->vernum + 1;
	    }

	  if ((h->elf_link_hash_flags & ELF_LINK_HIDDEN) != 0)
	    iversym.vs_vers |= VERSYM_HIDDEN;

	  eversym = (Elf_External_Versym *) finfo->symver_sec->contents;
	  eversym += h->dynindx;
	  _bfd_elf_swap_versym_out (finfo->output_bfd, &iversym, eversym);
	}
    }

  /* If we're stripping it, then it was just a dynamic symbol, and
     there's nothing else to do.  */
  if (strip || (input_sec->flags & SEC_EXCLUDE) != 0)
    return TRUE;

  h->indx = bfd_get_symcount (finfo->output_bfd);

  if (! elf_link_output_sym (finfo, h->root.root.string, &sym, input_sec, h))
    {
      eoinfo->failed = TRUE;
      return FALSE;
    }

  return TRUE;
}

/* Link an input file into the linker output file.  This function
   handles all the sections and relocations of the input file at once.
   This is so that we only have to read the local symbols once, and
   don't have to keep them in memory.  */

static bfd_boolean
elf_link_input_bfd (struct elf_final_link_info *finfo, bfd *input_bfd)
{
  bfd_boolean (*relocate_section)
    (bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
     Elf_Internal_Rela *, Elf_Internal_Sym *, asection **);
  bfd *output_bfd;
  Elf_Internal_Shdr *symtab_hdr;
  size_t locsymcount;
  size_t extsymoff;
  Elf_Internal_Sym *isymbuf;
  Elf_Internal_Sym *isym;
  Elf_Internal_Sym *isymend;
  long *pindex;
  asection **ppsection;
  asection *o;
  const struct elf_backend_data *bed;
  bfd_boolean emit_relocs;
  struct elf_link_hash_entry **sym_hashes;

  output_bfd = finfo->output_bfd;
  bed = get_elf_backend_data (output_bfd);
  relocate_section = bed->elf_backend_relocate_section;

  /* If this is a dynamic object, we don't want to do anything here:
     we don't want the local symbols, and we don't want the section
     contents.  */
  if ((input_bfd->flags & DYNAMIC) != 0)
    return TRUE;

  emit_relocs = (finfo->info->relocatable
		 || finfo->info->emitrelocations
		 || bed->elf_backend_emit_relocs);

  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  if (elf_bad_symtab (input_bfd))
    {
      locsymcount = symtab_hdr->sh_size / sizeof (Elf_External_Sym);
      extsymoff = 0;
    }
  else
    {
      locsymcount = symtab_hdr->sh_info;
      extsymoff = symtab_hdr->sh_info;
    }

  /* Read the local symbols.  */
  isymbuf = (Elf_Internal_Sym *) symtab_hdr->contents;
  if (isymbuf == NULL && locsymcount != 0)
    {
      isymbuf = bfd_elf_get_elf_syms (input_bfd, symtab_hdr, locsymcount, 0,
				      finfo->internal_syms,
				      finfo->external_syms,
				      finfo->locsym_shndx);
      if (isymbuf == NULL)
	return FALSE;
    }

  /* Find local symbol sections and adjust values of symbols in
     SEC_MERGE sections.  Write out those local symbols we know are
     going into the output file.  */
  isymend = isymbuf + locsymcount;
  for (isym = isymbuf, pindex = finfo->indices, ppsection = finfo->sections;
       isym < isymend;
       isym++, pindex++, ppsection++)
    {
      asection *isec;
      const char *name;
      Elf_Internal_Sym osym;

      *pindex = -1;

      if (elf_bad_symtab (input_bfd))
	{
	  if (ELF_ST_BIND (isym->st_info) != STB_LOCAL)
	    {
	      *ppsection = NULL;
	      continue;
	    }
	}

      if (isym->st_shndx == SHN_UNDEF)
	isec = bfd_und_section_ptr;
      else if (isym->st_shndx < SHN_LORESERVE
	       || isym->st_shndx > SHN_HIRESERVE)
	{
	  isec = section_from_elf_index (input_bfd, isym->st_shndx);
	  if (isec
	      && isec->sec_info_type == ELF_INFO_TYPE_MERGE
	      && ELF_ST_TYPE (isym->st_info) != STT_SECTION)
	    isym->st_value =
	      _bfd_merged_section_offset (output_bfd, &isec,
					  elf_section_data (isec)->sec_info,
					  isym->st_value, 0);
	}
      else if (isym->st_shndx == SHN_ABS)
	isec = bfd_abs_section_ptr;
      else if (isym->st_shndx == SHN_COMMON)
	isec = bfd_com_section_ptr;
      else
	{
	  /* Who knows?  */
	  isec = NULL;
	}

      *ppsection = isec;

      /* Don't output the first, undefined, symbol.  */
      if (ppsection == finfo->sections)
	continue;

      if (ELF_ST_TYPE (isym->st_info) == STT_SECTION)
	{
	  /* We never output section symbols.  Instead, we use the
	     section symbol of the corresponding section in the output
	     file.  */
	  continue;
	}

      /* If we are stripping all symbols, we don't want to output this
	 one.  */
      if (finfo->info->strip == strip_all)
	continue;

      /* If we are discarding all local symbols, we don't want to
	 output this one.  If we are generating a relocatable output
	 file, then some of the local symbols may be required by
	 relocs; we output them below as we discover that they are
	 needed.  */
      if (finfo->info->discard == discard_all)
	continue;

      /* If this symbol is defined in a section which we are
	 discarding, we don't need to keep it, but note that
	 linker_mark is only reliable for sections that have contents.
	 For the benefit of the MIPS ELF linker, we check SEC_EXCLUDE
	 as well as linker_mark.  */
      if ((isym->st_shndx < SHN_LORESERVE || isym->st_shndx > SHN_HIRESERVE)
	  && isec != NULL
	  && ((! isec->linker_mark && (isec->flags & SEC_HAS_CONTENTS) != 0)
	      || (! finfo->info->relocatable
		  && (isec->flags & SEC_EXCLUDE) != 0)))
	continue;

      /* Get the name of the symbol.  */
      name = bfd_elf_string_from_elf_section (input_bfd, symtab_hdr->sh_link,
					      isym->st_name);
      if (name == NULL)
	return FALSE;

      /* See if we are discarding symbols with this name.  */
      if ((finfo->info->strip == strip_some
	   && (bfd_hash_lookup (finfo->info->keep_hash, name, FALSE, FALSE)
	       == NULL))
	  || (((finfo->info->discard == discard_sec_merge
		&& (isec->flags & SEC_MERGE) && ! finfo->info->relocatable)
	       || finfo->info->discard == discard_l)
	      && bfd_is_local_label_name (input_bfd, name)))
	continue;

      /* If we get here, we are going to output this symbol.  */

      osym = *isym;

      /* Adjust the section index for the output file.  */
      osym.st_shndx = _bfd_elf_section_from_bfd_section (output_bfd,
							 isec->output_section);
      if (osym.st_shndx == SHN_BAD)
	return FALSE;

      *pindex = bfd_get_symcount (output_bfd);

      /* ELF symbols in relocatable files are section relative, but
	 in executable files they are virtual addresses.  Note that
	 this code assumes that all ELF sections have an associated
	 BFD section with a reasonable value for output_offset; below
	 we assume that they also have a reasonable value for
	 output_section.  Any special sections must be set up to meet
	 these requirements.  */
      osym.st_value += isec->output_offset;
      if (! finfo->info->relocatable)
	{
	  osym.st_value += isec->output_section->vma;
	  if (ELF_ST_TYPE (osym.st_info) == STT_TLS)
	    {
	      /* STT_TLS symbols are relative to PT_TLS segment base.  */
	      BFD_ASSERT (elf_hash_table (finfo->info)->tls_sec != NULL);
	      osym.st_value -= elf_hash_table (finfo->info)->tls_sec->vma;
	    }
	}

      if (! elf_link_output_sym (finfo, name, &osym, isec, NULL))
	return FALSE;
    }

  /* Relocate the contents of each section.  */
  sym_hashes = elf_sym_hashes (input_bfd);
  for (o = input_bfd->sections; o != NULL; o = o->next)
    {
      bfd_byte *contents;

      if (! o->linker_mark)
	{
	  /* This section was omitted from the link.  */
	  continue;
	}

      if ((o->flags & SEC_HAS_CONTENTS) == 0
	  || (o->_raw_size == 0 && (o->flags & SEC_RELOC) == 0))
	continue;

      if ((o->flags & SEC_LINKER_CREATED) != 0)
	{
	  /* Section was created by _bfd_elf_link_create_dynamic_sections
	     or somesuch.  */
	  continue;
	}

      /* Get the contents of the section.  They have been cached by a
	 relaxation routine.  Note that o is a section in an input
	 file, so the contents field will not have been set by any of
	 the routines which work on output files.  */
      if (elf_section_data (o)->this_hdr.contents != NULL)
	contents = elf_section_data (o)->this_hdr.contents;
      else
	{
	  contents = finfo->contents;
	  if (! bfd_get_section_contents (input_bfd, o, contents, 0,
					  o->_raw_size))
	    return FALSE;
	}

      if ((o->flags & SEC_RELOC) != 0)
	{
	  Elf_Internal_Rela *internal_relocs;

	  /* Get the swapped relocs.  */
	  internal_relocs
	    = _bfd_elf_link_read_relocs (input_bfd, o, finfo->external_relocs,
					 finfo->internal_relocs, FALSE);
	  if (internal_relocs == NULL
	      && o->reloc_count > 0)
	    return FALSE;

	  /* Run through the relocs looking for any against symbols
	     from discarded sections and section symbols from
	     removed link-once sections.  Complain about relocs
	     against discarded sections.  Zero relocs against removed
	     link-once sections.  Preserve debug information as much
	     as we can.  */
	  if (!elf_section_ignore_discarded_relocs (o))
	    {
	      Elf_Internal_Rela *rel, *relend;

	      rel = internal_relocs;
	      relend = rel + o->reloc_count * bed->s->int_rels_per_ext_rel;
	      for ( ; rel < relend; rel++)
		{
		  unsigned long r_symndx = ELF_R_SYM (rel->r_info);
		  asection *sec;

		  if (r_symndx >= locsymcount
		      || (elf_bad_symtab (input_bfd)
			  && finfo->sections[r_symndx] == NULL))
		    {
		      struct elf_link_hash_entry *h;

		      h = sym_hashes[r_symndx - extsymoff];
		      while (h->root.type == bfd_link_hash_indirect
			     || h->root.type == bfd_link_hash_warning)
			h = (struct elf_link_hash_entry *) h->root.u.i.link;

		      /* Complain if the definition comes from a
			 discarded section.  */
		      sec = h->root.u.def.section;
		      if ((h->root.type == bfd_link_hash_defined
			   || h->root.type == bfd_link_hash_defweak)
			  && elf_discarded_section (sec))
			{
			  if ((o->flags & SEC_DEBUGGING) != 0)
			    {
			      BFD_ASSERT (r_symndx != 0);
			      /* Try to preserve debug information.  */
			      if ((o->flags & SEC_DEBUGGING) != 0
				  && sec->kept_section != NULL
				  && sec->_raw_size == sec->kept_section->_raw_size)
				h->root.u.def.section
				  = sec->kept_section;
			      else
				memset (rel, 0, sizeof (*rel));
			    }
			  else
			    finfo->info->callbacks->error_handler
			      (LD_DEFINITION_IN_DISCARDED_SECTION,
			       _("%T: discarded in section `%s' from %s\n"),
			       h->root.root.string,
			       h->root.root.string,
			       h->root.u.def.section->name,
			       bfd_archive_filename (h->root.u.def.section->owner));
			}
		    }
		  else
		    {
		      sec = finfo->sections[r_symndx];

		      if (sec != NULL && elf_discarded_section (sec))
			{
			  if ((o->flags & SEC_DEBUGGING) != 0
			      || (sec->flags & SEC_LINK_ONCE) != 0)
			    {
			      BFD_ASSERT (r_symndx != 0);
			      /* Try to preserve debug information.  */
			      if ((o->flags & SEC_DEBUGGING) != 0
				  && sec->kept_section != NULL
				  && sec->_raw_size == sec->kept_section->_raw_size)
				finfo->sections[r_symndx]
				  = sec->kept_section;
			      else
				{
				  rel->r_info
				    = ELF_R_INFO (0, ELF_R_TYPE (rel->r_info));
				  rel->r_addend = 0;
				}
			    }
			  else
			    {
			      static int count;
			      int ok;
			      char *buf;

			      ok = asprintf (&buf, "local symbol %d",
					     count++);
			      if (ok <= 0)
				buf = (char *) "local symbol";
			      finfo->info->callbacks->error_handler
				(LD_DEFINITION_IN_DISCARDED_SECTION,
				 _("%T: discarded in section `%s' from %s\n"),
				 buf, buf, sec->name,
				 bfd_archive_filename (input_bfd));
			      if (ok != -1)
				free (buf);
			    }
			}
		    }
		}
	    }

	  /* Relocate the section by invoking a back end routine.

	     The back end routine is responsible for adjusting the
	     section contents as necessary, and (if using Rela relocs
	     and generating a relocatable output file) adjusting the
	     reloc addend as necessary.

	     The back end routine does not have to worry about setting
	     the reloc address or the reloc symbol index.

	     The back end routine is given a pointer to the swapped in
	     internal symbols, and can access the hash table entries
	     for the external symbols via elf_sym_hashes (input_bfd).

	     When generating relocatable output, the back end routine
	     must handle STB_LOCAL/STT_SECTION symbols specially.  The
	     output symbol is going to be a section symbol
	     corresponding to the output section, which will require
	     the addend to be adjusted.  */

	  if (! (*relocate_section) (output_bfd, finfo->info,
				     input_bfd, o, contents,
				     internal_relocs,
				     isymbuf,
				     finfo->sections))
	    return FALSE;

	  if (emit_relocs)
	    {
	      Elf_Internal_Rela *irela;
	      Elf_Internal_Rela *irelaend;
	      bfd_vma last_offset;
	      struct elf_link_hash_entry **rel_hash;
	      Elf_Internal_Shdr *input_rel_hdr, *input_rel_hdr2;
	      unsigned int next_erel;
	      bfd_boolean (*reloc_emitter)
		(bfd *, asection *, Elf_Internal_Shdr *, Elf_Internal_Rela *);
	      bfd_boolean rela_normal;

	      input_rel_hdr = &elf_section_data (o)->rel_hdr;
	      rela_normal = (bed->rela_normal
			     && (input_rel_hdr->sh_entsize
				 == sizeof (Elf_External_Rela)));

	      /* Adjust the reloc addresses and symbol indices.  */

	      irela = internal_relocs;
	      irelaend = irela + o->reloc_count * bed->s->int_rels_per_ext_rel;
	      rel_hash = (elf_section_data (o->output_section)->rel_hashes
			  + elf_section_data (o->output_section)->rel_count
			  + elf_section_data (o->output_section)->rel_count2);
	      last_offset = o->output_offset;
	      if (!finfo->info->relocatable)
		last_offset += o->output_section->vma;
	      for (next_erel = 0; irela < irelaend; irela++, next_erel++)
		{
		  unsigned long r_symndx;
		  asection *sec;
		  Elf_Internal_Sym sym;

		  if (next_erel == bed->s->int_rels_per_ext_rel)
		    {
		      rel_hash++;
		      next_erel = 0;
		    }

		  irela->r_offset = _bfd_elf_section_offset (output_bfd,
							     finfo->info, o,
							     irela->r_offset);
		  if (irela->r_offset >= (bfd_vma) -2)
		    {
		      /* This is a reloc for a deleted entry or somesuch.
			 Turn it into an R_*_NONE reloc, at the same
			 offset as the last reloc.  elf_eh_frame.c and
			 elf_bfd_discard_info rely on reloc offsets
			 being ordered.  */
		      irela->r_offset = last_offset;
		      irela->r_info = 0;
		      irela->r_addend = 0;
		      continue;
		    }

		  irela->r_offset += o->output_offset;

		  /* Relocs in an executable have to be virtual addresses.  */
		  if (!finfo->info->relocatable)
		    irela->r_offset += o->output_section->vma;

		  last_offset = irela->r_offset;

		  r_symndx = ELF_R_SYM (irela->r_info);
		  if (r_symndx == STN_UNDEF)
		    continue;

		  if (r_symndx >= locsymcount
		      || (elf_bad_symtab (input_bfd)
			  && finfo->sections[r_symndx] == NULL))
		    {
		      struct elf_link_hash_entry *rh;
		      unsigned long indx;

		      /* This is a reloc against a global symbol.  We
			 have not yet output all the local symbols, so
			 we do not know the symbol index of any global
			 symbol.  We set the rel_hash entry for this
			 reloc to point to the global hash table entry
			 for this symbol.  The symbol index is then
			 set at the end of elf_bfd_final_link.  */
		      indx = r_symndx - extsymoff;
		      rh = elf_sym_hashes (input_bfd)[indx];
		      while (rh->root.type == bfd_link_hash_indirect
			     || rh->root.type == bfd_link_hash_warning)
			rh = (struct elf_link_hash_entry *) rh->root.u.i.link;

		      /* Setting the index to -2 tells
			 elf_link_output_extsym that this symbol is
			 used by a reloc.  */
		      BFD_ASSERT (rh->indx < 0);
		      rh->indx = -2;

		      *rel_hash = rh;

		      continue;
		    }

		  /* This is a reloc against a local symbol.  */

		  *rel_hash = NULL;
		  sym = isymbuf[r_symndx];
		  sec = finfo->sections[r_symndx];
		  if (ELF_ST_TYPE (sym.st_info) == STT_SECTION)
		    {
		      /* I suppose the backend ought to fill in the
			 section of any STT_SECTION symbol against a
			 processor specific section.  If we have
			 discarded a section, the output_section will
			 be the absolute section.  */
		      if (bfd_is_abs_section (sec)
			  || (sec != NULL
			      && bfd_is_abs_section (sec->output_section)))
			r_symndx = 0;
		      else if (sec == NULL || sec->owner == NULL)
			{
			  bfd_set_error (bfd_error_bad_value);
			  return FALSE;
			}
		      else
			{
			  r_symndx = sec->output_section->target_index;
			  BFD_ASSERT (r_symndx != 0);
			}

		      /* Adjust the addend according to where the
			 section winds up in the output section.  */
		      if (rela_normal)
			irela->r_addend += sec->output_offset;
		    }
		  else
		    {
		      if (finfo->indices[r_symndx] == -1)
			{
			  unsigned long shlink;
			  const char *name;
			  asection *osec;

			  if (finfo->info->strip == strip_all)
			    {
			      /* You can't do ld -r -s.  */
			      bfd_set_error (bfd_error_invalid_operation);
			      return FALSE;
			    }

			  /* This symbol was skipped earlier, but
			     since it is needed by a reloc, we
			     must output it now.  */
			  shlink = symtab_hdr->sh_link;
			  name = (bfd_elf_string_from_elf_section
				  (input_bfd, shlink, sym.st_name));
			  if (name == NULL)
			    return FALSE;

			  osec = sec->output_section;
			  sym.st_shndx =
			    _bfd_elf_section_from_bfd_section (output_bfd,
							       osec);
			  if (sym.st_shndx == SHN_BAD)
			    return FALSE;

			  sym.st_value += sec->output_offset;
			  if (! finfo->info->relocatable)
			    {
			      sym.st_value += osec->vma;
			      if (ELF_ST_TYPE (sym.st_info) == STT_TLS)
				{
				  /* STT_TLS symbols are relative to PT_TLS
				     segment base.  */
				  BFD_ASSERT (elf_hash_table (finfo->info)
					      ->tls_sec != NULL);
				  sym.st_value -= (elf_hash_table (finfo->info)
						   ->tls_sec->vma);
				}
			    }

			  finfo->indices[r_symndx]
			    = bfd_get_symcount (output_bfd);

			  if (! elf_link_output_sym (finfo, name, &sym, sec,
						     NULL))
			    return FALSE;
			}

		      r_symndx = finfo->indices[r_symndx];
		    }

		  irela->r_info = ELF_R_INFO (r_symndx,
					      ELF_R_TYPE (irela->r_info));
		}

	      /* Swap out the relocs.  */
	      if (bed->elf_backend_emit_relocs
		  && !(finfo->info->relocatable
		       || finfo->info->emitrelocations))
		reloc_emitter = bed->elf_backend_emit_relocs;
	      else
		reloc_emitter = _bfd_elf_link_output_relocs;

	      if (input_rel_hdr->sh_size != 0
		  && ! (*reloc_emitter) (output_bfd, o, input_rel_hdr,
					 internal_relocs))
		return FALSE;

	      input_rel_hdr2 = elf_section_data (o)->rel_hdr2;
	      if (input_rel_hdr2 && input_rel_hdr2->sh_size != 0)
		{
		  internal_relocs += (NUM_SHDR_ENTRIES (input_rel_hdr)
				      * bed->s->int_rels_per_ext_rel);
		  if (! (*reloc_emitter) (output_bfd, o, input_rel_hdr2,
					  internal_relocs))
		    return FALSE;
		}
	    }
	}

      /* Write out the modified section contents.  */
      if (bed->elf_backend_write_section
	  && (*bed->elf_backend_write_section) (output_bfd, o, contents))
	{
	  /* Section written out.  */
	}
      else switch (o->sec_info_type)
	{
	case ELF_INFO_TYPE_STABS:
	  if (! (_bfd_write_section_stabs
		 (output_bfd,
		  &elf_hash_table (finfo->info)->stab_info,
		  o, &elf_section_data (o)->sec_info, contents)))
	    return FALSE;
	  break;
	case ELF_INFO_TYPE_MERGE:
	  if (! _bfd_write_merged_section (output_bfd, o,
					   elf_section_data (o)->sec_info))
	    return FALSE;
	  break;
	case ELF_INFO_TYPE_EH_FRAME:
	  {
	    if (! _bfd_elf_write_section_eh_frame (output_bfd, finfo->info,
						   o, contents))
	      return FALSE;
	  }
	  break;
	default:
	  {
	    bfd_size_type sec_size;

	    sec_size = (o->_cooked_size != 0 ? o->_cooked_size : o->_raw_size);
	    if (! (o->flags & SEC_EXCLUDE)
		&& ! bfd_set_section_contents (output_bfd, o->output_section,
					       contents,
					       (file_ptr) o->output_offset,
					       sec_size))
	      return FALSE;
	  }
	  break;
	}
    }

  return TRUE;
}

/* Generate a reloc when linking an ELF file.  This is a reloc
   requested by the linker, and does come from any input file.  This
   is used to build constructor and destructor tables when linking
   with -Ur.  */

static bfd_boolean
elf_reloc_link_order (bfd *output_bfd,
		      struct bfd_link_info *info,
		      asection *output_section,
		      struct bfd_link_order *link_order)
{
  reloc_howto_type *howto;
  long indx;
  bfd_vma offset;
  bfd_vma addend;
  struct elf_link_hash_entry **rel_hash_ptr;
  Elf_Internal_Shdr *rel_hdr;
  const struct elf_backend_data *bed = get_elf_backend_data (output_bfd);
  Elf_Internal_Rela irel[MAX_INT_RELS_PER_EXT_REL];
  bfd_byte *erel;
  unsigned int i;

  howto = bfd_reloc_type_lookup (output_bfd, link_order->u.reloc.p->reloc);
  if (howto == NULL)
    {
      bfd_set_error (bfd_error_bad_value);
      return FALSE;
    }

  addend = link_order->u.reloc.p->addend;

  /* Figure out the symbol index.  */
  rel_hash_ptr = (elf_section_data (output_section)->rel_hashes
		  + elf_section_data (output_section)->rel_count
		  + elf_section_data (output_section)->rel_count2);
  if (link_order->type == bfd_section_reloc_link_order)
    {
      indx = link_order->u.reloc.p->u.section->target_index;
      BFD_ASSERT (indx != 0);
      *rel_hash_ptr = NULL;
    }
  else
    {
      struct elf_link_hash_entry *h;

      /* Treat a reloc against a defined symbol as though it were
	 actually against the section.  */
      h = ((struct elf_link_hash_entry *)
	   bfd_wrapped_link_hash_lookup (output_bfd, info,
					 link_order->u.reloc.p->u.name,
					 FALSE, FALSE, TRUE));
      if (h != NULL
	  && (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak))
	{
	  asection *section;

	  section = h->root.u.def.section;
	  indx = section->output_section->target_index;
	  *rel_hash_ptr = NULL;
	  /* It seems that we ought to add the symbol value to the
	     addend here, but in practice it has already been added
	     because it was passed to constructor_callback.  */
	  addend += section->output_section->vma + section->output_offset;
	}
      else if (h != NULL)
	{
	  /* Setting the index to -2 tells elf_link_output_extsym that
	     this symbol is used by a reloc.  */
	  h->indx = -2;
	  *rel_hash_ptr = h;
	  indx = 0;
	}
      else
	{
	  if (! ((*info->callbacks->unattached_reloc)
		 (info, link_order->u.reloc.p->u.name, NULL, NULL, 0)))
	    return FALSE;
	  indx = 0;
	}
    }

  /* If this is an inplace reloc, we must write the addend into the
     object file.  */
  if (howto->partial_inplace && addend != 0)
    {
      bfd_size_type size;
      bfd_reloc_status_type rstat;
      bfd_byte *buf;
      bfd_boolean ok;
      const char *sym_name;

      size = bfd_get_reloc_size (howto);
      buf = bfd_zmalloc (size);
      if (buf == NULL)
	return FALSE;
      rstat = _bfd_relocate_contents (howto, output_bfd, addend, buf);
      switch (rstat)
	{
	case bfd_reloc_ok:
	  break;

	default:
	case bfd_reloc_outofrange:
	  abort ();

	case bfd_reloc_overflow:
	  if (link_order->type == bfd_section_reloc_link_order)
	    sym_name = bfd_section_name (output_bfd,
					 link_order->u.reloc.p->u.section);
	  else
	    sym_name = link_order->u.reloc.p->u.name;
	  if (! ((*info->callbacks->reloc_overflow)
		 (info, sym_name, howto->name, addend, NULL, NULL, 0)))
	    {
	      free (buf);
	      return FALSE;
	    }
	  break;
	}
      ok = bfd_set_section_contents (output_bfd, output_section, buf,
				     link_order->offset, size);
      free (buf);
      if (! ok)
	return FALSE;
    }

  /* The address of a reloc is relative to the section in a
     relocatable file, and is a virtual address in an executable
     file.  */
  offset = link_order->offset;
  if (! info->relocatable)
    offset += output_section->vma;

  for (i = 0; i < bed->s->int_rels_per_ext_rel; i++)
    {
      irel[i].r_offset = offset;
      irel[i].r_info = 0;
      irel[i].r_addend = 0;
    }
  irel[0].r_info = ELF_R_INFO (indx, howto->type);

  rel_hdr = &elf_section_data (output_section)->rel_hdr;
  erel = rel_hdr->contents;
  if (rel_hdr->sh_type == SHT_REL)
    {
      erel += (elf_section_data (output_section)->rel_count
	       * sizeof (Elf_External_Rel));
      (*bed->s->swap_reloc_out) (output_bfd, irel, erel);
    }
  else
    {
      irel[0].r_addend = addend;
      erel += (elf_section_data (output_section)->rel_count
	       * sizeof (Elf_External_Rela));
      (*bed->s->swap_reloca_out) (output_bfd, irel, erel);
    }

  ++elf_section_data (output_section)->rel_count;

  return TRUE;
}

/* Garbage collect unused sections.  */

static bfd_boolean elf_gc_sweep_symbol
  (struct elf_link_hash_entry *, void *);

static bfd_boolean elf_gc_allocate_got_offsets
  (struct elf_link_hash_entry *, void *);

/* The mark phase of garbage collection.  For a given section, mark
   it and any sections in this section's group, and all the sections
   which define symbols to which it refers.  */

typedef asection * (*gc_mark_hook_fn)
  (asection *, struct bfd_link_info *, Elf_Internal_Rela *,
   struct elf_link_hash_entry *, Elf_Internal_Sym *);

static bfd_boolean
elf_gc_mark (struct bfd_link_info *info,
	     asection *sec,
	     gc_mark_hook_fn gc_mark_hook)
{
  bfd_boolean ret;
  asection *group_sec;

  sec->gc_mark = 1;

  /* Mark all the sections in the group.  */
  group_sec = elf_section_data (sec)->next_in_group;
  if (group_sec && !group_sec->gc_mark)
    if (!elf_gc_mark (info, group_sec, gc_mark_hook))
      return FALSE;

  /* Look through the section relocs.  */
  ret = TRUE;
  if ((sec->flags & SEC_RELOC) != 0 && sec->reloc_count > 0)
    {
      Elf_Internal_Rela *relstart, *rel, *relend;
      Elf_Internal_Shdr *symtab_hdr;
      struct elf_link_hash_entry **sym_hashes;
      size_t nlocsyms;
      size_t extsymoff;
      bfd *input_bfd = sec->owner;
      const struct elf_backend_data *bed = get_elf_backend_data (input_bfd);
      Elf_Internal_Sym *isym = NULL;

      symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
      sym_hashes = elf_sym_hashes (input_bfd);

      /* Read the local symbols.  */
      if (elf_bad_symtab (input_bfd))
	{
	  nlocsyms = symtab_hdr->sh_size / sizeof (Elf_External_Sym);
	  extsymoff = 0;
	}
      else
	extsymoff = nlocsyms = symtab_hdr->sh_info;

      isym = (Elf_Internal_Sym *) symtab_hdr->contents;
      if (isym == NULL && nlocsyms != 0)
	{
	  isym = bfd_elf_get_elf_syms (input_bfd, symtab_hdr, nlocsyms, 0,
				       NULL, NULL, NULL);
	  if (isym == NULL)
	    return FALSE;
	}

      /* Read the relocations.  */
      relstart = _bfd_elf_link_read_relocs (input_bfd, sec, NULL, NULL,
					    info->keep_memory);
      if (relstart == NULL)
	{
	  ret = FALSE;
	  goto out1;
	}
      relend = relstart + sec->reloc_count * bed->s->int_rels_per_ext_rel;

      for (rel = relstart; rel < relend; rel++)
	{
	  unsigned long r_symndx;
	  asection *rsec;
	  struct elf_link_hash_entry *h;

	  r_symndx = ELF_R_SYM (rel->r_info);
	  if (r_symndx == 0)
	    continue;

	  if (r_symndx >= nlocsyms
	      || ELF_ST_BIND (isym[r_symndx].st_info) != STB_LOCAL)
	    {
	      h = sym_hashes[r_symndx - extsymoff];
	      rsec = (*gc_mark_hook) (sec, info, rel, h, NULL);
	    }
	  else
	    {
	      rsec = (*gc_mark_hook) (sec, info, rel, NULL, &isym[r_symndx]);
	    }

	  if (rsec && !rsec->gc_mark)
	    {
	      if (bfd_get_flavour (rsec->owner) != bfd_target_elf_flavour)
		rsec->gc_mark = 1;
	      else if (!elf_gc_mark (info, rsec, gc_mark_hook))
		{
		  ret = FALSE;
		  goto out2;
		}
	    }
	}

    out2:
      if (elf_section_data (sec)->relocs != relstart)
	free (relstart);
    out1:
      if (isym != NULL && symtab_hdr->contents != (unsigned char *) isym)
	{
	  if (! info->keep_memory)
	    free (isym);
	  else
	    symtab_hdr->contents = (unsigned char *) isym;
	}
    }

  return ret;
}

/* The sweep phase of garbage collection.  Remove all garbage sections.  */

typedef bfd_boolean (*gc_sweep_hook_fn)
  (bfd *, struct bfd_link_info *, asection *, const Elf_Internal_Rela *);

static bfd_boolean
elf_gc_sweep (struct bfd_link_info *info, gc_sweep_hook_fn gc_sweep_hook)
{
  bfd *sub;

  for (sub = info->input_bfds; sub != NULL; sub = sub->link_next)
    {
      asection *o;

      if (bfd_get_flavour (sub) != bfd_target_elf_flavour)
	continue;

      for (o = sub->sections; o != NULL; o = o->next)
	{
	  /* Keep special sections.  Keep .debug sections.  */
	  if ((o->flags & SEC_LINKER_CREATED)
	      || (o->flags & SEC_DEBUGGING))
	    o->gc_mark = 1;

	  if (o->gc_mark)
	    continue;

	  /* Skip sweeping sections already excluded.  */
	  if (o->flags & SEC_EXCLUDE)
	    continue;

	  /* Since this is early in the link process, it is simple
	     to remove a section from the output.  */
	  o->flags |= SEC_EXCLUDE;

	  /* But we also have to update some of the relocation
	     info we collected before.  */
	  if (gc_sweep_hook
	      && (o->flags & SEC_RELOC) && o->reloc_count > 0)
	    {
	      Elf_Internal_Rela *internal_relocs;
	      bfd_boolean r;

	      internal_relocs
		= _bfd_elf_link_read_relocs (o->owner, o, NULL, NULL,
					     info->keep_memory);
	      if (internal_relocs == NULL)
		return FALSE;

	      r = (*gc_sweep_hook) (o->owner, info, o, internal_relocs);

	      if (elf_section_data (o)->relocs != internal_relocs)
		free (internal_relocs);

	      if (!r)
		return FALSE;
	    }
	}
    }

  /* Remove the symbols that were in the swept sections from the dynamic
     symbol table.  GCFIXME: Anyone know how to get them out of the
     static symbol table as well?  */
  {
    int i = 0;

    elf_link_hash_traverse (elf_hash_table (info), elf_gc_sweep_symbol, &i);

    elf_hash_table (info)->dynsymcount = i;
  }

  return TRUE;
}

/* Sweep symbols in swept sections.  Called via elf_link_hash_traverse.  */

static bfd_boolean
elf_gc_sweep_symbol (struct elf_link_hash_entry *h, void *idxptr)
{
  int *idx = idxptr;

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  if (h->dynindx != -1
      && ((h->root.type != bfd_link_hash_defined
	   && h->root.type != bfd_link_hash_defweak)
	  || h->root.u.def.section->gc_mark))
    h->dynindx = (*idx)++;

  return TRUE;
}

/* Propagate collected vtable information.  This is called through
   elf_link_hash_traverse.  */

static bfd_boolean
elf_gc_propagate_vtable_entries_used (struct elf_link_hash_entry *h, void *okp)
{
  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  /* Those that are not vtables.  */
  if (h->vtable_parent == NULL)
    return TRUE;

  /* Those vtables that do not have parents, we cannot merge.  */
  if (h->vtable_parent == (struct elf_link_hash_entry *) -1)
    return TRUE;

  /* If we've already been done, exit.  */
  if (h->vtable_entries_used && h->vtable_entries_used[-1])
    return TRUE;

  /* Make sure the parent's table is up to date.  */
  elf_gc_propagate_vtable_entries_used (h->vtable_parent, okp);

  if (h->vtable_entries_used == NULL)
    {
      /* None of this table's entries were referenced.  Re-use the
	 parent's table.  */
      h->vtable_entries_used = h->vtable_parent->vtable_entries_used;
      h->vtable_entries_size = h->vtable_parent->vtable_entries_size;
    }
  else
    {
      size_t n;
      bfd_boolean *cu, *pu;

      /* Or the parent's entries into ours.  */
      cu = h->vtable_entries_used;
      cu[-1] = TRUE;
      pu = h->vtable_parent->vtable_entries_used;
      if (pu != NULL)
	{
	  const struct elf_backend_data *bed;
	  unsigned int log_file_align;

	  bed = get_elf_backend_data (h->root.u.def.section->owner);
	  log_file_align = bed->s->log_file_align;
	  n = h->vtable_parent->vtable_entries_size >> log_file_align;
	  while (n--)
	    {
	      if (*pu)
		*cu = TRUE;
	      pu++;
	      cu++;
	    }
	}
    }

  return TRUE;
}

static bfd_boolean
elf_gc_smash_unused_vtentry_relocs (struct elf_link_hash_entry *h, void *okp)
{
  asection *sec;
  bfd_vma hstart, hend;
  Elf_Internal_Rela *relstart, *relend, *rel;
  const struct elf_backend_data *bed;
  unsigned int log_file_align;

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  /* Take care of both those symbols that do not describe vtables as
     well as those that are not loaded.  */
  if (h->vtable_parent == NULL)
    return TRUE;

  BFD_ASSERT (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak);

  sec = h->root.u.def.section;
  hstart = h->root.u.def.value;
  hend = hstart + h->size;

  relstart = _bfd_elf_link_read_relocs (sec->owner, sec, NULL, NULL, TRUE);
  if (!relstart)
    return *(bfd_boolean *) okp = FALSE;
  bed = get_elf_backend_data (sec->owner);
  log_file_align = bed->s->log_file_align;

  relend = relstart + sec->reloc_count * bed->s->int_rels_per_ext_rel;

  for (rel = relstart; rel < relend; ++rel)
    if (rel->r_offset >= hstart && rel->r_offset < hend)
      {
	/* If the entry is in use, do nothing.  */
	if (h->vtable_entries_used
	    && (rel->r_offset - hstart) < h->vtable_entries_size)
	  {
	    bfd_vma entry = (rel->r_offset - hstart) >> log_file_align;
	    if (h->vtable_entries_used[entry])
	      continue;
	  }
	/* Otherwise, kill it.  */
	rel->r_offset = rel->r_info = rel->r_addend = 0;
      }

  return TRUE;
}

/* Do mark and sweep of unused sections.  */

bfd_boolean
elf_gc_sections (bfd *abfd, struct bfd_link_info *info)
{
  bfd_boolean ok = TRUE;
  bfd *sub;
  asection * (*gc_mark_hook)
    (asection *, struct bfd_link_info *, Elf_Internal_Rela *,
     struct elf_link_hash_entry *h, Elf_Internal_Sym *);

  if (!get_elf_backend_data (abfd)->can_gc_sections
      || info->relocatable
      || info->emitrelocations
      || !is_elf_hash_table (info->hash)
      || elf_hash_table (info)->dynamic_sections_created)
    {
      (*_bfd_error_handler)(_("Warning: gc-sections option ignored"));
      return TRUE;
    }

  /* Apply transitive closure to the vtable entry usage info.  */
  elf_link_hash_traverse (elf_hash_table (info),
			  elf_gc_propagate_vtable_entries_used,
			  &ok);
  if (!ok)
    return FALSE;

  /* Kill the vtable relocations that were not used.  */
  elf_link_hash_traverse (elf_hash_table (info),
			  elf_gc_smash_unused_vtentry_relocs,
			  &ok);
  if (!ok)
    return FALSE;

  /* Grovel through relocs to find out who stays ...  */

  gc_mark_hook = get_elf_backend_data (abfd)->gc_mark_hook;
  for (sub = info->input_bfds; sub != NULL; sub = sub->link_next)
    {
      asection *o;

      if (bfd_get_flavour (sub) != bfd_target_elf_flavour)
	continue;

      for (o = sub->sections; o != NULL; o = o->next)
	{
	  if (o->flags & SEC_KEEP)
	    if (!elf_gc_mark (info, o, gc_mark_hook))
	      return FALSE;
	}
    }

  /* ... and mark SEC_EXCLUDE for those that go.  */
  if (!elf_gc_sweep (info, get_elf_backend_data (abfd)->gc_sweep_hook))
    return FALSE;

  return TRUE;
}

/* Called from check_relocs to record the existence of a VTINHERIT reloc.  */

bfd_boolean
elf_gc_record_vtinherit (bfd *abfd,
			 asection *sec,
			 struct elf_link_hash_entry *h,
			 bfd_vma offset)
{
  struct elf_link_hash_entry **sym_hashes, **sym_hashes_end;
  struct elf_link_hash_entry **search, *child;
  bfd_size_type extsymcount;

  /* The sh_info field of the symtab header tells us where the
     external symbols start.  We don't care about the local symbols at
     this point.  */
  extsymcount = elf_tdata (abfd)->symtab_hdr.sh_size/sizeof (Elf_External_Sym);
  if (!elf_bad_symtab (abfd))
    extsymcount -= elf_tdata (abfd)->symtab_hdr.sh_info;

  sym_hashes = elf_sym_hashes (abfd);
  sym_hashes_end = sym_hashes + extsymcount;

  /* Hunt down the child symbol, which is in this section at the same
     offset as the relocation.  */
  for (search = sym_hashes; search != sym_hashes_end; ++search)
    {
      if ((child = *search) != NULL
	  && (child->root.type == bfd_link_hash_defined
	      || child->root.type == bfd_link_hash_defweak)
	  && child->root.u.def.section == sec
	  && child->root.u.def.value == offset)
	goto win;
    }

  (*_bfd_error_handler) ("%s: %s+%lu: No symbol found for INHERIT",
			 bfd_archive_filename (abfd), sec->name,
			 (unsigned long) offset);
  bfd_set_error (bfd_error_invalid_operation);
  return FALSE;

 win:
  if (!h)
    {
      /* This *should* only be the absolute section.  It could potentially
	 be that someone has defined a non-global vtable though, which
	 would be bad.  It isn't worth paging in the local symbols to be
	 sure though; that case should simply be handled by the assembler.  */

      child->vtable_parent = (struct elf_link_hash_entry *) -1;
    }
  else
    child->vtable_parent = h;

  return TRUE;
}

/* Called from check_relocs to record the existence of a VTENTRY reloc.  */

bfd_boolean
elf_gc_record_vtentry (bfd *abfd ATTRIBUTE_UNUSED,
		       asection *sec ATTRIBUTE_UNUSED,
		       struct elf_link_hash_entry *h,
		       bfd_vma addend)
{
  const struct elf_backend_data *bed = get_elf_backend_data (abfd);
  unsigned int log_file_align = bed->s->log_file_align;

  if (addend >= h->vtable_entries_size)
    {
      size_t size, bytes, file_align;
      bfd_boolean *ptr = h->vtable_entries_used;

      /* While the symbol is undefined, we have to be prepared to handle
	 a zero size.  */
      file_align = 1 << log_file_align;
      if (h->root.type == bfd_link_hash_undefined)
	size = addend + file_align;
      else
	{
	  size = h->size;
	  if (addend >= size)
	    {
	      /* Oops!  We've got a reference past the defined end of
		 the table.  This is probably a bug -- shall we warn?  */
	      size = addend + file_align;
	    }
	}
      size = (size + file_align - 1) & -file_align;

      /* Allocate one extra entry for use as a "done" flag for the
	 consolidation pass.  */
      bytes = ((size >> log_file_align) + 1) * sizeof (bfd_boolean);

      if (ptr)
	{
	  ptr = bfd_realloc (ptr - 1, bytes);

	  if (ptr != NULL)
	    {
	      size_t oldbytes;

	      oldbytes = (((h->vtable_entries_size >> log_file_align) + 1)
			  * sizeof (bfd_boolean));
	      memset (((char *) ptr) + oldbytes, 0, bytes - oldbytes);
	    }
	}
      else
	ptr = bfd_zmalloc (bytes);

      if (ptr == NULL)
	return FALSE;

      /* And arrange for that done flag to be at index -1.  */
      h->vtable_entries_used = ptr + 1;
      h->vtable_entries_size = size;
    }

  h->vtable_entries_used[addend >> log_file_align] = TRUE;

  return TRUE;
}

/* And an accompanying bit to work out final got entry offsets once
   we're done.  Should be called from final_link.  */

bfd_boolean
elf_gc_common_finalize_got_offsets (bfd *abfd,
				    struct bfd_link_info *info)
{
  bfd *i;
  const struct elf_backend_data *bed = get_elf_backend_data (abfd);
  bfd_vma gotoff;

  if (! is_elf_hash_table (info->hash))
    return FALSE;

  /* The GOT offset is relative to the .got section, but the GOT header is
     put into the .got.plt section, if the backend uses it.  */
  if (bed->want_got_plt)
    gotoff = 0;
  else
    gotoff = bed->got_header_size;

  /* Do the local .got entries first.  */
  for (i = info->input_bfds; i; i = i->link_next)
    {
      bfd_signed_vma *local_got;
      bfd_size_type j, locsymcount;
      Elf_Internal_Shdr *symtab_hdr;

      if (bfd_get_flavour (i) != bfd_target_elf_flavour)
	continue;

      local_got = elf_local_got_refcounts (i);
      if (!local_got)
	continue;

      symtab_hdr = &elf_tdata (i)->symtab_hdr;
      if (elf_bad_symtab (i))
	locsymcount = symtab_hdr->sh_size / sizeof (Elf_External_Sym);
      else
	locsymcount = symtab_hdr->sh_info;

      for (j = 0; j < locsymcount; ++j)
	{
	  if (local_got[j] > 0)
	    {
	      local_got[j] = gotoff;
	      gotoff += ARCH_SIZE / 8;
	    }
	  else
	    local_got[j] = (bfd_vma) -1;
	}
    }

  /* Then the global .got entries.  .plt refcounts are handled by
     adjust_dynamic_symbol  */
  elf_link_hash_traverse (elf_hash_table (info),
			  elf_gc_allocate_got_offsets,
			  &gotoff);
  return TRUE;
}

/* We need a special top-level link routine to convert got reference counts
   to real got offsets.  */

static bfd_boolean
elf_gc_allocate_got_offsets (struct elf_link_hash_entry *h, void *offarg)
{
  bfd_vma *off = offarg;

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  if (h->got.refcount > 0)
    {
      h->got.offset = off[0];
      off[0] += ARCH_SIZE / 8;
    }
  else
    h->got.offset = (bfd_vma) -1;

  return TRUE;
}

/* Many folk need no more in the way of final link than this, once
   got entry reference counting is enabled.  */

bfd_boolean
elf_gc_common_final_link (bfd *abfd, struct bfd_link_info *info)
{
  if (!elf_gc_common_finalize_got_offsets (abfd, info))
    return FALSE;

  /* Invoke the regular ELF backend linker to do all the work.  */
  return elf_bfd_final_link (abfd, info);
}

/* This function will be called though elf_link_hash_traverse to store
   all hash value of the exported symbols in an array.  */

static bfd_boolean
elf_collect_hash_codes (struct elf_link_hash_entry *h, void *data)
{
  unsigned long **valuep = data;
  const char *name;
  char *p;
  unsigned long ha;
  char *alc = NULL;

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  /* Ignore indirect symbols.  These are added by the versioning code.  */
  if (h->dynindx == -1)
    return TRUE;

  name = h->root.root.string;
  p = strchr (name, ELF_VER_CHR);
  if (p != NULL)
    {
      alc = bfd_malloc (p - name + 1);
      memcpy (alc, name, p - name);
      alc[p - name] = '\0';
      name = alc;
    }

  /* Compute the hash value.  */
  ha = bfd_elf_hash (name);

  /* Store the found hash value in the array given as the argument.  */
  *(*valuep)++ = ha;

  /* And store it in the struct so that we can put it in the hash table
     later.  */
  h->elf_hash_value = ha;

  if (alc != NULL)
    free (alc);

  return TRUE;
}

bfd_boolean
elf_reloc_symbol_deleted_p (bfd_vma offset, void *cookie)
{
  struct elf_reloc_cookie *rcookie = cookie;

  if (rcookie->bad_symtab)
    rcookie->rel = rcookie->rels;

  for (; rcookie->rel < rcookie->relend; rcookie->rel++)
    {
      unsigned long r_symndx;

      if (! rcookie->bad_symtab)
	if (rcookie->rel->r_offset > offset)
	  return FALSE;
      if (rcookie->rel->r_offset != offset)
	continue;

      r_symndx = ELF_R_SYM (rcookie->rel->r_info);
      if (r_symndx == SHN_UNDEF)
	return TRUE;

      if (r_symndx >= rcookie->locsymcount
	  || ELF_ST_BIND (rcookie->locsyms[r_symndx].st_info) != STB_LOCAL)
	{
	  struct elf_link_hash_entry *h;

	  h = rcookie->sym_hashes[r_symndx - rcookie->extsymoff];

	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;

	  if ((h->root.type == bfd_link_hash_defined
	       || h->root.type == bfd_link_hash_defweak)
	      && elf_discarded_section (h->root.u.def.section))
	    return TRUE;
	  else
	    return FALSE;
	}
      else
	{
	  /* It's not a relocation against a global symbol,
	     but it could be a relocation against a local
	     symbol for a discarded section.  */
	  asection *isec;
	  Elf_Internal_Sym *isym;

	  /* Need to: get the symbol; get the section.  */
	  isym = &rcookie->locsyms[r_symndx];
	  if (isym->st_shndx < SHN_LORESERVE || isym->st_shndx > SHN_HIRESERVE)
	    {
	      isec = section_from_elf_index (rcookie->abfd, isym->st_shndx);
	      if (isec != NULL && elf_discarded_section (isec))
		return TRUE;
	    }
	}
      return FALSE;
    }
  return FALSE;
}

/* Discard unneeded references to discarded sections.
   Returns TRUE if any section's size was changed.  */
/* This function assumes that the relocations are in sorted order,
   which is true for all known assemblers.  */

bfd_boolean
elf_bfd_discard_info (bfd *output_bfd, struct bfd_link_info *info)
{
  struct elf_reloc_cookie cookie;
  asection *stab, *eh;
  Elf_Internal_Shdr *symtab_hdr;
  const struct elf_backend_data *bed;
  bfd *abfd;
  unsigned int count;
  bfd_boolean ret = FALSE;

  if (info->traditional_format
      || !is_elf_hash_table (info->hash))
    return FALSE;

  for (abfd = info->input_bfds; abfd != NULL; abfd = abfd->link_next)
    {
      if (bfd_get_flavour (abfd) != bfd_target_elf_flavour)
	continue;

      bed = get_elf_backend_data (abfd);

      if ((abfd->flags & DYNAMIC) != 0)
	continue;

      eh = bfd_get_section_by_name (abfd, ".eh_frame");
      if (info->relocatable
	  || (eh != NULL
	      && (eh->_raw_size == 0
		  || bfd_is_abs_section (eh->output_section))))
	eh = NULL;

      stab = bfd_get_section_by_name (abfd, ".stab");
      if (stab != NULL
	  && (stab->_raw_size == 0
	      || bfd_is_abs_section (stab->output_section)
	      || stab->sec_info_type != ELF_INFO_TYPE_STABS))
	stab = NULL;

      if (stab == NULL
	  && eh == NULL
	  && bed->elf_backend_discard_info == NULL)
	continue;

      symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
      cookie.abfd = abfd;
      cookie.sym_hashes = elf_sym_hashes (abfd);
      cookie.bad_symtab = elf_bad_symtab (abfd);
      if (cookie.bad_symtab)
	{
	  cookie.locsymcount = symtab_hdr->sh_size / sizeof (Elf_External_Sym);
	  cookie.extsymoff = 0;
	}
      else
	{
	  cookie.locsymcount = symtab_hdr->sh_info;
	  cookie.extsymoff = symtab_hdr->sh_info;
	}

      cookie.locsyms = (Elf_Internal_Sym *) symtab_hdr->contents;
      if (cookie.locsyms == NULL && cookie.locsymcount != 0)
	{
	  cookie.locsyms = bfd_elf_get_elf_syms (abfd, symtab_hdr,
						 cookie.locsymcount, 0,
						 NULL, NULL, NULL);
	  if (cookie.locsyms == NULL)
	    return FALSE;
	}

      if (stab != NULL)
	{
	  cookie.rels = NULL;
	  count = stab->reloc_count;
	  if (count != 0)
	    cookie.rels = _bfd_elf_link_read_relocs (abfd, stab, NULL, NULL,
						     info->keep_memory);
	  if (cookie.rels != NULL)
	    {
	      cookie.rel = cookie.rels;
	      cookie.relend = cookie.rels;
	      cookie.relend += count * bed->s->int_rels_per_ext_rel;
	      if (_bfd_discard_section_stabs (abfd, stab,
					      elf_section_data (stab)->sec_info,
					      elf_reloc_symbol_deleted_p,
					      &cookie))
		ret = TRUE;
	      if (elf_section_data (stab)->relocs != cookie.rels)
		free (cookie.rels);
	    }
	}

      if (eh != NULL)
	{
	  cookie.rels = NULL;
	  count = eh->reloc_count;
	  if (count != 0)
	    cookie.rels = _bfd_elf_link_read_relocs (abfd, eh, NULL, NULL,
						     info->keep_memory);
	  cookie.rel = cookie.rels;
	  cookie.relend = cookie.rels;
	  if (cookie.rels != NULL)
	    cookie.relend += count * bed->s->int_rels_per_ext_rel;

	  if (_bfd_elf_discard_section_eh_frame (abfd, info, eh,
						 elf_reloc_symbol_deleted_p,
						 &cookie))
	    ret = TRUE;

	  if (cookie.rels != NULL
	      && elf_section_data (eh)->relocs != cookie.rels)
	    free (cookie.rels);
	}

      if (bed->elf_backend_discard_info != NULL
	  && (*bed->elf_backend_discard_info) (abfd, &cookie, info))
	ret = TRUE;

      if (cookie.locsyms != NULL
	  && symtab_hdr->contents != (unsigned char *) cookie.locsyms)
	{
	  if (! info->keep_memory)
	    free (cookie.locsyms);
	  else
	    symtab_hdr->contents = (unsigned char *) cookie.locsyms;
	}
    }

  if (info->eh_frame_hdr
      && !info->relocatable
      && _bfd_elf_discard_section_eh_frame_hdr (output_bfd, info))
    ret = TRUE;

  return ret;
}

static bfd_boolean
elf_section_ignore_discarded_relocs (asection *sec)
{
  const struct elf_backend_data *bed;

  switch (sec->sec_info_type)
    {
    case ELF_INFO_TYPE_STABS:
    case ELF_INFO_TYPE_EH_FRAME:
      return TRUE;
    default:
      break;
    }

  bed = get_elf_backend_data (sec->owner);
  if (bed->elf_backend_ignore_discarded_relocs != NULL
      && (*bed->elf_backend_ignore_discarded_relocs) (sec))
    return TRUE;

  return FALSE;
}

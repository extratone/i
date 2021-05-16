/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ar.h>
#include <mach-o/ranlib.h>
#include <libc.h>
#include "stuff/bool.h"
#include "stuff/ofile.h"
#include "stuff/errors.h"
#include "stuff/allocate.h"
#include "stuff/symbol.h"
#include "stuff/symbol.h"
#include "otool.h"
#include "ofile_print.h"
#include "m68k_disasm.h"
#include "i860_disasm.h"
#include "i386_disasm.h"
#include "m88k_disasm.h"
#include "ppc_disasm.h"
#include "hppa_disasm.h"
#include "sparc_disasm.h"
#include "arm_disasm.h"

/* Name of this program for error messages (argv[0]) */
char *progname = NULL;

/*
 * The flags to indicate the actions to perform.
 */
enum bool fflag = FALSE; /* print the fat headers */
enum bool aflag = FALSE; /* print the archive header */
enum bool hflag = FALSE; /* print the exec or mach header */
enum bool lflag = FALSE; /* print the load commands */
enum bool Lflag = FALSE; /* print the shared library names */
enum bool Dflag = FALSE; /* print the shared library id name */
enum bool tflag = FALSE; /* print the text */
enum bool dflag = FALSE; /* print the data */
enum bool oflag = FALSE; /* print the objctive-C info */
enum bool Oflag = FALSE; /* print the objctive-C selector strings only */
enum bool rflag = FALSE; /* print the relocation entries */
enum bool Tflag = FALSE; /* print the dylib table of contents */
enum bool Mflag = FALSE; /* print the dylib module table */
enum bool Rflag = FALSE; /* print the dylib reference table */
enum bool Iflag = FALSE; /* print the indirect symbol table entries */
enum bool Hflag = FALSE; /* print the two-level hints table */
enum bool Sflag = FALSE; /* print the contents of the __.SYMDEF file */
enum bool vflag = FALSE; /* print verbosely (symbolically) when possible */
enum bool Vflag = FALSE; /* print dissassembled operands verbosely */
enum bool cflag = FALSE; /* print the argument and environ strings of a core */
enum bool iflag = FALSE; /* print the shared library initialization table */
enum bool Wflag = FALSE; /* print the mod time of an archive as a number */
enum bool Xflag = FALSE; /* don't print leading address in disassembly */
enum bool Zflag = FALSE; /* don't use simplified ppc mnemonics in disassembly */
enum bool Bflag = FALSE; /* force Thumb disassembly (ARM objects only) */
char *pflag = NULL; 	 /* procedure name to start disassembling from */
char *segname = NULL;	 /* name of the section to print the contents of */
char *sectname = NULL;

/* this is set when any of the flags that process object files is set */
enum bool object_processing = FALSE;

static void usage(
    void);

static void processor(
    struct ofile *ofile,
    char *arch_name,
    void *cookie);

static void get_symbol_table_info(
    struct load_command *load_commands,
    uint32_t ncmds,
    uint32_t sizeofcmds,
    cpu_type_t cputype,
    enum byte_sex load_commands_byte_sex,
    char *object_addr,
    uint32_t object_size,
    struct nlist **symbols,
    struct nlist_64 **symbols64,
    uint32_t *nsymbols,
    char **strings,
    uint32_t *strings_size);

static void get_toc_info(
    struct load_command *load_commands,
    uint32_t ncmds,
    uint32_t sizeofcmds,
    enum byte_sex load_commands_byte_sex,
    char *object_addr,
    uint32_t object_size,
    struct dylib_table_of_contents **tocs,
    uint32_t *ntocs);

static void get_module_table_info(
    struct load_command *load_commands,
    uint32_t ncmds,
    uint32_t sizeofcmds,
    cpu_type_t cputype,
    enum byte_sex load_commands_byte_sex,
    char *object_addr,
    uint32_t object_size,
    struct dylib_module **mods,
    struct dylib_module_64 **mods64,
    uint32_t *nmods);

static void get_ref_info(
    struct load_command *load_commands,
    uint32_t ncmds,
    uint32_t sizeofcmds,
    enum byte_sex load_commands_byte_sex,
    char *object_addr,
    uint32_t object_size,
    struct dylib_reference **refs,
    uint32_t *nrefs);

static void get_indirect_symbol_table_info(
    struct load_command *load_commands,
    uint32_t ncmds,
    uint32_t sizeofcmds,
    enum byte_sex load_commands_byte_sex,
    char *object_addr,
    uint32_t object_size,
    uint32_t **indirect_symbols,
    uint32_t *nindirect_symbols);

static enum bool get_dyst(
    struct load_command *load_commands,
    uint32_t ncmds,
    uint32_t sizeofcmds,
    enum byte_sex load_commands_byte_sex,
    struct dysymtab_command *dyst);

static void get_hints_table_info(
    struct load_command *load_commands,
    uint32_t ncmds,
    uint32_t sizeofcmds,
    enum byte_sex load_commands_byte_sex,
    char *object_addr,
    uint32_t object_size,
    struct twolevel_hint **hints,
    uint32_t *nhints);

static enum bool get_hints_cmd(
    struct load_command *load_commands,
    uint32_t ncmds,
    uint32_t sizeofcmds,
    enum byte_sex load_commands_byte_sex,
    struct twolevel_hints_command *hints_cmd);

static int sym_compare(
    struct symbol *sym1,
    struct symbol *sym2);

static int rel_compare(
    struct relocation_info *rel1,
    struct relocation_info *rel2);

static void get_linked_reloc_info(
    struct load_command *load_commands,
    uint32_t ncmds,
    uint32_t sizeofcmds,
    enum byte_sex load_commands_byte_sex,
    char *object_addr,
    uint32_t object_size,
    struct relocation_info **ext_relocs,
    uint32_t *next_relocs,
    struct relocation_info **loc_relocs,
    uint32_t *nloc_relocs);

static void print_text(
    cpu_type_t cputype,
    enum byte_sex object_byte_sex,
    char *sect,
    uint32_t size,
    uint64_t addr,
    struct symbol *sorted_symbols,
    uint32_t nsorted_symbols,
    struct nlist *symbols,
    struct nlist_64 *symbols64,
    uint32_t nsymbols,
    char *strings,
    uint32_t strings_size,
    struct relocation_info *relocs,
    uint32_t nrelocs,
    uint32_t *indirect_symbols,
    uint32_t nindirect_symbols,
    struct load_command *load_commands,
    uint32_t ncmds,
    uint32_t sizeofcmds,
    enum bool disassemble,
    enum bool verbose,
    cpu_subtype_t cpusubtype);

static void print_argstrings(
    uint32_t magic,
    struct load_command *load_commands,
    uint32_t ncmds,
    uint32_t sizeofcmds,
    cpu_type_t cputype,
    cpu_subtype_t cpusubtype,
    enum byte_sex load_commands_byte_sex,
    char *object_addr,
    uint32_t object_size);

int
main(
int argc,
char **argv,
char **envp)
{
    int i;
    uint32_t j, nfiles;
    struct arch_flag *arch_flags;
    uint32_t narch_flags;
    enum bool all_archs, use_member_syntax;
    char **files;

	progname = argv[0];
	arch_flags = NULL;
	narch_flags = 0;
	all_archs = FALSE;
	use_member_syntax = TRUE;

	if(argc <= 1)
	    usage();

	/*
	 * Parse the arguments.
	 */
	nfiles = 0;
        files = allocate(sizeof(char *) * argc);
	for(i = 1; i < argc; i++){
	    if(argv[i][0] == '-' && argv[i][1] == '\0'){
		for(i += 1 ; i < argc; i++)
		    files[nfiles++] = argv[i];
		break;
	    }
	    if(argv[i][0] != '-'){
		files[nfiles++] = argv[i];
		continue;
	    }
	    if(strcmp(argv[i], "-arch") == 0){
		if(i + 1 == argc){
		    error("missing argument(s) to %s option", argv[i]);
		    usage();
		}
		if(strcmp("all", argv[i+1]) == 0){
		    all_archs = TRUE;
		}
		else{
		    arch_flags = reallocate(arch_flags,
			    (narch_flags + 1) * sizeof(struct arch_flag));
		    if(get_arch_from_flag(argv[i+1],
					  arch_flags + narch_flags) == 0){
			error("unknown architecture specification flag: "
			      "%s %s", argv[i], argv[i+1]);
			arch_usage();
			usage();
		    }
		    narch_flags++;
		}
		i++;
		continue;
	    }
	    if(argv[i][1] == 'p'){
		if(argc <=  i + 1){
		    error("-p requires an argument (a text symbol name)");
		    usage();
		}
		if(pflag)
		    error("only one -p flag can be specified");
		pflag = argv[i + 1];
		i++;
		continue;
	    }
	    if(argv[i][1] == 's'){
		if(argc <=  i + 2){
		    error("-s requires two arguments (a segment name and a "
			  "section name)");
		    usage();
		}
		if(sectname != NULL){
		    error("only one -s flag can be specified");
		    usage();
		}
		segname  = argv[i + 1];
		sectname = argv[i + 2];
		i += 2;
		object_processing = TRUE;
		continue;
	    }
	    for(j = 1; argv[i][j] != '\0'; j++){
		switch(argv[i][j]){
		case 'V':
		    Vflag = TRUE;
		case 'v':
		    vflag = TRUE;
		    break;
		case 'f':
		    fflag = TRUE;
		    break;
		case 'a':
		    aflag = TRUE;
		    break;
		case 'h':
		    hflag = TRUE;
		    object_processing = TRUE;
		    break;
		case 'l':
		    lflag = TRUE;
		    object_processing = TRUE;
		    break;
		case 'L':
		    Lflag = TRUE;
		    object_processing = TRUE;
		    break;
		case 'D':
		    Dflag = TRUE;
		    object_processing = TRUE;
		    break;
		case 't':
		    tflag = TRUE;
		    object_processing = TRUE;
		    break;
		case 'd':
		    dflag = TRUE;
		    object_processing = TRUE;
		    break;
		case 'o':
		    oflag = TRUE;
		    object_processing = TRUE;
		    break;
		case 'O':
		    Oflag = TRUE;
		    object_processing = TRUE;
		    break;
		case 'r':
		    rflag = TRUE;
		    object_processing = TRUE;
		    break;
		case 'T':
		    Tflag = TRUE;
		    object_processing = TRUE;
		    break;
		case 'M':
		    Mflag = TRUE;
		    object_processing = TRUE;
		    break;
		case 'R':
		    Rflag = TRUE;
		    object_processing = TRUE;
		    break;
		case 'I':
		    Iflag = TRUE;
		    object_processing = TRUE;
		    break;
		case 'H':
		    Hflag = TRUE;
		    object_processing = TRUE;
		    break;
		case 'S':
		    Sflag = TRUE;
		    break;
		case 'c':
		    cflag = TRUE;
		    object_processing = TRUE;
		    break;
		case 'i':
		    iflag = TRUE;
		    object_processing = TRUE;
		    break;
		case 'W':
		    Wflag = TRUE;
		    break;
		case 'X':
		    Xflag = TRUE;
		    break;
		case 'Z':
		    Zflag = TRUE;
		    break;
		case 'm':
		    use_member_syntax = FALSE;
		    break;
		case 'B':
		    Bflag = TRUE;
		    break;
		default:
		    error("unknown char `%c' in flag %s\n", argv[i][j],argv[i]);
		    usage();
		}
	    }
	}

	/*
	 * Check for correctness of arguments.
	 */
	if(!fflag && !aflag && !hflag && !lflag && !Lflag && !tflag && !dflag &&
	   !oflag && !Oflag && !rflag && !Tflag && !Mflag && !Rflag && !Iflag &&
	   !Hflag && !Sflag && !cflag && !iflag && !Dflag && !segname){
	    error("one of -fahlLtdoOrTMRIHScis must be specified");
	    usage();
	}
	if(nfiles == 0){
	    error("at least one file must be specified");
	    usage();
	}
	if(segname != NULL && sectname != NULL){
	    /* treat "-s __TEXT __text" the same as -t */
	    if(strcmp(segname, SEG_TEXT) == 0 &&
	       strcmp(sectname, SECT_TEXT) == 0){
		tflag = TRUE;
		segname = NULL;
		sectname = NULL;
	    }
	    /* treat "-s __TEXT __fvmlib0" the same as -i */
	    else if(strcmp(segname, SEG_TEXT) == 0 &&
	       strcmp(sectname, SECT_FVMLIB_INIT0) == 0){
		iflag = TRUE;
		segname = NULL;
		sectname = NULL;
	    }
	}

	for(j = 0; j < nfiles; j++){
	    ofile_process(files[j], arch_flags, narch_flags, all_archs, TRUE,
			  TRUE, use_member_syntax, processor, NULL);
	}

	if(errors)
	    return(EXIT_FAILURE);
	else
	    return(EXIT_SUCCESS);
}

/*
 * Print the current usage message.
 */
static
void
usage(
void)
{
	fprintf(stderr,
		"Usage: %s [-fahlLDtdorSTMRIHvVcXm] <object file> ...\n",
		progname);

	fprintf(stderr, "\t-f print the fat headers\n");
	fprintf(stderr, "\t-a print the archive header\n");
	fprintf(stderr, "\t-h print the mach header\n");
	fprintf(stderr, "\t-l print the load commands\n");
	fprintf(stderr, "\t-L print shared libraries used\n");
	fprintf(stderr, "\t-D print shared library id name\n");
	fprintf(stderr, "\t-t print the text section (disassemble with -v)\n");
	fprintf(stderr, "\t-p <routine name>  start dissassemble from routine "
		"name\n");
	fprintf(stderr, "\t-s <segname> <sectname> print contents of "
		"section\n");
	fprintf(stderr, "\t-d print the data section\n");
	fprintf(stderr, "\t-o print the Objective-C segment\n");
	fprintf(stderr, "\t-r print the relocation entries\n");
	fprintf(stderr, "\t-S print the table of contents of a library\n");
	fprintf(stderr, "\t-T print the table of contents of a dynamic "
		"shared library\n");
	fprintf(stderr, "\t-M print the module table of a dynamic shared "
		"library\n");
	fprintf(stderr, "\t-R print the reference table of a dynamic shared "
		"library\n");
	fprintf(stderr, "\t-I print the indirect symbol table\n");
	fprintf(stderr, "\t-H print the two-level hints table\n");
	fprintf(stderr, "\t-v print verbosely (symbolically) when possible\n");
	fprintf(stderr, "\t-V print disassembled operands symbolically\n");
	fprintf(stderr, "\t-c print argument strings of a core file\n");
	fprintf(stderr, "\t-X print no leading addresses or headers\n");
	fprintf(stderr, "\t-m don't use archive(member) syntax\n");
	fprintf(stderr, "\t-B force Thumb disassembly (ARM objects only)\n");
	exit(EXIT_FAILURE);
}

static
void
processor(
struct ofile *ofile,
char *arch_name,
void *cookie) /* cookie is not used */
{
    char *addr;
    uint32_t i, size, magic;
    struct mach_header mh;
    struct mach_header_64 mh64;
    cpu_type_t mh_cputype;
    cpu_subtype_t mh_cpusubtype;
    uint32_t mh_magic, mh_filetype, mh_ncmds, mh_sizeofcmds, sizeof_mach_header;
    struct load_command *load_commands;
    uint32_t nsymbols, nsorted_symbols, strings_size, len;
    struct nlist *symbols, *allocated_symbols;
    struct nlist_64 *symbols64, *allocated_symbols64;
    struct symbol *sorted_symbols;
    char *strings, *p;
    uint32_t n_strx;
    uint8_t n_type;
    uint16_t n_desc;
    uint64_t n_value;
    char *sect;
    uint32_t sect_nrelocs, sect_flags, nrelocs, next_relocs, nloc_relocs;
    uint64_t sect_addr, sect_size;
    struct relocation_info *sect_relocs, *relocs, *ext_relocs, *loc_relocs;
    uint32_t *indirect_symbols, *allocated_indirect_symbols;
    uint32_t nindirect_symbols;
    struct dylib_module *mods, *allocated_mods;
    struct dylib_module_64 *mods64, *allocated_mods64;
    struct dylib_table_of_contents *tocs, *allocated_tocs;
    struct dylib_reference *refs, *allocated_refs;
    uint32_t nmods, ntocs, nrefs;
    struct twolevel_hint *hints, *allocated_hints;
    uint32_t nhints;

	sorted_symbols = NULL;
	nsorted_symbols = 0;
	indirect_symbols = NULL;
	nindirect_symbols = 0;
	hints = NULL;
	nhints = 0;
	symbols = NULL;
	symbols64 = NULL;
	nsymbols = 0;
	strings = NULL;
	nmods = 0;
	mods64 = NULL;
	mods = NULL;
	/*
	 * These may or may not be allocated.  If allocated they will not be
	 * NULL and then free'ed before returning.
	 */
	load_commands = NULL;
	allocated_symbols = NULL;
	allocated_symbols64 = NULL;
	sorted_symbols = NULL;
	allocated_indirect_symbols = NULL;
	allocated_tocs = NULL;
	allocated_mods = NULL;
	allocated_refs = NULL;
	allocated_hints = NULL;

	/*
	 * The fat headers are printed in ofile_map() in ofile.c #ifdef'ed
	 * OTOOL.
	 */

	/*
	 * Archive headers.
	 */
	if(aflag && ofile->member_ar_hdr != NULL)
	    print_ar_hdr(ofile->member_ar_hdr, ofile->member_name,
			 ofile->member_name_size, vflag);

	/*
	 * Archive table of contents.
	 */
	if(ofile->member_ar_hdr != NULL &&
	   strncmp(ofile->member_name, SYMDEF, sizeof(SYMDEF)-1) == 0){
	    if(Sflag == FALSE)
		return;
	    if(ofile->file_type == OFILE_FAT){
		addr = ofile->file_addr + ofile->fat_archs[ofile->narch].offset;
		size = ofile->fat_archs[ofile->narch].size;
	    }
	    else{
		addr = ofile->file_addr;
		size = ofile->file_size;
	    }
	    if(addr + size > ofile->file_addr + ofile->file_size)
		size = (ofile->file_addr + ofile->file_size) - addr;
	    print_library_toc(ofile->member_ar_hdr, /* toc_ar_hdr */
			      ofile->member_name, /* toc_name */
			      ofile->member_name_size, /* toc_name_size */
			      ofile->member_addr, /* toc_addr */
			      ofile->member_size, /* toc_size */
			      get_toc_byte_sex(addr, size),
			      ofile->file_name, /* library_name */
			      addr, /* library_addr */
			      size, /* library_size */
			      arch_name,
			      vflag);
	    return;
	}

	if(object_processing == FALSE)
	    return;

	/*
	 * Print header for the object name if in an archive or an architecture
	 * name is passed in.
	 */
	if(Xflag == FALSE){
	    printf("%s", ofile->file_name);
	    if(ofile->member_ar_hdr != NULL){
		printf("(%.*s)", (int)ofile->member_name_size,
			ofile->member_name);
	    }
	    if(arch_name != NULL)
		printf(" (architecture %s):", arch_name);
	    else
		printf(":");
	    /*
	     * If the mach_header pointer is NULL the file is not an object
	     * file.  Truncated object file (where the file size is less
	     * than sizeof(struct mach_header) also does not have it's
	     * mach_header set.  So deal with both cases here and then
	     * return as the rest of this routine deals only with things
	     * in object files.
	     */
	    if(ofile->mh == NULL && ofile->mh64 == NULL){
		if(ofile->file_type == OFILE_FAT){
		    /*
		     * This routine is not called on fat files where the
		     * offset is past end of file.  An error message is
		     * printed in ofile_specific_arch() in ofile.c.
		     */
		    if(ofile->arch_type == OFILE_ARCHIVE){
			addr = ofile->member_addr;
			size = ofile->member_size;
		    }
		    else{
			addr = ofile->file_addr +
			       ofile->fat_archs[ofile->narch].offset;
			size = ofile->fat_archs[ofile->narch].size;
		    }
		    if(addr + size > ofile->file_addr + ofile->file_size)
			size = (ofile->file_addr + ofile->file_size) - addr;
		}
		else if(ofile->file_type == OFILE_ARCHIVE){
		    addr = ofile->member_addr;
		    size = ofile->member_size;
		}
		else{ /* ofile->file_type == OFILE_UNKNOWN */
		    addr = ofile->file_addr;
		    size = ofile->file_size;
		}
		if(size > sizeof(int32_t)){
		    memcpy(&magic, addr, sizeof(uint32_t));
		    if(magic == MH_MAGIC ||
		       magic == SWAP_INT(MH_MAGIC)){
			printf(" is a truncated object file\n");
			memset(&mh, '\0', sizeof(struct mach_header));
			if(size > sizeof(struct mach_header))
			    size = sizeof(struct mach_header);
			memcpy(&mh, addr, size);
			if(magic == SWAP_INT(MH_MAGIC))
			    swap_mach_header(&mh, get_host_byte_sex());
			if(hflag)
			    print_mach_header(mh.magic, mh.cputype,
				mh.cpusubtype, mh.filetype, mh.ncmds,
				mh.sizeofcmds, mh.flags, vflag);
			return;
		    }
		    else if(magic == MH_MAGIC_64 ||
		            magic == SWAP_INT(MH_MAGIC_64)){
			printf(" is a truncated object file\n");
			memset(&mh64, '\0', sizeof(struct mach_header_64));
			if(size > sizeof(struct mach_header_64))
			    size = sizeof(struct mach_header_64);
			memcpy(&mh64, addr, size);
			if(magic == SWAP_INT(MH_MAGIC_64))
			    swap_mach_header_64(&mh64, get_host_byte_sex());
			if(hflag)
			    print_mach_header(mh64.magic, mh64.cputype,
				mh64.cpusubtype, mh64.filetype, mh64.ncmds,
				mh64.sizeofcmds, mh64.flags, vflag);
			return;
		    }
		}
		printf(" is not an object file\n");
		return;
	    }
	}
	if(ofile->mh != NULL){
	    if((intptr_t)(ofile->mh) % sizeof(uint32_t)){
		if(Xflag == FALSE)
		    printf("(object file offset is not a multiple of sizeof("
			   "uint32_t))");
		memcpy(&mh, ofile->mh, sizeof(struct mach_header));
		if(mh.magic == SWAP_INT(MH_MAGIC))
		    swap_mach_header(&mh, get_host_byte_sex());
		ofile->mh = &mh;
	    }
	    else if(ofile->mh->magic == SWAP_INT(MH_MAGIC)){
		mh = *(ofile->mh);
		swap_mach_header(&mh, get_host_byte_sex());
		ofile->mh = &mh;
	    }
	}
	else if(ofile->mh64 != NULL){
	    if((intptr_t)(ofile->mh64) % sizeof(uint32_t)){
		if(Xflag == FALSE)
		    printf("(object file offset is not a multiple of sizeof("
			   "uint32_t))");
		memcpy(&mh64, ofile->mh64, sizeof(struct mach_header));
		if(mh64.magic == SWAP_INT(MH_MAGIC_64))
		    swap_mach_header_64(&mh64, get_host_byte_sex());
		ofile->mh64 = &mh64;
	    }
	    else if(ofile->mh64->magic == SWAP_INT(MH_MAGIC_64)){
		mh64 = *(ofile->mh64);
		swap_mach_header_64(&mh64, get_host_byte_sex());
		ofile->mh64 = &mh64;
	    }
	}
	if(Xflag == FALSE)
	    printf("\n");

	/*
	 * If this is not an object file then just return.
	 */
	if(ofile->mh == NULL && ofile->mh64 == NULL)
	    return;

	/*
	 * Calculate the true number of bytes of the of the object file that
	 * is in memory (in case this file is truncated).
	 */
	addr = ofile->object_addr;
	size = ofile->object_size;
	if(addr + size > ofile->file_addr + ofile->file_size)
	    size = (ofile->file_addr + ofile->file_size) - addr;

	/*
	 * Assign some local variables to the values in the mach_header for this
	 * ofile to make passing arguments to the print routines easier.
	 */
	if(ofile->mh != NULL){
	    mh_magic = ofile->mh->magic;
	    mh_cputype = ofile->mh->cputype;
	    mh_cpusubtype = ofile->mh->cpusubtype;
	    mh_filetype = ofile->mh->filetype;
	    mh_ncmds = ofile->mh->ncmds;
	    mh_sizeofcmds = ofile->mh->sizeofcmds;
	    sizeof_mach_header = sizeof(struct mach_header);
	}
	else{
	    mh_magic = ofile->mh64->magic;
	    mh_cputype = ofile->mh64->cputype;
	    mh_cpusubtype = ofile->mh64->cpusubtype;
	    mh_filetype = ofile->mh64->filetype;
	    mh_ncmds = ofile->mh64->ncmds;
	    mh_sizeofcmds = ofile->mh64->sizeofcmds;
	    sizeof_mach_header = sizeof(struct mach_header_64);
	}

	/*
	 * Mach header.
	 */
	if(hflag){
	    if(ofile->mh != NULL)
		print_mach_header(ofile->mh->magic, ofile->mh->cputype,
				  ofile->mh->cpusubtype, ofile->mh->filetype,
				  ofile->mh->ncmds, ofile->mh->sizeofcmds,
				  ofile->mh->flags, vflag);
	    else
		print_mach_header(ofile->mh64->magic, ofile->mh64->cputype,
				  ofile->mh64->cpusubtype,ofile->mh64->filetype,
				  ofile->mh64->ncmds, ofile->mh64->sizeofcmds,
				  ofile->mh64->flags, vflag);
	}

	/*
	 * Load commands.
	 */
	if(mh_sizeofcmds + sizeof_mach_header > size){
	    load_commands = allocate(mh_sizeofcmds);
	    memset(load_commands, '\0', mh_sizeofcmds);
	    memcpy(load_commands, ofile->load_commands, 
		   size - sizeof_mach_header);
	    ofile->load_commands = load_commands;
	}
	if(lflag)
	    print_loadcmds(ofile->load_commands, mh_ncmds, mh_sizeofcmds,
			   mh_cputype, mh_filetype, ofile->object_byte_sex,
			   size, vflag, Vflag);

	if(Lflag || Dflag)
	    print_libraries(ofile->load_commands, mh_ncmds, mh_sizeofcmds,
			    ofile->object_byte_sex, (Dflag && !Lflag), vflag);

	/*
	 * If the indicated operation needs the symbol table get it.
	 */
	sect_flags = 0;
	if(segname != NULL && sectname != NULL){
	    (void)get_sect_info(segname, sectname, ofile->load_commands,
			mh_ncmds, mh_sizeofcmds, mh_filetype,
			ofile->object_byte_sex,
			addr, size, &sect, &sect_size, &sect_addr,
			&sect_relocs, &sect_nrelocs, &sect_flags);
	    /*
	     * The MH_DYLIB_STUB format has all section sizes set to zero 
	     * except sections with indirect symbol table entries (so that the
	     * indirect symbol table table entries can be printed, which are
	     * based on the section size).  So if we are being asked to print
	     * the section contents of one of these sections in a MH_DYLIB_STUB
	     * we assume it has been stripped and set the section size to zero.
	     */
	    if(mh_filetype == MH_DYLIB_STUB &&
	       ((sect_flags & SECTION_TYPE) == S_NON_LAZY_SYMBOL_POINTERS ||
	        (sect_flags & SECTION_TYPE) == S_LAZY_SYMBOL_POINTERS ||
		(sect_flags & SECTION_TYPE) == S_LAZY_DYLIB_SYMBOL_POINTERS ||
	        (sect_flags & SECTION_TYPE) == S_SYMBOL_STUBS))
		sect_size = 0;
	}
	if(Rflag || Mflag)
	    get_symbol_table_info(ofile->load_commands, mh_ncmds, mh_sizeofcmds,
		mh_cputype, ofile->object_byte_sex, addr, size, &symbols,
		&symbols64, &nsymbols, &strings, &strings_size);
	if(vflag && (rflag || Tflag || Mflag || Rflag || Iflag || Hflag || tflag
	   || iflag || oflag ||
	   (sect_flags & SECTION_TYPE) == S_LITERAL_POINTERS ||
	   (sect_flags & SECTION_TYPE) == S_MOD_INIT_FUNC_POINTERS ||
	   (sect_flags & SECTION_TYPE) == S_MOD_TERM_FUNC_POINTERS ||
	   (sect_flags & S_ATTR_PURE_INSTRUCTIONS) ==
		S_ATTR_PURE_INSTRUCTIONS ||
	   (sect_flags & S_ATTR_SOME_INSTRUCTIONS) ==
		S_ATTR_SOME_INSTRUCTIONS ||
	   segname != NULL)){
	    get_symbol_table_info(ofile->load_commands, mh_ncmds, mh_sizeofcmds,
	        mh_cputype, ofile->object_byte_sex, addr, size, &symbols, 
		&symbols64, &nsymbols, &strings, &strings_size);

	    if(symbols != NULL){
		if((uintptr_t)symbols % sizeof(uint32_t) ||
		   ofile->object_byte_sex != get_host_byte_sex()){
		    allocated_symbols =
			allocate(nsymbols * sizeof(struct nlist));
		    memcpy(allocated_symbols, symbols,
			   nsymbols * sizeof(struct nlist));
		    symbols = allocated_symbols;
		}
		if(ofile->object_byte_sex != get_host_byte_sex())
		    swap_nlist(symbols, nsymbols, get_host_byte_sex());
	    }
	    else{
		if((uintptr_t)symbols64 % sizeof(uint32_t) ||
		   ofile->object_byte_sex != get_host_byte_sex()){
		    allocated_symbols64 =
			allocate(nsymbols * sizeof(struct nlist_64));
		    memcpy(allocated_symbols64, symbols64,
			   nsymbols * sizeof(struct nlist_64));
		    symbols64 = allocated_symbols64;
		}
		if(ofile->object_byte_sex != get_host_byte_sex())
		    swap_nlist_64(symbols64, nsymbols, get_host_byte_sex());
	    }

	    /*
	     * If the operation needs a sorted symbol table create it.
	     */
	    if(tflag || iflag || oflag || 
	       (((sect_flags & SECTION_TYPE) == S_MOD_INIT_FUNC_POINTERS ||
	         (sect_flags & SECTION_TYPE) == S_MOD_TERM_FUNC_POINTERS) &&
		  Vflag) ||
	       (sect_flags & S_ATTR_PURE_INSTRUCTIONS) ==
		    S_ATTR_PURE_INSTRUCTIONS ||
	       (sect_flags & S_ATTR_SOME_INSTRUCTIONS) ==
		    S_ATTR_SOME_INSTRUCTIONS){
		sorted_symbols = allocate(nsymbols * sizeof(struct symbol));
		nsorted_symbols = 0;
		for(i = 0; i < nsymbols; i++){
		    if(symbols != NULL){
			n_strx = symbols[i].n_un.n_strx;
			n_type = symbols[i].n_type;
			n_desc = symbols[i].n_desc;
			n_value = symbols[i].n_value;
		    }
		    else{
			n_strx = symbols64[i].n_un.n_strx;
			n_type = symbols64[i].n_type;
			n_desc = symbols64[i].n_desc;
			n_value = symbols64[i].n_value;
		    }
		    if(n_strx > 0 && n_strx < strings_size)
			p = strings + n_strx;
		    else
			p = "symbol with bad string index";
		    if(n_type & ~(N_TYPE|N_EXT|N_PEXT))
			continue;
		    n_type = n_type & N_TYPE;
		    if(n_type == N_ABS || n_type == N_SECT){
			len = strlen(p);
			if(len > sizeof(".o") - 1 &&
			   strcmp(p + (len - (sizeof(".o") - 1)), ".o") == 0)
			    continue;
			if(strcmp(p, "gcc_compiled.") == 0)
			    continue;
			if(n_type == N_ABS && n_value == 0 && *p == '.')
			    continue;
			sorted_symbols[nsorted_symbols].n_value = n_value;
			sorted_symbols[nsorted_symbols].name = p;
			sorted_symbols[nsorted_symbols].is_thumb =
			    n_desc & N_ARM_THUMB_DEF;
			nsorted_symbols++;
		    }
		}
		qsort(sorted_symbols, nsorted_symbols, sizeof(struct symbol),
		      (int (*)(const void *, const void *))sym_compare);
	    }
	}

	if(Mflag || Tflag || Rflag){
	    get_module_table_info(ofile->load_commands, mh_ncmds, mh_sizeofcmds,
		mh_cputype, ofile->object_byte_sex, addr, size, &mods, &mods64,
		&nmods);
	    if(mods != NULL){
		if((intptr_t)mods % sizeof(uint32_t) ||
		   ofile->object_byte_sex != get_host_byte_sex()){
		    allocated_mods = allocate(nmods *
					      sizeof(struct dylib_module));
		    memcpy(allocated_mods, mods,
			   nmods * sizeof(struct dylib_module));
		    mods = allocated_mods;
		}
		if(ofile->object_byte_sex != get_host_byte_sex())
		    swap_dylib_module(mods, nmods, get_host_byte_sex());
	    }
	    if(mods64 != NULL){
		if((intptr_t)mods64 % sizeof(uint64_t) ||
		   ofile->object_byte_sex != get_host_byte_sex()){
		    allocated_mods64 = allocate(nmods *
					        sizeof(struct dylib_module_64));
		    memcpy(allocated_mods64, mods64,
			   nmods * sizeof(struct dylib_module_64));
		    mods64 = allocated_mods64;
		}
		if(ofile->object_byte_sex != get_host_byte_sex())
		    swap_dylib_module_64(mods64, nmods, get_host_byte_sex());
	    }
	}

	if(Tflag){
	    get_toc_info(ofile->load_commands, mh_ncmds, mh_sizeofcmds,
		ofile->object_byte_sex, addr, size, &tocs, &ntocs);
	    if((intptr_t)tocs % sizeof(uint32_t) ||
	       ofile->object_byte_sex != get_host_byte_sex()){
		allocated_tocs = allocate(ntocs *
					sizeof(struct dylib_table_of_contents));
		memcpy(allocated_tocs, tocs,
		       ntocs * sizeof(struct dylib_table_of_contents));
		tocs = allocated_tocs;
	    }
	    if(ofile->object_byte_sex != get_host_byte_sex())
		swap_dylib_table_of_contents(tocs, ntocs, get_host_byte_sex());
	    print_toc(ofile->load_commands, mh_ncmds, mh_sizeofcmds,
		ofile->object_byte_sex, addr, size, tocs, ntocs, mods, mods64,
		nmods, symbols, symbols64, nsymbols, strings, strings_size,
		vflag);
	}

	if(Mflag){
	    if(mods != NULL)
		print_module_table(mods, nmods, strings, strings_size, vflag);
	    else
		print_module_table_64(mods64, nmods, strings, strings_size,
				      vflag);
	}

	if(Rflag){
	    get_ref_info(ofile->load_commands, mh_ncmds, mh_sizeofcmds,
		ofile->object_byte_sex, addr, size, &refs, &nrefs);
	    if((intptr_t)refs % sizeof(uint32_t) ||
	       ofile->object_byte_sex != get_host_byte_sex()){
		allocated_refs = allocate(nrefs *
					sizeof(struct dylib_reference));
		memcpy(allocated_refs, refs,
		       nrefs * sizeof(struct dylib_reference));
		refs = allocated_refs;
	    }
	    if(ofile->object_byte_sex != get_host_byte_sex())
		swap_dylib_reference(refs, nrefs, get_host_byte_sex());
	    print_refs(refs, nrefs, mods, mods64, nmods, symbols, symbols64,
		       nsymbols, strings, strings_size, vflag);
	}

	if(Iflag || (tflag && vflag)){
	    get_indirect_symbol_table_info(ofile->load_commands, mh_ncmds,
		mh_sizeofcmds, ofile->object_byte_sex, addr, size,
		&indirect_symbols, &nindirect_symbols);
	    if((intptr_t)indirect_symbols % sizeof(uint32_t) ||
	       ofile->object_byte_sex != get_host_byte_sex()){
		allocated_indirect_symbols = allocate(nindirect_symbols *
						     sizeof(uint32_t));
		memcpy(allocated_indirect_symbols, indirect_symbols,
		       nindirect_symbols * sizeof(uint32_t));
		indirect_symbols = allocated_indirect_symbols;
	    }
	    if(ofile->object_byte_sex != get_host_byte_sex())
		swap_indirect_symbols(indirect_symbols, nindirect_symbols,
				      get_host_byte_sex());
	}
	if(Hflag){
	    get_hints_table_info(ofile->load_commands, mh_ncmds, mh_sizeofcmds,
		ofile->object_byte_sex, addr, size, &hints, &nhints);
	    if((intptr_t)hints % sizeof(uint32_t) ||
	       ofile->object_byte_sex != get_host_byte_sex()){
		allocated_hints = allocate(nhints *
					   sizeof(struct twolevel_hint));
		memcpy(allocated_hints, hints,
		       nhints * sizeof(struct twolevel_hint));
		hints = allocated_hints;
	    }
	    if(ofile->object_byte_sex != get_host_byte_sex())
		swap_twolevel_hint(hints, nhints, get_host_byte_sex());
	    print_hints(ofile->load_commands, mh_ncmds, mh_sizeofcmds,
		ofile->object_byte_sex, hints, nhints, symbols, symbols64,
		nsymbols, strings, strings_size, vflag);
	}
	if(Iflag)
	    print_indirect_symbols(ofile->load_commands, mh_ncmds,mh_sizeofcmds,
		mh_cputype, ofile->object_byte_sex, indirect_symbols,
		nindirect_symbols, symbols, symbols64, nsymbols, strings,
		strings_size, vflag);

	if(rflag)
	    print_reloc(ofile->load_commands, mh_ncmds, mh_sizeofcmds,
			mh_cputype, ofile->object_byte_sex, addr, size, symbols,
		        symbols64, nsymbols, strings, strings_size, vflag);

	if(tflag ||
	   (sect_flags & S_ATTR_PURE_INSTRUCTIONS) ==
		S_ATTR_PURE_INSTRUCTIONS ||
	   (sect_flags & S_ATTR_SOME_INSTRUCTIONS) ==
		S_ATTR_SOME_INSTRUCTIONS){
	    if(tflag)
		(void)get_sect_info(SEG_TEXT, SECT_TEXT, ofile->load_commands,
		    mh_ncmds, mh_sizeofcmds, mh_filetype,
		    ofile->object_byte_sex,
		    addr, size, &sect, &sect_size, &sect_addr,
		    &sect_relocs, &sect_nrelocs, &sect_flags);

	    /* create aligned relocations entries as needed */
	    relocs = NULL;
	    nrelocs = 0;
	    if(Vflag){
		if((intptr_t)sect_relocs % sizeof(int32_t) != 0 ||
		   ofile->object_byte_sex != get_host_byte_sex()){
		    nrelocs = sect_nrelocs;
		    relocs = allocate(nrelocs *
				      sizeof(struct relocation_info));
		    memcpy(relocs, sect_relocs, nrelocs *
			   sizeof(struct relocation_info));
		}
		else{
		    nrelocs = sect_nrelocs;
		    relocs = sect_relocs;
		}
		if(ofile->object_byte_sex != get_host_byte_sex())
		    swap_relocation_info(relocs, nrelocs,
					 get_host_byte_sex());
	    }
	    if(Xflag == FALSE){
		if(tflag)
		    printf("(%s,%s) section\n", SEG_TEXT, SECT_TEXT);
		else
		    printf("Contents of (%.16s,%.16s) section\n", segname,
			   sectname);
	    }
	    print_text(mh_cputype, ofile->object_byte_sex,
		       sect, sect_size, sect_addr, sorted_symbols,
		       nsorted_symbols, symbols, symbols64, nsymbols, strings,
		       strings_size, relocs, nrelocs, indirect_symbols,
		       nindirect_symbols, ofile->load_commands, mh_ncmds,
		       mh_sizeofcmds, vflag, Vflag, mh_cpusubtype);

	    if(relocs != NULL && relocs != sect_relocs)
		free(relocs);
	}

	if(iflag){
	    if(get_sect_info(SEG_TEXT, SECT_FVMLIB_INIT0, ofile->load_commands,
		mh_ncmds, mh_sizeofcmds, mh_filetype, ofile->object_byte_sex,
		addr, size, &sect, &sect_size, &sect_addr,
		&sect_relocs, &sect_nrelocs, &sect_flags) == TRUE){

		/* create aligned, sorted relocations entries */
		nrelocs = sect_nrelocs;
		relocs = allocate(nrelocs * sizeof(struct relocation_info));
		memcpy(relocs, sect_relocs, nrelocs *
		       sizeof(struct relocation_info));
		if(ofile->object_byte_sex != get_host_byte_sex())
		    swap_relocation_info(relocs, nrelocs, get_host_byte_sex());
		qsort(relocs, nrelocs, sizeof(struct relocation_info),
		      (int (*)(const void *, const void *))rel_compare);

		if(Xflag == FALSE)
		    printf("Shared library initialization (%s,%s) section\n",
			   SEG_TEXT, SECT_FVMLIB_INIT0);
		print_shlib_init(ofile->object_byte_sex, sect, sect_size,
			sect_addr, sorted_symbols, nsorted_symbols, symbols,
			symbols64, nsymbols, strings, strings_size, relocs,
			nrelocs, vflag);
		free(relocs);
	    }
	}

	if(dflag){
	    if(get_sect_info(SEG_DATA, SECT_DATA, ofile->load_commands,
		mh_ncmds, mh_sizeofcmds, mh_filetype, ofile->object_byte_sex,
		addr, size, &sect, &sect_size, &sect_addr,
		&sect_relocs, &sect_nrelocs, &sect_flags) == TRUE){

		if(Xflag == FALSE)
		    printf("(%s,%s) section\n", SEG_DATA, SECT_DATA);
		print_sect(mh_cputype, ofile->object_byte_sex, sect, sect_size,
			   sect_addr);
	    }
	}

	if(segname != NULL && sectname != NULL &&
	   (sect_flags & S_ATTR_PURE_INSTRUCTIONS) !=
		S_ATTR_PURE_INSTRUCTIONS &&
	   (sect_flags & S_ATTR_SOME_INSTRUCTIONS) !=
		S_ATTR_SOME_INSTRUCTIONS){
	    if(strcmp(segname, SEG_OBJC) == 0 &&
	       strcmp(sectname, "__protocol") == 0 && vflag == TRUE){
		print_objc_protocol_section(ofile->load_commands, mh_ncmds,
		   mh_sizeofcmds, ofile->object_byte_sex, ofile->object_addr,
		   ofile->object_size, vflag);
	    }
	    else if(strcmp(segname, SEG_OBJC) == 0 &&
	            (strcmp(sectname, "__string_object") == 0 ||
	             strcmp(sectname, "__cstring_object") == 0) &&
		    vflag == TRUE){
		if(mh_cputype & CPU_ARCH_ABI64)
		    print_objc_string_object_section_64(sectname,
			ofile->load_commands, mh_ncmds, mh_sizeofcmds,
			ofile->object_byte_sex, ofile->object_addr,
			ofile->object_size, mh_cputype, symbols64, nsymbols,
			strings, strings_size, sorted_symbols, nsorted_symbols,
			vflag);
		else
		    print_objc_string_object_section(sectname,
			ofile->load_commands, mh_ncmds, mh_sizeofcmds,
			ofile->object_byte_sex, ofile->object_addr,
			ofile->object_size, vflag);
	    }
	    else if(strcmp(segname, SEG_OBJC) == 0 &&
	       strcmp(sectname, "__runtime_setup") == 0 && vflag == TRUE){
		print_objc_runtime_setup_section(ofile->load_commands, mh_ncmds,
		   mh_sizeofcmds, ofile->object_byte_sex, ofile->object_addr,
		   ofile->object_size, vflag);
	    }
#ifdef EFI_SUPPORT
	    else if(strcmp(segname, "__RELOC") == 0 &&
	       strcmp(sectname, "__reloc") == 0 && vflag == TRUE){
		print_coff_reloc_section(ofile->load_commands, mh_ncmds,
		   mh_sizeofcmds, mh_filetype, ofile->object_byte_sex,
		   ofile->object_addr, ofile->object_size, vflag);
	    }
#endif
	    else if(get_sect_info(segname, sectname, ofile->load_commands,
		mh_ncmds, mh_sizeofcmds, mh_filetype, ofile->object_byte_sex,
		addr, size, &sect, &sect_size, &sect_addr,
		&sect_relocs, &sect_nrelocs, &sect_flags) == TRUE){

		if(Xflag == FALSE)
		    printf("Contents of (%.16s,%.16s) section\n", segname,
			   sectname);

		if(vflag){
		    switch((sect_flags & SECTION_TYPE)){
		    case 0:
			print_sect(mh_cputype, ofile->object_byte_sex,
				   sect, sect_size, sect_addr);
			break;
		    case S_ZEROFILL:
			printf("zerofill section and has no contents in the "
			       "file\n");
			break;
		    case S_CSTRING_LITERALS:
			print_cstring_section(sect, sect_size, sect_addr,
				Xflag == TRUE ? FALSE : TRUE);
			break;
		    case S_4BYTE_LITERALS:
			print_literal4_section(sect, sect_size, sect_addr,
					      ofile->object_byte_sex,
					      Xflag == TRUE ? FALSE : TRUE);
			break;
		    case S_8BYTE_LITERALS:
			print_literal8_section(sect, sect_size, sect_addr,
					      ofile->object_byte_sex,
					      Xflag == TRUE ? FALSE : TRUE);
			break;
		    case S_16BYTE_LITERALS:
			print_literal16_section(sect, sect_size, sect_addr,
					       ofile->object_byte_sex,
					       Xflag == TRUE ? FALSE : TRUE);
			break;
		    case S_LITERAL_POINTERS:
			/* create aligned, sorted relocations entries */
			nrelocs = sect_nrelocs;
			relocs = allocate(nrelocs *
					  sizeof(struct relocation_info));
			memcpy(relocs, sect_relocs, nrelocs *
			       sizeof(struct relocation_info));
			if(ofile->object_byte_sex != get_host_byte_sex())
			    swap_relocation_info(relocs, nrelocs,
					         get_host_byte_sex());
			qsort(relocs, nrelocs, sizeof(struct relocation_info),
			      (int (*)(const void *, const void *))rel_compare);
			print_literal_pointer_section(ofile->load_commands,
				mh_ncmds, mh_sizeofcmds, ofile->object_byte_sex,
				addr, size, sect, sect_size, sect_addr, symbols,
				symbols64, nsymbols, strings, strings_size,
				relocs, nrelocs, Xflag == TRUE ? FALSE : TRUE);
			free(relocs);

			break;

		    case S_MOD_INIT_FUNC_POINTERS:
		    case S_MOD_TERM_FUNC_POINTERS:
			print_init_term_pointer_section(mh_cputype, sect,
			    sect_size, sect_addr, ofile->object_byte_sex,
			    sorted_symbols, nsorted_symbols, Vflag);
			break;

		    default:
			printf("Unknown section type (0x%x)\n",
			       (unsigned int)(sect_flags & SECTION_TYPE));
			print_sect(mh_cputype, ofile->object_byte_sex, sect,
				   sect_size, sect_addr);
			break;
		    }
		}
		else{
		    if((sect_flags & SECTION_TYPE) == S_ZEROFILL)
			printf("zerofill section and has no contents in the "
			       "file\n");
		    else
			print_sect(mh_cputype, ofile->object_byte_sex, sect,
				   sect_size, sect_addr);
		}
	    }
	}

	if(cflag)
	    print_argstrings(mh_magic, ofile->load_commands, mh_ncmds,
			     mh_sizeofcmds, mh_cputype, mh_cpusubtype,
			     ofile->object_byte_sex, ofile->object_addr,
			     ofile->object_size);

	if(oflag){
	    if(mh_cputype & CPU_ARCH_ABI64){
		get_linked_reloc_info(ofile->load_commands, mh_ncmds,
			mh_sizeofcmds, ofile->object_byte_sex,
			ofile->object_addr, ofile->object_size, &ext_relocs,
			&next_relocs, &loc_relocs, &nloc_relocs);
		/* create aligned relocations entries as needed */
		relocs = NULL;
		nrelocs = 0;
		if((intptr_t)ext_relocs % sizeof(int32_t) != 0 ||
		   ofile->object_byte_sex != get_host_byte_sex()){
		    relocs = allocate(next_relocs *
				      sizeof(struct relocation_info));
		    memcpy(relocs, ext_relocs, next_relocs *
			   sizeof(struct relocation_info));
		    ext_relocs = relocs;
		}
		if((intptr_t)loc_relocs % sizeof(int32_t) != 0 ||
		   ofile->object_byte_sex != get_host_byte_sex()){
		    relocs = allocate(nloc_relocs *
				      sizeof(struct relocation_info));
		    memcpy(relocs, loc_relocs, nloc_relocs *
			   sizeof(struct relocation_info));
		    loc_relocs = relocs;
		}
		if(ofile->object_byte_sex != get_host_byte_sex()){
		    swap_relocation_info(ext_relocs, next_relocs,
					 get_host_byte_sex());
		    swap_relocation_info(loc_relocs, nloc_relocs,
					 get_host_byte_sex());
		}
		print_objc2_64bit(mh_cputype, ofile->load_commands, mh_ncmds,
			    mh_sizeofcmds, ofile->object_byte_sex,
			    ofile->object_addr, ofile->object_size, symbols64,
			    nsymbols, strings, strings_size, sorted_symbols,
			    nsorted_symbols, ext_relocs, next_relocs,
			    loc_relocs, nloc_relocs, vflag, Vflag);
	    }
	    else if(mh_cputype == CPU_TYPE_ARM){
		get_linked_reloc_info(ofile->load_commands, mh_ncmds,
			mh_sizeofcmds, ofile->object_byte_sex,
			ofile->object_addr, ofile->object_size, &ext_relocs,
			&next_relocs, &loc_relocs, &nloc_relocs);
		/* create aligned relocations entries as needed */
		relocs = NULL;
		nrelocs = 0;
		if((intptr_t)ext_relocs % sizeof(int32_t) != 0 ||
		   ofile->object_byte_sex != get_host_byte_sex()){
		    relocs = allocate(next_relocs *
				      sizeof(struct relocation_info));
		    memcpy(relocs, ext_relocs, next_relocs *
			   sizeof(struct relocation_info));
		    ext_relocs = relocs;
		}
		if((intptr_t)loc_relocs % sizeof(int32_t) != 0 ||
		   ofile->object_byte_sex != get_host_byte_sex()){
		    relocs = allocate(nloc_relocs *
				      sizeof(struct relocation_info));
		    memcpy(relocs, loc_relocs, nloc_relocs *
			   sizeof(struct relocation_info));
		    loc_relocs = relocs;
		}
		if(ofile->object_byte_sex != get_host_byte_sex()){
		    swap_relocation_info(ext_relocs, next_relocs,
					 get_host_byte_sex());
		    swap_relocation_info(loc_relocs, nloc_relocs,
					 get_host_byte_sex());
		}
		print_objc2_32bit(mh_cputype, ofile->load_commands, mh_ncmds,
			    mh_sizeofcmds, ofile->object_byte_sex,
			    ofile->object_addr, ofile->object_size, symbols,
			    nsymbols, strings, strings_size, sorted_symbols,
			    nsorted_symbols, ext_relocs, next_relocs,
			    loc_relocs, nloc_relocs, vflag);
	    }
	    else{
		 print_objc_segment(ofile->load_commands,mh_ncmds,mh_sizeofcmds,
				    ofile->object_byte_sex, ofile->object_addr,
				    ofile->object_size, sorted_symbols,
				    nsorted_symbols, vflag);
	    }
	}

	if(load_commands != NULL)
	    free(load_commands);
	if(allocated_symbols != NULL)
	    free(allocated_symbols);
	if(sorted_symbols != NULL)
	    free(sorted_symbols);
	if(allocated_indirect_symbols != NULL)
	    free(allocated_indirect_symbols);
	if(allocated_hints != NULL)
	    free(allocated_hints);
	if(allocated_tocs != NULL)
	    free(allocated_tocs);
	if(allocated_mods != NULL)
	    free(allocated_mods);
	if(allocated_refs != NULL)
	    free(allocated_refs);
}

/*
 * get_symbol_table_info() returns pointers to the symbol table and string
 * table as well as the number of symbols and size of the string table.
 * This routine handles the problems related to the file being truncated and
 * only returns valid pointers and sizes that can be used.  This routine will
 * return pointers that are misaligned and it is up to the caller to deal with
 * alignment issues.  It is also up to the caller to deal with byte sex of the
 * the symbol table.
 */
static
void
get_symbol_table_info(
struct load_command *load_commands,	/* input */
uint32_t ncmds,
uint32_t sizeofcmds,
cpu_type_t cputype,
enum byte_sex load_commands_byte_sex,
char *object_addr,
uint32_t object_size,
struct nlist **symbols,			/* output */
struct nlist_64 **symbols64,
uint32_t *nsymbols,
char **strings,
uint32_t *strings_size)
{
    enum byte_sex host_byte_sex;
    enum bool swapped;
    uint32_t i, left, size, st_cmd;
    struct load_command *lc, l;
    struct symtab_command st;
    uint64_t bigsize;

	*symbols = NULL;
	*symbols64 = NULL;
	*nsymbols = 0;
	*strings = NULL;
	*strings_size = 0;

	host_byte_sex = get_host_byte_sex();
	swapped = host_byte_sex != load_commands_byte_sex;

	st_cmd = UINT_MAX;
	lc = load_commands;
	memset((char *)&st, '\0', sizeof(struct symtab_command));
	for(i = 0 ; i < ncmds; i++){
	    memcpy((char *)&l, (char *)lc, sizeof(struct load_command));
	    if(swapped)
		swap_load_command(&l, host_byte_sex);
	    if(l.cmdsize % sizeof(int32_t) != 0)
		printf("load command %u size not a multiple of "
		       "sizeof(int32_t)\n", i);
	    if((char *)lc + l.cmdsize >
	       (char *)load_commands + sizeofcmds)
		printf("load command %u extends past end of load "
		       "commands\n", i);
	    left = sizeofcmds - ((char *)lc - (char *)load_commands);

	    switch(l.cmd){
	    case LC_SYMTAB:
		if(st_cmd != UINT_MAX){
		    printf("more than one LC_SYMTAB command (using command %u)"
			   "\n", st_cmd);
		    break;
		}
		size = left < sizeof(struct symtab_command) ?
		       left : sizeof(struct symtab_command);
		memcpy((char *)&st, (char *)lc, size);
		if(swapped)
		    swap_symtab_command(&st, host_byte_sex);
		st_cmd = i;
	    }
	    if(l.cmdsize == 0){
		printf("load command %u size zero (can't advance to other "
		       "load commands)\n", i);
		break;
	    }
	    lc = (struct load_command *)((char *)lc + l.cmdsize);
	    if((char *)lc > (char *)load_commands + sizeofcmds)
		break;
	}
	if((char *)load_commands + sizeofcmds != (char *)lc)
	    printf("Inconsistent sizeofcmds\n");

	if(st_cmd == UINT_MAX){
	    return;
	}

	if(st.symoff >= object_size){
	    printf("symbol table offset is past end of file\n");
	}
	else{
	    if(cputype & CPU_ARCH_ABI64){
		*symbols64 = (struct nlist_64 *)(object_addr + st.symoff);
		bigsize = st.nsyms;
		bigsize *= sizeof(struct nlist_64);
		bigsize += st.symoff; 
		if(bigsize > object_size){
		    printf("symbol table extends past end of file\n");
		    *nsymbols = (object_size - st.symoff) /
			        sizeof(struct nlist_64);
		}
		else
		    *nsymbols = st.nsyms;
	    }
	    else{
		*symbols = (struct nlist *)(object_addr + st.symoff);
		bigsize = st.nsyms;
		bigsize *= sizeof(struct nlist);
		bigsize += st.symoff; 
		if(bigsize > object_size){
		    printf("symbol table extends past end of file\n");
		    *nsymbols = (object_size - st.symoff) /
			        sizeof(struct nlist);
		}
		else
		    *nsymbols = st.nsyms;
	    }
	}

	if(st.stroff >= object_size){
	    printf("string table offset is past end of file\n");
	}
	else{
	    *strings = object_addr + st.stroff;
	    if(st.stroff + st.strsize > object_size){
		printf("string table extends past end of file\n");
		*strings_size = object_size - st.symoff;
	    }
	    else
		*strings_size = st.strsize;
	}
}

/*
 * get_toc_info() returns a pointer and the size of the table of contents.
 * This routine handles the problems related to the file being truncated and
 * only returns valid pointers and sizes that can be used.  This routine will
 * return pointers that are misaligned and it is up to the caller to deal with
 * alignment issues.  It is also up to the caller to deal with byte sex of the
 * table.
 */
static
void
get_toc_info(
struct load_command *load_commands,
uint32_t ncmds,
uint32_t sizeofcmds,
enum byte_sex load_commands_byte_sex,
char *object_addr,
uint32_t object_size,
struct dylib_table_of_contents **tocs,	/* output */
uint32_t *ntocs)
{
    struct dysymtab_command dyst;
    uint64_t bigsize;

	*tocs = NULL;
	*ntocs = 0;

	if(get_dyst(load_commands, ncmds, sizeofcmds, load_commands_byte_sex,
		    &dyst) == FALSE)
	    return;

	if(dyst.tocoff >= object_size){
	    printf("table of contents offset is past end of file\n");
	}
	else{
	    *tocs = (struct dylib_table_of_contents *)(object_addr +
						       dyst.tocoff);
	    bigsize = dyst.ntoc;
	    bigsize *= sizeof(struct dylib_table_of_contents);
	    bigsize += dyst.tocoff;
	    if(bigsize > object_size){
		printf("table of contents extends past end of file\n");
		*ntocs = (object_size - dyst.tocoff) /
				     sizeof(struct dylib_table_of_contents);
	    }
	    else
		*ntocs = dyst.ntoc;
	}
}

/*
 * get_module_table_info() returns a pointer and the size of the
 * module table.  This routine handles the problems related to the file being
 * truncated and only returns valid pointers and sizes that can be used.  This
 * routine will return pointers that are misaligned and it is up to the caller
 * to deal with alignment issues.  It is also up to the caller to deal with
 * byte sex of the table.
 */
static
void
get_module_table_info(
struct load_command *load_commands,
uint32_t ncmds,
uint32_t sizeofcmds,
cpu_type_t cputype,
enum byte_sex load_commands_byte_sex,
char *object_addr,
uint32_t object_size,
struct dylib_module **mods,			/* output */
struct dylib_module_64 **mods64,
uint32_t *nmods)
{
    struct dysymtab_command dyst;
    uint64_t bigsize;

	*mods = NULL;
	*mods64 = NULL;
	*nmods = 0;

	if(get_dyst(load_commands, ncmds, sizeofcmds, load_commands_byte_sex,
		    &dyst) == FALSE)
	    return;

	if(dyst.modtaboff >= object_size){
	    printf("module table offset is past end of file\n");
	}
	else{
	    if(cputype & CPU_ARCH_ABI64){
		*mods64 = (struct dylib_module_64 *)(object_addr +
						     dyst.modtaboff);
		bigsize = dyst.nmodtab;
		bigsize *= sizeof(struct dylib_module_64);
		bigsize += dyst.modtaboff;
		if(bigsize > object_size){
		    printf("module table extends past end of file\n");
		    *nmods = (object_size - dyst.modtaboff) /
					 sizeof(struct dylib_module_64);
		}
		else
		    *nmods = dyst.nmodtab;
	    }
	    else{
		*mods = (struct dylib_module *)(object_addr + dyst.modtaboff);
		if(dyst.modtaboff +
		   dyst.nmodtab * sizeof(struct dylib_module) > object_size){
		    printf("module table extends past end of file\n");
		    *nmods = (object_size - dyst.modtaboff) /
					 sizeof(struct dylib_module);
		}
		else
		    *nmods = dyst.nmodtab;
	    }
	}
}

/*
 * get_ref_info() returns a pointer and the size of the reference table.
 * This routine handles the problems related to the file being truncated and
 * only returns valid pointers and sizes that can be used.  This routine will
 * return pointers that are misaligned and it is up to the caller to deal with
 * alignment issues.  It is also up to the caller to deal with byte sex of the
 * table.
 */
static
void
get_ref_info(
struct load_command *load_commands,
uint32_t ncmds,
uint32_t sizeofcmds,
enum byte_sex load_commands_byte_sex,
char *object_addr,
uint32_t object_size,
struct dylib_reference **refs,		/* output */
uint32_t *nrefs)
{
    struct dysymtab_command dyst;
    uint64_t bigsize;

	*refs = NULL;
	*nrefs = 0;

	if(get_dyst(load_commands, ncmds, sizeofcmds, load_commands_byte_sex,
		    &dyst) == FALSE)
	    return;

	if(dyst.extrefsymoff >= object_size){
	    printf("reference table offset is past end of file\n");
	}
	else{
	    *refs = (struct dylib_reference *)(object_addr + dyst.extrefsymoff);
	    bigsize = dyst.nextrefsyms;
	    bigsize *= sizeof(struct dylib_reference);
	    bigsize += dyst.extrefsymoff;
	    if(bigsize > object_size){
		printf("reference table extends past end of file\n");
		*nrefs = (object_size - dyst.extrefsymoff) /
				     sizeof(struct dylib_reference);
	    }
	    else
		*nrefs = dyst.nextrefsyms;
	}
}

/*
 * get_indirect_symbol_table_info() returns a pointer and the size of the
 * indirect symbol table.  This routine handles the problems related to the
 * file being truncated and only returns valid pointers and sizes that can be
 * used.  This routine will return pointers that are misaligned and it is up to
 * the caller to deal with alignment issues.  It is also up to the caller to
 * deal with byte sex of the table.
 */
static
void
get_indirect_symbol_table_info(
struct load_command *load_commands,
uint32_t ncmds,
uint32_t sizeofcmds,
enum byte_sex load_commands_byte_sex,
char *object_addr,
uint32_t object_size,
uint32_t **indirect_symbols,	/* output */
uint32_t *nindirect_symbols)
{
    struct dysymtab_command dyst;
    uint64_t bigsize;

	*indirect_symbols = NULL;
	*nindirect_symbols = 0;

	if(get_dyst(load_commands, ncmds, sizeofcmds, load_commands_byte_sex,
		    &dyst) == FALSE)
	    return;

	if(dyst.indirectsymoff >= object_size){
	    printf("indirect symbol table offset is past end of file\n");
	}
	else{
	    *indirect_symbols = (uint32_t *)(object_addr + dyst.indirectsymoff);
	    bigsize = dyst.nindirectsyms;
	    bigsize *= sizeof(uint32_t);
	    bigsize += dyst.indirectsymoff;
	    if(bigsize > object_size){
		printf("indirect symbol table extends past end of file\n");
		*nindirect_symbols = (object_size - dyst.indirectsymoff) /
				     sizeof(uint32_t);
	    }
	    else
		*nindirect_symbols = dyst.nindirectsyms;
	}
}

/*
 * get_dyst() gets the dysymtab_command from the mach header and load commands
 * passed to it and copys it into dyst.  It if doesn't find one it returns FALSE
 * else it returns TRUE.
 */
static
enum bool
get_dyst(
struct load_command *load_commands,
uint32_t ncmds,
uint32_t sizeofcmds,
enum byte_sex load_commands_byte_sex,
struct dysymtab_command *dyst)
{
    enum byte_sex host_byte_sex;
    enum bool swapped;
    uint32_t i, left, size, dyst_cmd;
    struct load_command *lc, l;

	host_byte_sex = get_host_byte_sex();
	swapped = host_byte_sex != load_commands_byte_sex;

	dyst_cmd = UINT_MAX;
	lc = load_commands;
	for(i = 0 ; i < ncmds; i++){
	    memcpy((char *)&l, (char *)lc, sizeof(struct load_command));
	    if(swapped)
		swap_load_command(&l, host_byte_sex);
	    if(l.cmdsize % sizeof(int32_t) != 0)
		printf("load command %u size not a multiple of "
		       "sizeof(int32_t)\n", i);
	    if((char *)lc + l.cmdsize >
	       (char *)load_commands + sizeofcmds)
		printf("load command %u extends past end of load "
		       "commands\n", i);
	    left = sizeofcmds - ((char *)lc - (char *)load_commands);

	    switch(l.cmd){
	    case LC_DYSYMTAB:
		if(dyst_cmd != UINT_MAX){
		    printf("more than one LC_DYSYMTAB command (using command "
			   "%u)\n", dyst_cmd);
		    break;
		}
		memset((char *)dyst, '\0', sizeof(struct dysymtab_command));
		size = left < sizeof(struct dysymtab_command) ?
		       left : sizeof(struct dysymtab_command);
		memcpy((char *)dyst, (char *)lc, size);
		if(swapped)
		    swap_dysymtab_command(dyst, host_byte_sex);
		dyst_cmd = i;
	    }
	    if(l.cmdsize == 0){
		printf("load command %u size zero (can't advance to other "
		       "load commands)\n", i);
		break;
	    }
	    lc = (struct load_command *)((char *)lc + l.cmdsize);
	    if((char *)lc > (char *)load_commands + sizeofcmds)
		break;
	}
	if((char *)load_commands + sizeofcmds != (char *)lc)
	    printf("Inconsistent sizeofcmds\n");

	if(dyst_cmd == UINT_MAX){
	    return(FALSE);
	}
	return(TRUE);
}

/*
 * get_hints_table_info() returns a pointer and the size of the two-level hints
 * table.  This routine handles the problems related to the file being truncated
 * and only returns valid pointers and sizes that can be used.  This routine
 * will return pointers that are misaligned and it is up to the caller to deal
 * with alignment issues.  It is also up to the caller to deal with byte sex of
 * the table.
 */
static
void
get_hints_table_info(
struct load_command *load_commands,
uint32_t ncmds,
uint32_t sizeofcmds,
enum byte_sex load_commands_byte_sex,
char *object_addr,
uint32_t object_size,
struct twolevel_hint **hints,	/* output */
uint32_t *nhints)
{
    struct twolevel_hints_command hints_cmd;
    uint64_t bigsize;

	*hints = NULL;
	*nhints = 0;

	memset(&hints_cmd, '\0', sizeof(struct twolevel_hints_command));
	if(get_hints_cmd(load_commands, ncmds, sizeofcmds,
		         load_commands_byte_sex, &hints_cmd) == FALSE)
	    return;

	if(hints_cmd.offset >= object_size){
	    printf("two-level hints offset is past end of file\n");
	}
	else{
	    *hints = (struct twolevel_hint *)(object_addr + hints_cmd.offset);
	    bigsize = hints_cmd.nhints;
	    bigsize *= sizeof(struct twolevel_hint);
	    bigsize += hints_cmd.offset;
	    if(bigsize > object_size){
		printf("two-level hints table extends past end of file\n");
		*nhints = (object_size - hints_cmd.offset) /
			  sizeof(struct twolevel_hint);
	    }
	    else
		*nhints = hints_cmd.nhints;
	}
}

/*
 * get_hints_cmd() gets the twolevel_hints_command from the mach header and
 * load commands passed to it and copys it into hints_cmd.  It if doesn't find
 * one it returns FALSE else it returns TRUE.
 */
static
enum bool
get_hints_cmd(
struct load_command *load_commands,
uint32_t ncmds,
uint32_t sizeofcmds,
enum byte_sex load_commands_byte_sex,
struct twolevel_hints_command *hints_cmd)
{
    enum byte_sex host_byte_sex;
    enum bool swapped;
    uint32_t i, left, size, cmd;
    struct load_command *lc, l;

	host_byte_sex = get_host_byte_sex();
	swapped = host_byte_sex != load_commands_byte_sex;

	cmd = UINT_MAX;
	lc = load_commands;
	for(i = 0 ; i < ncmds; i++){
	    memcpy((char *)&l, (char *)lc, sizeof(struct load_command));
	    if(swapped)
		swap_load_command(&l, host_byte_sex);
	    if(l.cmdsize % sizeof(int32_t) != 0)
		printf("load command %u size not a multiple of "
		       "sizeof(int32_t)\n", i);
	    if((char *)lc + l.cmdsize >
	       (char *)load_commands + sizeofcmds)
		printf("load command %u extends past end of load "
		       "commands\n", i);
	    left = sizeofcmds - ((char *)lc - (char *)load_commands);

	    switch(l.cmd){
	    case LC_TWOLEVEL_HINTS:
		if(cmd != UINT_MAX){
		    printf("more than one LC_TWOLEVEL_HINTS command (using "
			   "command %u)\n", cmd);
		    break;
		}
		memset((char *)hints_cmd, '\0',
		       sizeof(struct twolevel_hints_command));
		size = left < sizeof(struct twolevel_hints_command) ?
		       left : sizeof(struct twolevel_hints_command);
		memcpy((char *)hints_cmd, (char *)lc, size);
		if(swapped)
		    swap_twolevel_hints_command(hints_cmd, host_byte_sex);
		cmd = i;
	    }
	    if(l.cmdsize == 0){
		printf("load command %u size zero (can't advance to other "
		       "load commands)\n", i);
		break;
	    }
	    lc = (struct load_command *)((char *)lc + l.cmdsize);
	    if((char *)lc > (char *)load_commands + sizeofcmds)
		break;
	}
	if((char *)load_commands + sizeofcmds != (char *)lc)
	    printf("Inconsistent sizeofcmds\n");

	if(cmd == UINT_MAX){
	    return(FALSE);
	}
	return(TRUE);
}


/*
 * Function for qsort for comparing symbols.
 */
static
int
sym_compare(
struct symbol *sym1,
struct symbol *sym2)
{
	if(sym1->n_value == sym2->n_value)
	    return(0);
	if(sym1->n_value < sym2->n_value)
	    return(-1);
	else
	    return(1);
}

/*
 * Function for qsort for comparing relocation entries.
 */
static
int
rel_compare(
struct relocation_info *rel1,
struct relocation_info *rel2)
{
    struct scattered_relocation_info *srel;
    uint32_t r_address1, r_address2;

	if((rel1->r_address & R_SCATTERED) != 0){
	    srel = (struct scattered_relocation_info *)rel1;
	    r_address1 = srel->r_address;
	}
	else
	    r_address1 = rel1->r_address;
	if((rel2->r_address & R_SCATTERED) != 0){
	    srel = (struct scattered_relocation_info *)rel2;
	    r_address2 = srel->r_address;
	}
	else
	    r_address2 = rel2->r_address;

	if(r_address1 == r_address2)
	    return(0);
	if(r_address1 < r_address2)
	    return(-1);
	else
	    return(1);
}

enum bool
get_sect_info(
char *segname,				/* input */
char *sectname,
struct load_command *load_commands,
uint32_t ncmds,
uint32_t sizeofcmds,
uint32_t filetype,
enum byte_sex load_commands_byte_sex,
char *object_addr,
uint32_t object_size,
char **sect_pointer,			/* output */
uint64_t *sect_size,
uint64_t *sect_addr,
struct relocation_info **sect_relocs,
uint32_t *sect_nrelocs,
uint32_t *sect_flags)
{
    enum byte_sex host_byte_sex;
    enum bool found, swapped;
    uint32_t i, j, left, size;
    struct load_command *lc, l;
    uint32_t cmd;
    struct segment_command sg;
    struct segment_command_64 sg64;
    struct section s;
    struct section_64 s64;
    char *p;

	*sect_pointer = NULL;
	*sect_size = 0;
	*sect_addr = 0;
	*sect_relocs = NULL;
	*sect_nrelocs = 0;
	*sect_flags = 0;

	found = FALSE;
	cmd = 0;
	host_byte_sex = get_host_byte_sex();
	swapped = host_byte_sex != load_commands_byte_sex;

	lc = load_commands;
	for(i = 0 ; found == FALSE && i < ncmds; i++){
	    memcpy((char *)&l, (char *)lc, sizeof(struct load_command));
	    if(swapped)
		swap_load_command(&l, host_byte_sex);
	    if(l.cmdsize % sizeof(int32_t) != 0)
		printf("load command %u size not a multiple of "
		       "sizeof(int32_t)\n", i);
	    if((char *)lc + l.cmdsize >
	       (char *)load_commands + sizeofcmds)
		printf("load command %u extends past end of load "
		       "commands\n", i);
	    left = sizeofcmds - ((char *)lc - (char *)load_commands);

	    switch(l.cmd){
	    case LC_SEGMENT:
		memset((char *)&sg, '\0', sizeof(struct segment_command));
		size = left < sizeof(struct segment_command) ?
		       left : sizeof(struct segment_command);
		memcpy((char *)&sg, (char *)lc, size);
		if(swapped)
		    swap_segment_command(&sg, host_byte_sex);

		if((filetype == MH_OBJECT && sg.segname[0] == '\0') ||
		   strncmp(sg.segname, segname, sizeof(sg.segname)) == 0){

		    p = (char *)lc + sizeof(struct segment_command);
		    for(j = 0 ; found == FALSE && j < sg.nsects ; j++){
			if(p + sizeof(struct section) >
			   (char *)load_commands + sizeofcmds){
			    printf("section structure command extends past "
				   "end of load commands\n");
			}
			left = sizeofcmds - (p - (char *)load_commands);
			memset((char *)&s, '\0', sizeof(struct section));
			size = left < sizeof(struct section) ?
			       left : sizeof(struct section);
			memcpy((char *)&s, p, size);
			if(swapped)
			    swap_section(&s, 1, host_byte_sex);

			if(strncmp(s.sectname, sectname,
				   sizeof(s.sectname)) == 0 &&
			   strncmp(s.segname, segname,
				   sizeof(s.segname)) == 0){
			    found = TRUE;
			    cmd = LC_SEGMENT;
			    break;
			}

			if(p + sizeof(struct section) >
			   (char *)load_commands + sizeofcmds)
			    return(FALSE);
			p += size;
		    }
		}
		break;
	    case LC_SEGMENT_64:
		memset((char *)&sg64, '\0', sizeof(struct segment_command_64));
		size = left < sizeof(struct segment_command_64) ?
		       left : sizeof(struct segment_command_64);
		memcpy((char *)&sg64, (char *)lc, size);
		if(swapped)
		    swap_segment_command_64(&sg64, host_byte_sex);

		if((filetype == MH_OBJECT && sg64.segname[0] == '\0') ||
		   strncmp(sg64.segname, segname, sizeof(sg64.segname)) == 0){

		    p = (char *)lc + sizeof(struct segment_command_64);
		    for(j = 0 ; found == FALSE && j < sg64.nsects ; j++){
			if(p + sizeof(struct section_64) >
			   (char *)load_commands + sizeofcmds){
			    printf("section structure command extends past "
				   "end of load commands\n");
			}
			left = sizeofcmds - (p - (char *)load_commands);
			memset((char *)&s64, '\0', sizeof(struct section_64));
			size = left < sizeof(struct section_64) ?
			       left : sizeof(struct section_64);
			memcpy((char *)&s64, p, size);
			if(swapped)
			    swap_section_64(&s64, 1, host_byte_sex);

			if(strncmp(s64.sectname, sectname,
				   sizeof(s64.sectname)) == 0 &&
			   strncmp(s64.segname, segname,
				   sizeof(s64.segname)) == 0){
			    found = TRUE;
			    cmd = LC_SEGMENT_64;
			    break;
			}

			if(p + sizeof(struct section_64) >
			   (char *)load_commands + sizeofcmds)
			    return(FALSE);
			p += size;
		    }
		}
		break;
	    }
	    if(l.cmdsize == 0){
		printf("load command %u size zero (can't advance to other "
		       "load commands)\n", i);
		break;
	    }
	    lc = (struct load_command *)((char *)lc + l.cmdsize);
	    if((char *)lc > (char *)load_commands + sizeofcmds)
		break;
	}
	if(found == FALSE)
	    return(FALSE);

	if(cmd == LC_SEGMENT){
	    if((s.flags & SECTION_TYPE) == S_ZEROFILL){
		*sect_pointer = NULL;
		*sect_size = s.size;
	    }
	    else{
		if(s.offset > object_size){
		    printf("section offset for section (%.16s,%.16s) is past "
			   "end of file\n", s.segname, s.sectname);
		}
		else{
		    *sect_pointer = object_addr + s.offset;
		    if(s.offset + s.size > object_size){
			printf("section (%.16s,%.16s) extends past end of "
			       "file\n", s.segname, s.sectname);
			*sect_size = object_size - s.offset;
		    }
		    else
			*sect_size = s.size;
		}
	    }
	    if(s.reloff >= object_size){
		printf("relocation entries offset for (%.16s,%.16s): is past "
		       "end of file\n", s.segname, s.sectname);
	    }
	    else{
		*sect_relocs = (struct relocation_info *)(object_addr +
							  s.reloff);
		if(s.reloff + s.nreloc * sizeof(struct relocation_info) >
								object_size){
		    printf("relocation entries for section (%.16s,%.16s) "
			   "extends past end of file\n", s.segname, s.sectname);
		    *sect_nrelocs = (object_size - s.reloff) /
				    sizeof(struct relocation_info);
		}
		else
		    *sect_nrelocs = s.nreloc;
	    }
	    *sect_addr = s.addr;
	    *sect_flags = s.flags;
	}
	else{
	    if((s64.flags & SECTION_TYPE) == S_ZEROFILL){
		*sect_pointer = NULL;
		*sect_size = s64.size;
	    }
	    else{
		if(s64.offset > object_size){
		    printf("section offset for section (%.16s,%.16s) is past "
			   "end of file\n", s64.segname, s64.sectname);
		}
		else{
		    *sect_pointer = object_addr + s64.offset;
		    if(s64.offset + s64.size > object_size){
			printf("section (%.16s,%.16s) extends past end of "
			       "file\n", s64.segname, s64.sectname);
			*sect_size = object_size - s64.offset;
		    }
		    else
			*sect_size = s64.size;
		}
	    }
	    if(s64.reloff >= object_size){
		printf("relocation entries offset for (%.16s,%.16s): is past "
		       "end of file\n", s64.segname, s64.sectname);
	    }
	    else{
		*sect_relocs = (struct relocation_info *)(object_addr +
							  s64.reloff);
		if(s64.reloff + s64.nreloc * sizeof(struct relocation_info) >
								object_size){
		    printf("relocation entries for section (%.16s,%.16s) "
			   "extends past end of file\n", s64.segname,
			   s64.sectname);
		    *sect_nrelocs = (object_size - s64.reloff) /
				    sizeof(struct relocation_info);
		}
		else
		    *sect_nrelocs = s64.nreloc;
	    }
	    *sect_addr = s64.addr;
	    *sect_flags = s64.flags;
	}
	return(TRUE);
}

static
void
get_linked_reloc_info(
struct load_command *load_commands, 	/* input */
uint32_t ncmds,
uint32_t sizeofcmds,
enum byte_sex load_commands_byte_sex,
char *object_addr,
uint32_t object_size,
struct relocation_info **ext_relocs,	/* output */
uint32_t *next_relocs,
struct relocation_info **loc_relocs,
uint32_t *nloc_relocs)
{
    struct dysymtab_command dyst;
    uint64_t bigsize;

	*ext_relocs = NULL;
	*next_relocs = 0;
	*loc_relocs = NULL;
	*nloc_relocs = 0;

	if(get_dyst(load_commands, ncmds, sizeofcmds, load_commands_byte_sex,
		    &dyst) == FALSE)
	    return;

	if(dyst.extreloff >= object_size){
	    printf("external relocation entries offset is past end of file\n");
	}
	else{
	    *ext_relocs = (struct relocation_info *)(object_addr +
			   dyst.extreloff);
	    bigsize = dyst.nextrel;
	    bigsize *= sizeof(struct relocation_info);
	    bigsize += dyst.extreloff;
	    if(bigsize > object_size){
		printf("external relocation entries extend past end of file\n");
		*next_relocs = (object_size - dyst.extreloff) /
				     sizeof(struct relocation_info);
	    }
	    else
		*next_relocs = dyst.nextrel;
	}
	if(dyst.locreloff >= object_size){
	    printf("local relocation entries offset is past end of file\n");
	}
	else{
	    *loc_relocs = (struct relocation_info *)(object_addr +
			   dyst.locreloff);
	    bigsize = dyst.nlocrel;
	    bigsize *= sizeof(struct relocation_info);
	    bigsize += dyst.locreloff;
	    if(bigsize > object_size){
		printf("local relocation entries extend past end of file\n");
		*nloc_relocs = (object_size - dyst.locreloff) /
				     sizeof(struct relocation_info);
	    }
	    else
		*nloc_relocs = dyst.nlocrel;
	}
}

static
void
print_text(
cpu_type_t cputype,
enum byte_sex object_byte_sex,
char *sect,
uint32_t size,
uint64_t addr,
struct symbol *sorted_symbols,
uint32_t nsorted_symbols,
struct nlist *symbols,
struct nlist_64 *symbols64,
uint32_t nsymbols,
char *strings,
uint32_t strings_size,
struct relocation_info *relocs,
uint32_t nrelocs,
uint32_t *indirect_symbols,
uint32_t nindirect_symbols,
struct load_command *load_commands,
uint32_t ncmds,
uint32_t sizeofcmds,
enum bool disassemble,
enum bool verbose,
cpu_subtype_t cpusubtype)
{
    enum byte_sex host_byte_sex;
    enum bool swapped;
    uint32_t i, j, offset, long_word;
    uint64_t cur_addr;
    unsigned short short_word;
    unsigned char byte_word;

	host_byte_sex = get_host_byte_sex();
	swapped = host_byte_sex != object_byte_sex;

	if(disassemble == TRUE){
	    if(pflag){
		for(i = 0; i < nsorted_symbols; i++){
		    if(strcmp(sorted_symbols[i].name, pflag) == 0)
			break;
		}
		if(i == nsorted_symbols){
		    printf("Can't find -p symbol: %s\n", pflag);
		    return;
		}
		if(sorted_symbols[i].n_value < addr ||
		   sorted_symbols[i].n_value >= addr + size){
		    printf("-p symbol: %s not in text section\n", pflag);
		    return;
		}
		offset = sorted_symbols[i].n_value - addr;
		sect += offset;
		cur_addr = sorted_symbols[i].n_value;
	    }
	    else{
		offset = 0;
		cur_addr = addr;
	    }
	    if(cputype == CPU_TYPE_ARM && cpusubtype == CPU_SUBTYPE_ARM_V7)
		in_thumb = TRUE;
	    else
		in_thumb = FALSE;
	    for(i = offset ; i < size ; ){
		print_label(cur_addr, TRUE, sorted_symbols, nsorted_symbols);
		if(Xflag)
		    printf("\t");
		else{
		    if(cputype & CPU_ARCH_ABI64)
			printf("%016llx\t", cur_addr);
		    else
			printf("%08x\t", (uint32_t)cur_addr);
		}
		if(cputype == CPU_TYPE_POWERPC64)
		    j = ppc_disassemble(sect, size - i, cur_addr, addr,
				object_byte_sex, relocs, nrelocs, symbols,
				symbols64, nsymbols, sorted_symbols,
				nsorted_symbols, strings, strings_size,
				indirect_symbols, nindirect_symbols,
				load_commands, ncmds, sizeofcmds, verbose);
		else if(cputype == CPU_TYPE_X86_64)
		    j = i386_disassemble(sect, size - i, cur_addr, addr,
				object_byte_sex, relocs, nrelocs, NULL,
				symbols64, nsymbols, sorted_symbols,
				nsorted_symbols, strings, strings_size,
				indirect_symbols, nindirect_symbols, cputype,
				load_commands, ncmds, sizeofcmds, verbose);
	 	else if(cputype == CPU_TYPE_MC680x0)
		    j = m68k_disassemble(sect, size - i, cur_addr, addr,
				object_byte_sex, relocs, nrelocs, symbols,
				nsymbols, sorted_symbols, nsorted_symbols,
				strings, strings_size, indirect_symbols,
				nindirect_symbols, load_commands, ncmds,
				sizeofcmds, verbose);
		else if(cputype == CPU_TYPE_I860)
		    j = i860_disassemble(sect, size - i, cur_addr, addr,
				object_byte_sex, relocs, nrelocs, symbols,
				nsymbols, sorted_symbols, nsorted_symbols,
				strings, strings_size, verbose);
		else if(cputype == CPU_TYPE_I386)
		    j = i386_disassemble(sect, size - i, cur_addr, addr,
				object_byte_sex, relocs, nrelocs, symbols, NULL,
				nsymbols, sorted_symbols, nsorted_symbols,
				strings, strings_size, indirect_symbols,
				nindirect_symbols, cputype, load_commands, 
				ncmds, sizeofcmds, verbose);
		else if(cputype == CPU_TYPE_MC88000)
		    j = m88k_disassemble(sect, size - i, cur_addr, addr,
				object_byte_sex, relocs, nrelocs, symbols,
				nsymbols, sorted_symbols, nsorted_symbols,
				strings, strings_size, verbose);
		else if(cputype == CPU_TYPE_POWERPC ||
			cputype == CPU_TYPE_VEO)
		    j = ppc_disassemble(sect, size - i, cur_addr, addr,
				object_byte_sex, relocs, nrelocs, symbols,
				symbols64, nsymbols, sorted_symbols,
				nsorted_symbols, strings, strings_size,
				indirect_symbols, nindirect_symbols,
				load_commands, ncmds, sizeofcmds, verbose);
		else if(cputype == CPU_TYPE_HPPA)
		    j = hppa_disassemble(sect, size - i, cur_addr, addr,
				object_byte_sex, relocs, nrelocs, symbols,
				nsymbols, sorted_symbols, nsorted_symbols,
				strings, strings_size, verbose);
		else if(cputype == CPU_TYPE_SPARC)
		    j = sparc_disassemble(sect, size - i, cur_addr, addr,
				object_byte_sex, relocs, nrelocs, symbols,
				nsymbols, sorted_symbols, nsorted_symbols,
				strings, strings_size, indirect_symbols,
				nindirect_symbols, load_commands, ncmds,
				sizeofcmds, verbose);
		else if(cputype == CPU_TYPE_ARM)
		    j = arm_disassemble(sect, size - i, cur_addr, addr,
				object_byte_sex, relocs, nrelocs, symbols,
				nsymbols, sorted_symbols, nsorted_symbols,
				strings, strings_size, indirect_symbols,
				nindirect_symbols, load_commands, ncmds,
				sizeofcmds, cpusubtype, verbose);
		else{
		    printf("Can't disassemble unknown cputype %d\n", cputype);
		    return;
		}
		sect += j;
		cur_addr += j;
		i += j;
	    }
	}
	else{
	    if(cputype == CPU_TYPE_I386 || cputype == CPU_TYPE_X86_64){
		for(i = 0 ; i < size ; i += j , addr += j){
		    printf("%08x ", (unsigned int)addr);
		    for(j = 0;
			j < 16 * sizeof(char) && i + j < size;
			j += sizeof(char)){
			byte_word = *(sect + i + j);
			printf("%02x ", (unsigned int)byte_word);
		    }
		    printf("\n");
		}
	    }
	    else if(cputype == CPU_TYPE_MC680x0){
		for(i = 0 ; i < size ; i += j , addr += j){
		    printf("%08x ", (unsigned int)addr);
		    for(j = 0;
			j < 8 * sizeof(short) && i + j < size;
			j += sizeof(short)){
			memcpy(&short_word, sect + i + j, sizeof(short));
			if(swapped)
			    short_word = SWAP_SHORT(short_word);
			printf("%04x ", (unsigned int)short_word);
		    }
		    printf("\n");
		}
	    }
	    else{
		for(i = 0 ; i < size ; i += j , addr += j){
		    printf("%08x ", (unsigned int)addr);
		    for(j = 0;
			j < 4 * sizeof(int32_t) && i + j < size;
			j += sizeof(int32_t)){
			memcpy(&long_word, sect + i + j, sizeof(int32_t));
			if(swapped)
			    long_word = SWAP_INT(long_word);
			printf("%08x ", (unsigned int)long_word);
		    }
		    printf("\n");
		}
	    }
	}
}

static
void
print_argstrings(
uint32_t magic,
struct load_command *load_commands,
uint32_t ncmds,
uint32_t sizeofcmds,
cpu_type_t cputype,
cpu_subtype_t cpusubtype,
enum byte_sex load_commands_byte_sex,
char *object_addr,
uint32_t object_size)
{
    enum byte_sex host_byte_sex;
    enum bool swapped;
    uint32_t i, len, left, size, arg;
    uint64_t usrstack;
    struct load_command *lc, l;
    struct segment_command sg;
    struct segment_command_64 sg64;
    char *stack, *stack_top, *p, *q, *argv;
    struct arch_flag arch_flags;

	host_byte_sex = get_host_byte_sex();
	swapped = host_byte_sex != load_commands_byte_sex;

	arch_flags.cputype = cputype;
	if(magic == MH_MAGIC_64)
	    arch_flags.cputype |= CPU_ARCH_ABI64;
	arch_flags.cpusubtype = cpusubtype;
	usrstack = get_stack_addr_from_flag(&arch_flags);
	if(usrstack == 0){
	    printf("Don't know the value of USRSTACK for unknown cputype "
		   "(%d)\n", cputype);
	    return;
	}
	printf("Argument strings on the stack at: ");
	if(cputype & CPU_ARCH_ABI64 || magic == MH_MAGIC_64)
	    printf("%016llx\n", usrstack);
	else
	    printf("%08x\n", (uint32_t)usrstack);
	lc = load_commands;
	for(i = 0 ; i < ncmds; i++){
	    memcpy((char *)&l, (char *)lc, sizeof(struct load_command));
	    if(swapped)
		swap_load_command(&l, host_byte_sex);
	    if(cputype & CPU_ARCH_ABI64 || magic == MH_MAGIC_64){
		if(l.cmdsize % 8 != 0)
		    printf("load command %u size not a multiple of 8\n", i);
	    }
	    else{
		if(l.cmdsize % 4 != 0)
		    printf("load command %u size not a multiple of 4\n", i);
	    }
	    if((char *)lc + l.cmdsize >
	       (char *)load_commands + sizeofcmds)
		printf("load command %u extends past end of load "
		       "commands\n", i);
	    left = sizeofcmds - ((char *)lc - (char *)load_commands);

	    switch(l.cmd){
	    case LC_SEGMENT:
		memset((char *)&sg, '\0', sizeof(struct segment_command));
		size = left < sizeof(struct segment_command) ?
		       left : sizeof(struct segment_command);
		memcpy((char *)&sg, (char *)lc, size);
		if(swapped)
		    swap_segment_command(&sg, host_byte_sex);

		if(cputype == CPU_TYPE_HPPA){
		    if(sg.vmaddr == usrstack &&
		       sg.fileoff + sg.filesize <= object_size){
			stack = object_addr + sg.fileoff;
			stack_top = stack + sg.filesize;
			/*
			 * There appears to be some pointer then argc at the
			 * bottom of the stack first before argv[].
			 */
			argv = stack + 2 * sizeof(uint32_t);
			memcpy(&arg, argv, sizeof(uint32_t));
			if(swapped)
			    arg = SWAP_INT(arg);
			while(argv < stack_top &&
			      arg != 0 &&
			      arg >= usrstack && arg < usrstack + sg.filesize){
			    p = stack + (arg - usrstack);
			    printf("\t");
			    while(p < stack_top && *p != '\0'){
				printf("%c", *p);
				p++;
			    }
			    printf("\n");
			    argv += sizeof(uint32_t);
			    memcpy(&arg, argv, sizeof(uint32_t));
			    if(swapped)
				arg = SWAP_INT(arg);
			}
			/* after argv[] then there is envp[] */
			argv += sizeof(uint32_t);
			memcpy(&arg, argv, sizeof(uint32_t));
			if(swapped)
			    arg = SWAP_INT(arg);
			while(argv < stack_top &&
			      arg != 0 &&
			      arg >= usrstack && arg < usrstack + sg.filesize){
			    p = stack + (arg - usrstack);
			    printf("\t");
			    while(p < stack_top && *p != '\0'){
				printf("%c", *p);
				p++;
			    }
			    printf("\n");
			    argv += sizeof(uint32_t);
			    memcpy(&arg, argv, sizeof(uint32_t));
			    if(swapped)
				arg = SWAP_INT(arg);
			}
		    }
		}
		else{
		    if(sg.vmaddr + sg.vmsize == usrstack &&
		       sg.fileoff + sg.filesize <= object_size){
			stack = object_addr + sg.fileoff;
			stack_top = stack + sg.filesize;

			/* the first thing on the stack is a long 0 */
			stack_top -= 4;
			p = (char *)stack_top;

			/* find the first non-null character before the long 0*/
			while(p > stack && *p == '\0')
			    p--;
			if(p != (char *)stack_top)
			    p++;

			q = p;
			/* Stop when we find another long 0 */
			while(p > stack && (*p != '\0' || *(p-1) != '\0' ||
			      *(p-2) != '\0' || *(p-3) != '\0')){
			    p--;
			    /* step back over the string to its start */
			    while(p > stack && *p != '\0')
				p--;
			}

			p++; /* step forward to the start of the first string */
			while(p < q){
			    printf("\t");
			    len = 0;
			    while(p + len < q && p[len] != '\0'){
				printf("%c", p[len]);
				len++;
			    }
			    printf("\n");
			    p += len + 1;
			}
			return;
		    }
		}
		break;
	    case LC_SEGMENT_64:
		memset((char *)&sg64, '\0', sizeof(struct segment_command_64));
		size = left < sizeof(struct segment_command_64) ?
		       left : sizeof(struct segment_command_64);
		memcpy((char *)&sg64, (char *)lc, size);
		if(swapped)
		    swap_segment_command_64(&sg64, host_byte_sex);
		if(sg64.vmaddr + sg64.vmsize == usrstack &&
		   sg64.fileoff + sg64.filesize <= object_size){
		    stack = object_addr + sg64.fileoff;
		    stack_top = stack + sg64.filesize;

		    /* the first thing on the stack is a uint64_t 0 */
		    stack_top -= 8;
		    p = (char *)stack_top;

		    /* find the first non-null character before the uint64_t 0*/
		    while(p > stack && *p == '\0')
			p--;
		    if(p != (char *)stack_top)
			p++;

		    q = p;
		    /* Stop when we find another uint64_t 0 */
		    while(p > stack && (*p != '\0' || *(p-1) != '\0' ||
			  *(p-2) != '\0' || *(p-3) != '\0' || *(p-4) != '\0' ||
			  *(p-5) != '\0' || *(p-6) != '\0' || *(p-7) != '\0')){
			p--;
			/* step back over the string to its start */
			while(p > stack && *p != '\0')
			    p--;
		    }

		    p++; /* step forward to the start of the first string */
		    while(p < q){
			printf("\t");
			len = 0;
			while(p + len < q && p[len] != '\0'){
			    printf("%c", p[len]);
			    len++;
			}
			printf("\n");
			p += len + 1;
		    }
		    return;
		}
		break;
	    }

	    if(l.cmdsize == 0){
		printf("load command %u size zero (can't advance to other "
		       "load commands)\n", i);
		break;
	    }
	    lc = (struct load_command *)((char *)lc + l.cmdsize);
	    if((char *)lc > (char *)load_commands + sizeofcmds)
		break;
	}
}

#ifndef __DYNAMIC__
/*
 * To avoid linking in libm.  These variables are defined as they are used in
 * pthread_init() to put in place a fast sqrt().
 */
size_t hw_sqrt_len = 0;

double
sqrt(double x)
{
	return(0.0);
}
double
hw_sqrt(double x)
{
	return(0.0);
}

/*
 * More stubs to avoid linking in libm.  This works as along as we don't use
 * long doubles.
 */
int32_t
__fpclassifyd(double x)
{
	return(0);
}
int32_t
__fpclassify(long double x)
{
	return(0);
}
#endif /* !defined(__DYNAMIC__) */

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
#define __darwin_i386_exception_state i386_exception_state
#define __darwin_i386_float_state i386_float_state
#define __darwin_i386_thread_state i386_thread_state

#include <mach-o/loader.h>
#include <mach/m68k/thread_status.h>
#undef MACHINE_THREAD_STATE	/* need to undef these to avoid warnings */
#undef MACHINE_THREAD_STATE_COUNT
#undef THREAD_STATE_NONE
#undef VALID_THREAD_STATE_FLAVOR
#include <mach/ppc/thread_status.h>
#undef MACHINE_THREAD_STATE	/* need to undef these to avoid warnings */
#undef MACHINE_THREAD_STATE_COUNT
#undef THREAD_STATE_NONE
#undef VALID_THREAD_STATE_FLAVOR
#include <mach/m88k/thread_status.h>
#include <mach/i860/thread_status.h>
#include <mach/i386/thread_status.h>
#include <mach/hppa/thread_status.h>
#include <mach/sparc/thread_status.h>
#include <mach/arm/thread_status.h>
#include "stuff/bool.h"
#include "stuff/bytesex.h"
#include "stuff/errors.h"

/*
 * swap_object_headers() swaps the object file headers from the host byte sex
 * into the non-host byte sex.  It returns TRUE if it can and did swap the
 * headers else returns FALSE and does not touch the headers and prints an error
 * using the error() routine.
 */
__private_extern__
enum bool
swap_object_headers(
void *mach_header,
struct load_command *load_commands)
{
    unsigned long i;
    uint32_t magic, ncmds, sizeofcmds, cmd_multiple;
    cpu_type_t cputype;
    cpu_subtype_t cpusubtype;
    struct mach_header *mh;
    struct mach_header_64 *mh64;
    enum byte_sex target_byte_sex;
    struct load_command *lc, l;
    struct segment_command *sg;
    struct segment_command_64 *sg64;
    struct section *s;
    struct section_64 *s64;
    struct symtab_command *st;
    struct dysymtab_command *dyst;
    struct symseg_command *ss;
    struct fvmlib_command *fl;
    struct thread_command *ut;
    struct ident_command *id;
    struct dylib_command *dl;
    struct sub_framework_command *sub;
    struct sub_umbrella_command *usub;
    struct sub_library_command *lsub;
    struct sub_client_command *csub;
    struct prebound_dylib_command *pbdylib;
    struct dylinker_command *dyld;
    struct routines_command *rc;
    struct routines_command_64 *rc64;
    struct twolevel_hints_command *hints;
    struct prebind_cksum_command *cs;
    struct uuid_command *uuid;
    struct linkedit_data_command *ld;
    struct rpath_command *rpath;
    struct encryption_info_command *ec;
    struct dyld_info_command *dc;
    uint32_t flavor, count;
    unsigned long nflavor;
    char *p, *state, *cmd_name;

	magic = *((uint32_t *)mach_header);
	if(magic == MH_MAGIC){
	    mh = (struct mach_header *)mach_header;
	    ncmds = mh->ncmds;
	    sizeofcmds = mh->sizeofcmds;
	    cputype = mh->cputype;
	    cpusubtype = mh->cpusubtype;
	    cmd_multiple = 4;
	    mh64 = NULL;
	}
	else{
	    mh64 = (struct mach_header_64 *)mach_header;
	    ncmds = mh64->ncmds;
	    sizeofcmds = mh64->sizeofcmds;
	    cputype = mh64->cputype;
	    cpusubtype = mh64->cpusubtype;
	    cmd_multiple = 8;
	    mh = NULL;
	}
	/*
	 * Make a pass through the load commands checking them to the level
	 * that they can be parsed and then swapped.
	 */
	for(i = 0, lc = load_commands; i < ncmds; i++){
	    l = *lc;
	    /* check load command size for a correct multiple size */
	    if(lc->cmdsize % cmd_multiple != 0){
		error("in swap_object_headers(): malformed load command %lu "
		      "(cmdsize not a multiple of %u)", i, cmd_multiple);
		return(FALSE);
	    }
	    /* check that load command does not extends past end of commands */
	    if((char *)lc + lc->cmdsize >
	       (char *)load_commands + sizeofcmds){
		error("in swap_object_headers(): truncated or malformed load "
		      "command %lu (extends past the end of the all load "
		      "commands)", i);
		return(FALSE);
	    }
	    /* check that the load command size is not zero */
	    if(lc->cmdsize == 0){
		error("in swap_object_headers(): malformed load command %lu "
		      "(cmdsize is zero)", i);
		return(FALSE);
	    }
	    switch(lc->cmd){
	    case LC_SEGMENT:
		sg = (struct segment_command *)lc;
		if(sg->cmdsize != sizeof(struct segment_command) +
				     sg->nsects * sizeof(struct section)){
		    error("in swap_object_headers(): malformed load command "
			  "(inconsistent cmdsize in LC_SEGMENT command %lu for "
			  "the number of sections)", i);
		    return(FALSE);
		}
		break;

	    case LC_SEGMENT_64:
		sg64 = (struct segment_command_64 *)lc;
		if(sg64->cmdsize != sizeof(struct segment_command_64) +
				     sg64->nsects * sizeof(struct section_64)){
		    error("in swap_object_headers(): malformed load command "
			  "(inconsistent cmdsize in LC_SEGMENT_64 command %lu "
			  "for the number of sections)", i);
		    return(FALSE);
		}
		break;

	    case LC_SYMTAB:
		st = (struct symtab_command *)lc;
		if(st->cmdsize != sizeof(struct symtab_command)){
		    error("in swap_object_headers(): malformed load commands "
			  "(LC_SYMTAB command %lu has incorrect cmdsize", i);
		    return(FALSE);
		}
		break;

	    case LC_DYSYMTAB:
		dyst = (struct dysymtab_command *)lc;
		if(dyst->cmdsize != sizeof(struct dysymtab_command)){
		    error("in swap_object_headers(): malformed load commands "
			  "(LC_DYSYMTAB command %lu has incorrect cmdsize", i);
		    return(FALSE);
		}
		break;

	    case LC_SYMSEG:
		ss = (struct symseg_command *)lc;
		if(ss->cmdsize != sizeof(struct symseg_command)){
		    error("in swap_object_headers(): malformed load command "
			  "(LC_SYMSEG command %lu has incorrect cmdsize", i);
		    return(FALSE);
		}
		break;

	    case LC_IDFVMLIB:
	    case LC_LOADFVMLIB:
		fl = (struct fvmlib_command *)lc;
		if(fl->cmdsize < sizeof(struct fvmlib_command)){
		    error("in swap_object_headers(): malformed load commands "
			  "(%s command %lu has too small cmdsize field)",
			  fl->cmd == LC_IDFVMLIB ? "LC_IDFVMLIB" :
			  "LC_LOADFVMLIB", i);
		    return(FALSE);
		}
		if(fl->fvmlib.name.offset >= fl->cmdsize){
		    error("in swap_object_headers(): truncated or malformed "
			  "load commands (name.offset field of %s command %lu "
			  "extends past the end of all load commands)",
			  fl->cmd == LC_IDFVMLIB ? "LC_IDFVMLIB" :
			  "LC_LOADFVMLIB", i);
		    return(FALSE);
		}
		break;

	    case LC_ID_DYLIB:
		cmd_name = "LC_ID_DYLIB";
		goto check_dylib_command;
	    case LC_LOAD_DYLIB:
		cmd_name = "LC_LOAD_DYLIB";
		goto check_dylib_command;
	    case LC_LOAD_WEAK_DYLIB:
		cmd_name = "LC_LOAD_WEAK_DYLIB";
		goto check_dylib_command;
	    case LC_REEXPORT_DYLIB:
		cmd_name = "LC_REEXPORT_DYLIB";
		goto check_dylib_command;
	    case LC_LAZY_LOAD_DYLIB:
		cmd_name = "LC_LAZY_LOAD_DYLIB";
		goto check_dylib_command;
check_dylib_command:
		dl = (struct dylib_command *)lc;
		if(dl->cmdsize < sizeof(struct dylib_command)){
		    error("in swap_object_headers(): malformed load commands "
			  "(%s command %lu has too small cmdsize field)",
			  cmd_name, i);
		    return(FALSE);
		}
		if(dl->dylib.name.offset >= dl->cmdsize){
		    error("in swap_object_headers(): truncated or malformed "
			  "load commands (name.offset field of %s command %lu "
			  "extends past the end of all load commands)",
			  cmd_name, i);
		    return(FALSE);
		}
		break;

	    case LC_SUB_FRAMEWORK:
		sub = (struct sub_framework_command *)lc;
		if(sub->cmdsize < sizeof(struct sub_framework_command)){
		    error("in swap_object_headers(): malformed load commands "
			  "(LC_SUB_FRAMEWORK command %lu has too small cmdsize "
			  "field)", i);
		    return(FALSE);
		}
		if(sub->umbrella.offset >= sub->cmdsize){
		    error("in swap_object_headers(): truncated or malformed "
			  "load commands (umbrella.offset field of "
			  "LC_SUB_FRAMEWORK command %lu extends past the end "
			  "of all load commands)", i);
		    return(FALSE);
		}
		break;

	    case LC_SUB_UMBRELLA:
		usub = (struct sub_umbrella_command *)lc;
		if(usub->cmdsize < sizeof(struct sub_umbrella_command)){
		    error("in swap_object_headers(): malformed load commands "
			  "(LC_SUB_UMBRELLA command %lu has too small cmdsize "
			  "field)", i);
		    return(FALSE);
		}
		if(usub->sub_umbrella.offset >= usub->cmdsize){
		    error("in swap_object_headers(): truncated or malformed "
			  "load commands (sub_umbrella.offset field of "
			  "LC_SUB_UMBRELLA command %lu extends past the end "
			  "of all load commands)", i);
		    return(FALSE);
		}
		break;

	    case LC_SUB_LIBRARY:
		lsub = (struct sub_library_command *)lc;
		if(lsub->cmdsize < sizeof(struct sub_library_command)){
		    error("in swap_object_headers(): malformed load commands "
			  "(LC_SUB_LIBRARY command %lu has too small cmdsize "
			  "field)", i);
		    return(FALSE);
		}
		if(lsub->sub_library.offset >= lsub->cmdsize){
		    error("in swap_object_headers(): truncated or malformed "
			  "load commands (sub_library.offset field of "
			  "LC_SUB_LIBRARY command %lu extends past the end "
			  "of all load commands)", i);
		    return(FALSE);
		}
		break;

	    case LC_SUB_CLIENT:
		csub = (struct sub_client_command *)lc;
		if(csub->cmdsize < sizeof(struct sub_client_command)){
		    error("in swap_object_headers(): malformed load commands "
			  "(LC_SUB_CLIENT command %lu has too small cmdsize "
			  "field)", i);
		    return(FALSE);
		}
		if(csub->client.offset >= csub->cmdsize){
		    error("in swap_object_headers(): truncated or malformed "
			  "load commands (client.offset field of "
			  "LC_SUB_CLIENT command %lu extends past the end "
			  "of all load commands)", i);
		    return(FALSE);
		}
		break;

	    case LC_PREBOUND_DYLIB:
		pbdylib = (struct prebound_dylib_command *)lc;
		if(pbdylib->cmdsize < sizeof(struct prebound_dylib_command)){
		    error("in swap_object_headers(): malformed load commands "
			  "(LC_PREBOUND_DYLIB command %lu has too small "
			  "cmdsize field)", i);
		    return(FALSE);
		}
		if(pbdylib->name.offset >= pbdylib->cmdsize){
		    error("in swap_object_headers(): truncated or malformed "
			  "load commands (name.offset field of "
			  "LC_PREBOUND_DYLIB command %lu extends past the end "
			  "of all load commands)", i);
		    return(FALSE);
		}
		if(pbdylib->linked_modules.offset >= pbdylib->cmdsize){
		    error("in swap_object_headers(): truncated or malformed "
			  "load commands (linked_modules.offset field of "
			  "LC_PREBOUND_DYLIB command %lu extends past the end "
			  "of all load commands)", i);
		    return(FALSE);
		}
		break;

	    case LC_ID_DYLINKER:
	    case LC_LOAD_DYLINKER:
		dyld = (struct dylinker_command *)lc;
		if(dyld->cmdsize < sizeof(struct dylinker_command)){
		    error("in swap_object_headers(): malformed load commands "
			  "(%s command %lu has too small cmdsize field)",
			  dyld->cmd == LC_ID_DYLINKER ? "LC_ID_DYLINKER" :
			  "LC_LOAD_DYLINKER", i);
		    return(FALSE);
		}
		if(dyld->name.offset >= dyld->cmdsize){
		    error("in swap_object_headers(): truncated or malformed "
			  "load commands (name.offset field of %s command %lu "
			  "extends past the end of all load commands)",
			  dyld->cmd == LC_ID_DYLINKER ? "LC_ID_DYLINKER" :
			  "LC_LOAD_DYLINKER", i);
		    return(FALSE);
		}
		break;

	    case LC_UNIXTHREAD:
	    case LC_THREAD:
		ut = (struct thread_command *)lc;
		state = (char *)ut + sizeof(struct thread_command);

	    	if(cputype == CPU_TYPE_MC680x0){
		    struct m68k_thread_state_regs *cpu;
		    struct m68k_thread_state_68882 *fpu;
		    struct m68k_thread_state_user_reg *user_reg;

		    nflavor = 0;
		    p = (char *)ut + ut->cmdsize;
		    while(state < p){
			flavor = *((uint32_t *)state);
			state += sizeof(uint32_t);
			count = *((uint32_t *)state);
			state += sizeof(uint32_t);
			switch(flavor){
			case M68K_THREAD_STATE_REGS:
			    if(count != M68K_THREAD_STATE_REGS_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not M68K_THREAD_STATE_REGS_COUNT for "
				    "flavor number %lu which is a M68K_THREAD_"
				    "STATE_REGS flavor in %s command %lu)",
				    nflavor, ut->cmd == LC_UNIXTHREAD ? 
				    "LC_UNIXTHREAD" : "LC_THREAD", i);
				return(FALSE);
			    }
			    cpu = (struct m68k_thread_state_regs *)state;
			    state += sizeof(struct m68k_thread_state_regs);
			    break;
			case M68K_THREAD_STATE_68882:
			    if(count != M68K_THREAD_STATE_68882_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not M68K_THREAD_STATE_68882_COUNT for "
				    "flavor number %lu which is a M68K_THREAD_"
				    "STATE_68882 flavor in %s command %lu)",
				    nflavor, ut->cmd == LC_UNIXTHREAD ? 
				    "LC_UNIXTHREAD" : "LC_THREAD", i);
				return(FALSE);
			    }
			    fpu = (struct m68k_thread_state_68882 *)state;
			    state += sizeof(struct m68k_thread_state_68882);
			    break;
			case M68K_THREAD_STATE_USER_REG:
			    if(count != M68K_THREAD_STATE_USER_REG_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not M68K_THREAD_STATE_USER_REG_COUNT for "
				    "flavor number %lu which is a M68K_THREAD_"
				    "STATE_USER_REG flavor in %s command %lu)",
				    nflavor, ut->cmd == LC_UNIXTHREAD ? 
				    "LC_UNIXTHREAD" : "LC_THREAD", i);
				return(FALSE);
			    }
			    user_reg =
				(struct m68k_thread_state_user_reg *)state;
			    state += sizeof(struct m68k_thread_state_user_reg);
			    break;
			default:
			    error("in swap_object_headers(): malformed "
				"load commands (unknown "
				"flavor %u for flavor number %lu in %s command"
				" %lu can't byte swap it)", flavor, nflavor,
				ut->cmd == LC_UNIXTHREAD ? "LC_UNIXTHREAD" :
				"LC_THREAD", i);
			    return(FALSE);
			}
			nflavor++;
		    }
		    break;
		}
	    	if(cputype == CPU_TYPE_POWERPC ||
	    	   cputype == CPU_TYPE_VEO ||
		   cputype == CPU_TYPE_POWERPC64){
		    ppc_thread_state_t *cpu;
		    ppc_float_state_t *fpu;
		    ppc_exception_state_t *except;
		    ppc_thread_state64_t *cpu64;

		    nflavor = 0;
		    p = (char *)ut + ut->cmdsize;
		    while(state < p){
			flavor = *((uint32_t *)state);
			state += sizeof(uint32_t);
			count = *((uint32_t *)state);
			state += sizeof(uint32_t);
			switch(flavor){
			case PPC_THREAD_STATE:
			    if(count != PPC_THREAD_STATE_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not PPC_THREAD_STATE_COUNT for "
				    "flavor number %lu which is a PPC_THREAD_"
				    "STATE flavor in %s command %lu)",
				    nflavor, ut->cmd == LC_UNIXTHREAD ? 
				    "LC_UNIXTHREAD" : "LC_THREAD", i);
				return(FALSE);
			    }
			    cpu = (ppc_thread_state_t *)state;
			    state += sizeof(ppc_thread_state_t);
			    break;
			case PPC_FLOAT_STATE:
			    if(count != PPC_FLOAT_STATE_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not PPC_FLOAT_STATE_COUNT for "
				    "flavor number %lu which is a PPC_FLOAT_"
				    "STATE flavor in %s command %lu)",
				    nflavor, ut->cmd == LC_UNIXTHREAD ? 
				    "LC_UNIXTHREAD" : "LC_THREAD", i);
				return(FALSE);
			    }
			    fpu = (ppc_float_state_t *)state;
			    state += sizeof(ppc_float_state_t);
			    break;
			case PPC_EXCEPTION_STATE:
			    if(count != PPC_EXCEPTION_STATE_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not PPC_EXCEPTION_STATE_COUNT for "
				    "flavor number %lu which is a PPC_EXCEPT"
				    "ION_STATE flavor in %s command %lu)",
				    nflavor, ut->cmd == LC_UNIXTHREAD ? 
				    "LC_UNIXTHREAD" : "LC_THREAD", i);
				return(FALSE);
			    }
			    except = (ppc_exception_state_t *)state;
			    state += sizeof(ppc_exception_state_t);
			    break;
			case PPC_THREAD_STATE64:
			    if(count != PPC_THREAD_STATE64_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not PPC_THREAD_STATE64_COUNT for "
				    "flavor number %lu which is a PPC_THREAD_"
				    "STATE64 flavor in %s command %lu)",
				    nflavor, ut->cmd == LC_UNIXTHREAD ? 
				    "LC_UNIXTHREAD" : "LC_THREAD", i);
				return(FALSE);
			    }
			    cpu64 = (ppc_thread_state64_t *)state;
			    state += sizeof(ppc_thread_state64_t);
			    break;
			default:
			    error("in swap_object_headers(): malformed "
				"load commands (unknown "
				"flavor %u for flavor number %lu in %s command"
				" %lu can't byte swap it)", flavor, nflavor,
				ut->cmd == LC_UNIXTHREAD ? "LC_UNIXTHREAD" :
				"LC_THREAD", i);
			    return(FALSE);
			}
			nflavor++;
		    }
		    break;
		}
	    	if(cputype == CPU_TYPE_MC88000){
		    m88k_thread_state_grf_t *cpu;
		    m88k_thread_state_xrf_t *fpu;
		    m88k_thread_state_user_t *user;
		    m88110_thread_state_impl_t *spu;

		    nflavor = 0;
		    p = (char *)ut + ut->cmdsize;
		    while(state < p){
			flavor = *((uint32_t *)state);
			state += sizeof(uint32_t);
			count = *((uint32_t *)state);
			state += sizeof(uint32_t);
			switch(flavor){
			case M88K_THREAD_STATE_GRF:
			    if(count != M88K_THREAD_STATE_GRF_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not M88K_THREAD_STATE_GRF_COUNT for "
				    "flavor number %lu which is a M88K_THREAD_"
				    "STATE_GRF flavor in %s command %lu)",
				    nflavor, ut->cmd == LC_UNIXTHREAD ? 
				    "LC_UNIXTHREAD" : "LC_THREAD", i);
				return(FALSE);
			    }
			    cpu = (m88k_thread_state_grf_t *)state;
			    state += sizeof(m88k_thread_state_grf_t);
			    break;
			case M88K_THREAD_STATE_XRF:
			    if(count != M88K_THREAD_STATE_XRF_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not M88K_THREAD_STATE_XRF_COUNT for "
				    "flavor number %lu which is a M88K_THREAD_"
				    "STATE_XRF flavor in %s command %lu)",
				    nflavor, ut->cmd == LC_UNIXTHREAD ? 
				    "LC_UNIXTHREAD" : "LC_THREAD", i);
				return(FALSE);
			    }
			    fpu = (m88k_thread_state_xrf_t *)state;
			    state += sizeof(m88k_thread_state_xrf_t);
			    break;
			case M88K_THREAD_STATE_USER:
			    if(count != M88K_THREAD_STATE_USER_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not M88K_THREAD_STATE_USER_COUNT for "
				    "flavor number %lu which is a M88K_THREAD_"
				    "STATE_USER flavor in %s command %lu)",
				    nflavor, ut->cmd == LC_UNIXTHREAD ? 
				    "LC_UNIXTHREAD" : "LC_THREAD", i);
				return(FALSE);
			    }
			    user = (m88k_thread_state_user_t *)state;
			    state += sizeof(m88k_thread_state_user_t);
			    break;
			case M88110_THREAD_STATE_IMPL:
			    if(count != M88110_THREAD_STATE_IMPL_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not M88110_THREAD_STATE_IMPL_COUNT for "
				    "flavor number %lu which is a M88110_THREAD"
				    "_STATE_IMPL flavor in %s command %lu)",
				    nflavor, ut->cmd == LC_UNIXTHREAD ? 
				    "LC_UNIXTHREAD" : "LC_THREAD", i);
				return(FALSE);
			    }
			    spu = (m88110_thread_state_impl_t *)state;
			    state += sizeof(m88110_thread_state_impl_t);
			    break;
			default:
			    error("in swap_object_headers(): malformed "
				"load commands (unknown "
				"flavor %u for flavor number %lu in %s command"
				" %lu can't byte swap it)", flavor, nflavor,
				ut->cmd == LC_UNIXTHREAD ? "LC_UNIXTHREAD" :
				"LC_THREAD", i);
			    return(FALSE);
			}
			nflavor++;
		    }
		    break;
		}
	    	if(cputype == CPU_TYPE_I860){
		    struct i860_thread_state_regs *cpu;

		    nflavor = 0;
		    p = (char *)ut + ut->cmdsize;
		    while(state < p){
			flavor = *((uint32_t *)state);
			state += sizeof(uint32_t);
			count = *((uint32_t *)state);
			state += sizeof(uint32_t);
			switch(flavor){
			case I860_THREAD_STATE_REGS:
			    if(count != I860_THREAD_STATE_REGS_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not I860_THREAD_STATE_REGS_COUNT for "
				    "flavor number %lu which is a I860_THREAD_"
				    "STATE_REGS flavor in %s command %lu)",
				    nflavor, ut->cmd == LC_UNIXTHREAD ? 
				    "LC_UNIXTHREAD" : "LC_THREAD", i);
				return(FALSE);
			    }
			    cpu = (struct i860_thread_state_regs *)state;
			    state += sizeof(struct i860_thread_state_regs);
			    break;
			default:
			    error("in swap_object_headers(): malformed "
				"load commands (unknown "
				"flavor %u for flavor number %lu in %s command"
				" %lu can't byte swap it)", flavor, nflavor,
				ut->cmd == LC_UNIXTHREAD ? "LC_UNIXTHREAD" :
				"LC_THREAD", i);
			    return(FALSE);
			}
			nflavor++;
		    }
		    break;
		}
	    	if(cputype == CPU_TYPE_I386
#ifdef x86_THREAD_STATE64
		   || cputype == CPU_TYPE_X86_64
#endif /* x86_THREAD_STATE64 */
		   ){
		    i386_thread_state_t *cpu;
#ifdef x86_THREAD_STATE64
		    x86_thread_state64_t *cpu64;
#endif /* x86_THREAD_STATE64 */
/* current i386 thread states */
#if i386_THREAD_STATE == 1
		    struct i386_float_state *fpu;
		    i386_exception_state_t *exc;
#endif /* i386_THREAD_STATE == 1 */

/* i386 thread states on older releases */
#if i386_THREAD_STATE == -1
		    i386_thread_fpstate_t *fpu;
		    i386_thread_exceptstate_t *exc;
		    i386_thread_cthreadstate_t *user;
#endif /* i386_THREAD_STATE == -1 */

		    nflavor = 0;
		    p = (char *)ut + ut->cmdsize;
		    while(state < p){
			flavor = *((uint32_t *)state);
			state += sizeof(uint32_t);
			count = *((uint32_t *)state);
			state += sizeof(uint32_t);
			switch((int)flavor){
			case i386_THREAD_STATE:
/* current i386 thread states */
#if i386_THREAD_STATE == 1
			case -1:
#endif /* i386_THREAD_STATE == 1 */
/* i386 thread states on older releases */
#if i386_THREAD_STATE == -1
			case 1:
#endif /* i386_THREAD_STATE == -1 */
			    if(count != i386_THREAD_STATE_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not i386_THREAD_STATE_COUNT for flavor "
				    "number %lu which is a i386_THREAD_STATE "
				    "flavor in %s command %lu)", nflavor,
				    ut->cmd == LC_UNIXTHREAD ? "LC_UNIXTHREAD" :
				    "LC_THREAD", i);
				return(FALSE);
			    }
			    cpu = (i386_thread_state_t *)state;
			    state += sizeof(i386_thread_state_t);
			    break;
/* current i386 thread states */
#if i386_THREAD_STATE == 1
			case i386_FLOAT_STATE:
			    if(count != i386_FLOAT_STATE_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not i386_FLOAT_STATE_COUNT for flavor "
				    "number %lu which is a i386_FLOAT_STATE "
				    "flavor in %s command %lu)", nflavor,
				    ut->cmd == LC_UNIXTHREAD ? "LC_UNIXTHREAD" :
				    "LC_THREAD", i);
				return(FALSE);
			    }
			    fpu = (struct i386_float_state *)state;
			    state += sizeof(struct i386_float_state);
			    break;
			case i386_EXCEPTION_STATE:
			    if(count != I386_EXCEPTION_STATE_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not I386_EXCEPTION_STATE_COUNT for "
				    "flavor number %lu which is a i386_"
				    "EXCEPTION_STATE flavor in %s command %lu)",
				    nflavor,
				    ut->cmd == LC_UNIXTHREAD ? "LC_UNIXTHREAD" :
				    "LC_THREAD", i);
				return(FALSE);
			    }
			    exc = (i386_exception_state_t *)state;
			    state += sizeof(i386_exception_state_t);
			    break;
#endif /* i386_THREAD_STATE == 1 */

/* i386 thread states on older releases */
#if i386_THREAD_STATE == -1
			case i386_THREAD_FPSTATE:
			    if(count != i386_THREAD_FPSTATE_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not i386_THREAD_FPSTATE_COUNT for flavor "
				    "number %lu which is a i386_THREAD_FPSTATE "
				    "flavor in %s command %lu)", nflavor,
				    ut->cmd == LC_UNIXTHREAD ? "LC_UNIXTHREAD" :
				    "LC_THREAD", i);
				return(FALSE);
			    }
			    fpu = (i386_thread_fpstate_t *)state;
			    state += sizeof(i386_thread_fpstate_t);
			    break;
			case i386_THREAD_EXCEPTSTATE:
			    if(count != i386_THREAD_EXCEPTSTATE_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not i386_THREAD_EXCEPTSTATE_COUNT for "
				    "flavor number %lu which is a i386_THREAD_"
				    "EXCEPTSTATE flavor in %s command %lu)",
				    nflavor,
				    ut->cmd == LC_UNIXTHREAD ? "LC_UNIXTHREAD" :
				    "LC_THREAD", i);
				return(FALSE);
			    }
			    exc = (i386_thread_exceptstate_t *)state;
			    state += sizeof(i386_thread_fpstate_t);
			    break;
			case i386_THREAD_CTHREADSTATE:
			    if(count != i386_THREAD_CTHREADSTATE_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not i386_THREAD_CTHREADSTATE_COUNT for "
				    "flavor number %lu which is a i386_THREAD_"
				    "CTHREADSTATE flavor in %s command %lu)",
				    nflavor,
				    ut->cmd == LC_UNIXTHREAD ? "LC_UNIXTHREAD" :
				    "LC_THREAD", i);
				return(FALSE);
			    }
			    user = (i386_thread_cthreadstate_t *)state;
			    state += sizeof(i386_thread_fpstate_t);
			    break;
#endif /* i386_THREAD_STATE == -1 */
#ifdef x86_THREAD_STATE64
			case x86_THREAD_STATE64:
			    if(count != x86_THREAD_STATE64_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not x86_THREAD_STATE64_COUNT for "
				    "flavor number %lu which is an x86_THREAD_"
				    "STATE64 flavor in %s command %lu)",
				    nflavor,
				    ut->cmd == LC_UNIXTHREAD ? "LC_UNIXTHREAD" :
				    "LC_THREAD", i);
				return(FALSE);
			    }
			    cpu64 = (x86_thread_state64_t *)state;
			    state += sizeof(x86_thread_state64_t);
			    break;
#endif /* x86_THREAD_STATE64 */
			default:
			    error("in swap_object_headers(): malformed "
				"load commands (unknown "
				"flavor %u for flavor number %lu in %s command"
				" %lu can't byte swap it)", flavor, nflavor,
				ut->cmd == LC_UNIXTHREAD ? "LC_UNIXTHREAD" :
				"LC_THREAD", i);
			    return(FALSE);
			}
			nflavor++;
		    }
		    break;
		}
	        if(cputype == CPU_TYPE_HPPA){
		    struct hp_pa_integer_thread_state *cpu;
		    struct hp_pa_frame_thread_state *frame;
		    struct hp_pa_fp_thread_state *fpu;

		    nflavor = 0;
		    p = (char *)ut + ut->cmdsize;
		    while(state < p){
			flavor = *((uint32_t *)state);
			state += sizeof(uint32_t);
			count = *((uint32_t *)state);
			state += sizeof(uint32_t);
			switch(flavor){
			case HPPA_INTEGER_THREAD_STATE:
			    if(count != HPPA_INTEGER_THREAD_STATE_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not HPPA_INTEGER_THREAD_STATE_COUNT for "
				    "flavor number %lu which is a HPPA_INTEGER"
				    "_THREAD_STATE flavor in %s command %lu)",
				    nflavor, ut->cmd == LC_UNIXTHREAD ? 
				    "LC_UNIXTHREAD" : "LC_THREAD", i);
				return(FALSE);
			    }
			    cpu = (struct hp_pa_integer_thread_state *)state;
			    state += sizeof(struct hp_pa_integer_thread_state);
			    break;
			case HPPA_FRAME_THREAD_STATE:
			    if(count != HPPA_FRAME_THREAD_STATE_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not HPPA_FRAME_THREAD_STATE_COUNT for "
				    "flavor number %lu which is a HPPA_FRAME"
				    "_THREAD_STATE flavor in %s command %lu)",
				    nflavor, ut->cmd == LC_UNIXTHREAD ? 
				    "LC_UNIXTHREAD" : "LC_THREAD", i);
				return(FALSE);
			    }
			    frame = (struct hp_pa_frame_thread_state *)state;
			    state += sizeof(struct hp_pa_frame_thread_state);
			    break;
			case HPPA_FP_THREAD_STATE:
			    if(count != HPPA_FP_THREAD_STATE_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not HPPA_FP_THREAD_STATE_COUNT for "
				    "flavor number %lu which is a HPPA_FP"
				    "_THREAD_STATE flavor in %s command %lu)",
				    nflavor, ut->cmd == LC_UNIXTHREAD ? 
				    "LC_UNIXTHREAD" : "LC_THREAD", i);
				return(FALSE);
			    }
			    fpu = (struct hp_pa_fp_thread_state *)state;
			    state += sizeof(struct hp_pa_fp_thread_state);
			    break;
			default:
			    error("in swap_object_headers(): malformed "
				"load commands (unknown "
				"flavor %u for flavor number %lu in %s command"
				" %lu can't byte swap it)", flavor, nflavor,
				ut->cmd == LC_UNIXTHREAD ? "LC_UNIXTHREAD" :
				"LC_THREAD", i);
			    return(FALSE);
			}
			nflavor++;
		    }
		    break;
		}
		if(cputype == CPU_TYPE_SPARC) {
		  struct sparc_thread_state_regs *cpu;
		  struct sparc_thread_state_fpu *fpu;

		  nflavor = 0;
		  p = (char *)ut + ut->cmdsize;
		  while (state < p) {
		    flavor = *((uint32_t *) state);
		    state += sizeof(uint32_t);
		    count = *((uint32_t *) state);
		    state += sizeof(uint32_t);
		    switch (flavor) {
		    case SPARC_THREAD_STATE_REGS:
		      if (count != SPARC_THREAD_STATE_REGS_COUNT) {
			error("in swap_object_headers(): malformed "
			      "load commands (count "
			      "not SPARC_THREAD_STATE_REGS_COUNT for "
			      "flavor number %lu which is a SPARC_THREAD_"
			      "STATE_REGS flavor in %s command %lu)",
			      nflavor, ut->cmd == LC_UNIXTHREAD ? 
			      "LC_UNIXTHREAD" : "LC_THREAD", i);
			return(FALSE);
		      }
		      cpu = (struct sparc_thread_state_regs *) state;
		      state += sizeof(struct sparc_thread_state_regs);
		      break;
		    case SPARC_THREAD_STATE_FPU:
		      if (count != SPARC_THREAD_STATE_FPU_COUNT) {
			error("in swap_object_headers(): malformed "
			      "load commands (count "
			      "not SPARC_THREAD_STATE_FPU_COUNT for "
			      "flavor number %lu which is a SPARC_THREAD_"
			      "STATE_FPU flavor in %s command %lu)",
			      nflavor, ut->cmd == LC_UNIXTHREAD ? 
			      "LC_UNIXTHREAD" : "LC_THREAD", i);
			return(FALSE);
		      }
		      fpu = (struct sparc_thread_state_fpu *) state;
		      state += sizeof(struct sparc_thread_state_fpu);
		      break;
		    }
		  }
		  break;
		}
	    	if(cputype == CPU_TYPE_ARM){
		    arm_thread_state_t *cpu;

		    nflavor = 0;
		    p = (char *)ut + ut->cmdsize;
		    while(state < p){
			flavor = *((uint32_t *)state);
			state += sizeof(uint32_t);
			count = *((uint32_t *)state);
			state += sizeof(uint32_t);
			switch(flavor){
			case ARM_THREAD_STATE:
			    if(count != ARM_THREAD_STATE_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not ARM_THREAD_STATE_COUNT for "
				    "flavor number %lu which is a ARM_THREAD_"
				    "STATE flavor in %s command %lu)",
				    nflavor, ut->cmd == LC_UNIXTHREAD ? 
				    "LC_UNIXTHREAD" : "LC_THREAD", i);
				return(FALSE);
			    }
			    cpu = (arm_thread_state_t *)state;
			    state += sizeof(arm_thread_state_t);
			    break;
			default:
			    error("in swap_object_headers(): malformed load "
				  "commands (unknown flavor for flavor number "
				  "%lu in %s command %lu can't byte swap it)",
				  nflavor, ut->cmd == LC_UNIXTHREAD ?
				  "LC_UNIXTHREAD" : "LC_THREAD", i);
			    return(FALSE);
			}
			nflavor++;
		    }
		    break;
		}
		error("in swap_object_headers(): malformed load commands "
		    "(unknown cputype (%d) and cpusubtype (%d) of object and "
                    "can't byte swap %s command %lu)", cputype, 
		    cpusubtype, ut->cmd == LC_UNIXTHREAD ?
		    "LC_UNIXTHREAD" : "LC_THREAD", i);
		return(FALSE);
	    case LC_IDENT:
		id = (struct ident_command *)lc;
		if((char *)id + id->cmdsize >
		   (char *)load_commands + sizeofcmds){
		    error("in swap_object_headers(): truncated or malformed "
			"load commands (cmdsize field of LC_IDENT command %lu "
			"extends past the end of the load commands)", i);
		    return(FALSE);
		}
		break;

	    case LC_ROUTINES:
		rc = (struct routines_command *)lc;
		if(rc->cmdsize != sizeof(struct routines_command)){
		    error("in swap_object_headers(): malformed load commands ("
			  "LC_ROUTINES command %lu has incorrect cmdsize",
			  i);
		    return(FALSE);
		}
		break;

	    case LC_ROUTINES_64:
		rc64 = (struct routines_command_64 *)lc;
		if(rc64->cmdsize != sizeof(struct routines_command_64)){
		    error("in swap_object_headers(): malformed load commands ("
			  "LC_ROUTINES_64 command %lu has incorrect cmdsize",
			  i);
		    return(FALSE);
		}
		break;

	    case LC_TWOLEVEL_HINTS:
		hints = (struct twolevel_hints_command *)lc;
		if(hints->cmdsize != sizeof(struct twolevel_hints_command)){
		    error("in swap_object_headers(): malformed load commands "
			  "(LC_TWOLEVEL_HINTS command %lu has incorrect "
			  "cmdsize", i);
		    return(FALSE);
		}
		break;

	    case LC_PREBIND_CKSUM:
		cs = (struct prebind_cksum_command *)lc;
		if(cs->cmdsize != sizeof(struct prebind_cksum_command)){
		    error("in swap_object_headers(): malformed load commands "
			  "(LC_PREBIND_CKSUM command %lu has incorrect cmdsize",
			  i);
		    return(FALSE);
		}
		break;

	    case LC_UUID:
		uuid = (struct uuid_command *)lc;
		if(uuid->cmdsize != sizeof(struct uuid_command)){
		    error("in swap_object_headers(): malformed load commands "
			  "(LC_UUID command %lu has incorrect cmdsize", i);
		    return(FALSE);
		}
		break;

	    case LC_CODE_SIGNATURE:
		ld = (struct linkedit_data_command *)lc;
		if(ld->cmdsize != sizeof(struct linkedit_data_command)){
		    error("in swap_object_headers(): malformed load commands "
			  "(LC_CODE_SIGNATURE command %lu has incorrect "
			  "cmdsize", i);
		    return(FALSE);
		}
		break;

	    case LC_SEGMENT_SPLIT_INFO:
		ld = (struct linkedit_data_command *)lc;
		if(ld->cmdsize != sizeof(struct linkedit_data_command)){
		    error("in swap_object_headers(): malformed load commands "
			  "(LC_SEGMENT_SPLIT_INFO command %lu has incorrect "
			  "cmdsize", i);
		    return(FALSE);
		}
		break;

	    case LC_RPATH:
		rpath = (struct rpath_command *)lc;
		if(rpath->cmdsize < sizeof(struct rpath_command)){
		    error("in swap_object_headers(): malformed load commands "
			  "(LC_RPATH command %lu has too small cmdsize field)",
			  i);
		    return(FALSE);
		}
		if(rpath->path.offset >= rpath->cmdsize){
		    error("in swap_object_headers(): truncated or malformed "
			  "load commands (path.offset field of LC_RPATH "
			  "command %lu extends past the end of all load "
			  "commands)", i);
		    return(FALSE);
		}
		break;

	    case LC_ENCRYPTION_INFO:
		ec = (struct encryption_info_command *)lc;
		if(ec->cmdsize != sizeof(struct encryption_info_command)){
		    error("in swap_object_headers(): malformed load commands "
			  "(LC_ENCRYPTION_INFO command %lu has incorrect "
			  "cmdsize", i);
		    return(FALSE);
		}
		break;

	    case LC_DYLD_INFO:
	    case LC_DYLD_INFO_ONLY:
		dc = (struct dyld_info_command *)lc;
		if(dc->cmdsize != sizeof(struct dyld_info_command)){
		    error("in swap_object_headers(): malformed load commands "
			  "(LC_DYLD_INFO command %lu has incorrect "
			  "cmdsize", i);
		    return(FALSE);
		}
		break;

	    default:
		error("in swap_object_headers(): malformed load commands "
		      "(unknown load command %lu)", i);
		return(FALSE);
	    }

	    lc = (struct load_command *)((char *)lc + l.cmdsize);
	    /* check that next load command does not extends past the end */
	    if((char *)lc > (char *)load_commands + sizeofcmds){
		error("in swap_object_headers(): truncated or malformed load "
		      "commands (load command %lu extends past the end of all "
		      "load commands)", i + 1);
		return(FALSE);
	    }
	}
	/* check for an inconsistent size of the load commands */
	if((char *)load_commands + sizeofcmds != (char *)lc){
	    error("in swap_object_headers(): malformed load commands "
		  "(inconsistent sizeofcmds field in mach header)");
	    return(FALSE);
	}


	/*
	 * Now knowing the load commands can be parsed swap them.
	 */
	target_byte_sex = get_host_byte_sex() == BIG_ENDIAN_BYTE_SEX ?
			  LITTLE_ENDIAN_BYTE_SEX : BIG_ENDIAN_BYTE_SEX;
	for(i = 0, lc = load_commands; i < ncmds; i++){
	    l = *lc;
	    switch(lc->cmd){
	    case LC_SEGMENT:
		sg = (struct segment_command *)lc;
		s = (struct section *)
		    ((char *)sg + sizeof(struct segment_command));
		swap_section(s, sg->nsects, target_byte_sex);
		swap_segment_command(sg, target_byte_sex);
		break;

	    case LC_SEGMENT_64:
		sg64 = (struct segment_command_64 *)lc;
		s64 = (struct section_64 *)
		      ((char *)sg64 + sizeof(struct segment_command_64));
		swap_section_64(s64, sg64->nsects, target_byte_sex);
		swap_segment_command_64(sg64, target_byte_sex);
		break;

	    case LC_SYMTAB:
		st = (struct symtab_command *)lc;
		swap_symtab_command(st, target_byte_sex);
		break;

	    case LC_DYSYMTAB:
		dyst = (struct dysymtab_command *)lc;
		swap_dysymtab_command(dyst, target_byte_sex);
		break;

	    case LC_SYMSEG:
		ss = (struct symseg_command *)lc;
		swap_symseg_command(ss, target_byte_sex);
		break;

	    case LC_IDFVMLIB:
	    case LC_LOADFVMLIB:
		fl = (struct fvmlib_command *)lc;
		swap_fvmlib_command(fl, target_byte_sex);
		break;

	    case LC_ID_DYLIB:
	    case LC_LOAD_DYLIB:
	    case LC_LOAD_WEAK_DYLIB:
	    case LC_REEXPORT_DYLIB:
	    case LC_LAZY_LOAD_DYLIB:
		dl = (struct dylib_command *)lc;
		swap_dylib_command(dl, target_byte_sex);
		break;

	    case LC_SUB_FRAMEWORK:
		sub = (struct sub_framework_command *)lc;
		swap_sub_framework_command(sub, target_byte_sex);
		break;

	    case LC_SUB_UMBRELLA:
		usub = (struct sub_umbrella_command *)lc;
		swap_sub_umbrella_command(usub, target_byte_sex);
		break;

	    case LC_SUB_LIBRARY:
		lsub = (struct sub_library_command *)lc;
		swap_sub_library_command(lsub, target_byte_sex);
		break;

	    case LC_SUB_CLIENT:
		csub = (struct sub_client_command *)lc;
		swap_sub_client_command(csub, target_byte_sex);
		break;

	    case LC_PREBOUND_DYLIB:
		pbdylib = (struct prebound_dylib_command *)lc;
		swap_prebound_dylib_command(pbdylib, target_byte_sex);
		break;

	    case LC_ID_DYLINKER:
	    case LC_LOAD_DYLINKER:
		dyld = (struct dylinker_command *)lc;
		swap_dylinker_command(dyld, target_byte_sex);
		break;

	    case LC_UNIXTHREAD:
	    case LC_THREAD:
		ut = (struct thread_command *)lc;
		state = (char *)ut + sizeof(struct thread_command);
		p = (char *)ut + ut->cmdsize;
		swap_thread_command(ut, target_byte_sex);

	    	if(cputype == CPU_TYPE_MC680x0){
		    struct m68k_thread_state_regs *cpu;
		    struct m68k_thread_state_68882 *fpu;
		    struct m68k_thread_state_user_reg *user_reg;

		    while(state < p){
			flavor = *((uint32_t *)state);
			*((uint32_t *)state) = SWAP_INT(flavor);
			state += sizeof(uint32_t);
			count = *((uint32_t *)state);
			*((uint32_t *)state) = SWAP_INT(count);
			state += sizeof(uint32_t);
			switch(flavor){
			case M68K_THREAD_STATE_REGS:
			    cpu = (struct m68k_thread_state_regs *)state;
			    swap_m68k_thread_state_regs(cpu, target_byte_sex);
			    state += sizeof(struct m68k_thread_state_regs);
			    break;
			case M68K_THREAD_STATE_68882:
			    fpu = (struct m68k_thread_state_68882 *)state;
			    swap_m68k_thread_state_68882(fpu, target_byte_sex);
			    state += sizeof(struct m68k_thread_state_68882);
			    break;
			case M68K_THREAD_STATE_USER_REG:
			    user_reg =
				(struct m68k_thread_state_user_reg *)state;
			    swap_m68k_thread_state_user_reg(user_reg,
							    target_byte_sex);
			    state += sizeof(struct m68k_thread_state_user_reg);
			    break;
			}
		    }
		    break;
		}
	    	if(cputype == CPU_TYPE_POWERPC ||
	    	   cputype == CPU_TYPE_VEO ||
		   cputype == CPU_TYPE_POWERPC64){
		    ppc_thread_state_t *cpu;
		    ppc_thread_state64_t *cpu64;
		    ppc_float_state_t *fpu;
		    ppc_exception_state_t *except;

		    while(state < p){
			flavor = *((uint32_t *)state);
			*((uint32_t *)state) = SWAP_INT(flavor);
			state += sizeof(uint32_t);
			count = *((uint32_t *)state);
			*((uint32_t *)state) = SWAP_INT(count);
			state += sizeof(uint32_t);
			switch(flavor){
			case PPC_THREAD_STATE:
			    cpu = (ppc_thread_state_t *)state;
			    swap_ppc_thread_state_t(cpu, target_byte_sex);
			    state += sizeof(ppc_thread_state_t);
			    break;
			case PPC_THREAD_STATE64:
			    cpu64 = (ppc_thread_state64_t *)state;
			    swap_ppc_thread_state64_t(cpu64, target_byte_sex);
			    state += sizeof(ppc_thread_state64_t);
			    break;
			case PPC_FLOAT_STATE:
			    fpu = (ppc_float_state_t *)state;
			    swap_ppc_float_state_t(fpu, target_byte_sex);
			    state += sizeof(ppc_float_state_t);
			case PPC_EXCEPTION_STATE:
			    except = (ppc_exception_state_t *)state;
			    swap_ppc_exception_state_t(except, target_byte_sex);
			    state += sizeof(ppc_exception_state_t);
			    break;
			}
		    }
		    break;
		}
	    	if(cputype == CPU_TYPE_MC88000){
		    m88k_thread_state_grf_t *cpu;
		    m88k_thread_state_xrf_t *fpu;
		    m88k_thread_state_user_t *user;
		    m88110_thread_state_impl_t *spu;

		    while(state < p){
			flavor = *((uint32_t *)state);
			*((uint32_t *)state) = SWAP_INT(flavor);
			state += sizeof(uint32_t);
			count = *((uint32_t *)state);
			*((uint32_t *)state) = SWAP_INT(count);
			state += sizeof(uint32_t);
			switch(flavor){
			case M88K_THREAD_STATE_GRF:
			    cpu = (m88k_thread_state_grf_t *)state;
			    swap_m88k_thread_state_grf_t(cpu,
							 target_byte_sex);
			    state += sizeof(m88k_thread_state_grf_t);
			    break;
			case M88K_THREAD_STATE_XRF:
			    fpu = (m88k_thread_state_xrf_t *)state;
			    swap_m88k_thread_state_xrf_t(fpu,
							 target_byte_sex);
			    state += sizeof(m88k_thread_state_xrf_t);
			    break;
			case M88K_THREAD_STATE_USER:
			    user = (m88k_thread_state_user_t *)state;
			    swap_m88k_thread_state_user_t(user,
							  target_byte_sex);
			    state += sizeof(m88k_thread_state_user_t);
			    break;
			case M88110_THREAD_STATE_IMPL:
			    spu = (m88110_thread_state_impl_t *)state;
			    swap_m88110_thread_state_impl_t(spu,
							  target_byte_sex);
			    state += sizeof(m88110_thread_state_impl_t);
			    break;
			}
		    }
		    break;
		}
	    	if(cputype == CPU_TYPE_I860){
		    struct i860_thread_state_regs *cpu;

		    while(state < p){
			flavor = *((uint32_t *)state);
			*((uint32_t *)state) = SWAP_INT(flavor);
			state += sizeof(uint32_t);
			count = *((uint32_t *)state);
			*((uint32_t *)state) = SWAP_INT(count);
			state += sizeof(uint32_t);
			switch(flavor){
			case I860_THREAD_STATE_REGS:
			    cpu = (struct i860_thread_state_regs *)state;
			    swap_i860_thread_state_regs(cpu, target_byte_sex);
			    state += sizeof(struct i860_thread_state_regs);
			    break;
			}
		    }
		    break;
		}
	    	if(cputype == CPU_TYPE_I386
#ifdef x86_THREAD_STATE64
		   || cputype == CPU_TYPE_X86_64
#endif /* x86_THREAD_STATE64 */
		   ){
		    i386_thread_state_t *cpu;
#ifdef x86_THREAD_STATE64
		    x86_thread_state64_t *cpu64;
#endif /* x86_THREAD_STATE64 */
/* current i386 thread states */
#if i386_THREAD_STATE == 1
		    struct i386_float_state *fpu;
		    i386_exception_state_t *exc;
#endif /* i386_THREAD_STATE == 1 */

/* i386 thread states on older releases */
#if i386_THREAD_STATE == -1
		    i386_thread_fpstate_t *fpu;
		    i386_thread_exceptstate_t *exc;
		    i386_thread_cthreadstate_t *user;
#endif /* i386_THREAD_STATE == -1 */

		    while(state < p){
			flavor = *((uint32_t *)state);
			*((uint32_t *)state) = SWAP_INT(flavor);
			state += sizeof(uint32_t);
			count = *((uint32_t *)state);
			*((uint32_t *)state) = SWAP_INT(count);
			state += sizeof(uint32_t);
			switch((int)flavor){
			case i386_THREAD_STATE:
/* current i386 thread states */
#if i386_THREAD_STATE == 1
			case -1:
#endif /* i386_THREAD_STATE == 1 */
/* i386 thread states on older releases */
#if i386_THREAD_STATE == -1
			case 1:
#endif /* i386_THREAD_STATE == -1 */
			    cpu = (i386_thread_state_t *)state;
			    swap_i386_thread_state(cpu, target_byte_sex);
			    state += sizeof(i386_thread_state_t);
			    break;
/* current i386 thread states */
#if i386_THREAD_STATE == 1
			case i386_FLOAT_STATE:
			    fpu = (struct i386_float_state *)state;
			    swap_i386_float_state(fpu, target_byte_sex);
			    state += sizeof(struct i386_float_state);
			    break;
			case i386_EXCEPTION_STATE:
			    exc = (i386_exception_state_t *)state;
			    swap_i386_exception_state(exc, target_byte_sex);
			    state += sizeof(i386_exception_state_t);
			    break;
#endif /* i386_THREAD_STATE == 1 */

/* i386 thread states on older releases */
#if i386_THREAD_STATE == -1
			case i386_THREAD_FPSTATE:
			    fpu = (i386_thread_fpstate_t *)state;
			    swap_i386_thread_fpstate(fpu, target_byte_sex);
			    state += sizeof(i386_thread_fpstate_t);
			    break;
			case i386_THREAD_EXCEPTSTATE:
			    exc = (i386_thread_exceptstate_t *)state;
			    swap_i386_thread_exceptstate(exc, target_byte_sex);
			    state += sizeof(i386_thread_exceptstate_t);
			    break;
			case i386_THREAD_CTHREADSTATE:
			    user = (i386_thread_cthreadstate_t *)state;
			    swap_i386_thread_cthreadstate(user,target_byte_sex);
			    state += sizeof(i386_thread_cthreadstate_t);
			    break;
#endif /* i386_THREAD_STATE == -1 */
#ifdef x86_THREAD_STATE64
			case x86_THREAD_STATE64:
			    cpu64 = (x86_thread_state64_t *)state;
			    swap_x86_thread_state64(cpu64, target_byte_sex);
			    state += sizeof(x86_thread_state64_t);
			    break;
#endif /* x86_THREAD_STATE64 */
			}
		    }
		    break;
		}
	    	if(cputype == CPU_TYPE_HPPA){
		    struct hp_pa_integer_thread_state *cpu;
		    struct hp_pa_frame_thread_state *frame;
		    struct hp_pa_fp_thread_state *fpu;

		    while(state < p){
			flavor = *((uint32_t *)state);
			*((uint32_t *)state) = SWAP_INT(flavor);
			state += sizeof(uint32_t);
			count = *((uint32_t *)state);
			*((uint32_t *)state) = SWAP_INT(count);
			state += sizeof(uint32_t);
			switch(flavor){
			case HPPA_INTEGER_THREAD_STATE:
			    cpu = (struct hp_pa_integer_thread_state *)state;
			    swap_hppa_integer_thread_state(cpu,
							 target_byte_sex);
			    state += sizeof(struct hp_pa_integer_thread_state);
			    break;
			case HPPA_FRAME_THREAD_STATE:
			    frame = (struct hp_pa_frame_thread_state *)state;
			    swap_hppa_frame_thread_state(frame,
							 target_byte_sex);
			    state += sizeof(struct hp_pa_frame_thread_state);
			    break;
			case HPPA_FP_THREAD_STATE:
			    fpu = (struct hp_pa_fp_thread_state *)state;
			    swap_hppa_fp_thread_state(fpu,
						     target_byte_sex);
			    state += sizeof(struct hp_pa_fp_thread_state);
			    break;
			}
		    }
		    break;
		}

		if(cputype == CPU_TYPE_SPARC) {
		  struct sparc_thread_state_regs *cpu;
		  struct sparc_thread_state_fpu *fpu;

		  while (state < p) {
		    flavor = *((uint32_t *) state);
		    *((uint32_t *) state) = SWAP_INT(flavor);
		    state += sizeof(uint32_t);
		    count = *((unsigned int *) state);
		    *((unsigned int *) state) = SWAP_INT(count);
		    state += sizeof(uint32_t);
		    switch (flavor) {
		    case SPARC_THREAD_STATE_REGS:
		      cpu = (struct sparc_thread_state_regs *) state;
		      swap_sparc_thread_state_regs(cpu, target_byte_sex);
		      state += sizeof(struct sparc_thread_state_regs);
		      break;
		    case SPARC_THREAD_STATE_FPU:
		      fpu = (struct sparc_thread_state_fpu *) state;
		      swap_sparc_thread_state_fpu(fpu, target_byte_sex);
		      state += sizeof(struct sparc_thread_state_fpu);
		      break;
		    }
		  }
		  break;
		}
	    	if(cputype == CPU_TYPE_ARM){
		    arm_thread_state_t *cpu;

		    while(state < p){
			flavor = *((uint32_t *)state);
			*((uint32_t *)state) = SWAP_INT(flavor);
			state += sizeof(uint32_t);
			count = *((uint32_t *)state);
			*((uint32_t *)state) = SWAP_INT(count);
			state += sizeof(uint32_t);
			switch(flavor){
			case ARM_THREAD_STATE:
			    cpu = (arm_thread_state_t *)state;
			    swap_arm_thread_state_t(cpu, target_byte_sex);
			    state += sizeof(arm_thread_state_t);
			    break;
			}
		    }
		    break;
		}
		break;

	    case LC_IDENT:
		id = (struct ident_command *)lc;
		swap_ident_command(id, target_byte_sex);
		break;

	    case LC_ROUTINES:
		rc = (struct routines_command *)lc;
		swap_routines_command(rc, target_byte_sex);
		break;

	    case LC_ROUTINES_64:
		rc64 = (struct routines_command_64 *)lc;
		swap_routines_command_64(rc64, target_byte_sex);
		break;

	    case LC_TWOLEVEL_HINTS:
		hints = (struct twolevel_hints_command *)lc;
		swap_twolevel_hints_command(hints, target_byte_sex);
		break;

	    case LC_PREBIND_CKSUM:
		cs = (struct prebind_cksum_command *)lc;
		swap_prebind_cksum_command(cs, target_byte_sex);
		break;

	    case LC_UUID:
		uuid = (struct uuid_command *)lc;
		swap_uuid_command(uuid, target_byte_sex);
		break;

	    case LC_CODE_SIGNATURE:
	    case LC_SEGMENT_SPLIT_INFO:
		ld = (struct linkedit_data_command *)lc;
		swap_linkedit_data_command(ld, target_byte_sex);
		break;

	    case LC_RPATH:
		rpath = (struct rpath_command *)lc;
		swap_rpath_command(rpath, target_byte_sex);
		break;

	    case LC_ENCRYPTION_INFO:
		ec = (struct encryption_info_command *)lc;
		swap_encryption_command(ec, target_byte_sex);
		break;
		
	    case LC_DYLD_INFO:
	    case LC_DYLD_INFO_ONLY:
		dc = (struct dyld_info_command *)lc;
		swap_dyld_info_command(dc, target_byte_sex);
		break;
	    }

	    lc = (struct load_command *)((char *)lc + l.cmdsize);
	}
	if(mh != NULL)
	    swap_mach_header(mh, target_byte_sex);
	else
	    swap_mach_header_64(mh64, target_byte_sex);

	return(TRUE);
}

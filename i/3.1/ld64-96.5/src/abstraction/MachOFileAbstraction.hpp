/* -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*- 
 *
 * Copyright (c) 2005-2008 Apple Inc. All rights reserved.
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
#ifndef __MACH_O_FILE_ABSTRACTION__
#define __MACH_O_FILE_ABSTRACTION__

#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/reloc.h>
#include <mach-o/fat.h>
#include <mach-o/stab.h>
#include <mach-o/reloc.h>
#include <mach-o/ppc/reloc.h>
#include <mach-o/x86_64/reloc.h>
#include <mach-o/arm/reloc.h>
#include <mach-o/compact_unwind_encoding.h>
#include <mach/machine.h>

#include "FileAbstraction.hpp"


// stuff that will eventually go away once newer cctools headers are widespread
#ifndef CPU_SUBTYPE_ARM_V5TEJ
	#define CPU_SUBTYPE_ARM_V5TEJ		((cpu_subtype_t) 7)
#endif
#ifndef CPU_SUBTYPE_ARM_XSCALE
	#define CPU_SUBTYPE_ARM_XSCALE		((cpu_subtype_t) 8)
#endif
#ifndef CPU_SUBTYPE_ARM_V7
	#define CPU_SUBTYPE_ARM_V7			((cpu_subtype_t) 9)
#endif
#ifndef ARM_THUMB_32BIT_BRANCH
	#define ARM_THUMB_32BIT_BRANCH	7 
#endif
#ifndef N_ARM_THUMB_DEF
	#define N_ARM_THUMB_DEF	0x0008 
#endif
#ifndef MH_DEAD_STRIPPABLE_DYLIB
	#define MH_DEAD_STRIPPABLE_DYLIB 0x400000
#endif
#ifndef MH_KEXT_BUNDLE
	#define MH_KEXT_BUNDLE 11
#endif
#ifndef LC_DYLD_INFO
	#define	LC_DYLD_INFO 	0x22	/* compressed dyld information */
	#define	LC_DYLD_INFO_ONLY (0x22|LC_REQ_DYLD)	/* compressed dyld information only */

	struct dyld_info_command {
	   uint32_t   cmd;		/* LC_DYLD_INFO or LC_DYLD_INFO_ONLY */
	   uint32_t   cmdsize;		/* sizeof(struct dyld_info_command) */
		uint32_t   rebase_off;	/* file offset to rebase info  */
		uint32_t   rebase_size;	/* size of rebase info   */
		uint32_t   bind_off;	/* file offset to binding info   */
		uint32_t   bind_size;	/* size of binding info  */
		uint32_t   weak_bind_off;	/* file offset to weak binding info   */
		uint32_t   weak_bind_size;  /* size of weak binding info  */
		uint32_t   lazy_bind_off;	/* file offset to lazy binding info */
		uint32_t   lazy_bind_size;  /* size of lazy binding infs */
		uint32_t   export_off;	/* file offset to lazy binding info */
		uint32_t   export_size;	/* size of lazy binding infs */
	};

	#define REBASE_TYPE_POINTER					1
	#define REBASE_TYPE_TEXT_ABSOLUTE32				2
	#define REBASE_TYPE_TEXT_PCREL32				3

	#define REBASE_OPCODE_MASK					0xF0
	#define REBASE_IMMEDIATE_MASK					0x0F
	#define REBASE_OPCODE_DONE					0x00
	#define REBASE_OPCODE_SET_TYPE_IMM				0x10
	#define REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB		0x20
	#define REBASE_OPCODE_ADD_ADDR_ULEB				0x30
	#define REBASE_OPCODE_ADD_ADDR_IMM_SCALED			0x40
	#define REBASE_OPCODE_DO_REBASE_IMM_TIMES			0x50
	#define REBASE_OPCODE_DO_REBASE_ULEB_TIMES			0x60
	#define REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB			0x70
	#define REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB	0x80

	#define BIND_TYPE_POINTER					1
	#define BIND_TYPE_TEXT_ABSOLUTE32				2
	#define BIND_TYPE_TEXT_PCREL32					3

	#define BIND_SPECIAL_DYLIB_SELF					 0
	#define BIND_SPECIAL_DYLIB_MAIN_EXECUTABLE			-1
	#define BIND_SPECIAL_DYLIB_FLAT_LOOKUP				-2

	#define BIND_SYMBOL_FLAGS_WEAK_IMPORT				0x1
	#define BIND_SYMBOL_FLAGS_NON_WEAK_DEFINITION			0x8

	#define BIND_OPCODE_MASK					0xF0
	#define BIND_IMMEDIATE_MASK					0x0F
	#define BIND_OPCODE_DONE					0x00
	#define BIND_OPCODE_SET_DYLIB_ORDINAL_IMM			0x10
	#define BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB			0x20
	#define BIND_OPCODE_SET_DYLIB_SPECIAL_IMM			0x30
	#define BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM		0x40
	#define BIND_OPCODE_SET_TYPE_IMM				0x50
	#define BIND_OPCODE_SET_ADDEND_SLEB				0x60
	#define BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB			0x70
	#define BIND_OPCODE_ADD_ADDR_ULEB				0x80
	#define BIND_OPCODE_DO_BIND					0x90
	#define BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB			0xA0
	#define BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED			0xB0
	#define BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB		0xC0

	#define EXPORT_SYMBOL_FLAGS_KIND_MASK				0x03
	#define EXPORT_SYMBOL_FLAGS_KIND_REGULAR			0x00
	#define EXPORT_SYMBOL_FLAGS_KIND_THREAD_LOCAL			0x01
	#define EXPORT_SYMBOL_FLAGS_WEAK_DEFINITION			0x04
	#define EXPORT_SYMBOL_FLAGS_INDIRECT_DEFINITION			0x08
	#define EXPORT_SYMBOL_FLAGS_HAS_SPECIALIZATIONS			0x10

#endif 


//
// This abstraction layer makes every mach-o file look like a 64-bit mach-o file with native endianness
//



//
// mach-o file header
//
template <typename P> struct macho_header_content {};
template <> struct macho_header_content<Pointer32<BigEndian> >    { mach_header		fields; };
template <> struct macho_header_content<Pointer64<BigEndian> >	  { mach_header_64	fields; };
template <> struct macho_header_content<Pointer32<LittleEndian> > { mach_header		fields; };
template <> struct macho_header_content<Pointer64<LittleEndian> > { mach_header_64	fields; };

template <typename P>
class macho_header {
public:
	uint32_t		magic() const					INLINE { return E::get32(header.fields.magic); }
	void			set_magic(uint32_t value)		INLINE { E::set32(header.fields.magic, value); }

	uint32_t		cputype() const					INLINE { return E::get32(header.fields.cputype); }
	void			set_cputype(uint32_t value)		INLINE { E::set32((uint32_t&)header.fields.cputype, value); }

	uint32_t		cpusubtype() const				INLINE { return E::get32(header.fields.cpusubtype); }
	void			set_cpusubtype(uint32_t value)	INLINE { E::set32((uint32_t&)header.fields.cpusubtype, value); }

	uint32_t		filetype() const				INLINE { return E::get32(header.fields.filetype); }
	void			set_filetype(uint32_t value)	INLINE { E::set32(header.fields.filetype, value); }

	uint32_t		ncmds() const					INLINE { return E::get32(header.fields.ncmds); }
	void			set_ncmds(uint32_t value)		INLINE { E::set32(header.fields.ncmds, value); }

	uint32_t		sizeofcmds() const				INLINE { return E::get32(header.fields.sizeofcmds); }
	void			set_sizeofcmds(uint32_t value)	INLINE { E::set32(header.fields.sizeofcmds, value); }

	uint32_t		flags() const					INLINE { return E::get32(header.fields.flags); }
	void			set_flags(uint32_t value)		INLINE { E::set32(header.fields.flags, value); }

	uint32_t		reserved() const				INLINE { return E::get32(header.fields.reserved); }
	void			set_reserved(uint32_t value)	INLINE { E::set32(header.fields.reserved, value); }

	typedef typename P::E		E;
private:
	macho_header_content<P>	header;
};


//
// mach-o load command
//
template <typename P>
class macho_load_command {
public:
	uint32_t		cmd() const						INLINE { return E::get32(command.cmd); }
	void			set_cmd(uint32_t value)			INLINE { E::set32(command.cmd, value); }

	uint32_t		cmdsize() const					INLINE { return E::get32(command.cmdsize); }
	void			set_cmdsize(uint32_t value)		INLINE { E::set32(command.cmdsize, value); }

	typedef typename P::E		E;
private:
	load_command	command;
};


//
// mach-o segment load command
//
template <typename P> struct macho_segment_content {};
template <> struct macho_segment_content<Pointer32<BigEndian> >    { segment_command	fields; enum { CMD = LC_SEGMENT		}; };
template <> struct macho_segment_content<Pointer64<BigEndian> >	   { segment_command_64	fields; enum { CMD = LC_SEGMENT_64	}; };
template <> struct macho_segment_content<Pointer32<LittleEndian> > { segment_command	fields; enum { CMD = LC_SEGMENT		}; };
template <> struct macho_segment_content<Pointer64<LittleEndian> > { segment_command_64	fields; enum { CMD = LC_SEGMENT_64	}; };

template <typename P>
class macho_segment_command {
public:
	uint32_t		cmd() const						INLINE { return E::get32(segment.fields.cmd); }
	void			set_cmd(uint32_t value)			INLINE { E::set32(segment.fields.cmd, value); }

	uint32_t		cmdsize() const					INLINE { return E::get32(segment.fields.cmdsize); }
	void			set_cmdsize(uint32_t value)		INLINE { E::set32(segment.fields.cmdsize, value); }

	const char*		segname() const					INLINE { return segment.fields.segname; }
	void			set_segname(const char* value)	INLINE { strncpy(segment.fields.segname, value, 16); }
	
	uint64_t		vmaddr() const					INLINE { return P::getP(segment.fields.vmaddr); }
	void			set_vmaddr(uint64_t value)		INLINE { P::setP(segment.fields.vmaddr, value); }

	uint64_t		vmsize() const					INLINE { return P::getP(segment.fields.vmsize); }
	void			set_vmsize(uint64_t value)		INLINE { P::setP(segment.fields.vmsize, value); }

	uint64_t		fileoff() const					INLINE { return P::getP(segment.fields.fileoff); }
	void			set_fileoff(uint64_t value)		INLINE { P::setP(segment.fields.fileoff, value); }

	uint64_t		filesize() const				INLINE { return P::getP(segment.fields.filesize); }
	void			set_filesize(uint64_t value)	INLINE { P::setP(segment.fields.filesize, value); }

	uint32_t		maxprot() const					INLINE { return E::get32(segment.fields.maxprot); }
	void			set_maxprot(uint32_t value)		INLINE { E::set32((uint32_t&)segment.fields.maxprot, value); }

	uint32_t		initprot() const				INLINE { return E::get32(segment.fields.initprot); }
	void			set_initprot(uint32_t value)	INLINE { E::set32((uint32_t&)segment.fields.initprot, value); }

	uint32_t		nsects() const					INLINE { return E::get32(segment.fields.nsects); }
	void			set_nsects(uint32_t value)		INLINE { E::set32(segment.fields.nsects, value); }

	uint32_t		flags() const					INLINE { return E::get32(segment.fields.flags); }
	void			set_flags(uint32_t value)		INLINE { E::set32(segment.fields.flags, value); }

	enum {
		CMD = macho_segment_content<P>::CMD
	};

	typedef typename P::E		E;
private:
	macho_segment_content<P>	segment;
};


//
// mach-o section 
//
template <typename P> struct macho_section_content {};
template <> struct macho_section_content<Pointer32<BigEndian> >    { section	fields; };
template <> struct macho_section_content<Pointer64<BigEndian> >	   { section_64	fields; };
template <> struct macho_section_content<Pointer32<LittleEndian> > { section	fields; };
template <> struct macho_section_content<Pointer64<LittleEndian> > { section_64	fields; };

template <typename P>
class macho_section {
public:
	const char*		sectname() const				INLINE { return section.fields.sectname; }
	void			set_sectname(const char* value)	INLINE { strncpy(section.fields.sectname, value, 16); }
	
	const char*		segname() const					INLINE { return section.fields.segname; }
	void			set_segname(const char* value)	INLINE { strncpy(section.fields.segname, value, 16); }
	
	uint64_t		addr() const					INLINE { return P::getP(section.fields.addr); }
	void			set_addr(uint64_t value)		INLINE { P::setP(section.fields.addr, value); }

	uint64_t		size() const					INLINE { return P::getP(section.fields.size); }
	void			set_size(uint64_t value)		INLINE { P::setP(section.fields.size, value); }

	uint32_t		offset() const					INLINE { return E::get32(section.fields.offset); }
	void			set_offset(uint32_t value)		INLINE { E::set32(section.fields.offset, value); }

	uint32_t		align() const					INLINE { return E::get32(section.fields.align); }
	void			set_align(uint32_t value)		INLINE { E::set32(section.fields.align, value); }

	uint32_t		reloff() const					INLINE { return E::get32(section.fields.reloff); }
	void			set_reloff(uint32_t value)		INLINE { E::set32(section.fields.reloff, value); }

	uint32_t		nreloc() const					INLINE { return E::get32(section.fields.nreloc); }
	void			set_nreloc(uint32_t value)		INLINE { E::set32(section.fields.nreloc, value); }

	uint32_t		flags() const					INLINE { return E::get32(section.fields.flags); }
	void			set_flags(uint32_t value)		INLINE { E::set32(section.fields.flags, value); }

	uint32_t		reserved1() const				INLINE { return E::get32(section.fields.reserved1); }
	void			set_reserved1(uint32_t value)	INLINE { E::set32(section.fields.reserved1, value); }

	uint32_t		reserved2() const				INLINE { return E::get32(section.fields.reserved2); }
	void			set_reserved2(uint32_t value)	INLINE { E::set32(section.fields.reserved2, value); }

	typedef typename P::E		E;
private:
	macho_section_content<P>	section;
};


//
// mach-o dylib load command
//
template <typename P>
class macho_dylib_command {
public:
	uint32_t		cmd() const									INLINE { return E::get32(fields.cmd); }
	void			set_cmd(uint32_t value)						INLINE { E::set32(fields.cmd, value); }

	uint32_t		cmdsize() const								INLINE { return E::get32(fields.cmdsize); }
	void			set_cmdsize(uint32_t value)					INLINE { E::set32(fields.cmdsize, value); }

	uint32_t		name_offset() const							INLINE { return E::get32(fields.dylib.name.offset); }
	void			set_name_offset(uint32_t value)				INLINE { E::set32(fields.dylib.name.offset, value);  }
	
	uint32_t		timestamp() const							INLINE { return E::get32(fields.dylib.timestamp); }
	void			set_timestamp(uint32_t value)				INLINE { E::set32(fields.dylib.timestamp, value); }

	uint32_t		current_version() const						INLINE { return E::get32(fields.dylib.current_version); }
	void			set_current_version(uint32_t value)			INLINE { E::set32(fields.dylib.current_version, value); }

	uint32_t		compatibility_version() const				INLINE { return E::get32(fields.dylib.compatibility_version); }
	void			set_compatibility_version(uint32_t value)	INLINE { E::set32(fields.dylib.compatibility_version, value); }

	const char*		name() const								INLINE { return (const char*)&fields + name_offset(); }
	void			set_name_offset()							INLINE { set_name_offset(sizeof(fields)); }
	
	typedef typename P::E		E;
private:
	dylib_command	fields;
};


//
// mach-o dylinker load command
//
template <typename P>
class macho_dylinker_command {
public:
	uint32_t		cmd() const							INLINE { return E::get32(fields.cmd); }
	void			set_cmd(uint32_t value)				INLINE { E::set32(fields.cmd, value); }

	uint32_t		cmdsize() const						INLINE { return E::get32(fields.cmdsize); }
	void			set_cmdsize(uint32_t value)			INLINE { E::set32(fields.cmdsize, value); }

	uint32_t		name_offset() const					INLINE { return E::get32(fields.name.offset); }
	void			set_name_offset(uint32_t value)		INLINE { E::set32(fields.name.offset, value);  }
	
	const char*		name() const						INLINE { return (const char*)&fields + name_offset(); }
	void			set_name_offset()					INLINE { set_name_offset(sizeof(fields)); }
	
	typedef typename P::E		E;
private:
	dylinker_command	fields;
};


//
// mach-o sub_framework load command
//
template <typename P>
class macho_sub_framework_command {
public:
	uint32_t		cmd() const							INLINE { return E::get32(fields.cmd); }
	void			set_cmd(uint32_t value)				INLINE { E::set32(fields.cmd, value); }

	uint32_t		cmdsize() const						INLINE { return E::get32(fields.cmdsize); }
	void			set_cmdsize(uint32_t value)			INLINE { E::set32(fields.cmdsize, value); }

	uint32_t		umbrella_offset() const				INLINE { return E::get32(fields.umbrella.offset); }
	void			set_umbrella_offset(uint32_t value)	INLINE { E::set32(fields.umbrella.offset, value);  }
	
	const char*		umbrella() const					INLINE { return (const char*)&fields + umbrella_offset(); }
	void			set_umbrella_offset()				INLINE { set_umbrella_offset(sizeof(fields)); }
		
	typedef typename P::E		E;
private:
	sub_framework_command	fields;
};


//
// mach-o sub_client load command
//
template <typename P>
class macho_sub_client_command {
public:
	uint32_t		cmd() const							INLINE { return E::get32(fields.cmd); }
	void			set_cmd(uint32_t value)				INLINE { E::set32(fields.cmd, value); }

	uint32_t		cmdsize() const						INLINE { return E::get32(fields.cmdsize); }
	void			set_cmdsize(uint32_t value)			INLINE { E::set32(fields.cmdsize, value); }

	uint32_t		client_offset() const				INLINE { return E::get32(fields.client.offset); }
	void			set_client_offset(uint32_t value)	INLINE { E::set32(fields.client.offset, value);  }
	
	const char*		client() const						INLINE { return (const char*)&fields + client_offset(); }
	void			set_client_offset()					INLINE { set_client_offset(sizeof(fields)); }
		
	typedef typename P::E		E;
private:
	sub_client_command	fields;
};


//
// mach-o sub_umbrella load command
//
template <typename P>
class macho_sub_umbrella_command {
public:
	uint32_t		cmd() const								INLINE { return E::get32(fields.cmd); }
	void			set_cmd(uint32_t value)					INLINE { E::set32(fields.cmd, value); }

	uint32_t		cmdsize() const							INLINE { return E::get32(fields.cmdsize); }
	void			set_cmdsize(uint32_t value)				INLINE { E::set32(fields.cmdsize, value); }

	uint32_t		sub_umbrella_offset() const				INLINE { return E::get32(fields.sub_umbrella.offset); }
	void			set_sub_umbrella_offset(uint32_t value)	INLINE { E::set32(fields.sub_umbrella.offset, value);  }
	
	const char*		sub_umbrella() const					INLINE { return (const char*)&fields + sub_umbrella_offset(); }
	void			set_sub_umbrella_offset()				INLINE { set_sub_umbrella_offset(sizeof(fields)); }
		
	typedef typename P::E		E;
private:
	sub_umbrella_command	fields;
};


//
// mach-o sub_library load command
//
template <typename P>
class macho_sub_library_command {
public:
	uint32_t		cmd() const								INLINE { return E::get32(fields.cmd); }
	void			set_cmd(uint32_t value)					INLINE { E::set32(fields.cmd, value); }

	uint32_t		cmdsize() const							INLINE { return E::get32(fields.cmdsize); }
	void			set_cmdsize(uint32_t value)				INLINE { E::set32(fields.cmdsize, value); }

	uint32_t		sub_library_offset() const				INLINE { return E::get32(fields.sub_library.offset); }
	void			set_sub_library_offset(uint32_t value)	INLINE { E::set32(fields.sub_library.offset, value);  }
	
	const char*		sub_library() const						INLINE { return (const char*)&fields + sub_library_offset(); }
	void			set_sub_library_offset()				INLINE { set_sub_library_offset(sizeof(fields)); }
		
	typedef typename P::E		E;
private:
	sub_library_command	fields;
};


//
// mach-o uuid load command
//
template <typename P>
class macho_uuid_command {
public:
	uint32_t		cmd() const								INLINE { return E::get32(fields.cmd); }
	void			set_cmd(uint32_t value)					INLINE { E::set32(fields.cmd, value); }

	uint32_t		cmdsize() const							INLINE { return E::get32(fields.cmdsize); }
	void			set_cmdsize(uint32_t value)				INLINE { E::set32(fields.cmdsize, value); }

	const uint8_t*	uuid() const							INLINE { return fields.uuid; }
	void			set_uuid(uint8_t u[16])					INLINE { memcpy(&fields.uuid, u, 16); }
			
	typedef typename P::E		E;
private:
	uuid_command	fields;
};


//
// mach-o routines load command
//
template <typename P> struct macho_routines_content {};
template <> struct macho_routines_content<Pointer32<BigEndian> >    { routines_command		fields; enum { CMD = LC_ROUTINES	}; };
template <> struct macho_routines_content<Pointer64<BigEndian> >	{ routines_command_64	fields; enum { CMD = LC_ROUTINES_64	}; };
template <> struct macho_routines_content<Pointer32<LittleEndian> > { routines_command		fields; enum { CMD = LC_ROUTINES	}; };
template <> struct macho_routines_content<Pointer64<LittleEndian> > { routines_command_64	fields; enum { CMD = LC_ROUTINES_64	}; };

template <typename P>
class macho_routines_command {
public:
	uint32_t		cmd() const							INLINE { return E::get32(routines.fields.cmd); }
	void			set_cmd(uint32_t value)				INLINE { E::set32(routines.fields.cmd, value); }

	uint32_t		cmdsize() const						INLINE { return E::get32(routines.fields.cmdsize); }
	void			set_cmdsize(uint32_t value)			INLINE { E::set32(routines.fields.cmdsize, value); }

	uint64_t		init_address() const				INLINE { return P::getP(routines.fields.init_address); }
	void			set_init_address(uint64_t value)	INLINE { P::setP(routines.fields.init_address, value); }

	uint64_t		init_module() const					INLINE { return P::getP(routines.fields.init_module); }
	void			set_init_module(uint64_t value)		INLINE { P::setP(routines.fields.init_module, value); }

	uint64_t		reserved1() const					INLINE { return P::getP(routines.fields.reserved1); }
	void			set_reserved1(uint64_t value)		INLINE { P::setP(routines.fields.reserved1, value); }
	
	uint64_t		reserved2() const					INLINE { return P::getP(routines.fields.reserved2); }
	void			set_reserved2(uint64_t value)		INLINE { P::setP(routines.fields.reserved2, value); }
	
	uint64_t		reserved3() const					INLINE { return P::getP(routines.fields.reserved3); }
	void			set_reserved3(uint64_t value)		INLINE { P::setP(routines.fields.reserved3, value); }
	
	uint64_t		reserved4() const					INLINE { return P::getP(routines.fields.reserved4); }
	void			set_reserved4(uint64_t value)		INLINE { P::setP(routines.fields.reserved4, value); }
	
	uint64_t		reserved5() const					INLINE { return P::getP(routines.fields.reserved5); }
	void			set_reserved5(uint64_t value)		INLINE { P::setP(routines.fields.reserved5, value); }
	
	uint64_t		reserved6() const					INLINE { return P::getP(routines.fields.reserved6); }
	void			set_reserved6(uint64_t value)		INLINE { P::setP(routines.fields.reserved6, value); }
	
	typedef typename P::E		E;
	enum {
		CMD = macho_routines_content<P>::CMD
	};
private:
	macho_routines_content<P>	routines;
};


//
// mach-o symbol table load command
//
template <typename P>
class macho_symtab_command {
public:
	uint32_t		cmd() const					INLINE { return E::get32(fields.cmd); }
	void			set_cmd(uint32_t value)		INLINE { E::set32(fields.cmd, value); }

	uint32_t		cmdsize() const				INLINE { return E::get32(fields.cmdsize); }
	void			set_cmdsize(uint32_t value)	INLINE { E::set32(fields.cmdsize, value); }

	uint32_t		symoff() const				INLINE { return E::get32(fields.symoff); }
	void			set_symoff(uint32_t value)	INLINE { E::set32(fields.symoff, value);  }
	
	uint32_t		nsyms() const				INLINE { return E::get32(fields.nsyms); }
	void			set_nsyms(uint32_t value)	INLINE { E::set32(fields.nsyms, value);  }
	
	uint32_t		stroff() const				INLINE { return E::get32(fields.stroff); }
	void			set_stroff(uint32_t value)	INLINE { E::set32(fields.stroff, value);  }
	
	uint32_t		strsize() const				INLINE { return E::get32(fields.strsize); }
	void			set_strsize(uint32_t value)	INLINE { E::set32(fields.strsize, value);  }
	
	
	typedef typename P::E		E;
private:
	symtab_command	fields;
};


//
// mach-o dynamic symbol table load command
//
template <typename P>
class macho_dysymtab_command {
public:
	uint32_t		cmd() const							INLINE { return E::get32(fields.cmd); }
	void			set_cmd(uint32_t value)				INLINE { E::set32(fields.cmd, value); }

	uint32_t		cmdsize() const						INLINE { return E::get32(fields.cmdsize); }
	void			set_cmdsize(uint32_t value)			INLINE { E::set32(fields.cmdsize, value); }

	uint32_t		ilocalsym() const					INLINE { return E::get32(fields.ilocalsym); }
	void			set_ilocalsym(uint32_t value)		INLINE { E::set32(fields.ilocalsym, value);  }
	
	uint32_t		nlocalsym() const					INLINE { return E::get32(fields.nlocalsym); }
	void			set_nlocalsym(uint32_t value)		INLINE { E::set32(fields.nlocalsym, value);  }
	
	uint32_t		iextdefsym() const					INLINE { return E::get32(fields.iextdefsym); }
	void			set_iextdefsym(uint32_t value)		INLINE { E::set32(fields.iextdefsym, value);  }
	
	uint32_t		nextdefsym() const					INLINE { return E::get32(fields.nextdefsym); }
	void			set_nextdefsym(uint32_t value)		INLINE { E::set32(fields.nextdefsym, value);  }
	
	uint32_t		iundefsym() const					INLINE { return E::get32(fields.iundefsym); }
	void			set_iundefsym(uint32_t value)		INLINE { E::set32(fields.iundefsym, value);  }
	
	uint32_t		nundefsym() const					INLINE { return E::get32(fields.nundefsym); }
	void			set_nundefsym(uint32_t value)		INLINE { E::set32(fields.nundefsym, value);  }
	
	uint32_t		tocoff() const						INLINE { return E::get32(fields.tocoff); }
	void			set_tocoff(uint32_t value)			INLINE { E::set32(fields.tocoff, value);  }
	
	uint32_t		ntoc() const						INLINE { return E::get32(fields.ntoc); }
	void			set_ntoc(uint32_t value)			INLINE { E::set32(fields.ntoc, value);  }
	
	uint32_t		modtaboff() const					INLINE { return E::get32(fields.modtaboff); }
	void			set_modtaboff(uint32_t value)		INLINE { E::set32(fields.modtaboff, value);  }
	
	uint32_t		nmodtab() const						INLINE { return E::get32(fields.nmodtab); }
	void			set_nmodtab(uint32_t value)			INLINE { E::set32(fields.nmodtab, value);  }
	
	uint32_t		extrefsymoff() const				INLINE { return E::get32(fields.extrefsymoff); }
	void			set_extrefsymoff(uint32_t value)	INLINE { E::set32(fields.extrefsymoff, value);  }
	
	uint32_t		nextrefsyms() const					INLINE { return E::get32(fields.nextrefsyms); }
	void			set_nextrefsyms(uint32_t value)		INLINE { E::set32(fields.nextrefsyms, value);  }
	
	uint32_t		indirectsymoff() const				INLINE { return E::get32(fields.indirectsymoff); }
	void			set_indirectsymoff(uint32_t value)	INLINE { E::set32(fields.indirectsymoff, value);  }
	
	uint32_t		nindirectsyms() const				INLINE { return E::get32(fields.nindirectsyms); }
	void			set_nindirectsyms(uint32_t value)	INLINE { E::set32(fields.nindirectsyms, value);  }
	
	uint32_t		extreloff() const					INLINE { return E::get32(fields.extreloff); }
	void			set_extreloff(uint32_t value)		INLINE { E::set32(fields.extreloff, value);  }
	
	uint32_t		nextrel() const						INLINE { return E::get32(fields.nextrel); }
	void			set_nextrel(uint32_t value)			INLINE { E::set32(fields.nextrel, value);  }
	
	uint32_t		locreloff() const					INLINE { return E::get32(fields.locreloff); }
	void			set_locreloff(uint32_t value)		INLINE { E::set32(fields.locreloff, value);  }
	
	uint32_t		nlocrel() const						INLINE { return E::get32(fields.nlocrel); }
	void			set_nlocrel(uint32_t value)			INLINE { E::set32(fields.nlocrel, value);  }
	
	typedef typename P::E		E;
private:
	dysymtab_command	fields;
};




//
// mach-o module table entry (for compatibility with old ld/dyld)
//
template <typename P> struct macho_dylib_module_content {};
template <> struct macho_dylib_module_content<Pointer32<BigEndian> >    { struct dylib_module		fields; };
template <> struct macho_dylib_module_content<Pointer32<LittleEndian> > { struct dylib_module		fields; };
template <> struct macho_dylib_module_content<Pointer64<BigEndian> >    { struct dylib_module_64	fields; };
template <> struct macho_dylib_module_content<Pointer64<LittleEndian> > { struct dylib_module_64	fields; };

template <typename P>
class macho_dylib_module {
public:
	uint32_t		module_name() const				INLINE { return E::get32(module.fields.module_name); }
	void			set_module_name(uint32_t value)	INLINE { E::set32(module.fields.module_name, value);  }
	
	uint32_t		iextdefsym() const				INLINE { return E::get32(module.fields.iextdefsym); }
	void			set_iextdefsym(uint32_t value)	INLINE { E::set32(module.fields.iextdefsym, value);  }
	
	uint32_t		nextdefsym() const				INLINE { return E::get32(module.fields.nextdefsym); }
	void			set_nextdefsym(uint32_t value)	INLINE { E::set32(module.fields.nextdefsym, value);  }
	
	uint32_t		irefsym() const					INLINE { return E::get32(module.fields.irefsym); }
	void			set_irefsym(uint32_t value)		INLINE { E::set32(module.fields.irefsym, value);  }
	
	uint32_t		nrefsym() const					INLINE { return E::get32(module.fields.nrefsym); }
	void			set_nrefsym(uint32_t value)		INLINE { E::set32(module.fields.nrefsym, value);  }
	
	uint32_t		ilocalsym() const				INLINE { return E::get32(module.fields.ilocalsym); }
	void			set_ilocalsym(uint32_t value)	INLINE { E::set32(module.fields.ilocalsym, value);  }
	
	uint32_t		nlocalsym() const				INLINE { return E::get32(module.fields.nlocalsym); }
	void			set_nlocalsym(uint32_t value)	INLINE { E::set32(module.fields.nlocalsym, value);  }
	
	uint32_t		iextrel() const					INLINE { return E::get32(module.fields.iextrel); }
	void			set_iextrel(uint32_t value)		INLINE { E::set32(module.fields.iextrel, value);  }
	
	uint32_t		nextrel() const					INLINE { return E::get32(module.fields.nextrel); }
	void			set_nextrel(uint32_t value)		INLINE { E::set32(module.fields.nextrel, value);  }
	
	uint16_t		iinit() const					INLINE { return E::get32(module.fields.iinit_iterm) & 0xFFFF; }
	uint16_t		iterm() const					INLINE { return E::get32(module.fields.iinit_iterm) > 16; }
	void			set_iinit_iterm(uint16_t init, uint16_t term)	INLINE { E::set32(module.fields.iinit_iterm, (term<<16) | (init &0xFFFF));  }
	
	uint16_t		ninit() const					INLINE { return E::get32(module.fields.ninit_nterm) & 0xFFFF; }
	uint16_t		nterm() const					INLINE { return E::get32(module.fields.ninit_nterm) > 16; }
	void			set_ninit_nterm(uint16_t init, uint16_t term)	INLINE { E::set32(module.fields.ninit_nterm, (term<<16) | (init &0xFFFF));  }
	
	uint64_t		objc_module_info_addr() const				INLINE { return P::getP(module.fields.objc_module_info_addr); }
	void			set_objc_module_info_addr(uint64_t value)	INLINE { P::setP(module.fields.objc_module_info_addr, value);  }
	
	uint32_t		objc_module_info_size() const				INLINE { return E::get32(module.fields.objc_module_info_size); }
	void			set_objc_module_info_size(uint32_t value)	INLINE { E::set32(module.fields.objc_module_info_size, value);  }
	
	
	typedef typename P::E		E;
private:
	macho_dylib_module_content<P>	module;
};


//
// mach-o dylib_reference entry
//
template <typename P>
class macho_dylib_reference {
public:
	uint32_t		isym() const				INLINE { return E::getBits(fields, 0, 24); }
	void			set_isym(uint32_t value)	INLINE { E::setBits(fields, value, 0, 24); }
	
	uint8_t			flags() const				INLINE { return E::getBits(fields, 24, 8); }
	void			set_flags(uint8_t value)	INLINE { E::setBits(fields, value, 24, 8); }
	
	typedef typename P::E		E;
private:
	uint32_t		fields;
};



//
// mach-o two-level hints load command
//
template <typename P>
class macho_dylib_table_of_contents {
public:
	uint32_t		symbol_index() const				INLINE { return E::get32(fields.symbol_index); }
	void			set_symbol_index(uint32_t value)	INLINE { E::set32(fields.symbol_index, value); }

	uint32_t		module_index() const				INLINE { return E::get32(fields.module_index); }
	void			set_module_index(uint32_t value)	INLINE { E::set32(fields.module_index, value);  }
		
	typedef typename P::E		E;
private:
	dylib_table_of_contents	fields;
};



//
// mach-o two-level hints load command
//
template <typename P>
class macho_twolevel_hints_command {
public:
	uint32_t		cmd() const					INLINE { return E::get32(fields.cmd); }
	void			set_cmd(uint32_t value)		INLINE { E::set32(fields.cmd, value); }

	uint32_t		cmdsize() const				INLINE { return E::get32(fields.cmdsize); }
	void			set_cmdsize(uint32_t value)	INLINE { E::set32(fields.cmdsize, value); }

	uint32_t		offset() const				INLINE { return E::get32(fields.offset); }
	void			set_offset(uint32_t value)	INLINE { E::set32(fields.offset, value);  }
	
	uint32_t		nhints() const				INLINE { return E::get32(fields.nhints); }
	void			set_nhints(uint32_t value)	INLINE { E::set32(fields.nhints, value);  }
	
	typedef typename P::E		E;
private:
	twolevel_hints_command	fields;
};


//
// mach-o threads load command
//
template <typename P>
class macho_thread_command {
public:
	uint32_t		cmd() const											INLINE { return E::get32(fields.cmd); }
	void			set_cmd(uint32_t value)								INLINE { E::set32(fields.cmd, value); }

	uint32_t		cmdsize() const										INLINE { return E::get32(fields.cmdsize); }
	void			set_cmdsize(uint32_t value)							INLINE { E::set32(fields.cmdsize, value); }

	uint32_t		flavor() const										INLINE { return E::get32(fields_flavor); }
	void			set_flavor(uint32_t value)							INLINE { E::set32(fields_flavor, value);  }
	
	uint32_t		count() const										INLINE { return E::get32(fields_count); }
	void			set_count(uint32_t value)							INLINE { E::set32(fields_count, value);  }
	
	uint64_t		thread_register(uint32_t index) const				INLINE { return P::getP(thread_registers[index]); }
	void			set_thread_register(uint32_t index, uint64_t value)	INLINE { P::setP(thread_registers[index], value); }
	
	typedef typename P::E		E;
	typedef typename P::uint_t	pint_t;
private:
	struct thread_command	fields;
	uint32_t				fields_flavor;
	uint32_t				fields_count;
	pint_t					thread_registers[1];
};


//
// mach-o misc data 
//
template <typename P>
class macho_linkedit_data_command {
public:
	uint32_t		cmd() const					INLINE { return E::get32(fields.cmd); }
	void			set_cmd(uint32_t value)		INLINE { E::set32(fields.cmd, value); }

	uint32_t		cmdsize() const				INLINE { return E::get32(fields.cmdsize); }
	void			set_cmdsize(uint32_t value)	INLINE { E::set32(fields.cmdsize, value); }

	uint32_t		dataoff() const				INLINE { return E::get32(fields.dataoff); }
	void			set_dataoff(uint32_t value)	INLINE { E::set32(fields.dataoff, value);  }
	
	uint32_t		datasize() const			INLINE { return E::get32(fields.datasize); }
	void			set_datasize(uint32_t value)INLINE { E::set32(fields.datasize, value);  }
	
	
	typedef typename P::E		E;
private:
	struct linkedit_data_command	fields;
};


//
// mach-o rpath  
//
template <typename P>
class macho_rpath_command {
public:
	uint32_t		cmd() const						INLINE { return E::get32(fields.cmd); }
	void			set_cmd(uint32_t value)			INLINE { E::set32(fields.cmd, value); }

	uint32_t		cmdsize() const					INLINE { return E::get32(fields.cmdsize); }
	void			set_cmdsize(uint32_t value)		INLINE { E::set32(fields.cmdsize, value); }

	uint32_t		path_offset() const				INLINE { return E::get32(fields.path.offset); }
	void			set_path_offset(uint32_t value)	INLINE { E::set32(fields.path.offset, value);  }
	
	const char*		path() const					INLINE { return (const char*)&fields + path_offset(); }
	void			set_path_offset()				INLINE { set_path_offset(sizeof(fields)); }
	
	
	typedef typename P::E		E;
private:
	struct rpath_command	fields;
};



//
// mach-o symbol table entry 
//
template <typename P> struct macho_nlist_content {};
template <> struct macho_nlist_content<Pointer32<BigEndian> >    { struct nlist		fields; };
template <> struct macho_nlist_content<Pointer64<BigEndian> >	 { struct nlist_64	fields; };
template <> struct macho_nlist_content<Pointer32<LittleEndian> > { struct nlist		fields; };
template <> struct macho_nlist_content<Pointer64<LittleEndian> > { struct nlist_64	fields; };

template <typename P>
class macho_nlist {
public:
	uint32_t		n_strx() const					INLINE { return E::get32(entry.fields.n_un.n_strx); }
	void			set_n_strx(uint32_t value)		INLINE { E::set32((uint32_t&)entry.fields.n_un.n_strx, value); }

	uint8_t			n_type() const					INLINE { return entry.fields.n_type; }
	void			set_n_type(uint8_t value)		INLINE { entry.fields.n_type = value; }

	uint8_t			n_sect() const					INLINE { return entry.fields.n_sect; }
	void			set_n_sect(uint8_t value)		INLINE { entry.fields.n_sect = value; }

	uint16_t		n_desc() const					INLINE { return E::get16(entry.fields.n_desc); }
	void			set_n_desc(uint16_t value)		INLINE { E::set16((uint16_t&)entry.fields.n_desc, value); }

	uint64_t		n_value() const					INLINE { return P::getP(entry.fields.n_value); }
	void			set_n_value(uint64_t value)		INLINE { P::setP(entry.fields.n_value, value); }

	typedef typename P::E		E;
private:
	macho_nlist_content<P>	entry;
};



//
// mach-o relocation info
//
template <typename P>
class macho_relocation_info {
public:
	uint32_t		r_address() const				INLINE { return E::get32(address); }
	void			set_r_address(uint32_t value)	INLINE { E::set32(address, value); }

	uint32_t		r_symbolnum() const				INLINE { return E::getBits(other, 0, 24); }
	void			set_r_symbolnum(uint32_t value) INLINE { E::setBits(other, value, 0, 24); }

	bool			r_pcrel() const					INLINE { return E::getBits(other, 24, 1); }
	void			set_r_pcrel(bool value)			INLINE { E::setBits(other, value, 24, 1); }	
	
	uint8_t			r_length() const				INLINE { return E::getBits(other, 25, 2); }
	void			set_r_length(uint8_t value)		INLINE { E::setBits(other, value, 25, 2); }
	
	bool			r_extern() const				INLINE { return E::getBits(other, 27, 1); }
	void			set_r_extern(bool value)		INLINE { E::setBits(other, value, 27, 1); }
	
	uint8_t			r_type() const					INLINE { return E::getBits(other, 28, 4); }
	void			set_r_type(uint8_t value)		INLINE { E::setBits(other, value, 28, 4); }
		
	void			set_r_length()					INLINE { set_r_length((sizeof(typename P::uint_t)==8) ? 3 : 2); }

	typedef typename P::E		E;
private:
	uint32_t		address;
	uint32_t		other;
};


//
// mach-o scattered relocation info
// The bit fields are always in big-endian order (see mach-o/reloc.h)
//
template <typename P>
class macho_scattered_relocation_info {
public:
	bool			r_scattered() const			INLINE { return BigEndian::getBitsRaw(E::get32(other), 0, 1); }
	void			set_r_scattered(bool x)		INLINE { uint32_t temp = E::get32(other); BigEndian::setBitsRaw(temp, x, 0, 1);  E::set32(other, temp); }

	bool			r_pcrel() const				INLINE { return BigEndian::getBitsRaw(E::get32(other), 1, 1); }
	void			set_r_pcrel(bool x)			INLINE { uint32_t temp = E::get32(other); BigEndian::setBitsRaw(temp, x, 1, 1);  E::set32(other, temp); }

	uint8_t			r_length() const			INLINE { return BigEndian::getBitsRaw(E::get32(other), 2, 2); }
	void			set_r_length(uint8_t x)		INLINE { uint32_t temp = E::get32(other); BigEndian::setBitsRaw(temp, x, 2, 2);  E::set32(other, temp); }

	uint8_t			r_type() const				INLINE { return BigEndian::getBitsRaw(E::get32(other), 4, 4); }
	void			set_r_type(uint8_t x)		INLINE { uint32_t temp = E::get32(other); BigEndian::setBitsRaw(temp, x, 4, 4);  E::set32(other, temp); }

	uint32_t		r_address() const			INLINE { return BigEndian::getBitsRaw(E::get32(other), 8, 24); }
	void			set_r_address(uint32_t x)			{ if ( x > 0x00FFFFFF ) throw "scattered reloc r_address too large"; 
														uint32_t temp = E::get32(other); BigEndian::setBitsRaw(temp, x, 8, 24);  E::set32(other, temp); }

	uint32_t		r_value() const				INLINE { return E::get32(value); }
	void			set_r_value(uint32_t x)		INLINE { E::set32(value, x); }

	uint32_t		r_other() const				INLINE { return other; }
	
	void			set_r_length()				INLINE { set_r_length((sizeof(typename P::uint_t)==8) ? 3 : 2); }

	typedef typename P::E		E;
private:
	uint32_t		other;
	uint32_t		value;
};



//
// mach-o encyrption info load command
//
template <typename P>
class macho_encryption_info_command {
public:
	uint32_t		cmd() const						INLINE { return E::get32(fields.cmd); }
	void			set_cmd(uint32_t value)			INLINE { E::set32(fields.cmd, value); }

	uint32_t		cmdsize() const					INLINE { return E::get32(fields.cmdsize); }
	void			set_cmdsize(uint32_t value)		INLINE { E::set32(fields.cmdsize, value); }

	uint32_t		cryptoff() const				INLINE { return E::get32(fields.cryptoff); }
	void			set_cryptoff(uint32_t value)	INLINE { E::set32(fields.cryptoff, value);  }
	
	uint32_t		cryptsize() const				INLINE { return E::get32(fields.cryptsize); }
	void			set_cryptsize(uint32_t value)	INLINE { E::set32(fields.cryptsize, value);  }
	
	uint32_t		cryptid() const					INLINE { return E::get32(fields.cryptid); }
	void			set_cryptid(uint32_t value)		INLINE { E::set32(fields.cryptid, value);  }
	
	typedef typename P::E		E;
private:
	encryption_info_command	fields;
};


//
// start of __unwind_info section  
//
template <typename P>
class macho_unwind_info_section_header {
public:
	uint32_t		version() const											INLINE { return E::get32(fields.version); }
	void			set_version(uint32_t value)								INLINE { E::set32(fields.version, value); }

	uint32_t		commonEncodingsArraySectionOffset() const				INLINE { return E::get32(fields.commonEncodingsArraySectionOffset); }
	void			set_commonEncodingsArraySectionOffset(uint32_t value)	INLINE { E::set32(fields.commonEncodingsArraySectionOffset, value); }

	uint32_t		commonEncodingsArrayCount() const						INLINE { return E::get32(fields.commonEncodingsArrayCount); }
	void			set_commonEncodingsArrayCount(uint32_t value)			INLINE { E::set32(fields.commonEncodingsArrayCount, value); }

	uint32_t		personalityArraySectionOffset() const					INLINE { return E::get32(fields.personalityArraySectionOffset); }
	void			set_personalityArraySectionOffset(uint32_t value)		INLINE { E::set32(fields.personalityArraySectionOffset, value); }

	uint32_t		personalityArrayCount() const							INLINE { return E::get32(fields.personalityArrayCount); }
	void			set_personalityArrayCount(uint32_t value)				INLINE { E::set32(fields.personalityArrayCount, value); }

	uint32_t		indexSectionOffset() const								INLINE { return E::get32(fields.indexSectionOffset); }
	void			set_indexSectionOffset(uint32_t value)					INLINE { E::set32(fields.indexSectionOffset, value); }

	uint32_t		indexCount() const										INLINE { return E::get32(fields.indexCount); }
	void			set_indexCount(uint32_t value)							INLINE { E::set32(fields.indexCount, value); }

	typedef typename P::E		E;
private:
	unwind_info_section_header	fields;
};



//
// uwind first level index entry  
//
template <typename P>
class macho_unwind_info_section_header_index_entry {
public:
	uint32_t		functionOffset() const								INLINE { return E::get32(fields.functionOffset); }
	void			set_functionOffset(uint32_t value)					INLINE { E::set32(fields.functionOffset, value); }

	uint32_t		secondLevelPagesSectionOffset() const				INLINE { return E::get32(fields.secondLevelPagesSectionOffset); }
	void			set_secondLevelPagesSectionOffset(uint32_t value)	INLINE { E::set32(fields.secondLevelPagesSectionOffset, value); }

	uint32_t		lsdaIndexArraySectionOffset() const					INLINE { return E::get32(fields.lsdaIndexArraySectionOffset); }
	void			set_lsdaIndexArraySectionOffset(uint32_t value)		INLINE { E::set32(fields.lsdaIndexArraySectionOffset, value); }

	typedef typename P::E		E;
private:
	unwind_info_section_header_index_entry	fields;
};


//
// LSDA table entry  
//
template <typename P>
class macho_unwind_info_section_header_lsda_index_entry {
public:
	uint32_t		functionOffset() const								INLINE { return E::get32(fields.functionOffset); }
	void			set_functionOffset(uint32_t value)					INLINE { E::set32(fields.functionOffset, value); }

	uint32_t		lsdaOffset() const									INLINE { return E::get32(fields.lsdaOffset); }
	void			set_lsdaOffset(uint32_t value)						INLINE { E::set32(fields.lsdaOffset, value); }

	typedef typename P::E		E;
private:
	unwind_info_section_header_lsda_index_entry	fields;
};


//
// regular second level entry  
//
template <typename P>
class macho_unwind_info_regular_second_level_entry {
public:
	uint32_t		functionOffset() const								INLINE { return E::get32(fields.functionOffset); }
	void			set_functionOffset(uint32_t value)					INLINE { E::set32(fields.functionOffset, value); }

	uint32_t		encoding() const									INLINE { return E::get32(fields.encoding); }
	void			set_encoding(uint32_t value)						INLINE { E::set32(fields.encoding, value); }

	typedef typename P::E		E;
private:
	unwind_info_regular_second_level_entry	fields;
};


//
// start of second level regular page  
//
template <typename P>
class macho_unwind_info_regular_second_level_page_header {
public:
	uint32_t		kind() const								INLINE { return E::get32(fields.kind); }
	void			set_kind(uint32_t value)					INLINE { E::set32(fields.kind, value); }

	uint16_t		entryPageOffset() const						INLINE { return E::get16(fields.entryPageOffset); }
	void			set_entryPageOffset(uint16_t value)			INLINE { E::set16((uint16_t&)fields.entryPageOffset, value); }

	uint16_t		entryCount() const							INLINE { return E::get16(fields.entryCount); }
	void			set_entryCount(uint16_t value)				INLINE { E::set16((uint16_t&)fields.entryCount, value); }

	typedef typename P::E		E;
private:
	unwind_info_regular_second_level_page_header	fields;
};


//
// start of second level compressed page  
//
template <typename P>
class macho_unwind_info_compressed_second_level_page_header {
public:
	uint32_t		kind() const								INLINE { return E::get32(fields.kind); }
	void			set_kind(uint32_t value)					INLINE { E::set32(fields.kind, value); }

	uint16_t		entryPageOffset() const						INLINE { return E::get16(fields.entryPageOffset); }
	void			set_entryPageOffset(uint16_t value)			INLINE { E::set16((uint16_t&)fields.entryPageOffset, value); }

	uint16_t		entryCount() const							INLINE { return E::get16(fields.entryCount); }
	void			set_entryCount(uint16_t value)				INLINE { E::set16((uint16_t&)fields.entryCount, value); }

	uint16_t		encodingsPageOffset() const					INLINE { return E::get16(fields.encodingsPageOffset); }
	void			set_encodingsPageOffset(uint16_t value)		INLINE { E::set16((uint16_t&)fields.encodingsPageOffset, value); }

	uint16_t		encodingsCount() const						INLINE { return E::get16(fields.encodingsCount); }
	void			set_encodingsCount(uint16_t value)			INLINE { E::set16((uint16_t&)fields.encodingsCount, value); }

	typedef typename P::E		E;
private:
	unwind_info_compressed_second_level_page_header	fields;
};


//
// compressed dyld info load command
//
template <typename P>
class macho_dyld_info_command {
public:
	uint32_t		cmd() const					INLINE { return E::get32(fields.cmd); }
	void			set_cmd(uint32_t value)		INLINE { E::set32(fields.cmd, value); }

	uint32_t		cmdsize() const				INLINE { return E::get32(fields.cmdsize); }
	void			set_cmdsize(uint32_t value)	INLINE { E::set32(fields.cmdsize, value); }

	uint32_t		rebase_off() const				INLINE { return E::get32(fields.rebase_off); }
	void			set_rebase_off(uint32_t value)	INLINE { E::set32(fields.rebase_off, value);  }
	
	uint32_t		rebase_size() const				INLINE { return E::get32(fields.rebase_size); }
	void			set_rebase_size(uint32_t value)	INLINE { E::set32(fields.rebase_size, value);  }
	
	uint32_t		bind_off() const				INLINE { return E::get32(fields.bind_off); }
	void			set_bind_off(uint32_t value)	INLINE { E::set32(fields.bind_off, value);  }
	
	uint32_t		bind_size() const				INLINE { return E::get32(fields.bind_size); }
	void			set_bind_size(uint32_t value)	INLINE { E::set32(fields.bind_size, value);  }
	
	uint32_t		weak_bind_off() const				INLINE { return E::get32(fields.weak_bind_off); }
	void			set_weak_bind_off(uint32_t value)	INLINE { E::set32(fields.weak_bind_off, value);  }
	
	uint32_t		weak_bind_size() const				INLINE { return E::get32(fields.weak_bind_size); }
	void			set_weak_bind_size(uint32_t value)	INLINE { E::set32(fields.weak_bind_size, value);  }
	
	uint32_t		lazy_bind_off() const				INLINE { return E::get32(fields.lazy_bind_off); }
	void			set_lazy_bind_off(uint32_t value)	INLINE { E::set32(fields.lazy_bind_off, value);  }
	
	uint32_t		lazy_bind_size() const				INLINE { return E::get32(fields.lazy_bind_size); }
	void			set_lazy_bind_size(uint32_t value)	INLINE { E::set32(fields.lazy_bind_size, value);  }
	
	uint32_t		export_off() const				INLINE { return E::get32(fields.export_off); }
	void			set_export_off(uint32_t value)	INLINE { E::set32(fields.export_off, value);  }
	
	uint32_t		export_size() const				INLINE { return E::get32(fields.export_size); }
	void			set_export_size(uint32_t value)	INLINE { E::set32(fields.export_size, value);  }
	
	
	typedef typename P::E		E;
private:
	dyld_info_command	fields;
};




#endif	// __MACH_O_FILE_ABSTRACTION__



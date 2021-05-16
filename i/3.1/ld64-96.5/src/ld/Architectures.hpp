/* -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*- 
 *
 * Copyright (c) 2005-2007 Apple Inc. All rights reserved.
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
 
#ifndef __ARCHITECTURES__
#define __ARCHITECTURES__

#include "FileAbstraction.hpp"


//
// Architectures
//
struct ppc
{
	typedef Pointer32<BigEndian>		P;
	
	enum ReferenceKinds {  kNoFixUp, kFollowOn, kGroupSubordinate, kPointer, kPointerWeakImport,   
							kPointerDiff16, kPointerDiff32, kPointerDiff=kPointerDiff32, kPointerDiff64,  
							kBranch24, kBranch24WeakImport, kBranch14,
							kPICBaseLow16, kPICBaseLow14, kPICBaseHigh16, 
							kAbsLow16, kAbsLow14, kAbsHigh16, kAbsHigh16AddLow, 
							kDtraceProbe, kDtraceProbeSite, kDtraceIsEnabledSite, kDtraceTypeReference };
};

struct ppc64
{
	typedef Pointer64<BigEndian>		P;
	
	enum ReferenceKinds {  kNoFixUp, kFollowOn, kGroupSubordinate, kPointer, kPointerWeakImport, 
							kPointerDiff16, kPointerDiff32, kPointerDiff64, kPointerDiff=kPointerDiff64,
							kBranch24, kBranch24WeakImport, kBranch14,
							kPICBaseLow16, kPICBaseLow14, kPICBaseHigh16, 
							kAbsLow16, kAbsLow14, kAbsHigh16, kAbsHigh16AddLow, 
							kDtraceProbe, kDtraceProbeSite, kDtraceIsEnabledSite, kDtraceTypeReference  };
};

struct x86
{
	typedef Pointer32<LittleEndian>		P;
	
	enum ReferenceKinds {  kNoFixUp, kFollowOn, kGroupSubordinate, kPointer, kPointerWeakImport, kPointerDiff, kPointerDiff32=kPointerDiff, kPointerDiff16,
							kPCRel32, kPCRel32WeakImport, kAbsolute32,  kPCRel16, kPCRel8, 
							kImageOffset32, kPointerDiff24, kSectionOffset24,
							kDtraceProbe, kDtraceProbeSite, kDtraceIsEnabledSite, kDtraceTypeReference  };
};

struct x86_64
{
	typedef Pointer64<LittleEndian>		P;
	
	enum ReferenceKinds {  kNoFixUp, kFollowOn, kGroupSubordinate, kPointer, kPointer32, kPointerWeakImport, kPointerDiff, kPointerDiff32, 
							kPCRel32, kPCRel32_1, kPCRel32_2, kPCRel32_4,
							kBranchPCRel32, kBranchPCRel32WeakImport,
							kPCRel32GOTLoad, kPCRel32GOTLoadWeakImport,
							kPCRel32GOT, kPCRel32GOTWeakImport, kBranchPCRel8, kGOTNoFixUp, 
							kImageOffset32, kPointerDiff24, kSectionOffset24,
							kDtraceProbe, kDtraceProbeSite, kDtraceIsEnabledSite, kDtraceTypeReference  };
};

struct arm
{
	typedef Pointer32<LittleEndian>		P;
	
	enum ReferenceKinds {  kNoFixUp, kFollowOn, kGroupSubordinate, kPointer, kPointerWeakImport, kPointerDiff, 
							kPointerDiff32=kPointerDiff, kReadOnlyPointer, kPointerDiff12, 
							kBranch24, kBranch24WeakImport, kThumbBranch22, kThumbBranch22WeakImport, 
							kDtraceProbe, kDtraceProbeSite, kDtraceIsEnabledSite, kDtraceTypeReference  };
};

#endif // __ARCHITECTURES__



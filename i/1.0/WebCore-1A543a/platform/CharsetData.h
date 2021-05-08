/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "TextEncoding.h"


#include <CoreFoundation/CFStringEncodingExt.h>

#if !defined(__arm__)
// Not currently supported in PPC/x86, see CFStringEncodingExt.h
#define kCFStringEncodingMacJapanese 0
#define kCFStringEncodingDOSJapanese 0
#define kCFStringEncodingJIS_X0201_76 0
#define kCFStringEncodingJIS_X0208_83 0
#define kCFStringEncodingJIS_X0208_90 0
#define kCFStringEncodingJIS_C6226_78 0
#define kCFStringEncodingJIS_X0212_90 0
#define kCFStringEncodingShiftJIS_X0213_00 0
#define kCFStringEncodingISO_2022_JP 0
#define kCFStringEncodingISO_2022_JP_2 0
#define kCFStringEncodingISO_2022_JP_1 0
#define kCFStringEncodingISO_2022_JP_3 0
#define kCFStringEncodingEUC_JP 0
#define kCFStringEncodingShiftJIS 0
#define kCFStringEncodingMacChineseTrad 0
#define kCFStringEncodingMacKorean 0
#define kCFStringEncodingMacGreek 0
#define kCFStringEncodingMacCyrillic 0
#define kCFStringEncodingMacChineseSimp 0
#define kCFStringEncodingISOLatin2 0
#define kCFStringEncodingISOLatin3 0
#define kCFStringEncodingISOLatin4 0
#define kCFStringEncodingISOLatinCyrillic 0
#define kCFStringEncodingISOLatinGreek 0
#define kCFStringEncodingISOLatin5 0
#define kCFStringEncodingISOLatin6 0
#define kCFStringEncodingISOLatin7 0
#define kCFStringEncodingISOLatin8 0
#define kCFStringEncodingISOLatin9 0
#define kCFStringEncodingISOLatin10 0
#define kCFStringEncodingDOSLatinUS 0
#define kCFStringEncodingDOSGreek 0
#define kCFStringEncodingDOSLatin1 0
#define kCFStringEncodingDOSLatin2 0
#define kCFStringEncodingDOSTurkish 0
#define kCFStringEncodingDOSIcelandic 0
#define kCFStringEncodingDOSRussian 0
#define kCFStringEncodingDOSGreek2 0
#define kCFStringEncodingDOSChineseTrad 0
#define kCFStringEncodingWindowsLatin2 0
#define kCFStringEncodingWindowsCyrillic 0
#define kCFStringEncodingWindowsGreek 0
#define kCFStringEncodingWindowsLatin5 0
#define kCFStringEncodingWindowsKoreanJohab 0
#define kCFStringEncodingGB_18030_2000 0
#define kCFStringEncodingISO_2022_CN 0
#define kCFStringEncodingISO_2022_CN_EXT 0
#define kCFStringEncodingISO_2022_KR 0
#define kCFStringEncodingEUC_CN 0
#define kCFStringEncodingEUC_TW 0
#define kCFStringEncodingEUC_KR 0
#define kCFStringEncodingKOI8_R 0
#define kCFStringEncodingBig5 0
#define kCFStringEncodingMacRomanLatin1 0
#define kCFStringEncodingHZ_GB_2312 0
#define kCFStringEncodingBig5_HKSCS_1999 0
#define kCFStringEncodingKOI8_U 0
#define kCFStringEncodingEBCDIC_CP037 0

#endif


namespace WebCore {

    struct CharsetEntry {
        const char* name;
        TextEncodingID encoding;
        int flags; // actually TextEncodingFlags
    };

    extern const CharsetEntry CharsetTable[];

}

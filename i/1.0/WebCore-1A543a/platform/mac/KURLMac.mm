/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#include "config.h"
#import "KURL.h"

#import "FoundationExtras.h"
#import <wtf/Assertions.h>
#import <wtf/Vector.h>

namespace WebCore {

KURL::KURL(NSURL *url)
{
    if (url) {
        CFIndex bytesLength = CFURLGetBytes((CFURLRef)url, 0, 0);
        Vector<char, 2048> buffer(bytesLength + 6);  // 5 for "file:", 1 for NUL terminator
        char *bytes = &buffer[5];
        CFURLGetBytes((CFURLRef)url, (UInt8 *)bytes, bytesLength);
        bytes[bytesLength] = '\0';
        if (bytes[0] == '/') {
            buffer[0] = 'f';
            buffer[1] = 'i';
            buffer[2] = 'l';
            buffer[3] = 'e';
            buffer[4] = ':';
            parse(buffer, 0);
        } else
            parse(bytes, 0);
    } else
        parse("", 0);
}

CFURLRef KURL::createCFURL() const
{
    const UInt8 *bytes = (const UInt8 *)urlString.latin1();
    // NOTE: We use UTF-8 here since this encoding is used when computing strings when returning URL components
    // (e.g calls to NSURL -path). However, this function is not tolerant of illegal UTF-8 sequences, which
    // could either be a malformed string or bytes in a different encoding, like Shift-JIS, so we fall back
    // onto using ISO Latin-1 in those cases.
    CFURLRef result = CFURLCreateAbsoluteURLWithBytes(0, bytes, urlString.length(), kCFStringEncodingUTF8, 0, true);
    if (!result)
        result = CFURLCreateAbsoluteURLWithBytes(0, bytes, urlString.length(), kCFStringEncodingISOLatin1, 0, true);
    return result;
}

NSURL *KURL::getNSURL() const
{
    return HardAutorelease(createCFURL());
}

}

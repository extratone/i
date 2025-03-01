/*
 * Copyright (C) 2009, 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RuntimeApplicationChecks_h
#define RuntimeApplicationChecks_h

namespace WebCore {

bool applicationIsAOLInstantMessenger();
bool applicationIsAdobeInstaller();
bool applicationIsAperture();
bool applicationIsAppleMail();
bool applicationIsMicrosoftMessenger();
bool applicationIsMicrosoftMyDay();
bool applicationIsMicrosoftOutlook();
bool applicationIsSafari();
bool applicationIsSolidStateNetworksDownloader();
bool applicationIsVersions();
bool applicationIsHRBlock();

} // namespace WebCore

#if !PLATFORM(MAC) || PLATFORM(IOS)
inline bool WebCore::applicationIsAOLInstantMessenger() { return false; }
inline bool WebCore::applicationIsAdobeInstaller() { return false; }
inline bool WebCore::applicationIsAperture() { return false; }
inline bool WebCore::applicationIsAppleMail() { return false; }
inline bool WebCore::applicationIsMicrosoftMessenger() { return false; }
inline bool WebCore::applicationIsMicrosoftMyDay() { return false; }
inline bool WebCore::applicationIsMicrosoftOutlook() { return false; }
inline bool WebCore::applicationIsSafari() { return false; }
inline bool WebCore::applicationIsSolidStateNetworksDownloader() { return false; }
#endif // !PLATFORM(MAC) || PLATFORM(IOS)

#endif // RuntimeApplicationChecks_h

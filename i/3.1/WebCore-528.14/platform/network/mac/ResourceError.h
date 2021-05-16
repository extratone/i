/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef ResourceError_h
#define ResourceError_h

#include "ResourceErrorBase.h"
#include <wtf/RetainPtr.h>

#ifdef __OBJC__
@class NSError;
#else
class NSError;
#endif

namespace WebCore {

    class ResourceError : public ResourceErrorBase {
    public:
        ResourceError()
            : m_dataIsUpToDate(true)
        {
        }

        ResourceError(const String& domain, int errorCode, const String& failingURL, const String& localizedDescription)
            : ResourceErrorBase(domain, errorCode, failingURL, localizedDescription)
            , m_dataIsUpToDate(true)
        {
        }

        ResourceError(NSError* error)
            : m_dataIsUpToDate(false)
            , m_platformError(error)
        {
            m_isNull = !error;
        }

        operator NSError*() const;

    private:
        friend class ResourceErrorBase;

        void platformLazyInit();
        static bool platformCompare(const ResourceError& a, const ResourceError& b);

        bool m_dataIsUpToDate;
        mutable RetainPtr<NSError> m_platformError;
};

} // namespace WebCore

#endif // ResourceError_h_

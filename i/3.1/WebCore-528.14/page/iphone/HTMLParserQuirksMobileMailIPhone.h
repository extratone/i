/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef HTMLParserQuirksMobileMailIPhone_h
#define HTMLParserQuirksMobileMailIPhone_h

#include "HTMLParserQuirks.h"

namespace WebCore {

class HTMLParserQuirksMobileMailIPhone : public HTMLParserQuirks {
public:
    HTMLParserQuirksMobileMailIPhone()
        : m_macOSXMailDivTagSkipCount(0)
    {
    }

    ~HTMLParserQuirksMobileMailIPhone() { }

    virtual void reset();

    virtual bool shouldInsertNode(Node* parent, Node* newNode);
    virtual bool shouldPopBlock(const AtomicString& tagNameOnStack, const AtomicString& tagNameToPop);

private:
    // Mac OS X Mail regularly exercised a WebKit bug (see <rdar://problem/6102483>)
    // where extraneous <div> elements were inserted into messages with content that
    // was copied and pasted.  Over time this built up to intolerable levels of nested
    // tags.  Because these HTML documents are still well-formed, we count the number
    // of <div> tags removed so that we may ignore the same number of mismatched
    // </div> tags on the way out, thus preserving the formatting of the message.
    size_t m_macOSXMailDivTagSkipCount;
};

} // namespace WebCore

#endif // HTMLParserQuirksMobileMailIPhone_h

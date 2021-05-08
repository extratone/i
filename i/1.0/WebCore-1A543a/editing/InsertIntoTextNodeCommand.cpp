/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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
#include "InsertIntoTextNodeCommand.h"

#include "Text.h"
#include "RenderText.h"

#define SHOULD_MAKE_LAST_CHARACTER_INSECURE 0

namespace WebCore {

InsertIntoTextNodeCommand::InsertIntoTextNodeCommand(Document *document, Text *node, int offset, const String &text)
    : EditCommand(document), m_node(node), m_offset(offset)
{
    ASSERT(m_node);
    ASSERT(m_offset >= 0);
    ASSERT(!text.isEmpty());
    
    m_text = text.copy(); // make a copy to ensure that the string never changes
}

void InsertIntoTextNodeCommand::doApply()
{
    ASSERT(m_node);
    ASSERT(m_offset >= 0);
    ASSERT(!m_text.isEmpty());

    ExceptionCode ec = 0;
#if SHOULD_MAKE_LAST_CHARACTER_INSECURE
    RenderText::makeLastCharacterInsecure();
#endif
    m_node->insertData(m_offset, m_text, ec);
#if SHOULD_MAKE_LAST_CHARACTER_INSECURE
    RenderText::makeLastCharacterSecure();
#endif
    ASSERT(ec == 0);
}

void InsertIntoTextNodeCommand::doUnapply()
{
    ASSERT(m_node);
    ASSERT(m_offset >= 0);
    ASSERT(!m_text.isEmpty());

    ExceptionCode ec = 0;
    m_node->deleteData(m_offset, m_text.length(), ec);
    ASSERT(ec == 0);
}

} // namespace WebCore

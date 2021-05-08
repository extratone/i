/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "CSSSelectorList.h"

namespace WebCore {

CSSSelectorList::~CSSSelectorList() 
{
    deleteSelectors();
}
    
void CSSSelectorList::adopt(CSSSelectorList& list)
{
    deleteSelectors();
    m_selectorArray = list.m_selectorArray;
    list.m_selectorArray = 0;
}

void CSSSelectorList::adoptSelectorVector(Vector<CSSSelector*>& selectorVector)
{
    deleteSelectors();
    const size_t size = selectorVector.size();
    ASSERT(size);
    if (size == 1) {
        m_selectorArray = selectorVector[0];
        m_selectorArray->setLastInSelectorList();
        selectorVector.shrink(0);
        return;
    }
    m_selectorArray = reinterpret_cast<CSSSelector*>(fastMalloc(sizeof(CSSSelector) * selectorVector.size()));
    for (size_t i = 0; i < size; ++i) {
        memcpy(&m_selectorArray[i], selectorVector[i], sizeof(CSSSelector));
        fastFree(selectorVector[i]);
        ASSERT(!m_selectorArray[i].isLastInSelectorList());
    }
    m_selectorArray[size - 1].setLastInSelectorList();
    selectorVector.shrink(0);
}

void CSSSelectorList::deleteSelectors()
{
    if (!m_selectorArray)
        return;
    CSSSelector* s = m_selectorArray;
    while (1) {
        bool done = s->isLastInSelectorList();
        s->~CSSSelector();
        if (done)
            break;
        ++s;
    }
    fastFree(m_selectorArray); 
}

}

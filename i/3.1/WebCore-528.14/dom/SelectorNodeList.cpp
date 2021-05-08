/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#include "config.h"
#include "SelectorNodeList.h"

#include "CSSSelector.h"
#include "CSSSelectorList.h"
#include "CSSStyleSelector.h"
#include "Document.h"
#include "Element.h"
#include "HTMLNames.h"

namespace WebCore {

using namespace HTMLNames;

PassRefPtr<StaticNodeList> createSelectorNodeList(Node* rootNode, const CSSSelectorList& querySelectorList)
{
    Vector<RefPtr<Node> > nodes;
    Document* document = rootNode->document();
    CSSSelector* onlySelector = querySelectorList.hasOneSelector() ? querySelectorList.first() : 0;
    bool strictParsing = !document->inCompatMode();

    CSSStyleSelector::SelectorChecker selectorChecker(document, strictParsing);

    if (strictParsing && rootNode->inDocument() && onlySelector && onlySelector->m_match == CSSSelector::Id && !document->containsMultipleElementsWithId(onlySelector->m_value)) {
        ASSERT(querySelectorList.first()->attribute() == idAttr);
        Element* element = document->getElementById(onlySelector->m_value);
        if (element && (rootNode->isDocumentNode() || element->isDescendantOf(rootNode)) && selectorChecker.checkSelector(onlySelector, element))
            nodes.append(element);
    } else {
        for (Node* n = rootNode->firstChild(); n; n = n->traverseNextNode(rootNode)) {
            if (n->isElementNode()) {
                Element* element = static_cast<Element*>(n);
                for (CSSSelector* selector = querySelectorList.first(); selector; selector = CSSSelectorList::next(selector)) {
                    if (selectorChecker.checkSelector(selector, element)) {
                        nodes.append(n);
                        break;
                    }
                }
            }
        }
    }
    
    return StaticNodeList::adopt(nodes);
}

} // namespace WebCore

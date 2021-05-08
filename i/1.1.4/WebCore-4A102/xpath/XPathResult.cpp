/*
 * Copyright 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "XPathResult.h"

#if XPATH_SUPPORT

#include "EventListener.h"
#include "EventNames.h"
#include "EventTargetNode.h"
#include "ExceptionCode.h"
#include "XPathEvaluator.h"

namespace WebCore {

using namespace XPath;

class InvalidatingEventListener : public EventListener {
public:
    InvalidatingEventListener(XPathResult* result) : m_result(result) { }
    virtual void handleEvent(Event*, bool) { m_result->invalidateIteratorState(); }
private:
    XPathResult* m_result;
};

XPathResult::XPathResult(EventTargetNode* eventTarget, const Value& value)
    : m_value(value)
    , m_eventTarget(eventTarget)
{
    m_eventListener = new InvalidatingEventListener(this);
    m_eventTarget->addEventListener(EventNames::DOMSubtreeModifiedEvent, m_eventListener, false);
    switch (m_value.type()) {
        case Value::BooleanValue:
            m_resultType = BOOLEAN_TYPE;
            return;
        case Value::NumberValue:
            m_resultType = NUMBER_TYPE;
            return;
        case Value::StringValue:
            m_resultType = STRING_TYPE;
            return;
        case Value::NodeVectorValue:
            m_resultType = UNORDERED_NODE_ITERATOR_TYPE;
            m_nodeSetPosition = 0;
            m_nodeSet = m_value.toNodeVector();
            m_invalidIteratorState = false;
            return;
    }
    ASSERT_NOT_REACHED();
}

XPathResult::~XPathResult()
{
    if (m_eventTarget)
        m_eventTarget->removeEventListener(EventNames::DOMSubtreeModifiedEvent, m_eventListener.get(), false);
}

void XPathResult::convertTo(unsigned short type, ExceptionCode& ec)
{
    switch (type) {
        case ANY_TYPE:
            break;
        case NUMBER_TYPE:
            m_resultType = type;
            m_value = m_value.toNumber();
            break;
        case STRING_TYPE:
            m_resultType = type;
            m_value = m_value.toString();
            break;
        case BOOLEAN_TYPE:
            m_resultType = type;
            m_value = m_value.toBoolean();
            break;
        case UNORDERED_NODE_ITERATOR_TYPE:
        case ORDERED_NODE_ITERATOR_TYPE:
        case UNORDERED_NODE_SNAPSHOT_TYPE:
        case ORDERED_NODE_SNAPSHOT_TYPE:
        case ANY_UNORDERED_NODE_TYPE:
        case FIRST_ORDERED_NODE_TYPE:
            if (!m_value.isNodeVector()) {
                ec = TYPE_ERR;
                return;
            }
            m_resultType = type;
    }
}

unsigned short XPathResult::resultType() const
{
    return m_resultType;
}

double XPathResult::numberValue(ExceptionCode& ec) const
{
    if (resultType() != NUMBER_TYPE) {
        ec = TYPE_ERR;
        return 0.0;
    }
    return m_value.toNumber();
}

String XPathResult::stringValue(ExceptionCode& ec) const
{
    if (resultType() != STRING_TYPE) {
        ec = TYPE_ERR;
        return String();
    }
    return m_value.toString();
}

bool XPathResult::booleanValue(ExceptionCode& ec) const
{
    if (resultType() != BOOLEAN_TYPE) {
        ec = TYPE_ERR;
        return false;
    }
    return m_value.toBoolean();
}

Node* XPathResult::singleNodeValue(ExceptionCode& ec) const
{
    if (resultType() != ANY_UNORDERED_NODE_TYPE && resultType() != FIRST_ORDERED_NODE_TYPE) {
        ec = TYPE_ERR;
        return 0;
    }
  
    NodeVector nodes = m_value.toNodeVector();
    if (nodes.size () == 0)
        return 0;

    return nodes[0].get();
}

void XPathResult::invalidateIteratorState()
{ 
    m_invalidIteratorState = true;
    
    ASSERT(m_eventTarget);
    ASSERT(m_eventListener);
    
    m_eventTarget->removeEventListener(EventNames::DOMSubtreeModifiedEvent, m_eventListener.get(), false);
    
    m_eventTarget = 0;
}

bool XPathResult::invalidIteratorState() const
{
    if (resultType() != UNORDERED_NODE_ITERATOR_TYPE && resultType() != ORDERED_NODE_ITERATOR_TYPE)
        return false;
    
    return m_invalidIteratorState;
}

unsigned long XPathResult::snapshotLength(ExceptionCode& ec) const
{
    if (resultType() != UNORDERED_NODE_SNAPSHOT_TYPE && resultType() != ORDERED_NODE_SNAPSHOT_TYPE) {
        ec = TYPE_ERR;
        return 0;
    }

    return m_value.toNodeVector().size();
}

Node* XPathResult::iterateNext(ExceptionCode& ec)
{
    if (resultType() != UNORDERED_NODE_ITERATOR_TYPE && resultType() != ORDERED_NODE_ITERATOR_TYPE) {
        ec = TYPE_ERR;
        return 0;
    }
    
    if (m_invalidIteratorState) {
        ec = INVALID_STATE_ERR;
        return 0;
    }
    
    if (m_nodeSetPosition + 1 > m_nodeSet.size())
        return 0;

    Node* node = m_nodeSet[m_nodeSetPosition].get();
    
    m_nodeSetPosition++;

    return node;
}

Node* XPathResult::snapshotItem(unsigned long index, ExceptionCode& ec)
{
    if (resultType() != UNORDERED_NODE_SNAPSHOT_TYPE && resultType() != ORDERED_NODE_SNAPSHOT_TYPE) {
        ec = TYPE_ERR;
        return 0;
    }
    
    NodeVector nodes = m_value.toNodeVector();
    if (index >= nodes.size())
        return 0;
    
    return nodes[index].get();
}

}

#endif // XPATH_SUPPORT

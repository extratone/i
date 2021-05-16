/*
 * Copyright (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
*/

#ifndef CounterNode_h
#define CounterNode_h

#include <wtf/Noncopyable.h>

// This implements a counter tree that is used for finding parents in counters() lookup,
// and for propagating count changes when nodes are added or removed.

// Parents represent unique counters and their scope, which are created either explicitly
// by "counter-reset" style rules or implicitly by referring to a counter that is not in scope.
// Such nodes are tagged as "reset" nodes, although they are not all due to "counter-reset".

// Not that render tree children are often counter tree siblings due to counter scoping rules.

namespace WebCore {

class RenderObject;

class CounterNode : Noncopyable {
public:
    CounterNode(RenderObject*, bool isReset, int value);

    bool isReset() const { return m_isReset; }
    int value() const { return m_value; }
    int countInParent() const { return m_countInParent; }
    RenderObject* renderer() const { return m_renderer; }

    CounterNode* parent() const { return m_parent; }
    CounterNode* previousSibling() const { return m_previousSibling; }
    CounterNode* nextSibling() const { return m_nextSibling; }
    CounterNode* firstChild() const { return m_firstChild; }
    CounterNode* lastChild() const { return m_lastChild; }

    void insertAfter(CounterNode* newChild, CounterNode* beforeChild);
    void removeChild(CounterNode*);

private:
    int computeCountInParent() const;
    void recount();

    bool m_isReset;
    int m_value;
    int m_countInParent;
    RenderObject* m_renderer;

    CounterNode* m_parent;
    CounterNode* m_previousSibling;
    CounterNode* m_nextSibling;
    CounterNode* m_firstChild;
    CounterNode* m_lastChild;
};

} // namespace WebCore

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showTree(const WebCore::CounterNode*);
#endif

#endif // CounterNode_h

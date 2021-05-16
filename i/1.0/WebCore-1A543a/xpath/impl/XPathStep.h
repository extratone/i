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

#ifndef XPathStep_H
#define XPathStep_H

#if XPATH_SUPPORT

#include "Node.h"
#include "XPathExpressionNode.h"
#include "XPathUtil.h"

namespace WebCore {

    namespace XPath {

        class Predicate;
        
        class Step : public ParseNode, Noncopyable {
        public:
            enum Axis {
                AncestorAxis, AncestorOrSelfAxis, AttributeAxis,
                ChildAxis, DescendantAxis, DescendantOrSelfAxis,
                FollowingAxis, FollowingSiblingAxis, NamespaceAxis,
                ParentAxis, PrecedingAxis, PrecedingSiblingAxis,
                SelfAxis
            };

            Step(Axis, const String& nodeTest, const Vector<Predicate*>& predicates = Vector<Predicate*>());
            ~Step();

            NodeVector evaluate(Node* context) const;

            void optimize();

        private:
            NodeVector nodesInAxis(Node* context) const;
            NodeVector nodeTestMatches(const NodeVector& nodes) const;
            String namespaceFromNodetest(const String& nodeTest) const;
            Node::NodeType primaryNodeType(Axis) const;

            Axis m_axis;
            String m_nodeTest;
            String m_namespaceURI;
            Vector<Predicate*> m_predicates;
        };

    }

}

#endif // XPATH_SUPPORT

#endif // XPath_Step_H

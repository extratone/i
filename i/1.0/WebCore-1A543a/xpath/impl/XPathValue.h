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

#ifndef XPathValue_H
#define XPathValue_H

#if XPATH_SUPPORT

#include "PlatformString.h"
#include "XPathUtil.h"

namespace WebCore {

    namespace XPath {
    
        class Value {
        public:
            enum Type { NodeVectorValue, BooleanValue, NumberValue, StringValue };
            
            Value();
            Value(Node*);
            Value(const NodeVector&);
            Value(bool);
            Value(unsigned);
            Value(unsigned long);
            Value(double);
            Value(const String&);
            
            Type type() const { return m_type; }

            bool isNodeVector() const { return m_type == NodeVectorValue; }
            bool isBoolean() const { return m_type == BooleanValue; }
            bool isNumber() const { return m_type == NumberValue; }
            bool isString() const { return m_type == StringValue; }

            const NodeVector& toNodeVector() const;    
            bool toBoolean() const;
            double toNumber() const;
            String toString() const;
            
        private:
            Type m_type;
            NodeVector m_nodeVector;
            bool m_bool;
            double m_number;
            String m_string;
        };

    }
}

#endif // XPATH_SUPPORT

#endif // XPath_Value_H

/*
 * Copyright 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
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
#include "XPathFunctions.h"

#if ENABLE(XPATH)

#include "Document.h"
#include "Element.h"
#include "NamedAttrMap.h"
#include "XMLNames.h"
#include "XPathUtil.h"
#include "XPathValue.h"
#include <wtf/MathExtras.h>

namespace WebCore {
namespace XPath {

static inline bool isWhitespace(UChar c)
{
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}


#define DEFINE_FUNCTION_CREATOR(Class) static Function* create##Class() { return new Class; }

class Interval {
public:
    static const int Inf = -1;

    Interval();
    Interval(int value);
    Interval(int min, int max);

    bool contains(int value) const;

private:
    int m_min;
    int m_max;
};

struct FunctionRec {
    typedef Function *(*FactoryFn)();
    FactoryFn factoryFn;
    Interval args;
};

static HashMap<String, FunctionRec>* functionMap;

class FunLast : public Function {
    virtual Value evaluate() const;
};

class FunPosition : public Function {
    virtual Value evaluate() const;
};

class FunCount : public Function {
    virtual Value evaluate() const;
};

class FunId : public Function {
    virtual Value evaluate() const;
};

class FunLocalName : public Function {
    virtual Value evaluate() const;
};

class FunNamespaceURI : public Function {
    virtual Value evaluate() const;
};

class FunName : public Function {
    virtual Value evaluate() const;
};

class FunString : public Function {
    virtual Value evaluate() const;
};

class FunConcat : public Function {
    virtual Value evaluate() const;
};

class FunStartsWith : public Function {
    virtual Value evaluate() const;
};

class FunContains : public Function {
    virtual Value evaluate() const;
};

class FunSubstringBefore : public Function {
    virtual Value evaluate() const;
};

class FunSubstringAfter : public Function {
    virtual Value evaluate() const;
};

class FunSubstring : public Function {
    virtual Value evaluate() const;
};

class FunStringLength : public Function {
    virtual Value evaluate() const;
};

class FunNormalizeSpace : public Function {
    virtual Value evaluate() const;
};

class FunTranslate : public Function {
    virtual Value evaluate() const;
};

class FunBoolean : public Function {
    virtual Value evaluate() const;
};

class FunNot : public Function {
    virtual Value evaluate() const;
};

class FunTrue : public Function {
    virtual Value evaluate() const;
};

class FunFalse : public Function {
    virtual Value evaluate() const;
};

class FunLang : public Function {
    virtual Value evaluate() const;
};

class FunNumber : public Function {
    virtual Value evaluate() const;
};

class FunSum : public Function {
    virtual Value evaluate() const;
};

class FunFloor : public Function {
    virtual Value evaluate() const;
};

class FunCeiling : public Function {
    virtual Value evaluate() const;
};

class FunRound : public Function {
    virtual Value evaluate() const;
public:
    static double round(double);
};

DEFINE_FUNCTION_CREATOR(FunLast)
DEFINE_FUNCTION_CREATOR(FunPosition)
DEFINE_FUNCTION_CREATOR(FunCount)
DEFINE_FUNCTION_CREATOR(FunId)
DEFINE_FUNCTION_CREATOR(FunLocalName)
DEFINE_FUNCTION_CREATOR(FunNamespaceURI)
DEFINE_FUNCTION_CREATOR(FunName)

DEFINE_FUNCTION_CREATOR(FunString)
DEFINE_FUNCTION_CREATOR(FunConcat)
DEFINE_FUNCTION_CREATOR(FunStartsWith)
DEFINE_FUNCTION_CREATOR(FunContains)
DEFINE_FUNCTION_CREATOR(FunSubstringBefore)
DEFINE_FUNCTION_CREATOR(FunSubstringAfter)
DEFINE_FUNCTION_CREATOR(FunSubstring)
DEFINE_FUNCTION_CREATOR(FunStringLength)
DEFINE_FUNCTION_CREATOR(FunNormalizeSpace)
DEFINE_FUNCTION_CREATOR(FunTranslate)

DEFINE_FUNCTION_CREATOR(FunBoolean)
DEFINE_FUNCTION_CREATOR(FunNot)
DEFINE_FUNCTION_CREATOR(FunTrue)
DEFINE_FUNCTION_CREATOR(FunFalse)
DEFINE_FUNCTION_CREATOR(FunLang)

DEFINE_FUNCTION_CREATOR(FunNumber)
DEFINE_FUNCTION_CREATOR(FunSum)
DEFINE_FUNCTION_CREATOR(FunFloor)
DEFINE_FUNCTION_CREATOR(FunCeiling)
DEFINE_FUNCTION_CREATOR(FunRound)

#undef DEFINE_FUNCTION_CREATOR

inline Interval::Interval()
    : m_min(Inf), m_max(Inf)
{
}

inline Interval::Interval(int value)
    : m_min(value), m_max(value)
{
}

inline Interval::Interval(int min, int max)
    : m_min(min), m_max(max)
{
}

inline bool Interval::contains(int value) const
{
    if (m_min == Inf && m_max == Inf)
        return true;

    if (m_min == Inf)
        return value <= m_max;

    if (m_max == Inf)
        return value >= m_min;

    return value >= m_min && value <= m_max;
}

void Function::setArguments(const Vector<Expression*>& args)
{
    Vector<Expression*>::const_iterator end = args.end();

    for (Vector<Expression*>::const_iterator it = args.begin(); it != end; it++)
        addSubExpression(*it);
}

Value FunLast::evaluate() const
{
    return Expression::evaluationContext().size;
}

Value FunPosition::evaluate() const
{
    return Expression::evaluationContext().position;
}

Value FunId::evaluate() const
{
    Value a = arg(0)->evaluate();
    Vector<UChar> idList; // A whitespace-separated list of IDs

    if (a.isNodeSet()) {
        const NodeSet& nodes = a.toNodeSet();
        for (size_t i = 0; i < nodes.size(); ++i) {
            String str = stringValue(nodes[i]);
            idList.append(str.characters(), str.length());
            idList.append(' ');
        }
    } else {
        String str = a.toString();
        idList.append(str.characters(), str.length());
    }
    
    Document* contextDocument = evaluationContext().node->document();
    NodeSet result;
    HashSet<Node*> resultSet;

    size_t startPos = 0;
    size_t length = idList.size();
    while (true) {
        while (startPos < length && isWhitespace(idList[startPos]))
            ++startPos;
        
        if (startPos == length)
            break;

        size_t endPos = startPos;
        while (endPos < length && !isWhitespace(idList[endPos]))
            ++endPos;

        // If there are several nodes with the same id, id() should return the first one.
        // In WebKit, getElementById behaves so, too, although its behavior in this case is formally undefined.
        Node* node = contextDocument->getElementById(String(&idList[startPos], endPos - startPos));
        if (node && resultSet.add(node).second)
            result.append(node);
        
        startPos = endPos;
    }
    
    result.markSorted(false);
    
    return Value(result, Value::adopt);
}

Value FunLocalName::evaluate() const
{
    Node* node = 0;
    if (argCount() > 0) {
        Value a = arg(0)->evaluate();
        if (!a.isNodeSet())
            return "";

        node = a.toNodeSet().firstNode();
        if (!node)
            return "";
    }

    if (!node)
        node = evaluationContext().node.get();

    return node->localName().string();
}

Value FunNamespaceURI::evaluate() const
{
    Node* node = 0;
    if (argCount() > 0) {
        Value a = arg(0)->evaluate();
        if (!a.isNodeSet())
            return "";

        node = a.toNodeSet().firstNode();
        if (!node)
            return "";
    }

    if (!node)
        node = evaluationContext().node.get();

    return node->namespaceURI().string();
}

Value FunName::evaluate() const
{
    Node* node = 0;
    if (argCount() > 0) {
        Value a = arg(0)->evaluate();
        if (!a.isNodeSet())
            return "";

        node = a.toNodeSet().firstNode();
        if (!node)
            return "";
    }

    if (!node)
        node = evaluationContext().node.get();

    const AtomicString& prefix = node->prefix();
    return prefix.isEmpty() ? node->localName().string() : prefix + ":" + node->localName();
}

Value FunCount::evaluate() const
{
    Value a = arg(0)->evaluate();
    
    if (!a.isNodeSet())
        return 0.0;
    
    return double(a.toNodeSet().size());
}

Value FunString::evaluate() const
{
    if (!argCount())
        return Value(Expression::evaluationContext().node.get()).toString();
    return arg(0)->evaluate().toString();
}

Value FunConcat::evaluate() const
{
    Vector<UChar, 1024> result;

    unsigned count = argCount();
    for (unsigned i = 0; i < count; ++i) {
        String str(arg(i)->evaluate().toString());
        result.append(str.characters(), str.length());
    }

    return String(result.data(), result.size());
}

Value FunStartsWith::evaluate() const
{
    String s1 = arg(0)->evaluate().toString();
    String s2 = arg(1)->evaluate().toString();

    if (s2.isEmpty())
        return true;

    return s1.startsWith(s2);
}

Value FunContains::evaluate() const
{
    String s1 = arg(0)->evaluate().toString();
    String s2 = arg(1)->evaluate().toString();

    if (s2.isEmpty()) 
        return true;

    return s1.contains(s2) != 0;
}

Value FunSubstringBefore::evaluate() const
{
    String s1 = arg(0)->evaluate().toString();
    String s2 = arg(1)->evaluate().toString();

    if (s2.isEmpty())
        return "";

    int i = s1.find(s2);

    if (i == -1)
        return "";

    return s1.left(i);
}

Value FunSubstringAfter::evaluate() const
{
    String s1 = arg(0)->evaluate().toString();
    String s2 = arg(1)->evaluate().toString();

    int i = s1.find(s2);
    if (i == -1)
        return "";

    return s1.substring(i + s2.length());
}

Value FunSubstring::evaluate() const
{
    String s = arg(0)->evaluate().toString();
    long pos = static_cast<long>(FunRound::round(arg(1)->evaluate().toNumber()));
    bool haveLength = argCount() == 3;
    long len = -1;
    if (haveLength) {
        double doubleLen = arg(2)->evaluate().toNumber();
        if (isnan(doubleLen))
            return "";
        len = static_cast<long>(FunRound::round(doubleLen));
    }

    if (pos > long(s.length())) 
        return "";

    if (haveLength && pos < 1) {
        len -= 1 - pos;
        pos = 1;
        if (len < 1)
            return "";
    }

    return s.substring(pos - 1, len);
}

Value FunStringLength::evaluate() const
{
    if (!argCount())
        return Value(Expression::evaluationContext().node.get()).toString().length();
    return arg(0)->evaluate().toString().length();
}

Value FunNormalizeSpace::evaluate() const
{
    if (!argCount()) {
        String s = Value(Expression::evaluationContext().node.get()).toString();
        return s.simplifyWhiteSpace();
    }

    String s = arg(0)->evaluate().toString();
    return s.simplifyWhiteSpace();
}

Value FunTranslate::evaluate() const
{
    String s1 = arg(0)->evaluate().toString();
    String s2 = arg(1)->evaluate().toString();
    String s3 = arg(2)->evaluate().toString();
    String newString;

    // FIXME: Building a String a character at a time is quite slow.
    for (unsigned i1 = 0; i1 < s1.length(); ++i1) {
        UChar ch = s1[i1];
        int i2 = s2.find(ch);
        
        if (i2 == -1)
            newString += String(&ch, 1);
        else if ((unsigned)i2 < s3.length()) {
            UChar c2 = s3[i2];
            newString += String(&c2, 1);
        }
    }

    return newString;
}

Value FunBoolean::evaluate() const
{
    return arg(0)->evaluate().toBoolean();
}

Value FunNot::evaluate() const
{
    return !arg(0)->evaluate().toBoolean();
}

Value FunTrue::evaluate() const
{
    return true;
}

Value FunLang::evaluate() const
{
    String lang = arg(0)->evaluate().toString();

    RefPtr<Node> langNode = 0;
    Node* node = evaluationContext().node.get();
    while (node) {
        NamedAttrMap* attrs = node->attributes();
        if (attrs)
            langNode = attrs->getNamedItemNS(XMLNames::xmlNamespaceURI, "lang");
        if (langNode)
            break;
        node = node->parentNode();
    }

    if (!langNode)
        return false;

    String langNodeValue = langNode->nodeValue();
    while (true) {
        if (equalIgnoringCase(langNodeValue, lang))
            return true;

        // Remove suffixes one by one.
        int index = langNodeValue.reverseFind('-');
        if (index == -1)
            break;
        langNodeValue = langNodeValue.left(index);
    }

    return false;
}

Value FunFalse::evaluate() const
{
    return false;
}

Value FunNumber::evaluate() const
{
    if (!argCount())
        return Value(Expression::evaluationContext().node.get()).toNumber();
    return arg(0)->evaluate().toNumber();
}

Value FunSum::evaluate() const
{
    Value a = arg(0)->evaluate();
    if (!a.isNodeSet())
        return 0.0;

    double sum = 0.0;
    const NodeSet& nodes = a.toNodeSet();
    // To be really compliant, we should sort the node-set, as floating point addition is not associative.
    // However, this is unlikely to ever become a practical issue, and sorting is slow.

    for (unsigned i = 0; i < nodes.size(); i++)
        sum += Value(stringValue(nodes[i])).toNumber();
    
    return sum;
}

Value FunFloor::evaluate() const
{
    return floor(arg(0)->evaluate().toNumber());
}

Value FunCeiling::evaluate() const
{
    return ceil(arg(0)->evaluate().toNumber());
}

double FunRound::round(double val)
{
    if (!isnan(val) && !isinf(val)) {
        if (signbit(val) && val >= -0.5)
            val *= 0; // negative zero
        else
            val = floor(val + 0.5);
    }
    return val;
}

Value FunRound::evaluate() const
{
    return round(arg(0)->evaluate().toNumber());
}

static void createFunctionMap()
{
    struct FunctionMapping {
        const char *name;
        FunctionRec function;
    };
    static const FunctionMapping functions[] = {
        { "boolean", { &createFunBoolean, 1 } },
        { "ceiling", { &createFunCeiling, 1 } },
        { "concat", { &createFunConcat, Interval(2, Interval::Inf) } },
        { "contains", { &createFunContains, 2 } },
        { "count", { &createFunCount, 1 } },
        { "false", { &createFunFalse, 0 } },
        { "floor", { &createFunFloor, 1 } },
        { "id", { &createFunId, 1 } },
        { "lang", { &createFunLang, 1 } },
        { "last", { &createFunLast, 0 } },
        { "local-name", { &createFunLocalName, Interval(0, 1) } },
        { "name", { &createFunName, Interval(0, 1) } },
        { "namespace-uri", { &createFunNamespaceURI, Interval(0, 1) } },
        { "normalize-space", { &createFunNormalizeSpace, Interval(0, 1) } },
        { "not", { &createFunNot, 1 } },
        { "number", { &createFunNumber, Interval(0, 1) } },
        { "position", { &createFunPosition, 0 } },
        { "round", { &createFunRound, 1 } },
        { "starts-with", { &createFunStartsWith, 2 } },
        { "string", { &createFunString, Interval(0, 1) } },
        { "string-length", { &createFunStringLength, Interval(0, 1) } },
        { "substring", { &createFunSubstring, Interval(2, 3) } },
        { "substring-after", { &createFunSubstringAfter, 2 } },
        { "substring-before", { &createFunSubstringBefore, 2 } },
        { "sum", { &createFunSum, 1 } },
        { "translate", { &createFunTranslate, 3 } },
        { "true", { &createFunTrue, 0 } },
    };
    const unsigned int numFunctions = sizeof(functions) / sizeof(functions[0]);

    functionMap = new HashMap<String, FunctionRec>;
    for (unsigned i = 0; i < numFunctions; ++i)
        functionMap->set(functions[i].name, functions[i].function);
}

Function* createFunction(const String& name, const Vector<Expression*>& args)
{
    if (!functionMap)
        createFunctionMap();

    HashMap<String, FunctionRec>::iterator functionMapIter = functionMap->find(name);
    FunctionRec* functionRec = 0;

    if (functionMapIter == functionMap->end() || !(functionRec = &functionMapIter->second)->args.contains(args.size()))
        return 0;

    Function* function = functionRec->factoryFn();
    function->setArguments(args);
    function->setName(name);
    return function;
}

}
}

#endif // ENABLE(XPATH)

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
#include "XPathFunctions.h"

#if XPATH_SUPPORT

#include "NamedAttrMap.h"
#include "XPathValue.h"
#include <wtf/MathExtras.h>

namespace WebCore {
namespace XPath {
        
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
    virtual bool isConstant() const;
    virtual Value doEvaluate() const;
};

class FunPosition : public Function {
    virtual bool isConstant() const;
    virtual Value doEvaluate() const;
};

class FunCount : public Function {
    virtual bool isConstant() const;
    virtual Value doEvaluate() const;
};

class FunLocalName : public Function {
    virtual bool isConstant() const;
    virtual Value doEvaluate() const;
};

class FunNamespaceURI : public Function {
    virtual bool isConstant() const;
    virtual Value doEvaluate() const;
};

class FunName : public Function {
    virtual bool isConstant() const;
    virtual Value doEvaluate() const;
};

class FunString : public Function {
    virtual Value doEvaluate() const;
};

class FunConcat : public Function {
    virtual Value doEvaluate() const;
};

class FunStartsWith : public Function {
    virtual Value doEvaluate() const;
};

class FunContains : public Function {
    virtual Value doEvaluate() const;
};

class FunSubstringBefore : public Function {
    virtual Value doEvaluate() const;
};

class FunSubstringAfter : public Function {
    virtual Value doEvaluate() const;
};

class FunSubstring : public Function {
    virtual Value doEvaluate() const;
};

class FunStringLength : public Function {
    virtual Value doEvaluate() const;
};

class FunNormalizeSpace : public Function {
    virtual Value doEvaluate() const;
};

class FunTranslate : public Function {
    virtual Value doEvaluate() const;
};

class FunBoolean : public Function {
    virtual Value doEvaluate() const;
};

class FunNot : public Function {
    virtual Value doEvaluate() const;
};

class FunTrue : public Function {
    virtual bool isConstant() const;
    virtual Value doEvaluate() const;
};

class FunFalse : public Function {
    virtual bool isConstant() const;
    virtual Value doEvaluate() const;
};

class FunLang : public Function {
    virtual bool isConstant() const;
    virtual Value doEvaluate() const;
};

class FunNumber : public Function {
    virtual Value doEvaluate() const;
};

class FunSum : public Function {
    virtual Value doEvaluate() const;
};

class FunFloor : public Function {
    virtual Value doEvaluate() const;
};

class FunCeiling : public Function {
    virtual Value doEvaluate() const;
};

class FunRound : public Function {
    virtual Value doEvaluate() const;
};

DEFINE_FUNCTION_CREATOR(FunLast)
DEFINE_FUNCTION_CREATOR(FunPosition)
DEFINE_FUNCTION_CREATOR(FunCount)
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

void Function::setName(const String& name)
{
    m_name = name;
}

Expression* Function::arg(int i)
{
    return subExpr(i);
}

const Expression* Function::arg(int i) const
{
    return subExpr(i);
}

unsigned int Function::argCount() const
{
    return subExprCount();
}

String Function::name() const
{
    return m_name;
}

Value FunLast::doEvaluate() const
{
    return Expression::evaluationContext().size;
}

bool FunLast::isConstant() const
{
    return false;
}

Value FunPosition::doEvaluate() const
{
    return Expression::evaluationContext().position;
}

bool FunPosition::isConstant() const
{
    return false;
}

bool FunLocalName::isConstant() const
{
    return false;
}

Value FunLocalName::doEvaluate() const
{
    Node* node = 0;
    if (argCount() > 0) {
        Value a = arg(0)->evaluate();
        if (!a.isNodeVector() || a.toNodeVector().size() == 0)
            return "";
        
        node = a.toNodeVector()[0].get();
    }

    if (!node)
        node = evaluationContext().node.get();

    return Value(node->localName());
}

bool FunNamespaceURI::isConstant() const
{
    return false;
}

Value FunNamespaceURI::doEvaluate() const
{
    Node* node = 0;
    if (argCount() > 0) {
        Value a = arg(0)->evaluate();
        if (!a.isNodeVector() || a.toNodeVector().size() == 0)
            return "";

        node = a.toNodeVector()[0].get();
    }

    if (!node)
        node = evaluationContext().node.get();

    return Value(node->namespaceURI());
}

bool FunName::isConstant() const
{
    return false;
}

Value FunName::doEvaluate() const
{
    Node* node = 0;
    if (argCount() > 0) {
        Value a = arg(0)->evaluate();
        if (!a.isNodeVector() || a.toNodeVector().size() == 0)
            return "";

        node = a.toNodeVector()[0].get();
    }

    if (!node)
        node = evaluationContext().node.get();

    return node->prefix() + ":" + node->localName();
}

Value FunCount::doEvaluate() const
{
    Value a = arg(0)->evaluate();
    
    if (!a.isNodeVector())
        return 0.0;
    
    return a.toNodeVector().size();
}

bool FunCount::isConstant() const
{
    return false;
}

Value FunString::doEvaluate() const
{
    if (argCount() == 0)
        return Value(Expression::evaluationContext().node).toString();
    return arg(0)->evaluate().toString();
}

Value FunConcat::doEvaluate() const
{
    String str = "";

    for (unsigned i = 0; i < argCount(); ++i)
        str += arg(i)->evaluate().toString();

    return str;
}

Value FunStartsWith::doEvaluate() const
{
    String s1 = arg(0)->evaluate().toString();
    String s2 = arg(1)->evaluate().toString();

    if (s2.isEmpty())
        return true;

    return s1.startsWith(s2);
}

Value FunContains::doEvaluate() const
{
    String s1 = arg(0)->evaluate().toString();
    String s2 = arg(1)->evaluate().toString();

    if (s2.isEmpty()) 
        return true;

    return s1.contains(s2) != 0;
}

Value FunSubstringBefore::doEvaluate() const
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

Value FunSubstringAfter::doEvaluate() const
{
    String s1 = arg(0)->evaluate().toString();
    String s2 = arg(1)->evaluate().toString();

    if (s2.isEmpty())
        return s2;

    int i = s1.find(s2);
    if (i == -1)
        return "";

    return s1.substring(i + 1);
}

Value FunSubstring::doEvaluate() const
{
    String s = arg(0)->evaluate().toString();
    long pos = lround(arg(1)->evaluate().toNumber());
    bool haveLength = argCount() == 3;
    long len = -1;
    if (haveLength)
        len = lround(arg(2)->evaluate().toNumber());

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

Value FunStringLength::doEvaluate() const
{
    if (argCount() == 0)
        return Value(Expression::evaluationContext().node).toString().length();
    return arg(0)->evaluate().toString().length();
}

Value FunNormalizeSpace::doEvaluate() const
{
    if (argCount() == 0) {
        String s = Value(Expression::evaluationContext().node).toString();
        return Value(s.deprecatedString().simplifyWhiteSpace());
    }

    String s = arg(0)->evaluate().toString();
    return Value(s.deprecatedString().simplifyWhiteSpace());
}

Value FunTranslate::doEvaluate() const
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

Value FunBoolean::doEvaluate() const
{
    return arg(0)->evaluate().toBoolean();
}

Value FunNot::doEvaluate() const
{
    return !arg(0)->evaluate().toBoolean();
}

Value FunTrue::doEvaluate() const
{
    return true;
}

bool FunTrue::isConstant() const
{
    return true;
}

Value FunLang::doEvaluate() const
{
    String lang = arg(0)->evaluate().toString();

    RefPtr<Node> langNode = 0;
    Node* node = evaluationContext().node.get();
    String xmsnsURI = node->lookupNamespaceURI("xms");
    while (node) {
        NamedAttrMap* attrs = node->attributes();
        langNode = attrs->getNamedItemNS(xmsnsURI, "lang");
        if (langNode)
            break;
        node = node->parentNode();
    }

    if (!langNode)
        return false;

    String langNodeValue = langNode->nodeValue();

    // extract 'en' out of 'en-us'
    int index = langNodeValue.find('-');
    if (index != -1)
        langNodeValue = langNodeValue.left(index);

    return equalIgnoringCase(langNodeValue, lang);
}

bool FunLang::isConstant() const
{
    return false;
}

Value FunFalse::doEvaluate() const
{
    return false;
}

bool FunFalse::isConstant() const
{
    return true;
}

Value FunNumber::doEvaluate() const
{
    return arg(0)->evaluate().toNumber();
}

Value FunSum::doEvaluate() const
{
    Value a = arg(0)->evaluate();
    if (!a.isNodeVector())
        return 0.0;

    double sum = 0.0;
    NodeVector nodes = a.toNodeVector();
    
    for (unsigned i = 0; i < nodes.size(); i++)
        sum += Value(stringValue(nodes[i].get())).toNumber();
    
    return sum;
}

Value FunFloor::doEvaluate() const
{
    return floor(arg(0)->evaluate().toNumber());
}

Value FunCeiling::doEvaluate() const
{
    return ceil(arg(0)->evaluate().toNumber());
}

Value FunRound::doEvaluate() const
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
        { "lang", { &createFunLang, 1 } },
        { "last", { &createFunLast, 0 } },
        { "local-name", { &createFunLocalName, Interval(0, 1) } },
        { "name", { &createFunName, Interval(0, 1) } },
        { "namespace-uri", { &createFunNamespaceURI, Interval(0, 1) } },
        { "normalize-space", { &createFunNormalizeSpace, 1 } },
        { "not", { &createFunNot, 1 } },
        { "number", { &createFunNumber, 1 } },
        { "position", { &createFunPosition, 0 } },
        { "round", { &createFunRound, 1 } },
        { "starts-with", { &createFunStartsWith, 2 } },
        { "string", { &createFunString, Interval(0, 1) } },
        { "string-length", { &createFunStringLength, 1 } },
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

    if (!functionMap->contains(name)) {
        deleteAllValues(args);

        // Return a dummy function instead of 0.
        Function* funcTrue = functionMap->get("true").factoryFn();
        funcTrue->setName("true");
        return funcTrue;
    }

    FunctionRec functionRec = functionMap->get(name);
    if (!functionRec.args.contains(args.size())) {
        deleteAllValues(args);
        return 0;
    }

    Function* function = functionRec.factoryFn();
    function->setArguments(args);
    function->setName(name);
    return function;
}

}
}

#endif // XPATH_SUPPORT

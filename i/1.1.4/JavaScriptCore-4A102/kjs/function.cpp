// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003 Apple Computer, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "function.h"

#include "internal.h"
#include "function_object.h"
#include "lexer.h"
#include "nodes.h"
#include "operations.h"
#include "debugger.h"
#include "context.h"

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>

#include <wtf/unicode/Unicode.h>

namespace KJS {

// ----------------------------- FunctionImp ----------------------------------

const ClassInfo FunctionImp::info = {"Function", &InternalFunctionImp::info, 0, 0};

  class Parameter {
  public:
    Parameter(const Identifier &n) : name(n) { }
    Identifier name;
    OwnPtr<Parameter> next;
  };

FunctionImp::FunctionImp(ExecState *exec, const Identifier &n, FunctionBodyNode* b)
  : InternalFunctionImp(static_cast<FunctionPrototype*>
                        (exec->lexicalInterpreter()->builtinFunctionPrototype()), n)
  , body(b)
{
}

FunctionImp::~FunctionImp()
{
}

JSValue *FunctionImp::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
  JSObject *globalObj = exec->dynamicInterpreter()->globalObject();

  // enter a new execution context
  Context ctx(globalObj, exec->dynamicInterpreter(), thisObj, body.get(),
                 codeType(), exec->context(), this, &args);
  ExecState newExec(exec->dynamicInterpreter(), &ctx);
  if (exec->hadException())
    newExec.setException(exec->exception());

  // assign user supplied arguments to parameters
  processParameters(&newExec, args);
  // add variable declarations (initialized to undefined)
  processVarDecls(&newExec);

  Debugger *dbg = exec->dynamicInterpreter()->debugger();
  int sid = -1;
  int lineno = -1;
  if (dbg) {
    if (inherits(&DeclaredFunctionImp::info)) {
      sid = static_cast<DeclaredFunctionImp*>(this)->body->sourceId();
      lineno = static_cast<DeclaredFunctionImp*>(this)->body->firstLine();
    }

    bool cont = dbg->callEvent(&newExec,sid,lineno,this,args);
    if (!cont) {
      dbg->imp()->abort();
      return jsUndefined();
    }
  }

  Completion comp = execute(&newExec);

  // if an exception occured, propogate it back to the previous execution object
  if (newExec.hadException())
    comp = Completion(Throw, newExec.exception());

#ifdef KJS_VERBOSE
  if (comp.complType() == Throw)
    printInfo(exec,"throwing", comp.value());
  else if (comp.complType() == ReturnValue)
    printInfo(exec,"returning", comp.value());
  else
    fprintf(stderr, "returning: undefined\n");
#endif

  // The debugger may have been deallocated by now if the WebFrame
  // we were running in has been destroyed, so refetch it.
  // See http://bugzilla.opendarwin.org/show_bug.cgi?id=9477
  dbg = exec->dynamicInterpreter()->debugger();

  if (dbg) {
    if (inherits(&DeclaredFunctionImp::info))
      lineno = static_cast<DeclaredFunctionImp*>(this)->body->lastLine();

    if (comp.complType() == Throw)
        newExec.setException(comp.value());

    int cont = dbg->returnEvent(&newExec,sid,lineno,this);
    if (!cont) {
      dbg->imp()->abort();
      return jsUndefined();
    }
  }

  if (comp.complType() == Throw) {
    exec->setException(comp.value());
    return comp.value();
  }
  else if (comp.complType() == ReturnValue)
    return comp.value();
  else
    return jsUndefined();
}

void FunctionImp::addParameter(const Identifier &n)
{
  OwnPtr<Parameter> *p = &param;
  while (*p)
    p = &(*p)->next;

  p->set(new Parameter(n));
}

UString FunctionImp::parameterString() const
{
  UString s;
  const Parameter *p = param.get();
  while (p) {
    if (!s.isEmpty())
        s += ", ";
    s += p->name.ustring();
    p = p->next.get();
  }

  return s;
}


// ECMA 10.1.3q
void FunctionImp::processParameters(ExecState *exec, const List &args)
{
  JSObject* variable = exec->context()->variableObject();

#ifdef KJS_VERBOSE
  fprintf(stderr, "---------------------------------------------------\n"
	  "processing parameters for %s call\n",
	  name().isEmpty() ? "(internal)" : name().ascii());
#endif

  if (param) {
    ListIterator it = args.begin();
    Parameter *p = param.get();
    JSValue  *v = *it;
    while (p) {
      if (it != args.end()) {
#ifdef KJS_VERBOSE
	fprintf(stderr, "setting parameter %s ", p->name.ascii());
	printInfo(exec,"to", *it);
#endif
	variable->put(exec, p->name, v);
	v = ++it;
      } else
	variable->put(exec, p->name, jsUndefined());
      p = p->next.get();
    }
  }
#ifdef KJS_VERBOSE
  else {
    for (int i = 0; i < args.size(); i++)
      printInfo(exec,"setting argument", args[i]);
  }
#endif
}

void FunctionImp::processVarDecls(ExecState */*exec*/)
{
}

JSValue *FunctionImp::argumentsGetter(ExecState* exec, JSObject*, const Identifier& propertyName, const PropertySlot& slot)
{
  FunctionImp *thisObj = static_cast<FunctionImp *>(slot.slotBase());
  Context *context = exec->m_context;
  while (context) {
    if (context->function() == thisObj) {
      return static_cast<ActivationImp *>(context->activationObject())->get(exec, propertyName);
    }
    context = context->callingContext();
  }
  return jsNull();
}

JSValue *FunctionImp::lengthGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot& slot)
{
  FunctionImp *thisObj = static_cast<FunctionImp *>(slot.slotBase());
  const Parameter *p = thisObj->param.get();
  int count = 0;
  while (p) {
    ++count;
    p = p->next.get();
  }
  return jsNumber(count);
}

bool FunctionImp::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
    // Find the arguments from the closest context.
    if (propertyName == exec->dynamicInterpreter()->argumentsIdentifier()) {
        slot.setCustom(this, argumentsGetter);
        return true;
    }
    
    // Compute length of parameters.
    if (propertyName == lengthPropertyName) {
        slot.setCustom(this, lengthGetter);
        return true;
    }
    
    return InternalFunctionImp::getOwnPropertySlot(exec, propertyName, slot);
}

void FunctionImp::put(ExecState *exec, const Identifier &propertyName, JSValue *value, int attr)
{
    if (propertyName == exec->dynamicInterpreter()->argumentsIdentifier() || propertyName == lengthPropertyName)
        return;
    InternalFunctionImp::put(exec, propertyName, value, attr);
}

bool FunctionImp::deleteProperty(ExecState *exec, const Identifier &propertyName)
{
    if (propertyName == exec->dynamicInterpreter()->argumentsIdentifier() || propertyName == lengthPropertyName)
        return false;
    return InternalFunctionImp::deleteProperty(exec, propertyName);
}

/* Returns the parameter name corresponding to the given index. eg:
 * function f1(x, y, z): getParameterName(0) --> x
 *
 * If a name appears more than once, only the last index at which
 * it appears associates with it. eg:
 * function f2(x, x): getParameterName(0) --> null
 */
Identifier FunctionImp::getParameterName(int index)
{
  int i = 0;
  Parameter *p = param.get();
  
  if(!p)
    return Identifier::null();
  
  // skip to the parameter we want
  while (i++ < index && (p = p->next.get()))
    ;
  
  if (!p)
    return Identifier::null();
  
  Identifier name = p->name;

  // Are there any subsequent parameters with the same name?
  while ((p = p->next.get()))
    if (p->name == name)
      return Identifier::null();
  
  return name;
}

// ------------------------------ DeclaredFunctionImp --------------------------

// ### is "Function" correct here?
const ClassInfo DeclaredFunctionImp::info = {"Function", &FunctionImp::info, 0, 0};

DeclaredFunctionImp::DeclaredFunctionImp(ExecState *exec, const Identifier &n,
					 FunctionBodyNode *b, const ScopeChain &sc)
  : FunctionImp(exec, n, b)
{
  setScope(sc);
}

bool DeclaredFunctionImp::implementsConstruct() const
{
  return true;
}

// ECMA 13.2.2 [[Construct]]
JSObject *DeclaredFunctionImp::construct(ExecState *exec, const List &args)
{
  JSObject *proto;
  JSValue *p = get(exec,prototypePropertyName);
  if (p->isObject())
    proto = static_cast<JSObject*>(p);
  else
    proto = exec->lexicalInterpreter()->builtinObjectPrototype();

  JSObject *obj(new JSObject(proto));

  JSValue *res = call(exec,obj,args);

  if (res->isObject())
    return static_cast<JSObject *>(res);
  else
    return obj;
}

Completion DeclaredFunctionImp::execute(ExecState *exec)
{
  Completion result = body->execute(exec);

  if (result.complType() == Throw || result.complType() == ReturnValue)
      return result;
  return Completion(Normal, jsUndefined()); // TODO: or ReturnValue ?
}

void DeclaredFunctionImp::processVarDecls(ExecState *exec)
{
  body->processVarDecls(exec);
}

// ------------------------------ IndexToNameMap ---------------------------------

// We map indexes in the arguments array to their corresponding argument names. 
// Example: function f(x, y, z): arguments[0] = x, so we map 0 to Identifier("x"). 

// Once we have an argument name, we can get and set the argument's value in the 
// activation object.

// We use Identifier::null to indicate that a given argument's value
// isn't stored in the activation object.

IndexToNameMap::IndexToNameMap(FunctionImp *func, const List &args)
{
  _map = new Identifier[args.size()];
  this->size = args.size();
  
  int i = 0;
  ListIterator iterator = args.begin(); 
  for (; iterator != args.end(); i++, iterator++)
    _map[i] = func->getParameterName(i); // null if there is no corresponding parameter
}

IndexToNameMap::~IndexToNameMap() {
  delete [] _map;
}

bool IndexToNameMap::isMapped(const Identifier &index) const
{
  bool indexIsNumber;
  int indexAsNumber = index.toUInt32(&indexIsNumber);
  
  if (!indexIsNumber)
    return false;
  
  if (indexAsNumber >= size)
    return false;

  if (_map[indexAsNumber].isNull())
    return false;
  
  return true;
}

void IndexToNameMap::unMap(const Identifier &index)
{
  bool indexIsNumber;
  int indexAsNumber = index.toUInt32(&indexIsNumber);

  assert(indexIsNumber && indexAsNumber < size);
  
  _map[indexAsNumber] = Identifier::null();
}

Identifier& IndexToNameMap::operator[](int index)
{
  return _map[index];
}

Identifier& IndexToNameMap::operator[](const Identifier &index)
{
  bool indexIsNumber;
  int indexAsNumber = index.toUInt32(&indexIsNumber);

  assert(indexIsNumber && indexAsNumber < size);
  
  return (*this)[indexAsNumber];
}

// ------------------------------ Arguments ---------------------------------

const ClassInfo Arguments::info = {"Arguments", 0, 0, 0};

// ECMA 10.1.8
Arguments::Arguments(ExecState *exec, FunctionImp *func, const List &args, ActivationImp *act)
: JSObject(exec->lexicalInterpreter()->builtinObjectPrototype()), 
_activationObject(act),
indexToNameMap(func, args)
{
  putDirect(calleePropertyName, func, DontEnum);
  putDirect(lengthPropertyName, args.size(), DontEnum);
  
  int i = 0;
  ListIterator iterator = args.begin(); 
  for (; iterator != args.end(); i++, iterator++) {
    if (!indexToNameMap.isMapped(Identifier::from(i))) {
      JSObject::put(exec, Identifier::from(i), *iterator, DontEnum);
    }
  }
}

void Arguments::mark() 
{
  JSObject::mark();
  if (_activationObject && !_activationObject->marked())
    _activationObject->mark();
}

JSValue *Arguments::mappedIndexGetter(ExecState* exec, JSObject*, const Identifier& propertyName, const PropertySlot& slot)
{
  Arguments *thisObj = static_cast<Arguments *>(slot.slotBase());
  return thisObj->_activationObject->get(exec, thisObj->indexToNameMap[propertyName]);
}

bool Arguments::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  if (indexToNameMap.isMapped(propertyName)) {
    slot.setCustom(this, mappedIndexGetter);
    return true;
  }

  return JSObject::getOwnPropertySlot(exec, propertyName, slot);
}

void Arguments::put(ExecState *exec, const Identifier &propertyName, JSValue *value, int attr)
{
  if (indexToNameMap.isMapped(propertyName)) {
    _activationObject->put(exec, indexToNameMap[propertyName], value, attr);
  } else {
    JSObject::put(exec, propertyName, value, attr);
  }
}

bool Arguments::deleteProperty(ExecState *exec, const Identifier &propertyName) 
{
  if (indexToNameMap.isMapped(propertyName)) {
    indexToNameMap.unMap(propertyName);
    return true;
  } else {
    return JSObject::deleteProperty(exec, propertyName);
  }
}

// ------------------------------ ActivationImp --------------------------------

const ClassInfo ActivationImp::info = {"Activation", 0, 0, 0};

// ECMA 10.1.6
ActivationImp::ActivationImp(FunctionImp *function, const List &arguments)
    : _function(function), _arguments(true), _argumentsObject(0)
{
  _arguments.copyFrom(arguments);
  // FIXME: Do we need to support enumerating the arguments property?
}

JSValue *ActivationImp::argumentsGetter(ExecState* exec, JSObject*, const Identifier&, const PropertySlot& slot)
{
  ActivationImp *thisObj = static_cast<ActivationImp *>(slot.slotBase());

  // default: return builtin arguments array
  if (!thisObj->_argumentsObject)
    thisObj->createArgumentsObject(exec);
  
  return thisObj->_argumentsObject;
}

PropertySlot::GetValueFunc ActivationImp::getArgumentsGetter()
{
  return ActivationImp::argumentsGetter;
}

bool ActivationImp::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
    // do this first so property map arguments property wins over the below
    // we don't call JSObject because we won't have getter/setter properties
    // and we don't want to support __proto__

    if (JSValue **location = getDirectLocation(propertyName)) {
        slot.setValueSlot(this, location);
        return true;
    }

    if (propertyName == exec->dynamicInterpreter()->argumentsIdentifier()) {
        slot.setCustom(this, getArgumentsGetter());
        return true;
    }

    return false;
}

bool ActivationImp::deleteProperty(ExecState *exec, const Identifier &propertyName)
{
    if (propertyName == exec->dynamicInterpreter()->argumentsIdentifier())
        return false;
    return JSObject::deleteProperty(exec, propertyName);
}

void ActivationImp::put(ExecState*, const Identifier& propertyName, JSValue* value, int attr)
{
  // There's no way that an activation object can have a prototype or getter/setter properties
  assert(!_prop.hasGetterSetterProperties());
  assert(prototype() == jsNull());

  _prop.put(propertyName, value, attr, (attr == None || attr == DontDelete));
}

void ActivationImp::mark()
{
    if (_function && !_function->marked()) 
        _function->mark();
    _arguments.mark();
    if (_argumentsObject && !_argumentsObject->marked())
        _argumentsObject->mark();
    JSObject::mark();
}

void ActivationImp::createArgumentsObject(ExecState *exec) const
{
  _argumentsObject = new Arguments(exec, _function, _arguments, const_cast<ActivationImp *>(this));
}

// ------------------------------ GlobalFunc -----------------------------------


GlobalFuncImp::GlobalFuncImp(ExecState*, FunctionPrototype* funcProto, int i, int len, const Identifier& name)
  : InternalFunctionImp(funcProto, name)
  , id(i)
{
  putDirect(lengthPropertyName, len, DontDelete|ReadOnly|DontEnum);
}

CodeType GlobalFuncImp::codeType() const
{
  return id == Eval ? EvalCode : codeType();
}

static JSValue *encode(ExecState *exec, const List &args, const char *do_not_escape)
{
  UString r = "", s, str = args[0]->toString(exec);
  CString cstr = str.UTF8String();
  const char *p = cstr.c_str();
  for (size_t k = 0; k < cstr.size(); k++, p++) {
    char c = *p;
    if (c && strchr(do_not_escape, c)) {
      r.append(c);
    } else {
      char tmp[4];
      snprintf(tmp, sizeof(tmp), "%%%02X", (unsigned char)c);
      r += tmp;
    }
  }
  return jsString(r);
}

static JSValue *decode(ExecState *exec, const List &args, const char *do_not_unescape, bool strict)
{
  UString s = "", str = args[0]->toString(exec);
  int k = 0, len = str.size();
  const UChar *d = str.data();
  UChar u;
  while (k < len) {
    const UChar *p = d + k;
    UChar c = *p;
    if (c == '%') {
      int charLen = 0;
      if (k <= len - 3 && isxdigit(p[1].uc) && isxdigit(p[2].uc)) {
        const char b0 = Lexer::convertHex(p[1].uc, p[2].uc);
        const int sequenceLen = UTF8SequenceLength(b0);
        if (sequenceLen != 0 && k <= len - sequenceLen * 3) {
          charLen = sequenceLen * 3;
          char sequence[5];
          sequence[0] = b0;
          for (int i = 1; i < sequenceLen; ++i) {
            const UChar *q = p + i * 3;
            if (q[0] == '%' && isxdigit(q[1].uc) && isxdigit(q[2].uc))
              sequence[i] = Lexer::convertHex(q[1].uc, q[2].uc);
            else {
              charLen = 0;
              break;
            }
          }
          if (charLen != 0) {
            sequence[sequenceLen] = 0;
            const int character = decodeUTF8Sequence(sequence);
            if (character < 0 || character >= 0x110000) {
              charLen = 0;
            } else if (character >= 0x10000) {
              // Convert to surrogate pair.
              s.append(static_cast<unsigned short>(0xD800 | ((character - 0x10000) >> 10)));
              u = static_cast<unsigned short>(0xDC00 | ((character - 0x10000) & 0x3FF));
            } else {
              u = static_cast<unsigned short>(character);
            }
          }
        }
      }
      if (charLen == 0) {
        if (strict)
          return throwError(exec, URIError);
        // The only case where we don't use "strict" mode is the "unescape" function.
        // For that, it's good to support the wonky "%u" syntax for compatibility with WinIE.
        if (k <= len - 6 && p[1] == 'u'
            && isxdigit(p[2].uc) && isxdigit(p[3].uc)
            && isxdigit(p[4].uc) && isxdigit(p[5].uc)) {
	  charLen = 6;
	  u = Lexer::convertUnicode(p[2].uc, p[3].uc, p[4].uc, p[5].uc);
        }
      }
      if (charLen && (u.uc == 0 || u.uc >= 128 || !strchr(do_not_unescape, u.low()))) {
        c = u;
        k += charLen - 1;
      }
    }
    k++;
    s.append(c);
  }
  return jsString(s);
}

static bool isStrWhiteSpace(unsigned short c)
{
    switch (c) {
        case 0x0009:
        case 0x000A:
        case 0x000B:
        case 0x000C:
        case 0x000D:
        case 0x0020:
        case 0x00A0:
        case 0x2028:
        case 0x2029:
            return true;
        default:
            return WTF::Unicode::isSeparatorSpace(c);
    }
}

static int parseDigit(unsigned short c, int radix)
{
    int digit = -1;

    if (c >= '0' && c <= '9') {
        digit = c - '0';
    } else if (c >= 'A' && c <= 'Z') {
        digit = c - 'A' + 10;
    } else if (c >= 'a' && c <= 'z') {
        digit = c - 'a' + 10;
    }

    if (digit >= radix)
        return -1;
    return digit;
}

static double parseInt(const UString &s, int radix)
{
    int length = s.size();
    int p = 0;

    while (p < length && isStrWhiteSpace(s[p].uc)) {
        ++p;
    }

    double sign = 1;
    if (p < length) {
        if (s[p] == '+') {
            ++p;
        } else if (s[p] == '-') {
            sign = -1;
            ++p;
        }
    }

    if ((radix == 0 || radix == 16) && length - p >= 2 && s[p] == '0' && (s[p + 1] == 'x' || s[p + 1] == 'X')) {
        radix = 16;
        p += 2;
    } else if (radix == 0) {
        if (p < length && s[p] == '0')
            radix = 8;
        else
            radix = 10;
    }

    if (radix < 2 || radix > 36)
        return NaN;

    bool sawDigit = false;
    double number = 0;
    while (p < length) {
        int digit = parseDigit(s[p].uc, radix);
        if (digit == -1)
            break;
        sawDigit = true;
        number *= radix;
        number += digit;
        ++p;
    }

    if (!sawDigit)
        return NaN;

    return sign * number;
}

static double parseFloat(const UString &s)
{
    // Check for 0x prefix here, because toDouble allows it, but we must treat it as 0.
    // Need to skip any whitespace and then one + or - sign.
    int length = s.size();
    int p = 0;
    while (p < length && isStrWhiteSpace(s[p].uc)) {
        ++p;
    }
    if (p < length && (s[p] == '+' || s[p] == '-')) {
        ++p;
    }
    if (length - p >= 2 && s[p] == '0' && (s[p + 1] == 'x' || s[p + 1] == 'X')) {
        return 0;
    }

    return s.toDouble( true /*tolerant*/, false /* NaN for empty string */ );
}

JSValue *GlobalFuncImp::callAsFunction(ExecState *exec, JSObject */*thisObj*/, const List &args)
{
  JSValue *res = jsUndefined();

  static const char do_not_escape[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789"
    "*+-./@_";

  static const char do_not_escape_when_encoding_URI_component[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789"
    "!'()*-._~";
  static const char do_not_escape_when_encoding_URI[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789"
    "!#$&'()*+,-./:;=?@_~";
  static const char do_not_unescape_when_decoding_URI[] =
    "#$&+,/:;=?@";

  switch (id) {
    case Eval: { // eval()
      JSValue *x = args[0];
      if (!x->isString())
        return x;
      else {
        UString s = x->toString(exec);
        
        int sid;
        int errLine;
        UString errMsg;
        RefPtr<ProgramNode> progNode(Parser::parse(UString(), 0, s.data(),s.size(),&sid,&errLine,&errMsg));

        Debugger *dbg = exec->dynamicInterpreter()->debugger();
        if (dbg) {
          bool cont = dbg->sourceParsed(exec, sid, UString(), s, 0, errLine, errMsg);
          if (!cont)
            return jsUndefined();
        }

        // no program node means a syntax occurred
        if (!progNode)
          return throwError(exec, SyntaxError, errMsg, errLine, sid, NULL);

        // enter a new execution context
        JSObject *thisVal = static_cast<JSObject *>(exec->context()->thisValue());
        Context ctx(exec->dynamicInterpreter()->globalObject(),
                       exec->dynamicInterpreter(),
                       thisVal,
                       progNode.get(),
                       EvalCode,
                       exec->context());
        
        ExecState newExec(exec->dynamicInterpreter(), &ctx);
        if (exec->hadException())
            newExec.setException(exec->exception());
        
        // execute the code
        progNode->processVarDecls(&newExec);
        Completion c = progNode->execute(&newExec);

        // if an exception occured, propogate it back to the previous execution object
        if (newExec.hadException())
          exec->setException(newExec.exception());

        res = jsUndefined();
        if (c.complType() == Throw)
          exec->setException(c.value());
        else if (c.isValueCompletion())
            res = c.value();
      }
      break;
    }
  case ParseInt:
    res = jsNumber(parseInt(args[0]->toString(exec), args[1]->toInt32(exec)));
    break;
  case ParseFloat:
    res = jsNumber(parseFloat(args[0]->toString(exec)));
    break;
  case IsNaN:
    res = jsBoolean(isNaN(args[0]->toNumber(exec)));
    break;
  case IsFinite: {
    double n = args[0]->toNumber(exec);
    res = jsBoolean(!isNaN(n) && !isInf(n));
    break;
  }
  case DecodeURI:
    res = decode(exec, args, do_not_unescape_when_decoding_URI, true);
    break;
  case DecodeURIComponent:
    res = decode(exec, args, "", true);
    break;
  case EncodeURI:
    res = encode(exec, args, do_not_escape_when_encoding_URI);
    break;
  case EncodeURIComponent:
    res = encode(exec, args, do_not_escape_when_encoding_URI_component);
    break;
  case Escape:
    {
      UString r = "", s, str = args[0]->toString(exec);
      const UChar *c = str.data();
      for (int k = 0; k < str.size(); k++, c++) {
        int u = c->uc;
        if (u > 255) {
          char tmp[7];
          snprintf(tmp, sizeof(tmp), "%%u%04X", u);
          s = UString(tmp);
        } else if (u != 0 && strchr(do_not_escape, (char)u)) {
          s = UString(c, 1);
        } else {
          char tmp[4];
          snprintf(tmp, sizeof(tmp), "%%%02X", u);
          s = UString(tmp);
        }
        r += s;
      }
      res = jsString(r);
      break;
    }
  case UnEscape:
    {
      UString s = "", str = args[0]->toString(exec);
      int k = 0, len = str.size();
      while (k < len) {
        const UChar *c = str.data() + k;
        UChar u;
        if (*c == UChar('%') && k <= len - 6 && *(c+1) == UChar('u')) {
          if (Lexer::isHexDigit((c+2)->uc) && Lexer::isHexDigit((c+3)->uc) &&
              Lexer::isHexDigit((c+4)->uc) && Lexer::isHexDigit((c+5)->uc)) {
	  u = Lexer::convertUnicode((c+2)->uc, (c+3)->uc,
				    (c+4)->uc, (c+5)->uc);
	  c = &u;
	  k += 5;
          }
        } else if (*c == UChar('%') && k <= len - 3 &&
                   Lexer::isHexDigit((c+1)->uc) && Lexer::isHexDigit((c+2)->uc)) {
          u = UChar(Lexer::convertHex((c+1)->uc, (c+2)->uc));
          c = &u;
          k += 2;
        }
        k++;
        s += UString(c, 1);
      }
      res = jsString(s);
      break;
    }
#ifndef NDEBUG
  case KJSPrint:
    puts(args[0]->toString(exec).ascii());
    break;
#endif
  }

  return res;
}

UString escapeStringForPrettyPrinting(const UString& s)
{
    UString escapedString;
    
    for (int i = 0; i < s.size(); i++) {
        unsigned short c = s.data()[i].unicode();
        
        switch (c) {
        case '\"':
            escapedString += "\\\"";
            break;
        case '\n':
            escapedString += "\\n";
            break;
        case '\r':
            escapedString += "\\r";
            break;
        case '\t':
            escapedString += "\\t";
            break;
        case '\\':
            escapedString += "\\\\";
            break;
        default:
            if (c < 128 && WTF::Unicode::isPrintableChar(c))
                escapedString.append(c);
            else {
                char hexValue[7];
            
#if PLATFORM(WIN_OS)
                _snprintf(hexValue, 7, "\\u%04x", c);
#else
                snprintf(hexValue, 7, "\\u%04x", c);
#endif
                escapedString += hexValue;
            }
        }
    }
    
    return escapedString;    
}


} // namespace

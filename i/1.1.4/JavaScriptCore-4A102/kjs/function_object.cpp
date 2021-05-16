// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "function_object.h"
#include "internal.h"
#include "function.h"
#include "array_object.h"
#include "nodes.h"
#include "lexer.h"
#include "debugger.h"
#include "object.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

using namespace KJS;

// ------------------------------ FunctionPrototype -------------------------

FunctionPrototype::FunctionPrototype(ExecState *exec)
{
  putDirect(lengthPropertyName, jsNumber(0), DontDelete|ReadOnly|DontEnum);
  putDirectFunction(new FunctionProtoFunc(exec, this, FunctionProtoFunc::ToString, 0, toStringPropertyName), DontEnum);
  static const Identifier applyPropertyName("apply");
  putDirectFunction(new FunctionProtoFunc(exec, this, FunctionProtoFunc::Apply, 2, applyPropertyName), DontEnum);
  static const Identifier callPropertyName("call");
  putDirectFunction(new FunctionProtoFunc(exec, this, FunctionProtoFunc::Call, 1, callPropertyName), DontEnum);
}

FunctionPrototype::~FunctionPrototype()
{
}

// ECMA 15.3.4
JSValue *FunctionPrototype::callAsFunction(ExecState */*exec*/, JSObject */*thisObj*/, const List &/*args*/)
{
  return jsUndefined();
}

// ------------------------------ FunctionProtoFunc -------------------------

FunctionProtoFunc::FunctionProtoFunc(ExecState*, FunctionPrototype* funcProto, int i, int len, const Identifier& name)
  : InternalFunctionImp(funcProto, name)
  , id(i)
{
  putDirect(lengthPropertyName, len, DontDelete|ReadOnly|DontEnum);
}

JSValue *FunctionProtoFunc::callAsFunction(ExecState *exec, JSObject *thisObj, const List &args)
{
  JSValue *result = NULL;

  switch (id) {
  case ToString:
    if (!thisObj || !thisObj->inherits(&InternalFunctionImp::info)) {
#ifndef NDEBUG
      fprintf(stderr,"attempted toString() call on null or non-function object\n");
#endif
      return throwError(exec, TypeError);
    }
    if (thisObj->inherits(&DeclaredFunctionImp::info)) {
        DeclaredFunctionImp *fi = static_cast<DeclaredFunctionImp*>(thisObj);
        return jsString("function " + fi->functionName().ustring() + "(" +
                        fi->parameterString() + ") " + fi->body->toString());
     } else if (thisObj->inherits(&InternalFunctionImp::info) &&
                !static_cast<InternalFunctionImp*>(thisObj)->functionName().isNull()) {
       result = jsString("\nfunction " + static_cast<InternalFunctionImp*>(thisObj)->functionName().ustring() + "() {\n"
                       "    [native code]\n}\n");
    } else {
      result = jsString("[function]");
    }
    break;
  case Apply: {
    JSValue *thisArg = args[0];
    JSValue *argArray = args[1];
    JSObject *func = thisObj;

    if (!func->implementsCall())
      return throwError(exec, TypeError);

    JSObject *applyThis;
    if (thisArg->isUndefinedOrNull())
      applyThis = exec->dynamicInterpreter()->globalObject();
    else
      applyThis = thisArg->toObject(exec);

    List applyArgs;
    if (!argArray->isUndefinedOrNull()) {
      if (argArray->isObject() &&
           (static_cast<JSObject *>(argArray)->inherits(&ArrayInstance::info) ||
            static_cast<JSObject *>(argArray)->inherits(&Arguments::info))) {

        JSObject *argArrayObj = static_cast<JSObject *>(argArray);
        unsigned int length = argArrayObj->get(exec,lengthPropertyName)->toUInt32(exec);
        for (unsigned int i = 0; i < length; i++)
          applyArgs.append(argArrayObj->get(exec,i));
      }
      else
        return throwError(exec, TypeError);
    }
    result = func->call(exec,applyThis,applyArgs);
    }
    break;
  case Call: {
    JSValue *thisArg = args[0];
    JSObject *func = thisObj;

    if (!func->implementsCall())
      return throwError(exec, TypeError);

    JSObject *callThis;
    if (thisArg->isUndefinedOrNull())
      callThis = exec->dynamicInterpreter()->globalObject();
    else
      callThis = thisArg->toObject(exec);

    result = func->call(exec,callThis,args.copyTail());
    }
    break;
  }

  return result;
}

// ------------------------------ FunctionObjectImp ----------------------------

FunctionObjectImp::FunctionObjectImp(ExecState*, FunctionPrototype* funcProto)
  : InternalFunctionImp(funcProto)
{
  putDirect(prototypePropertyName, funcProto, DontEnum|DontDelete|ReadOnly);

  // no. of arguments for constructor
  putDirect(lengthPropertyName, jsNumber(1), ReadOnly|DontDelete|DontEnum);
}

FunctionObjectImp::~FunctionObjectImp()
{
}

bool FunctionObjectImp::implementsConstruct() const
{
  return true;
}

// ECMA 15.3.2 The Function Constructor
JSObject* FunctionObjectImp::construct(ExecState* exec, const List& args, const Identifier& functionName, const UString& sourceURL, int lineNumber)
{
  UString p("");
  UString body;
  int argsSize = args.size();
  if (argsSize == 0) {
    body = "";
  } else if (argsSize == 1) {
    body = args[0]->toString(exec);
  } else {
    p = args[0]->toString(exec);
    for (int k = 1; k < argsSize - 1; k++)
      p += "," + args[k]->toString(exec);
    body = args[argsSize-1]->toString(exec);
  }

  // parse the source code
  int sid;
  int errLine;
  UString errMsg;
  RefPtr<ProgramNode> progNode = Parser::parse(sourceURL, lineNumber, body.data(),body.size(),&sid,&errLine,&errMsg);

  // notify debugger that source has been parsed
  Debugger *dbg = exec->dynamicInterpreter()->debugger();
  if (dbg) {
    // send empty sourceURL to indicate constructed code
    bool cont = dbg->sourceParsed(exec, sid, UString(), body, lineNumber, errLine, errMsg);
    if (!cont) {
      dbg->imp()->abort();
      return new JSObject();
    }
  }

  // no program node == syntax error - throw a syntax error
  if (!progNode)
    // we can't return a Completion(Throw) here, so just set the exception
    // and return it
    return throwError(exec, SyntaxError, errMsg, errLine, sid, sourceURL);

  ScopeChain scopeChain;
  scopeChain.push(exec->lexicalInterpreter()->globalObject());
  FunctionBodyNode *bodyNode = progNode.get();

  FunctionImp* fimp = new DeclaredFunctionImp(exec, functionName, bodyNode, scopeChain);
  
  // parse parameter list. throw syntax error on illegal identifiers
  int len = p.size();
  const UChar *c = p.data();
  int i = 0, params = 0;
  UString param;
  while (i < len) {
      while (*c == ' ' && i < len)
          c++, i++;
      if (Lexer::isIdentStart(c->uc)) {  // else error
          param = UString(c, 1);
          c++, i++;
          while (i < len && (Lexer::isIdentPart(c->uc))) {
              param += UString(c, 1);
              c++, i++;
          }
          while (i < len && *c == ' ')
              c++, i++;
          if (i == len) {
              fimp->addParameter(Identifier(param));
              params++;
              break;
          } else if (*c == ',') {
              fimp->addParameter(Identifier(param));
              params++;
              c++, i++;
              continue;
          } // else error
      }
      return throwError(exec, SyntaxError, "Syntax error in parameter list");
  }
  
  List consArgs;

  JSObject *objCons = exec->lexicalInterpreter()->builtinObject();
  JSObject *prototype = objCons->construct(exec,List::empty());
  prototype->put(exec, constructorPropertyName, fimp, DontEnum|DontDelete|ReadOnly);
  fimp->put(exec, prototypePropertyName, prototype, DontEnum|DontDelete|ReadOnly);
  return fimp;
}

// ECMA 15.3.2 The Function Constructor
JSObject* FunctionObjectImp::construct(ExecState* exec, const List& args)
{
  return construct(exec, args, "anonymous", UString(), 0);
}

// ECMA 15.3.1 The Function Constructor Called as a Function
JSValue* FunctionObjectImp::callAsFunction(ExecState* exec, JSObject* /*thisObj*/, const List &args)
{
  return construct(exec, args);
}

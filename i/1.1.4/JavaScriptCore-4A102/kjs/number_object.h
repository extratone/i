// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
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

#ifndef NUMBER_OBJECT_H_
#define NUMBER_OBJECT_H_

#include "function_object.h"

namespace KJS {

  class NumberInstance : public JSObject {
  public:
    NumberInstance(JSObject *proto);

    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;
  };

  /**
   * @internal
   *
   * The initial value of Number.prototype (and thus all objects created
   * with the Number constructor
   */
  class NumberPrototype : public NumberInstance {
  public:
    NumberPrototype(ExecState *exec,
                       ObjectPrototype *objProto,
                       FunctionPrototype *funcProto);
  };

  /**
   * @internal
   *
   * Class to implement all methods that are properties of the
   * Number.prototype object
   */
  class NumberProtoFunc : public InternalFunctionImp {
  public:
    NumberProtoFunc(ExecState*, FunctionPrototype*, int i, int len, const Identifier&);

    virtual JSValue *callAsFunction(ExecState *exec, JSObject *thisObj, const List &args);

    enum { ToString, ToLocaleString, ValueOf, ToFixed, ToExponential, ToPrecision };
  private:
    int id;
  };

  /**
   * @internal
   *
   * The initial value of the the global variable's "Number" property
   */
  class NumberObjectImp : public InternalFunctionImp {
  public:
    NumberObjectImp(ExecState *exec,
                    FunctionPrototype *funcProto,
                    NumberPrototype *numberProto);

    virtual bool implementsConstruct() const;
    virtual JSObject *construct(ExecState *exec, const List &args);

    virtual JSValue *callAsFunction(ExecState *exec, JSObject *thisObj, const List &args);

    bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;

    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;
    enum { NaNValue, NegInfinity, PosInfinity, MaxValue, MinValue };

    Completion execute(const List &);
    JSObject *construct(const List &);
  };

} // namespace

#endif

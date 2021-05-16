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

#ifndef _OBJECT_OBJECT_H_
#define _OBJECT_OBJECT_H_

#include "internal.h"

namespace KJS {

  class FunctionPrototype;

  /**
   * @internal
   *
   * The initial value of Object.prototype (and thus all objects created
   * with the Object constructor
   */
  class ObjectPrototype : public JSObject {
  public:
    ObjectPrototype(ExecState *exec, FunctionPrototype *funcProto);
  };

  /**
   * @internal
   *
   * Class to implement all methods that are properties of the
   * Object.prototype object
   */
  class ObjectProtoFunc : public InternalFunctionImp {
  public:
    ObjectProtoFunc(ExecState* exec, FunctionPrototype* funcProto, int i, int len, const Identifier&);

    virtual JSValue *callAsFunction(ExecState *, JSObject *, const List &args);

    enum { ToString, ToLocaleString, ValueOf, HasOwnProperty, IsPrototypeOf, PropertyIsEnumerable,
           DefineGetter, DefineSetter, LookupGetter, LookupSetter };
  private:
    int id;
  };

  /**
   * @internal
   *
   * The initial value of the the global variable's "Object" property
   */
  class ObjectObjectImp : public InternalFunctionImp {
  public:

    ObjectObjectImp(ExecState *exec,
                    ObjectPrototype *objProto,
                    FunctionPrototype *funcProto);

    virtual bool implementsConstruct() const;
    virtual JSObject *construct(ExecState *, const List &args);
    virtual JSValue *callAsFunction(ExecState *, JSObject *, const List &args);
  };

} // namespace

#endif

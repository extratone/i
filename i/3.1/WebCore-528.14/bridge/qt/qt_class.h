/*
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#ifndef BINDINGS_QT_CLASS_H_
#define BINDINGS_QT_CLASS_H_

#include "runtime.h"

#include "qglobal.h"

QT_BEGIN_NAMESPACE
class QObject;
class QMetaObject;
QT_END_NAMESPACE

namespace JSC {
namespace Bindings {


class QtClass : public Class {
protected:
    QtClass(const QMetaObject*);

public:
    static QtClass* classForObject(QObject*);
    virtual ~QtClass();

    virtual const char* name() const;
    virtual MethodList methodsNamed(const Identifier&, Instance*) const;
    virtual Field* fieldNamed(const Identifier&, Instance*) const;

    virtual JSValuePtr fallbackObject(ExecState*, Instance*, const Identifier&);

private:
    QtClass(const QtClass&); // prohibit copying
    QtClass& operator=(const QtClass&); // prohibit assignment

    const QMetaObject* m_metaObject;
};

} // namespace Bindings
} // namespace JSC

#endif

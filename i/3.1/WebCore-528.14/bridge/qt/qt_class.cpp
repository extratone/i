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

#include "config.h"
#include "Identifier.h"

#include "qt_class.h"
#include "qt_instance.h"
#include "qt_runtime.h"

#include <qmetaobject.h>
#include <qdebug.h>

namespace JSC {
namespace Bindings {

QtClass::QtClass(const QMetaObject* mo)
    : m_metaObject(mo)
{
}

QtClass::~QtClass()
{
}

typedef HashMap<const QMetaObject*, QtClass*> ClassesByMetaObject;
static ClassesByMetaObject* classesByMetaObject = 0;

QtClass* QtClass::classForObject(QObject* o)
{
    if (!classesByMetaObject)
        classesByMetaObject = new ClassesByMetaObject;

    const QMetaObject* mo = o->metaObject();
    QtClass* aClass = classesByMetaObject->get(mo);
    if (!aClass) {
        aClass = new QtClass(mo);
        classesByMetaObject->set(mo, aClass);
    }

    return aClass;
}

const char* QtClass::name() const
{
    return m_metaObject->className();
}

// We use this to get at signals (so we can return a proper function object,
// and not get wrapped in RuntimeMethod). Also, use this for methods,
// so we can cache the object and return the same object for the same
// identifier.
JSValuePtr QtClass::fallbackObject(ExecState* exec, Instance* inst, const Identifier& identifier)
{
    QtInstance* qtinst = static_cast<QtInstance*>(inst);

    QByteArray name(identifier.ascii());

    // First see if we have a cache hit
    JSObject* val = qtinst->m_methods.value(name);
    if (val)
        return val;

    // Nope, create an entry
    QByteArray normal = QMetaObject::normalizedSignature(name.constData());

    // See if there is an exact match
    int index = -1;
    if (normal.contains('(') && (index = m_metaObject->indexOfMethod(normal)) != -1) {
        QMetaMethod m = m_metaObject->method(index);
        if (m.access() != QMetaMethod::Private) {
            QtRuntimeMetaMethod* val = new (exec) QtRuntimeMetaMethod(exec, identifier, static_cast<QtInstance*>(inst), index, normal, false);
            qtinst->m_methods.insert(name, val);
            return val;
        }
    }

    // Nope.. try a basename match
    int count = m_metaObject->methodCount();
    for (index = count - 1; index >= 0; --index) {
        const QMetaMethod m = m_metaObject->method(index);
        if (m.access() == QMetaMethod::Private)
            continue;

        QByteArray signature = m.signature();
        signature.truncate(signature.indexOf('('));

        if (normal == signature) {
            QtRuntimeMetaMethod* val = new (exec) QtRuntimeMetaMethod(exec, identifier, static_cast<QtInstance*>(inst), index, normal, false);
            qtinst->m_methods.insert(name, val);
            return val;
        }
    }

    return jsUndefined();
}

// This functionality is handled by the fallback case above...
MethodList QtClass::methodsNamed(const Identifier&, Instance*) const
{
    return MethodList();
}

// ### we may end up with a different search order than QtScript by not
// folding this code into the fallbackMethod above, but Fields propagate out
// of the binding code
Field* QtClass::fieldNamed(const Identifier& identifier, Instance* instance) const
{
    // Check static properties first
    QtInstance* qtinst = static_cast<QtInstance*>(instance);

    QObject* obj = qtinst->getObject();
    UString ustring = identifier.ustring();
    QString objName(QString::fromUtf16((const ushort*)ustring.rep()->data(),ustring.size()));
    QByteArray ba = objName.toAscii();

    // First check for a cached field
    QtField* f = qtinst->m_fields.value(objName);

    if (obj) {
        if (f) {
            // We only cache real metaproperties, but we do store the
            // other types so we can delete them later
            if (f->fieldType() == QtField::MetaProperty)
                return f;
            else if (f->fieldType() == QtField::DynamicProperty) {
                if (obj->dynamicPropertyNames().indexOf(ba) >= 0)
                    return f;
                else {
                    // Dynamic property that disappeared
                    qtinst->m_fields.remove(objName);
                    delete f;
                }
            } else {
                QList<QObject*> children = obj->children();
                for (int index = 0; index < children.count(); ++index) {
                    QObject *child = children.at(index);
                    if (child->objectName() == objName)
                        return f;
                }

                // Didn't find it, delete it from the cache
                qtinst->m_fields.remove(objName);
                delete f;
            }
        }

        int index = m_metaObject->indexOfProperty(identifier.ascii());
        if (index >= 0) {
            QMetaProperty prop = m_metaObject->property(index);

            if (prop.isScriptable(obj)) {
                f = new QtField(prop);
                qtinst->m_fields.insert(objName, f);
                return f;
            }
        }

        // Dynamic properties
        index = obj->dynamicPropertyNames().indexOf(ba);
        if (index >= 0) {
            f = new QtField(ba);
            qtinst->m_fields.insert(objName, f);
            return f;
        }

        // Child objects

        QList<QObject*> children = obj->children();
        for (index = 0; index < children.count(); ++index) {
            QObject *child = children.at(index);
            if (child->objectName() == objName) {
                f = new QtField(child);
                qtinst->m_fields.insert(objName, f);
                return f;
            }
        }

        // Nothing named this
        return 0;
    } else {
        QByteArray ba(identifier.ascii());
        // For compatibility with qtscript, cached methods don't cause
        // errors until they are accessed, so don't blindly create an error
        // here.
        if (qtinst->m_methods.contains(ba))
            return 0;

        // deleted qobject, but can't throw an error from here (no exec)
        // create a fake QtField that will throw upon access
        if (!f) {
            f = new QtField(ba);
            qtinst->m_fields.insert(objName, f);
        }
        return f;
    }
}

}
}


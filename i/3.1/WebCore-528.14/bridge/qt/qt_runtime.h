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

#ifndef BINDINGS_QT_RUNTIME_H_
#define BINDINGS_QT_RUNTIME_H_

#include "Completion.h"
#include "Protect.h"
#include "runtime.h"
#include "runtime_method.h"

#include <qbytearray.h>
#include <qmetaobject.h>
#include <qpointer.h>
#include <qvariant.h>

namespace JSC {
namespace Bindings {

class QtInstance;

class QtField : public Field {
public:

    typedef enum {
        MetaProperty,
        DynamicProperty,
        ChildObject
    } QtFieldType;

    QtField(const QMetaProperty &p)
        : m_type(MetaProperty), m_property(p)
        {}

    QtField(const QByteArray &b)
        : m_type(DynamicProperty), m_dynamicProperty(b)
        {}

    QtField(QObject *child)
        : m_type(ChildObject), m_childObject(child)
        {}

    virtual JSValuePtr valueFromInstance(ExecState*, const Instance*) const;
    virtual void setValueToInstance(ExecState*, const Instance*, JSValuePtr) const;
    virtual const char* name() const;
    QtFieldType fieldType() const {return m_type;}
private:
    QtFieldType m_type;
    QByteArray m_dynamicProperty;
    QMetaProperty m_property;
    QPointer<QObject> m_childObject;
};


class QtMethod : public Method
{
public:
    QtMethod(const QMetaObject *mo, int i, const QByteArray &ident, int numParameters)
        : m_metaObject(mo),
          m_index(i),
          m_identifier(ident),
          m_nParams(numParameters)
        { }

    virtual const char* name() const { return m_identifier.constData(); }
    virtual int numParameters() const { return m_nParams; }

private:
    friend class QtInstance;
    const QMetaObject *m_metaObject;
    int m_index;
    QByteArray m_identifier;
    int m_nParams;
};


template <typename T> class QtArray : public Array
{
public:
    QtArray(QList<T> list, QMetaType::Type type, PassRefPtr<RootObject>);
    virtual ~QtArray();

    RootObject* rootObject() const;

    virtual void setValueAt(ExecState*, unsigned index, JSValuePtr) const;
    virtual JSValuePtr valueAt(ExecState*, unsigned index) const;
    virtual unsigned int getLength() const {return m_length;}

private:
    mutable QList<T> m_list; // setValueAt is const!
    unsigned int m_length;
    QMetaType::Type m_type;
};

// Based on RuntimeMethod

// Extra data classes (to avoid the CELL_SIZE limit on JS objects)

class QtRuntimeMethodData {
    public:
        virtual ~QtRuntimeMethodData();
        RefPtr<QtInstance> m_instance;
};

class QtRuntimeConnectionMethod;
class QtRuntimeMetaMethodData : public QtRuntimeMethodData {
    public:
        ~QtRuntimeMetaMethodData();
        QByteArray m_signature;
        bool m_allowPrivate;
        int m_index;
        QtRuntimeConnectionMethod *m_connect;
        QtRuntimeConnectionMethod *m_disconnect;
};

class QtRuntimeConnectionMethodData : public QtRuntimeMethodData {
    public:
        ~QtRuntimeConnectionMethodData();
        QByteArray m_signature;
        int m_index;
        bool m_isConnect;
};

// Common base class (doesn't really do anything interesting)
class QtRuntimeMethod : public InternalFunction {
public:
    virtual ~QtRuntimeMethod();

    static const ClassInfo s_info;

    static FunctionPrototype* createPrototype(ExecState* exec)
    {
        return exec->lexicalGlobalObject()->functionPrototype();
    }

    static PassRefPtr<Structure> createStructure(JSValuePtr prototype)
    {
        return Structure::create(prototype, TypeInfo(ObjectType));
    }

protected:
    QtRuntimeMethodData *d_func() const {return d_ptr;}
    QtRuntimeMethod(QtRuntimeMethodData *dd, ExecState *exec, const Identifier &n, PassRefPtr<QtInstance> inst);
    QtRuntimeMethodData *d_ptr;
};

class QtRuntimeMetaMethod : public QtRuntimeMethod
{
public:
    QtRuntimeMetaMethod(ExecState *exec, const Identifier &n, PassRefPtr<QtInstance> inst, int index, const QByteArray& signature, bool allowPrivate);

    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);

    virtual void mark();

protected:
    QtRuntimeMetaMethodData* d_func() const {return reinterpret_cast<QtRuntimeMetaMethodData*>(d_ptr);}

private:
    virtual CallType getCallData(CallData&);
    static JSValuePtr call(ExecState* exec, JSObject* functionObject, JSValuePtr thisValue, const ArgList& args);
    static JSValuePtr lengthGetter(ExecState*, const Identifier&, const PropertySlot&);
    static JSValuePtr connectGetter(ExecState*, const Identifier&, const PropertySlot&);
    static JSValuePtr disconnectGetter(ExecState*, const Identifier&, const PropertySlot&);
};

class QtConnectionObject;
class QtRuntimeConnectionMethod : public QtRuntimeMethod
{
public:
    QtRuntimeConnectionMethod(ExecState *exec, const Identifier &n, bool isConnect, PassRefPtr<QtInstance> inst, int index, const QByteArray& signature );

    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);

protected:
    QtRuntimeConnectionMethodData* d_func() const {return reinterpret_cast<QtRuntimeConnectionMethodData*>(d_ptr);}

private:
    virtual CallType getCallData(CallData&);
    static JSValuePtr call(ExecState* exec, JSObject* functionObject, JSValuePtr thisValue, const ArgList& args);
    static JSValuePtr lengthGetter(ExecState*, const Identifier&, const PropertySlot&);
    static QMultiMap<QObject *, QtConnectionObject *> connections;
    friend class QtConnectionObject;
};

class QtConnectionObject: public QObject
{
public:
    QtConnectionObject(PassRefPtr<QtInstance> instance, int signalIndex, JSObject* thisObject, JSObject* funcObject);
    ~QtConnectionObject();

    static const QMetaObject staticMetaObject;
    virtual const QMetaObject *metaObject() const;
    virtual void *qt_metacast(const char *);
    virtual int qt_metacall(QMetaObject::Call, int, void **argv);

    bool match(QObject *sender, int signalIndex, JSObject* thisObject, JSObject *funcObject);

    // actual slot:
    void execute(void **argv);

private:
    RefPtr<QtInstance> m_instance;
    int m_signalIndex;
    QObject* m_originalObject; // only used as a key, not dereferenced
    ProtectedPtr<JSObject> m_thisObject;
    ProtectedPtr<JSObject> m_funcObject;
};

QVariant convertValueToQVariant(ExecState* exec, JSValuePtr value, QMetaType::Type hint, int *distance);

} // namespace Bindings
} // namespace JSC

#endif

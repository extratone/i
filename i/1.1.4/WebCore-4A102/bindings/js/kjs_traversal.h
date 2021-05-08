// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef KJS_TRAVERSAL_H_
#define KJS_TRAVERSAL_H_

#include "NodeFilter.h"
#include "NodeFilterCondition.h"
#include "NodeIterator.h"
#include "TreeWalker.h"
#include "kjs_dom.h"

namespace WebCore {
    class NodeFilter;
    class NodeIterator;
    class TreeWalker;
}

namespace KJS {

  class DOMNodeIterator : public DOMObject {
  public:
    DOMNodeIterator(ExecState*, WebCore::NodeIterator*);
    ~DOMNodeIterator();
    virtual void mark();
    virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState*, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Filter, Root, WhatToShow, ExpandEntityReferences, ReferenceNode, PointerBeforeReferenceNode,
           NextNode, PreviousNode, Detach };
    WebCore::NodeIterator* impl() const { return m_impl.get(); }
  private:
    RefPtr<WebCore::NodeIterator> m_impl;
  };

  class DOMNodeFilter : public DOMObject {
  public:
    DOMNodeFilter(ExecState*, WebCore::NodeFilter*);
    ~DOMNodeFilter();
    virtual void mark();
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    WebCore::NodeFilter* impl() const { return m_impl.get(); }
    enum { AcceptNode };
  private:
    RefPtr<WebCore::NodeFilter> m_impl;
  };

  class DOMTreeWalker : public DOMObject {
  public:
    DOMTreeWalker(ExecState*, WebCore::TreeWalker*);
    ~DOMTreeWalker();
    virtual void mark();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue* getValueProperty(ExecState*, int token) const;
    virtual void put(ExecState*, const Identifier& propertyName, JSValue*, int attr = None);
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Root, WhatToShow, Filter, ExpandEntityReferences, CurrentNode,
           ParentNode, FirstChild, LastChild, PreviousSibling, NextSibling,
           PreviousNode, NextNode };
    WebCore::TreeWalker* impl() const { return m_impl.get(); }
  private:
    RefPtr<WebCore::TreeWalker> m_impl;
  };

  JSValue* toJS(ExecState*, WebCore::NodeIterator*);
  JSValue* toJS(ExecState*, WebCore::NodeFilter*);
  JSValue* toJS(ExecState*, WebCore::TreeWalker*);

  PassRefPtr<WebCore::NodeFilter> toNodeFilter(JSValue*); // returns 0 if value is not a DOMNodeFilter or JS function 

  class JSNodeFilterCondition : public WebCore::NodeFilterCondition {
  public:
    JSNodeFilterCondition(JSObject* filter);
    virtual short acceptNode(WebCore::Node*) const;
    virtual void mark();
  protected:
    JSObject *filter;
  };

} // namespace

#endif

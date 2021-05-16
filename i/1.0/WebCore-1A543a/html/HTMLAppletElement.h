/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef HTMLAppletElement_H
#define HTMLAppletElement_H

#include "HTMLPlugInElement.h"

#if PLATFORM(MAC)
#include <JavaScriptCore/runtime.h>
#else
namespace KJS { namespace Bindings { class Instance; } }
#endif

namespace WebCore {

class HTMLFormElement;
class HTMLImageLoader;

class HTMLAppletElement : public HTMLPlugInElement
{
public:
    HTMLAppletElement(Document*);
    ~HTMLAppletElement();

    virtual int tagPriority() const { return 1; }

    virtual void parseMappedAttribute(MappedAttribute*);
    
    virtual bool rendererIsNeeded(RenderStyle*);
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
    virtual void closeRenderer();
    virtual void detach();
    
#if PLATFORM(MAC)
    virtual KJS::Bindings::Instance* getInstance() const;
#endif

    String alt() const;
    void setAlt(const String&);

    String archive() const;
    void setArchive(const String&);

    String code() const;
    void setCode(const String&);

    String codeBase() const;
    void setCodeBase(const String&);

    String hspace() const;
    void setHspace(const String&);

    String object() const;
    void setObject(const String&);

    String vspace() const;
    void setVspace(const String&);

    virtual bool allParamsAvailable();
    void setupApplet() const;

    virtual void insertedIntoDocument();
    virtual void removedFromDocument();

private:
    String oldIdAttr;
    bool m_allParamsAvailable;
};

}

#endif

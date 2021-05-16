/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef HTMLTextAreaElement_h
#define HTMLTextAreaElement_h

#include "HTMLFormControlElement.h"

namespace WebCore {

class Selection;

class HTMLTextAreaElement : public HTMLFormControlElementWithState {
public:
    HTMLTextAreaElement(const QualifiedName&, Document*, HTMLFormElement* = 0);

    virtual bool checkDTD(const Node* newChild) { return newChild->isTextNode(); }

    int cols() const { return m_cols; }
    int rows() const { return m_rows; }

    bool shouldWrapText() const { return m_wrap != NoWrap; }

    virtual bool isEnumeratable() const { return true; }

    virtual const AtomicString& type() const;

    virtual bool saveState(String& value) const;
    virtual void restoreState(const String&);

    bool readOnly() const { return isReadOnlyControl(); }

    virtual bool isTextControl() const { return true; }

    int selectionStart();
    int selectionEnd();

    void setSelectionStart(int);
    void setSelectionEnd(int);

    void select();
    void setSelectionRange(int, int);

    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);
    virtual void parseMappedAttribute(MappedAttribute*);
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
    virtual bool appendFormData(FormDataList&, bool);
    virtual void reset();
    virtual void defaultEventHandler(Event*);
    virtual bool isMouseFocusable() const;
    virtual bool isKeyboardFocusable(KeyboardEvent*) const;
    virtual void updateFocusAppearance(bool restorePreviousSelection);

    String value() const;
    void setValue(const String&);
    String defaultValue() const;
    void setDefaultValue(const String&);
    
    void rendererWillBeDestroyed();
    
    virtual void accessKeyAction(bool sendToAnyElement);
    
    const AtomicString& accessKey() const;
    void setAccessKey(const String&);

    void setCols(int);
    void setRows(int);
    
    void cacheSelection(int s, int e) { m_cachedSelectionStart = s; m_cachedSelectionEnd = e; };
    Selection selection() const;

    virtual bool willRespondToMouseClickEvents();

    virtual bool shouldUseInputMethod() const;

private:
    enum WrapMethod { NoWrap, SoftWrap, HardWrap };

    void updateValue() const;

    int m_rows;
    int m_cols;
    WrapMethod m_wrap;
    mutable String m_value;
    int m_cachedSelectionStart;
    int m_cachedSelectionEnd;
};

} //namespace

#endif

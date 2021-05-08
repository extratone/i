/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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

#ifndef HTML_HTMLSelectElementImpl_H
#define HTML_HTMLSelectElementImpl_H

#include "Event.h"
#include "HTMLGenericFormElement.h"
#include "HTMLCollection.h"
#include "RenderStyle.h"
#include <wtf/Vector.h>

namespace WebCore {

class HTMLOptionElement;
class HTMLOptionsCollection;
class KeyboardEvent;

class HTMLSelectElement : public HTMLGenericFormElement {

public:
    HTMLSelectElement(Document*, HTMLFormElement* = 0);
    HTMLSelectElement(const QualifiedName& tagName, Document*, HTMLFormElement* = 0);
    ~HTMLSelectElement();

    virtual int tagPriority() const { return 6; }
    virtual bool checkDTD(const Node* newChild);

    virtual const AtomicString& type() const;

    virtual bool isKeyboardFocusable() const;
    virtual bool isMouseFocusable() const;

    virtual void recalcStyle(StyleChange);

    virtual void dispatchBlurEvent();

    int selectedIndex() const;
    void setSelectedIndex(int index, bool deselect = true, bool fireOnChange = false);
    void notifyOptionSelected(HTMLOptionElement* selectedOption, bool selected);
    int lastSelectedListIndex() const;

    virtual bool isEnumeratable() const { return true; }

    int length() const;

    int minWidth() const { return m_minwidth; }

    int size() const { return m_size; }

    bool multiple() const { return m_multiple; }

    void add(HTMLElement* element, HTMLElement* before, ExceptionCode&);
    void remove(int index);

    String value();
    void setValue(const String&);
    
    PassRefPtr<HTMLOptionsCollection> options();

    virtual String stateValue() const;
    virtual void restoreState(const String&);

    virtual bool insertBefore(PassRefPtr<Node> newChild, Node* refChild, ExceptionCode&);
    virtual bool replaceChild(PassRefPtr<Node> newChild, Node* oldChild, ExceptionCode&);
    virtual bool removeChild(Node* child, ExceptionCode&);
    virtual bool appendChild(PassRefPtr<Node> newChild, ExceptionCode&);
    virtual ContainerNode* addChild(PassRefPtr<Node>);

    virtual void childrenChanged();

    virtual void parseMappedAttribute(MappedAttribute*);

    virtual RenderObject* createRenderer(RenderArena*, RenderStyle *);
    virtual bool appendFormData(FormDataList&, bool);

    // get the actual listbox index of the optionIndexth option
    int optionToListIndex(int optionIndex) const;
    // reverse of optionToListIndex - get optionIndex from listboxIndex
    int listToOptionIndex(int listIndex) const;

    void setRecalcListItems();

    const Vector<HTMLElement*>& listItems() const
    {
        if (m_recalcListItems)
            recalcListItems();
        return m_listItems;
    }
    virtual void reset();

    virtual void defaultEventHandler(Event*);
    virtual void accessKeyAction(bool sendToAnyElement);

    void setMultiple(bool);

    void setSize(int);

    void setOption(unsigned index, HTMLOptionElement*, ExceptionCode&);
    void setLength(unsigned, ExceptionCode&);

    virtual Node* namedItem(const String& name, bool caseSensitive = true);

    HTMLCollection::CollectionInfo* collectionInfo() { return &m_collectionInfo; }
    
    void setActiveSelectionAnchorIndex(int index);
    void setActiveSelectionEndIndex(int index) { m_activeSelectionEndIndex = index; }
    void updateListBoxSelection(bool deselectOtherOptions);
    void listBoxOnChange();

    virtual bool willRespondToMouseClickEvents();

private:
    void recalcListItems() const;
    void deselectItems(HTMLOptionElement* excludeElement = 0);
    bool usesMenuList() const { return true; }
    int nextSelectableListIndex(int startIndex);
    int previousSelectableListIndex(int startIndex);
    void menuListDefaultEventHandler(Event*);
    void listBoxDefaultEventHandler(Event*);
    void typeAheadFind(KeyboardEvent*);

    mutable Vector<HTMLElement*> m_listItems;
    Vector<bool> m_cachedStateForActiveSelection;
    Vector<bool> m_lastOnChangeSelection;
    int m_minwidth;
    int m_size;
    bool m_multiple;
    mutable bool m_recalcListItems;
    int m_lastOnChangeIndex;
    
    int m_activeSelectionAnchorIndex;
    int m_activeSelectionEndIndex;  
    bool m_activeSelectionState;

    // Instance variables for type-ahead find
    UChar m_repeatingChar;
    DOMTimeStamp m_lastCharTime;
    String m_typedString;

    HTMLCollection::CollectionInfo m_collectionInfo;
};

} // namespace

#endif

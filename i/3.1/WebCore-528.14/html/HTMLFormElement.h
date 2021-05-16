/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef HTMLFormElement_h
#define HTMLFormElement_h

#include "FormDataBuilder.h"
#include "HTMLCollection.h" 
#include "HTMLElement.h"

#include <wtf/OwnPtr.h>

namespace WebCore {

class Event;
class FormData;
class HTMLFormControlElement;
class HTMLImageElement;
class HTMLInputElement;
class HTMLFormCollection;
class TextEncoding;

class HTMLFormElement : public HTMLElement { 
public:
    HTMLFormElement(const QualifiedName&, Document*);
    virtual ~HTMLFormElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 3; }

    virtual void attach();
    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
 
    virtual void handleLocalEvents(Event*, bool useCapture);
     
    PassRefPtr<HTMLCollection> elements();
    void getNamedElements(const AtomicString&, Vector<RefPtr<Node> >&);
    
    unsigned length() const;
    Node* item(unsigned index);

    String enctype() const { return m_formDataBuilder.encodingType(); }
    void setEnctype(const String&);

    String encoding() const { return m_formDataBuilder.encodingType(); }
    void setEncoding(const String& value) { setEnctype(value); }

    bool autoComplete() const { return m_autocomplete; }
    
    bool autocorrect() const;
    void setAutocorrect(bool);
    
    bool autocapitalize() const;
    void setAutocapitalize(bool);
    
    virtual void parseMappedAttribute(MappedAttribute*);

    void registerFormElement(HTMLFormControlElement*);
    void removeFormElement(HTMLFormControlElement*);
    void registerImgElement(HTMLImageElement*);
    void removeImgElement(HTMLImageElement*);

    bool prepareSubmit(Event*);
    void submit(Event* = 0, bool activateSubmitButton = false, bool lockHistory = false, bool lockBackForwardList = false);
    void reset();

    // Used to indicate a malformed state to keep from applying the bottom margin of the form.
    void setMalformed(bool malformed) { m_malformed = malformed; }
    bool isMalformed() const { return m_malformed; }

    virtual bool isURLAttribute(Attribute*) const;
    
    void submitClick(Event*);
    bool formWouldHaveSecureSubmission(const String& url);

    String name() const;
    void setName(const String&);

    String acceptCharset() const { return m_formDataBuilder.acceptCharset(); }
    void setAcceptCharset(const String&);

    String action() const;
    void setAction(const String&);

    String method() const;
    void setMethod(const String&);

    virtual String target() const;
    void setTarget(const String&);
    
    PassRefPtr<HTMLFormControlElement> elementForAlias(const AtomicString&);
    void addElementAlias(HTMLFormControlElement*, const AtomicString& alias);

    // FIXME: Change this to be private after getting rid of all the clients.
    Vector<HTMLFormControlElement*> formElements;

    class CheckedRadioButtons {
    public:
        void addButton(HTMLFormControlElement*);
        void removeButton(HTMLFormControlElement*);
        HTMLInputElement* checkedButtonForGroup(const AtomicString& name) const;

    private:
        typedef HashMap<AtomicStringImpl*, HTMLInputElement*> NameToInputMap;
        OwnPtr<NameToInputMap> m_nameToCheckedRadioButtonMap;
    };
    
    CheckedRadioButtons& checkedRadioButtons() { return m_checkedRadioButtons; }
    
    virtual void documentDidBecomeActive();

protected:
    virtual void willMoveToNewOwnerDocument();
    virtual void didMoveToNewOwnerDocument();

private:
    bool isMailtoForm() const;
    TextEncoding dataEncoding() const;
    PassRefPtr<FormData> createFormData(const CString& boundary);
    unsigned formElementIndex(HTMLFormControlElement*);

    friend class HTMLFormCollection;

    typedef HashMap<RefPtr<AtomicStringImpl>, RefPtr<HTMLFormControlElement> > AliasMap;

    FormDataBuilder m_formDataBuilder;
    AliasMap* m_elementAliases;
    HTMLCollection::CollectionInfo* collectionInfo;

    CheckedRadioButtons m_checkedRadioButtons;
    
    Vector<HTMLImageElement*> imgElements;
    String m_url;
    String m_target;
    bool m_autocomplete : 1;
    bool m_insubmit : 1;
    bool m_doingsubmit : 1;
    bool m_inreset : 1;
    bool m_malformed : 1;
    AtomicString m_name;
};

} // namespace WebCore

#endif // HTMLFormElement_h

/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
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

#include "config.h"
#include "HTMLInputElement.h"

#include "ChromeClient.h"
#include "CSSPropertyNames.h"
#include "Document.h"
#include "Editor.h"
#include "Event.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "File.h"
#include "FileList.h"
#include "FocusController.h"
#include "FormDataList.h"
#include "Frame.h"
#include "HTMLFormElement.h"
#include "HTMLImageLoader.h"
#include "HTMLNames.h"
#include "KeyboardEvent.h"
#include "LocalizedStrings.h"
#include "MouseEvent.h"
#include "Page.h"
#include "RenderButton.h"
#include "RenderFileUploadControl.h"
#include "RenderImage.h"
#include "RenderSlider.h"
#include "RenderText.h"
#include "RenderTextControlSingleLine.h"
#include "RenderTheme.h"
#include "TextEvent.h"
#include <wtf/StdLibExtras.h>

using namespace std;

namespace WebCore {

using namespace HTMLNames;

const int maxSavedResults = 256;

HTMLInputElement::HTMLInputElement(const QualifiedName& tagName, Document* doc, HTMLFormElement* f)
    : HTMLFormControlElementWithState(tagName, doc, f)
    , m_data(this, this)
    , m_xPos(0)
    , m_yPos(0)
    , m_maxResults(-1)
    , m_type(TEXT)
    , m_checked(false)
    , m_defaultChecked(false)
    , m_useDefaultChecked(true)
    , m_indeterminate(false)
    , m_haveType(false)
    , m_activeSubmit(false)
    , m_autocomplete(Uninitialized)
    , m_autofilled(false)
    , m_inited(false)
{
    ASSERT(hasTagName(inputTag) || hasTagName(isindexTag));
}

HTMLInputElement::~HTMLInputElement()
{
    if (needsActivationCallback())
        document()->unregisterForDocumentActivationCallbacks(this);

    document()->checkedRadioButtons().removeButton(this);

    // Need to remove this from the form while it is still an HTMLInputElement,
    // so can't wait for the base class's destructor to do it.
    removeFromForm();
}

const AtomicString& HTMLInputElement::name() const
{
    return m_data.name();
}

bool HTMLInputElement::autoComplete() const
{
    if (m_autocomplete != Uninitialized)
        return m_autocomplete == On;
    
    // Assuming we're still in a Form, respect the Form's setting
    if (HTMLFormElement* form = this->form())
        return form->autoComplete();
    
    // The default is true
    return true;
}


static inline HTMLFormElement::CheckedRadioButtons& checkedRadioButtons(const HTMLInputElement *element)
{
    if (HTMLFormElement* form = element->form())
        return form->checkedRadioButtons();
    
    return element->document()->checkedRadioButtons();
}

bool HTMLInputElement::isKeyboardFocusable(KeyboardEvent* event) const
{
    if (readOnly())
        return false;
    // If text fields can be focused, then they should always be keyboard focusable
    if (isTextField())
        return HTMLFormControlElementWithState::isFocusable();
        
    // If the base class says we can't be focused, then we can stop now.
    if (!HTMLFormControlElementWithState::isKeyboardFocusable(event))
        return false;

    if (inputType() == RADIO) {

        // Never allow keyboard tabbing to leave you in the same radio group.  Always
        // skip any other elements in the group.
        Node* currentFocusedNode = document()->focusedNode();
        if (currentFocusedNode && currentFocusedNode->hasTagName(inputTag)) {
            HTMLInputElement* focusedInput = static_cast<HTMLInputElement*>(currentFocusedNode);
            if (focusedInput->inputType() == RADIO && focusedInput->form() == form() &&
                focusedInput->name() == name())
                return false;
        }
        
        // Allow keyboard focus if we're checked or if nothing in the group is checked.
        return checked() || !checkedRadioButtons(this).checkedButtonForGroup(name());
    }
    
    return true;
}

bool HTMLInputElement::isMouseFocusable() const
{
    if (isTextField())
        return HTMLFormControlElementWithState::isFocusable();
    return HTMLFormControlElementWithState::isMouseFocusable();
}

void HTMLInputElement::updateFocusAppearance(bool restorePreviousSelection)
{        
    if (isTextField())
        InputElement::updateFocusAppearance(m_data, document(), restorePreviousSelection);
    else
        HTMLFormControlElementWithState::updateFocusAppearance(restorePreviousSelection);
}

void HTMLInputElement::aboutToUnload()
{
    InputElement::aboutToUnload(m_data, document());
}

bool HTMLInputElement::shouldUseInputMethod() const
{
    return m_type == TEXT || m_type == SEARCH || m_type == ISINDEX;
}

void HTMLInputElement::dispatchFocusEvent()
{
    InputElement::dispatchFocusEvent(m_data, document());

    if (isTextField())
        m_autofilled = false;

    HTMLFormControlElementWithState::dispatchFocusEvent();
}

void HTMLInputElement::dispatchBlurEvent()
{
    InputElement::dispatchBlurEvent(m_data, document());
    HTMLFormControlElementWithState::dispatchBlurEvent();
}

void HTMLInputElement::setType(const String& t)
{
    if (t.isEmpty()) {
        int exccode;
        removeAttribute(typeAttr, exccode);
    } else
        setAttribute(typeAttr, t);
}

void HTMLInputElement::setInputType(const String& t)
{
    InputType newType;
    
    if (equalIgnoringCase(t, "password"))
        newType = PASSWORD;
    else if (equalIgnoringCase(t, "checkbox"))
        newType = CHECKBOX;
    else if (equalIgnoringCase(t, "radio"))
        newType = RADIO;
    else if (equalIgnoringCase(t, "submit"))
        newType = SUBMIT;
    else if (equalIgnoringCase(t, "reset"))
        newType = RESET;
    else if (equalIgnoringCase(t, "file"))
        newType = FILE;
    else if (equalIgnoringCase(t, "hidden"))
        newType = HIDDEN;
    else if (equalIgnoringCase(t, "image"))
        newType = IMAGE;
    else if (equalIgnoringCase(t, "button"))
        newType = BUTTON;
    else if (equalIgnoringCase(t, "khtml_isindex"))
        newType = ISINDEX;
    else if (equalIgnoringCase(t, "search"))
        newType = SEARCH;
    else if (equalIgnoringCase(t, "email"))
        newType = EMAIL;
    else if (equalIgnoringCase(t, "number"))
        newType = NUMBER;
    else if (equalIgnoringCase(t, "tel"))
        newType = TELEPHONE;
    else if (equalIgnoringCase(t, "url"))
        newType = URL;
    else
        newType = TEXT;

    // IMPORTANT: Don't allow the type to be changed to FILE after the first
    // type change, otherwise a JavaScript programmer would be able to set a text
    // field's value to something like /etc/passwd and then change it to a file field.
    if (inputType() != newType) {
        if (newType == FILE)
            setDisabled(true);
        if (newType == FILE && m_haveType)
            // Set the attribute back to the old value.
            // Useful in case we were called from inside parseMappedAttribute.
            setAttribute(typeAttr, type());
        else {
            checkedRadioButtons(this).removeButton(this);

            if (newType == FILE && !m_fileList)
                m_fileList = FileList::create();

            bool wasAttached = attached();
            if (wasAttached)
                detach();

            bool didStoreValue = storesValueSeparateFromAttribute();
            bool wasPasswordField = inputType() == PASSWORD;
            bool didRespectHeightAndWidth = respectHeightAndWidthAttrs();
            m_type = newType;
            bool willStoreValue = storesValueSeparateFromAttribute();
            bool isPasswordField = inputType() == PASSWORD;
            bool willRespectHeightAndWidth = respectHeightAndWidthAttrs();

            if (didStoreValue && !willStoreValue && !m_data.value().isNull()) {
                setAttribute(valueAttr, m_data.value());
                m_data.setValue(String());
            }
            if (!didStoreValue && willStoreValue)
                m_data.setValue(constrainValue(getAttribute(valueAttr)));
            else
                InputElement::updateValueIfNeeded(m_data);

            if (wasPasswordField && !isPasswordField)
                unregisterForActivationCallbackIfNeeded();
            else if (!wasPasswordField && isPasswordField)
                registerForActivationCallbackIfNeeded();

            if (didRespectHeightAndWidth != willRespectHeightAndWidth) {
                NamedMappedAttrMap* map = mappedAttributes();
                if (Attribute* height = map->getAttributeItem(heightAttr))
                    attributeChanged(height, false);
                if (Attribute* width = map->getAttributeItem(widthAttr))
                    attributeChanged(width, false);
                if (Attribute* align = map->getAttributeItem(alignAttr))
                    attributeChanged(align, false);
            }

            if (wasAttached) {
                attach();
                if (document()->focusedNode() == this)
                    updateFocusAppearance(true);
            }

            checkedRadioButtons(this).addButton(this);
        }

        InputElement::notifyFormStateChanged(m_data, document());
    }
    m_haveType = true;

    if (inputType() != IMAGE && m_imageLoader)
        m_imageLoader.clear();
}

const AtomicString& HTMLInputElement::type() const
{
    // needs to be lowercase according to DOM spec
    switch (inputType()) {
        case BUTTON: {
            DEFINE_STATIC_LOCAL(const AtomicString, button, ("button"));
            return button;
        }
        case CHECKBOX: {
            DEFINE_STATIC_LOCAL(const AtomicString, checkbox, ("checkbox"));
            return checkbox;
        }
        case EMAIL: {
            DEFINE_STATIC_LOCAL(const AtomicString, email, ("email"));
            return email;
        }
        case FILE: {
            DEFINE_STATIC_LOCAL(const AtomicString, file, ("file"));
            return file;
        }
        case HIDDEN: {
            DEFINE_STATIC_LOCAL(const AtomicString, hidden, ("hidden"));
            return hidden;
        }
        case IMAGE: {
            DEFINE_STATIC_LOCAL(const AtomicString, image, ("image"));
            return image;
        }
        case ISINDEX:
            return emptyAtom;
        case NUMBER: {
            DEFINE_STATIC_LOCAL(const AtomicString, number, ("number"));
            return number;
        }
        case PASSWORD: {
            DEFINE_STATIC_LOCAL(const AtomicString, password, ("password"));
            return password;
        }
        case RADIO: {
            DEFINE_STATIC_LOCAL(const AtomicString, radio, ("radio"));
            return radio;
        }
        case RANGE: {
            DEFINE_STATIC_LOCAL(const AtomicString, range, ("range"));
            return range;
        }
        case RESET: {
            DEFINE_STATIC_LOCAL(const AtomicString, reset, ("reset"));
            return reset;
        }
        case SEARCH: {
            DEFINE_STATIC_LOCAL(const AtomicString, search, ("search"));
            return search;
        }
        case SUBMIT: {
            DEFINE_STATIC_LOCAL(const AtomicString, submit, ("submit"));
            return submit;
        }
        case TELEPHONE: {
            DEFINE_STATIC_LOCAL(const AtomicString, telephone, ("tel"));
            return telephone;
        }
        case TEXT: {
            DEFINE_STATIC_LOCAL(const AtomicString, text, ("text"));
            return text;
        }
        case URL: {
            DEFINE_STATIC_LOCAL(const AtomicString, url, ("url"));
            return url;
        }
    }
    return emptyAtom;
}

bool HTMLInputElement::saveState(String& result) const
{
    if (!autoComplete())
        return false;

    switch (inputType()) {
        case BUTTON:
        case EMAIL:
        case FILE:
        case HIDDEN:
        case IMAGE:
        case ISINDEX:
        case NUMBER:
        case RANGE:
        case RESET:
        case SEARCH:
        case SUBMIT:
        case TELEPHONE:
        case TEXT:
        case URL:
            result = value();
            return true;
        case CHECKBOX:
        case RADIO:
            result = checked() ? "on" : "off";
            return true;
        case PASSWORD:
            return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

void HTMLInputElement::restoreState(const String& state)
{
    ASSERT(inputType() != PASSWORD); // should never save/restore password fields
    switch (inputType()) {
        case BUTTON:
        case EMAIL:
        case FILE:
        case HIDDEN:
        case IMAGE:
        case ISINDEX:
        case NUMBER:
        case RANGE:
        case RESET:
        case SEARCH:
        case SUBMIT:
        case TELEPHONE:
        case TEXT:
        case URL:
            setValue(state);
            break;
        case CHECKBOX:
        case RADIO:
            setChecked(state == "on");
            break;
        case PASSWORD:
            break;
    }
}

bool HTMLInputElement::canStartSelection() const
{
    if (!isTextField())
        return false;
    return HTMLFormControlElementWithState::canStartSelection();
}

bool HTMLInputElement::canHaveSelection() const
{
    return isTextField();
}

int HTMLInputElement::selectionStart() const
{
    if (!isTextField())
        return 0;
    if (document()->focusedNode() != this && m_data.cachedSelectionStart() != -1)
        return m_data.cachedSelectionStart();
    if (!renderer())
        return 0;
    return static_cast<RenderTextControl*>(renderer())->selectionStart();
}

int HTMLInputElement::selectionEnd() const
{
    if (!isTextField())
        return 0;
    if (document()->focusedNode() != this && m_data.cachedSelectionEnd() != -1)
        return m_data.cachedSelectionEnd();
    if (!renderer())
        return 0;
    return static_cast<RenderTextControl*>(renderer())->selectionEnd();
}

void HTMLInputElement::setSelectionStart(int start)
{
    if (!isTextField())
        return;
    if (!renderer())
        return;
    static_cast<RenderTextControl*>(renderer())->setSelectionStart(start);
}

void HTMLInputElement::setSelectionEnd(int end)
{
    if (!isTextField())
        return;
    if (!renderer())
        return;
    static_cast<RenderTextControl*>(renderer())->setSelectionEnd(end);
}

void HTMLInputElement::select()
{
    if (!isTextField())
        return;
    if (!renderer())
        return;
    static_cast<RenderTextControl*>(renderer())->select();
}

void HTMLInputElement::setSelectionRange(int start, int end)
{
    InputElement::updateSelectionRange(m_data, start, end);
}

void HTMLInputElement::accessKeyAction(bool sendToAnyElement)
{
    switch (inputType()) {
        case BUTTON:
        case CHECKBOX:
        case FILE:
        case IMAGE:
        case RADIO:
        case RANGE:
        case RESET:
        case SUBMIT:
            focus(false);
            // send the mouse button events iff the caller specified sendToAnyElement
            dispatchSimulatedClick(0, sendToAnyElement);
            break;
        case HIDDEN:
            // a no-op for this type
            break;
        case EMAIL:
        case ISINDEX:
        case NUMBER:
        case PASSWORD:
        case SEARCH:
        case TELEPHONE:
        case TEXT:
        case URL:
            // should never restore previous selection here
            focus(false);
            break;
    }
}

bool HTMLInputElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (((attrName == heightAttr || attrName == widthAttr) && respectHeightAndWidthAttrs()) ||
        attrName == vspaceAttr ||
        attrName == hspaceAttr) {
        result = eUniversal;
        return false;
    } 

    if (attrName == alignAttr) {
        if (inputType() == IMAGE) {
            // Share with <img> since the alignment behavior is the same.
            result = eReplaced;
            return false;
        }
    }

    return HTMLElement::mapToEntry(attrName, result);
}

void HTMLInputElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == nameAttr) {
        checkedRadioButtons(this).removeButton(this);
        m_data.setName(attr->value());
        checkedRadioButtons(this).addButton(this);
    } else if (attr->name() == autocompleteAttr) {
        if (equalIgnoringCase(attr->value(), "off")) {
            m_autocomplete = Off;
            registerForActivationCallbackIfNeeded();
        } else {
            if (m_autocomplete == Off)
                unregisterForActivationCallbackIfNeeded();
            if (attr->isEmpty())
                m_autocomplete = Uninitialized;
            else
                m_autocomplete = On;
        }
    } else if (attr->name() == typeAttr) {
        setInputType(attr->value());
    } else if (attr->name() == valueAttr) {
        // We only need to setChanged if the form is looking at the default value right now.
        if (m_data.value().isNull())
            setChanged();
        setValueMatchesRenderer(false);
    } else if (attr->name() == checkedAttr) {
        m_defaultChecked = !attr->isNull();
        if (m_useDefaultChecked) {
            setChecked(m_defaultChecked);
            m_useDefaultChecked = true;
        }
    } else if (attr->name() == maxlengthAttr)
        InputElement::parseMaxLengthAttribute(m_data, attr);
    else if (attr->name() == sizeAttr)
        InputElement::parseSizeAttribute(m_data, attr);
    else if (attr->name() == altAttr) {
        if (renderer() && inputType() == IMAGE)
            static_cast<RenderImage*>(renderer())->updateAltText();
    } else if (attr->name() == srcAttr) {
        if (renderer() && inputType() == IMAGE) {
            if (!m_imageLoader)
                m_imageLoader.set(new HTMLImageLoader(this));
            m_imageLoader->updateFromElementIgnoringPreviousError();
        }
    } else if (attr->name() == usemapAttr ||
               attr->name() == accesskeyAttr) {
        // FIXME: ignore for the moment
    } else if (attr->name() == vspaceAttr) {
        addCSSLength(attr, CSSPropertyMarginTop, attr->value());
        addCSSLength(attr, CSSPropertyMarginBottom, attr->value());
    } else if (attr->name() == hspaceAttr) {
        addCSSLength(attr, CSSPropertyMarginLeft, attr->value());
        addCSSLength(attr, CSSPropertyMarginRight, attr->value());
    } else if (attr->name() == alignAttr) {
        if (inputType() == IMAGE)
            addHTMLAlignment(attr);
    } else if (attr->name() == widthAttr) {
        if (respectHeightAndWidthAttrs())
            addCSSLength(attr, CSSPropertyWidth, attr->value());
    } else if (attr->name() == heightAttr) {
        if (respectHeightAndWidthAttrs())
            addCSSLength(attr, CSSPropertyHeight, attr->value());
    } else if (attr->name() == onfocusAttr) {
        setInlineEventListenerForTypeAndAttribute(eventNames().focusEvent, attr);
    } else if (attr->name() == onblurAttr) {
        setInlineEventListenerForTypeAndAttribute(eventNames().blurEvent, attr);
    } else if (attr->name() == onselectAttr) {
        setInlineEventListenerForTypeAndAttribute(eventNames().selectEvent, attr);
    } else if (attr->name() == onchangeAttr) {
        setInlineEventListenerForTypeAndAttribute(eventNames().changeEvent, attr);
    } else if (attr->name() == oninputAttr) {
        setInlineEventListenerForTypeAndAttribute(eventNames().inputEvent, attr);
    }
    // Search field and slider attributes all just cause updateFromElement to be called through style
    // recalcing.
    else if (attr->name() == onsearchAttr) {
        setInlineEventListenerForTypeAndAttribute(eventNames().searchEvent, attr);
    } else if (attr->name() == resultsAttr) {
        int oldResults = m_maxResults;
        m_maxResults = !attr->isNull() ? min(attr->value().toInt(), maxSavedResults) : -1;
        // FIXME: Detaching just for maxResults change is not ideal.  We should figure out the right
        // time to relayout for this change.
        if (m_maxResults != oldResults && (m_maxResults <= 0 || oldResults <= 0) && attached()) {
            detach();
            attach();
        }
        setChanged();
    } else if (attr->name() == placeholderAttr) {
        if (isTextField())
            InputElement::updatePlaceholderVisibility(m_data, document(), true);
    } else if (attr->name() == autosaveAttr ||
             attr->name() == incrementalAttr ||
             attr->name() == minAttr ||
             attr->name() == maxAttr ||
             attr->name() == multipleAttr ||
             attr->name() == precisionAttr)
        setChanged();
    else
        HTMLFormControlElementWithState::parseMappedAttribute(attr);
}

bool HTMLInputElement::rendererIsNeeded(RenderStyle *style)
{
    switch (inputType()) {
        case BUTTON:
        case CHECKBOX:
        case EMAIL:
        case FILE:
        case IMAGE:
        case ISINDEX:
        case NUMBER:
        case PASSWORD:
        case RADIO:
        case RANGE:
        case RESET:
        case SEARCH:
        case SUBMIT:
        case TELEPHONE:
        case TEXT:
        case URL:
            return HTMLFormControlElementWithState::rendererIsNeeded(style);
        case HIDDEN:
            return false;
    }
    ASSERT(false);
    return false;
}

RenderObject *HTMLInputElement::createRenderer(RenderArena *arena, RenderStyle *style)
{
    switch (inputType()) {
        case BUTTON:
        case RESET:
        case SUBMIT:
            return new (arena) RenderButton(this);
        case CHECKBOX:
        case RADIO:
            return RenderObject::createObject(this, style);
        case FILE:
            return new (arena) RenderFileUploadControl(this);
        case HIDDEN:
            break;
        case IMAGE:
            return new (arena) RenderImage(this);
        case RANGE:
            return new (arena) RenderSlider(this);
        case EMAIL:
        case ISINDEX:
        case NUMBER:
        case PASSWORD:
        case SEARCH:
        case TELEPHONE:
        case TEXT:
        case URL:
            return new (arena) RenderTextControlSingleLine(this);
    }
    ASSERT(false);
    return 0;
}

void HTMLInputElement::attach()
{
    if (!m_inited) {
        if (!m_haveType)
            setInputType(getAttribute(typeAttr));
        m_inited = true;
    }

    HTMLFormControlElementWithState::attach();

    if (inputType() == IMAGE) {
        if (!m_imageLoader)
            m_imageLoader.set(new HTMLImageLoader(this));
        m_imageLoader->updateFromElement();
        if (renderer()) {
            RenderImage* imageObj = static_cast<RenderImage*>(renderer());
            imageObj->setCachedImage(m_imageLoader->image()); 
            
            // If we have no image at all because we have no src attribute, set
            // image height and width for the alt text instead.
            if (!m_imageLoader->image() && !imageObj->cachedImage())
                imageObj->setImageSizeForAltText();
        }
    }
}

void HTMLInputElement::detach()
{
    HTMLFormControlElementWithState::detach();
    setValueMatchesRenderer(false);
}

String HTMLInputElement::altText() const
{
    // http://www.w3.org/TR/1998/REC-html40-19980424/appendix/notes.html#altgen
    // also heavily discussed by Hixie on bugzilla
    // note this is intentionally different to HTMLImageElement::altText()
    String alt = getAttribute(altAttr);
    // fall back to title attribute
    if (alt.isNull())
        alt = getAttribute(titleAttr);
    if (alt.isNull())
        alt = getAttribute(valueAttr);
    if (alt.isEmpty())
        alt = inputElementAltText();
    return alt;
}

bool HTMLInputElement::isSuccessfulSubmitButton() const
{
    // HTML spec says that buttons must have names to be considered successful.
    // However, other browsers do not impose this constraint. So we do likewise.
    return !disabled() && (inputType() == IMAGE || inputType() == SUBMIT);
}

bool HTMLInputElement::isActivatedSubmit() const
{
    return m_activeSubmit;
}

void HTMLInputElement::setActivatedSubmit(bool flag)
{
    m_activeSubmit = flag;
}

bool HTMLInputElement::appendFormData(FormDataList& encoding, bool multipart)
{
    // image generates its own names, but for other types there is no form data unless there's a name
    if (name().isEmpty() && inputType() != IMAGE)
        return false;

    switch (inputType()) {
        case EMAIL:
        case HIDDEN:
        case ISINDEX:
        case NUMBER:
        case PASSWORD:
        case RANGE:
        case SEARCH:
        case TELEPHONE:
        case TEXT:
        case URL:
            // always successful
            encoding.appendData(name(), value());
            return true;

        case CHECKBOX:
        case RADIO:
            if (checked()) {
                encoding.appendData(name(), value());
                return true;
            }
            break;

        case BUTTON:
        case RESET:
            // these types of buttons are never successful
            return false;

        case IMAGE:
            if (m_activeSubmit) {
                encoding.appendData(name().isEmpty() ? "x" : (name() + ".x"), m_xPos);
                encoding.appendData(name().isEmpty() ? "y" : (name() + ".y"), m_yPos);
                if (!name().isEmpty() && !value().isEmpty())
                    encoding.appendData(name(), value());
                return true;
            }
            break;

        case SUBMIT:
            if (m_activeSubmit) {
                String enc_str = valueWithDefault();
                encoding.appendData(name(), enc_str);
                return true;
            }
            break;

        case FILE: {
            // Can't submit file on GET.
            if (!multipart)
                return false;

            // If no filename at all is entered, return successful but empty.
            // Null would be more logical, but Netscape posts an empty file. Argh.
            unsigned numFiles = m_fileList->length();
            if (!numFiles) {
                encoding.appendFile(name(), File::create(""));
                return true;
            }

            for (unsigned i = 0; i < numFiles; ++i)
                encoding.appendFile(name(), m_fileList->item(i));
            return true;
        }
    }
    return false;
}

void HTMLInputElement::reset()
{
    if (storesValueSeparateFromAttribute())
        setValue(String());

    setChecked(m_defaultChecked);
    m_useDefaultChecked = true;
}

void HTMLInputElement::setChecked(bool nowChecked, bool sendChangeEvent)
{
    if (checked() == nowChecked)
        return;

    checkedRadioButtons(this).removeButton(this);

    m_useDefaultChecked = false;
    m_checked = nowChecked;
    setChanged();

    checkedRadioButtons(this).addButton(this);
    
    if (renderer() && renderer()->style()->hasAppearance())
        theme()->stateChanged(renderer(), CheckedState);

    // Only send a change event for items in the document (avoid firing during
    // parsing) and don't send a change event for a radio button that's getting
    // unchecked to match other browsers. DOM is not a useful standard for this
    // because it says only to fire change events at "lose focus" time, which is
    // definitely wrong in practice for these types of elements.
    if (sendChangeEvent && inDocument() && (inputType() != RADIO || nowChecked))
        onChange();
}

void HTMLInputElement::setIndeterminate(bool _indeterminate)
{
    // Only checkboxes honor indeterminate.
    if (inputType() != CHECKBOX || indeterminate() == _indeterminate)
        return;

    m_indeterminate = _indeterminate;

    setChanged();

    if (renderer() && renderer()->style()->hasAppearance())
        theme()->stateChanged(renderer(), CheckedState);
}

int HTMLInputElement::size() const
{
    return m_data.size();
}

void HTMLInputElement::copyNonAttributeProperties(const Element* source)
{
    const HTMLInputElement* sourceElement = static_cast<const HTMLInputElement*>(source);

    m_data.setValue(sourceElement->m_data.value());
    m_checked = sourceElement->m_checked;
    m_indeterminate = sourceElement->m_indeterminate;

    HTMLFormControlElementWithState::copyNonAttributeProperties(source);
}

String HTMLInputElement::value() const
{
    // The HTML5 spec (as of the 10/24/08 working draft) says that the value attribute isn't applicable to the file upload control
    // but we don't want to break existing websites, who may be relying on being able to get the file name as a value.
    if (inputType() == FILE) {
        if (!m_fileList->isEmpty())
            return m_fileList->item(0)->fileName();
        return String();
    }

    String value = m_data.value();
    if (value.isNull()) {
        value = constrainValue(getAttribute(valueAttr));

        // If no attribute exists, then just use "on" or "" based off the checked() state of the control.
        if (value.isNull() && (inputType() == CHECKBOX || inputType() == RADIO))
            return checked() ? "on" : "";
    }

    return value;
}

String HTMLInputElement::valueWithDefault() const
{
    String v = value();
    if (v.isNull()) {
        switch (inputType()) {
            case BUTTON:
            case CHECKBOX:
            case EMAIL:
            case FILE:
            case HIDDEN:
            case IMAGE:
            case ISINDEX:
            case NUMBER:
            case PASSWORD:
            case RADIO:
            case RANGE:
            case SEARCH:
            case TELEPHONE:
            case TEXT:
            case URL:
                break;
            case RESET:
                v = resetButtonDefaultLabel();
                break;
            case SUBMIT:
                v = submitButtonDefaultLabel();
                break;
        }
    }
    return v;
}

void HTMLInputElement::setValue(const String& value)
{
    // For security reasons, we don't allow setting the filename, but we do allow clearing it.
    // The HTML5 spec (as of the 10/24/08 working draft) says that the value attribute isn't applicable to the file upload control
    // but we don't want to break existing websites, who may be relying on this method to clear things.
    if (inputType() == FILE && !value.isEmpty())
        return;

    setValueMatchesRenderer(false);
    if (storesValueSeparateFromAttribute()) {
        if (inputType() == FILE)
            m_fileList->clear();
        else {
            m_data.setValue(constrainValue(value));
            if (isTextField()) {
                InputElement::updatePlaceholderVisibility(m_data, document());
                if (inDocument())
                    document()->updateRendering();
            }
        }
        if (renderer())
            renderer()->updateFromElement();
        setChanged();
    } else
        setAttribute(valueAttr, constrainValue(value));
    
    if (isTextField()) {
        unsigned max = m_data.value().length();
        if (document()->focusedNode() == this)
            InputElement::updateSelectionRange(m_data, max, max);
        else
            cacheSelection(max, max);
    }
    InputElement::notifyFormStateChanged(m_data, document());

    if (document() && document()->frame())
        document()->frame()->formElementDidSetValue(this);
}

String HTMLInputElement::placeholderValue() const
{
    return getAttribute(placeholderAttr).string();
}

bool HTMLInputElement::searchEventsShouldBeDispatched() const
{
    return hasAttribute(incrementalAttr);
}

void HTMLInputElement::setValueFromRenderer(const String& value)
{
    // File upload controls will always use setFileListFromRenderer.
    ASSERT(inputType() != FILE);
    InputElement::setValueFromRenderer(m_data, document(), value);
}

void HTMLInputElement::setFileListFromRenderer(const Vector<String>& paths)
{
    m_fileList->clear();
    int size = paths.size();
    for (int i = 0; i < size; i++)
        m_fileList->append(File::create(paths[i]));

    setValueMatchesRenderer();
    InputElement::notifyFormStateChanged(m_data, document());
}

bool HTMLInputElement::storesValueSeparateFromAttribute() const
{
    switch (inputType()) {
        case BUTTON:
        case CHECKBOX:
        case HIDDEN:
        case IMAGE:
        case RADIO:
        case RESET:
        case SUBMIT:
            return false;
        case EMAIL:
        case FILE:
        case ISINDEX:
        case NUMBER:
        case PASSWORD:
        case RANGE:
        case SEARCH:
        case TELEPHONE:
        case TEXT:
        case URL:
            return true;
    }
    return false;
}

void* HTMLInputElement::preDispatchEventHandler(Event *evt)
{
    // preventDefault or "return false" are used to reverse the automatic checking/selection we do here.
    // This result gives us enough info to perform the "undo" in postDispatch of the action we take here.
    void* result = 0; 
    if ((inputType() == CHECKBOX || inputType() == RADIO) && evt->isMouseEvent()
            && evt->type() == eventNames().clickEvent && static_cast<MouseEvent*>(evt)->button() == LeftButton) {
        if (inputType() == CHECKBOX) {
            // As a way to store the state, we return 0 if we were unchecked, 1 if we were checked, and 2 for
            // indeterminate.
            if (indeterminate()) {
                result = (void*)0x2;
                setIndeterminate(false);
            } else {
                if (checked())
                    result = (void*)0x1;
                setChecked(!checked(), true);
            }
        } else {
            // For radio buttons, store the current selected radio object.
            // We really want radio groups to end up in sane states, i.e., to have something checked.
            // Therefore if nothing is currently selected, we won't allow this action to be "undone", since
            // we want some object in the radio group to actually get selected.
            HTMLInputElement* currRadio = checkedRadioButtons(this).checkedButtonForGroup(name());
            if (currRadio) {
                // We have a radio button selected that is not us.  Cache it in our result field and ref it so
                // that it can't be destroyed.
                currRadio->ref();
                result = currRadio;
            }
            setChecked(true, true);
        }
    }
    return result;
}

void HTMLInputElement::postDispatchEventHandler(Event *evt, void* data)
{
    if ((inputType() == CHECKBOX || inputType() == RADIO) && evt->isMouseEvent()
            && evt->type() == eventNames().clickEvent && static_cast<MouseEvent*>(evt)->button() == LeftButton) {
        if (inputType() == CHECKBOX) {
            // Reverse the checking we did in preDispatch.
            if (evt->defaultPrevented() || evt->defaultHandled()) {
                if (data == (void*)0x2)
                    setIndeterminate(true);
                else
                    setChecked(data);
            }
        } else if (data) {
            HTMLInputElement* input = static_cast<HTMLInputElement*>(data);
            if (evt->defaultPrevented() || evt->defaultHandled()) {
                // Restore the original selected radio button if possible.
                // Make sure it is still a radio button and only do the restoration if it still
                // belongs to our group.

                if (input->form() == form() && input->inputType() == RADIO && input->name() == name()) {
                    // Ok, the old radio button is still in our form and in our group and is still a 
                    // radio button, so it's safe to restore selection to it.
                    input->setChecked(true);
                }
            }
            input->deref();
        }

        // Left clicks on radio buttons and check boxes already performed default actions in preDispatchEventHandler(). 
        evt->setDefaultHandled();
    }
}

void HTMLInputElement::defaultEventHandler(Event* evt)
{
    bool clickDefaultFormButton = false;

    if (isTextField() && evt->type() == eventNames().textInputEvent && evt->isTextEvent() && static_cast<TextEvent*>(evt)->data() == "\n")
        clickDefaultFormButton = true;

    if (inputType() == IMAGE && evt->isMouseEvent() && evt->type() == eventNames().clickEvent) {
        // record the mouse position for when we get the DOMActivate event
        MouseEvent* me = static_cast<MouseEvent*>(evt);
        // FIXME: We could just call offsetX() and offsetY() on the event,
        // but that's currently broken, so for now do the computation here.
        if (me->isSimulated() || !renderer()) {
            m_xPos = 0;
            m_yPos = 0;
        } else {
            // FIXME: This doesn't work correctly with transforms.
            IntPoint absOffset = roundedIntPoint(renderer()->localToAbsolute());
            m_xPos = me->pageX() - absOffset.x();
            m_yPos = me->pageY() - absOffset.y();
        }
    }

    if (isTextField() && evt->type() == eventNames().keydownEvent && evt->isKeyboardEvent() && focused() && document()->frame()
        && document()->frame()->doTextFieldCommandFromEvent(this, static_cast<KeyboardEvent*>(evt))) {
        evt->setDefaultHandled();
        return;
    }

    if (inputType() == RADIO && evt->isMouseEvent()
        && evt->type() == eventNames().clickEvent && static_cast<MouseEvent*>(evt)->button() == LeftButton) {
        evt->setDefaultHandled();
        return;
    }
    
    // Let the key handling done in EventTargetNode take precedence over the event handling here for editable text fields
    if (!clickDefaultFormButton) {
        HTMLFormControlElementWithState::defaultEventHandler(evt);
        if (evt->defaultHandled())
            return;
    }

    // DOMActivate events cause the input to be "activated" - in the case of image and submit inputs, this means
    // actually submitting the form. For reset inputs, the form is reset. These events are sent when the user clicks
    // on the element, or presses enter while it is the active element. JavaScript code wishing to activate the element
    // must dispatch a DOMActivate event - a click event will not do the job.
    if (evt->type() == eventNames().DOMActivateEvent && !disabled()) {
        if (inputType() == IMAGE || inputType() == SUBMIT || inputType() == RESET) {
            if (!form())
                return;
            if (inputType() == RESET)
                form()->reset();
            else {
                m_activeSubmit = true;
                // FIXME: Would be cleaner to get m_xPos and m_yPos out of the underlying mouse
                // event (if any) here instead of relying on the variables set above when
                // processing the click event. Even better, appendFormData could pass the
                // event in, and then we could get rid of m_xPos and m_yPos altogether!
                if (!form()->prepareSubmit(evt)) {
                    m_xPos = 0;
                    m_yPos = 0;
                }
                m_activeSubmit = false;
            }
        } else if (inputType() == FILE && renderer())
            static_cast<RenderFileUploadControl*>(renderer())->click();
    }

    // Use key press event here since sending simulated mouse events
    // on key down blocks the proper sending of the key press event.
    if (evt->type() == eventNames().keypressEvent && evt->isKeyboardEvent()) {
        bool clickElement = false;

        int charCode = static_cast<KeyboardEvent*>(evt)->charCode();

        if (charCode == '\r') {
            switch (inputType()) {
                case CHECKBOX:
                case EMAIL:
                case HIDDEN:
                case ISINDEX:
                case NUMBER:
                case PASSWORD:
                case RANGE:
                case SEARCH:
                case TELEPHONE:
                case TEXT:
                case URL:
                    // Simulate mouse click on the default form button for enter for these types of elements.
                    clickDefaultFormButton = true;
                    break;
                case BUTTON:
                case FILE:
                case IMAGE:
                case RESET:
                case SUBMIT:
                    // Simulate mouse click for enter for these types of elements.
                    clickElement = true;
                    break;
                case RADIO:
                    break; // Don't do anything for enter on a radio button.
            }
        } else if (charCode == ' ') {
            switch (inputType()) {
                case BUTTON:
                case CHECKBOX:
                case FILE:
                case IMAGE:
                case RESET:
                case SUBMIT:
                case RADIO:
                    // Prevent scrolling down the page.
                    evt->setDefaultHandled();
                    return;
                default:
                    break;
            }
        }

        if (clickElement) {
            dispatchSimulatedClick(evt);
            evt->setDefaultHandled();
            return;
        }
    }

    if (evt->type() == eventNames().keydownEvent && evt->isKeyboardEvent()) {
        String key = static_cast<KeyboardEvent*>(evt)->keyIdentifier();

        if (key == "U+0020") {
            switch (inputType()) {
                case BUTTON:
                case CHECKBOX:
                case FILE:
                case IMAGE:
                case RESET:
                case SUBMIT:
                case RADIO:
                    setActive(true, true);
                    // No setDefaultHandled() - IE dispatches a keypress in this case.
                    return;
                default:
                    break;
            }
        }

        if (inputType() == RADIO && (key == "Up" || key == "Down" || key == "Left" || key == "Right")) {
            // Left and up mean "previous radio button".
            // Right and down mean "next radio button".
            // Tested in WinIE, and even for RTL, left still means previous radio button (and so moves
            // to the right).  Seems strange, but we'll match it.
            bool forward = (key == "Down" || key == "Right");
            
            // We can only stay within the form's children if the form hasn't been demoted to a leaf because
            // of malformed HTML.
            Node* n = this;
            while ((n = (forward ? n->traverseNextNode() : n->traversePreviousNode()))) {
                // Once we encounter a form element, we know we're through.
                if (n->hasTagName(formTag))
                    break;
                    
                // Look for more radio buttons.
                if (n->hasTagName(inputTag)) {
                    HTMLInputElement* elt = static_cast<HTMLInputElement*>(n);
                    if (elt->form() != form())
                        break;
                    if (n->hasTagName(inputTag)) {
                        HTMLInputElement* inputElt = static_cast<HTMLInputElement*>(n);
                        if (inputElt->inputType() == RADIO && inputElt->name() == name() && inputElt->isFocusable()) {
                            inputElt->setChecked(true);
                            document()->setFocusedNode(inputElt);
                            inputElt->dispatchSimulatedClick(evt, false, false);
                            evt->setDefaultHandled();
                            break;
                        }
                    }
                }
            }
        }
    }

    if (evt->type() == eventNames().keyupEvent && evt->isKeyboardEvent()) {
        bool clickElement = false;

        String key = static_cast<KeyboardEvent*>(evt)->keyIdentifier();

        if (key == "U+0020") {
            switch (inputType()) {
                case BUTTON:
                case CHECKBOX:
                case FILE:
                case IMAGE:
                case RESET:
                case SUBMIT:
                    // Simulate mouse click for spacebar for these types of elements.
                    // The AppKit already does this for some, but not all, of them.
                    clickElement = true;
                    break;
                case RADIO:
                    // If an unselected radio is tabbed into (because the entire group has nothing
                    // checked, or because of some explicit .focus() call), then allow space to check it.
                    if (!checked())
                        clickElement = true;
                    break;
                case EMAIL:
                case HIDDEN:
                case ISINDEX:
                case NUMBER:
                case PASSWORD:
                case RANGE:
                case SEARCH:
                case TELEPHONE:
                case TEXT:
                case URL:
                    break;
            }
        }

        if (clickElement) {
            if (active())
                dispatchSimulatedClick(evt);
            evt->setDefaultHandled();
            return;
        }        
    }

    if (clickDefaultFormButton) {
        if (isSearchField()) {
            addSearchResult();
            onSearch();
        }
        // Fire onChange for text fields.
        RenderObject* r = renderer();
        if (r && r->isTextField() && r->isEdited()) {
            onChange();
            // Refetch the renderer since arbitrary JS code run during onchange can do anything, including destroying it.
            r = renderer();
            if (r)
                r->setEdited(false);
        }
        // Form may never have been present, or may have been destroyed by the change event.
        if (form())
            form()->submitClick(evt);
        evt->setDefaultHandled();
        return;
    }

    if (evt->isBeforeTextInsertedEvent())
        InputElement::handleBeforeTextInsertedEvent(m_data, document(), evt);

    if (isTextField() && renderer() && (evt->isMouseEvent() || evt->isDragEvent() || evt->isWheelEvent() || evt->type() == eventNames().blurEvent || evt->type() == eventNames().focusEvent))
        static_cast<RenderTextControlSingleLine*>(renderer())->forwardEvent(evt);

    if (inputType() == RANGE && renderer()) {
        RenderSlider* slider = static_cast<RenderSlider*>(renderer());
        if (evt->isMouseEvent() && evt->type() == eventNames().mousedownEvent && static_cast<MouseEvent*>(evt)->button() == LeftButton) {
            MouseEvent* mEvt = static_cast<MouseEvent*>(evt);
            if (!slider->mouseEventIsInThumb(mEvt)) {
                IntPoint eventOffset(mEvt->offsetX(), mEvt->offsetY());
                if (mEvt->target() != this) // Does this ever happen now? Was added for <video> controls
                    eventOffset = roundedIntPoint(renderer()->absoluteToLocal(FloatPoint(mEvt->pageX(), mEvt->pageY()), false, true));
                slider->setValueForPosition(slider->positionForOffset(eventOffset));
            }
        }
        if (evt->isMouseEvent() || evt->isDragEvent() || evt->isWheelEvent())
            slider->forwardEvent(evt);
    }
}

bool HTMLInputElement::isURLAttribute(Attribute *attr) const
{
    return (attr->name() == srcAttr);
}

String HTMLInputElement::defaultValue() const
{
    return getAttribute(valueAttr);
}

void HTMLInputElement::setDefaultValue(const String &value)
{
    setAttribute(valueAttr, value);
}

bool HTMLInputElement::defaultChecked() const
{
    return !getAttribute(checkedAttr).isNull();
}

void HTMLInputElement::setDefaultChecked(bool defaultChecked)
{
    setAttribute(checkedAttr, defaultChecked ? "" : 0);
}

void HTMLInputElement::setDefaultName(const AtomicString& name)
{
    m_data.setName(name);
}

String HTMLInputElement::accept() const
{
    return getAttribute(acceptAttr);
}

void HTMLInputElement::setAccept(const String &value)
{
    setAttribute(acceptAttr, value);
}

String HTMLInputElement::accessKey() const
{
    return getAttribute(accesskeyAttr);
}

void HTMLInputElement::setAccessKey(const String &value)
{
    setAttribute(accesskeyAttr, value);
}

String HTMLInputElement::align() const
{
    return getAttribute(alignAttr);
}

void HTMLInputElement::setAlign(const String &value)
{
    setAttribute(alignAttr, value);
}

String HTMLInputElement::alt() const
{
    return getAttribute(altAttr);
}

void HTMLInputElement::setAlt(const String &value)
{
    setAttribute(altAttr, value);
}

int HTMLInputElement::maxLength() const
{
    return m_data.maxLength();
}

void HTMLInputElement::setMaxLength(int _maxLength)
{
    setAttribute(maxlengthAttr, String::number(_maxLength));
}

void HTMLInputElement::setSize(unsigned _size)
{
    setAttribute(sizeAttr, String::number(_size));
}

KURL HTMLInputElement::src() const
{
    return document()->completeURL(getAttribute(srcAttr));
}

void HTMLInputElement::setSrc(const String &value)
{
    setAttribute(srcAttr, value);
}

String HTMLInputElement::useMap() const
{
    return getAttribute(usemapAttr);
}

void HTMLInputElement::setUseMap(const String &value)
{
    setAttribute(usemapAttr, value);
}

void HTMLInputElement::setAutofilled(bool b)
{
    if (b == m_autofilled)
        return;
        
    m_autofilled = b;
    setChanged();
}

FileList* HTMLInputElement::files()
{
    if (inputType() != FILE)
        return 0;
    return m_fileList.get();
}

String HTMLInputElement::constrainValue(const String& proposedValue) const
{
    return InputElement::constrainValue(m_data, proposedValue, m_data.maxLength());
}

bool HTMLInputElement::needsActivationCallback()
{
    return inputType() == PASSWORD || m_autocomplete == Off;
}

void HTMLInputElement::registerForActivationCallbackIfNeeded()
{
    if (needsActivationCallback())
        document()->registerForDocumentActivationCallbacks(this);
}

void HTMLInputElement::unregisterForActivationCallbackIfNeeded()
{
    if (!needsActivationCallback())
        document()->unregisterForDocumentActivationCallbacks(this);
}

void HTMLInputElement::cacheSelection(int start, int end)
{
    m_data.setCachedSelectionStart(start);
    m_data.setCachedSelectionEnd(end);
}

void HTMLInputElement::addSearchResult()
{
}

void HTMLInputElement::onSearch()
{
}

Selection HTMLInputElement::selection() const
{
   if (!renderer() || !isTextField() || m_data.cachedSelectionStart() == -1 || m_data.cachedSelectionEnd() == -1)
        return Selection();
   return static_cast<RenderTextControl*>(renderer())->selection(m_data.cachedSelectionStart(), m_data.cachedSelectionEnd());
}

void HTMLInputElement::documentDidBecomeActive()
{
    ASSERT(needsActivationCallback());
    reset();
}

void HTMLInputElement::willMoveToNewOwnerDocument()
{
    // Always unregister for cache callbacks when leaving a document, even if we would otherwise like to be registered
    if (needsActivationCallback())
        document()->unregisterForDocumentActivationCallbacks(this);
        
    document()->checkedRadioButtons().removeButton(this);
    
    HTMLFormControlElementWithState::willMoveToNewOwnerDocument();
}

void HTMLInputElement::didMoveToNewOwnerDocument()
{
    registerForActivationCallbackIfNeeded();
        
    HTMLFormControlElementWithState::didMoveToNewOwnerDocument();
}

bool HTMLInputElement::willRespondToMouseClickEvents()
{
    return !disabled();
}
    
void HTMLInputElement::addSubresourceAttributeURLs(ListHashSet<KURL>& urls) const
{
    HTMLFormControlElementWithState::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, src());
}

bool HTMLInputElement::willValidate() const
{
    // FIXME: This shall check for new WF2 input types too
    return HTMLFormControlElementWithState::willValidate() && inputType() != HIDDEN &&
           inputType() != BUTTON && inputType() != RESET;
}

bool HTMLInputElement::placeholderShouldBeVisible() const
{
    return m_data.placeholderShouldBeVisible();
}

} // namespace

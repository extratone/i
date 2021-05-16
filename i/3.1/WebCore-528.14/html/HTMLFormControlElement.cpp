/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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
#include "HTMLFormControlElement.h"

#include "Document.h"
#include "EventHandler.h"
#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLParser.h"
#include "HTMLTokenizer.h"
#include "RenderTheme.h"

namespace WebCore {

using namespace HTMLNames;

HTMLFormControlElement::HTMLFormControlElement(const QualifiedName& tagName, Document* doc, HTMLFormElement* f)
    : HTMLElement(tagName, doc)
    , m_form(f)
    , m_disabled(false)
    , m_readOnly(false)
    , m_valueMatchesRenderer(false)
{
    if (!m_form)
        m_form = findFormAncestor();
    if (m_form)
        m_form->registerFormElement(this);
}

HTMLFormControlElement::~HTMLFormControlElement()
{
    if (m_form)
        m_form->removeFormElement(this);
}

void HTMLFormControlElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == nameAttr) {
        // Do nothing.
    } else if (attr->name() == disabledAttr) {
        bool oldDisabled = m_disabled;
        m_disabled = !attr->isNull();
        if (oldDisabled != m_disabled) {
            setChanged();
            if (renderer() && renderer()->style()->hasAppearance())
                theme()->stateChanged(renderer(), EnabledState);
        }
    } else if (attr->name() == readonlyAttr) {
        bool oldReadOnly = m_readOnly;
        m_readOnly = !attr->isNull();
        if (oldReadOnly != m_readOnly) {
            setChanged();
            if (renderer() && renderer()->style()->hasAppearance())
                theme()->stateChanged(renderer(), ReadOnlyState);
        }
    } else
        HTMLElement::parseMappedAttribute(attr);
}

void HTMLFormControlElement::attach()
{
    ASSERT(!attached());

    HTMLElement::attach();

    // The call to updateFromElement() needs to go after the call through
    // to the base class's attach() because that can sometimes do a close
    // on the renderer.
    if (renderer())
        renderer()->updateFromElement();
        
    // Focus the element if it should honour its autofocus attribute.
    // We have to determine if the element is a TextArea/Input/Button/Select,
    // if input type hidden ignore autofocus. So if disabled or readonly.
    if (autofocus() && renderer() && !document()->ignoreAutofocus() && !isReadOnlyControl() &&
            ((hasTagName(inputTag) && !isInputTypeHidden()) || hasTagName(selectTag) ||
              hasTagName(buttonTag) || hasTagName(textareaTag)))
         focus();
}

void HTMLFormControlElement::insertedIntoTree(bool deep)
{
    if (!m_form) {
        // This handles the case of a new form element being created by
        // JavaScript and inserted inside a form.  In the case of the parser
        // setting a form, we will already have a non-null value for m_form, 
        // and so we don't need to do anything.
        m_form = findFormAncestor();
        if (m_form)
            m_form->registerFormElement(this);
        else
            document()->checkedRadioButtons().addButton(this);
    }

    HTMLElement::insertedIntoTree(deep);
}

static inline Node* findRoot(Node* n)
{
    Node* root = n;
    for (; n; n = n->parentNode())
        root = n;
    return root;
}

void HTMLFormControlElement::removedFromTree(bool deep)
{
    // If the form and element are both in the same tree, preserve the connection to the form.
    // Otherwise, null out our form and remove ourselves from the form's list of elements.
    HTMLParser* parser = 0;
    if (Tokenizer* tokenizer = document()->tokenizer())
        if (tokenizer->isHTMLTokenizer())
            parser = static_cast<HTMLTokenizer*>(tokenizer)->htmlParser();
    
    if (m_form && !(parser && parser->isHandlingResidualStyleAcrossBlocks()) && findRoot(this) != findRoot(m_form)) {
        m_form->removeFormElement(this);
        m_form = 0;
    }

    HTMLElement::removedFromTree(deep);
}

const AtomicString& HTMLFormControlElement::name() const
{
    const AtomicString& n = getAttribute(nameAttr);
    return n.isNull() ? emptyAtom : n;
}

void HTMLFormControlElement::setName(const AtomicString &value)
{
    setAttribute(nameAttr, value);
}

void HTMLFormControlElement::onChange()
{
    dispatchEventForType(eventNames().changeEvent, true, false);
}

bool HTMLFormControlElement::disabled() const
{
    return m_disabled;
}

void HTMLFormControlElement::setDisabled(bool b)
{
    setAttribute(disabledAttr, b ? "" : 0);
}

void HTMLFormControlElement::setReadOnly(bool b)
{
    setAttribute(readonlyAttr, b ? "" : 0);
}

bool HTMLFormControlElement::autofocus() const
{
    return hasAttribute(autofocusAttr);
}

void HTMLFormControlElement::setAutofocus(bool b)
{
    setAttribute(autofocusAttr, b ? "autofocus" : 0);
}

void HTMLFormControlElement::recalcStyle(StyleChange change)
{
    HTMLElement::recalcStyle(change);

    if (renderer())
        renderer()->updateFromElement();
}

bool HTMLFormControlElement::isFocusable() const
{
    if (disabled() || !renderer() || 
        (renderer()->style() && renderer()->style()->visibility() != VISIBLE) || 
        !renderer()->isBox() || toRenderBox(renderer())->size().isEmpty())
        return false;
    return true;
}

bool HTMLFormControlElement::isKeyboardFocusable(KeyboardEvent* event) const
{
    if (isFocusable())
        if (document()->frame())
            return document()->frame()->eventHandler()->tabsToAllControls(event);
    return false;
}

bool HTMLFormControlElement::isMouseFocusable() const
{
#if PLATFORM(GTK)
    return HTMLElement::isMouseFocusable();
#else
    return false;
#endif
}

short HTMLFormControlElement::tabIndex() const
{
    // Skip the supportsFocus check in HTMLElement.
    return Element::tabIndex();
}

bool HTMLFormControlElement::willValidate() const
{
    // FIXME: Implementation shall be completed with these checks:
    //      The control does not have a repetition template as an ancestor.
    //      The control does not have a datalist element as an ancestor.
    //      The control is not an output element.
    return form() && name().length() && !disabled() && !isReadOnlyControl();
}
    
bool HTMLFormControlElement::supportsFocus() const
{
    return isFocusable() || (!disabled() && !document()->haveStylesheetsLoaded());
}

HTMLFormElement* HTMLFormControlElement::virtualForm() const
{
    return m_form;
}

void HTMLFormControlElement::removeFromForm()
{
    if (!m_form)
        return;
    m_form->removeFormElement(this);
    m_form = 0;
}

HTMLFormControlElementWithState::HTMLFormControlElementWithState(const QualifiedName& tagName, Document* doc, HTMLFormElement* f)
    : HTMLFormControlElement(tagName, doc, f)
{
    FormControlElementWithState::registerFormControlElementWithState(this, document());
}

HTMLFormControlElementWithState::~HTMLFormControlElementWithState()
{
    FormControlElementWithState::unregisterFormControlElementWithState(this, document());
}

void HTMLFormControlElementWithState::willMoveToNewOwnerDocument()
{
    FormControlElementWithState::unregisterFormControlElementWithState(this, document());
    HTMLFormControlElement::willMoveToNewOwnerDocument();
}

void HTMLFormControlElementWithState::didMoveToNewOwnerDocument()
{
    FormControlElementWithState::registerFormControlElementWithState(this, document());
    HTMLFormControlElement::didMoveToNewOwnerDocument();
}

void HTMLFormControlElementWithState::finishParsingChildren()
{
    HTMLFormControlElement::finishParsingChildren();
    Document* doc = document();
    if (doc->hasStateForNewFormElements()) {
        String state;
        if (doc->takeStateForFormElement(name().impl(), type().impl(), state))
            restoreState(state);
    }
}

void HTMLFormControlElement::dispatchFocusEvent()
{
    if (document()->frame())
        document()->frame()->formElementDidFocus(this);
    
    HTMLElement::dispatchFocusEvent();
}

void HTMLFormControlElement::dispatchBlurEvent()
{
    if (document()->frame())
        document()->frame()->formElementDidBlur(this);
    
    HTMLElement::dispatchBlurEvent();
}
    
bool HTMLFormControlElement::autocorrect() const
{
    if (hasAttribute(autocorrectAttr))
        return !equalIgnoringCase(getAttribute(autocorrectAttr), "off");
    
    // Assuming we're still in a Form, respect the Form's setting
    if (HTMLFormElement* form = this->form())
        return form->autocorrect();
    
    // The default is true
    return true;
}

void HTMLFormControlElement::setAutocorrect(bool b)
{
    setAttribute(autocorrectAttr, b ? "on" : "off");
}

bool HTMLFormControlElement::autocapitalize() const
{
    if (hasAttribute(autocapitalizeAttr))
        return !equalIgnoringCase(getAttribute(autocapitalizeAttr), "off");
    
    // Assuming we're still in a Form, respect the Form's setting
    if (HTMLFormElement* form = this->form())
        return form->autocapitalize();
    
    // The default is true
    return true;
}

void HTMLFormControlElement::setAutocapitalize(bool b)
{
    setAttribute(autocapitalizeAttr, b ? "on" : "off");
}

} // namespace Webcore

/*
* Copyright (C) 2008 Apple Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
* 1.  Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
* 2.  Redistributions in binary form must reproduce the above copyright
*     notice, this list of conditions and the following disclaimer in the
*     documentation and/or other materials provided with the distribution.
* 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
*     its contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
* THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "config.h"
#include "AccessibilityRenderObject.h"

#include "AXObjectCache.h"
#include "AccessibilityListBox.h"
#include "AccessibilityImageMapLink.h"
#include "CharacterNames.h"
#include "EventNames.h"
#include "FloatRect.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLAreaElement.h"
#include "HTMLFrameElementBase.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLLabelElement.h"
#include "HTMLMapElement.h"
#include "HTMLOptGroupElement.h"
#include "HTMLOptionElement.h"
#include "HTMLOptionsCollection.h"
#include "HTMLSelectElement.h"
#include "HTMLTextAreaElement.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "LocalizedStrings.h"
#include "NodeList.h"
#include "NotImplemented.h"
#include "Page.h"
#include "RenderFieldset.h"
#include "RenderFileUploadControl.h"
#include "RenderImage.h"
#include "RenderListBox.h"
#include "RenderListMarker.h"
#include "RenderMenuList.h"
#include "RenderText.h"
#include "RenderTextControl.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "SelectionController.h"
#include "Text.h"
#include "TextIterator.h"
#include "htmlediting.h"
#include "visible_units.h"
#include <wtf/StdLibExtras.h>

using namespace std;

namespace WebCore {

using namespace HTMLNames;

AccessibilityRenderObject::AccessibilityRenderObject(RenderObject* renderer)
    : m_renderer(renderer)
    , m_ariaRole(UnknownRole)
{
    setAriaRole();
#ifndef NDEBUG
    m_renderer->setHasAXObject(true);
#endif
}

AccessibilityRenderObject::~AccessibilityRenderObject()
{
    ASSERT(isDetached());
}

PassRefPtr<AccessibilityRenderObject> AccessibilityRenderObject::create(RenderObject* renderer)
{
    return adoptRef(new AccessibilityRenderObject(renderer));
}

void AccessibilityRenderObject::detach()
{
    clearChildren();
    AccessibilityObject::detach();
    
#ifndef NDEBUG
    if (m_renderer)
        m_renderer->setHasAXObject(false);
#endif
    m_renderer = 0;    
}

AccessibilityObject* AccessibilityRenderObject::firstChild() const
{
    if (!m_renderer)
        return 0;
    
    RenderObject* firstChild = m_renderer->firstChild();
    if (!firstChild)
        return 0;
    
    return m_renderer->document()->axObjectCache()->get(firstChild);
}

AccessibilityObject* AccessibilityRenderObject::lastChild() const
{
    if (!m_renderer)
        return 0;
    
    RenderObject* lastChild = m_renderer->lastChild();
    if (!lastChild)
        return 0;
    
    return m_renderer->document()->axObjectCache()->get(lastChild);
}

AccessibilityObject* AccessibilityRenderObject::previousSibling() const
{
    if (!m_renderer)
        return 0;
    
    RenderObject* previousSibling = m_renderer->previousSibling();
    if (!previousSibling)
        return 0;
    
    return m_renderer->document()->axObjectCache()->get(previousSibling);
}

AccessibilityObject* AccessibilityRenderObject::nextSibling() const
{
    if (!m_renderer)
        return 0;
    
    RenderObject* nextSibling = m_renderer->nextSibling();
    if (!nextSibling)
        return 0;
    
    return m_renderer->document()->axObjectCache()->get(nextSibling);
}

AccessibilityObject* AccessibilityRenderObject::parentObject() const
{
    if (!m_renderer)
        return 0;
    
    RenderObject *parent = m_renderer->parent();
    if (!parent)
        return 0;
    
    if (ariaRoleAttribute() == MenuBarRole)
        return m_renderer->document()->axObjectCache()->get(parent);

    // menuButton and its corresponding menu are DOM siblings, but Accessibility needs them to be parent/child
    if (ariaRoleAttribute() == MenuRole) {
        AccessibilityObject* parent = menuButtonForMenu();
        if (parent)
            return parent;
    }
    
    return m_renderer->document()->axObjectCache()->get(parent);
}

bool AccessibilityRenderObject::isWebArea() const
{
    return roleValue() == WebAreaRole;
}

bool AccessibilityRenderObject::isImageButton() const
{
    return isNativeImage() && roleValue() == ButtonRole;
}

bool AccessibilityRenderObject::isAnchor() const
{
    return !isNativeImage() && isLink();
}

bool AccessibilityRenderObject::isNativeTextControl() const
{
    return m_renderer->isTextField() || m_renderer->isTextArea();
}
    
bool AccessibilityRenderObject::isTextControl() const
{
    AccessibilityRole role = roleValue();
    return role == TextAreaRole || role == TextFieldRole;
}

bool AccessibilityRenderObject::isNativeImage() const
{
    return m_renderer->isImage();
}    
    
bool AccessibilityRenderObject::isImage() const
{
    return roleValue() == ImageRole;
}

bool AccessibilityRenderObject::isAttachment() const
{
    // Widgets are the replaced elements that we represent to AX as attachments
    bool isWidget = m_renderer && m_renderer->isWidget();
    ASSERT(!isWidget || (m_renderer->isReplaced() && !isImage()));
    return isWidget && ariaRoleAttribute() == UnknownRole;
}

bool AccessibilityRenderObject::isPasswordField() const
{
    ASSERT(m_renderer);
    if (!m_renderer->element() || !m_renderer->element()->isHTMLElement())
        return false;
    return static_cast<HTMLElement*>(m_renderer->element())->isPasswordField() && ariaRoleAttribute() == UnknownRole;
}

bool AccessibilityRenderObject::isCheckboxOrRadio() const
{
    AccessibilityRole role = roleValue();
    return role == RadioButtonRole || role == CheckBoxRole;
}    
    
bool AccessibilityRenderObject::isFileUploadButton() const
{
    if (m_renderer && m_renderer->element() && m_renderer->element()->hasTagName(inputTag)) {
        HTMLInputElement* input = static_cast<HTMLInputElement*>(m_renderer->element());
        return input->inputType() == HTMLInputElement::FILE;
    }
    
    return false;
}
    
bool AccessibilityRenderObject::isInputImage() const
{
    if (m_renderer && m_renderer->element() && m_renderer->element()->hasTagName(inputTag)) {
        HTMLInputElement* input = static_cast<HTMLInputElement*>(m_renderer->element());
        return input->inputType() == HTMLInputElement::IMAGE;
    }
    
    return false;
}

bool AccessibilityRenderObject::isProgressIndicator() const
{
    return roleValue() == ProgressIndicatorRole;
}

bool AccessibilityRenderObject::isSlider() const
{
    return roleValue() == SliderRole;
}
    
bool AccessibilityRenderObject::isMenuRelated() const
{
    AccessibilityRole role = roleValue();
    return  role == MenuRole ||
            role == MenuBarRole ||
            role == MenuButtonRole ||
            role == MenuItemRole;
}    

bool AccessibilityRenderObject::isMenu() const
{
    return roleValue() == MenuRole;
}

bool AccessibilityRenderObject::isMenuBar() const
{
    return roleValue() == MenuBarRole;
}

bool AccessibilityRenderObject::isMenuButton() const
{
    return roleValue() == MenuButtonRole;
}

bool AccessibilityRenderObject::isMenuItem() const
{
    return roleValue() == MenuItemRole;
}
     
bool AccessibilityRenderObject::isPressed() const
{
    ASSERT(m_renderer);
    if (roleValue() != ButtonRole)
        return false;

    Node* node = m_renderer->node();
    if (!node)
        return false;

    // If this is an ARIA button, check the aria-pressed attribute rather than node()->active()
    if (ariaRoleAttribute() == ButtonRole) {
        if (equalIgnoringCase(getAttribute(aria_pressedAttr).string(), "true"))
            return true;
        return false;
    }

    return node->active();
}

bool AccessibilityRenderObject::isIndeterminate() const
{
    ASSERT(m_renderer);
    return m_renderer->node() && m_renderer->node()->isIndeterminate();
}

bool AccessibilityRenderObject::isChecked() const
{
    ASSERT(m_renderer);
    return m_renderer->node() && m_renderer->node()->isChecked();
}

bool AccessibilityRenderObject::isHovered() const
{
    ASSERT(m_renderer);
    return m_renderer->node() && m_renderer->node()->hovered();
}

bool AccessibilityRenderObject::isMultiSelect() const
{
    ASSERT(m_renderer);
    if (!m_renderer->isListBox())
        return false;
    return m_renderer->element() && static_cast<HTMLSelectElement*>(m_renderer->element())->multiple();
}
    
bool AccessibilityRenderObject::isReadOnly() const
{
    ASSERT(m_renderer);
    
    if (isWebArea()) {
        Document* document = m_renderer->document();
        if (!document)
            return true;
        
        HTMLElement* body = document->body();
        if (body && body->isContentEditable())
            return false;
        
        Frame* frame = document->frame();
        if (!frame)
            return true;
        
        return !frame->isContentEditable();
    }

    return !m_renderer->node() || !m_renderer->node()->isContentEditable();
}

bool AccessibilityRenderObject::isOffScreen() const
{
    ASSERT(m_renderer);
    IntRect contentRect = m_renderer->absoluteClippedOverflowRect();
    FrameView* view = m_renderer->document()->frame()->view();
    FloatRect viewRect = view->visibleContentRect();
    viewRect.intersect(contentRect);
    return viewRect.isEmpty();
}

int AccessibilityRenderObject::headingLevel(Node* node)
{
    // headings can be in block flow and non-block flow
    if (!node)
        return 0;

    if (RenderObject* renderer = node->renderer()) {
        AccessibilityObject* axObjectForNode = node->document()->axObjectCache()->get(renderer);
        if (axObjectForNode->ariaRoleAttribute() == HeadingRole) {
            if (!node->isElementNode())
                return 0;
            Element* element = static_cast<Element*>(node);
            return element->getAttribute(aria_levelAttr).toInt();
        }
    }
            
    
    if (node->hasTagName(h1Tag))
        return 1;
    
    if (node->hasTagName(h2Tag))
        return 2;
    
    if (node->hasTagName(h3Tag))
        return 3;
    
    if (node->hasTagName(h4Tag))
        return 4;
    
    if (node->hasTagName(h5Tag))
        return 5;
    
    if (node->hasTagName(h6Tag))
        return 6;
    
    return 0;
}

bool AccessibilityRenderObject::isHeading() const
{
    return roleValue() == HeadingRole;
}
    
bool AccessibilityRenderObject::isLink() const
{
    return roleValue() == WebCoreLinkRole;
}    
    
bool AccessibilityRenderObject::isControl() const
{
    if (!m_renderer)
        return false;
    
    Node* node = m_renderer->element();
    return node && (node->isControl() || AccessibilityObject::isARIAControl(ariaRoleAttribute()));
}

bool AccessibilityRenderObject::isFieldset() const
{
    if (!m_renderer)
        return false;
    
    return m_renderer->isFieldset();
}
  
bool AccessibilityRenderObject::isGroup() const
{
    return roleValue() == GroupRole;
}
    
const AtomicString& AccessibilityRenderObject::getAttribute(const QualifiedName& attribute) const
{
    Node* node = m_renderer->element();
    if (!node)
        return nullAtom;

    if (!node->isElementNode())
        return nullAtom;

    Element* element = static_cast<Element*>(node);
    return element->getAttribute(attribute);
}

Element* AccessibilityRenderObject::anchorElement() const
{
    if (!m_renderer)
        return 0;
    
    AXObjectCache* cache = axObjectCache();
    RenderObject* currRenderer;
    
    // Search up the render tree for a RenderObject with a DOM node.  Defer to an earlier continuation, though.
    for (currRenderer = m_renderer; currRenderer && !currRenderer->element(); currRenderer = currRenderer->parent()) {
        RenderFlow* continuation = currRenderer->virtualContinuation();
        if (continuation)
            return cache->get(continuation)->anchorElement();
    }
    
    // bail if none found
    if (!currRenderer)
        return 0;
    
    // search up the DOM tree for an anchor element
    // NOTE: this assumes that any non-image with an anchor is an HTMLAnchorElement
    Node* node = currRenderer->node();
    for ( ; node; node = node->parentNode()) {
        if (node->hasTagName(aTag) || (node->renderer() && cache->get(node->renderer())->isAnchor()))
            return static_cast<Element*>(node);
    }
    
    return 0;
}

Element* AccessibilityRenderObject::actionElement() const
{
    if (!m_renderer)
        return 0;
    
    Node* node = m_renderer->element();
    if (node) {
        if (node->hasTagName(inputTag)) {
            HTMLInputElement* input = static_cast<HTMLInputElement*>(node);
            if (!input->disabled() && (isCheckboxOrRadio() || input->isTextButton()))
                return input;
        } else if (node->hasTagName(buttonTag))
            return static_cast<Element*>(node);
    }
            
    if (isFileUploadButton())
        return static_cast<Element*>(m_renderer->element());
            
    if (AccessibilityObject::isARIAInput(ariaRoleAttribute()))
        return static_cast<Element*>(m_renderer->element());

    if (isImageButton())
        return static_cast<Element*>(m_renderer->element());
    
    if (m_renderer->isMenuList())
        return static_cast<RenderMenuList*>(m_renderer)->selectElement();
    
    Element* elt = anchorElement();
    if (!elt)
        elt = mouseButtonListener();
    return elt;
}

Element* AccessibilityRenderObject::mouseButtonListener() const
{
    Node* node = m_renderer->element();
    if (!node)
        return 0;
    if (!node->isEventTargetNode())
        return 0;
    
    // FIXME: Do the continuation search like anchorElement does
    for (EventTargetNode* elt = static_cast<EventTargetNode*>(node); elt; elt = static_cast<EventTargetNode*>(elt->parentNode())) {
        if (elt->inlineEventListenerForType(eventNames().clickEvent) || elt->inlineEventListenerForType(eventNames().mousedownEvent) || elt->inlineEventListenerForType(eventNames().mouseupEvent))
            return static_cast<Element*>(elt);
    }
    
    return 0;
}

static Element* siblingWithAriaRole(String role, Node* node)
{
    Node* sibling = node->parent()->firstChild();
    while (sibling) {
        if (sibling->isElementNode()) {
            String siblingAriaRole = static_cast<Element*>(sibling)->getAttribute(roleAttr).string();
            if (equalIgnoringCase(siblingAriaRole, role))
                return static_cast<Element*>(sibling);
        }
        sibling = sibling->nextSibling();
    }
    
    return 0;
}

Element* AccessibilityRenderObject::menuElementForMenuButton() const
{
    if (ariaRoleAttribute() != MenuButtonRole)
        return 0;

    return siblingWithAriaRole("menu", renderer()->node());
}

AccessibilityObject* AccessibilityRenderObject::menuForMenuButton() const
{
    Element* menu = menuElementForMenuButton();
    if (menu && menu->renderer())
        return m_renderer->document()->axObjectCache()->get(menu->renderer());
    return 0;
}

Element* AccessibilityRenderObject::menuItemElementForMenu() const
{
    if (ariaRoleAttribute() != MenuRole)
        return 0;
    
    return siblingWithAriaRole("menuitem", renderer()->node());    
}

AccessibilityObject* AccessibilityRenderObject::menuButtonForMenu() const
{
    Element* menuItem = menuItemElementForMenu();

    if (menuItem && menuItem->renderer()) {
        // ARIA just has generic menu items.  AppKit needs to know if this is a top level items like MenuBarButton or MenuBarItem
        AccessibilityObject* menuItemAX = m_renderer->document()->axObjectCache()->get(menuItem->renderer());
        if (menuItemAX->isMenuButton())
            return menuItemAX;
    }
    return 0;
}

String AccessibilityRenderObject::helpText() const
{
    if (!m_renderer)
        return String();
    
    for (RenderObject* curr = m_renderer; curr; curr = curr->parent()) {
        if (curr->element() && curr->element()->isHTMLElement()) {
            const AtomicString& summary = static_cast<Element*>(curr->element())->getAttribute(summaryAttr);
            if (!summary.isEmpty())
                return summary;
            const AtomicString& title = static_cast<Element*>(curr->element())->getAttribute(titleAttr);
            if (!title.isEmpty())
                return title;
        }
    }
    
    return String();
}

String AccessibilityRenderObject::language() const
{
    if (!m_renderer)
        return String();
    
    // Defer to parent if this element doesn't have a language set
    Node* node = m_renderer->element();
    if (!node)
        return AccessibilityObject::language();
    
    if (!node->isElementNode())
        return AccessibilityObject::language();
    
    String language = static_cast<Element*>(node)->getAttribute(langAttr);
    if (language.isEmpty())
        return AccessibilityObject::language();
    
    return language;
}
    
String AccessibilityRenderObject::textUnderElement() const
{
    if (!m_renderer)
        return String();
    
    if (isFileUploadButton()) {
        RenderFileUploadControl* uploadControl = static_cast<RenderFileUploadControl*>(m_renderer);
        return uploadControl->buttonValue();
    }
    
    Node* node = m_renderer->element();
    if (node) {
        if (Frame* frame = node->document()->frame()) {
            // catch stale WebCoreAXObject (see <rdar://problem/3960196>)
            if (frame->document() != node->document())
                return String();
            return plainText(rangeOfContents(node).get());
        }
    }
    
    // return the null string for anonymous text because it is non-trivial to get
    // the actual text and, so far, that is not needed
    return String();
}

bool AccessibilityRenderObject::hasIntValue() const
{
    if (isHeading())
        return true;
    
    if (m_renderer->element() && isCheckboxOrRadio())
        return true;
    
    return false;
}

int AccessibilityRenderObject::intValue() const
{
    if (!m_renderer || isPasswordField())
        return 0;
    
    if (isHeading())
        return headingLevel(m_renderer->element());
    
    Node* node = m_renderer->element();
    if (!node || !isCheckboxOrRadio())
        return 0;

    // If this is an ARIA checkbox or radio, check the aria-checked attribute rather than node()->checked()
    AccessibilityRole ariaRole = ariaRoleAttribute();
    if (ariaRole == RadioButtonRole || ariaRole == CheckBoxRole) {
        if (equalIgnoringCase(getAttribute(aria_checkedAttr).string(), "true"))
            return true;
        return false;
    }
    
    return static_cast<HTMLInputElement*>(node)->checked();
}

float AccessibilityRenderObject::valueForRange() const
{
    if (!isProgressIndicator() && !isSlider())
        return 0.0f;

    return getAttribute(aria_valuenowAttr).toFloat();
}

float AccessibilityRenderObject::maxValueForRange() const
{
    if (!isProgressIndicator() && !isSlider())
        return 0.0f;

    return getAttribute(aria_valuemaxAttr).toFloat();
}

float AccessibilityRenderObject::minValueForRange() const
{
    if (!isProgressIndicator() && !isSlider())
        return 0.0f;

    return getAttribute(aria_valueminAttr).toFloat();
}

String AccessibilityRenderObject::stringValue() const
{
    if (!m_renderer || isPasswordField())
        return String();
    
    if (m_renderer->isText())
        return textUnderElement();
    
    if (m_renderer->isMenuList())
        return static_cast<RenderMenuList*>(m_renderer)->text();
    
    if (m_renderer->isListMarker())
        return static_cast<RenderListMarker*>(m_renderer)->text();
    
    if (isWebArea()) {
        if (m_renderer->document()->frame())
            return String();
        
        // FIXME: should use startOfDocument and endOfDocument (or rangeForDocument?) here
        VisiblePosition startVisiblePosition = m_renderer->positionForCoordinates(0, 0);
        VisiblePosition endVisiblePosition = m_renderer->positionForCoordinates(INT_MAX, INT_MAX);
        if (startVisiblePosition.isNull() || endVisiblePosition.isNull())
            return String();
        
        return plainText(makeRange(startVisiblePosition, endVisiblePosition).get());
    }
    
    if (isTextControl())
        return text();
    
    
    // FIXME: We might need to implement a value here for more types
    // FIXME: It would be better not to advertise a value at all for the types for which we don't implement one;
    // this would require subclassing or making accessibilityAttributeNames do something other than return a
    // single static array.
    return String();
}

// This function implements the ARIA accessible name as described by the Mozilla
// ARIA Implementer's Guide.
static String accessibleNameForNode(Node* node)
{
    if (node->isTextNode())
        return static_cast<Text*>(node)->data();

    if (node->hasTagName(inputTag))
        return static_cast<HTMLInputElement*>(node)->value();

    if (node->isHTMLElement()) {
        const AtomicString& alt = static_cast<HTMLElement*>(node)->getAttribute(altAttr);
        if (!alt.isEmpty())
            return alt;
    }

    return String();
}

String AccessibilityRenderObject::ariaAccessiblityName(const String& s) const
{
    Document* document = m_renderer->document();
    if (!document)
        return String();

    String idList = s;
    idList.replace('\n', ' ');
    Vector<String> idVector;
    idList.split(' ', idVector);

    Vector<UChar> ariaLabel;
    unsigned size = idVector.size();
    for (unsigned i = 0; i < size; ++i) {
        String idName = idVector[i];
        Element* idElement = document->getElementById(idName);
        if (idElement) {
            String nameFragment = accessibleNameForNode(idElement);
            ariaLabel.append(nameFragment.characters(), nameFragment.length());
            for (Node* n = idElement->firstChild(); n; n = n->traverseNextNode(idElement->nextSibling())) {
                nameFragment = accessibleNameForNode(n);
                ariaLabel.append(nameFragment.characters(), nameFragment.length());
            }
            ariaLabel.append(' ');
        }
    }
    return String::adopt(ariaLabel);
}

String AccessibilityRenderObject::ariaLabeledByAttribute() const
{
    Node* node = m_renderer->node();
    if (!node)
        return String();

    if (!node->isElementNode())
        return String();

    // The ARIA spec uses the British spelling: "labelled." It seems prudent to support the American
    // spelling ("labeled") as well.
    String idList = getAttribute(aria_labeledbyAttr).string();
    if (idList.isEmpty()) {
        idList = getAttribute(aria_labelledbyAttr).string();
        if (idList.isEmpty())
            return String();
    }

    return ariaAccessiblityName(idList);
}

static HTMLLabelElement* labelForElement(Element* element)
{
    RefPtr<NodeList> list = element->document()->getElementsByTagName("label");
    unsigned len = list->length();
    for (unsigned i = 0; i < len; i++) {
        if (list->item(i)->hasTagName(labelTag)) {
            HTMLLabelElement* label = static_cast<HTMLLabelElement*>(list->item(i));
            if (label->correspondingControl() == element)
                return label;
        }
    }
    
    return 0;
}
    
HTMLLabelElement* AccessibilityRenderObject::labelElementContainer() const
{
    if (!m_renderer)
        return false;

    // the control element should not be considered part of the label
    if (isControl())
        return false;
    
    // find if this has a parent that is a label
    for (Node* parentNode = m_renderer->element(); parentNode; parentNode = parentNode->parentNode()) {
        if (parentNode->hasTagName(labelTag))
            return static_cast<HTMLLabelElement*>(parentNode);
    }
    
    return 0;
}

String AccessibilityRenderObject::title() const
{
    AccessibilityRole ariaRole = ariaRoleAttribute();
    
    if (!m_renderer)
        return String();

    Node* node = m_renderer->element();
    if (!node)
        return String();
    
    String ariaLabel = ariaLabeledByAttribute();
    if (!ariaLabel.isEmpty())
        return ariaLabel;
    
    const AtomicString& title = getAttribute(titleAttr);
    if (!title.isEmpty())
        return title;
    
    bool isInputTag = node->hasTagName(inputTag);
    if (isInputTag) {
        HTMLInputElement* input = static_cast<HTMLInputElement*>(node);
        if (input->isTextButton())
            return input->value();
    }
    
    if (isInputTag || AccessibilityObject::isARIAInput(ariaRole) || isControl()) {
        HTMLLabelElement* label = labelForElement(static_cast<Element*>(node));
        if (label && !titleUIElement())
            return label->innerText();
    }
    
    if (roleValue() == ButtonRole
        || ariaRole == ListBoxOptionRole
        || ariaRole == MenuItemRole
        || ariaRole == MenuButtonRole
        || isHeading())
        return textUnderElement();
    
    if (isLink())
        return textUnderElement();
    
    return String();
}

String AccessibilityRenderObject::ariaDescribedByAttribute() const
{
    String idList = getAttribute(aria_describedbyAttr).string();
    if (idList.isEmpty())
        return String();
    
    return ariaAccessiblityName(idList);
}

String AccessibilityRenderObject::accessibilityDescription() const
{
    if (!m_renderer)
        return String();

    String ariaLabel = getAttribute(aria_labelAttr).string();
    if (!ariaLabel.isEmpty())
        return ariaLabel;    
    
    String ariaDescription = ariaDescribedByAttribute();
    if (!ariaDescription.isEmpty())
        return ariaDescription;
    
    if (isImage()) {
        if (m_renderer->element() && m_renderer->element()->isHTMLElement()) {
            const AtomicString& alt = static_cast<HTMLElement*>(m_renderer->element())->getAttribute(altAttr);
            if (alt.isEmpty())
                return String();
            return alt;
        }
    }
    
    if (isWebArea()) {
        Document *document = m_renderer->document();
        Node* owner = document->ownerElement();
        if (owner) {
            if (owner->hasTagName(frameTag) || owner->hasTagName(iframeTag)) {
                const AtomicString& title = static_cast<HTMLFrameElementBase*>(owner)->getAttribute(titleAttr);
                if (!title.isEmpty())
                    return title;
                return static_cast<HTMLFrameElementBase*>(owner)->name();
            }
            if (owner->isHTMLElement())
                return static_cast<HTMLElement*>(owner)->getAttribute(nameAttr);
        }
        owner = document->body();
        if (owner && owner->isHTMLElement())
            return static_cast<HTMLElement*>(owner)->getAttribute(nameAttr);
    }
    
    
    return String();
}

IntRect AccessibilityRenderObject::boundingBoxRect() const
{
    RenderObject* obj = m_renderer;
    
    if (!obj)
        return IntRect();
    
    if (obj->isInlineContinuation())
        obj = obj->element()->renderer();
    
    Vector<FloatQuad> quads;
    obj->absoluteQuads(quads);
    const size_t n = quads.size();
    if (!n)
        return IntRect();

    IntRect result;
    for (size_t i = 0; i < n; ++i) {
        IntRect r = quads[i].enclosingBoundingBox();
        if (!r.isEmpty()) {
            if (obj->style()->hasAppearance())
                theme()->adjustRepaintRect(obj, r);
            result.unite(r);
        }
    }
    return result;
}
    
IntRect AccessibilityRenderObject::checkboxOrRadioRect() const
{
    if (!m_renderer)
        return IntRect();
    
    HTMLLabelElement* label = labelForElement(static_cast<Element*>(m_renderer->element()));
    if (!label || !label->renderer())
        return boundingBoxRect();
    
    IntRect labelRect = axObjectCache()->get(label->renderer())->elementRect();
    labelRect.unite(boundingBoxRect());
    return labelRect;
}

IntRect AccessibilityRenderObject::elementRect() const
{
    // a checkbox or radio button should encompass its label
    if (isCheckboxOrRadio())
        return checkboxOrRadioRect();
    
    return boundingBoxRect();
}

IntSize AccessibilityRenderObject::size() const
{
    IntRect rect = elementRect();
    return rect.size();
}

AccessibilityObject* AccessibilityRenderObject::internalLinkElement() const
{
    Element* element = anchorElement();
    if (!element)
        return 0;
    
    // Right now, we do not support ARIA links as internal link elements
    if (!element->hasTagName(aTag))
        return 0;
    HTMLAnchorElement* anchor = static_cast<HTMLAnchorElement*>(element);
    
    KURL linkURL = anchor->href();
    String ref = linkURL.ref();
    if (ref.isEmpty())
        return 0;
    
    // check if URL is the same as current URL
    linkURL.removeRef();
    if (m_renderer->document()->url() != linkURL)
        return 0;
    
    Node* linkedNode = m_renderer->document()->findAnchor(ref);
    if (!linkedNode)
        return 0;
    
    // the element we find may not be accessible, keep searching until we find a good one
    AccessibilityObject* linkedAXElement = m_renderer->document()->axObjectCache()->get(linkedNode->renderer());
    while (linkedAXElement && linkedAXElement->accessibilityIsIgnored()) {
        linkedNode = linkedNode->traverseNextNode();
        
        while (linkedNode && !linkedNode->renderer())
            linkedNode = linkedNode->traverseNextSibling();
        
        if (!linkedNode)
            return 0;
        linkedAXElement = m_renderer->document()->axObjectCache()->get(linkedNode->renderer());
    }
    
    return linkedAXElement;
}
    
void AccessibilityRenderObject::addRadioButtonGroupMembers(AccessibilityChildrenVector& linkedUIElements) const
{
    if (!m_renderer || roleValue() != RadioButtonRole)
        return;
    
    Node* node = m_renderer->node();
    if (!node || !node->hasTagName(inputTag))
        return;
    
    HTMLInputElement* input = static_cast<HTMLInputElement*>(node);
    // if there's a form, then this is easy
    if (input->form()) {
        Vector<RefPtr<Node> > formElements;
        input->form()->getNamedElements(input->name(), formElements);
        
        unsigned len = formElements.size();
        for (unsigned i = 0; i < len; ++i) {
            Node* associateElement = formElements[i].get();
            if (AccessibilityObject* object = m_renderer->document()->axObjectCache()->get(associateElement->renderer()))
                linkedUIElements.append(object);        
        } 
    } else {
        RefPtr<NodeList> list = node->document()->getElementsByTagName("input");
        unsigned len = list->length();
        for (unsigned i = 0; i < len; ++i) {
            if (list->item(i)->hasTagName(inputTag)) {
                HTMLInputElement* associateElement = static_cast<HTMLInputElement*>(list->item(i));
                if (associateElement->isRadioButton() && associateElement->name() == input->name()) {
                    if (AccessibilityObject* object = m_renderer->document()->axObjectCache()->get(associateElement->renderer()))
                        linkedUIElements.append(object);
                }
            }
        }
    }
}
    
// linked ui elements could be all the related radio buttons in a group
// or an internal anchor connection
void AccessibilityRenderObject::linkedUIElements(AccessibilityChildrenVector& linkedUIElements) const
{
    if (isAnchor()) {
        AccessibilityObject* linkedAXElement = internalLinkElement();
        if (linkedAXElement)
            linkedUIElements.append(linkedAXElement);
    }

    if (roleValue() == RadioButtonRole)
        addRadioButtonGroupMembers(linkedUIElements);
}

AccessibilityObject* AccessibilityRenderObject::titleUIElement() const
{
    if (!m_renderer)
        return 0;
    
    // if isFieldset is true, the renderer is guaranteed to be a RenderFieldset
    if (isFieldset())
        return axObjectCache()->get(static_cast<RenderFieldset*>(m_renderer)->findLegend());
    
    // checkbox and radio hide their labels. Only controls get titleUIElements for now
    if (isCheckboxOrRadio() || !isControl())
        return 0;
    
    Node* element = m_renderer->element();
    HTMLLabelElement* label = labelForElement(static_cast<Element*>(element));
    if (label && label->renderer())
        return axObjectCache()->get(label->renderer());

    return 0;   
}
    
bool AccessibilityRenderObject::ariaIsHidden() const
{
    if (equalIgnoringCase(getAttribute(aria_hiddenAttr).string(), "true"))
        return true;
    
    // aria-hidden hides this object and any children
    AccessibilityObject* object = parentObject();
    while (object) {
        if (object->isAccessibilityRenderObject() && equalIgnoringCase(static_cast<AccessibilityRenderObject*>(object)->getAttribute(aria_hiddenAttr).string(), "true"))
            return true;
        object = object->parentObject();
    }

    return false;
}

bool AccessibilityRenderObject::accessibilityIsIgnored() const
{
    // ignore invisible element
    if (!m_renderer || m_renderer->style()->visibility() != VISIBLE)
        return true;

    if (ariaIsHidden())
        return true;
    
    if (isPresentationalChildOfAriaRole())
        return true;
        
    // ignore popup menu items because AppKit does
    for (RenderObject* parent = m_renderer->parent(); parent; parent = parent->parent()) {
        if (parent->isMenuList())
            return true;
    }
    
    // find out if this element is inside of a label element.
    // if so, it may be ignored because it's the label for a checkbox or radio button
    HTMLLabelElement* labelElement = labelElementContainer();
    if (labelElement) {
        HTMLElement* correspondingControl = labelElement->correspondingControl();
        if (correspondingControl && correspondingControl->renderer()) {
            AccessibilityObject* controlObject = axObjectCache()->get(correspondingControl->renderer());
            if (controlObject->isCheckboxOrRadio())
                return true;
        }
    }
        
    AccessibilityRole ariaRole = ariaRoleAttribute();
    if (ariaRole == TextAreaRole || ariaRole == StaticTextRole) {
        String ariaText = text();
        return ariaText.isNull() || ariaText.isEmpty();
    }    
    
    // NOTE: BRs always have text boxes now, so the text box check here can be removed
    if (m_renderer->isText()) {
        // static text beneath MenuItems and MenuButtons are just reported along with the menu item, so it's ignored on an individual level
        if (parentObjectUnignored()->ariaRoleAttribute() == MenuItemRole ||
            parentObjectUnignored()->ariaRoleAttribute() == MenuButtonRole)
            return true;
         return m_renderer->isBR() || !toRenderText(m_renderer)->firstTextBox();
    }
    
    if (isHeading())
        return false;
    
    if (isLink())
        return false;
    
    // all controls are accessible
    if (isControl())
        return false;
    
    // don't ignore labels, because they serve as TitleUIElements
    Node* node = m_renderer->element();
    if (node && node->hasTagName(labelTag))
        return false;
    
    if (m_renderer->isBlockFlow() && m_renderer->childrenInline())
        return !static_cast<RenderBlock*>(m_renderer)->firstLineBox() && !mouseButtonListener();
    
    // ignore images seemingly used as spacers
    if (isImage()) {
        if (node && node->isElementNode()) {
            Element* elt = static_cast<Element*>(node);
            const AtomicString& alt = elt->getAttribute(altAttr);
            // don't ignore an image that has an alt tag
            if (!alt.isEmpty())
                return false;
            // informal standard is to ignore images with zero-length alt strings
            if (!alt.isNull())
                return true;
        }
        
        // check for one-dimensional image
        RenderImage* image = static_cast<RenderImage*>(m_renderer);
        if (image->height() <= 1 || image->width() <= 1)
            return true;
        
        // check whether rendered image was stretched from one-dimensional file image
        if (isNativeImage()) {
            if (image->cachedImage()) {
                IntSize imageSize = image->cachedImage()->imageSize(image->view()->zoomFactor());
                return imageSize.height() <= 1 || imageSize.width() <= 1;
            }
        }
        return false;
    }
    
    if (ariaRole != UnknownRole)
        return false;
    
    // make a platform-specific decision
    if (isAttachment())
        return accessibilityIgnoreAttachment();
    
    return !m_renderer->isListMarker() && !isWebArea();
}

bool AccessibilityRenderObject::isLoaded() const
{
    return !m_renderer->document()->tokenizer();
}

int AccessibilityRenderObject::layoutCount() const
{
    if (!m_renderer->isRenderView())
        return 0;
    return static_cast<RenderView*>(m_renderer)->frameView()->layoutCount();
}

String AccessibilityRenderObject::text() const
{
    if (!isTextControl() || isPasswordField())
        return String();
    
    if (isNativeTextControl())
        return static_cast<RenderTextControl*>(m_renderer)->text();
    
    Node* node = m_renderer->element();
    if (!node)
        return String();
    if (!node->isElementNode())
        return String();
    
    return static_cast<Element*>(node)->innerText();
}
    
int AccessibilityRenderObject::textLength() const
{
    ASSERT(isTextControl());
    
    if (isPasswordField())
        return -1; // need to return something distinct from 0
    
    return text().length();
}

PassRefPtr<Range> AccessibilityRenderObject::ariaSelectedTextDOMRange() const
{
    Node* node = m_renderer->element();
    if (!node)
        return 0;
    
    RefPtr<Range> currentSelectionRange = selection().toRange();
    if (!currentSelectionRange)
        return 0;
    
    ExceptionCode ec = 0;
    if (!currentSelectionRange->intersectsNode(node, ec))
        return Range::create(currentSelectionRange->ownerDocument());
    
    RefPtr<Range> ariaRange = rangeOfContents(node);
    Position startPosition, endPosition;
    
    // Find intersection of currentSelectionRange and ariaRange
    if (ariaRange->startOffset() > currentSelectionRange->startOffset())
        startPosition = ariaRange->startPosition();
    else
        startPosition = currentSelectionRange->startPosition();
    
    if (ariaRange->endOffset() < currentSelectionRange->endOffset())
        endPosition = ariaRange->endPosition();
    else
        endPosition = currentSelectionRange->endPosition();
    
    return Range::create(ariaRange->ownerDocument(), startPosition, endPosition);
}

String AccessibilityRenderObject::selectedText() const
{
    ASSERT(isTextControl());
    
    if (isPasswordField())
        return String(); // need to return something distinct from empty string
    
    if (isNativeTextControl()) {
        RenderTextControl* textControl = static_cast<RenderTextControl*>(m_renderer);
        return textControl->text().substring(textControl->selectionStart(), textControl->selectionEnd() - textControl->selectionStart());
    }
    
    if (ariaRoleAttribute() == UnknownRole)
        return String();
    
    RefPtr<Range> ariaRange = ariaSelectedTextDOMRange();
    if (!ariaRange)
        return String();
    return ariaRange->text();
}

const AtomicString& AccessibilityRenderObject::accessKey() const
{
    Node* node = m_renderer->element();
    if (!node)
        return nullAtom;
    if (!node->isElementNode())
        return nullAtom;
    return static_cast<Element*>(node)->getAttribute(accesskeyAttr);
}

Selection AccessibilityRenderObject::selection() const
{
    return m_renderer->document()->frame()->selection()->selection();
}

PlainTextRange AccessibilityRenderObject::selectedTextRange() const
{
    ASSERT(isTextControl());
    
    if (isPasswordField())
        return PlainTextRange();
    
    AccessibilityRole ariaRole = ariaRoleAttribute();
    if (isNativeTextControl() && ariaRole == UnknownRole) {
        RenderTextControl* textControl = static_cast<RenderTextControl*>(m_renderer);
        return PlainTextRange(textControl->selectionStart(), textControl->selectionEnd() - textControl->selectionStart());
    }
    
    if (ariaRole == UnknownRole)
        return PlainTextRange();
    
    RefPtr<Range> ariaRange = ariaSelectedTextDOMRange();
    if (!ariaRange)
        return PlainTextRange();
    return PlainTextRange(ariaRange->startOffset(), ariaRange->endOffset());
}

void AccessibilityRenderObject::setSelectedTextRange(const PlainTextRange& range)
{
    if (isNativeTextControl()) {
        RenderTextControl* textControl = static_cast<RenderTextControl*>(m_renderer);
        textControl->setSelectionRange(range.start, range.start + range.length);
        return;
    }
    
    Document* document = m_renderer->document();
    if (!document)
        return;
    Frame* frame = document->frame();
    if (!frame)
        return;
    Node* node = m_renderer->element();
    frame->selection()->setSelection(Selection(Position(node, range.start),
        Position(node, range.start + range.length), DOWNSTREAM));
}

KURL AccessibilityRenderObject::url() const
{
    if (isAnchor() && m_renderer->element()->hasTagName(aTag)) {
        if (HTMLAnchorElement* anchor = static_cast<HTMLAnchorElement*>(anchorElement()))
            return anchor->href();
    }
    
    if (isWebArea())
        return m_renderer->document()->url();
    
    if (isImage() && m_renderer->element() && m_renderer->element()->hasTagName(imgTag))
        return static_cast<HTMLImageElement*>(m_renderer->element())->src();
    
    if (isInputImage())
        return static_cast<HTMLInputElement*>(m_renderer->element())->src();
    
    return KURL();
}

bool AccessibilityRenderObject::isVisited() const
{
    return m_renderer->style()->pseudoState() == PseudoVisited;
}

bool AccessibilityRenderObject::isSelected() const
{
    if (!m_renderer)
        return false;
    
    Node* node = m_renderer->node();
    if (!node)
        return false;
    
    return false;
}

bool AccessibilityRenderObject::isFocused() const
{
    if (!m_renderer)
        return false;
    
    Document* document = m_renderer->document();
    if (!document)
        return false;
    
    Node* focusedNode = document->focusedNode();
    if (!focusedNode)
        return false;
    
    // A web area is represented by the Document node in the DOM tree, which isn't focusable.
    // Check instead if the frame's selection controller is focused
    if (focusedNode == m_renderer->element() || 
        (roleValue() == WebAreaRole && document->frame()->selection()->isFocusedAndActive()))
        return true;
    
    return false;
}

void AccessibilityRenderObject::setFocused(bool on)
{
    if (!canSetFocusAttribute())
        return;
    
    if (!on)
        m_renderer->document()->setFocusedNode(0);
    else {
        if (m_renderer->element()->isElementNode())
            static_cast<Element*>(m_renderer->element())->focus();
        else
            m_renderer->document()->setFocusedNode(m_renderer->element());
    }
}

void AccessibilityRenderObject::setValue(const String& string)
{
    // FIXME: Do we want to do anything here for ARIA textboxes?
    if (m_renderer->isTextField()) {
        HTMLInputElement* input = static_cast<HTMLInputElement*>(m_renderer->element());
        input->setValue(string);
    } else if (m_renderer->isTextArea()) {
        HTMLTextAreaElement* textArea = static_cast<HTMLTextAreaElement*>(m_renderer->element());
        textArea->setValue(string);
    }
}

bool AccessibilityRenderObject::isEnabled() const
{
    if (equalIgnoringCase(getAttribute(aria_disabledAttr).string(), "true"))
        return false;

    return m_renderer->element() ? m_renderer->element()->isEnabled() : true;
}

RenderView* AccessibilityRenderObject::topRenderer() const
{
    return m_renderer->document()->topDocument()->renderView();
}

Document* AccessibilityRenderObject::document() const
{
    return m_renderer->document();
}

FrameView* AccessibilityRenderObject::topDocumentFrameView() const
{
    return topRenderer()->view()->frameView();
}

Widget* AccessibilityRenderObject::widget() const
{
    if (!m_renderer->isWidget())
        return 0;
    
    return static_cast<RenderWidget*>(m_renderer)->widget();
}

AXObjectCache* AccessibilityRenderObject::axObjectCache() const
{
    return m_renderer->document()->axObjectCache();
}

AccessibilityObject* AccessibilityRenderObject::accessibilityParentForImageMap(HTMLMapElement* map) const
{
    // find an image that is using this map
    if (!m_renderer || !map)
        return 0;

    RefPtr<HTMLCollection> coll = m_renderer->document()->images();
    for (Node* curr = coll->firstItem(); curr; curr = coll->nextItem()) {
        RenderObject* obj = curr->renderer();
        if (!obj || !curr->hasTagName(imgTag))
            continue;
        
        // The HTMLImageElement's useMap() value includes the '#' symbol at the beginning,
        // which has to be stripped off
        if (static_cast<HTMLImageElement*>(curr)->useMap().substring(1) == map->getName())
            return axObjectCache()->get(obj);
    }
    
    return 0;
}
    
void AccessibilityRenderObject::getDocumentLinks(AccessibilityChildrenVector& result)
{
    Document* document = m_renderer->document();
    RefPtr<HTMLCollection> coll = document->links();
    Node* curr = coll->firstItem();
    while (curr) {
        RenderObject* obj = curr->renderer();
        if (obj) {
            RefPtr<AccessibilityObject> axobj = document->axObjectCache()->get(obj);
            ASSERT(axobj);
            ASSERT(axobj->roleValue() == WebCoreLinkRole);
            if (!axobj->accessibilityIsIgnored())
                result.append(axobj);
        } else {
            Node* parent = curr->parent();
            if (parent && curr->hasTagName(areaTag) && parent->hasTagName(mapTag)) {
                AccessibilityImageMapLink* areaObject = static_cast<AccessibilityImageMapLink*>(axObjectCache()->get(ImageMapLinkRole));
                areaObject->setHTMLAreaElement(static_cast<HTMLAreaElement*>(curr));
                areaObject->setHTMLMapElement(static_cast<HTMLMapElement*>(parent));
                areaObject->setParent(accessibilityParentForImageMap(static_cast<HTMLMapElement*>(parent)));

                result.append(areaObject);
            }
        }
        curr = coll->nextItem();
    }
}

FrameView* AccessibilityRenderObject::documentFrameView() const 
{ 
    if (!m_renderer || !m_renderer->document()) 
        return 0; 

    // this is the RenderObject's Document's Frame's FrameView 
    return m_renderer->document()->view();
}

Widget* AccessibilityRenderObject::widgetForAttachmentView() const
{
    if (!isAttachment())
        return 0;
    return static_cast<RenderWidget*>(m_renderer)->widget();
}

FrameView* AccessibilityRenderObject::frameViewIfRenderView() const
{
    if (!m_renderer->isRenderView())
        return 0;
    // this is the RenderObject's Document's renderer's FrameView
    return m_renderer->view()->frameView();
}

// This function is like a cross-platform version of - (WebCoreTextMarkerRange*)textMarkerRange. It returns
// a Range that we can convert to a WebCoreTextMarkerRange in the Obj-C file
VisiblePositionRange AccessibilityRenderObject::visiblePositionRange() const
{
    if (!m_renderer)
        return VisiblePositionRange();
    
    // construct VisiblePositions for start and end
    Node* node = m_renderer->element();
    if (!node)
        return VisiblePositionRange();
    
    VisiblePosition startPos = VisiblePosition(node, 0, VP_DEFAULT_AFFINITY);
    VisiblePosition endPos = VisiblePosition(node, maxDeepOffset(node), VP_DEFAULT_AFFINITY);
    
    // the VisiblePositions are equal for nodes like buttons, so adjust for that
    if (startPos == endPos) {
        endPos = endPos.next();
        if (endPos.isNull())
            endPos = startPos;
    }
    
    return VisiblePositionRange(startPos, endPos);
}

VisiblePositionRange AccessibilityRenderObject::visiblePositionRangeForLine(unsigned lineCount) const
{
    if (lineCount == 0 || !m_renderer)
        return VisiblePositionRange();
    
    // iterate over the lines
    // FIXME: this is wrong when lineNumber is lineCount+1,  because nextLinePosition takes you to the
    // last offset of the last line
    VisiblePosition visiblePos = m_renderer->document()->renderer()->positionForCoordinates(0, 0);
    VisiblePosition savedVisiblePos;
    while (--lineCount != 0) {
        savedVisiblePos = visiblePos;
        visiblePos = nextLinePosition(visiblePos, 0);
        if (visiblePos.isNull() || visiblePos == savedVisiblePos)
            return VisiblePositionRange();
    }
    
    // make a caret selection for the marker position, then extend it to the line
    // NOTE: ignores results of sel.modify because it returns false when
    // starting at an empty line.  The resulting selection in that case
    // will be a caret at visiblePos.
    SelectionController selection;
    selection.setSelection(Selection(visiblePos));
    selection.modify(SelectionController::EXTEND, SelectionController::RIGHT, LineBoundary);
    
    return VisiblePositionRange(selection.selection().visibleStart(), selection.selection().visibleEnd());
}
    
VisiblePosition AccessibilityRenderObject::visiblePositionForIndex(int index) const
{
    if (!m_renderer)
        return VisiblePosition();
    
    if (isNativeTextControl())
        return static_cast<RenderTextControl*>(m_renderer)->visiblePositionForIndex(index);
    
    if (!isTextControl() && !m_renderer->isText())
        return VisiblePosition();
    
    Node* node = m_renderer->node();
    if (!node)
        return VisiblePosition();
    
    if (index <= 0)
        return VisiblePosition(node, 0, DOWNSTREAM);
    
    ExceptionCode ec = 0;
    RefPtr<Range> range = Range::create(m_renderer->document());
    range->selectNodeContents(node, ec);
    CharacterIterator it(range.get());
    it.advance(index - 1);
    return VisiblePosition(it.range()->endContainer(ec), it.range()->endOffset(ec), UPSTREAM);
}
    
int AccessibilityRenderObject::indexForVisiblePosition(const VisiblePosition& pos) const
{
    if (isNativeTextControl())
        return static_cast<RenderTextControl*>(m_renderer)->indexForVisiblePosition(pos);
    
    if (!isTextControl())
        return 0;
    
    Node* node = m_renderer->node();
    if (!node)
        return 0;
    
    Position indexPosition = pos.deepEquivalent();
    if (!indexPosition.node() || indexPosition.node()->rootEditableElement() != node)
        return 0;
    
    ExceptionCode ec = 0;
    RefPtr<Range> range = Range::create(m_renderer->document());
    range->setStart(node, 0, ec);
    range->setEnd(indexPosition.node(), indexPosition.offset(), ec);
    return TextIterator::rangeLength(range.get());
}

IntRect AccessibilityRenderObject::boundsForVisiblePositionRange(const VisiblePositionRange& visiblePositionRange) const
{
    if (visiblePositionRange.isNull())
        return IntRect();
    
    // Create a mutable VisiblePositionRange.
    VisiblePositionRange range(visiblePositionRange);
    IntRect rect1 = range.start.absoluteCaretBounds();
    IntRect rect2 = range.end.absoluteCaretBounds();
    
    // readjust for position at the edge of a line.  This is to exclude line rect that doesn't need to be accounted in the range bounds
    if (rect2.y() != rect1.y()) {
        VisiblePosition endOfFirstLine = endOfLine(range.start);
        if (range.start == endOfFirstLine) {
            range.start.setAffinity(DOWNSTREAM);
            rect1 = range.start.absoluteCaretBounds();
        }
        if (range.end == endOfFirstLine) {
            range.end.setAffinity(UPSTREAM);
            rect2 = range.end.absoluteCaretBounds();
        }
    }
    
    IntRect ourrect = rect1;
    ourrect.unite(rect2);
    
    // if the rectangle spans lines and contains multiple text chars, use the range's bounding box intead
    if (rect1.bottom() != rect2.bottom()) {
        RefPtr<Range> dataRange = makeRange(range.start, range.end);
        IntRect boundingBox = dataRange->boundingBox();
        String rangeString = plainText(dataRange.get());
        if (rangeString.length() > 1 && !boundingBox.isEmpty())
            ourrect = boundingBox;
    }
    
    return ourrect;
}
    
void AccessibilityRenderObject::setSelectedVisiblePositionRange(const VisiblePositionRange& range) const
{
    if (range.start.isNull() || range.end.isNull())
        return;
    
    // make selection and tell the document to use it. if it's zero length, then move to that position
    if (range.start == range.end) {
        m_renderer->document()->frame()->selection()->moveTo(range.start, true);
    }
    else {
        Selection newSelection = Selection(range.start, range.end);
        m_renderer->document()->frame()->selection()->setSelection(newSelection);
    }    
}

VisiblePosition AccessibilityRenderObject::visiblePositionForPoint(const IntPoint& point) const
{
    // convert absolute point to view coordinates
    FrameView* frameView = m_renderer->document()->topDocument()->renderer()->view()->frameView();
    RenderView* renderView = topRenderer();
    Node* innerNode = 0;
    
    // locate the node containing the point
    IntPoint pointResult;
    while (1) {
        IntPoint ourpoint;
        ourpoint = point;
        HitTestRequest request(true, true);
        HitTestResult result(ourpoint);
        renderView->layer()->hitTest(request, result);
        innerNode = result.innerNode();
        if (!innerNode || !innerNode->renderer())
            return VisiblePosition();
        
        pointResult = result.localPoint();
        
        // done if hit something other than a widget
        RenderObject* renderer = innerNode->renderer();
        if (!renderer->isWidget())
            break;
        
        // descend into widget (FRAME, IFRAME, OBJECT...)
        Widget* widget = static_cast<RenderWidget*>(renderer)->widget();
        if (!widget || !widget->isFrameView())
            break;
        Frame* frame = static_cast<FrameView*>(widget)->frame();
        if (!frame)
            break;
        Document* document = frame->document();
        if (!document)
            break;
        renderView = document->renderView();
        frameView = static_cast<FrameView*>(widget);
    }
    
    return innerNode->renderer()->positionForPoint(pointResult);
}

// NOTE: Consider providing this utility method as AX API
VisiblePosition AccessibilityRenderObject::visiblePositionForIndex(unsigned indexValue, bool lastIndexOK) const
{
    if (!isTextControl())
        return VisiblePosition();
    
    // lastIndexOK specifies whether the position after the last character is acceptable
    if (indexValue >= text().length()) {
        if (!lastIndexOK || indexValue > text().length())
            return VisiblePosition();
    }
    VisiblePosition position = visiblePositionForIndex(indexValue);
    position.setAffinity(DOWNSTREAM);
    return position;
}

// NOTE: Consider providing this utility method as AX API
int AccessibilityRenderObject::index(const VisiblePosition& position) const
{
    if (!isTextControl())
        return -1;
    
    Node* node = position.deepEquivalent().node();
    if (!node)
        return -1;
    
    for (RenderObject* renderer = node->renderer(); renderer && renderer->element(); renderer = renderer->parent()) {
        if (renderer == m_renderer)
            return indexForVisiblePosition(position);
    }
    
    return -1;
}

// Given a line number, the range of characters of the text associated with this accessibility
// object that contains the line number.
PlainTextRange AccessibilityRenderObject::doAXRangeForLine(unsigned lineNumber) const
{
    if (!isTextControl())
        return PlainTextRange();
    
    // iterate to the specified line
    VisiblePosition visiblePos = visiblePositionForIndex(0);
    VisiblePosition savedVisiblePos;
    for (unsigned lineCount = lineNumber; lineCount != 0; lineCount -= 1) {
        savedVisiblePos = visiblePos;
        visiblePos = nextLinePosition(visiblePos, 0);
        if (visiblePos.isNull() || visiblePos == savedVisiblePos)
            return PlainTextRange();
    }
    
    // make a caret selection for the marker position, then extend it to the line
    // NOTE: ignores results of selection.modify because it returns false when
    // starting at an empty line.  The resulting selection in that case
    // will be a caret at visiblePos.
    SelectionController selection;
    selection.setSelection(Selection(visiblePos));
    selection.modify(SelectionController::EXTEND, SelectionController::LEFT, LineBoundary);
    selection.modify(SelectionController::EXTEND, SelectionController::RIGHT, LineBoundary);
    
    // calculate the indices for the selection start and end
    VisiblePosition startPosition = selection.selection().visibleStart();
    VisiblePosition endPosition = selection.selection().visibleEnd();
    int index1 = indexForVisiblePosition(startPosition);
    int index2 = indexForVisiblePosition(endPosition);
    
    // add one to the end index for a line break not caused by soft line wrap (to match AppKit)
    if (endPosition.affinity() == DOWNSTREAM && endPosition.next().isNotNull())
        index2 += 1;
    
    // return nil rather than an zero-length range (to match AppKit)
    if (index1 == index2)
        return PlainTextRange();
    
    return PlainTextRange(index1, index2 - index1);
}

// The composed character range in the text associated with this accessibility object that
// is specified by the given index value. This parameterized attribute returns the complete
// range of characters (including surrogate pairs of multi-byte glyphs) at the given index.
PlainTextRange AccessibilityRenderObject::doAXRangeForIndex(unsigned index) const
{
    if (!isTextControl())
        return PlainTextRange();
    
    String elementText = text();
    if (!elementText.length() || index > elementText.length() - 1)
        return PlainTextRange();
    
    return PlainTextRange(index, 1);
}

// A substring of the text associated with this accessibility object that is
// specified by the given character range.
String AccessibilityRenderObject::doAXStringForRange(const PlainTextRange& range) const
{
    if (isPasswordField())
        return String();
    
    if (range.length == 0)
        return "";
    
    if (!isTextControl())
        return String();
    
    String elementText = text();
    if (range.start + range.length > elementText.length())
        return String();
    
    return elementText.substring(range.start, range.length);
}

// The bounding rectangle of the text associated with this accessibility object that is
// specified by the given range. This is the bounding rectangle a sighted user would see
// on the display screen, in pixels.
IntRect AccessibilityRenderObject::doAXBoundsForRange(const PlainTextRange& range) const
{
    if (isTextControl())
        return boundsForVisiblePositionRange(visiblePositionRangeForRange(range));
    return IntRect();
}

AccessibilityObject* AccessibilityRenderObject::doAccessibilityHitTest(const IntPoint& point) const
{
    if (!m_renderer || !m_renderer->hasLayer())
        return 0;
    
    RenderLayer* layer = toRenderBox(m_renderer)->layer();
     
    HitTestRequest request(true, true);
    HitTestResult hitTestResult = HitTestResult(point);
    layer->hitTest(request, hitTestResult);
    if (!hitTestResult.innerNode())
        return 0;
    Node* node = hitTestResult.innerNode()->shadowAncestorNode();
    RenderObject* obj = node->renderer();
    if (!obj)
        return 0;
    
    AccessibilityObject *result = obj->document()->axObjectCache()->get(obj);

    if (obj->isListBox())
        return static_cast<AccessibilityListBox*>(result)->doAccessibilityHitTest(point);
        
    if (result->accessibilityIsIgnored())
        result = result->parentObjectUnignored();

    return result;
}

AccessibilityObject* AccessibilityRenderObject::focusedUIElement() const
{
    // get the focused node in the page
    Page* page = m_renderer->document()->page();
    if (!page)
        return 0;
    
    Document* focusedDocument = page->focusController()->focusedOrMainFrame()->document();
    Node* focusedNode = focusedDocument->focusedNode();
    if (!focusedNode)
        focusedNode = focusedDocument;
    
    RenderObject* focusedNodeRenderer = focusedNode->renderer();
    if (!focusedNodeRenderer)
        return 0;
    
    AccessibilityObject* obj = focusedNodeRenderer->document()->axObjectCache()->get(focusedNodeRenderer);
    
    if (obj->shouldFocusActiveDescendant()) {
        if (AccessibilityObject* descendant = obj->activeDescendant())
            obj = descendant;
    }
    
    // the HTML element, for example, is focusable but has an AX object that is ignored
    if (obj->accessibilityIsIgnored())
        obj = obj->parentObjectUnignored();
    
    return obj;
}

bool AccessibilityRenderObject::shouldFocusActiveDescendant() const
{
    switch (ariaRoleAttribute()) {
    case GroupRole:
    case ComboBoxRole:
    case ListBoxRole:
    case MenuRole:
    case MenuBarRole:
    case RadioGroupRole:
    case RowRole:
    case PopUpButtonRole:
    case ProgressIndicatorRole:
    case ToolbarRole:
    case OutlineRole:
    /* FIXME: replace these with actual roles when they are added to AccessibilityRole
    composite
    alert
    alertdialog
    grid
    status
    timer
    tree
    */
        return true;
    default:
        return false;
    }
}

AccessibilityObject* AccessibilityRenderObject::activeDescendant() const
{
    if (renderer()->element() && !renderer()->element()->isElementNode())
        return 0;
    Element* element = static_cast<Element*>(renderer()->element());
        
    String activeDescendantAttrStr = element->getAttribute(aria_activedescendantAttr).string();
    if (activeDescendantAttrStr.isNull() || activeDescendantAttrStr.isEmpty())
        return 0;
    
    Element* target = renderer()->document()->getElementById(activeDescendantAttrStr);
    if (!target)
        return 0;
    
    AccessibilityObject* obj = renderer()->document()->axObjectCache()->get(target->renderer());
    if (obj->isAccessibilityRenderObject())
    // an activedescendant is only useful if it has a renderer, because that's what's needed to post the notification
        return obj;
    return 0;
}


void AccessibilityRenderObject::handleActiveDescendantChanged()
{
    Element* element = static_cast<Element*>(renderer()->element());
    if (!element)
        return;
    Document* doc = renderer()->document();
    if (!doc->frame()->selection()->isFocusedAndActive() || doc->focusedNode() != element)
        return; 
    AccessibilityRenderObject* activedescendant = static_cast<AccessibilityRenderObject*>(activeDescendant());
    
    if (activedescendant && shouldFocusActiveDescendant())
        doc->axObjectCache()->postNotificationToElement(activedescendant->renderer(), "AXFocusedUIElementChanged");
}


AccessibilityObject* AccessibilityRenderObject::observableObject() const
{
    for (RenderObject* renderer = m_renderer; renderer && renderer->element(); renderer = renderer->parent()) {
        if (renderer->isTextField() || renderer->isTextArea())
            return renderer->document()->axObjectCache()->get(renderer);
    }
    
    return 0;
}
    
typedef HashMap<String, AccessibilityRole, CaseFoldingHash> ARIARoleMap;

static const ARIARoleMap& createARIARoleMap()
{
    struct RoleEntry {
        String ariaRole;
        AccessibilityRole webcoreRole;
    };

    const RoleEntry roles[] = {
        { "button", ButtonRole },
        { "checkbox", CheckBoxRole },
        { "group", GroupRole },
        { "heading", HeadingRole },
        { "img", ImageRole },
        { "link", WebCoreLinkRole },
        { "listbox", ListBoxRole },
        // "option" isn't here because it may map to different roles depending on the parent element's role
        { "menu", MenuRole },
        { "menubar", GroupRole },
        // "menuitem" isn't here because it may map to different roles depending on the parent element's role
        { "menuitemcheckbox", MenuItemRole },
        { "menuitemradio", MenuItemRole },
        { "progressbar", ProgressIndicatorRole },
        { "radio", RadioButtonRole },
        { "range", SliderRole },
        { "slider", SliderRole },
        { "spinbutton", ProgressIndicatorRole },
        { "textbox", TextAreaRole }
    };
    ARIARoleMap& roleMap = *new ARIARoleMap;
        
    const unsigned numRoles = sizeof(roles) / sizeof(roles[0]);
    for (unsigned i = 0; i < numRoles; ++i)
        roleMap.set(roles[i].ariaRole, roles[i].webcoreRole);
    return roleMap;
}

static AccessibilityRole ariaRoleToWebCoreRole(String value)
{
    ASSERT(!value.isEmpty() && !value.isNull());
    static const ARIARoleMap& roleMap = createARIARoleMap();
    return roleMap.get(value);
}

AccessibilityRole AccessibilityRenderObject::determineAriaRoleAttribute() const
{
    String ariaRole = getAttribute(roleAttr).string();
    if (ariaRole.isNull() || ariaRole.isEmpty())
        return UnknownRole;
    
    AccessibilityRole role = ariaRoleToWebCoreRole(ariaRole);
    if (role)
        return role;
    // selects and listboxes both have options as child roles, but they map to different roles within WebCore
    if (equalIgnoringCase(ariaRole,"option")) {
        if (parentObjectUnignored()->ariaRoleAttribute() == MenuRole)
            return MenuItemRole;
        if (parentObjectUnignored()->ariaRoleAttribute() == ListBoxRole)
            return ListBoxOptionRole;
    }
    // an aria "menuitem" may map to MenuButton or MenuItem depending on its parent
    if (equalIgnoringCase(ariaRole,"menuitem")) {
        if (parentObjectUnignored()->ariaRoleAttribute() == GroupRole)
            return MenuButtonRole;
        if (parentObjectUnignored()->ariaRoleAttribute() == MenuRole)
            return MenuItemRole;
    }
    
    return UnknownRole;
}

void AccessibilityRenderObject::setAriaRole()
{
    m_ariaRole = determineAriaRoleAttribute();
}

AccessibilityRole AccessibilityRenderObject::ariaRoleAttribute() const
{
    return m_ariaRole;
}

AccessibilityRole AccessibilityRenderObject::roleValue() const
{
    if (!m_renderer)
        return UnknownRole;
    
    Node* node = m_renderer->element();
    AccessibilityRole ariaRole = ariaRoleAttribute();
    if (ariaRole != UnknownRole)
        return ariaRole;
    
    if (node && node->isLink()) {
        if (m_renderer->isImage())
            return ImageMapRole;
        return WebCoreLinkRole;
    }
    if (m_renderer->isListMarker())
        return ListMarkerRole;
    if (node && node->hasTagName(buttonTag))
        return ButtonRole;
    if (m_renderer->isText())
        return StaticTextRole;
    if (m_renderer->isImage()) {
        if (node && node->hasTagName(inputTag))
            return ButtonRole;
        return ImageRole;
    }
    if (m_renderer->isRenderView())
        return WebAreaRole;
    
    if (m_renderer->isTextField())
        return TextFieldRole;
    
    if (m_renderer->isTextArea())
        return TextAreaRole;
    
    if (node && node->hasTagName(inputTag)) {
        HTMLInputElement* input = static_cast<HTMLInputElement*>(node);
        if (input->inputType() == HTMLInputElement::CHECKBOX)
            return CheckBoxRole;
        if (input->inputType() == HTMLInputElement::RADIO)
            return RadioButtonRole;
        if (input->isTextButton())
            return ButtonRole;
    }

    if (node && node->hasTagName(buttonTag))
        return ButtonRole;

    if (isFileUploadButton())
        return ButtonRole;
    
    if (m_renderer->isMenuList())
        return PopUpButtonRole;
    
    if (headingLevel(m_renderer->element()) != 0)
        return HeadingRole;
    
    if (node && node->hasTagName(ddTag))
        return DefinitionListDefinitionRole;
    
    if (node && node->hasTagName(dtTag))
        return DefinitionListTermRole;

    if (m_renderer->isBlockFlow() || (node && node->hasTagName(labelTag)))
        return GroupRole;
    
    return UnknownRole;
}

bool AccessibilityRenderObject::isPresentationalChildOfAriaRole() const
{
    // Walk the parent chain looking for a parent that has presentational children
    AccessibilityObject* parent;
    for (parent = parentObject(); parent && !parent->ariaRoleHasPresentationalChildren(); parent = parent->parentObject())
        ;
    return parent;
}
    
bool AccessibilityRenderObject::ariaRoleHasPresentationalChildren() const
{
    switch (m_ariaRole) {
    case ButtonRole:
    case SliderRole:
    case ImageRole:
    case ProgressIndicatorRole:
    //case SeparatorRole:
        return true;
    default:
        return false;
    }
}

bool AccessibilityRenderObject::canSetFocusAttribute() const
{
    // NOTE: It would be more accurate to ask the document whether setFocusedNode() would
    // do anything.  For example, it setFocusedNode() will do nothing if the current focused
    // node will not relinquish the focus.
    if (!m_renderer->element() || !m_renderer->element()->isEnabled())
        return false;
    
    switch (roleValue()) {
        case WebCoreLinkRole:
        case ImageMapLinkRole:
        case TextFieldRole:
        case TextAreaRole:
        case ButtonRole:
        case PopUpButtonRole:
        case CheckBoxRole:
        case RadioButtonRole:
            return true;
        default:
            return false;
    }
}

bool AccessibilityRenderObject::canSetValueAttribute() const
{
    if (equalIgnoringCase(getAttribute(aria_readonlyAttr).string(), "true"))
        return false;

    if (isWebArea()) 
        return !isReadOnly();

    return isTextControl() || isProgressIndicator() || isSlider();
}

bool AccessibilityRenderObject::canSetTextRangeAttributes() const
{
    return isTextControl();
}

void AccessibilityRenderObject::childrenChanged()
{
    // this method is meant as a quick way of marking dirty
    // a portion of the accessibility tree
    
    markChildrenDirty();
    
    // this object may not be accessible (and thus may not appear
    // in the hierarchy), which means we need to go up the parent
    // chain and mark the parent's dirty. Ideally, we would want
    // to only access the next object that is not ignored, but
    // asking an element if it's ignored can lead to an examination of the
    // render tree which is dangerous.
    for (AccessibilityObject* parent = parentObject(); parent; parent = parent->parentObject()) {
        if (parent->isAccessibilityRenderObject())
            static_cast<AccessibilityRenderObject *>(parent)->markChildrenDirty();
    }
}
    
bool AccessibilityRenderObject::canHaveChildren() const
{
    if (!m_renderer)
        return false;
    
    // Elements that should not have children
    switch (roleValue()) {
        case ImageRole:
        case ButtonRole:
        case PopUpButtonRole:
        case CheckBoxRole:
        case RadioButtonRole:
            return false;
        default:
            return true;
    }
}

const AccessibilityObject::AccessibilityChildrenVector& AccessibilityRenderObject::children()
{
    if (m_childrenDirty) {
        clearChildren();        
        m_childrenDirty = false;
    }
    
    if (!m_haveChildren)
        addChildren();
    return m_children;
}

void AccessibilityRenderObject::addChildren()
{
    // If the need to add more children in addition to existing children arises, 
    // childrenChanged should have been called, leaving the object with no children.
    ASSERT(!m_haveChildren); 
    
    // nothing to add if there is no RenderObject
    if (!m_renderer)
        return;
    
    m_haveChildren = true;
    
    if (!canHaveChildren())
        return;
    
    // add all unignored acc children
    for (RefPtr<AccessibilityObject> obj = firstChild(); obj; obj = obj->nextSibling()) {
        if (obj->accessibilityIsIgnored()) {
            if (!obj->hasChildren())
                obj->addChildren();
            AccessibilityChildrenVector children = obj->children();
            unsigned length = children.size();
            for (unsigned i = 0; i < length; ++i)
                m_children.append(children[i]);
        } else
            m_children.append(obj);
    }
    
    // for a RenderImage, add the <area> elements as individual accessibility objects
    if (m_renderer->isRenderImage()) {
        HTMLMapElement* map = static_cast<RenderImage*>(m_renderer)->imageMap();
        if (map) {
            for (Node* current = map->firstChild(); current; current = current->traverseNextNode(map)) {

                // add an <area> element for this child if it has a link
                if (current->isLink()) {
                    AccessibilityImageMapLink* areaObject = static_cast<AccessibilityImageMapLink*>(m_renderer->document()->axObjectCache()->get(ImageMapLinkRole));
                    areaObject->setHTMLAreaElement(static_cast<HTMLAreaElement*>(current));
                    areaObject->setHTMLMapElement(map);
                    areaObject->setParent(this);

                    m_children.append(areaObject);
                }
            }
        }
    }
}

void AccessibilityRenderObject::ariaListboxSelectedChildren(AccessibilityChildrenVector& result)
{
    AccessibilityObject* child = firstChild();
    bool isMultiselectable = false;
    
    Element* element = static_cast<Element*>(renderer()->element());        
    if (!element || !element->isElementNode()) // do this check to ensure safety of static_cast above
        return;

    String multiselectablePropertyStr = element->getAttribute("aria-multiselectable").string();
    isMultiselectable = equalIgnoringCase(multiselectablePropertyStr, "true");
    
    while (child) {
        // every child should have aria-role option, and if so, check for selected attribute/state
        AccessibilityRole ariaRole = child->ariaRoleAttribute();
        RenderObject* childRenderer = 0;
        if (child->isAccessibilityRenderObject())
            childRenderer = static_cast<AccessibilityRenderObject*>(child)->renderer();
        if (childRenderer && ariaRole == ListBoxOptionRole) {
            Element* childElement = static_cast<Element*>(childRenderer->element());
            if (childElement && childElement->isElementNode()) { // do this check to ensure safety of static_cast above
                String selectedAttrString = childElement->getAttribute("aria-selected").string();
                if (equalIgnoringCase(selectedAttrString, "true")) {
                    result.append(child);
                    if (isMultiselectable)
                        return;
                }
            }
        }
        child = child->nextSibling(); 
    }
}

void AccessibilityRenderObject::selectedChildren(AccessibilityChildrenVector& result)
{
    ASSERT(result.isEmpty());

    // only listboxes should be asked for their selected children. 
    if (ariaRoleAttribute() != ListBoxRole) { // native list boxes would be AccessibilityListBoxes, so only check for aria list boxes
        ASSERT_NOT_REACHED(); 
        return;
    }
    return ariaListboxSelectedChildren(result);
}

void AccessibilityRenderObject::ariaListboxVisibleChildren(AccessibilityChildrenVector& result)      
{
    if (!hasChildren())
        addChildren();
    
    unsigned length = m_children.size();
    for (unsigned i = 0; i < length; i++) {
        if (!m_children[i]->isOffScreen())
            result.append(m_children[i]);
    }
}

void AccessibilityRenderObject::visibleChildren(AccessibilityChildrenVector& result)
{
    ASSERT(result.isEmpty());
        
    // only listboxes are asked for their visible children. 
    if (ariaRoleAttribute() != ListBoxRole) { // native list boxes would be AccessibilityListBoxes, so only check for aria list boxes
        ASSERT_NOT_REACHED();
        return;
    }
    return ariaListboxVisibleChildren(result);
}
 
void AccessibilityRenderObject::removeAXObjectID()
{
    if (!m_id)
        return;
#if PLATFORM(MAC)
    m_renderer->document()->axObjectCache()->removeAXID(this);
#endif
}   
    
const String& AccessibilityRenderObject::actionVerb() const
{
    DEFINE_STATIC_LOCAL(const String, noAction, ());
    return noAction;
}
     
void AccessibilityRenderObject::updateBackingStore()
{
    if (!m_renderer)
        return;
    m_renderer->view()->layoutIfNeeded();
}    
    
} // namespace WebCore

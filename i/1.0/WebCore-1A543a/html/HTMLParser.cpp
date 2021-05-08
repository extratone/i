/*
    This file is part of the KDE libraries

    Copyright (C) 1997 Martin Jones (mjones@kde.org)
              (C) 1997 Torben Weis (weis@kde.org)
              (C) 1999,2001 Lars Knoll (knoll@kde.org)
              (C) 2000,2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#include "HTMLParser.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "Comment.h"
#include "DocumentFragment.h"
#include "Frame.h"
#include "HTMLAnchorElement.h"
#include "HTMLBodyElement.h"
#include "HTMLCanvasElement.h"
#include "HTMLDivElement.h"
#include "HTMLDListElement.h"
#include "HTMLElementFactory.h"
#include "HTMLFormElement.h"
#include "HTMLHeadElement.h"
#include "HTMLHRElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLIsIndexElement.h"
#include "HTMLMapElement.h"
#include "HTMLNames.h"
#include "HTMLTableCellElement.h"
#include "HTMLTableRowElement.h"
#include "HTMLTableSectionElement.h"
#include "HTMLTokenizer.h"
#include "LocalizedStrings.h"

#include "Text.h"
#include "WebCoreTelephoneParser.h"

namespace WebCore {

using namespace HTMLNames;

const UChar nonBreakingSpace = 0xa0;

/**
 * @internal
 */

    class RefNonDocNodePtr
    {
    public:
        RefNonDocNodePtr() : m_ptr(0) {}
        RefNonDocNodePtr(Node* ptr) : m_ptr(ptr), m_isDoc(ptr->isDocumentNode()) { if (!m_isDoc && ptr) ptr->ref(); }
        RefNonDocNodePtr(const RefNonDocNodePtr& o) : m_ptr(o.m_ptr), m_isDoc(o.m_isDoc) { if (!m_isDoc && m_ptr) m_ptr->ref(); }

        ~RefNonDocNodePtr() { if (!m_isDoc && m_ptr) m_ptr->deref(); }
        
        Node *get() const { return m_ptr; }
        
        Node& operator*() const { return *m_ptr; }
        Node *operator->() const { return m_ptr; }
        
        bool operator!() const { return !m_ptr; }
    
        // This conversion operator allows implicit conversion to bool but not to other integer types.
        typedef Node* (RefNonDocNodePtr::*UnspecifiedBoolType)() const;
        operator UnspecifiedBoolType() const { return m_ptr ? &RefNonDocNodePtr::get : 0; }
        
        RefNonDocNodePtr& operator=(const RefNonDocNodePtr&);
        RefNonDocNodePtr& operator=(Node*);
        RefNonDocNodePtr& operator=(RefPtr<Node>&);

    private:
        Node* m_ptr;
        bool m_isDoc;
    };
    
    inline RefNonDocNodePtr& RefNonDocNodePtr::operator=(const RefNonDocNodePtr& o)
    {
        Node* optr = o.get();
        if (!o.m_isDoc && optr)
            optr->ref();
        Node* ptr = m_ptr;
        m_ptr = optr;
        if (!m_isDoc && ptr)
            ptr->deref();
        m_isDoc = o.m_isDoc;
        return *this;
    }
    
    inline RefNonDocNodePtr& RefNonDocNodePtr::operator=(Node* optr)
    {
        bool o_isDoc = optr->isDocumentNode();
        if (!o_isDoc && optr)
            optr->ref();
        Node* ptr = m_ptr;
        m_ptr = optr;
        if (!m_isDoc && ptr)
            ptr->deref();
        m_isDoc = o_isDoc;
        return *this;
    }

    inline RefNonDocNodePtr& RefNonDocNodePtr::operator=(RefPtr<Node>& o)
    {
        Node* optr = o.get();
        bool o_isDoc = optr->isDocumentNode();
        if (!o_isDoc && optr)
            optr->ref();
        Node* ptr = m_ptr;
        m_ptr = optr;
        if (!m_isDoc && ptr)
            ptr->deref();
        m_isDoc = o_isDoc;
        return *this;
    }

    inline bool operator==(const RefNonDocNodePtr& a, const RefNonDocNodePtr& b)
    { 
        return a.get() == b.get(); 
    }

    inline bool operator==(const RefNonDocNodePtr& a, Node* b)
    { 
        return a.get() == b; 
    }
    
    inline bool operator==(Node* a, const RefNonDocNodePtr& b) 
    {
        return a == b.get(); 
    }
    
    inline bool operator!=(const RefNonDocNodePtr& a, const RefNonDocNodePtr& b)
    { 
        return a.get() != b.get(); 
    }

    inline bool operator!=(const RefNonDocNodePtr& a, Node* b)
    {
        return a.get() != b; 
    }

    inline bool operator!=(Node* a, const RefNonDocNodePtr& b)
    { 
        return a != b.get(); 
    }
    


class HTMLStackElem
{
public:
    HTMLStackElem(const AtomicString& _tagName,
                  int _level,
                  Node *_node,
                  HTMLStackElem * _next
        )
        :
        tagName(_tagName),
        level(_level),
        strayTableContent(false),
        node(_node),
        next(_next)
        { }

    AtomicString tagName;
    int level;
    bool strayTableContent;
    RefNonDocNodePtr node;
    HTMLStackElem* next;
};

/**
 * @internal
 *
 * The parser parses tokenized input into the document, building up the
 * document tree. If the document is wellformed, parsing it is
 * straightforward.
 * Unfortunately, people can't write wellformed HTML documents, so the parser
 * has to be tolerant about errors.
 *
 * We have to take care of the following error conditions:
 * 1. The element being added is explicitly forbidden inside some outer tag.
 *    In this case we should close all tags up to the one, which forbids
 *    the element, and add it afterwards.
 * 2. We are not allowed to add the element directly. It could be, that
 *    the person writing the document forgot some tag inbetween (or that the
 *    tag inbetween is optional...) This could be the case with the following
 *    tags: HTML HEAD BODY TBODY TR TD LI (did I forget any?)
 * 3. We wan't to add a block element inside to an inline element. Close all
 *    inline elements up to the next higher block element.
 * 4. If this doesn't help close elements, until we are allowed to add the
 *    element or ignore the tag.
 *
 */
HTMLParser::HTMLParser(Document* doc) 
    : document(doc)
    , current(0)
    , currentIsReferenced(false)
    , blockStack(0)
    , m_fragment(false)
{
    reset();
}

HTMLParser::HTMLParser(DocumentFragment* frag)
    : document(frag->document())
    , current(0)
    , currentIsReferenced(false)
    , blockStack(0)
    , m_fragment(true)
{
    reset();
    setCurrent(frag);
    inBody = true;
}

HTMLParser::~HTMLParser()
{
    freeBlock();

    setCurrent(0);
}

void HTMLParser::reset()
{
    setCurrent(doc());

    freeBlock();

    inBody = false;
    haveFrameSet = false;
    haveContent = false;
    inSelect = false;
    inStrayTableContent = 0;
    
    form = 0;
    map = 0;
    head = 0;
    end = false;
    isindex = 0;
    
    discard_until = nullAtom;
}

void HTMLParser::setCurrent(Node *newCurrent) 
{
    bool newCurrentIsReferenced = newCurrent && newCurrent != doc();
    if (newCurrentIsReferenced) 
        newCurrent->ref(); 
    if (currentIsReferenced) 
        current->deref(); 
    current = newCurrent;
    currentIsReferenced = newCurrentIsReferenced;
}

PassRefPtr<Node> HTMLParser::parseToken(Token *t)
{
    if (!discard_until.isNull()) {
        if (t->tagName == discard_until && !t->beginTag)
            discard_until = nullAtom;

        // do not skip </iframe>
        if (!discard_until.isNull() || (current->localName() != t->tagName))
            return 0;
    }

    // Apparently some sites use </br> instead of <br>.  Be compatible with IE and Firefox and treat this like <br>.
    if (t->isCloseTag(brTag) && doc()->inCompatMode())
        t->beginTag = true;

    if (!t->beginTag) {
        processCloseTag(t);
        return 0;
    }

    // ignore spaces, if we're not inside a paragraph or other inline code
    if (t->tagName == textAtom && t->text) {
        if (inBody && !skipMode() && current->localName() != styleTag && current->localName() != titleTag && 
            current->localName() != scriptTag && !t->text->containsOnlyWhitespace()) 
            haveContent = true;
    }

    RefPtr<Node> n = getNode(t);
    // just to be sure, and to catch currently unimplemented stuff
    if (!n)
        return 0;

    // set attributes
    if (n->isHTMLElement()) {
        HTMLElement* e = static_cast<HTMLElement*>(n.get());
        e->setAttributeMap(t->attrs.get());

        // take care of optional close tags
        if (e->endTagRequirement() == TagStatusOptional)
            popBlock(t->tagName);
            
        if (isHeaderTag(t->tagName))
            // Do not allow two header tags to be nested if the intervening tags are inlines.
            popNestedHeaderTag();
    }

    if (!insertNode(n.get(), t->flat)) {
        // we couldn't insert the node

        if (n->isElementNode()) {
            Element* e = static_cast<Element*>(n.get());
            e->setAttributeMap(0);
        }

        if (map == n)
            map = 0;

        if (form == n)
            form = 0;

        if (head == n)
            head = 0;

        return 0;
    }
    return n;
}

static bool isTableSection(Node* n)
{
    return n->hasTagName(tbodyTag) || n->hasTagName(tfootTag) || n->hasTagName(theadTag);
}

static bool isTablePart(Node* n)
{
    return n->hasTagName(trTag) || n->hasTagName(tdTag) || n->hasTagName(thTag) ||
           isTableSection(n);
}

static bool isTableRelated(Node* n)
{
    return n->hasTagName(tableTag) || isTablePart(n);
}

Text *HTMLParser::parseNextPhoneNumber(Text *inputText)

{
    Text *outputText = nil;
    String s = inputText->toString();

    //printf("Match candidate: %s\n", inputText->toString().deprecatedString().latin1());

    unsigned len = s.length();
    int phoneStartPos, phoneEndPos;
    WebCoreFindTelephoneNumber(s.characters(), len, &phoneStartPos, &phoneEndPos);
    if (phoneStartPos != -1 && phoneEndPos != -1){
        String phone = s.substring(phoneStartPos, phoneEndPos - phoneStartPos + 1);
        HTMLAnchorElement *myAnchor = new HTMLAnchorElement(doc());
        if (current->childAllowed(myAnchor)) {
            ExceptionCode ec = 0;
            Text *phoneText = (inputText)->splitText(phoneStartPos, ec);
            Text *afterPhoneText = phoneText->splitText(phoneEndPos - phoneStartPos + 1, ec);
            
            // printf("\tbefore: %s\n", inputText->toString().deprecatedString().latin1());
            // printf("\ttext: %s\n", phoneText->toString().deprecatedString().latin1());
            // printf("\tafter: %s\n\n", afterPhoneText->toString().deprecatedString().latin1());
            
            myAnchor->addChild(phoneText);
            String phoneLink = "tel:";
            phoneLink.append(phoneText->toString());
            myAnchor->setHref(phoneLink);
            
            current->appendChild(inputText, ec);
            current->appendChild(myAnchor, ec);
            outputText = afterPhoneText;
            
        } else {
            delete myAnchor;
        }                                            
    }
    return outputText;
}

Node *HTMLParser::parseTelephoneNumbers(Node *inputNode)
{
    Node *outputNode = inputNode;
    if (outputNode->isTextNode()) {
        bool shouldParseTelephoneNumbers = true;
        Node *parentNode = current;
        do {
            if (parentNode->isLink()  || parentNode->isCommentNode() || parentNode->hasTagName(scriptTag) || (parentNode->isHTMLElement() && ((HTMLElement *)parentNode)->isGenericFormElement()) || parentNode->hasTagName(styleTag) || parentNode->hasTagName(ttTag) || parentNode->hasTagName(preTag) || parentNode->hasTagName(codeTag)) {
                shouldParseTelephoneNumbers = false;
                break;
            }
            parentNode = parentNode->parentNode();
        } while (parentNode);
        if (shouldParseTelephoneNumbers) {
            if (current->isHTMLElement()) {
                Node *linkifiedNodeRemnants = nil;
                do {
                    linkifiedNodeRemnants = parseNextPhoneNumber(static_cast<Text *>(outputNode));
                    if (linkifiedNodeRemnants != nil)
                        outputNode = linkifiedNodeRemnants;
                } while (linkifiedNodeRemnants != nil);
            }
        }
    }            
    return outputNode;
}

bool HTMLParser::insertNode(Node *n, bool flat)
{
    RefPtr<Node> protectNode(n);

    const AtomicString& localName = n->localName();
    //printf("insertNode candidate: %s\n", n->toString().deprecatedString().latin1());
    int tagPriority = n->isHTMLElement() ? static_cast<HTMLElement*>(n)->tagPriority() : 0;

    bool parentAttached = current->attached();    
    if (parentAttached && Document::isTelephoneNumberParsingEnabled()) {
        n = parseTelephoneNumbers(n);
    }
    
    // let's be stupid and just try to insert it.
    // this should work if the document is well-formed
    Node *newNode = current->addChild(n);
    if (newNode) {
        // don't push elements without end tags (e.g., <img>) on the stack
        if (tagPriority > 0 && !flat) {
            pushBlock(localName, tagPriority);
            if (newNode == current)
                popBlock(localName);
            else
                setCurrent(newNode);
            if (parentAttached && !n->attached() && !m_fragment)
                n->attach();
        } else {
            if (parentAttached && !n->attached() && !m_fragment)
                n->attach();
            n->closeRenderer();
        }

        return true;
    } else
        return handleError(n, flat, localName, tagPriority); // Try to handle the error.
}

bool HTMLParser::handleError(Node* n, bool flat, const AtomicString& localName, int tagPriority)
{
    // Error handling code.  This is just ad hoc handling of specific parent/child combinations.
    HTMLElement *e;
    bool handled = false;

    // 1. Check out the element's tag name to decide how to deal with errors.
    if (n->isTextNode()) {
        if (current->hasTagName(selectTag))
            return false;
    } else if (n->isHTMLElement()) {
        HTMLElement* h = static_cast<HTMLElement*>(n);
        if (h->hasLocalName(trTag) || h->hasLocalName(thTag) || h->hasLocalName(tdTag)) {
            if (inStrayTableContent && !isTableRelated(current)) {
                // pop out to the nearest enclosing table-related tag.
                while (blockStack && !isTableRelated(current))
                    popOneBlock();
                return insertNode(n);
            }
        } else if (h->hasLocalName(headTag)) {
            if (!current->isDocumentNode() && !current->hasTagName(htmlTag))
                return false;
        } else if (h->hasLocalName(metaTag) || h->hasLocalName(linkTag) || h->hasLocalName(baseTag)) {
            if (!head)
                createHead();
            if (head) {
                if (head->addChild(n)) {
                    if (!n->attached() && !m_fragment)
                        n->attach();
                    return true;
                } else
                    return false;
            }
        } else if (h->hasLocalName(htmlTag)) {
            if (!current->isDocumentNode() ) {
                if (doc()->firstChild()->hasTagName(htmlTag)) {
                    // we have another <HTML> element.... apply attributes to existing one
                    // make sure we don't overwrite already existing attributes
                    NamedAttrMap *map = static_cast<Element*>(n)->attributes(true);
                    Element *existingHTML = static_cast<Element*>(doc()->firstChild());
                    NamedAttrMap *bmap = existingHTML->attributes(false);
                    for (unsigned l = 0; map && l < map->length(); ++l) {
                        Attribute* it = map->attributeItem(l);
                        if (!bmap->getAttributeItem(it->name()))
                            existingHTML->setAttribute(it->name(), it->value());
                    }
                }
                return false;
            }
        } else if (h->hasLocalName(titleTag) || h->hasLocalName(styleTag)) {
            if (!head)
                createHead();
            if (head) {
                Node *newNode = head->addChild(n);
                if (newNode) {
                    pushBlock(localName, tagPriority);
                    setCurrent(newNode);
                    if (!n->attached() && !m_fragment)
                        n->attach();
                } else {
                    setSkipMode(styleTag);
                    return false;
                }
                return true;
            } else if(inBody) {
                setSkipMode(styleTag);
                return false;
            }
        } else if (h->hasLocalName(bodyTag)) {
            if (inBody && doc()->body()) {
                // we have another <BODY> element.... apply attributes to existing one
                // make sure we don't overwrite already existing attributes
                // some sites use <body bgcolor=rightcolor>...<body bgcolor=wrongcolor>
                NamedAttrMap *map = static_cast<Element*>(n)->attributes(true);
                Element *existingBody = doc()->body();
                NamedAttrMap *bmap = existingBody->attributes(false);
                for (unsigned l = 0; map && l < map->length(); ++l) {
                    Attribute* it = map->attributeItem(l);
                    if (!bmap->getAttributeItem(it->name()))
                        existingBody->setAttribute(it->name(), it->value());
                }
                return false;
            }
            else if (!current->isDocumentNode())
                return false;
        } else if (h->hasLocalName(inputTag)) {
            if (equalIgnoringCase(h->getAttribute(typeAttr), "hidden") && form) {
                form->addChild(n);
                if (!n->attached() && !m_fragment)
                    n->attach();
                return true;
            }
        } else if (h->hasLocalName(ddTag) || h->hasLocalName(dtTag)) {
            e = new HTMLDListElement(document);
            if (insertNode(e)) {
                insertNode(n);
                return true;
            }
        } else if (h->hasLocalName(areaTag)) {
            if (map) {
                map->addChild(n);
                if (!n->attached() && !m_fragment)
                    n->attach();
                handled = true;
                return true;
            }
            return false;
        } else if (h->hasLocalName(captionTag)) {
            if (isTablePart(current)) {
                Node* tsection = current;
                if (current->hasTagName(trTag))
                    tsection = current->parent();
                else if (current->hasTagName(tdTag) || current->hasTagName(thTag))
                    tsection = current->parent()->parent();
                Node* table = tsection->parent();
                ExceptionCode ec = 0;
                table->insertBefore(n, tsection, ec);
                pushBlock(localName, tagPriority);
                setCurrent(n);
                inStrayTableContent++;
                blockStack->strayTableContent = true;
                return true;
            }
        } else if (h->hasLocalName(theadTag) || h->hasLocalName(tbodyTag) ||
                   h->hasLocalName(tfootTag) || h->hasLocalName(colgroupTag)) {
            if (isTableRelated(current)) {
                while (blockStack && isTablePart(current))
                    popOneBlock();
                return insertNode(n);
            }
        }
    }
    
    // 2. Next we examine our currently active element to do some further error handling.
    if (current->isHTMLElement()) {
        HTMLElement* h = static_cast<HTMLElement*>(current);
        const AtomicString& currentTagName = current->localName();
        if (h->hasLocalName(htmlTag)) {
            HTMLElement* elt = n->isHTMLElement() ? static_cast<HTMLElement*>(n) : 0;
            if (elt && (elt->hasLocalName(scriptTag) || elt->hasLocalName(styleTag) ||
                elt->hasLocalName(metaTag) || elt->hasLocalName(linkTag) ||
                elt->hasLocalName(objectTag) || elt->hasLocalName(embedTag) ||
                elt->hasLocalName(titleTag) || elt->hasLocalName(isindexTag) ||
                elt->hasLocalName(baseTag))) {
                if (!head) {
                    head = new HTMLHeadElement(document);
                    e = head;
                    insertNode(e);
                    handled = true;
                }
            } else {
                if (n->isTextNode()) {
                    Text *t = static_cast<Text *>(n);
                    if (t->containsOnlyWhitespace())
                        return false;
                }
                if (!haveFrameSet) {
                    e = new HTMLBodyElement(document);
                    startBody();
                    insertNode(e);
                    handled = true;
                }
            }
        } else if (h->hasLocalName(headTag)) {
            if (n->hasTagName(htmlTag))
                return false;
            else {
                // This means the body starts here...
                if (!haveFrameSet) {
                    popBlock(currentTagName);
                    e = new HTMLBodyElement(document);
                    startBody();
                    insertNode(e);
                    handled = true;
                }
            }
        } else if (h->hasLocalName(addressTag) || h->hasLocalName(dlTag) || h->hasLocalName(dtTag)
                   || h->hasLocalName(fontTag) || h->hasLocalName(styleTag) || h->hasLocalName(titleTag)) {
            popBlock(currentTagName);
            handled = true;
        } else if (h->hasLocalName(captionTag)) {
            // Illegal content in a caption. Close the caption and try again.
            popBlock(currentTagName);
            if (isTablePart(n))
                return insertNode(n, flat);
        } else if (h->hasLocalName(tableTag) || h->hasLocalName(trTag) || isTableSection(h)) {
            if (n->hasTagName(tableTag)) {
                popBlock(localName); // end the table
                handled = true;      // ...and start a new one
            } else {
                bool possiblyMoveStrayContent = true;
                ExceptionCode ec = 0;
                if (n->isTextNode()) {
                    Text *t = static_cast<Text *>(n);
                    if (t->containsOnlyWhitespace())
                        return false;
                    StringImpl *i = t->string();
                    unsigned int pos = 0;
                    while (pos < i->length() && ((*i)[pos] == ' ' || (*i)[pos] == nonBreakingSpace))
                        pos++;
                    if (pos == i->length())
                        possiblyMoveStrayContent = false;
                }
                if (possiblyMoveStrayContent) {
                    Node *node = current;
                    Node *parent = node->parentNode();
                    // A script may have removed the current node's parent from the DOM
                    // http://bugzilla.opendarwin.org/show_bug.cgi?id=7137
                    // FIXME: we should do real recovery here and re-parent with the correct node.
                    if (!parent)
                        return false;
                    Node *grandparent = parent->parentNode();

                    if (n->isTextNode() ||
                        (h->hasLocalName(trTag) &&
                         isTableSection(parent) && grandparent->hasTagName(tableTag)) ||
                         ((!n->hasTagName(tdTag) && !n->hasTagName(thTag) &&
                           !n->hasTagName(formTag) && !n->hasTagName(scriptTag)) && isTableSection(node) &&
                         parent->hasTagName(tableTag))) {
                        node = (node->hasTagName(tableTag)) ? node :
                                ((node->hasTagName(trTag)) ? grandparent : parent);
                        Node *parent = node->parentNode();
                        if (!parent)
                            return false;
                        parent->insertBefore(n, node, ec);
                        if (!ec) {
                            if (n->isHTMLElement() && tagPriority > 0 && 
                                !flat && static_cast<HTMLElement*>(n)->endTagRequirement() != TagStatusForbidden)
                            {
                                pushBlock(localName, tagPriority);
                                setCurrent(n);
                                inStrayTableContent++;
                                blockStack->strayTableContent = true;
                            }
                            return true;
                        }
                    }

                    if (!ec) {
                        if (current->hasTagName(trTag))
                            e = new HTMLTableCellElement(tdTag, document);
                        else if (current->hasTagName(tableTag))
                            e = new HTMLTableSectionElement(tbodyTag, document, true); // implicit 
                        else
                            e = new HTMLTableRowElement(document);
                        
                        insertNode(e);
                        handled = true;
                    }
                }
            }
        } else if (h->hasLocalName(objectTag)) {
            setSkipMode(objectTag);
            return false;
        } else if (h->hasLocalName(ulTag) || h->hasLocalName(olTag) ||
                 h->hasLocalName(dirTag) || h->hasLocalName(menuTag)) {
            e = new HTMLDivElement(document);
            insertNode(e);
            handled = true;
        } else if (h->hasLocalName(selectTag)) {
            if (isInline(n))
                return false;
        } else if (h->hasLocalName(pTag) || isHeaderTag(currentTagName)) {
            if (!isInline(n)) {
                popBlock(currentTagName);
                handled = true;
            }
        } else if (h->hasLocalName(optionTag) || h->hasLocalName(optgroupTag)) {
            if (localName == optgroupTag) {
                popBlock(currentTagName);
                handled = true;
            } else if (localName == selectTag) {
                // IE treats a nested select as </select>. Let's do the same
                popBlock(localName);
            }
        } else if (h->hasLocalName(colgroupTag)) {
            if (!n->isTextNode()) {
                popBlock(currentTagName);
                handled = true;
            }
        } else if (!h->hasLocalName(bodyTag)) {
            if (isInline(current)) {
                popInlineBlocks();
                handled = true;
            }
        }
    } else if (current->isDocumentNode()) {
        if (current->firstChild() == 0 || !current->firstChild()->isHTMLElement()) {
            e = new HTMLHtmlElement(document);
            insertNode(e);
            handled = true;
        }
    }

    // 3. If we couldn't handle the error, just return false and attempt to error-correct again.
    if (!handled)
        return false;
    return insertNode(n);
}

typedef bool (HTMLParser::*CreateErrorCheckFunc)(Token* t, RefPtr<Node>&);
typedef HashMap<AtomicStringImpl*, CreateErrorCheckFunc> FunctionMap;

bool HTMLParser::textCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    result = new Text(document, t->text.get());
    return false;
}

bool HTMLParser::commentCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    result = new Comment(document, t->text.get());
    return false;
}

bool HTMLParser::headCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    if (!head || current->localName() == htmlTag) {
        head = new HTMLHeadElement(document);
        result = head;
    }
    return false;
}

bool HTMLParser::bodyCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    // body no longer allowed if we have a frameset
    if (haveFrameSet)
        return false;
    popBlock(headTag);
    startBody();
    return true;
}

bool HTMLParser::framesetCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    popBlock(headTag);
    if (inBody && !haveFrameSet && !haveContent) {
        popBlock(bodyTag);
        // ### actually for IE document.body returns the now hidden "body" element
        // we can't implement that behaviour now because it could cause too many
        // regressions and the headaches are not worth the work as long as there is
        // no site actually relying on that detail (Dirk)
        if (doc()->body())
            doc()->body()->setAttribute(styleAttr, "display:none");
        inBody = false;
    }
    if ((haveContent || haveFrameSet) && current->localName() == htmlTag)
        return false;
    haveFrameSet = true;
    startBody();
    return true;
}

bool HTMLParser::iframeCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    // a bit of a special case, since the frame is inlined
    setSkipMode(iframeTag);
    return true;
}

bool HTMLParser::formCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    // Only create a new form if we're not already inside one.
    // This is consistent with other browsers' behavior.
    if (!form) {
        form = new HTMLFormElement(document);
        result = form;
    }
    return false;
}

bool HTMLParser::isindexCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    Node *n = handleIsindex(t);
    if (!inBody) {
        isindex = n;
    } else {
        t->flat = true;
        result = n;
    }
    return false;
}

bool HTMLParser::selectCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    inSelect = true;
    return true;
}

bool HTMLParser::ddCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    popBlock(dtTag);
    popBlock(ddTag);
    return true;
}

bool HTMLParser::dtCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    popBlock(ddTag);
    popBlock(dtTag);
    return true;
}

bool HTMLParser::nestedCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    popBlock(t->tagName);
    return true;
}

bool HTMLParser::nestedStyleCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    return allowNestedRedundantTag(t->tagName);
}

bool HTMLParser::tableCellCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    popBlock(tdTag);
    popBlock(thTag);
    return true;
}

bool HTMLParser::tableSectionCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    popBlock(theadTag);
    popBlock(tbodyTag);
    popBlock(tfootTag);
    return true;
}

bool HTMLParser::noembedCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    setSkipMode(noembedTag);
    return true;
}

bool HTMLParser::noframesCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    setSkipMode(noframesTag);
    return true;
}

bool HTMLParser::noscriptCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    if (!m_fragment && document->frame() && document->frame()->jScriptEnabled())
        setSkipMode(noscriptTag);
    return true;
}

bool HTMLParser::mapCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    map = new HTMLMapElement(document);
    result = map;
    return false;
}

bool HTMLParser::canvasCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    if (!m_fragment && document->frame() && document->frame()->jScriptEnabled())
        setSkipMode(canvasTag);
    return true;
}

PassRefPtr<Node> HTMLParser::getNode(Token* t)
{
    // Init our error handling table.
    static FunctionMap gFunctionMap;
    if (gFunctionMap.isEmpty()) {
        gFunctionMap.set(aTag.localName().impl(), &HTMLParser::nestedCreateErrorCheck);
        gFunctionMap.set(bTag.localName().impl(), &HTMLParser::nestedStyleCreateErrorCheck);
        gFunctionMap.set(bigTag.localName().impl(), &HTMLParser::nestedStyleCreateErrorCheck);
        gFunctionMap.set(bodyTag.localName().impl(), &HTMLParser::bodyCreateErrorCheck);
        gFunctionMap.set(buttonTag.localName().impl(), &HTMLParser::nestedCreateErrorCheck);
        gFunctionMap.set(canvasTag.localName().impl(), &HTMLParser::canvasCreateErrorCheck);
        gFunctionMap.set(commentAtom.impl(), &HTMLParser::commentCreateErrorCheck);
        gFunctionMap.set(ddTag.localName().impl(), &HTMLParser::ddCreateErrorCheck);
        gFunctionMap.set(dtTag.localName().impl(), &HTMLParser::dtCreateErrorCheck);
        gFunctionMap.set(formTag.localName().impl(), &HTMLParser::formCreateErrorCheck);
        gFunctionMap.set(framesetTag.localName().impl(), &HTMLParser::framesetCreateErrorCheck);
        gFunctionMap.set(headTag.localName().impl(), &HTMLParser::headCreateErrorCheck);
        gFunctionMap.set(iTag.localName().impl(), &HTMLParser::nestedStyleCreateErrorCheck);
        gFunctionMap.set(iframeTag.localName().impl(), &HTMLParser::iframeCreateErrorCheck);
        gFunctionMap.set(isindexTag.localName().impl(), &HTMLParser::isindexCreateErrorCheck);
        gFunctionMap.set(liTag.localName().impl(), &HTMLParser::nestedCreateErrorCheck);
        gFunctionMap.set(mapTag.localName().impl(), &HTMLParser::mapCreateErrorCheck);
        gFunctionMap.set(nobrTag.localName().impl(), &HTMLParser::nestedCreateErrorCheck);
        gFunctionMap.set(noembedTag.localName().impl(), &HTMLParser::noembedCreateErrorCheck);
        gFunctionMap.set(noframesTag.localName().impl(), &HTMLParser::noframesCreateErrorCheck);
        gFunctionMap.set(noscriptTag.localName().impl(), &HTMLParser::noscriptCreateErrorCheck);
        gFunctionMap.set(sTag.localName().impl(), &HTMLParser::nestedStyleCreateErrorCheck);
        gFunctionMap.set(selectTag.localName().impl(), &HTMLParser::selectCreateErrorCheck);
        gFunctionMap.set(smallTag.localName().impl(), &HTMLParser::nestedStyleCreateErrorCheck);
        gFunctionMap.set(strikeTag.localName().impl(), &HTMLParser::nestedStyleCreateErrorCheck);
        gFunctionMap.set(tbodyTag.localName().impl(), &HTMLParser::tableSectionCreateErrorCheck);
        gFunctionMap.set(tdTag.localName().impl(), &HTMLParser::tableCellCreateErrorCheck);
        gFunctionMap.set(textAtom.impl(), &HTMLParser::textCreateErrorCheck);
        gFunctionMap.set(tfootTag.localName().impl(), &HTMLParser::tableSectionCreateErrorCheck);
        gFunctionMap.set(thTag.localName().impl(), &HTMLParser::tableCellCreateErrorCheck);
        gFunctionMap.set(theadTag.localName().impl(), &HTMLParser::tableSectionCreateErrorCheck);
        gFunctionMap.set(trTag.localName().impl(), &HTMLParser::nestedCreateErrorCheck);
        gFunctionMap.set(ttTag.localName().impl(), &HTMLParser::nestedStyleCreateErrorCheck);
        gFunctionMap.set(uTag.localName().impl(), &HTMLParser::nestedStyleCreateErrorCheck);
        gFunctionMap.set(wbrTag.localName().impl(), &HTMLParser::nestedCreateErrorCheck);
    }

    bool proceed = true;
    RefPtr<Node> result;
    if (CreateErrorCheckFunc errorCheckFunc = gFunctionMap.get(t->tagName.impl()))
        proceed = (this->*errorCheckFunc)(t, result);
    if (proceed)
        result = HTMLElementFactory::createHTMLElement(t->tagName, doc(), form);
    return result.release();
}

#define MAX_REDUNDANT 20

bool HTMLParser::allowNestedRedundantTag(const AtomicString& _tagName)
{
    // www.liceo.edu.mx is an example of a site that achieves a level of nesting of
    // about 1500 tags, all from a bunch of <b>s.  We will only allow at most 20
    // nested tags of the same type before just ignoring them all together.
    int i = 0;
    for (HTMLStackElem* curr = blockStack;
         i < MAX_REDUNDANT && curr && curr->tagName == _tagName;
         curr = curr->next, i++);
    return i != MAX_REDUNDANT;
}

void HTMLParser::processCloseTag(Token *t)
{
    // Support for really broken html.
    // we never close the body tag, since some stupid web pages close it before the actual end of the doc.
    // let's rely on the end() call to close things.
    if (t->tagName == htmlTag || t->tagName == bodyTag)
        return;
    
    if (t->tagName == formTag)
        form = 0;
    else if (t->tagName == mapTag)
        map = 0;
    else if (t->tagName == selectTag)
        inSelect = false;
        
    HTMLStackElem* oldElem = blockStack;
    popBlock(t->tagName);
    if (oldElem == blockStack && t->tagName == pTag) {
        // We encountered a stray </p>.  Amazingly Gecko, WinIE, and MacIE all treat
        // this as a valid break, i.e., <p></p>.  So go ahead and make the empty
        // paragraph.
        t->beginTag = true;
        parseToken(t);
        popBlock(t->tagName);
    }
}

bool HTMLParser::isHeaderTag(const AtomicString& tagName)
{
    static HashSet<AtomicStringImpl*> headerTags;
    if (headerTags.isEmpty()) {
        headerTags.add(h1Tag.localName().impl());
        headerTags.add(h2Tag.localName().impl());
        headerTags.add(h3Tag.localName().impl());
        headerTags.add(h4Tag.localName().impl());
        headerTags.add(h5Tag.localName().impl());
        headerTags.add(h6Tag.localName().impl());
    }
    
    return headerTags.contains(tagName.impl());
}

void HTMLParser::popNestedHeaderTag()
{
    // This function only cares about checking for nested headers that have only inlines in between them.
    Node* currNode = current;
    for (HTMLStackElem* curr = blockStack; curr; curr = curr->next) {
        if (isHeaderTag(curr->tagName)) {
            popBlock(curr->tagName);
            return;
        }
        if (currNode && !isInline(currNode))
            return;
        currNode = curr->node.get();
    }
}

bool HTMLParser::isInline(Node* node) const
{
    if (node->isTextNode())
        return true;

    if (node->isHTMLElement()) {
        HTMLElement* e = static_cast<HTMLElement*>(node);
        if (e->hasLocalName(aTag) || e->hasLocalName(fontTag) || e->hasLocalName(ttTag) ||
            e->hasLocalName(uTag) || e->hasLocalName(bTag) || e->hasLocalName(iTag) ||
            e->hasLocalName(sTag) || e->hasLocalName(strikeTag) || e->hasLocalName(bigTag) ||
            e->hasLocalName(smallTag) || e->hasLocalName(emTag) || e->hasLocalName(strongTag) ||
            e->hasLocalName(dfnTag) || e->hasLocalName(codeTag) || e->hasLocalName(sampTag) ||
            e->hasLocalName(kbdTag) || e->hasLocalName(varTag) || e->hasLocalName(citeTag) ||
            e->hasLocalName(abbrTag) || e->hasLocalName(acronymTag) || e->hasLocalName(subTag) ||
            e->hasLocalName(supTag) || e->hasLocalName(spanTag) || e->hasLocalName(nobrTag) ||
            e->hasLocalName(wbrTag) || e->hasLocalName(noframesTag) || e->hasLocalName(nolayerTag) ||
            e->hasLocalName(noembedTag) || (e->hasLocalName(noscriptTag) && !m_fragment && document->frame() && document->frame()->jScriptEnabled()))
            return true;
    }
    
    return false;
}

bool HTMLParser::isResidualStyleTag(const AtomicString& tagName)
{
    static HashSet<AtomicStringImpl*> residualStyleTags;
    if (residualStyleTags.isEmpty()) {
        residualStyleTags.add(aTag.localName().impl());
        residualStyleTags.add(fontTag.localName().impl());
        residualStyleTags.add(ttTag.localName().impl());
        residualStyleTags.add(uTag.localName().impl());
        residualStyleTags.add(bTag.localName().impl());
        residualStyleTags.add(iTag.localName().impl());
        residualStyleTags.add(sTag.localName().impl());
        residualStyleTags.add(strikeTag.localName().impl());
        residualStyleTags.add(bigTag.localName().impl());
        residualStyleTags.add(smallTag.localName().impl());
        residualStyleTags.add(emTag.localName().impl());
        residualStyleTags.add(strongTag.localName().impl());
        residualStyleTags.add(dfnTag.localName().impl());
        residualStyleTags.add(codeTag.localName().impl());
        residualStyleTags.add(sampTag.localName().impl());
        residualStyleTags.add(kbdTag.localName().impl());
        residualStyleTags.add(varTag.localName().impl());
        residualStyleTags.add(nobrTag.localName().impl());
        residualStyleTags.add(wbrTag.localName().impl());
    }
    
    return residualStyleTags.contains(tagName.impl());
}

bool HTMLParser::isAffectedByResidualStyle(const AtomicString& tagName)
{
    if (isResidualStyleTag(tagName))
        return true;

    static HashSet<AtomicStringImpl*> affectedBlockTags;
    if (affectedBlockTags.isEmpty()) {
        affectedBlockTags.add(addressTag.localName().impl());
        affectedBlockTags.add(blockquoteTag.localName().impl());
        affectedBlockTags.add(centerTag.localName().impl());
        affectedBlockTags.add(ddTag.localName().impl());
        affectedBlockTags.add(divTag.localName().impl());
        affectedBlockTags.add(dlTag.localName().impl());
        affectedBlockTags.add(dtTag.localName().impl());
        affectedBlockTags.add(formTag.localName().impl());
        affectedBlockTags.add(h1Tag.localName().impl());
        affectedBlockTags.add(h2Tag.localName().impl());
        affectedBlockTags.add(h3Tag.localName().impl());
        affectedBlockTags.add(h4Tag.localName().impl());
        affectedBlockTags.add(h5Tag.localName().impl());
        affectedBlockTags.add(h6Tag.localName().impl());
        affectedBlockTags.add(liTag.localName().impl());
        affectedBlockTags.add(listingTag.localName().impl());
        affectedBlockTags.add(olTag.localName().impl());
        affectedBlockTags.add(pTag.localName().impl());
        affectedBlockTags.add(preTag.localName().impl());
        affectedBlockTags.add(ulTag.localName().impl());
    }
    
    return affectedBlockTags.contains(tagName.impl());
}

void HTMLParser::handleResidualStyleCloseTagAcrossBlocks(HTMLStackElem* elem)
{
    // Find the element that crosses over to a higher level.   For now, if there is more than
    // one, we will just give up and not attempt any sort of correction.  It's highly unlikely that
    // there will be more than one, since <p> tags aren't allowed to be nested.
    ExceptionCode ec = 0;
    HTMLStackElem* curr = blockStack;
    HTMLStackElem* maxElem = 0;
    HTMLStackElem* prev = 0;
    HTMLStackElem* prevMaxElem = 0;
    while (curr && curr != elem) {
        if (curr->level > elem->level) {
            if (maxElem)
                return;
            maxElem = curr;
            prevMaxElem = prev;
        }

        prev = curr;
        curr = curr->next;
    }

    if (!curr || !maxElem || !isAffectedByResidualStyle(maxElem->tagName)) return;

    Node* residualElem = prev->node.get();
    Node* blockElem = prevMaxElem ? prevMaxElem->node.get() : current;
    Node* parentElem = elem->node.get();

    // Check to see if the reparenting that is going to occur is allowed according to the DOM.
    // FIXME: We should either always allow it or perform an additional fixup instead of
    // just bailing here.
    // Example: <p><font><center>blah</font></center></p> isn't doing a fixup right now.
    if (!parentElem->childAllowed(blockElem))
        return;
    
    if (maxElem->node->parentNode() != elem->node) {
        // Walk the stack and remove any elements that aren't residual style tags.  These
        // are basically just being closed up.  Example:
        // <font><span>Moo<p>Goo</font></p>.
        // In the above example, the <span> doesn't need to be reopened.  It can just close.
        HTMLStackElem* currElem = maxElem->next;
        HTMLStackElem* prevElem = maxElem;
        while (currElem != elem) {
            HTMLStackElem* nextElem = currElem->next;
            if (!isResidualStyleTag(currElem->tagName)) {
                prevElem->next = nextElem;
                prevElem->node = currElem->node;
                delete currElem;
            }
            else
                prevElem = currElem;
            currElem = nextElem;
        }
        
        // We have to reopen residual tags in between maxElem and elem.  An example of this case is:
        // <font><i>Moo<p>Foo</font>.
        // In this case, we need to transform the part before the <p> into:
        // <font><i>Moo</i></font><i>
        // so that the <i> will remain open.  This involves the modification of elements
        // in the block stack.
        // This will also affect how we ultimately reparent the block, since we want it to end up
        // under the reopened residual tags (e.g., the <i> in the above example.)
        RefPtr<Node> prevNode = 0;
        currElem = maxElem;
        while (currElem->node != residualElem) {
            if (isResidualStyleTag(currElem->node->localName())) {
                // Create a clone of this element.
                RefPtr<Node> currNode = currElem->node->cloneNode(false);

                // Change the stack element's node to point to the clone.
                currElem->node = currNode;
                
                // Attach the previous node as a child of this new node.
                if (prevNode)
                    currNode->appendChild(prevNode, ec);
                else // The new parent for the block element is going to be the innermost clone.
                    parentElem = currNode.get();
                                
                prevNode = currNode.get();
            }
            
            currElem = currElem->next;
        }

        // Now append the chain of new residual style elements if one exists.
        if (prevNode)
            elem->node->appendChild(prevNode, ec);
    }
         
    // Check if the block is still in the tree. If it isn't, then we don't
    // want to remove it from its parent (that would crash) or insert it into
    // a new parent later. See http://bugzilla.opendarwin.org/show_bug.cgi?id=6778
    bool isBlockStillInTree = blockElem->parentNode();

    // We need to make a clone of |residualElem| and place it just inside |blockElem|.
    // All content of |blockElem| is reparented to be under this clone.  We then
    // reparent |blockElem| using real DOM calls so that attachment/detachment will
    // be performed to fix up the rendering tree.
    // So for this example: <b>...<p>Foo</b>Goo</p>
    // The end result will be: <b>...</b><p><b>Foo</b>Goo</p>
    //
    // Step 1: Remove |blockElem| from its parent, doing a batch detach of all the kids.
    if (form)
        form->setPreserveAcrossRemove(true);
    if (isBlockStillInTree)
        blockElem->parentNode()->removeChild(blockElem, ec);

    // Step 2: Clone |residualElem|.
    RefPtr<Node> newNode = residualElem->cloneNode(false); // Shallow clone. We don't pick up the same kids.

    // Step 3: Place |blockElem|'s children under |newNode|.  Remove all of the children of |blockElem|
    // before we've put |newElem| into the document.  That way we'll only do one attachment of all
    // the new content (instead of a bunch of individual attachments).
    Node* currNode = blockElem->firstChild();
    while (currNode) {
        Node* nextNode = currNode->nextSibling();
        newNode->appendChild(currNode, ec);
        currNode = nextNode;
    }

    // Step 4: Place |newNode| under |blockElem|.  |blockElem| is still out of the document, so no
    // attachment can occur yet.
    blockElem->appendChild(newNode.release(), ec);
    
    // Step 5: Reparent |blockElem|.  Now the full attachment of the fixed up tree takes place.
    if (isBlockStillInTree)
        parentElem->appendChild(blockElem, ec);
        
    // Step 6: Elide |elem|, since it is effectively no longer open.  Also update
    // the node associated with the previous stack element so that when it gets popped,
    // it doesn't make the residual element the next current node.
    HTMLStackElem* currElem = maxElem;
    HTMLStackElem* prevElem = 0;
    while (currElem != elem) {
        prevElem = currElem;
        currElem = currElem->next;
    }
    prevElem->next = elem->next;
    prevElem->node = elem->node;
    delete elem;
    
    // Step 7: Reopen intermediate inlines, e.g., <b><p><i>Foo</b>Goo</p>.
    // In the above example, Goo should stay italic.
    curr = blockStack;
    HTMLStackElem* residualStyleStack = 0;
    while (curr && curr != maxElem) {
        // We will actually schedule this tag for reopening
        // after we complete the close of this entire block.
        Node* currNode = current;
        if (isResidualStyleTag(curr->tagName)) {
            // We've overloaded the use of stack elements and are just reusing the
            // struct with a slightly different meaning to the variables.  Instead of chaining
            // from innermost to outermost, we build up a list of all the tags we need to reopen
            // from the outermost to the innermost, i.e., residualStyleStack will end up pointing
            // to the outermost tag we need to reopen.
            // We also set curr->node to be the actual element that corresponds to the ID stored in
            // curr->id rather than the node that you should pop to when the element gets pulled off
            // the stack.
            popOneBlock(false);
            curr->node = currNode;
            curr->next = residualStyleStack;
            residualStyleStack = curr;
        }
        else
            popOneBlock();

        curr = blockStack;
    }

    reopenResidualStyleTags(residualStyleStack, 0); // FIXME: Deal with stray table content some day
                                                    // if it becomes necessary to do so.
                                                    
    if (form)
        form->setPreserveAcrossRemove(false);
}

void HTMLParser::reopenResidualStyleTags(HTMLStackElem* elem, Node* malformedTableParent)
{
    // Loop for each tag that needs to be reopened.
    while (elem) {
        // Create a shallow clone of the DOM node for this element.
        RefPtr<Node> newNode = elem->node->cloneNode(false); 

        // Append the new node. In the malformed table case, we need to insert before the table,
        // which will be the last child.
        ExceptionCode ec = 0;
        if (malformedTableParent)
            malformedTableParent->insertBefore(newNode, malformedTableParent->lastChild(), ec);
        else
            current->appendChild(newNode, ec);
        // FIXME: Is it really OK to ignore the exceptions here?

        // Now push a new stack element for this node we just created.
        pushBlock(elem->tagName, elem->level);

        // Set our strayTableContent boolean if needed, so that the reopened tag also knows
        // that it is inside a malformed table.
        blockStack->strayTableContent = malformedTableParent != 0;
        if (blockStack->strayTableContent)
            inStrayTableContent++;

        // Clear our malformed table parent variable.
        malformedTableParent = 0;

        // Update |current| manually to point to the new node.
        setCurrent(newNode.get());
        
        // Advance to the next tag that needs to be reopened.
        HTMLStackElem* next = elem->next;
        delete elem;
        elem = next;
    }
}

void HTMLParser::pushBlock(const AtomicString& tagName, int _level)
{
    HTMLStackElem *Elem = new HTMLStackElem(tagName, _level, current, blockStack);
    blockStack = Elem;
}

void HTMLParser::popBlock(const AtomicString& _tagName)
{
    HTMLStackElem *Elem = blockStack;
    
    int maxLevel = 0;

    while (Elem && (Elem->tagName != _tagName)) {
        if (maxLevel < Elem->level)
            maxLevel = Elem->level;
        Elem = Elem->next;
    }

    if (!Elem)
        return;

    if (maxLevel > Elem->level) {
        // We didn't match because the tag is in a different scope, e.g.,
        // <b><p>Foo</b>.  Try to correct the problem.
        if (!isResidualStyleTag(_tagName))
            return;
        return handleResidualStyleCloseTagAcrossBlocks(Elem);
    }

    bool isAffectedByStyle = isAffectedByResidualStyle(Elem->tagName);
    HTMLStackElem* residualStyleStack = 0;
    Node* malformedTableParent = 0;
    
    Elem = blockStack;
    while (Elem) {
        if (Elem->tagName == _tagName) {
            int strayTable = inStrayTableContent;
            popOneBlock();
            Elem = 0;

            // This element was the root of some malformed content just inside an implicit or
            // explicit <tbody> or <tr>.
            // If we end up needing to reopen residual style tags, the root of the reopened chain
            // must also know that it is the root of malformed content inside a <tbody>/<tr>.
            if (strayTable && (inStrayTableContent < strayTable) && residualStyleStack) {
                Node* curr = current;
                while (curr && !curr->hasTagName(tableTag))
                    curr = curr->parentNode();
                malformedTableParent = curr ? curr->parentNode() : 0;
            }
        }
        else {
            if (form && Elem->tagName == formTag)
                // A <form> is being closed prematurely (and this is
                // malformed HTML).  Set an attribute on the form to clear out its
                // bottom margin.
                form->setMalformed(true);

            // Schedule this tag for reopening
            // after we complete the close of this entire block.
            Node* currNode = current;
            if (isAffectedByStyle && isResidualStyleTag(Elem->tagName)) {
                // We've overloaded the use of stack elements and are just reusing the
                // struct with a slightly different meaning to the variables.  Instead of chaining
                // from innermost to outermost, we build up a list of all the tags we need to reopen
                // from the outermost to the innermost, i.e., residualStyleStack will end up pointing
                // to the outermost tag we need to reopen.
                // We also set Elem->node to be the actual element that corresponds to the ID stored in
                // Elem->id rather than the node that you should pop to when the element gets pulled off
                // the stack.
                popOneBlock(false);
                Elem->next = residualStyleStack;
                Elem->node = currNode;
                residualStyleStack = Elem;
            }
            else
                popOneBlock();
            Elem = blockStack;
        }
    }

    reopenResidualStyleTags(residualStyleStack, malformedTableParent);
}

void HTMLParser::popOneBlock(bool delBlock)
{
    HTMLStackElem* elem = blockStack;

    // Form elements restore their state during the parsing process.
    // Also, a few elements (<applet>, <object>) need to know when all child elements (<param>s) are available.
    if (current && elem->node != current)
        current->closeRenderer();

    blockStack = elem->next;
    setCurrent(elem->node.get());

    if (elem->strayTableContent)
        inStrayTableContent--;
    
    if (delBlock)
        delete elem;
}

void HTMLParser::popInlineBlocks()
{
    while (blockStack && isInline(current))
        popOneBlock();
}

void HTMLParser::freeBlock()
{
    while (blockStack)
        popOneBlock();
}

void HTMLParser::createHead()
{
    if (head || !doc()->firstChild())
        return;

    head = new HTMLHeadElement(document);
    HTMLElement* body = doc()->body();
    ExceptionCode ec = 0;
    Node *parent = doc()->firstChild();
    while (parent && !parent->isHTMLElement() && !parent->hasTagName(htmlTag))
        parent = parent->nextSibling();
    if (parent) {
        parent->insertBefore(head, body, ec);
        if (ec)
            head = 0;
    }
}

Node* HTMLParser::handleIsindex(Token* t)
{
    Node* n = new HTMLDivElement(document);

    NamedMappedAttrMap* attrs = t->attrs.get();

    RefPtr<HTMLIsIndexElement> isIndex = new HTMLIsIndexElement(document, form);
    isIndex->setAttributeMap(attrs);
    isIndex->setAttribute(typeAttr, "khtml_isindex");

    String text = searchableIndexIntroduction();
    if (attrs) {
        if (Attribute *a = attrs->getAttributeItem(promptAttr))
            text = a->value().domString() + " ";
        t->attrs = 0;
    }

    n->addChild(new HTMLHRElement(document));
    n->addChild(new Text(document, text));
    n->addChild(isIndex.get());
    n->addChild(new HTMLHRElement(document));

    return n;
}

void HTMLParser::startBody()
{
    if(inBody) return;

    inBody = true;

    if (isindex) {
        insertNode(isindex.get(), true /* don't decend into this node */);
        isindex = 0;
    }
}

void HTMLParser::finished()
{
    // In the case of a completely empty document, here's the place to create the HTML element.
    if (current && current->isDocumentNode() && !current->firstChild())
        insertNode(new HTMLHtmlElement(document));

    // This ensures that "current" is not left pointing to a node when the document is destroyed.
    freeBlock();
    setCurrent(0);

    // Warning, this may delete the tokenizer and parser, so don't try to do anything else after this.
    if (!m_fragment)
        document->finishedParsing();
}

}

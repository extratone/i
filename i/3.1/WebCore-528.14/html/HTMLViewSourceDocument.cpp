/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "HTMLViewSourceDocument.h"

#include "DOMImplementation.h"
#include "HTMLTokenizer.h"
#include "HTMLHtmlElement.h"
#include "HTMLAnchorElement.h"
#include "HTMLBodyElement.h"
#include "HTMLDivElement.h"
#include "HTMLTableElement.h"
#include "HTMLTableCellElement.h"
#include "HTMLTableRowElement.h"
#include "HTMLTableSectionElement.h"
#include "Text.h"
#include "TextDocument.h"
#include "HTMLNames.h"

namespace WebCore {

using namespace HTMLNames;

HTMLViewSourceDocument::HTMLViewSourceDocument(Frame* frame, const String& mimeType)
    : HTMLDocument(frame)
    , m_type(mimeType)
    , m_current(0)
    , m_tbody(0)
    , m_td(0)
{
    setUsesBeforeAfterRules(true);
}

Tokenizer* HTMLViewSourceDocument::createTokenizer()
{
    if (implementation()->isTextMIMEType(m_type))
        return createTextTokenizer(this);
    return new HTMLTokenizer(this);
}

void HTMLViewSourceDocument::createContainingTable()
{
    RefPtr<Element> html = new HTMLHtmlElement(htmlTag, this);
    addChild(html);
    html->attach();
    RefPtr<Element> body = new HTMLBodyElement(bodyTag, this);
    html->addChild(body);
    body->attach();
    
    // Create a line gutter div that can be used to make sure the gutter extends down the height of the whole
    // document.
    RefPtr<Element> div = new HTMLDivElement(divTag, this);
    RefPtr<NamedMappedAttrMap> attrs = NamedMappedAttrMap::create();
    attrs->insertAttribute(MappedAttribute::create(classAttr, "webkit-line-gutter-backdrop"), true);
    div->setAttributeMap(attrs.release());
    body->addChild(div);
    div->attach();

    RefPtr<Element> table = new HTMLTableElement(tableTag, this);
    body->addChild(table);
    table->attach();
    m_tbody = new HTMLTableSectionElement(tbodyTag, this);
    table->addChild(m_tbody);
    m_tbody->attach();
    m_current = m_tbody;
}

void HTMLViewSourceDocument::addViewSourceText(const String& text)
{
    if (!m_current)
        createContainingTable();
    addText(text, "");
}

void HTMLViewSourceDocument::addViewSourceToken(Token* token)
{
    if (!m_current)
        createContainingTable();

    if (token->tagName == textAtom)
        addText(token->text.get(), "");
    else if (token->tagName == commentAtom) {
        if (token->beginTag) {
            m_current = addSpanWithClassName("webkit-html-comment");
            addText(String("<!--") + token->text.get() + "-->", "webkit-html-comment");
        }
    } else {
        // Handle the tag.
        String classNameStr = "webkit-html-tag";
        m_current = addSpanWithClassName(classNameStr);

        String text = "<";
        if (!token->beginTag)
            text += "/";
        text += token->tagName;
        Vector<UChar>* guide = token->m_sourceInfo.get();
        if (!guide || !guide->size())
            text += ">";

        addText(text, classNameStr);

        // Walk our guide string that tells us where attribute names/values should go.
        if (guide && guide->size()) {
            unsigned size = guide->size();
            unsigned begin = 0;
            unsigned currAttr = 0;
            RefPtr<Attribute> attr = 0;
            for (unsigned i = 0; i < size; i++) {
                if (guide->at(i) == 'a' || guide->at(i) == 'x' || guide->at(i) == 'v') {
                    // Add in the string.
                    addText(String(static_cast<UChar*>(guide->data()) + begin, i - begin), classNameStr);
                     
                    begin = i + 1;

                    if (guide->at(i) == 'a') {
                        if (token->attrs && currAttr < token->attrs->length())
                            attr = token->attrs->attributeItem(currAttr++);
                        else
                            attr = 0;
                    }
                    if (attr) {
                        if (guide->at(i) == 'a') {
                            String name = attr->name().toString();
                            
                            m_current = addSpanWithClassName("webkit-html-attribute-name");
                            addText(name, "webkit-html-attribute-name");
                            if (m_current != m_tbody)
                                m_current = static_cast<Element*>(m_current->parent());
                        } else {
                            const String& value = attr->value().string();
                            // FIXME: XML could use namespace prefixes and confuse us.
                            if (equalIgnoringCase(attr->name().localName(), "src") || equalIgnoringCase(attr->name().localName(), "href"))
                                m_current = addLink(value, equalIgnoringCase(token->tagName, "a"));
                            else
                                m_current = addSpanWithClassName("webkit-html-attribute-value");
                            addText(value, "webkit-html-attribute-value");
                            if (m_current != m_tbody)
                                m_current = static_cast<Element*>(m_current->parent());
                        }
                    }
                }
            }
            
            // Add in any string that might be left.
            if (begin < size)
                addText(String(static_cast<UChar*>(guide->data()) + begin, size - begin), classNameStr);

            // Add in the end tag.
            addText(">", classNameStr);
        }
        
        m_current = m_td;
    }
}

void HTMLViewSourceDocument::addViewSourceDoctypeToken(DoctypeToken* doctypeToken)
{
    if (!m_current)
        createContainingTable();
    m_current = addSpanWithClassName("webkit-html-doctype");
    String text = "<";
    text += String::adopt(doctypeToken->m_source);
    text += ">";
    addText(text, "webkit-html-doctype");
}

Element* HTMLViewSourceDocument::addSpanWithClassName(const String& className)
{
    if (m_current == m_tbody) {
        addLine(className);
        return m_current;
    }

    Element* span = new HTMLElement(spanTag, this);
    RefPtr<NamedMappedAttrMap> attrs = NamedMappedAttrMap::create();
    attrs->insertAttribute(MappedAttribute::create(classAttr, className), true);
    span->setAttributeMap(attrs.release());
    m_current->addChild(span);
    span->attach();
    return span;
}

void HTMLViewSourceDocument::addLine(const String& className)
{
    // Create a table row.
    RefPtr<Element> trow = new HTMLTableRowElement(trTag, this);
    m_tbody->addChild(trow);
    trow->attach();
    
    // Create a cell that will hold the line number (it is generated in the stylesheet using counters).
    Element* td = new HTMLTableCellElement(tdTag, this);
    RefPtr<NamedMappedAttrMap> attrs = NamedMappedAttrMap::create();
    attrs->insertAttribute(MappedAttribute::create(classAttr, "webkit-line-number"), true);
    td->setAttributeMap(attrs.release());
    trow->addChild(td);
    td->attach();

    // Create a second cell for the line contents
    td = new HTMLTableCellElement(tdTag, this);
    attrs = NamedMappedAttrMap::create();
    attrs->insertAttribute(MappedAttribute::create(classAttr, "webkit-line-content"), true);
    td->setAttributeMap(attrs.release());
    trow->addChild(td);
    td->attach();
    m_current = m_td = td;

#ifdef DEBUG_LINE_NUMBERS
    RefPtr<Text> lineNumberText = new Text(this, String::number(tokenizer()->lineNumber() + 1) + " ");
    td->addChild(lineNumberText);
    lineNumberText->attach();
#endif

    // Open up the needed spans.
    if (!className.isEmpty()) {
        if (className == "webkit-html-attribute-name" || className == "webkit-html-attribute-value")
            m_current = addSpanWithClassName("webkit-html-tag");
        m_current = addSpanWithClassName(className);
    }
}

void HTMLViewSourceDocument::addText(const String& text, const String& className)
{
    if (text.isEmpty())
        return;

    // Add in the content, splitting on newlines.
    Vector<String> lines;
    text.split('\n', true, lines);
    unsigned size = lines.size();
    for (unsigned i = 0; i < size; i++) {
        String substring = lines[i];
        if (substring.isEmpty()) {
            if (i == size - 1)
                break;
            substring = " ";
        }
        if (m_current == m_tbody)
            addLine(className);
        RefPtr<Text> t = new Text(this, substring);
        m_current->addChild(t);
        t->attach();
        if (i < size - 1)
            m_current = m_tbody;
    }
    
    // Set current to m_tbody if the last character was a newline.
    if (text[text.length() - 1] == '\n')
        m_current = m_tbody;
}

Element* HTMLViewSourceDocument::addLink(const String& url, bool isAnchor)
{
    if (m_current == m_tbody)
        addLine("webkit-html-tag");
    
    // Now create a link for the attribute value instead of a span.
    Element* anchor = new HTMLAnchorElement(aTag, this);
    RefPtr<NamedMappedAttrMap> attrs = NamedMappedAttrMap::create();
    const char* classValue;
    if (isAnchor)
        classValue = "webkit-html-attribute-value webkit-html-external-link";
    else
        classValue = "webkit-html-attribute-value webkit-html-resource-link";
    attrs->insertAttribute(MappedAttribute::create(classAttr, classValue), true);
    attrs->insertAttribute(MappedAttribute::create(targetAttr, "_blank"), true);
    attrs->insertAttribute(MappedAttribute::create(hrefAttr, url), true);
    anchor->setAttributeMap(attrs.release());
    m_current->addChild(anchor);
    anchor->attach();
    return anchor;
}

}

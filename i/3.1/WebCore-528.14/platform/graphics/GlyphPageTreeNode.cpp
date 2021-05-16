/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "GlyphPageTreeNode.h"

#include "CharacterNames.h"
#include "SegmentedFontData.h"
#include "SimpleFontData.h"
#include <wtf/unicode/Unicode.h>

namespace WebCore {

using std::max;
using std::min;

HashMap<int, GlyphPageTreeNode*>* GlyphPageTreeNode::roots = 0;
GlyphPageTreeNode* GlyphPageTreeNode::pageZeroRoot = 0;

GlyphPageTreeNode* GlyphPageTreeNode::getRoot(unsigned pageNumber)
{
    static bool initialized;
    if (!initialized) {
        initialized = true;
        roots = new HashMap<int, GlyphPageTreeNode*>;
        pageZeroRoot = new GlyphPageTreeNode;
    }

    GlyphPageTreeNode* node = pageNumber ? roots->get(pageNumber) : pageZeroRoot;
    if (!node) {
        node = new GlyphPageTreeNode;
#ifndef NDEBUG
        node->m_pageNumber = pageNumber;
#endif
        if (pageNumber)
            roots->set(pageNumber, node);
        else
            pageZeroRoot = node;
    }
    return node;
}

size_t GlyphPageTreeNode::treeGlyphPageCount()
{
    size_t count = 0;
    if (roots) {
        HashMap<int, GlyphPageTreeNode*>::iterator end = roots->end();
        for (HashMap<int, GlyphPageTreeNode*>::iterator it = roots->begin(); it != end; ++it)
            count += it->second->pageCount();
    }
    
    if (pageZeroRoot)
        count += pageZeroRoot->pageCount();

    return count;
}

size_t GlyphPageTreeNode::pageCount() const
{
    size_t count = m_page && m_page->owner() == this ? 1 : 0;
    HashMap<const FontData*, GlyphPageTreeNode*>::const_iterator end = m_children.end();
    for (HashMap<const FontData*, GlyphPageTreeNode*>::const_iterator it = m_children.begin(); it != end; ++it)
        count += it->second->pageCount();

    return count;
}

void GlyphPageTreeNode::pruneTreeCustomFontData(const FontData* fontData)
{
    // Enumerate all the roots and prune any tree that contains our custom font data.
    if (roots) {
        HashMap<int, GlyphPageTreeNode*>::iterator end = roots->end();
        for (HashMap<int, GlyphPageTreeNode*>::iterator it = roots->begin(); it != end; ++it)
            it->second->pruneCustomFontData(fontData);
    }
    
    if (pageZeroRoot)
        pageZeroRoot->pruneCustomFontData(fontData);
}

void GlyphPageTreeNode::pruneTreeFontData(const SimpleFontData* fontData)
{
    if (roots) {
        HashMap<int, GlyphPageTreeNode*>::iterator end = roots->end();
        for (HashMap<int, GlyphPageTreeNode*>::iterator it = roots->begin(); it != end; ++it)
            it->second->pruneFontData(fontData);
    }
    
    if (pageZeroRoot)
        pageZeroRoot->pruneFontData(fontData);
}

GlyphPageTreeNode::~GlyphPageTreeNode()
{
    deleteAllValues(m_children);
    delete m_systemFallbackChild;
}

void GlyphPageTreeNode::initializePage(const FontData* fontData, unsigned pageNumber)
{
    ASSERT(!m_page);

    // This function must not be called for the root of the tree, because that
    // level does not contain any glyphs.
    ASSERT(m_level > 0 && m_parent);

    // The parent's page will be 0 if we are level one or the parent's font data
    // did not contain any glyphs for that page.
    GlyphPage* parentPage = m_parent->page();

    // NULL FontData means we're being asked for the system fallback font.
    if (fontData) {
        if (m_level == 1) {
            // Children of the root hold pure pages. These will cover only one
            // font data's glyphs, and will have glyph index 0 if the font data does not
            // contain the glyph.
            unsigned start = pageNumber * GlyphPage::size;
            UChar buffer[GlyphPage::size * 2 + 2];
            unsigned bufferLength;
            unsigned i;

            // Fill in a buffer with the entire "page" of characters that we want to look up glyphs for.
            if (start < 0x10000) {
                bufferLength = GlyphPage::size;
                for (i = 0; i < GlyphPage::size; i++)
                    buffer[i] = start + i;

                if (start == 0) {
                    // Control characters must not render at all.
                    for (i = 0; i < 0x20; ++i)
                        buffer[i] = zeroWidthSpace;
                    for (i = 0x7F; i < 0xA0; i++)
                        buffer[i] = zeroWidthSpace;

                    // \n, \t, and nonbreaking space must render as a space.
                    buffer[(int)'\n'] = ' ';
                    buffer[(int)'\t'] = ' ';
                    buffer[noBreakSpace] = ' ';
                } else if (start == (leftToRightMark & ~(GlyphPage::size - 1))) {
                    // LRM, RLM, LRE, RLE and PDF must not render at all.
                    buffer[leftToRightMark - start] = zeroWidthSpace;
                    buffer[rightToLeftMark - start] = zeroWidthSpace;
                    buffer[leftToRightEmbed - start] = zeroWidthSpace;
                    buffer[rightToLeftEmbed - start] = zeroWidthSpace;
                    buffer[leftToRightOverride - start] = zeroWidthSpace;
                    buffer[rightToLeftOverride - start] = zeroWidthSpace;
                    buffer[popDirectionalFormatting - start] = zeroWidthSpace;
                } else if (start == (objectReplacementCharacter & ~(GlyphPage::size - 1))) {
                    // Object replacement character must not render at all.
                    buffer[objectReplacementCharacter - start] = zeroWidthSpace;
                }
            } else {
                bufferLength = GlyphPage::size * 2;
                for (i = 0; i < GlyphPage::size; i++) {
                    int c = i + start;
                    buffer[i * 2] = U16_LEAD(c);
                    buffer[i * 2 + 1] = U16_TRAIL(c);
                }
            }
            
            m_page = GlyphPage::create(this);

            // Now that we have a buffer full of characters, we want to get back an array
            // of glyph indices.  This part involves calling into the platform-specific 
            // routine of our glyph map for actually filling in the page with the glyphs.
            // Success is not guaranteed. For example, Times fails to fill page 260, giving glyph data
            // for only 128 out of 256 characters.
            bool haveGlyphs;
            if (fontData->isSegmented()) {
                haveGlyphs = false;

                const SegmentedFontData* segmentedFontData = static_cast<const SegmentedFontData*>(fontData);
                unsigned numRanges = segmentedFontData->numRanges();
                bool zeroFilled = false;
                RefPtr<GlyphPage> scratchPage;
                GlyphPage* pageToFill = m_page.get();
                for (unsigned i = 0; i < numRanges; i++) {
                    const FontDataRange& range = segmentedFontData->rangeAt(i);
                    int from = max(0, range.from() - static_cast<int>(start));
                    int to = 1 + min(range.to() - static_cast<int>(start), static_cast<int>(GlyphPage::size) - 1);
                    if (from < static_cast<int>(GlyphPage::size) && to > 0) {
                        if (haveGlyphs && !scratchPage) {
                            scratchPage = GlyphPage::create(this);
                            pageToFill = scratchPage.get();
                        }

                        if (!zeroFilled) {
                            if (from > 0 || to < static_cast<int>(GlyphPage::size)) {
                                for (unsigned i = 0; i < GlyphPage::size; i++)
                                    pageToFill->setGlyphDataForIndex(i, 0, 0);
                            }
                            zeroFilled = true;
                        }
                        haveGlyphs |= pageToFill->fill(from, to - from, buffer + from * (start < 0x10000 ? 1 : 2), (to - from) * (start < 0x10000 ? 1 : 2), range.fontData());
                        if (scratchPage) {
                            for (int j = from; j < to; j++) {
                                if (!m_page->m_glyphs[j].glyph && pageToFill->m_glyphs[j].glyph)
                                    m_page->m_glyphs[j] = pageToFill->m_glyphs[j];
                            }
                        }
                    }
                }
            } else
                haveGlyphs = m_page->fill(0, GlyphPage::size, buffer, bufferLength, static_cast<const SimpleFontData*>(fontData));

            if (!haveGlyphs)
                m_page = 0;
        } else if (parentPage && parentPage->owner() != m_parent) {
            // The page we're overriding may not be owned by our parent node.
            // This happens when our parent node provides no useful overrides
            // and just copies the pointer to an already-existing page (see
            // below).
            //
            // We want our override to be shared by all nodes that reference
            // that page to avoid duplication, and so standardize on having the
            // page's owner collect all the overrides.  Call getChild on the
            // page owner with the desired font data (this will populate
            // the page) and then reference it.
            m_page = parentPage->owner()->getChild(fontData, pageNumber)->page();
        } else {
            // Get the pure page for the fallback font (at level 1 with no
            // overrides). getRootChild will always create a page if one
            // doesn't exist, but the page doesn't necessarily have glyphs
            // (this pointer may be 0).
            GlyphPage* fallbackPage = getRootChild(fontData, pageNumber)->page();
            if (!parentPage) {
                // When the parent has no glyphs for this page, we can easily
                // override it just by supplying the glyphs from our font.
                m_page = fallbackPage;
            } else if (!fallbackPage) {
                // When our font has no glyphs for this page, we can just reference the
                // parent page.
                m_page = parentPage;
            } else {
                // Combine the parent's glyphs and ours to form a new more complete page.
                m_page = GlyphPage::create(this);

                // Overlay the parent page on the fallback page. Check if the fallback font
                // has added anything.
                bool newGlyphs = false;
                for (unsigned i = 0; i < GlyphPage::size; i++) {
                    if (parentPage->m_glyphs[i].glyph)
                        m_page->m_glyphs[i] = parentPage->m_glyphs[i];
                    else  if (fallbackPage->m_glyphs[i].glyph) {
                        m_page->m_glyphs[i] = fallbackPage->m_glyphs[i];
                        newGlyphs = true;
                    } else {
                        const GlyphData data = { 0, 0 };
                        m_page->m_glyphs[i] = data;
                    }
                }

                if (!newGlyphs)
                    // We didn't override anything, so our override is just the parent page.
                    m_page = parentPage;
            }
        }
    } else {
        m_page = GlyphPage::create(this);
        // System fallback. Initialized with the parent's page here, as individual
        // entries may use different fonts depending on character. If the Font
        // ever finds it needs a glyph out of the system fallback page, it will
        // ask the system for the best font to use and fill that glyph in for us.
        if (parentPage && pageNumber != 6) // don't copy the fallback page for arabic - rdar://6814152 see glyphDataForCharacter in FontFastPath.cpp
            memcpy(m_page->m_glyphs, parentPage->m_glyphs, GlyphPage::size * sizeof(m_page->m_glyphs[0]));
        else {
            const GlyphData data = { 0, 0 };
            for (unsigned i = 0; i < GlyphPage::size; i++)
                m_page->m_glyphs[i] = data;
        }
    }
}

GlyphPageTreeNode* GlyphPageTreeNode::getChild(const FontData* fontData, unsigned pageNumber)
{
    ASSERT(fontData || !m_isSystemFallback);
    ASSERT(pageNumber == m_pageNumber);

    GlyphPageTreeNode* child = fontData ? m_children.get(fontData) : m_systemFallbackChild;
    if (!child) {
        child = new GlyphPageTreeNode;
        child->m_parent = this;
        child->m_level = m_level + 1;
        if (fontData && fontData->isCustomFont()) {
            for (GlyphPageTreeNode* curr = this; curr; curr = curr->m_parent)
                curr->m_customFontCount++;
        }

#ifndef NDEBUG
        child->m_pageNumber = m_pageNumber;
#endif
        if (fontData) {
            m_children.set(fontData, child);
            fontData->setMaxGlyphPageTreeLevel(max(fontData->maxGlyphPageTreeLevel(), child->m_level));
        } else {
            m_systemFallbackChild = child;
            child->m_isSystemFallback = true;
        }
        child->initializePage(fontData, pageNumber);
    }
    return child;
}

void GlyphPageTreeNode::pruneCustomFontData(const FontData* fontData)
{
    if (!fontData || !m_customFontCount)
        return;
        
    // Prune any branch that contains this FontData.
    GlyphPageTreeNode* node = m_children.get(fontData);
    if (node) {
        m_children.remove(fontData);
        unsigned fontCount = node->m_customFontCount + 1;
        delete node;
        for (GlyphPageTreeNode* curr = this; curr; curr = curr->m_parent)
            curr->m_customFontCount -= fontCount;
    }
    
    // Check any branches that remain that still have custom fonts underneath them.
    if (!m_customFontCount)
        return;
    HashMap<const FontData*, GlyphPageTreeNode*>::iterator end = m_children.end();
    for (HashMap<const FontData*, GlyphPageTreeNode*>::iterator it = m_children.begin(); it != end; ++it)
        it->second->pruneCustomFontData(fontData);
}

void GlyphPageTreeNode::pruneFontData(const SimpleFontData* fontData, unsigned level)
{
    ASSERT(fontData);
    if (!fontData)
        return;

    // Prune any branch that contains this FontData.
    HashMap<const FontData*, GlyphPageTreeNode*>::iterator child = m_children.find(fontData);
    if (child == m_children.end()) {
        // If there is no level-1 node for fontData, then there is no deeper node for it in this tree.
        if (!level)
            return;
    } else {
        GlyphPageTreeNode* node = child->second;
        m_children.remove(fontData);
        unsigned customFontCount = node->m_customFontCount;
        delete node;
        if (customFontCount) {
            for (GlyphPageTreeNode* curr = this; curr; curr = curr->m_parent)
                curr->m_customFontCount -= customFontCount;
        }
    }

    level++;
    if (level > fontData->maxGlyphPageTreeLevel())
        return;

    HashMap<const FontData*, GlyphPageTreeNode*>::iterator end = m_children.end();
    for (HashMap<const FontData*, GlyphPageTreeNode*>::iterator it = m_children.begin(); it != end; ++it)
        it->second->pruneFontData(fontData, level);
}

}

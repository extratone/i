/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "Frame.h"

#import "BlockExceptions.h"
#import "ColorMac.h"
#import "Cursor.h"
#import "DOMInternal.h"
#import "DocumentLoader.h"
#import "EditorClient.h"
#import "Event.h"
#import "FrameLoaderClient.h"
#import "FrameView.h"
#import "GraphicsContext.h"
#import "HTMLNames.h"
#import "HTMLTableCellElement.h"
#import "HitTestRequest.h"
#import "HitTestResult.h"
#import "KeyboardEvent.h"
#import "Logging.h"
#import "MouseEventWithHitTestResults.h"
#import "Page.h"
#import "PlatformKeyboardEvent.h"
#import "PlatformWheelEvent.h"
#import "RegularExpression.h"
#import "RenderTableCell.h"
#import "Scrollbar.h"
#import "SimpleFontData.h"
#import "UserStyleSheetLoader.h"
#import "WebCoreViewFactory.h"
#import "visible_units.h"

#import <runtime/JSLock.h>
#import <wtf/StdLibExtras.h>

#if ENABLE(DASHBOARD_SUPPORT)
#import "WebDashboardRegion.h"
#endif

#import "EventListener.h"
#import <GraphicsServices/GraphicsServices.h>
#import <wtf/GetPtr.h>
#import <wtf/UnusedParam.h>

#ifndef NSView
#define NSView WAKView
#endif
#import "WAKView.h"
 
@interface NSView (WebCoreHTMLDocumentView)
- (void)drawSingleRect:(NSRect)rect;
@end
 
using namespace std;

using JSC::JSLock;

namespace WebCore {

using namespace HTMLNames;

// Either get cached regexp or build one that matches any of the labels.
// The regexp we build is of the form:  (STR1|STR2|STRN)
static RegularExpression* regExpForLabels(NSArray* labels)
{
    // All the ObjC calls in this method are simple array and string
    // calls which we can assume do not raise exceptions


    // Parallel arrays that we use to cache regExps.  In practice the number of expressions
    // that the app will use is equal to the number of locales is used in searching.
    static const unsigned int regExpCacheSize = 4;
    static NSMutableArray* regExpLabels = nil;
    DEFINE_STATIC_LOCAL(Vector<RegularExpression*>, regExps, ());
    DEFINE_STATIC_LOCAL(RegularExpression, wordRegExp, ("\\w", TextCaseSensitive));

    RegularExpression* result;
    if (!regExpLabels)
        regExpLabels = [[NSMutableArray alloc] initWithCapacity:regExpCacheSize];
    CFIndex cacheHit = [regExpLabels indexOfObject:labels];
    if (cacheHit != NSNotFound)
        result = regExps.at(cacheHit);
    else {
        String pattern("(");
        unsigned int numLabels = [labels count];
        unsigned int i;
        for (i = 0; i < numLabels; i++) {
            String label = [labels objectAtIndex:i];

            bool startsWithWordChar = false;
            bool endsWithWordChar = false;
            if (label.length() != 0) {
                startsWithWordChar = wordRegExp.match(label.substring(0, 1)) >= 0;
                endsWithWordChar = wordRegExp.match(label.substring(label.length() - 1, 1)) >= 0;
            }
            
            if (i != 0)
                pattern.append("|");
            // Search for word boundaries only if label starts/ends with "word characters".
            // If we always searched for word boundaries, this wouldn't work for languages
            // such as Japanese.
            if (startsWithWordChar)
                pattern.append("\\b");
            pattern.append(label);
            if (endsWithWordChar)
                pattern.append("\\b");
        }
        pattern.append(")");
        result = new RegularExpression(pattern, TextCaseInsensitive);
    }

    // add regexp to the cache, making sure it is at the front for LRU ordering
    if (cacheHit != 0) {
        if (cacheHit != NSNotFound) {
            // remove from old spot
            [regExpLabels removeObjectAtIndex:cacheHit];
            regExps.remove(cacheHit);
        }
        // add to start
        [regExpLabels insertObject:labels atIndex:0];
        regExps.insert(0, result);
        // trim if too big
        if ([regExpLabels count] > regExpCacheSize) {
            [regExpLabels removeObjectAtIndex:regExpCacheSize];
            RegularExpression* last = regExps.last();
            regExps.removeLast();
            delete last;
        }
    }
    return result;
}

NSString* Frame::searchForNSLabelsAboveCell(RegularExpression* regExp, HTMLTableCellElement* cell)
{
    RenderTableCell* cellRenderer = static_cast<RenderTableCell*>(cell->renderer());

    if (cellRenderer && cellRenderer->isTableCell()) {
        RenderTableCell* cellAboveRenderer = cellRenderer->table()->cellAbove(cellRenderer);

        if (cellAboveRenderer) {
            HTMLTableCellElement* aboveCell =
                static_cast<HTMLTableCellElement*>(cellAboveRenderer->element());

            if (aboveCell) {
                // search within the above cell we found for a match
                for (Node* n = aboveCell->firstChild(); n; n = n->traverseNextNode(aboveCell)) {
                    if (n->isTextNode() && n->renderer() && n->renderer()->style()->visibility() == VISIBLE) {
                        // For each text chunk, run the regexp
                        String nodeString = n->nodeValue();
                        int pos = regExp->searchRev(nodeString);
                        if (pos >= 0)
                            return nodeString.substring(pos, regExp->matchedLength());
                    }
                }
            }
        }
    }
    // Any reason in practice to search all cells in that are above cell?
    return nil;
}

NSString* Frame::searchForLabelsBeforeElement(NSArray* labels, Element* element)
{
    RegularExpression* regExp = regExpForLabels(labels);
    // We stop searching after we've seen this many chars
    const unsigned int charsSearchedThreshold = 500;
    // This is the absolute max we search.  We allow a little more slop than
    // charsSearchedThreshold, to make it more likely that we'll search whole nodes.
    const unsigned int maxCharsSearched = 600;
    // If the starting element is within a table, the cell that contains it
    HTMLTableCellElement* startingTableCell = 0;
    bool searchedCellAbove = false;

    // walk backwards in the node tree, until another element, or form, or end of tree
    int unsigned lengthSearched = 0;
    Node* n;
    for (n = element->traversePreviousNode();
         n && lengthSearched < charsSearchedThreshold;
         n = n->traversePreviousNode())
    {
        if (n->hasTagName(formTag)
            || (n->isHTMLElement() && static_cast<Element*>(n)->isFormControlElement()))
        {
            // We hit another form element or the start of the form - bail out
            break;
        } else if (n->hasTagName(tdTag) && !startingTableCell) {
            startingTableCell = static_cast<HTMLTableCellElement*>(n);
        } else if (n->hasTagName(trTag) && startingTableCell) {
            NSString* result = searchForLabelsAboveCell(regExp, startingTableCell);
            if (result && [result length] > 0)
                return result;
            searchedCellAbove = true;
        } else if (n->isTextNode() && n->renderer() && n->renderer()->style()->visibility() == VISIBLE) {
            // For each text chunk, run the regexp
            String nodeString = n->nodeValue();
            // add 100 for slop, to make it more likely that we'll search whole nodes
            if (lengthSearched + nodeString.length() > maxCharsSearched)
                nodeString = nodeString.right(charsSearchedThreshold - lengthSearched);
            int pos = regExp->searchRev(nodeString);
            if (pos >= 0)
                return nodeString.substring(pos, regExp->matchedLength());

            lengthSearched += nodeString.length();
        }
    }

    // If we started in a cell, but bailed because we found the start of the form or the
    // previous element, we still might need to search the row above us for a label.
    if (startingTableCell && !searchedCellAbove) {
        NSString* result = searchForLabelsAboveCell(regExp, startingTableCell);
        if (result && [result length] > 0)
            return result;
    }
    
    return nil;
}

NSString* Frame::matchLabelsAgainstElement(NSArray* labels, Element* element)
{
    String name = element->getAttribute(nameAttr);
    if (name.isEmpty())
        return nil;

    // Make numbers and _'s in field names behave like word boundaries, e.g., "address2"
    replace(name, RegularExpression("\\d", TextCaseSensitive), " ");
    name.replace('_', ' ');

    RegularExpression* regExp = regExpForLabels(labels);
    // Use the largest match we can find in the whole name string
    int pos;
    int length;
    int bestPos = -1;
    int bestLength = -1;
    int start = 0;
    do {
        pos = regExp->match(name, start);
        if (pos != -1) {
            length = regExp->matchedLength();
            if (length >= bestLength) {
                bestPos = pos;
                bestLength = length;
            }
            start = pos + 1;
        }
    } while (pos != -1);

    if (bestPos != -1)
        return name.substring(bestPos, bestLength);
    return nil;
}


NSDictionary* Frame::fontAttributesForSelectionStart() const
{
    Node* nodeToRemove;
    RenderStyle* style = styleForSelectionStart(nodeToRemove);
    if (!style)
        return nil;

    NSMutableDictionary* result = [NSMutableDictionary dictionary];


    return result;
}

NSWritingDirection Frame::baseWritingDirectionForSelectionStart() const
{
    // Convert WebCore's WritingDirection into an NSWritingDirection.
    WritingDirection direction = editor()->baseWritingDirectionForSelectionStart();
    ASSERT(direction != NaturalWritingDirection);
    return direction == LeftToRightWritingDirection ? NSWritingDirectionLeftToRight : NSWritingDirectionRightToLeft;
}

const short enableRomanKeyboardsOnly = -23;
void Frame::setUseSecureKeyboardEntry(bool enable)
{
    UNUSED_PARAM(enable);
}

#if ENABLE(DASHBOARD_SUPPORT)
NSMutableDictionary* Frame::dashboardRegionsDictionary()
{
    Document* doc = document();
    if (!doc)
        return nil;

    const Vector<DashboardRegionValue>& regions = doc->dashboardRegions();
    size_t n = regions.size();

    // Convert the Vector<DashboardRegionValue> into a NSDictionary of WebDashboardRegions
    NSMutableDictionary* webRegions = [NSMutableDictionary dictionaryWithCapacity:n];
    for (size_t i = 0; i < n; i++) {
        const DashboardRegionValue& region = regions[i];

        if (region.type == StyleDashboardRegion::None)
            continue;
        
        NSString *label = region.label;
        WebDashboardRegionType type = WebDashboardRegionTypeNone;
        if (region.type == StyleDashboardRegion::Circle)
            type = WebDashboardRegionTypeCircle;
        else if (region.type == StyleDashboardRegion::Rectangle)
            type = WebDashboardRegionTypeRectangle;
        NSMutableArray *regionValues = [webRegions objectForKey:label];
        if (!regionValues) {
            regionValues = [[NSMutableArray alloc] initWithCapacity:1];
            [webRegions setObject:regionValues forKey:label];
            [regionValues release];
        }
        
        WebDashboardRegion *webRegion = [[WebDashboardRegion alloc] initWithRect:region.bounds clip:region.clip type:type];
        [regionValues addObject:webRegion];
        [webRegion release];
    }
    
    return webRegions;
}
#endif

DragImageRef Frame::dragImageForSelection() 
{
    return nil;
}

#define NSFloatValue(aFloat) [NSNumber numberWithFloat:aFloat]

NSDictionary* Frame::dictionaryForViewportArguments(const ViewportArguments& arguments) const
{
    return [NSDictionary dictionaryWithObjects:[NSArray arrayWithObjects:NSFloatValue(arguments.initialScale), NSFloatValue(arguments.minimumScale), 
                                                NSFloatValue(arguments.maximumScale), NSFloatValue(arguments.userScalable), 
                                                NSFloatValue(arguments.width), NSFloatValue(arguments.height), nil] 
                                       forKeys:[NSArray arrayWithObjects:@"initial-scale", @"minimum-scale", @"maximum-scale", @"user-scalable", @"width", @"height", nil]];
}

const ViewportArguments& Frame::viewportArguments() const
{
    return m_viewportArguments;
}

void Frame::setViewportArguments(const ViewportArguments& arguments)
{
    m_viewportArguments = arguments;
}

#if ENABLE(IPHONE_PPT)
static int sAllLayouts = 0;

void Frame::didParse(double duration)
{
    m_parseCount++;
    m_parseDuration += duration;
}

void Frame::didLayout(bool /*firstLayout*/, double duration)
{
    sAllLayouts++;
    m_layoutCount++;
    m_layoutDuration += duration;
}

void Frame::didForcedLayout()
{
    m_forcedLayoutCount++;
}

void Frame::getPPTStats(unsigned& parseCount, unsigned& layoutCount, unsigned& forcedLayoutCount, CFTimeInterval& parseDuration, CFTimeInterval& layoutDuration)
{
    parseCount = m_parseCount;
    layoutCount = m_layoutCount;
    forcedLayoutCount = m_forcedLayoutCount;
    parseDuration = m_parseDuration;
    layoutDuration = m_layoutDuration;

//fprintf(stderr, "All Layouts: %d\n", sAllLayouts);    
}

void Frame::clearPPTStats()
{
    m_parseCount = 0;
    m_layoutCount = 0;
    m_forcedLayoutCount = 0;
    m_parseDuration = 0.0;
    m_layoutDuration = 0.0;
}
#endif // ENABLE(IPHONE_PPT)

void Frame::setUserStyleSheetLocation(const KURL& url)
{
    delete m_userStyleSheetLoader;
    m_userStyleSheetLoader = 0;
    if (m_doc && m_doc->docLoader())
        m_userStyleSheetLoader = new UserStyleSheetLoader(m_doc, url.string());
}

void Frame::setUserStyleSheet(const String& styleSheet, bool saveStyleSheet)
{
    if (saveStyleSheet)
        m_userStyleSheet = styleSheet;

    delete m_userStyleSheetLoader;
    m_userStyleSheetLoader = 0;
    if (m_doc)
        m_doc->setUserStyleSheet(styleSheet);
}

} // namespace WebCore

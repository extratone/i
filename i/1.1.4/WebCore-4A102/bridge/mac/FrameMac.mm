/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
#import "FrameMac.h"

#import "AXObjectCache.h"
#import "BeforeUnloadEvent.h"
#import "BlockExceptions.h"
#import "BrowserExtensionMac.h"
#import "CSSComputedStyleDeclaration.h"
#import "Cache.h"
#import "ClipboardEvent.h"
#import "Cursor.h"
#import "DOMInternal.h"
#import "DOMWindow.h"
#import "Decoder.h"
#import "Event.h"
#import "EventNames.h"
#import "FloatRect.h"
#import "FoundationExtras.h"
#import "FramePrivate.h"
#import "GraphicsContext.h"
#import "HTMLDocument.h"
#import "HTMLFormElement.h"
#import "HTMLFrameElement.h"
#import "HTMLGenericFormElement.h"
#import "HTMLNames.h"
#import "HTMLTableCellElement.h"
#import "WebCoreEditCommand.h"
#import "FormDataMac.h"
#import "WebCorePageState.h"
#import "Logging.h"
#import "MouseEventWithHitTestResults.h"
#import "PlatformKeyboardEvent.h"
#import "PlatformWheelEvent.h"
#import "Plugin.h"
#import "RegularExpression.h"
#import "RenderImage.h"
#import "RenderListItem.h"
#import "RenderPart.h"
#import "RenderTableCell.h"
#import "RenderTheme.h"
#import "RenderView.h"
#import "TextIterator.h"
#import "TransferJob.h"
#import "WebCoreFrameBridge.h"
#import "WebCoreViewFactory.h"
#import "WebDashboardRegion.h"
#import "WebScriptObjectPrivate.h"
#import "csshelper.h"
#import "htmlediting.h"
#import "kjs_window.h"
#import "visible_units.h"

#undef _webcore_TIMING

@interface NSObject (WebPlugIn)
- (id)objectForWebScript;
- (void *)pluginScriptableObject;
@end

using namespace std;
using namespace KJS::Bindings;

using KJS::JSLock;
using KJS::PausedTimeouts;
using KJS::SavedBuiltins;
using KJS::SavedProperties;

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

GSEventRef FrameMac::_currentEvent = nil;

static NSMutableDictionary* createNSDictionary(const HashMap<String, String>& map)
{
    NSMutableDictionary* dict = [[NSMutableDictionary alloc] initWithCapacity:map.size()];
    HashMap<String, String>::const_iterator end = map.end();
    for (HashMap<String, String>::const_iterator it = map.begin(); it != end; ++it) {
        NSString* key = it->first;
        NSString* object = it->second;
        [dict setObject:object forKey:key];
    }
    return dict;
}


bool FrameView::isFrameView() const
{
    return true;
}

FrameMac::FrameMac(Page* page, Element* ownerElement)
    : Frame(page, ownerElement)
    , _bridge(nil)
    , _mouseDownView(nil)
    , _sendingEventToSubview(false)
    , _mouseDownMayStartDrag(false)
    , _mouseDownMayStartSelect(false)
    , _activationEventNumber(0)
#if BINDINGS
    , _bindingRoot(0)
    , _windowScriptObject(0)
#endif
#if NETSCAPE_API
    , _windowScriptNPObject(0)
#endif
{
    d->m_extension = new BrowserExtensionMac(this);
}

FrameMac::~FrameMac()
{
    setView(0);
    clearRecordedFormValues();
    
    [_bridge clearFrame];
    HardRelease(_bridge);
    _bridge = nil;
}


bool FrameMac::openURL(const KURL &url)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    // FIXME: The lack of args here to get the reload flag from
    // indicates a problem in how we use Frame::processObjectRequest,
    // where we are opening the URL before the args are set up.
    [_bridge loadURL:url.getNSURL()
            referrer:[_bridge referrer]
              reload:NO
         userGesture:userGestureHint()
              target:nil
     triggeringEvent:nil
                form:nil
          formValues:nil];

    END_BLOCK_OBJC_EXCEPTIONS;

    return true;
}

void FrameMac::openURLRequest(const ResourceRequest& request)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSString *referrer;
    String argsReferrer = request.referrer();
    if (!argsReferrer.isEmpty())
        referrer = argsReferrer;
    else
        referrer = [_bridge referrer];

    [_bridge loadURL:request.url().getNSURL()
            referrer:referrer
              reload:request.reload
         userGesture:userGestureHint()
              target:request.frameName
     triggeringEvent:nil
                form:nil
          formValues:nil];

    END_BLOCK_OBJC_EXCEPTIONS;
}


// Either get cached regexp or build one that matches any of the labels.
// The regexp we build is of the form:  (STR1|STR2|STRN)
RegularExpression *regExpForLabels(NSArray *labels)
{
    // All the ObjC calls in this method are simple array and string
    // calls which we can assume do not raise exceptions


    // Parallel arrays that we use to cache regExps.  In practice the number of expressions
    // that the app will use is equal to the number of locales is used in searching.
    static const unsigned int regExpCacheSize = 4;
    static NSMutableArray *regExpLabels = nil;
    static Vector<RegularExpression*> regExps;
    static RegularExpression wordRegExp = RegularExpression("\\w");

    RegularExpression *result;
    if (!regExpLabels)
        regExpLabels = [[NSMutableArray alloc] initWithCapacity:regExpCacheSize];
    CFIndex cacheHit = [regExpLabels indexOfObject:labels];
    if (cacheHit != NSNotFound)
        result = regExps.at(cacheHit);
    else {
        DeprecatedString pattern("(");
        unsigned int numLabels = [labels count];
        unsigned int i;
        for (i = 0; i < numLabels; i++) {
            DeprecatedString label = DeprecatedString::fromNSString((NSString *)[labels objectAtIndex:i]);

            bool startsWithWordChar = false;
            bool endsWithWordChar = false;
            if (label.length() != 0) {
                startsWithWordChar = wordRegExp.search(label.at(0)) >= 0;
                endsWithWordChar = wordRegExp.search(label.at(label.length() - 1)) >= 0;
            }
            
            if (i != 0)
                pattern.append("|");
            // Search for word boundaries only if label starts/ends with "word characters".
            // If we always searched for word boundaries, this wouldn't work for languages
            // such as Japanese.
            if (startsWithWordChar) {
                pattern.append("\\b");
            }
            pattern.append(label);
            if (endsWithWordChar) {
                pattern.append("\\b");
            }
        }
        pattern.append(")");
        result = new RegularExpression(pattern, false);
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
            RegularExpression *last = regExps.last();
            regExps.removeLast();
            delete last;
        }
    }
    return result;
}

NSString* FrameMac::searchForLabelsAboveCell(RegularExpression* regExp, HTMLTableCellElement* cell)
{
    RenderTableCell* cellRenderer = static_cast<RenderTableCell*>(cell->renderer());

    if (cellRenderer && cellRenderer->isTableCell()) {
        RenderTableCell* cellAboveRenderer = cellRenderer->table()->cellAbove(cellRenderer);

        if (cellAboveRenderer) {
            HTMLTableCellElement *aboveCell =
                static_cast<HTMLTableCellElement*>(cellAboveRenderer->element());

            if (aboveCell) {
                // search within the above cell we found for a match
                for (Node *n = aboveCell->firstChild(); n; n = n->traverseNextNode(aboveCell)) {
                    if (n->isTextNode() && n->renderer() && n->renderer()->style()->visibility() == VISIBLE) {
                        // For each text chunk, run the regexp
                        DeprecatedString nodeString = n->nodeValue().deprecatedString();
                        int pos = regExp->searchRev(nodeString);
                        if (pos >= 0)
                            return nodeString.mid(pos, regExp->matchedLength()).getNSString();
                    }
                }
            }
        }
    }
    // Any reason in practice to search all cells in that are above cell?
    return nil;
}

NSString *FrameMac::searchForLabelsBeforeElement(NSArray *labels, Element *element)
{
    RegularExpression *regExp = regExpForLabels(labels);
    // We stop searching after we've seen this many chars
    const unsigned int charsSearchedThreshold = 500;
    // This is the absolute max we search.  We allow a little more slop than
    // charsSearchedThreshold, to make it more likely that we'll search whole nodes.
    const unsigned int maxCharsSearched = 600;
    // If the starting element is within a table, the cell that contains it
    HTMLTableCellElement *startingTableCell = 0;
    bool searchedCellAbove = false;

    // walk backwards in the node tree, until another element, or form, or end of tree
    int unsigned lengthSearched = 0;
    Node *n;
    for (n = element->traversePreviousNode();
         n && lengthSearched < charsSearchedThreshold;
         n = n->traversePreviousNode())
    {
        if (n->hasTagName(formTag)
            || (n->isHTMLElement()
                && static_cast<HTMLElement*>(n)->isGenericFormElement()))
        {
            // We hit another form element or the start of the form - bail out
            break;
        } else if (n->hasTagName(tdTag) && !startingTableCell) {
            startingTableCell = static_cast<HTMLTableCellElement*>(n);
        } else if (n->hasTagName(trTag) && startingTableCell) {
            NSString *result = searchForLabelsAboveCell(regExp, startingTableCell);
            if (result) {
                return result;
            }
            searchedCellAbove = true;
        } else if (n->isTextNode() && n->renderer() && n->renderer()->style()->visibility() == VISIBLE) {
            // For each text chunk, run the regexp
            DeprecatedString nodeString = n->nodeValue().deprecatedString();
            // add 100 for slop, to make it more likely that we'll search whole nodes
            if (lengthSearched + nodeString.length() > maxCharsSearched)
                nodeString = nodeString.right(charsSearchedThreshold - lengthSearched);
            int pos = regExp->searchRev(nodeString);
            if (pos >= 0)
                return nodeString.mid(pos, regExp->matchedLength()).getNSString();
            else
                lengthSearched += nodeString.length();
        }
    }

    // If we started in a cell, but bailed because we found the start of the form or the
    // previous element, we still might need to search the row above us for a label.
    if (startingTableCell && !searchedCellAbove) {
         return searchForLabelsAboveCell(regExp, startingTableCell);
    } else {
        return nil;
    }
}

NSString *FrameMac::matchLabelsAgainstElement(NSArray *labels, Element *element)
{
    DeprecatedString name = element->getAttribute(nameAttr).deprecatedString();
    // Make numbers and _'s in field names behave like word boundaries, e.g., "address2"
    name.replace(RegularExpression("[[:digit:]]"), " ");
    name.replace('_', ' ');
    
    RegularExpression *regExp = regExpForLabels(labels);
    // Use the largest match we can find in the whole name string
    int pos;
    int length;
    int bestPos = -1;
    int bestLength = -1;
    int start = 0;
    do {
        pos = regExp->search(name, start);
        if (pos != -1) {
            length = regExp->matchedLength();
            if (length >= bestLength) {
                bestPos = pos;
                bestLength = length;
            }
            start = pos+1;
        }
    } while (pos != -1);

    if (bestPos != -1)
        return name.mid(bestPos, bestLength).getNSString();
    return nil;
}

void FrameMac::submitForm(const ResourceRequest& request)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    // FIXME: We'd like to remove this altogether and fix the multiple form submission issue another way.
    // We do not want to submit more than one form from the same page,
    // nor do we want to submit a single form more than once.
    // This flag prevents these from happening; not sure how other browsers prevent this.
    // The flag is reset in each time we start handle a new mouse or key down event, and
    // also in setView since this part may get reused for a page from the back/forward cache.
    // The form multi-submit logic here is only needed when we are submitting a form that affects this frame.
    // FIXME: Frame targeting is only one of the ways the submission could end up doing something other
    // than replacing this frame's content, so this check is flawed. On the other hand, the check is hardly
    // needed any more now that we reset d->m_submittedFormURL on each mouse or key down event.
    WebCoreFrameBridge *target = request.frameName.isEmpty() ? _bridge : [_bridge findFrameNamed:request.frameName];
    Frame *targetPart = [target impl];
    bool willReplaceThisFrame = false;
    for (Frame *p = this; p; p = p->tree()->parent()) {
        if (p == targetPart) {
            willReplaceThisFrame = true;
            break;
        }
    }
    if (willReplaceThisFrame) {
        if (d->m_submittedFormURL == request.url())
            return;
        d->m_submittedFormURL = request.url();
    }

    ObjCDOMElement* submitForm = [DOMElement _elementWith:d->m_formAboutToBeSubmitted.get()];
    NSMutableDictionary* formValues = createNSDictionary(d->m_formValuesAboutToBeSubmitted);
    
    if (!request.doPost()) {
        [_bridge loadURL:request.url().getNSURL()
                referrer:[_bridge referrer] 
                  reload:request.reload
             userGesture:true
                  target:request.frameName
         triggeringEvent:_currentEvent
                    form:submitForm
              formValues:formValues];
    } else {
        ASSERT(request.contentType().startsWith("Content-Type: "));
        [_bridge postWithURL:request.url().getNSURL()
                    referrer:[_bridge referrer] 
                      target:request.frameName
                        data:arrayFromFormData(request.postData)
                 contentType:request.contentType().substring(14)
             triggeringEvent:_currentEvent
                        form:submitForm
                  formValues:formValues];
    }
    [formValues release];
    clearRecordedFormValues();

    END_BLOCK_OBJC_EXCEPTIONS;
}

void FrameMac::frameDetached()
{
    Frame::frameDetached();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [Mac(this)->bridge() frameDetached];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void FrameMac::urlSelected(const ResourceRequest& request)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSString* referrer;
    String argsReferrer = request.referrer();
    if (!argsReferrer.isEmpty())
        referrer = argsReferrer;
    else
        referrer = [_bridge referrer];

    [_bridge loadURL:request.url().getNSURL()
            referrer:referrer
              reload:request.reload
         userGesture:true
              target:request.frameName
     triggeringEvent:_currentEvent
                form:nil
          formValues:nil];

    END_BLOCK_OBJC_EXCEPTIONS;
}

ObjectContentType FrameMac::objectContentType(const KURL& url, const String& mimeType)
{
    return (ObjectContentType)[_bridge determineObjectFromMIMEType:mimeType URL:url.getNSURL()];
}

static NSArray* nsArray(const Vector<String>& vector)
{
    unsigned len = vector.size();
    NSMutableArray* array = [NSMutableArray arrayWithCapacity:len];
    for (unsigned x = 0; x < len; x++)
        [array addObject:vector[x]];
    return array;
}

Plugin* FrameMac::createPlugin(Element* element, const KURL& url, const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    return new Plugin(new Widget([_bridge viewForPluginWithURL:url.getNSURL()
                                  attributeNames:nsArray(paramNames)
                                  attributeValues:nsArray(paramValues)
                                  MIMEType:mimeType
                                  DOMElement:(element ? [DOMElement _elementWith:element] : nil)
                                loadManually:d->m_doc->isPluginDocument()]));

    END_BLOCK_OBJC_EXCEPTIONS;
    return 0;
}

void FrameMac::redirectDataToPlugin(Widget* pluginWidget)
{
    [_bridge redirectDataToPlugin:pluginWidget->getView()];
}


Frame* FrameMac::createFrame(const KURL& url, const String& name, Element* ownerElement, const String& referrer)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    
    BOOL allowsScrolling = YES;
    int marginWidth = -1;
    int marginHeight = -1;
    if (ownerElement->hasTagName(frameTag) || ownerElement->hasTagName(iframeTag)) {
        HTMLFrameElement* o = static_cast<HTMLFrameElement*>(ownerElement);
        allowsScrolling = o->scrollingMode() != ScrollBarAlwaysOff;
        marginWidth = o->getMarginWidth();
        marginHeight = o->getMarginHeight();
    }

    WebCoreFrameBridge *childBridge = [_bridge createChildFrameNamed:name
                                                             withURL:url.getNSURL()
                                                            referrer:referrer 
                                                          ownerElement:ownerElement
                                                     allowsScrolling:allowsScrolling
                                                         marginWidth:marginWidth
                                                        marginHeight:marginHeight];
    return [childBridge impl];

    END_BLOCK_OBJC_EXCEPTIONS;
    return 0;
}

void FrameMac::setView(FrameView *view)
{
    // Detach the document now, so any onUnload handlers get run - if
    // we wait until the view is destroyed, then things won't be
    // hooked up enough for some JavaScript calls to work.
    if (d->m_doc && view == 0)
        d->m_doc->detach();
    
    d->m_view = view;
    
    // Delete old PlugIn data structures
    cleanupPluginRootObjects();
    _bindingRoot = 0;
    HardRelease(_windowScriptObject);
    _windowScriptObject = 0;


    // Only one form submission is allowed per view of a part.
    // Since this part may be getting reused as a result of being
    // pulled from the back/forward cache, reset this flag.
    d->m_submittedFormURL = KURL();
}

void FrameMac::setTitle(const String &title)
{
    String text = title;
    text.replace('\\', backslashAsCurrencySymbol());

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [_bridge setTitle:text];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void FrameMac::setStatusBarText(const String& status)
{
    String text = status;
    text.replace('\\', backslashAsCurrencySymbol());
    
    // We want the temporaries allocated here to be released even before returning to the 
    // event loop; see <http://bugzilla.opendarwin.org/show_bug.cgi?id=9880>.
    NSAutoreleasePool* localPool = [[NSAutoreleasePool alloc] init];

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [_bridge setStatusText:text];
    END_BLOCK_OBJC_EXCEPTIONS;

    [localPool release];
}

void FrameMac::scheduleClose()
{
    if (!shouldClose())
        return;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [_bridge closeWindowSoon];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void FrameMac::focusWindow()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    // If we're a top level window, bring the window to the front.
    if (!tree()->parent()) {
        [_bridge activateWindow:userGestureHint()];
    }

    // Might not have a view yet: this could be a child frame that has not yet received its first byte of data.
    // FIXME: Should remember that the frame needs focus.  See <rdar://problem/4645685>.
    if (d->m_view) {
        NSView *view = d->m_view->getDocumentView();
        if ([_bridge firstResponder] != view)
            [_bridge makeFirstResponder:view];
    }

    END_BLOCK_OBJC_EXCEPTIONS;
}

void FrameMac::unfocusWindow()
{
    // Might not have a view yet: this could be a child frame that has not yet received its first byte of data.
    // FIXME: Should remember that the frame needs to unfocus.  See <rdar://problem/4645685>.
    if (!d->m_view)
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    NSView *view = d->m_view->getDocumentView();
    if ([_bridge firstResponder] == view) {
        // If we're a top level window, deactivate the window.
        if (!tree()->parent())
            [_bridge deactivateWindow];
        else {
            // We want to shift focus to our parent.
            FrameMac* parentFrame = static_cast<FrameMac*>(tree()->parent());
            NSView* parentView = parentFrame->d->m_view->getDocumentView();
            [parentFrame->bridge() makeFirstResponder:parentView];
        }
    }
    END_BLOCK_OBJC_EXCEPTIONS;
}


bool FrameMac::wheelEvent(GSEventRef event)
{
    FrameView *v = d->m_view.get();

    if (v) {
        GSEventRef oldCurrentEvent = _currentEvent;
        _currentEvent = (GSEventRef)CFRetain(event);
        PlatformWheelEvent qEvent(event);
        v->handleWheelEvent(qEvent);

        ASSERT(_currentEvent == event);
        CFRelease (event);
        _currentEvent = oldCurrentEvent;

        if (qEvent.isAccepted())
            return true;
    }

    // FIXME: The scrolling done here should be done in the default handlers
    // of the elements rather than here in the part.

    ScrollDirection direction;
    float multiplier;
    float deltaX = GSEventGetDeltaX (event);
    float deltaY = GSEventGetDeltaY (event);
    if (deltaX < 0) {
        direction = ScrollRight;
        multiplier = -deltaX;
    } else if (deltaX > 0) {
        direction = ScrollLeft;
        multiplier = deltaX;
    } else if (deltaY < 0) {
        direction = ScrollDown;
        multiplier = -deltaY;
    }  else if (deltaY > 0) {
        direction = ScrollUp;
        multiplier = deltaY;
    } else
        return false;

    RenderObject *r = renderer();
    if (!r)
        return false;
    
    CGPoint p = GSEventGetLocationInWindow(event);
    NSPoint point = [d->m_view->getDocumentView() convertPoint:p fromView:nil];
    RenderObject::NodeInfo nodeInfo(true, true);
    r->layer()->hitTest(nodeInfo, IntPoint(point));    
    
    Node *node = nodeInfo.innerNode();
    if (!node)
        return false;
    
    r = node->renderer();
    if (!r)
        return false;

    return r->scroll(direction, ScrollByWheel, multiplier);
}

void FrameMac::startRedirectionTimer()
{
    stopRedirectionTimer();

    Frame::startRedirectionTimer();

    // Don't report history navigations, just actual redirection.
    if (d->m_scheduledRedirection != historyNavigationScheduled) {
        NSTimeInterval interval = d->m_redirectionTimer.nextFireInterval();
        NSDate *fireDate = [[NSDate alloc] initWithTimeIntervalSinceNow:interval];
        [_bridge reportClientRedirectToURL:KURL(d->m_redirectURL).getNSURL()
                                     delay:d->m_delayRedirect
                                  fireDate:fireDate
                               lockHistory:d->m_redirectLockHistory
                               isJavaScriptFormAction:d->m_executingJavaScriptFormAction];
        [fireDate release];
    }
}

void FrameMac::stopRedirectionTimer()
{
    bool wasActive = d->m_redirectionTimer.isActive();

    Frame::stopRedirectionTimer();

    // Don't report history navigations, just actual redirection.
    if (wasActive && d->m_scheduledRedirection != historyNavigationScheduled)
        [_bridge reportClientRedirectCancelled:d->m_cancelWithLoadInProgress];
}

String FrameMac::userAgent() const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [_bridge userAgentForURL:url().getNSURL()];
    END_BLOCK_OBJC_EXCEPTIONS;
         
    return String();
}

String FrameMac::mimeTypeForFileName(const String& fileName) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [_bridge MIMETypeForPath:fileName];
    END_BLOCK_OBJC_EXCEPTIONS;

    return String();
}

NSView* FrameMac::nextKeyViewInFrame(Node* n, SelectionDirection direction, bool* focusCallResultedInViewBeingCreated)
{
    Document* doc = document();
    if (!doc)
        return nil;
    
    RefPtr<Node> node = n;
    for (;;) {
        node = direction == SelectingNext
            ? doc->nextFocusNode(node.get()) : doc->previousFocusNode(node.get());
        if (!node)
            return nil;
        
        RenderObject* renderer = node->renderer();
        
        if (!renderer->isWidget()) {
            static_cast<Element*>(node.get())->focus(); 
            // The call to focus might have triggered event handlers that causes the 
            // current renderer to be destroyed.
            if (!(renderer = node->renderer()))
                continue;
                
            // FIXME: When all input elements are native, we should investigate if this extra check is needed
            if (!renderer->isWidget()) {
                [_bridge willMakeFirstResponderForNodeFocus];
                return [_bridge documentView];
            } else if (focusCallResultedInViewBeingCreated)
                *focusCallResultedInViewBeingCreated = true;
        }

        if (Widget* widget = static_cast<RenderWidget*>(renderer)->widget()) {
            NSView* view;
            if (widget->isFrameView())
                view = Mac(static_cast<FrameView*>(widget)->frame())->nextKeyViewInFrame(0, direction);
            else
                view = widget->getView();
            if (view)
                return view;
        }
    }
}

NSView *FrameMac::nextKeyViewInFrameHierarchy(Node *node, SelectionDirection direction)
{
    bool focusCallResultedInViewBeingCreated = false;
    NSView *next = nextKeyViewInFrame(node, direction, &focusCallResultedInViewBeingCreated);
    if (!next)
        if (FrameMac *parent = Mac(tree()->parent()))
            next = parent->nextKeyViewInFrameHierarchy(ownerElement(), direction);
    
    // remove focus from currently focused node if we're giving focus to another view
    // unless the other view was created as a result of calling focus in nextKeyViewWithFrame.
    // FIXME: The focusCallResultedInViewBeingCreated calls can be removed when all input element types
    // have been made native.
    if (next && (next != [_bridge documentView] && !focusCallResultedInViewBeingCreated))
        if (Document *doc = document())
            doc->setFocusNode(0);

    // The common case where a view was created is when an <input> element changed from native 
    // to non-native. When this happens, HTMLGenericFormElement::attach() method will call setFocus()
    // on the widget. For views with a field editor, setFocus() will set the active responder to be the field editor. 
    // In this case, we want to return the field editor as the next key view. Otherwise, the focus will be lost
    // and a blur message will be sent. 
    // FIXME: This code can be removed when all input element types are native.
    
    return next;
}

NSView *FrameMac::nextKeyView(Node *node, SelectionDirection direction)
{
    NSView * next;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    next = nextKeyViewInFrameHierarchy(node, direction);
    if (next)
        return next;

    // Look at views from the top level part up, looking for a next key view that we can use.

    next = direction == SelectingNext
        ? [_bridge nextKeyViewOutsideWebFrameViews]
        : [_bridge previousKeyViewOutsideWebFrameViews];

    if (next)
        return next;

    END_BLOCK_OBJC_EXCEPTIONS;
    
    // If all else fails, make a loop by starting from 0.
    return nextKeyViewInFrameHierarchy(0, direction);
}

NSView *FrameMac::nextKeyViewForWidget(Widget *startingWidget, SelectionDirection direction)
{
    // Use the event filter object to figure out which RenderWidget owns this Widget and get to the DOM.
    // Then get the next key view in the order determined by the DOM.
    Node *node = nodeForWidget(startingWidget);
    ASSERT(node);
    return Mac(frameForNode(node))->nextKeyView(node, direction);
}

bool FrameMac::currentEventIsMouseDownInWidget(Widget *candidate)
{
    return false;
}

bool FrameMac::currentEventIsKeyboardOptionTab()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    GSEventRef evt = WKEventGetCurrentEvent();
    if (!evt)
        return false;
    
    if (GSEventGetType (evt) != kGSEventKeyDown) {
        return false;
    }

    if ((GSEventGetModifierFlags(evt) & kGSEventFlagMaskAlternate) == 0) {
        return false;
    }
    
    NSString *chars = (NSString *)GSEventCopyCharactersIgnoringModifiers (evt);
	[chars autorelease];
    if ([chars length] != 1)
        return NO;
    
    const unichar tabKey = 0x0009;
    const unichar shiftTabKey = 0x0019;
    unichar c = [chars characterAtIndex:0];
    if (c != tabKey && c != shiftTabKey)
        return NO;
    
    END_BLOCK_OBJC_EXCEPTIONS;
    return YES;
}

bool FrameMac::handleKeyboardOptionTabInView(NSView *view)
{
    return true;
}

bool FrameMac::tabsToLinks() const
{
    if ([_bridge keyboardUIMode] & WebCoreKeyboardAccessTabsToLinks)
        return !FrameMac::currentEventIsKeyboardOptionTab();
    else
        return FrameMac::currentEventIsKeyboardOptionTab();
}

bool FrameMac::tabsToAllControls() const
{
    WebCoreKeyboardUIMode keyboardUIMode = [_bridge keyboardUIMode];
    BOOL handlingOptionTab = FrameMac::currentEventIsKeyboardOptionTab();

    // If tab-to-links is off, option-tab always highlights all controls
    if ((keyboardUIMode & WebCoreKeyboardAccessTabsToLinks) == 0 && handlingOptionTab) {
        return YES;
    }
    
    // If system preferences say to include all controls, we always include all controls
    if (keyboardUIMode & WebCoreKeyboardAccessFull) {
        return YES;
    }
    
    // Otherwise tab-to-links includes all controls, unless the sense is flipped via option-tab.
    if (keyboardUIMode & WebCoreKeyboardAccessTabsToLinks) {
        return !handlingOptionTab;
    }
    
    return handlingOptionTab;
}

KJS::Bindings::RootObject *FrameMac::executionContextForDOM()
{
    if (!jScriptEnabled())
        return 0;

    return bindingRootObject();
}

KJS::Bindings::RootObject *FrameMac::bindingRootObject()
{
    assert(jScriptEnabled());
    if (!_bindingRoot) {
        JSLock lock;
        _bindingRoot = new KJS::Bindings::RootObject(0);    // The root gets deleted by JavaScriptCore.
        KJS::JSObject *win = KJS::Window::retrieveWindow(this);
        _bindingRoot->setRootObjectImp (win);
        _bindingRoot->setInterpreter(jScript()->interpreter());
        addPluginRootObject (_bindingRoot);
    }
    return _bindingRoot;
}

WebScriptObject *FrameMac::windowScriptObject()
{
    if (!jScriptEnabled())
        return 0;

    if (!_windowScriptObject) {
        KJS::JSLock lock;
        KJS::JSObject *win = KJS::Window::retrieveWindow(this);
        _windowScriptObject = HardRetainWithNSRelease([[WebScriptObject alloc] _initWithJSObject:win originExecutionContext:bindingRootObject() executionContext:bindingRootObject()]);
    }

    return _windowScriptObject;
}


void FrameMac::partClearedInBegin()
{
    if (jScriptEnabled())
        [_bridge windowObjectCleared];
}

void FrameMac::openURLFromPageCache(WebCorePageState *state)
{
    // It's safe to assume none of the WebCorePageState methods will raise
    // exceptions, since WebCorePageState is implemented by WebCore and
    // does not throw

    Document *doc = [state document];
    Node *mousePressNode = [state mousePressNode];
    KURL *kurl = [state URL];
    SavedProperties *windowProperties = [state windowProperties];
    SavedProperties *locationProperties = [state locationProperties];
    SavedBuiltins *interpreterBuiltins = [state interpreterBuiltins];
    PausedTimeouts *timeouts = [state pausedTimeouts];
    
    cancelRedirection();

    // We still have to close the previous part page.
    closeURL();
            
    d->m_bComplete = false;
    
    // Don't re-emit the load event.
    d->m_bLoadEventEmitted = true;
    
    // delete old status bar msg's from kjs (if it _was_ activated on last URL)
    if (jScriptEnabled()) {
        d->m_kjsStatusBarText = String();
        d->m_kjsDefaultStatusBarText = String();
    }

    ASSERT(kurl);
    
    d->m_url = *kurl;
    
    // initializing m_url to the new url breaks relative links when opening such a link after this call and _before_ begin() is called (when the first
    // data arrives) (Simon)
    if (url().protocol().startsWith("http") && !url().host().isEmpty() && url().path().isEmpty())
        d->m_url.setPath("/");
    
    // copy to m_workingURL after fixing url() above
    d->m_workingURL = url();
        
    started();
    
    // -----------begin-----------
    clear();

    doc->setInPageCache(NO);

    d->m_bCleared = false;
    d->m_bComplete = false;
    d->m_bLoadEventEmitted = false;
    d->m_referrer = url().url();
    
    setView(doc->view());
    
    d->m_doc = doc;
    d->m_mousePressNode = mousePressNode;
    d->m_decoder = doc->decoder();

    updatePolicyBaseURL();

    { // scope the lock
        JSLock lock;
        restoreWindowProperties(windowProperties);
        restoreLocationProperties(locationProperties);
        restoreInterpreterBuiltins(*interpreterBuiltins);
    }

    resumeTimeouts(timeouts);
    
    checkCompleted();
}

WebCoreFrameBridge *FrameMac::bridgeForWidget(const Widget *widget)
{
    ASSERT_ARG(widget, widget);
    
    FrameMac *frame = Mac(frameForWidget(widget));
    ASSERT(frame);
    return frame->bridge();
}

NSView *FrameMac::documentViewForNode(Node *node)
{
    WebCoreFrameBridge *bridge = Mac(frameForNode(node))->bridge();
    return [bridge documentView];
}

void FrameMac::saveDocumentState()
{
    // Do not save doc state if the page has a password field and a form that would be submitted
    // via https
    if (!(d->m_doc && d->m_doc->hasSecureForm())) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS;
        [_bridge saveDocumentState];
        END_BLOCK_OBJC_EXCEPTIONS;
    }
}

void FrameMac::restoreDocumentState()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [_bridge restoreDocumentState];
    END_BLOCK_OBJC_EXCEPTIONS;
}

String FrameMac::incomingReferrer() const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [_bridge incomingReferrer];
    END_BLOCK_OBJC_EXCEPTIONS;

    return String();
}

void FrameMac::runJavaScriptAlert(const String& message)
{
    String text = message;
    text.replace('\\', backslashAsCurrencySymbol());
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [_bridge runJavaScriptAlertPanelWithMessage:text];
    END_BLOCK_OBJC_EXCEPTIONS;
}

bool FrameMac::runJavaScriptConfirm(const String& message)
{
    String text = message;
    text.replace('\\', backslashAsCurrencySymbol());

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [_bridge runJavaScriptConfirmPanelWithMessage:text];
    END_BLOCK_OBJC_EXCEPTIONS;

    return false;
}

bool FrameMac::runJavaScriptPrompt(const String& prompt, const String& defaultValue, String& result)
{
    String promptText = prompt;
    promptText.replace('\\', backslashAsCurrencySymbol());
    String defaultValueText = defaultValue;
    defaultValueText.replace('\\', backslashAsCurrencySymbol());

    bool ok;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    NSString *returnedText = nil;

    ok = [_bridge runJavaScriptTextInputPanelWithPrompt:prompt
        defaultText:defaultValue returningText:&returnedText];

    if (ok) {
        result = String(returnedText);
        result.replace(backslashAsCurrencySymbol(), '\\');
    }

    return ok;
    END_BLOCK_OBJC_EXCEPTIONS;
    
    return false;
}

bool FrameMac::shouldInterruptJavaScript()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [_bridge shouldInterruptJavaScript];
    END_BLOCK_OBJC_EXCEPTIONS;
    
    return false;
}

bool FrameMac::locationbarVisible()
{
    return [_bridge areToolbarsVisible];
}

bool FrameMac::menubarVisible()
{
    // The menubar is always on in Mac OS X UI
    return true;
}

bool FrameMac::personalbarVisible()
{
    return [_bridge areToolbarsVisible];
}

bool FrameMac::statusbarVisible()
{
    return [_bridge isStatusbarVisible];
}

bool FrameMac::toolbarVisible()
{
    return [_bridge areToolbarsVisible];
}

void FrameMac::createEmptyDocument()
{
    if ([_bridge isLoading])
        return;

    // Although it's not completely clear from the name of this function,
    // it does nothing if we already have a document, and just creates an
    // empty one if we have no document at all.
    if (!d->m_doc) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS;
        [_bridge loadEmptyDocumentSynchronously];
        END_BLOCK_OBJC_EXCEPTIONS;

        updateBaseURLForEmptyDocument();
    }
}

bool FrameMac::keyEvent(GSEventRef event)
{
    bool result;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    ASSERT (GSEventIsKeyEventType (event));
    // Check for cases where we are too early for events -- possible unmatched key up
    // from pressing return in the location bar.
    Document *doc = document();
    if (!doc) {
        return false;
    }
    Node *node = doc->focusNode();
    if (!node) {
        if (doc->isHTMLDocument())
            node = doc->body();
        else
            node = doc->documentElement();
        if (!node)
            return false;
    }

    if (GSEventGetType (event) == kGSEventKeyDown) {
        prepareForUserAction();
    }

    GSEventRef oldCurrentEvent = _currentEvent;
    _currentEvent = (GSEventRef)CFRetain(event);

    PlatformKeyboardEvent qEvent(event);
    result = !EventTargetNodeCast(node)->dispatchKeyEvent(qEvent);

    // We want to send both a down and a press for the initial key event.
    // To get KHTML to do this, we send a second KeyPress with "is repeat" set to true,
    // which causes it to send a press to the DOM.
    // That's not a great hack; it would be good to do this in a better way.
    if (GSEventGetType (event) == kGSEventKeyDown && !GSEventIsKeyRepeating (event)) {
        PlatformKeyboardEvent repeatEvent(event, true);
        if (!EventTargetNodeCast(node)->dispatchKeyEvent(repeatEvent))
            result = true;
    }

    ASSERT(_currentEvent == event);
    CFRelease (event);
    _currentEvent = oldCurrentEvent;

    return result;

    END_BLOCK_OBJC_EXCEPTIONS;

    return false;
}

void FrameMac::handleMousePressEvent(const MouseEventWithHitTestResults& event)
{
    bool singleClick = GSEventGetClickCount (_currentEvent) <= 1;    
    // If we got the event back, that must mean it wasn't prevented,
    // so it's allowed to start a drag or selection.
    _mouseDownMayStartSelect = canMouseDownStartSelect(event.targetNode());
    
    // Careful that the drag starting logic stays in sync with eventMayStartDrag()
    _mouseDownMayStartDrag = singleClick;

    d->m_mousePressNode = event.targetNode();
    
    if (!passWidgetMouseDownEventToWidget(event, false)) {
        // We don't do this at the start of mouse down handling (before calling into WebCore),
        // because we don't want to do it until we know we didn't hit a widget.
        NSView *view = d->m_view->getDocumentView();

        if (singleClick) {
            BEGIN_BLOCK_OBJC_EXCEPTIONS;
            if ([_bridge firstResponder] != view) {
                [_bridge makeFirstResponder:view];
            }
            END_BLOCK_OBJC_EXCEPTIONS;
        }

        Frame::handleMousePressEvent(event);
    }
}

bool FrameMac::passMouseDownEventToWidget(Widget* widget)
{
    // FIXME: this method always returns true

    if (!widget) {
        LOG_ERROR("hit a RenderWidget without a corresponding Widget, means a frame is half-constructed");
        return true;
    }

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    
    NSView *nodeView = widget->getView();
    ASSERT(nodeView);
    ASSERT([nodeView superview]);
    NSPoint p = GSEventGetLocationInWindow (_currentEvent);
    NSView *view = [nodeView hitTest:[[nodeView superview] convertPoint:p fromView:nil]];
    if (view == nil) {
        LOG_ERROR("KHTML says we hit a RenderWidget, but AppKit doesn't agree we hit the corresponding NSView");
        return true;
    }
    
    if ([_bridge firstResponder] == view) {
        // In the case where we just became first responder, we should send the mouseDown:
        // to the NSTextField, not the NSTextField's editor. This code makes sure that happens.
        // If we don't do this, we see a flash of selected text when clicking in a text field.
    } else {
        // Normally [NSWindow sendEvent:] handles setting the first responder.
        // But in our case, the event was sent to the view representing the entire web page.
        int clickCount = GSEventGetClickCount (_currentEvent);
        if (clickCount <= 1 && [view acceptsFirstResponder] && [view needsPanelToBecomeKey]) {
            [_bridge makeFirstResponder:view];
        }
    }

    // We need to "defer loading" and defer timers while we are tracking the mouse.
    // That's because we don't want the new page to load while the user is holding the mouse down.
    
    BOOL wasDeferringLoading = [_bridge defersLoading];
    if (!wasDeferringLoading)
        [_bridge setDefersLoading:YES];
    BOOL wasDeferringTimers = isDeferringTimers();
    if (!wasDeferringTimers)
        setDeferringTimers(true);

    ASSERT(!_sendingEventToSubview);
    _sendingEventToSubview = true;
    [view mouseDown:_currentEvent];
    _sendingEventToSubview = false;
    
    if (!wasDeferringTimers)
        setDeferringTimers(false);
    if (!wasDeferringLoading)
        [_bridge setDefersLoading:NO];

    // Remember which view we sent the event to, so we can direct the release event properly.
    _mouseDownView = view;
    _mouseDownWasInSubframe = false;

    END_BLOCK_OBJC_EXCEPTIONS;

    return true;
}

bool FrameMac::lastEventIsMouseUp() const
{
    // Many AK widgets run their own event loops and consume events while the mouse is down.
    // When they finish, currentEvent is the mouseUp that they exited on.  We need to update
    // the khtml state with this mouseUp, which khtml never saw.  This method lets us detect
    // that state.

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    GSEventRef currentEventAfterHandlingMouseDown = WKEventGetCurrentEvent();
    if (_currentEvent != currentEventAfterHandlingMouseDown) {
        if (GSEventGetType (currentEventAfterHandlingMouseDown) == kGSEventLeftMouseUp) {
            return true;
        }
    }
    END_BLOCK_OBJC_EXCEPTIONS;

    return false;
}
    
// Note that this does the same kind of check as [target isDescendantOf:superview].
// There are two differences: This is a lot slower because it has to walk the whole
// tree, and this works in cases where the target has already been deallocated.
static bool findViewInSubviews(NSView *superview, NSView *target)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    NSEnumerator *e = [[superview subviews] objectEnumerator];
    NSView *subview;
    while ((subview = [e nextObject])) {
        if (subview == target || findViewInSubviews(subview, target)) {
            return true;
        }
    }
    END_BLOCK_OBJC_EXCEPTIONS;
    
    return false;
}

NSView *FrameMac::mouseDownViewIfStillGood()
{
    // Since we have no way of tracking the lifetime of _mouseDownView, we have to assume that
    // it could be deallocated already. We search for it in our subview tree; if we don't find
    // it, we set it to nil.
    NSView *mouseDownView = _mouseDownView;
    if (!mouseDownView) {
        return nil;
    }
    FrameView *topFrameView = d->m_view.get();
    NSView *topView = topFrameView ? topFrameView->getView() : nil;
    if (!topView || !findViewInSubviews(topView, mouseDownView)) {
        _mouseDownView = nil;
        return nil;
    }
    return mouseDownView;
}


// The link drag hysteresis is much larger than the others because there
// needs to be enough space to cancel the link press without starting a link drag,
// and because dragging links is rare.
const float LinkDragHysteresis = 40.0;
const float ImageDragHysteresis = 5.0;
const float TextDragHysteresis = 3.0;
const float GeneralDragHysteresis = 3.0;
const float TextDragDelay = 0.15;

bool FrameMac::dragHysteresisExceeded(float dragLocationX, float dragLocationY) const
{
    IntPoint dragViewportLocation((int)dragLocationX, (int)dragLocationY);
    IntPoint dragLocation = d->m_view->viewportToContents(dragViewportLocation);
    IntSize delta = dragLocation - m_mouseDownPos;
    
    float threshold = GeneralDragHysteresis;
    if (_dragSrcIsImage)
        threshold = ImageDragHysteresis;
    else if (_dragSrcIsLink)
        threshold = LinkDragHysteresis;
    else if (_dragSrcInSelection)
        threshold = TextDragHysteresis;

    return fabsf(delta.width()) >= threshold || fabsf(delta.height()) >= threshold;
}


void FrameMac::handleMouseReleaseEvent(const MouseEventWithHitTestResults& event)
{
    NSView *view = mouseDownViewIfStillGood();
    if (!view) {
        // If this was the first click in the window, we don't even want to clear the selection.
        // This case occurs when the user clicks on a draggable element, since we have to process
        // the mouse down and drag events to see if we might start a drag.  For other first clicks
        // in a window, we just don't acceptFirstMouse, and the whole down-drag-up sequence gets
        // ignored upstream of this layer.
        if (_activationEventNumber != GSEventGetEventNumber (_currentEvent))
            Frame::handleMouseReleaseEvent(event);
        return;
    }
    stopAutoscrollTimer();
    
    _sendingEventToSubview = true;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [view mouseUp:_currentEvent];
    END_BLOCK_OBJC_EXCEPTIONS;
    _sendingEventToSubview = false;
}

bool FrameMac::passSubframeEventToSubframe(MouseEventWithHitTestResults& event, Frame* subframePart)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    switch (GSEventGetType (_currentEvent)) {
        case kGSEventMouseMoved: {
            ASSERT(subframePart);
            [Mac(subframePart)->bridge() mouseMoved:_currentEvent];
            return true;
        }
        
    	case kGSEventLeftMouseDown: {
            Node *node = event.targetNode();
            if (!node) {
                return false;
            }
            RenderObject *renderer = node->renderer();
            if (!renderer || !renderer->isWidget()) {
                return false;
            }
            Widget *widget = static_cast<RenderWidget*>(renderer)->widget();
            if (!widget || !widget->isFrameView())
                return false;
            if (!passWidgetMouseDownEventToWidget(static_cast<RenderWidget*>(renderer))) {
                return false;
            }
            _mouseDownWasInSubframe = true;
            return true;
        }
    	case kGSEventLeftMouseUp: {
            if (!_mouseDownWasInSubframe) {
                return false;
            }
            NSView *view = mouseDownViewIfStillGood();
            if (!view) {
                return false;
            }
            ASSERT(!_sendingEventToSubview);
            _sendingEventToSubview = true;
            [view mouseUp:_currentEvent];
            _sendingEventToSubview = false;
            return true;
        }
    	case kGSEventLeftMouseDragged: {
            if (!_mouseDownWasInSubframe) {
                return false;
            }
            NSView *view = mouseDownViewIfStillGood();
            if (!view) {
                return false;
            }
            ASSERT(!_sendingEventToSubview);
            _sendingEventToSubview = true;
            [view mouseDragged:_currentEvent];
            _sendingEventToSubview = false;
            return true;
        }
        default:
            return false;
    }
    END_BLOCK_OBJC_EXCEPTIONS;

    return false;
}

void FrameMac::passEventToScrollView()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    switch (GSEventGetType (_currentEvent)) {
        case kGSEventLeftMouseDown:
        case kGSEventLeftMouseDragged:
        case kGSEventLeftMouseUp:
            if (_mouseDownMayStartDrag)
                [_bridge handleScrollViewEvent:_currentEvent];
            break;            
        default:
            break;
    }
    
    END_BLOCK_OBJC_EXCEPTIONS;
}

bool FrameMac::passWheelEventToChildWidget(Node *node)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    if (GSEventGetType(_currentEvent) != kGSEventScrollWheel || _sendingEventToSubview || !node)
        return false;
    else {
        RenderObject *renderer = node->renderer();
        if (!renderer || !renderer->isWidget())
            return false;
        Widget *widget = static_cast<RenderWidget*>(renderer)->widget();
        if (!widget)
            return false;
            
        NSView *nodeView = widget->getView();
        ASSERT(nodeView);
        ASSERT([nodeView superview]);
        NSView *view = [nodeView hitTest:[[nodeView superview] convertPoint:GSEventGetLocationInWindow(_currentEvent) fromView:nil]];
        ASSERT(view);
        _sendingEventToSubview = true;
        [view scrollWheel:_currentEvent];
        _sendingEventToSubview = false;
        return true;
    }
            
    END_BLOCK_OBJC_EXCEPTIONS;
    return false;
}

void FrameMac::mouseDown(GSEventRef event)
{
    FrameView *v = d->m_view.get();
    if (!v || _sendingEventToSubview)
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    prepareForUserAction();

    _mouseDownView = nil;
    _dragSrc = 0;
    
    GSEventRef  oldCurrentEvent = _currentEvent;
    _currentEvent = (GSEventRef)CFRetain(event);
    CGPoint loc = GSEventGetLocationInWindow (event);
    m_mouseDownPos = d->m_view->viewportToContents(IntPoint(loc));
    _mouseDownTimestamp = GSEventGetTimestamp (event);
    _mouseDownMayStartDrag = false;
    _mouseDownMayStartSelect = false;

    v->handleMousePressEvent(event);
    
    ASSERT(_currentEvent == event);
    CFRelease (event);
    _currentEvent = oldCurrentEvent;

    END_BLOCK_OBJC_EXCEPTIONS;
}

void FrameMac::mouseDragged(GSEventRef event)
{
    FrameView *v = d->m_view.get();
    if (!v || _sendingEventToSubview) {
        return;
    }

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    GSEventRef oldCurrentEvent = _currentEvent;
    _currentEvent = (GSEventRef)CFRetain(event);
    // Don't use mutateEventForBestPoint here because it can hurt dragging performance.

    v->handleMouseMoveEvent(event);
    
    ASSERT(_currentEvent == event);
    CFRelease (event);
    _currentEvent = oldCurrentEvent;

    END_BLOCK_OBJC_EXCEPTIONS;
}

void FrameMac::mouseUp(GSEventRef event)
{
    FrameView *v = d->m_view.get();
    if (!v || _sendingEventToSubview)
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    GSEventRef oldCurrentEvent = _currentEvent;
    _currentEvent = (GSEventRef)CFRetain(event);

    // Our behavior here is a little different that Qt. Qt always sends
    // a mouse release event, even for a double click. To correct problems
    // in khtml's DOM click event handling we do not send a release here
    // for a double click. Instead we send that event from FrameView's
    // handleMouseDoubleClickEvent. Note also that the third click of
    // a triple click is treated as a single click, but the fourth is then
    // treated as another double click. Hence the "% 2" below.
    int clickCount = GSEventGetClickCount (event);
    if (clickCount > 0 && clickCount % 2 == 0)
        v->handleMouseDoubleClickEvent(event);
    else
        v->handleMouseReleaseEvent(event);
    
    ASSERT(_currentEvent == event);
    CFRelease (event);
    _currentEvent = oldCurrentEvent;
    
    _mouseDownView = nil;

    END_BLOCK_OBJC_EXCEPTIONS;
}

/*
 A hack for the benefit of AK's PopUpButton, which uses the Carbon menu manager, which thus
 eats all subsequent events after it is starts its modal tracking loop.  After the interaction
 is done, this routine is used to fix things up.  When a mouse down started us tracking in
 the widget, we post a fake mouse up to balance the mouse down we started with. When a 
 key down started us tracking in the widget, we post a fake key up to balance things out.
 In addition, we post a fake mouseMoved to get the cursor in sync with whatever we happen to 
 be over after the tracking is done.
 */
void FrameMac::sendFakeEventsAfterWidgetTracking(GSEventRef initiatingEvent)
{
}

void FrameMac::mouseMoved(GSEventRef event)
{
    FrameView *v = d->m_view.get();
    // Reject a mouse moved if the button is down - screws up tracking during autoscroll
    // These happen because WebKit sometimes has to fake up moved events.
    if (!v || d->m_bMousePressed || _sendingEventToSubview)
        return;
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    GSEventRef oldCurrentEvent = _currentEvent;
    _currentEvent = (GSEventRef)CFRetain(event);
    
    v->handleMouseMoveEvent(event);
    
    ASSERT(_currentEvent == event);
    CFRelease(event);
    _currentEvent = oldCurrentEvent;

    END_BLOCK_OBJC_EXCEPTIONS;
}


bool FrameMac::sendContextMenuEvent(GSEventRef event)
{
    Document* doc = d->m_doc.get();
    FrameView* v = d->m_view.get();
    if (!doc || !v)
        return false;

    bool swallowEvent;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    GSEventRef oldCurrentEvent = _currentEvent;
    _currentEvent = (GSEventRef)CFRetain(event);
    
    PlatformMouseEvent mouseEvent(event);

    IntPoint viewportPos = v->viewportToContents(mouseEvent.pos());
    MouseEventWithHitTestResults mev = doc->prepareMouseEvent(false, true, false, viewportPos, mouseEvent);

    swallowEvent = v->dispatchMouseEvent(contextmenuEvent, mev.targetNode(), true, 0, mouseEvent, true);
    if (!swallowEvent && !isPointInsideSelection(viewportPos) &&
            ([_bridge selectWordBeforeMenuEvent] || [_bridge isEditable]
                || (mev.targetNode() && mev.targetNode()->isContentEditable()))) {
        _mouseDownMayStartSelect = true; // context menu events are always allowed to perform a selection
        selectClosestWordFromMouseEvent(mouseEvent, mev.targetNode());
    }

    ASSERT(_currentEvent == event);
    CFRelease(event);
    _currentEvent = oldCurrentEvent;

    return swallowEvent;

    END_BLOCK_OBJC_EXCEPTIONS;

    return false;
}

struct ListItemInfo {
    unsigned start;
    unsigned end;
};


// FIXME: This collosal function needs to be refactored into maintainable smaller bits.

#define BULLET_CHAR 0x2022
#define SQUARE_CHAR 0x25AA
#define CIRCLE_CHAR 0x25E6



GSFontRef FrameMac::fontForSelection(bool *hasMultipleFonts) const
{
    if (hasMultipleFonts)
        *hasMultipleFonts = false;

    if (!d->m_selection.isRange()) {
        Node *nodeToRemove;
        RenderStyle *style = styleForSelectionStart(nodeToRemove); // sets nodeToRemove

        GSFontRef result = 0;
        if (style)
            result = style->font().getFont();        
        
        if (nodeToRemove) {
            ExceptionCode ec;
            nodeToRemove->remove(ec);
            ASSERT(ec == 0);
        }

        return result;
    }

    GSFontRef font = 0;

    RefPtr<Range> range = d->m_selection.toRange();
    Node *startNode = range->editingStartPosition().node();
    if (startNode != nil) {
        Node *pastEnd = range->pastEndNode();
        // In the loop below, n should eventually match pastEnd and not become nil, but we've seen at least one
        // unreproducible case where this didn't happen, so check for nil also.
        for (Node *n = startNode; n && n != pastEnd; n = n->traverseNextNode()) {
            RenderObject *renderer = n->renderer();
            if (!renderer)
                continue;
            // FIXME: Are there any node types that have renderers, but that we should be skipping?
            GSFontRef f = renderer->style()->font().getFont();
            if (!font) {
                font = f;
                if (!hasMultipleFonts)
                    break;
            } else if (font != f) {
                *hasMultipleFonts = true;
                break;
            }
        }
    }

    return font;
}

NSDictionary *FrameMac::fontAttributesForSelectionStart() const
{
    Node *nodeToRemove;
    RenderStyle *style = styleForSelectionStart(nodeToRemove);
    if (!style)
        return nil;

    NSMutableDictionary *result = [NSMutableDictionary dictionary];


    return result;
}

NSWritingDirection FrameMac::baseWritingDirectionForSelectionStart() const
{
    NSWritingDirection result = NSWritingDirectionLeftToRight;

    Position pos = VisiblePosition(d->m_selection.start(), d->m_selection.affinity()).deepEquivalent();
    Node *node = pos.node();
    if (!node || !node->renderer() || !node->renderer()->containingBlock())
        return result;
    RenderStyle *style = node->renderer()->containingBlock()->style();
    if (!style)
        return result;
        
    switch (style->direction()) {
        case LTR:
            result = NSWritingDirectionLeftToRight;
            break;
        case RTL:
            result = NSWritingDirectionRightToLeft;
            break;
    }

    return result;
}

void FrameMac::tokenizerProcessedData()
{
    if (d->m_doc)
        checkCompleted();
    [_bridge tokenizerProcessedData];
}

void FrameMac::setBridge(WebCoreFrameBridge *bridge)
{ 
    if (_bridge == bridge)
        return;

    HardRetain(bridge);
    HardRelease(_bridge);
    _bridge = bridge;
}

String FrameMac::overrideMediaType() const
{
    NSString *overrideType = [_bridge overrideMediaType];
    if (overrideType)
        return overrideType;
    return String();
}

CGColorRef FrameMac::bodyBackgroundColor() const
{
    if (document() && document()->body() && document()->body()->renderer()) {
        Color bgColor = document()->body()->renderer()->style()->backgroundColor();
        if (bgColor.isValid())
            return cgColor(bgColor);
    }
    return nil;
}

WebCoreKeyboardUIMode FrameMac::keyboardUIMode() const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [_bridge keyboardUIMode];
    END_BLOCK_OBJC_EXCEPTIONS;

    return WebCoreKeyboardAccessDefault;
}

void FrameMac::didTellBridgeAboutLoad(const String& URL)
{
    urlsBridgeKnowsAbout.add(URL.impl());
}

bool FrameMac::haveToldBridgeAboutLoad(const String& URL)
{
    return urlsBridgeKnowsAbout.contains(URL.impl());
}

void FrameMac::clear()
{
    urlsBridgeKnowsAbout.clear();
    setMarkedTextRange(0, nil, nil);
    Frame::clear();
}

void FrameMac::print()
{
    [Mac(this)->_bridge print];
}

#if BINDINGS

static KJS::Bindings::Instance *getInstanceForView(NSView *aView)
{
    if ([aView respondsToSelector:@selector(objectForWebScript)]){
        id object = [aView objectForWebScript];
        if (object) {
            KJS::Bindings::RootObject *executionContext = KJS::Bindings::RootObject::findRootObjectForNativeHandleFunction ()(aView);
            return KJS::Bindings::Instance::createBindingForLanguageInstance (KJS::Bindings::Instance::ObjectiveCLanguage, object, executionContext);
        }
    }
    else if ([aView respondsToSelector:@selector(pluginScriptableObject)]){
        void *object = [aView pluginScriptableObject];
        if (object) {
            KJS::Bindings::RootObject *executionContext = KJS::Bindings::RootObject::findRootObjectForNativeHandleFunction ()(aView);
            return KJS::Bindings::Instance::createBindingForLanguageInstance (KJS::Bindings::Instance::CLanguage, object, executionContext);
        }
    }
    return 0;
}

KJS::Bindings::Instance *FrameMac::getEmbedInstanceForWidget(Widget *widget)
{
    return getInstanceForView(widget->getView());
}

KJS::Bindings::Instance *FrameMac::getObjectInstanceForWidget(Widget *widget)
{
    return getInstanceForView(widget->getView());
}

void FrameMac::addPluginRootObject(KJS::Bindings::RootObject *root)
{
    m_rootObjects.append(root);
}

void FrameMac::cleanupPluginRootObjects()
{
    JSLock lock;

    unsigned count = m_rootObjects.size();
    for (unsigned i = 0; i < count; i++)
        m_rootObjects[i]->removeAllNativeReferences();
    m_rootObjects.clear();
}
#endif //BINDINGS

void FrameMac::registerCommandForUndoOrRedo(const EditCommandPtr &cmd, bool isRedo)
{
}

void FrameMac::registerCommandForUndo(const EditCommandPtr &cmd)
{
    registerCommandForUndoOrRedo(cmd, NO);
}

void FrameMac::registerCommandForRedo(const EditCommandPtr &cmd)
{
    registerCommandForUndoOrRedo(cmd, YES);
}

void FrameMac::clearUndoRedoOperations()
{
    if (_haveUndoRedoOperations) {
        _haveUndoRedoOperations = NO;
    }
}

void FrameMac::issueUndoCommand()
{
}

void FrameMac::issueRedoCommand()
{
}

void FrameMac::issueCutCommand()
{
    [_bridge issueCutCommand];
}

void FrameMac::issueCopyCommand()
{
    [_bridge issueCopyCommand];
}

void FrameMac::issuePasteCommand()
{
    [_bridge issuePasteCommand];
}

void FrameMac::issuePasteAndMatchStyleCommand()
{
    [_bridge issuePasteAndMatchStyleCommand];
}

void FrameMac::issueTransposeCommand()
{
    [_bridge issueTransposeCommand];
}

bool FrameMac::canUndo() const
{
    return NO;
}

bool FrameMac::canRedo() const
{
    return NO;
}

bool FrameMac::canPaste() const
{
    return [Mac(this)->_bridge canPaste];
}

void FrameMac::markMisspellingsInAdjacentWords(const VisiblePosition &p)
{
}

void FrameMac::markMisspellings(const SelectionController &selection)
{
}

void FrameMac::respondToChangedSelection(const SelectionController &oldSelection, bool closeTyping)
{
    if (document()) {
        if ([_bridge isContinuousSpellCheckingEnabled]) {
            SelectionController oldAdjacentWords = SelectionController();
            
            // If this is a change in selection resulting from a delete operation, oldSelection may no longer
            // be in the document.
            if (oldSelection.start().node() && oldSelection.start().node()->inDocument()) {
                VisiblePosition oldStart(oldSelection.start(), oldSelection.affinity());
                oldAdjacentWords = SelectionController(startOfWord(oldStart, LeftWordIfOnBoundary), endOfWord(oldStart, RightWordIfOnBoundary));   
            }

            VisiblePosition newStart(selection().start(), selection().affinity());
            SelectionController newAdjacentWords(startOfWord(newStart, LeftWordIfOnBoundary), endOfWord(newStart, RightWordIfOnBoundary));

            // When typing we check spelling elsewhere, so don't redo it here.
            if (closeTyping && oldAdjacentWords != newAdjacentWords)
                markMisspellings(oldAdjacentWords);

            // This only erases a marker in the first word of the selection.
            // Perhaps peculiar, but it matches AppKit.
            document()->removeMarkers(newAdjacentWords.toRange().get(), DocumentMarker::Spelling);
        } else
            // When continuous spell checking is off, no markers appear after the selection changes.
            document()->removeMarkers(DocumentMarker::Spelling);
    }

    [_bridge respondToChangedSelection];
}

bool FrameMac::shouldChangeSelection(const SelectionController &oldSelection, const SelectionController &newSelection, EAffinity affinity, bool stillSelecting) const
{
    return [_bridge shouldChangeSelectedDOMRange:[DOMRange _rangeWith:oldSelection.toRange().get()]
                                      toDOMRange:[DOMRange _rangeWith:newSelection.toRange().get()]
                                        affinity:static_cast<NSSelectionAffinity>(affinity)
                                  stillSelecting:stillSelecting];
}

bool FrameMac::shouldDeleteSelection(const SelectionController &selection) const
{
    return [_bridge shouldDeleteSelectedDOMRange:[DOMRange _rangeWith:selection.toRange().get()]];
}

void FrameMac::respondToChangedContents()
{
    [_bridge respondToChangedContents];
}

bool FrameMac::isContentEditable() const
{
    return Frame::isContentEditable() || [_bridge isEditable];
}

bool FrameMac::shouldBeginEditing(const Range *range) const
{
    ASSERT(range);
    return [_bridge shouldBeginEditing:[DOMRange _rangeWith:const_cast<Range*>(range)]];
}

bool FrameMac::shouldEndEditing(const Range *range) const
{
    ASSERT(range);
    return [_bridge shouldEndEditing:[DOMRange _rangeWith:const_cast<Range*>(range)]];
}

void FrameMac::didBeginEditing() const
{
    [_bridge didBeginEditing];
}

void FrameMac::didEndEditing() const
{
    [_bridge didEndEditing];
}

void FrameMac::textFieldDidBeginEditing(Element* input)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [_bridge textFieldDidBeginEditing:(DOMHTMLInputElement *)[DOMElement _elementWith:input]];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void FrameMac::textFieldDidEndEditing(Element* input)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [_bridge textFieldDidEndEditing:(DOMHTMLInputElement *)[DOMElement _elementWith:input]];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void FrameMac::textDidChangeInTextField(Element* input)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [_bridge textDidChangeInTextField:(DOMHTMLInputElement *)[DOMElement _elementWith:input]];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void FrameMac::textDidChangeInTextArea(Element* textarea)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [_bridge textDidChangeInTextArea:(DOMHTMLTextAreaElement *)[DOMElement _elementWith:textarea]];
    END_BLOCK_OBJC_EXCEPTIONS;
}

bool FrameMac::doTextFieldCommandFromEvent(Element* input, const PlatformKeyboardEvent* event)
{
    return false;
}

void FrameMac::textWillBeDeletedInTextField(Element* input)
{
    // We're using the deleteBackward selector for all deletion operations since the autofill code treats all deletions the same way.
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [_bridge textField:(DOMHTMLInputElement *)[DOMElement _elementWith:input] doCommandBySelector:@selector(deleteBackward:)];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void FrameMac::formElementDidSetValue(Element * anElement)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [_bridge formElementDidSetValue:(DOMHTMLElement *)[DOMElement _elementWith:anElement]];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void FrameMac::formElementDidFocus(Element * anElement)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [_bridge formElementDidFocus:(DOMHTMLElement *)[DOMElement _elementWith:anElement]];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void FrameMac::formElementDidBlur(Element * anElement)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [_bridge formElementDidBlur:(DOMHTMLElement *)[DOMElement _elementWith:anElement]];
    END_BLOCK_OBJC_EXCEPTIONS;
}

#define NSFloatValue(aFloat) [NSNumber numberWithFloat:aFloat]

NSDictionary * FrameMac::dictionaryForViewportArguments(ViewportArguments arguments)
{
    return [NSDictionary dictionaryWithObjects:[NSArray arrayWithObjects:NSFloatValue(arguments.initialScale), NSFloatValue(arguments.minimumScale), 
                                                NSFloatValue(arguments.maximumScale), NSFloatValue(arguments.userScalable), 
                                                NSFloatValue(arguments.width), NSFloatValue(arguments.height), nil] 
                                       forKeys:[NSArray arrayWithObjects:@"initial-scale", @"minimum-scale", @"maximum-scale", @"user-scalable", @"width", @"height", nil]];
}

ViewportArguments FrameMac::viewportArguments()
{
    return d->m_viewportArguments;
}

void FrameMac::setViewportArguments(ViewportArguments arguments)
{
    d->m_viewportArguments = arguments;
}

void FrameMac::didReceiveViewportArguments(ViewportArguments arguments)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [_bridge didReceiveViewportArguments:dictionaryForViewportArguments(arguments)];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void FrameMac::setNeedsScrollNotifications(bool aFlag)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [_bridge setNeedsScrollNotifications:[NSNumber numberWithBool:aFlag]];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void FrameMac::deferredContentChangeObserved()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [_bridge didObserveDeferredContentChange];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void FrameMac::clearObservedContentModifiers()
{
    if (WebThreadCountOfObservedContentModifiers() > 0) {
        WebThreadClearObservedContentModifiers();
        deferredContentChangeObserved();
    }        
}


void FrameMac::setMarkedTextRange(const Range *range, NSArray *attributes, NSArray *ranges)
{
    int exception = 0;

    ASSERT(!range || range->startContainer(exception) == range->endContainer(exception));
    ASSERT(!range || range->collapsed(exception) || range->startContainer(exception)->isTextNode());

    if (attributes == nil) {
        d->m_markedTextUsesUnderlines = false;
        d->m_markedTextUnderlines.clear();
    } else {
        d->m_markedTextUsesUnderlines = true;
    }

    RenderObject * previousRenderer = NULL;
    
    if (m_markedTextRange.get() && document() && m_markedTextRange->startContainer(exception)->renderer()) {
        m_markedTextRange->startContainer(exception)->renderer()->repaint();
        previousRenderer = m_markedTextRange->startContainer(exception)->renderer();
    }

    if (range && range->collapsed(exception))
        m_markedTextRange = 0;
    else
        m_markedTextRange = const_cast<Range*>(range);

    if (m_markedTextRange.get() && document() && m_markedTextRange->startContainer(exception)->renderer()) {
        if(m_markedTextRange->startContainer(exception)->renderer()->style()->textSecurity() != TSNONE) {
            m_markedTextRange->startContainer(exception)->setChanged(true);
            m_markedTextRange->startContainer(exception)->recalcStyle();
        }
        m_markedTextRange->startContainer(exception)->renderer()->repaint();
    }
    else if(!m_markedTextRange.get() && previousRenderer && previousRenderer->style()->textSecurity() != TSNONE) {
        previousRenderer->node()->setChanged(true);
        previousRenderer->node()->recalcStyle();
    }
}

bool FrameMac::canGoBackOrForward(int distance) const
{
    return [_bridge canGoBackOrForward:distance];
}

void FrameMac::didParse(double duration)
{
    [_bridge didParse:duration];
}

void FrameMac::didLayout(bool firstLayout, double duration)
{
    [_bridge didLayout:firstLayout duration:duration];
}

void FrameMac::didForcedLayout()
{
    [_bridge didForcedLayout];
}

void FrameMac::didReceiveDocType()
{
    [_bridge didReceiveDocType];
}

char *FrameMac::windowState()
{
    return [_bridge windowState];
}

NSMutableDictionary *FrameMac::dashboardRegionsDictionary()
{
    Document *doc = document();
    if (!doc)
        return nil;

    const DeprecatedValueList<DashboardRegionValue> regions = doc->dashboardRegions();
    unsigned i, count = regions.count();

    // Convert the DeprecatedValueList<DashboardRegionValue> into a NSDictionary of WebDashboardRegions
    NSMutableDictionary *webRegions = [[[NSMutableDictionary alloc] initWithCapacity:count] autorelease];
    for (i = 0; i < count; i++) {
        DashboardRegionValue region = regions[i];

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
            regionValues = [NSMutableArray array];
            [webRegions setObject:regionValues forKey:label];
        }
        
        WebDashboardRegion *webRegion = [[[WebDashboardRegion alloc] initWithRect:region.bounds clip:region.clip type:type] autorelease];
        [regionValues addObject:webRegion];
    }
    
    return webRegions;
}

void FrameMac::dashboardRegionsChanged()
{
    NSMutableDictionary *webRegions = dashboardRegionsDictionary();
    [_bridge dashboardRegionsChanged:webRegions];
}

bool FrameMac::isCharacterSmartReplaceExempt(const DeprecatedChar &c, bool isPreviousChar)
{
    return [_bridge isCharacterSmartReplaceExempt:c.unicode() isPreviousCharacter:isPreviousChar];
}

void FrameMac::handledOnloadEvents()
{
    [_bridge handledOnloadEvents];
}

bool FrameMac::shouldClose()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    if (![_bridge canRunBeforeUnloadConfirmPanel])
        return true;

    RefPtr<Document> doc = document();
    if (!doc)
        return true;
    HTMLElement* body = doc->body();
    if (!body)
        return true;

    RefPtr<BeforeUnloadEvent> event = new BeforeUnloadEvent;
    event->setTarget(doc.get());
    doc->handleWindowEvent(event.get(), false);

    if (!event->defaultPrevented() && doc)
        doc->defaultEventHandler(event.get());
    if (event->result().isNull())
        return true;

    String text = event->result();
    text.replace('\\', backslashAsCurrencySymbol());

    return [_bridge runBeforeUnloadConfirmPanelWithMessage:text];

    END_BLOCK_OBJC_EXCEPTIONS;

    return true;
}


void Frame::setNeedsReapplyStyles()
{
    [Mac(this)->bridge() setNeedsReapplyStyles];
}

FloatRect FrameMac::customHighlightLineRect(const AtomicString& type, const FloatRect& lineRect)
{
    return [bridge() customHighlightRect:type forLine:lineRect];
}

void FrameMac::paintCustomHighlight(const AtomicString& type, const FloatRect& boxRect, const FloatRect& lineRect, bool text, bool line)
{
    [bridge() paintCustomHighlight:type forBox:boxRect onLine:lineRect behindText:text entireLine:line];
}

int FrameMac::maximumImageSize()
{
    return [Mac(this)->bridge() getMaximumImageSize];
}


void FrameMac::notifySelectionLayoutChanged() const
{
    [_bridge caretChanged];
}

int FrameMac::orientation() const
{
    return [_bridge rotationDegrees];
}

bool FrameMac::canTargetLoadInFrame(Frame* targetFrame)
{
    return [_bridge canTargetLoadInFrame:Mac(targetFrame)->bridge()];
}

}

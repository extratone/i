/*
 * Copyright (C) 2006, 2007, 2008, Apple Inc. All rights reserved.
 */

#import "config.h"
#import "Frame.h"

#if PLATFORM(IOS)

#import "AnimationController.h"
#import "BlockExceptions.h"
#import "DOMCore.h"
#import "DOMCSSStyleDeclarationInternal.h"
#import "DOMInternal.h"
#import "DOMNodeInternal.h"
#import "DOMWindow.h"
#import "Document.h"
#import "DocumentMarker.h"
#import "DocumentMarkerController.h"
#import "Editor.h"
#import "EditorClient.h"
#import "EventHandler.h"
#import "EventNames.h"
#import "FormController.h"
#import "FrameSelection.h"
#import "FrameSnapshottingMac.h"
#import "FrameView.h"
#import "HTMLAreaElement.h"
#import "HTMLDocument.h"
#import "HTMLElement.h"
#import "HTMLNames.h"
#import "HTMLObjectElement.h"
#import "HitTestRequest.h"
#import "HitTestResult.h"
#import "JSDOMWindowBase.h"
#import "NodeRenderStyle.h"
#import "NodeTraversal.h"
#import "Page.h"
#import "PageTransitionEvent.h"
#import "PropertySetCSSStyleDeclaration.h"
#import "RenderLayer.h"
#import "RenderLayerCompositor.h"
#import "RenderTextControl.h"
#import "RenderView.h"
#import "TextBoundaries.h"
#import "TextIterator.h"
#import "VisiblePosition.h"
#import "WAKWindow.h"
#import "WebCoreSystemInterface.h"
#import "VisibleUnits.h"
#import <runtime/JSLock.h>

using namespace WebCore::HTMLNames;
using namespace WTF::Unicode;

using JSC::JSLockHolder;

namespace WebCore {

// Create <html><body (style="...")></body></html> doing minimal amount of work.
void Frame::initWithSimpleHTMLDocument(const String& style, const KURL& url)
{
    m_loader.initForSynthesizedDocument(url);

    RefPtr<HTMLDocument> document = HTMLDocument::createSynthesizedDocument(this, url);
    document->setCompatibilityMode(Document::LimitedQuirksMode);
    document->createDOMWindow();
    setDocument(document);

    ExceptionCode ec;
    RefPtr<Element> rootElement = document->createElementNS(xhtmlNamespaceURI, "html", ec);

    RefPtr<Element> body = document->createElementNS(xhtmlNamespaceURI, "body", ec);
    if (!style.isEmpty())
        body->setAttribute(HTMLNames::styleAttr, style);

    rootElement->appendChild(body, ec);
    document->appendChild(rootElement, ec);
}

int Frame::indexCountOfWordPrecedingSelection(NSString *word) const
{
    int result = -1;

    if (!page() || page()->selection().isNone())
        return result;

    RefPtr<Range> searchRange(rangeOfContents(document()));
    VisiblePosition start(page()->selection().start(), page()->selection().affinity());
    VisiblePosition oneBeforeStart = start.previous();

    setEnd(searchRange.get(), oneBeforeStart.isNotNull() ? oneBeforeStart : start);

    int exception = 0;
    if (searchRange->collapsed(exception)) {
        return result;
    }

    WordAwareIterator it(searchRange.get());
    while (!it.atEnd()) {
        const UChar *chars = it.characters();
        int len = it.length();
        if (len > 1 || !isSpaceOrNewline(chars[0])) {
            int word_start = 0;
            for (int i = 1; i < len; i++) {
                if (isSpaceOrNewline(chars[i]) || chars[i] == 0xA0) {
                    int word_length = i - word_start;
                    NSString *chunk = [[NSString alloc] initWithCharactersNoCopy:const_cast<unichar *>(chars) + word_start length:word_length freeWhenDone:NO];
                    if ([chunk isEqualToString:word])
                        result++;
                    [chunk release];
                    word_start += word_length + 1;
                }
            }
            if (word_start < len) {
                NSString *chunk = [[NSString alloc] initWithCharactersNoCopy:const_cast<unichar *>(chars) + word_start length:len - word_start freeWhenDone:NO];
                if ([chunk isEqualToString:word])
                    result++;
                [chunk release];
            }
        }
        it.advance();
    }

    return result + 1;
}

NSArray *Frame::wordsInCurrentParagraph() const
{
    document()->updateLayout();

    if (!page() || !page()->selection().isCaret())
        return nil;

    VisiblePosition pos(page()->selection().start(), page()->selection().affinity());
    VisiblePosition end(pos);
    if (!isStartOfParagraph(end)) {
        VisiblePosition previous = end.previous();
        UChar c(previous.characterAfter());
        if (!iswpunct(c) && !isSpaceOrNewline(c) && c != 0xA0)
            end = startOfWord(end);
    }
    VisiblePosition start(startOfParagraph(end));

    RefPtr<Range> searchRange(rangeOfContents(document()));
    setStart(searchRange.get(), start);
    setEnd(searchRange.get(), end);

    int exception = 0;
    if (searchRange->collapsed(exception))
        return nil;

    NSMutableArray *words = [NSMutableArray array];

    WordAwareIterator it(searchRange.get());
    while (!it.atEnd()) {
        const UChar *chars = it.characters();
        int len = it.length();
        if (len > 1 || !isSpaceOrNewline(chars[0])) {
            int word_start = 0;
            for (int i = 1; i < len; i++) {
                if (isSpaceOrNewline(chars[i]) || chars[i] == 0xA0) {
                    int word_length = i - word_start;
                    if (word_length > 0) {
                        NSString *chunk = [[NSString alloc] initWithCharactersNoCopy:const_cast<unichar *>(chars) + word_start length:word_length freeWhenDone:NO];
                        [words addObject:chunk];
                        [chunk release];
                    }
                    word_start += word_length + 1;
                }
            }
            if (word_start < len) {
                NSString *chunk = [[NSString alloc] initWithCharactersNoCopy:const_cast<unichar *>(chars) + word_start length:len - word_start freeWhenDone:NO];
                [words addObject:chunk];
                [chunk release];
            }
        }
        it.advance();
    }

    if ([words count] > 0 && isEndOfParagraph(pos) && !isStartOfParagraph(pos)) {
        VisiblePosition previous = pos.previous();
        UChar c(previous.characterAfter());
        if (!isSpaceOrNewline(c) && c != 0xA0)
            [words removeLastObject];
    }

    return words;
}

#define CHECK_FONT_SIZE 0
#define RECT_LOGGING 0

CGRect Frame::renderRectForPoint(CGPoint point, bool* isReplaced, float* fontSize) const
{
    *isReplaced = false;
    *fontSize = 0;

    if (!m_doc || !m_doc->renderBox())
        return CGRectZero;

    // FIXME: why this layer check?
    RenderLayer* layer = m_doc->renderBox()->layer();
    if (!layer)
        return CGRectZero;

    HitTestResult result = eventHandler()->hitTestResultAtPoint(IntPoint(roundf(point.x), roundf(point.y)));

    Node* node = result.innerNode();
    if (!node)
        return CGRectZero;

    RenderObject* hitRenderer = node->renderer();
    RenderObject* renderer = hitRenderer;
#if RECT_LOGGING
    printf("\n%f %f\n", point.x, point.y);
#endif
    while (renderer && !renderer->isBody() && !renderer->isRoot()) {
#if RECT_LOGGING
        CGRect rect = renderer->absoluteBoundingBoxRect(true);
        if (renderer->node()) {
            const char *nodeName = renderer->node()->nodeName().ascii().data();
            printf("%s %f %f %f %f\n", nodeName, rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
        }
#endif
        if (renderer->isRenderBlock() || renderer->isInlineBlockOrInlineTable() || renderer->isReplaced()) {
            *isReplaced = renderer->isReplaced();
#if CHECK_FONT_SIZE
            for (RenderObject *textRenderer = hitRenderer; textRenderer; textRenderer = textRenderer->traverseNext(hitRenderer)) {
                if (textRenderer->isText()) {
                    *fontSize = textRenderer->font(true).pixelSize();
                    break;
                }
            }
#endif
            IntRect targetRect = renderer->absoluteBoundingBoxRect(true);
            for (Widget* currView = renderer->view()->frameView(); currView && currView != view(); currView = currView->parent())
                targetRect = currView->convertToContainingView(targetRect);

            return targetRect;
        }
        renderer = renderer->parent();
    }

    return CGRectZero;
}

#define ALLOW_SCROLL_LISTENERS 0

static Node* ancestorRespondingToScrollWheelEvents(const HitTestResult& hitTestResult, Node* terminationNode, IntRect* nodeBounds)
{
    if (nodeBounds)
        *nodeBounds = IntRect();

    Node* scrollingAncestor = 0;
    for (Node* node = hitTestResult.innerNode(); node && node != terminationNode && !node->hasTagName(HTMLNames::bodyTag); node = node->parentNode()) {
#if ALLOW_SCROLL_LISTENERS
        if (node->willRespondToMouseWheelEvents()) {
            scrollingAncestor = node;
            continue;
        }
#endif

        RenderObject* renderer = node->renderer();
        if (!renderer)
            continue;

        if ((renderer->isTextField() || renderer->isTextArea()) && static_cast<RenderTextControl *>(renderer)->canScroll()) {
            scrollingAncestor = node;
            continue;
        }

        RenderStyle* style = renderer->style();
        if (!style)
            continue;

        if (style && renderer->hasOverflowClip() &&
            (style->overflowY() == OAUTO || style->overflowY() == OSCROLL || style->overflowY() == OOVERLAY ||
             style->overflowX() == OAUTO || style->overflowX() == OSCROLL || style->overflowX() == OOVERLAY))
            scrollingAncestor = node;
    }

    return scrollingAncestor;
}

static Node* ancestorRespondingToClickEvents(const HitTestResult& hitTestResult, Node* terminationNode, IntRect* nodeBounds)
{
    bool bodyHasBeenReached = false;
    bool pointerCursorStillValid = true;

    if (nodeBounds)
        *nodeBounds = IntRect();

    Node* pointerCursorNode = 0;
    for (Node* node = hitTestResult.innerNode(); node && node != terminationNode; node = node->parentNode()) {
        ASSERT(!node->isInShadowTree());

        // We only accept pointer nodes before reaching the body tag.
        if (node->hasTagName(HTMLNames::bodyTag)) {
#if USE(UIKIT_EDITING)
            // Make sure we cover the case of an empty editable body.
            if (!pointerCursorNode && node->isContentEditable())
                pointerCursorNode = node;
#endif
            bodyHasBeenReached = true;
            pointerCursorStillValid = false;
        }

        // If we already have a pointer, and we reach a table, don't accept it.
        if (pointerCursorNode && (node->hasTagName(HTMLNames::tableTag) || node->hasTagName(HTMLNames::tbodyTag)))
            pointerCursorStillValid = false;

        // If we haven't reached the body, and we are still paying attention to pointer cursors, and the node has a pointer cursor...
        if (pointerCursorStillValid && node->renderStyle() && node->renderStyle()->cursor() == CURSOR_POINTER)
            pointerCursorNode = node;
        // We want the lowest unbroken chain of pointer cursors.
        else if (pointerCursorNode)
            pointerCursorStillValid = false;

        if (node->willRespondToMouseClickEvents() || node->willRespondToMouseMoveEvents()) {
            // If we're at the body or higher, use the pointer cursor node (which may be null).
            if (bodyHasBeenReached)
                node = pointerCursorNode;

            // If we are interested about the frame, use it.
            if (nodeBounds) {
                // This is a check to see whether this node is an area element.  The only way this can happen is if this is the first check.
                if (node == hitTestResult.innerNode() && node != hitTestResult.innerNonSharedNode() && node->hasTagName(HTMLNames::areaTag))
                    *nodeBounds = pixelSnappedIntRect(static_cast<HTMLAreaElement *>(node)->computeRect(hitTestResult.innerNonSharedNode()->renderer()));
                else if (node && node->renderer())
                    *nodeBounds = node->renderer()->absoluteBoundingBoxRect(true);
            }

            return node;
        }
    }

    return 0;
}

void Frame::betterApproximateNode(const IntPoint& testPoint, NodeQualifier nodeQualifierFunction, Node*& best, Node* failedNode, IntPoint& bestPoint, IntRect& bestRect, const IntRect& testRect)
{
    IntRect candidateRect;
    Node* candidate = nodeQualifierFunction(eventHandler()->hitTestResultAtPoint(testPoint), failedNode, &candidateRect);

    // Bail if we have no candidate, or the candidate is already equal to our current best node,
    // or our candidate is the avoidedNode and there is a current best node.
    if (!candidate || candidate == best)
        return;

    // The document should never be considered the best alternative.
    if (candidate->isDocumentNode())
        return;

    if (best) {
        IntRect bestIntersect = intersection(testRect, bestRect);
        IntRect candidateIntersect = intersection(testRect, candidateRect);
        // if the candidate intersection is smaller than the current best intersection, bail.
        if (candidateIntersect.width() * candidateIntersect.height() <= bestIntersect.width() * bestIntersect.height())
            return;
    }

    // At this point we either don't have a previous best, or our current candidate has a better intersection.
    best = candidate;
    bestPoint = testPoint;
    bestRect = candidateRect;
}

bool Frame::hitTestResultAtViewportLocation(const FloatPoint& viewportLocation, HitTestResult& hitTestResult, IntPoint& center)
{
    if (!m_doc || !m_doc->renderer())
        return false;

    FrameView *view = m_view.get();
    if (!view)
        return false;

    center = view->windowToContents(roundedIntPoint(viewportLocation));
    hitTestResult = eventHandler()->hitTestResultAtPoint(center);
    return true;
}

Node* Frame::qualifyingNodeAtViewportLocation(const FloatPoint& viewportLocation, FloatPoint& adjustedViewportLocation, NodeQualifier nodeQualifierFunction, bool shouldApproximate)
{
    IntPoint testCenter;
    HitTestResult candidateInfo;
    if (!hitTestResultAtViewportLocation(viewportLocation, candidateInfo, testCenter))
        return 0;

    IntPoint bestPoint = testCenter;

    // We have the candidate node at the location, check whether it or one of its ancestors passes
    // the qualifier function, which typically checks if the node responds to a particular event type.
    Node* approximateNode = nodeQualifierFunction(candidateInfo, 0, 0);

#if USE(UIKIT_EDITING)
    if (approximateNode && approximateNode->isContentEditable()) {
        // If we are in editable content, we look for the root editable element.
        approximateNode = approximateNode->rootEditableElement();
        // If we have a focusable node, there is no need to approximate.
        if (approximateNode)
            shouldApproximate = false;
    }
#endif
    if (approximateNode && shouldApproximate) {
        float scale = m_documentScale;

        const int defaultMaxRadius = 15;
        int maxRadius = (scale < 1.0) ? (int)(defaultMaxRadius / scale) : defaultMaxRadius;

        const float testOffsets[] = {
            -.3f, -.3f,
            -.6f, -.6f,
            +.3f, +.3f,
            -.9f, -.9f,
        };

        Node* originalApproximateNode = approximateNode;
        for (unsigned n = 0; n < sizeof(testOffsets) / sizeof(testOffsets[0]); n += 2) {
            IntSize testOffset(testOffsets[n] * maxRadius, testOffsets[n + 1] * maxRadius);
            IntPoint testPoint = testCenter + testOffset;

            HitTestResult candidateInfo = eventHandler()->hitTestResultAtPoint(testPoint);
            Node* candidateNode = nodeQualifierFunction(candidateInfo, 0, 0);
            if (candidateNode && candidateNode->isDescendantOf(originalApproximateNode)) {
                approximateNode = candidateNode;
                bestPoint = testPoint;
                break;
            }
        }
    } else if (!approximateNode && shouldApproximate) {
        // Grab the closest parent element of our failed candidate node.
        Node* candidate = candidateInfo.innerNode();
        Node* failedNode = candidate;

        while (candidate && !candidate->isElementNode())
            candidate = candidate->parentNode();

        if (candidate)
            failedNode = candidate;

        // We don't approximate the node if we are dragging, we instead force the user to be precise.
        float scale = m_documentScale;

        const int defaultMaxRadius = 15;
        int maxRadius = (scale < 1.0) ? (int)(defaultMaxRadius / scale) : defaultMaxRadius;

        // The center point was tested earlier.
        const float testOffsets[] = {
            -.3f, -.3f,
            +.3f, -.3f,
            -.3f, +.3f,
            +.3f, +.3f,
            -.6f, -.6f,
            +.6f, -.6f,
            -.6f, +.6f,
            +.6f, +.6f,
            -1.f, 0,
            +1.f, 0,
            0, +1.f,
            0, -1.f,
        };
        IntRect bestFrame;
        IntRect testRect(testCenter, IntSize());
        testRect.inflate(maxRadius);
        int currentTestRadius = 0;
        for (unsigned n = 0; n < sizeof(testOffsets) / sizeof(testOffsets[0]); n += 2) {
            IntSize testOffset(testOffsets[n] * maxRadius, testOffsets[n + 1] * maxRadius);
            IntPoint testPoint = testCenter + testOffset;
            int testRadius = max(abs(testOffset.width()), abs(testOffset.height()));
            if (testRadius > currentTestRadius) {
                // Bail out with the best match within a radius
                currentTestRadius = testRadius;
                if (approximateNode)
                    break;
            }
            betterApproximateNode(testPoint, nodeQualifierFunction, approximateNode, failedNode, bestPoint, bestFrame, testRect);
        }
    }

    if (approximateNode) {
        IntPoint p = m_view->contentsToWindow(bestPoint);
        adjustedViewportLocation = p;
#if USE(UIKIT_EDITING)
        if (approximateNode->isContentEditable()) {
            // When in editable content, look for the root editable node again,
            // since this could be the node found with the approximation.
            approximateNode = approximateNode->rootEditableElement();
        }
#endif
    }

    return approximateNode;
}

Node* Frame::deepestNodeAtLocation(const FloatPoint& viewportLocation)
{
    IntPoint center;
    HitTestResult hitTestResult;
    if (!hitTestResultAtViewportLocation(viewportLocation, hitTestResult, center))
        return 0;

    return hitTestResult.innerNode();
}

Node* Frame::nodeRespondingToClickEvents(const FloatPoint& viewportLocation, FloatPoint& adjustedViewportLocation)
{
    return qualifyingNodeAtViewportLocation(viewportLocation, adjustedViewportLocation, &ancestorRespondingToClickEvents, true);
}

Node* Frame::nodeRespondingToScrollWheelEvents(const FloatPoint& viewportLocation)
{
    FloatPoint adjustedViewportLocation;
    return qualifyingNodeAtViewportLocation(viewportLocation, adjustedViewportLocation, &ancestorRespondingToScrollWheelEvents, false);
}

int Frame::preferredHeight() const
{
    Document *doc = document();
    if (!doc)
        return 0;

    doc->updateLayout();

    Node *body = doc->body();
    if (!body)
        return 0;

    RenderObject *renderer = body->renderer();
    if (!renderer || !renderer->isRenderBlock())
        return 0;

    RenderBlock *block = static_cast<RenderBlock *>(renderer);
    return block->height() + block->marginTop() + block->marginBottom();
}

int Frame::innerLineHeight(DOMNode *node) const
{
    if (!node)
        return 0;

    Document *doc = document();
    if (!doc)
        return 0;

    doc->updateLayout();

    Node *n = core(node);
    if (!n)
        return 0;

    RenderObject *renderer = n->renderer();
    if (!renderer)
        return 0;

    return renderer->innerLineHeight();
}

void Frame::updateLayout() const
{
    Document *doc = document();
    if (!doc)
        return;
    doc->updateLayout();

    if (view())
        view()->adjustViewSize();
}

NSRect Frame::caretRect() const
{
    if (selection()->isNone())
        return CGRectZero;
    return selection()->isCaret() ? selection()->absoluteCaretBounds() : VisiblePosition(selection()->end()).absoluteCaretBounds();
}

NSRect Frame::rectForScrollToVisible() const
{
    VisibleSelection selection(this->selection()->selection());
    return rectForSelection(selection);
}

NSRect Frame::rectForSelection(VisibleSelection& selection) const
{
    if (selection.isNone())
        return CGRectZero;

    if (selection.isCaret())
        return caretRect();

    if (editor().client())
        editor().client()->suppressSelectionNotifications();

    VisibleSelection originalSelection(selection);
    Position pos;

    // The selection controllers below need to be associated with
    // a frame in order to calculate geometry. This causes them
    // to do more work here than we would like.
    // Ideally, we would have a sort offline geometry-only mode
    // for selection controllers so we could do this kind of
    // work as cheaply as possible.

    pos = originalSelection.start();
    selection.setBase(pos);
    selection.setExtent(pos);
    FrameSelection startFrameSelection(const_cast<Frame*>(this));
    startFrameSelection.suppressCloseTyping();
    startFrameSelection.setSelection(selection);
    FloatRect startRect(startFrameSelection.absoluteCaretBounds());
    startFrameSelection.restoreCloseTyping();

    pos = originalSelection.end();
    selection.setBase(pos);
    selection.setExtent(pos);
    FrameSelection endFrameSelection(const_cast<Frame*>(this));
    endFrameSelection.suppressCloseTyping();
    endFrameSelection.setSelection(selection);
    FloatRect endRect(endFrameSelection.absoluteCaretBounds());
    endFrameSelection.restoreCloseTyping();

    if (editor().client())
        editor().client()->restoreSelectionNotifications();

    return unionRect(startRect, endRect);
}

DOMCSSStyleDeclaration *Frame::styleAtSelectionStart() const
{
    Position start = selection()->start();
    RefPtr<EditingStyle> editingStyle = EditingStyle::styleAtSelectionStart(selection()->selection());
    if (!editingStyle)
        return 0;
    PropertySetCSSStyleDeclaration *propertySetCSSStyleDeclaration = new PropertySetCSSStyleDeclaration(editingStyle->style());
    // The auto-generated code for DOMCSSStyleDeclaration derefs its pointer when it is deallocated.
    return kit((CSSStyleDeclaration *)propertySetCSSStyleDeclaration);
}
    
void Frame::createDefaultFieldEditorDocumentStructure() const
{
    Document* doc = document();

    ASSERT(doc);
    ASSERT(doc->body());

// The system font is different on iPhone 4 (i.e. devices where the screen scale is 2.0).
// <rdar://problem/7933949> New system fonts for iPhone 4 (helvetica neue 55 and 75)
#define STYLE_ATTR "margin:0px;word-wrap:break-word;-webkit-nbsp-mode:space;-webkit-line-break:after-white-space;white-space:nowrap;font-family:"
    RefPtr<Element> body = doc->body();
    body->setAttribute(styleAttr, wkGetScreenScaleFactor() == 2.0 ? STYLE_ATTR"'.Helvetica NeueUI'" : STYLE_ATTR"'Helvetica'");
#undef STYLE_ATTR

    ExceptionCode ec = 0;
    RefPtr<Element> sizeElement = doc->createElementNS(xhtmlNamespaceURI, "div", ec);
    ASSERT(!ec);
    sizeElement->setAttribute(idAttr, "size");
    sizeElement->setAttribute(contenteditableAttr, "false");

    RefPtr<Element> textElement = doc->createElementNS(xhtmlNamespaceURI, "div", ec);
    ASSERT(!ec);
    textElement->setAttribute(idAttr, "text");
    textElement->setAttribute(contenteditableAttr, "true");

    sizeElement->appendChild(textElement, ec);
    ASSERT(!ec);
    body->appendChild(sizeElement, ec);
    ASSERT(!ec);
}

static inline bool inSameEditableContent(const VisiblePosition &a, const VisiblePosition &b)
{
    Position ap = a.deepEquivalent();
    Node *an = ap.deprecatedNode();
    if (!an)
        return false;

    Position bp = b.deepEquivalent();
    Node *bn = bp.deprecatedNode();
    if (!bn)
        return false;

    if (!an->isContentEditable() || !bn->isContentEditable())
        return false;

    return an->rootEditableElement() == bn->rootEditableElement();
}

static inline bool isStartOfEditableContent(const VisiblePosition &p)
{
    return !inSameEditableContent(p, p.previous());
}

static inline bool isEndOfEditableContent(const VisiblePosition &p)
{
    return !inSameEditableContent(p, p.next());
}

unsigned Frame::formElementsCharacterCount() const
{
    Document* doc = document();
    if (!doc)
        return 0;
    return doc->formController().formElementsCharacterCount();
}

void Frame::setTimersPaused(bool paused)
{
    JSLockHolder lock(JSDOMWindowBase::commonVM());
    setTimersPausedInternal(paused);
}

void Frame::setTimersPausedInternal(bool paused)
{
    if (paused) {
        m_timersPausedCount++;
        if (m_timersPausedCount == 1) {
            clearTimers();
            if (document())
                document()->suspendScheduledTasks(ActiveDOMObject::DocumentWillBePaused);
        }
    } else {
        m_timersPausedCount--;
        ASSERT(m_timersPausedCount >= 0);
        if (m_timersPausedCount == 0) {
            if (document())
                document()->resumeScheduledTasks(ActiveDOMObject::DocumentWillBePaused);

            // clearTimers() suspended animations and pending relayouts, reschedule if needed.
            if (animation())
                animation()->resumeAnimationsForDocument(document());

            if (view())
                view()->scheduleRelayout();
        }
    }

    // We need to make sure all subframes' states are up to date.
    for (Frame *frame = tree()->firstChild(); frame; frame = frame->tree()->nextSibling())
        frame->setTimersPausedInternal(paused);
}

void Frame::dispatchPageHideEventBeforePause()
{
    ASSERT(this == page()->mainFrame());
    if (this != page()->mainFrame())
        return;

    for (Frame *frame = this; frame; frame = frame->tree()->traverseNext(this))
        frame->document()->domWindow()->dispatchEvent(PageTransitionEvent::create(eventNames().pagehideEvent, true), document());
}

void Frame::dispatchPageShowEventBeforeResume()
{
    ASSERT(this == page()->mainFrame());
    if (this != page()->mainFrame())
        return;

    for (Frame *frame = this; frame; frame = frame->tree()->traverseNext(this))
        frame->document()->domWindow()->dispatchEvent(PageTransitionEvent::create(eventNames().pageshowEvent, true), document());
}

void Frame::setRangedSelectionBaseToCurrentSelection()
{
    m_rangedSelectionBase = selection()->selection();
}

void Frame::setRangedSelectionBaseToCurrentSelectionStart()
{
    FrameSelection* sel = selection();
    m_rangedSelectionBase = VisibleSelection(sel->selection().start(), sel->affinity());
}

void Frame::setRangedSelectionBaseToCurrentSelectionEnd()
{
    FrameSelection* sel = selection();
    m_rangedSelectionBase = VisibleSelection(sel->selection().end(), sel->affinity());
}

VisibleSelection Frame::rangedSelectionBase() const
{
    return m_rangedSelectionBase;
}

void Frame::clearRangedSelectionInitialExtent()
{
    m_rangedSelectionInitialExtent = VisibleSelection();
}

void Frame::setRangedSelectionInitialExtentToCurrentSelectionStart()
{
    FrameSelection* sel = selection();
    m_rangedSelectionInitialExtent = VisibleSelection(sel->selection().start(), sel->affinity());
}

void Frame::setRangedSelectionInitialExtentToCurrentSelectionEnd()
{
    FrameSelection* sel = selection();
    m_rangedSelectionInitialExtent = VisibleSelection(sel->selection().end(), sel->affinity());
}

VisibleSelection Frame::rangedSelectionInitialExtent() const
{
    return m_rangedSelectionInitialExtent;
}

void Frame::recursiveSetUpdateAppearanceEnabled(bool enabled)
{
    selection()->setUpdateAppearanceEnabled(enabled);
    for (Frame* child = tree()->firstChild(); child; child = child->tree()->nextSibling())
        child->recursiveSetUpdateAppearanceEnabled(enabled);
}

// FIXME: Break this function up into pieces with descriptive function names so that it's easier to follow.
NSArray* Frame::interpretationsForCurrentRoot() const
{
    if (!document())
        return nil;
    
    Element *root = selection()->selectionType() == VisibleSelection::NoSelection ? document()->body() : selection()->rootEditableElement();
    unsigned rootChildCount = root->childNodeCount();
    RefPtr<Range> rangeOfRootContents = Range::create(document(), createLegacyEditingPosition(root, 0), createLegacyEditingPosition(root, rootChildCount));
    
    Vector<DocumentMarker*> markersInRoot = document()->markers()->markersInRange(rangeOfRootContents.get(), DocumentMarker::DictationPhraseWithAlternatives);
    
    // There are no phrases with alternatives, so there is just one interpretation.
    if (markersInRoot.size() == 0)
        return [NSArray arrayWithObject:plainText(rangeOfRootContents.get())];
    
    // The number of interpretations will be i1 * i2 * ... * iN, where iX is the number of interpretations for the Xth phrase with alternatives.
    size_t interpretationsCount = 1;
    
    Vector<DocumentMarker*>::const_iterator end = markersInRoot.end();
    for (Vector<DocumentMarker*>::const_iterator it = markersInRoot.begin(); it != end; ++it)
        interpretationsCount *= (*it)->alternatives().size() + 1;
    
    Vector<Vector<UChar> > interpretations;
    interpretations.grow(interpretationsCount);
    
    Position precedingTextStartPosition = createLegacyEditingPosition(root, 0);
    
    unsigned combinationsSoFar = 1;
    
    Node* pastLastNode = rangeOfRootContents->pastLastNode();
    for (Node* node = rangeOfRootContents->firstNode(); node != pastLastNode; node = NodeTraversal::next(node)) {
        Vector<DocumentMarker> markers = document()->markers()->markersForNode(node);
        Vector<DocumentMarker>::const_iterator end = markers.end();
        for (Vector<DocumentMarker>::const_iterator it = markers.begin(); it != end; ++it) {
            
            const DocumentMarker& marker = *it;
            
            if (marker.type() != DocumentMarker::DictationPhraseWithAlternatives)
                continue;
            
            // First, add text that precede the marker.
            if (precedingTextStartPosition != createLegacyEditingPosition(node, marker.startOffset())) {
                RefPtr<Range> precedingTextRange = Range::create(document(), precedingTextStartPosition, createLegacyEditingPosition(node, marker.startOffset()));
                String precedingText = plainText(precedingTextRange.get());
                if (precedingText.length()) {
                    for (size_t i = 0; i < interpretationsCount; i++)
                        interpretations.at(i).append(precedingText.characters(), precedingText.length());
                }
            }
            
            RefPtr<Range> rangeForMarker = Range::create(document(), createLegacyEditingPosition(node, marker.startOffset()), createLegacyEditingPosition(node, marker.endOffset()));
            String visibleTextForMarker = plainText(rangeForMarker.get());
            size_t interpretationsCountForCurrentMarker = marker.alternatives().size() + 1;
            
            for (size_t i = 0; i < interpretationsCount; i++) {
                
                // Determine text for the ith interpretation. It will either be the visible text, or one of its 
                // alternatives stored in the marker.
                
                size_t indexOfInterpretationForCurrentMarker = (i / combinationsSoFar) % interpretationsCountForCurrentMarker;
                if (indexOfInterpretationForCurrentMarker == 0)
                    interpretations.at(i).append(visibleTextForMarker.characters(), visibleTextForMarker.length());
                else {
                    const String& alternative = marker.alternatives().at(i % marker.alternatives().size());
                    interpretations.at(i).append(alternative.characters(), alternative.length());
                }
            }
            
            combinationsSoFar *= interpretationsCountForCurrentMarker;
            
            precedingTextStartPosition = createLegacyEditingPosition(node, marker.endOffset());
        }
    }
    
    // Finally, add any text after the last marker.
    RefPtr<Range> afterLastMarkerRange = Range::create(document(), precedingTextStartPosition, createLegacyEditingPosition(root, rootChildCount));
    String textAfterLastMarker = plainText(afterLastMarkerRange.get());
    if (textAfterLastMarker.length()) {
        for (size_t i = 0; i < interpretationsCount; i++)
            interpretations.at(i).append(textAfterLastMarker.characters(), textAfterLastMarker.length());
    }
    
    NSMutableArray *result = [NSMutableArray array];
    for (size_t i = 0; i < interpretationsCount; i++)
        [result addObject:(NSString *)String(interpretations.at(i))];
    
    return result;
}

void Frame::setDocumentScale(float newScale)
{
    if (newScale == m_documentScale)
        return;

    m_documentScale = newScale;
    for (Frame* child = tree()->firstChild(); child; child = child->tree()->nextSibling())
        child->setDocumentScale(newScale);

#if USE(ACCELERATED_COMPOSITING)
    RenderView* root = contentRenderer();
    if (root && root->compositor())
        root->compositor()->deviceOrPageScaleFactorChanged();
#endif
}

float Frame::documentScale() const
{
    return m_documentScale;
}

float Frame::minimumDocumentScale() const
{
    WAKWindow *window = [m_view.get()->documentView() window];
    return window ? [window zoomedOutTileScale] : 1;
}

float Frame::deviceScaleFactor() const
{
    WAKWindow *window = [m_view.get()->documentView() window];
    return window ? [window screenScale] : 1;
}

static bool anyFrameHasTiledLayers(Frame* rootFrame)
{
    for (Frame* frame = rootFrame; frame; frame = frame->tree()->traverseNext(rootFrame)) {
        if (frame->containsTiledBackingLayers())
            return true;
    }
    return false;
}

void Frame::viewportOffsetChanged(ViewportOffsetChangeType changeType)
{
#if USE(ACCELERATED_COMPOSITING)
    if (changeType == IncrementalScrollOffset) {
        if (anyFrameHasTiledLayers(this)) {
            if (RenderView* root = contentRenderer()) {
                if (root->compositor())
                    root->compositor()->didChangeVisibleRect();
            }
        }
    }

    if (changeType == CompletedScrollOffset) {
        if (RenderView* root = contentRenderer()) {
            if (root->compositor())
                root->compositor()->updateCompositingLayers(CompositingUpdateOnScroll);
        }
    }
#endif
}

bool Frame::containsTiledBackingLayers() const
{
#if USE(ACCELERATED_COMPOSITING)
    if (RenderView* root = contentRenderer()) {
        if (root->compositor())
            return root->compositor()->hasNonMainLayersWithTiledBacking();
    }
#endif
    return false;
}

void Frame::overflowScrollPositionChangedForNode(const IntPoint& position, Node* node, bool isUserScroll)
{
    RenderObject* renderer = node->renderer();
    if (!renderer || !renderer->hasLayer())
        return;
    
    RenderLayer* layer = toRenderBoxModelObject(renderer)->layer();
    
    layer->setIsUserScroll(isUserScroll);
    layer->scrollToOffsetWithoutAnimation(position);
    layer->setIsUserScroll(false);
    layer->didEndScroll(); // FIXME: Should we always call this?
}

void Frame::resetAllGeolocationPermission()
{
    if (document()->domWindow())
        document()->domWindow()->resetAllGeolocationPermission();

    for (Frame* child = tree()->firstChild(); child; child = child->tree()->nextSibling())
        child->resetAllGeolocationPermission();
}

} // namespace WebCore
#endif // PLATFORM(IOS)

#if ENABLE(IOS_TEXT_AUTOSIZING)
namespace WebCore {

float Frame::textAutosizingWidth() const
{
    return m_textAutosizingWidth;
}

void Frame::setTextAutosizingWidth(float width)
{
    m_textAutosizingWidth = width;
}

} // namespace WebCore
#endif // ENABLE(IOS_TEXT_AUTOSIZING)

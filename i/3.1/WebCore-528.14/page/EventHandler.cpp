/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
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

#include "config.h"
#include "EventHandler.h"

#include "AXObjectCache.h"
#include "CachedImage.h"
#include "ChromeClient.h"
#include "Cursor.h"
#include "Document.h"
#include "DragController.h"
#include "Editor.h"
#include "EventNames.h"
#include "FloatPoint.h"
#include "FloatRect.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "HTMLFrameSetElement.h"
#include "HTMLFrameElementBase.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "Image.h"
#include "InspectorController.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "MouseEventWithHitTestResults.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformWheelEvent.h"
#include "RenderFrameSet.h"
#include "RenderWidget.h"
#include "RenderView.h"
#include "Scrollbar.h"
#include "SelectionController.h"
#include "Settings.h"
#include "TextEvent.h"
#include <wtf/StdLibExtras.h>

#if ENABLE(TOUCH_EVENTS)
#include "Touch.h"
#endif

#if ENABLE(SVG)
#include "SVGDocument.h"
#include "SVGElementInstance.h"
#include "SVGNames.h"
#include "SVGUseElement.h"
#endif

#include <wtf/UnusedParam.h>

namespace WebCore {

using namespace HTMLNames;

// The link drag hysteresis is much larger than the others because there
// needs to be enough space to cancel the link press without starting a link drag,
// and because dragging links is rare.
const int LinkDragHysteresis = 40;
const int ImageDragHysteresis = 5;
const int TextDragHysteresis = 3;
const int GeneralDragHysteresis = 3;

#if ENABLE(TOUCH_EVENTS)
const float GestureUnknown = 0.0f;
#endif

// Match key code of composition keydown event on windows.
// IE sends VK_PROCESSKEY which has value 229;
const int CompositionEventKeyCode = 229;

#if ENABLE(SVG)
using namespace SVGNames;
#endif

// When the autoscroll or the panScroll is triggered when do the scroll every 0.05s to make it smooth
const double autoscrollInterval = 0.05;

static Frame* subframeForTargetNode(Node*);
static Frame* subframeForHitTestResult(const MouseEventWithHitTestResults&);

static inline void scrollAndAcceptEvent(float delta, ScrollDirection positiveDirection, ScrollDirection negativeDirection, PlatformWheelEvent& e, Node* node)
{
    if (!delta)
        return;
        
    // Find the nearest enclosing box.
    RenderBox* enclosingBox = node->renderer()->enclosingBox();

    if (e.granularity() == ScrollByPageWheelEvent) {
        if (enclosingBox->scroll(delta < 0 ? negativeDirection : positiveDirection, ScrollByPage, 1))
            e.accept();
        return;
    } 
    float pixelsToScroll = delta > 0 ? delta : -delta;
    if (e.granularity() == ScrollByLineWheelEvent)
        pixelsToScroll *= cMouseWheelPixelsPerLineStep;
    if (enclosingBox->scroll(delta < 0 ? negativeDirection : positiveDirection, ScrollByPixel, pixelsToScroll))
        e.accept();
}

EventHandler::EventHandler(Frame* frame)
    : m_frame(frame)
    , m_mousePressed(false)
    , m_mouseDownMayStartSelect(false)
    , m_mouseDownMayStartDrag(false)
    , m_mouseDownWasSingleClickInSelection(false)
    , m_beganSelectingText(false)
    , m_panScrollInProgress(false)
    , m_hoverTimer(this, &EventHandler::hoverTimerFired)
    , m_autoscrollTimer(this, &EventHandler::autoscrollTimerFired)
    , m_autoscrollRenderer(0)
    , m_autoscrollInProgress(false)
    , m_mouseDownMayStartAutoscroll(false)
    , m_mouseDownWasInSubframe(false)
#if ENABLE(SVG)
    , m_svgPan(false)
#endif
    , m_resizeLayer(0)
    , m_capturingMouseEventsNode(0)
    , m_clickCount(0)
#if ENABLE(TOUCH_EVENTS)
    , m_gestureInitialDiameter(GestureUnknown)
    , m_gestureLastDiameter(GestureUnknown)
    , m_gestureInitialRotation(GestureUnknown)
    , m_gestureLastRotation(GestureUnknown)
#endif
    , m_mouseDownTimestamp(0)
    , m_pendingFrameUnloadEventCount(0)
    , m_pendingFrameBeforeUnloadEventCount(0)
#if PLATFORM(MAC)
    , m_mouseDownView(nil)
    , m_sendingEventToSubview(false)
#endif
{
}

EventHandler::~EventHandler()
{
}
    
EventHandler::EventHandlerDragState& EventHandler::dragState()
{
    DEFINE_STATIC_LOCAL(EventHandlerDragState, state, ());
    return state;
}
    
void EventHandler::clear()
{
    m_hoverTimer.stop();
    m_resizeLayer = 0;
    m_nodeUnderMouse = 0;
    m_lastNodeUnderMouse = 0;
#if ENABLE(SVG)
    m_instanceUnderMouse = 0;
    m_lastInstanceUnderMouse = 0;
#endif
    m_lastMouseMoveEventSubframe = 0;
    m_lastScrollbarUnderMouse = 0;
    m_clickCount = 0;
    m_clickNode = 0;
#if ENABLE(TOUCH_EVENTS)
    m_gestureInitialDiameter = GestureUnknown;
    m_gestureLastDiameter = GestureUnknown;
    m_gestureInitialRotation = GestureUnknown;
    m_gestureLastRotation = GestureUnknown;
    m_touchesByID.clear();
    m_gestureTargets.clear();
#endif    
    m_frameSetBeingResized = 0;
    m_dragTarget = 0;
    m_currentMousePosition = IntPoint();
    m_mousePressNode = 0;
    m_mousePressed = false;
    m_capturingMouseEventsNode = 0;
}

void EventHandler::selectClosestWordFromMouseEvent(const MouseEventWithHitTestResults& result)
{
    Node* innerNode = result.targetNode();
    Selection newSelection;

    if (innerNode && innerNode->renderer() && m_mouseDownMayStartSelect) {
        VisiblePosition pos(innerNode->renderer()->positionForPoint(result.localPoint()));
        if (pos.isNotNull()) {
            newSelection = Selection(pos);
            newSelection.expandUsingGranularity(WordGranularity);
        }
    
        if (newSelection.isRange()) {
            m_frame->setSelectionGranularity(WordGranularity);
            m_beganSelectingText = true;
            if (result.event().clickCount() == 2 && m_frame->editor()->isSelectTrailingWhitespaceEnabled()) 
                newSelection.appendTrailingWhitespace();            
        }
        
        if (m_frame->shouldChangeSelection(newSelection))
            m_frame->selection()->setSelection(newSelection);
    }
}

void EventHandler::selectClosestWordOrLinkFromMouseEvent(const MouseEventWithHitTestResults& result)
{
    if (!result.hitTestResult().isLiveLink())
        return selectClosestWordFromMouseEvent(result);

    Node* innerNode = result.targetNode();

    if (innerNode && innerNode->renderer() && m_mouseDownMayStartSelect) {
        Selection newSelection;
        Element* URLElement = result.hitTestResult().URLElement();
        VisiblePosition pos(innerNode->renderer()->positionForPoint(result.localPoint()));
        if (pos.isNotNull() && pos.deepEquivalent().node()->isDescendantOf(URLElement))
            newSelection = Selection::selectionFromContentsOfNode(URLElement);
    
        if (newSelection.isRange()) {
            m_frame->setSelectionGranularity(WordGranularity);
            m_beganSelectingText = true;
        }

        if (m_frame->shouldChangeSelection(newSelection))
            m_frame->selection()->setSelection(newSelection);
    }
}

bool EventHandler::handleMousePressEventDoubleClick(const MouseEventWithHitTestResults& event)
{
    if (event.event().button() != LeftButton)
        return false;

    if (m_frame->selection()->isRange())
        // A double-click when range is already selected
        // should not change the selection.  So, do not call
        // selectClosestWordFromMouseEvent, but do set
        // m_beganSelectingText to prevent handleMouseReleaseEvent
        // from setting caret selection.
        m_beganSelectingText = true;
    else
        selectClosestWordFromMouseEvent(event);

    return true;
}

bool EventHandler::handleMousePressEventTripleClick(const MouseEventWithHitTestResults& event)
{
    if (event.event().button() != LeftButton)
        return false;
    
    Node* innerNode = event.targetNode();
    if (!(innerNode && innerNode->renderer() && m_mouseDownMayStartSelect))
        return false;

    Selection newSelection;
    VisiblePosition pos(innerNode->renderer()->positionForPoint(event.localPoint()));
    if (pos.isNotNull()) {
        newSelection = Selection(pos);
        newSelection.expandUsingGranularity(ParagraphGranularity);
    }
    if (newSelection.isRange()) {
        m_frame->setSelectionGranularity(ParagraphGranularity);
        m_beganSelectingText = true;
    }
    
    if (m_frame->shouldChangeSelection(newSelection))
        m_frame->selection()->setSelection(newSelection);

    return true;
}

bool EventHandler::handleMousePressEventSingleClick(const MouseEventWithHitTestResults& event)
{
    if (event.event().button() != LeftButton)
        return false;
    
    Node* innerNode = event.targetNode();
    if (!(innerNode && innerNode->renderer() && m_mouseDownMayStartSelect))
        return false;

    // Extend the selection if the Shift key is down, unless the click is in a link.
    bool extendSelection = event.event().shiftKey() && !event.isOverLink();

    // Don't restart the selection when the mouse is pressed on an
    // existing selection so we can allow for text dragging.
    IntPoint vPoint = m_frame->view()->windowToContents(event.event().pos());
    if (!extendSelection && m_frame->selection()->contains(vPoint)) {
        m_mouseDownWasSingleClickInSelection = true;
        return false;
    }

    VisiblePosition visiblePos(innerNode->renderer()->positionForPoint(event.localPoint()));
    if (visiblePos.isNull())
        visiblePos = VisiblePosition(innerNode, 0, DOWNSTREAM);
    Position pos = visiblePos.deepEquivalent();
    
    Selection newSelection = m_frame->selection()->selection();
    if (extendSelection && newSelection.isCaretOrRange()) {
        m_frame->selection()->setLastChangeWasHorizontalExtension(false);
        
        // See <rdar://problem/3668157> REGRESSION (Mail): shift-click deselects when selection 
        // was created right-to-left
        Position start = newSelection.start();
        Position end = newSelection.end();
        short before = Range::compareBoundaryPoints(pos.node(), pos.offset(), start.node(), start.offset());
        if (before <= 0)
            newSelection = Selection(pos, end);
        else
            newSelection = Selection(start, pos);

        if (m_frame->selectionGranularity() != CharacterGranularity)
            newSelection.expandUsingGranularity(m_frame->selectionGranularity());
        m_beganSelectingText = true;
    } else {
        newSelection = Selection(visiblePos);
        m_frame->setSelectionGranularity(CharacterGranularity);
    }
    
    if (m_frame->shouldChangeSelection(newSelection))
        m_frame->selection()->setSelection(newSelection);

    return true;
}

bool EventHandler::handleMousePressEvent(const MouseEventWithHitTestResults& event)
{
    // Reset drag state.
    dragState().m_dragSrc = 0;

    bool singleClick = event.event().clickCount() <= 1;

    // If we got the event back, that must mean it wasn't prevented,
    // so it's allowed to start a drag or selection.
    m_mouseDownMayStartSelect = canMouseDownStartSelect(event.targetNode());
    
    // Careful that the drag starting logic stays in sync with eventMayStartDrag()
    m_mouseDownMayStartDrag = singleClick;

    m_mouseDownWasSingleClickInSelection = false;

    if (event.isOverWidget() && passWidgetMouseDownEventToWidget(event))
        return true;

#if ENABLE(SVG)
    if (m_frame->document()->isSVGDocument() &&
       static_cast<SVGDocument*>(m_frame->document())->zoomAndPanEnabled()) {
        if (event.event().shiftKey() && singleClick) {
            m_svgPan = true;
            static_cast<SVGDocument*>(m_frame->document())->startPan(event.event().pos());
            return true;
        }
    }
#endif

    // We don't do this at the start of mouse down handling,
    // because we don't want to do it until we know we didn't hit a widget.
    if (singleClick)
        focusDocumentView();

    Node* innerNode = event.targetNode();

    m_mousePressNode = innerNode;
    m_dragStartPos = event.event().pos();

    bool swallowEvent = false;
    if (event.event().button() == LeftButton || event.event().button() == MiddleButton) {
        m_frame->selection()->setCaretBlinkingSuspended(true);
        m_mousePressed = true;
        m_beganSelectingText = false;

        if (event.event().clickCount() == 2)
            swallowEvent = handleMousePressEventDoubleClick(event);
        else if (event.event().clickCount() >= 3)
            swallowEvent = handleMousePressEventTripleClick(event);
        else
            swallowEvent = handleMousePressEventSingleClick(event);
    }
    
    m_mouseDownMayStartAutoscroll = m_mouseDownMayStartSelect || 
        (m_mousePressNode && m_mousePressNode->renderBox() && m_mousePressNode->renderBox()->canBeProgramaticallyScrolled(true));

   return swallowEvent;
}

bool EventHandler::handleMouseDraggedEvent(const MouseEventWithHitTestResults& event)
{
    UNUSED_PARAM(event);
    return false;
}
    

void EventHandler::updateSelectionForMouseDrag()
{
    FrameView* view = m_frame->view();
    if (!view)
        return;
    RenderView* renderer = m_frame->contentRenderer();
    if (!renderer)
        return;
    RenderLayer* layer = renderer->layer();
    if (!layer)
        return;

    HitTestResult result(view->windowToContents(m_currentMousePosition));
    layer->hitTest(HitTestRequest(true, true, true), result);
    updateSelectionForMouseDrag(result.innerNode(), result.localPoint());
}

void EventHandler::updateSelectionForMouseDrag(Node* targetNode, const IntPoint& localPoint)
{
    if (!m_mouseDownMayStartSelect)
        return;

    if (!targetNode)
        return;

    RenderObject* targetRenderer = targetNode->renderer();
    if (!targetRenderer)
        return;
        
    if (!canMouseDragExtendSelect(targetNode))
        return;

    VisiblePosition targetPosition(targetRenderer->positionForPoint(localPoint));

    // Don't modify the selection if we're not on a node.
    if (targetPosition.isNull())
        return;

    // Restart the selection if this is the first mouse move. This work is usually
    // done in handleMousePressEvent, but not if the mouse press was on an existing selection.
    Selection newSelection = m_frame->selection()->selection();

#if ENABLE(SVG)
    // Special case to limit selection to the containing block for SVG text.
    // FIXME: Isn't there a better non-SVG-specific way to do this?
    if (Node* selectionBaseNode = newSelection.base().node())
        if (RenderObject* selectionBaseRenderer = selectionBaseNode->renderer())
            if (selectionBaseRenderer->isSVGText())
                if (targetNode->renderer()->containingBlock() != selectionBaseRenderer->containingBlock())
                    return;
#endif

    if (!m_beganSelectingText) {
        m_beganSelectingText = true;
        newSelection = Selection(targetPosition);
    }

    newSelection.setExtent(targetPosition);
    if (m_frame->selectionGranularity() != CharacterGranularity)
        newSelection.expandUsingGranularity(m_frame->selectionGranularity());

    if (m_frame->shouldChangeSelection(newSelection)) {
        m_frame->selection()->setLastChangeWasHorizontalExtension(false);
        m_frame->selection()->setSelection(newSelection);
    }
}
    
bool EventHandler::handleMouseUp(const MouseEventWithHitTestResults& event)
{
    if (eventLoopHandleMouseUp(event))
        return true;
    
    // If this was the first click in the window, we don't even want to clear the selection.
    // This case occurs when the user clicks on a draggable element, since we have to process
    // the mouse down and drag events to see if we might start a drag.  For other first clicks
    // in a window, we just don't acceptFirstMouse, and the whole down-drag-up sequence gets
    // ignored upstream of this layer.
    return eventActivatedView(event.event());
}    

bool EventHandler::handleMouseReleaseEvent(const MouseEventWithHitTestResults& event)
{
    if (m_autoscrollInProgress)
        stopAutoscrollTimer();

    if (handleMouseUp(event))
        return true;

    // Used to prevent mouseMoveEvent from initiating a drag before
    // the mouse is pressed again.
    m_frame->selection()->setCaretBlinkingSuspended(false);
    m_mousePressed = false;
    m_mouseDownMayStartDrag = false;
    m_mouseDownMayStartSelect = false;
    m_mouseDownMayStartAutoscroll = false;
    m_mouseDownWasInSubframe = false;
  
    bool handled = false;

    // Clear the selection if the mouse didn't move after the last mouse press.
    // We do this so when clicking on the selection, the selection goes away.
    // However, if we are editing, place the caret.
    if (m_mouseDownWasSingleClickInSelection && !m_beganSelectingText
            && m_dragStartPos == event.event().pos()
            && m_frame->selection()->isRange()) {
        Selection newSelection;
        Node *node = event.targetNode();
        if (node && node->isContentEditable() && node->renderer()) {
            VisiblePosition pos = node->renderer()->positionForPoint(event.localPoint());
            newSelection = Selection(pos);
        }
        if (m_frame->shouldChangeSelection(newSelection))
            m_frame->selection()->setSelection(newSelection);

        handled = true;
    }

    m_frame->notifyRendererOfSelectionChange(true);

    m_frame->selection()->selectFrameElementInParentIfFullySelected();

    return handled;
}

void EventHandler::handleAutoscroll(RenderObject* renderer)
{
    // We don't want to trigger the autoscroll or the panScroll if it's already active
    if (m_autoscrollTimer.isActive())
        return;     

    setAutoscrollRenderer(renderer);

#if ENABLE(PAN_SCROLLING)
    if (m_panScrollInProgress) {
        m_panScrollStartPos = currentMousePosition();
        m_frame->view()->addPanScrollIcon(m_panScrollStartPos);
        // If we're not in the top frame we notify it that we are using the panScroll
        if (m_frame != m_frame->page()->mainFrame())
            m_frame->page()->mainFrame()->eventHandler()->setPanScrollInProgress(true);
    }
#endif

    startAutoscrollTimer();
}

void EventHandler::autoscrollTimerFired(Timer<EventHandler>*)
{
    RenderObject* r = autoscrollRenderer();
    if (!r || !r->isBox()) {
        stopAutoscrollTimer();
        return;
    }

    if (m_autoscrollInProgress) {
        if (!m_mousePressed) {
            stopAutoscrollTimer();
            return;
        }
        toRenderBox(r)->autoscroll();
    } else {
        // we verify that the main frame hasn't received the order to stop the panScroll
        if (!m_frame->page()->mainFrame()->eventHandler()->panScrollInProgress()) {
            stopAutoscrollTimer();
            return;
        }
#if ENABLE(PAN_SCROLLING)
        setPanScrollCursor();
        toRenderBox(r)->panScroll(m_panScrollStartPos);
#endif
    }
}

void EventHandler::setPanScrollCursor()
{
}

RenderObject* EventHandler::autoscrollRenderer() const
{
    return m_autoscrollRenderer;
}

void EventHandler::updateAutoscrollRenderer()
{
    if (!m_autoscrollRenderer)
        return;

    HitTestResult hitTest = hitTestResultAtPoint(m_panScrollStartPos, true);

    if (Node* nodeAtPoint = hitTest.innerNode())
        m_autoscrollRenderer = nodeAtPoint->renderer();

    while (m_autoscrollRenderer && (!m_autoscrollRenderer->isBox() || !toRenderBox(m_autoscrollRenderer)->canBeProgramaticallyScrolled(false)))
        m_autoscrollRenderer = m_autoscrollRenderer->parent();
}

void EventHandler::setAutoscrollRenderer(RenderObject* renderer)
{
    m_autoscrollRenderer = renderer;
}

void EventHandler::allowDHTMLDrag(bool& flagDHTML, bool& flagUA) const
{
    flagDHTML = false;
    flagUA = false;
}
    
HitTestResult EventHandler::hitTestResultAtPoint(const IntPoint& point, bool allowShadowContent)
{
    HitTestResult result(point);
    if (!m_frame->contentRenderer())
        return result;
    m_frame->contentRenderer()->layer()->hitTest(HitTestRequest(true, true), result);

    while (true) {
        Node* n = result.innerNode();
        if (!result.isOverWidget() || !n || !n->renderer() || !n->renderer()->isWidget())
            break;
        RenderWidget* renderWidget = static_cast<RenderWidget*>(n->renderer());
        Widget* widget = renderWidget->widget();
        if (!widget || !widget->isFrameView())
            break;
        Frame* frame = static_cast<HTMLFrameElementBase*>(n)->contentFrame();
        if (!frame || !frame->contentRenderer())
            break;
        FrameView* view = static_cast<FrameView*>(widget);
        IntPoint widgetPoint(result.localPoint().x() + view->scrollX() - renderWidget->borderLeft() - renderWidget->paddingLeft(), 
            result.localPoint().y() + view->scrollY() - renderWidget->borderTop() - renderWidget->paddingTop());
        HitTestResult widgetHitTestResult(widgetPoint);
        frame->contentRenderer()->layer()->hitTest(HitTestRequest(true, true), widgetHitTestResult);
        result = widgetHitTestResult;
    }
    
    // If our HitTestResult is not visible, then we started hit testing too far down the frame chain. 
    // Another hit test at the main frame level should get us the correct visible result.
    Frame* resultFrame = result.innerNonSharedNode() ? result.innerNonSharedNode()->document()->frame() : 0;
    Frame* mainFrame = m_frame->page()->mainFrame();
    if (m_frame != mainFrame && resultFrame && resultFrame != mainFrame && !resultFrame->editor()->insideVisibleArea(result.point())) {
        IntPoint windowPoint = resultFrame->view()->contentsToWindow(result.point());
        IntPoint mainFramePoint = mainFrame->view()->windowToContents(windowPoint);
        result = mainFrame->eventHandler()->hitTestResultAtPoint(mainFramePoint, allowShadowContent);
    }

    if (!allowShadowContent)
        result.setToNonShadowAncestor();

    return result;
}


void EventHandler::startAutoscrollTimer()
{
    m_autoscrollTimer.startRepeating(autoscrollInterval);
}

void EventHandler::stopAutoscrollTimer(bool rendererIsBeingDestroyed)
{
    if (m_autoscrollInProgress) {
        if (m_mouseDownWasInSubframe) {
            if (Frame* subframe = subframeForTargetNode(m_mousePressNode.get()))
                subframe->eventHandler()->stopAutoscrollTimer(rendererIsBeingDestroyed);
            return;
        }
    }

    if (autoscrollRenderer()) {
        if (!rendererIsBeingDestroyed && (m_autoscrollInProgress || m_panScrollInProgress))
            toRenderBox(autoscrollRenderer())->stopAutoscroll();
#if ENABLE(PAN_SCROLLING)
        if (m_panScrollInProgress) {
            m_frame->view()->removePanScrollIcon();
            m_frame->view()->setCursor(pointerCursor());
        }
#endif

        setAutoscrollRenderer(0);
    }

    m_autoscrollTimer.stop();

    m_panScrollInProgress = false;
    // If we're not in the top frame we notify it that we are not using the panScroll anymore
    if (m_frame->page() && m_frame != m_frame->page()->mainFrame())
            m_frame->page()->mainFrame()->eventHandler()->setPanScrollInProgress(false);
    m_autoscrollInProgress = false;
}

Node* EventHandler::mousePressNode() const
{
    return m_mousePressNode.get();
}

void EventHandler::setMousePressNode(PassRefPtr<Node> node)
{
    m_mousePressNode = node;
}

bool EventHandler::scrollOverflow(ScrollDirection direction, ScrollGranularity granularity)
{
    if (!m_frame->document())
        return false;
    
    Node* node = m_frame->document()->focusedNode();
    if (!node)
        node = m_mousePressNode.get();
    
    if (node) {
        RenderObject* r = node->renderer();
        if (r && !r->isListBox())
            return r->enclosingBox()->scroll(direction, granularity);
    }

    return false;
}

IntPoint EventHandler::currentMousePosition() const
{
    return m_currentMousePosition;
}

Frame* subframeForHitTestResult(const MouseEventWithHitTestResults& hitTestResult)
{
    if (!hitTestResult.isOverWidget())
        return 0;
    return subframeForTargetNode(hitTestResult.targetNode());
}

Frame* subframeForTargetNode(Node* node)
{
    if (!node)
        return 0;

    RenderObject* renderer = node->renderer();
    if (!renderer || !renderer->isWidget())
        return 0;

    Widget* widget = static_cast<RenderWidget*>(renderer)->widget();
    if (!widget || !widget->isFrameView())
        return 0;

    return static_cast<FrameView*>(widget)->frame();
}

    
static IntPoint documentPointForWindowPoint(Frame* frame, const IntPoint& windowPoint)
{
    FrameView* view = frame->view();
    // FIXME: Is it really OK to use the wrong coordinates here when view is 0?
    // Historically the code would just crash; this is clearly no worse than that.
    return view ? view->windowToContents(windowPoint) : windowPoint;
}

bool EventHandler::handleMousePressEvent(const PlatformMouseEvent& mouseEvent)
{
    if (!m_frame->document())
        return false;

    RefPtr<FrameView> protector(m_frame->view());

    m_mousePressed = true;
    m_currentMousePosition = mouseEvent.pos();
    m_mouseDownTimestamp = mouseEvent.timestamp();
    m_mouseDownMayStartDrag = false;
    m_mouseDownMayStartSelect = false;
    m_mouseDownMayStartAutoscroll = false;
    m_mouseDownPos = m_frame->view()->windowToContents(mouseEvent.pos());
    m_mouseDownWasInSubframe = false;

    // Save the document point we generate in case the window coordinate is invalidated by what happens 
    // when we dispatch the event.
    IntPoint documentPoint = documentPointForWindowPoint(m_frame, mouseEvent.pos());
    MouseEventWithHitTestResults mev = m_frame->document()->prepareMouseEvent(HitTestRequest(false, true), documentPoint, mouseEvent);

    if (!mev.targetNode()) {
        invalidateClick();
        return false;
    }

    m_mousePressNode = mev.targetNode();


    Frame* subframe = subframeForHitTestResult(mev);
    if (subframe && passMousePressEventToSubframe(mev, subframe)) {
        // Start capturing future events for this frame.  We only do this if we didn't clear
        // the m_mousePressed flag, which may happen if an AppKit widget entered a modal event loop.
        if (m_mousePressed)
            m_capturingMouseEventsNode = mev.targetNode();
        invalidateClick();
        return true;
    }

#if ENABLE(PAN_SCROLLING)
    if (m_frame->page()->mainFrame()->eventHandler()->panScrollInProgress() || m_autoscrollInProgress) {
        stopAutoscrollTimer();
        invalidateClick();
        return true;
    }
    
    if (mouseEvent.button() == MiddleButton && !mev.isOverLink()) {
        RenderObject* renderer = mev.targetNode()->renderer();

        while (renderer && (!renderer->isBox() || !toRenderBox(renderer)->canBeProgramaticallyScrolled(false))) {
            if (!renderer->parent() && renderer->node() == renderer->document() && renderer->document()->ownerElement())
                renderer = renderer->document()->ownerElement()->renderer();
            else
                renderer = renderer->parent();
        }

        if (renderer) {
            m_panScrollInProgress = true;
            handleAutoscroll(renderer);
            invalidateClick();
            return true;
        }
   }
#endif

    m_clickCount = mouseEvent.clickCount();
    m_clickNode = mev.targetNode();

    if (!m_clickNode) {
        invalidateClick();
        return false;
    }
    
    RenderLayer* layer = m_clickNode->renderer() ? m_clickNode->renderer()->enclosingLayer() : 0;
    IntPoint p = m_frame->view()->windowToContents(mouseEvent.pos());
    if (layer && layer->isPointInResizeControl(p)) {
        layer->setInResizeMode(true);
        m_resizeLayer = layer;
        m_offsetFromResizeCorner = layer->offsetFromResizeCorner(p);
        invalidateClick();
        return true;
    }

    bool swallowEvent = dispatchMouseEvent(eventNames().mousedownEvent, mev.targetNode(), true, m_clickCount, mouseEvent, true);

    // If the hit testing originally determined the event was in a scrollbar, refetch the MouseEventWithHitTestResults
    // in case the scrollbar widget was destroyed when the mouse event was handled.
    if (mev.scrollbar()) {
        const bool wasLastScrollBar = mev.scrollbar() == m_lastScrollbarUnderMouse.get();
        mev = m_frame->document()->prepareMouseEvent(HitTestRequest(true, true), documentPoint, mouseEvent);

        if (wasLastScrollBar && mev.scrollbar() != m_lastScrollbarUnderMouse.get())
            m_lastScrollbarUnderMouse = 0;
    }

    if (swallowEvent) {
        // scrollbars should get events anyway, even disabled controls might be scrollable
        if (mev.scrollbar())
            passMousePressEventToScrollbar(mev, mev.scrollbar());
    } else {
        // Refetch the event target node if it currently is the shadow node inside an <input> element.
        // If a mouse event handler changes the input element type to one that has a widget associated,
        // we'd like to EventHandler::handleMousePressEvent to pass the event to the widget and thus the
        // event target node can't still be the shadow node.
        if (mev.targetNode()->isShadowNode() && mev.targetNode()->shadowParentNode()->hasTagName(inputTag))
            mev = m_frame->document()->prepareMouseEvent(HitTestRequest(true, true), documentPoint, mouseEvent);

        Scrollbar* scrollbar = m_frame->view()->scrollbarUnderMouse(mouseEvent);
        if (!scrollbar)
            scrollbar = mev.scrollbar();
        if (scrollbar && passMousePressEventToScrollbar(mev, scrollbar))
            swallowEvent = true;
        else
            swallowEvent = handleMousePressEvent(mev);
    }

    return swallowEvent;
}

// This method only exists for platforms that don't know how to deliver 
bool EventHandler::handleMouseDoubleClickEvent(const PlatformMouseEvent& mouseEvent)
{
    if (!m_frame->document())
        return false;

    RefPtr<FrameView> protector(m_frame->view());

    // We get this instead of a second mouse-up 
    m_mousePressed = false;
    m_currentMousePosition = mouseEvent.pos();

    MouseEventWithHitTestResults mev = prepareMouseEvent(HitTestRequest(false, true), mouseEvent);
    Frame* subframe = subframeForHitTestResult(mev);
    if (subframe && passMousePressEventToSubframe(mev, subframe)) {
        m_capturingMouseEventsNode = 0;
        return true;
    }

    m_clickCount = mouseEvent.clickCount();
    bool swallowMouseUpEvent = dispatchMouseEvent(eventNames().mouseupEvent, mev.targetNode(), true, m_clickCount, mouseEvent, false);

    bool swallowClickEvent = false;
    // Don't ever dispatch click events for right clicks
    if (mouseEvent.button() != RightButton && mev.targetNode() == m_clickNode)
        swallowClickEvent = dispatchMouseEvent(eventNames().clickEvent, mev.targetNode(), true, m_clickCount, mouseEvent, true);

    if (m_lastScrollbarUnderMouse)
        swallowMouseUpEvent = m_lastScrollbarUnderMouse->mouseUp();
            
    bool swallowMouseReleaseEvent = false;
    if (!swallowMouseUpEvent)
        swallowMouseReleaseEvent = handleMouseReleaseEvent(mev);

    invalidateClick();

    return swallowMouseUpEvent || swallowClickEvent || swallowMouseReleaseEvent;
}

bool EventHandler::mouseMoved(const PlatformMouseEvent& event)
{
    HitTestResult hoveredNode = HitTestResult(IntPoint());
    bool result = handleMouseMoveEvent(event, &hoveredNode);

    Page* page = m_frame->page();
    if (!page)
        return result;

    hoveredNode.setToNonShadowAncestor();
    page->chrome()->mouseDidMoveOverElement(hoveredNode, event.modifierFlags());
    page->chrome()->setToolTip(hoveredNode);
    return result;
}

bool EventHandler::handleMouseMoveEvent(const PlatformMouseEvent& mouseEvent, HitTestResult* hoveredNode)
{
    // in Radar 3703768 we saw frequent crashes apparently due to the
    // part being null here, which seems impossible, so check for nil
    // but also assert so that we can try to figure this out in debug
    // builds, if it happens.
    ASSERT(m_frame);
    if (!m_frame || !m_frame->document())
        return false;

    RefPtr<FrameView> protector(m_frame->view());
    m_currentMousePosition = mouseEvent.pos();

    if (m_hoverTimer.isActive())
        m_hoverTimer.stop();

#if ENABLE(SVG)
    if (m_svgPan) {
        static_cast<SVGDocument*>(m_frame->document())->updatePan(m_currentMousePosition);
        return true;
    }
#endif

    if (m_frameSetBeingResized)
        return dispatchMouseEvent(eventNames().mousemoveEvent, m_frameSetBeingResized.get(), false, 0, mouseEvent, false);

// No scrollbars on iPhone

    // Treat mouse move events while the mouse is pressed as "read-only" in prepareMouseEvent
    // if we are allowed to select.
    // This means that :hover and :active freeze in the state they were in when the mouse
    // was pressed, rather than updating for nodes the mouse moves over as you hold the mouse down.
    HitTestRequest request(m_mousePressed && m_mouseDownMayStartSelect, m_mousePressed, true);
    MouseEventWithHitTestResults mev = prepareMouseEvent(request, mouseEvent);
    if (hoveredNode)
        *hoveredNode = mev.hitTestResult();

    Scrollbar* scrollbar = 0;

    if (m_resizeLayer && m_resizeLayer->inResizeMode())
        m_resizeLayer->resize(mouseEvent, m_offsetFromResizeCorner);
    else {
        if (m_frame->view())
            scrollbar = m_frame->view()->scrollbarUnderMouse(mouseEvent);

        if (!scrollbar)
            scrollbar = mev.scrollbar();

        if (m_lastScrollbarUnderMouse != scrollbar) {
            // Send mouse exited to the old scrollbar.
            if (m_lastScrollbarUnderMouse)
                m_lastScrollbarUnderMouse->mouseExited();
            m_lastScrollbarUnderMouse = m_mousePressed ? 0 : scrollbar;
        }
    }

    bool swallowEvent = false;
    RefPtr<Frame> newSubframe = m_capturingMouseEventsNode.get() ? subframeForTargetNode(m_capturingMouseEventsNode.get()) : subframeForHitTestResult(mev);
 
    // We want mouseouts to happen first, from the inside out.  First send a move event to the last subframe so that it will fire mouseouts.
    if (m_lastMouseMoveEventSubframe && m_lastMouseMoveEventSubframe->tree()->isDescendantOf(m_frame) && m_lastMouseMoveEventSubframe != newSubframe)
        passMouseMoveEventToSubframe(mev, m_lastMouseMoveEventSubframe.get());

    if (newSubframe) {
        // Update over/out state before passing the event to the subframe.
        updateMouseEventTargetNode(mev.targetNode(), mouseEvent, true);
        
        // Event dispatch in updateMouseEventTargetNode may have caused the subframe of the target
        // node to be detached from its FrameView, in which case the event should not be passed.
        if (newSubframe->view())
            swallowEvent |= passMouseMoveEventToSubframe(mev, newSubframe.get(), hoveredNode);
    }
    
    m_lastMouseMoveEventSubframe = newSubframe;

    if (swallowEvent)
        return true;
    
    swallowEvent = dispatchMouseEvent(eventNames().mousemoveEvent, mev.targetNode(), false, 0, mouseEvent, true);
    if (!swallowEvent)
        swallowEvent = handleMouseDraggedEvent(mev);

    return swallowEvent;
}

void EventHandler::invalidateClick()
{
    m_clickCount = 0;
    m_clickNode = 0;
}

bool EventHandler::handleMouseReleaseEvent(const PlatformMouseEvent& mouseEvent)
{
    if (!m_frame->document())
        return false;

    RefPtr<FrameView> protector(m_frame->view());

    m_mousePressed = false;
    m_currentMousePosition = mouseEvent.pos();

#if ENABLE(SVG)
    if (m_svgPan) {
        m_svgPan = false;
        static_cast<SVGDocument*>(m_frame->document())->updatePan(m_currentMousePosition);
        return true;
    }
#endif

    if (m_frameSetBeingResized)
        return dispatchMouseEvent(eventNames().mouseupEvent, m_frameSetBeingResized.get(), true, m_clickCount, mouseEvent, false);

    if (m_lastScrollbarUnderMouse) {
        invalidateClick();
        return m_lastScrollbarUnderMouse->mouseUp();
    }

    MouseEventWithHitTestResults mev = prepareMouseEvent(HitTestRequest(false, false, false, true), mouseEvent);
    Frame* subframe = m_capturingMouseEventsNode.get() ? subframeForTargetNode(m_capturingMouseEventsNode.get()) : subframeForHitTestResult(mev);
    if (subframe && passMouseReleaseEventToSubframe(mev, subframe)) {
        m_capturingMouseEventsNode = 0;
        return true;
    }
    m_capturingMouseEventsNode = 0;

    bool swallowMouseUpEvent = dispatchMouseEvent(eventNames().mouseupEvent, mev.targetNode(), true, m_clickCount, mouseEvent, false);

    // Don't ever dispatch click events for right clicks
    bool swallowClickEvent = false;
    if (m_clickCount > 0 && mouseEvent.button() != RightButton && mev.targetNode() == m_clickNode)
        swallowClickEvent = dispatchMouseEvent(eventNames().clickEvent, mev.targetNode(), true, m_clickCount, mouseEvent, true);

    if (m_resizeLayer) {
        m_resizeLayer->setInResizeMode(false);
        m_resizeLayer = 0;
    }

    bool swallowMouseReleaseEvent = false;
    if (!swallowMouseUpEvent)
        swallowMouseReleaseEvent = handleMouseReleaseEvent(mev);

    invalidateClick();

    return swallowMouseUpEvent || swallowClickEvent || swallowMouseReleaseEvent;
}

bool EventHandler::dispatchDragEvent(const AtomicString& eventType, Node* dragTarget, const PlatformMouseEvent& event, Clipboard* clipboard)
{
    m_frame->view()->resetDeferredRepaintDelay();

    IntPoint contentsPos = m_frame->view()->windowToContents(event.pos());
    
    RefPtr<MouseEvent> me = MouseEvent::create(eventType,
        true, true, m_frame->document()->defaultView(),
        0, event.globalX(), event.globalY(), contentsPos.x(), contentsPos.y(),
        event.ctrlKey(), event.altKey(), event.shiftKey(), event.metaKey(),
        0, 0, clipboard);

    ExceptionCode ec = 0;
    EventTargetNodeCast(dragTarget)->dispatchEvent(me.get(), ec);
    return me->defaultPrevented();
}

bool EventHandler::updateDragAndDrop(const PlatformMouseEvent& event, Clipboard* clipboard)
{
    bool accept = false;

    if (!m_frame->document())
        return false;

    if (!m_frame->view())
        return false;
    
    MouseEventWithHitTestResults mev = prepareMouseEvent(HitTestRequest(true, false), event);

    // Drag events should never go to text nodes (following IE, and proper mouseover/out dispatch)
    Node* newTarget = mev.targetNode();
    if (newTarget && newTarget->isTextNode())
        newTarget = newTarget->parentNode();
    if (newTarget)
        newTarget = newTarget->shadowAncestorNode();

    if (m_dragTarget != newTarget) {
        // FIXME: this ordering was explicitly chosen to match WinIE. However,
        // it is sometimes incorrect when dragging within subframes, as seen with
        // LayoutTests/fast/events/drag-in-frames.html.
        if (newTarget)
            if (newTarget->hasTagName(frameTag) || newTarget->hasTagName(iframeTag))
                accept = static_cast<HTMLFrameElementBase*>(newTarget)->contentFrame()->eventHandler()->updateDragAndDrop(event, clipboard);
            else
                accept = dispatchDragEvent(eventNames().dragenterEvent, newTarget, event, clipboard);
        
        if (m_dragTarget) {
            Frame* frame = (m_dragTarget->hasTagName(frameTag) || m_dragTarget->hasTagName(iframeTag)) 
                            ? static_cast<HTMLFrameElementBase*>(m_dragTarget.get())->contentFrame() : 0;
            if (frame)
                accept = frame->eventHandler()->updateDragAndDrop(event, clipboard);
            else
                dispatchDragEvent(eventNames().dragleaveEvent, m_dragTarget.get(), event, clipboard);
        }
    } else {
        if (newTarget)
            if (newTarget->hasTagName(frameTag) || newTarget->hasTagName(iframeTag))
                accept = static_cast<HTMLFrameElementBase*>(newTarget)->contentFrame()->eventHandler()->updateDragAndDrop(event, clipboard);
            else
                accept = dispatchDragEvent(eventNames().dragoverEvent, newTarget, event, clipboard);
    }
    m_dragTarget = newTarget;

    return accept;
}

void EventHandler::cancelDragAndDrop(const PlatformMouseEvent& event, Clipboard* clipboard)
{
    if (m_dragTarget) {
        Frame* frame = (m_dragTarget->hasTagName(frameTag) || m_dragTarget->hasTagName(iframeTag)) 
                        ? static_cast<HTMLFrameElementBase*>(m_dragTarget.get())->contentFrame() : 0;
        if (frame)
            frame->eventHandler()->cancelDragAndDrop(event, clipboard);
        else
            dispatchDragEvent(eventNames().dragleaveEvent, m_dragTarget.get(), event, clipboard);
    }
    clearDragState();
}

bool EventHandler::performDragAndDrop(const PlatformMouseEvent& event, Clipboard* clipboard)
{
    bool accept = false;
    if (m_dragTarget) {
        Frame* frame = (m_dragTarget->hasTagName(frameTag) || m_dragTarget->hasTagName(iframeTag)) 
                        ? static_cast<HTMLFrameElementBase*>(m_dragTarget.get())->contentFrame() : 0;
        if (frame)
            accept = frame->eventHandler()->performDragAndDrop(event, clipboard);
        else
            accept = dispatchDragEvent(eventNames().dropEvent, m_dragTarget.get(), event, clipboard);
    }
    clearDragState();
    return accept;
}

void EventHandler::clearDragState()
{
    m_dragTarget = 0;
    m_capturingMouseEventsNode = 0;
#if PLATFORM(MAC)
    m_sendingEventToSubview = false;
#endif
}

void EventHandler::setCapturingMouseEventsNode(PassRefPtr<Node> n)
{
    m_capturingMouseEventsNode = n;
}

MouseEventWithHitTestResults EventHandler::prepareMouseEvent(const HitTestRequest& request, const PlatformMouseEvent& mev)
{
    ASSERT(m_frame);
    ASSERT(m_frame->document());
    
    return m_frame->document()->prepareMouseEvent(request, documentPointForWindowPoint(m_frame, mev.pos()), mev);
}

#if ENABLE(SVG)
static inline SVGElementInstance* instanceAssociatedWithShadowTreeElement(Node* referenceNode)
{
    if (!referenceNode || !referenceNode->isSVGElement())
        return 0;

    Node* shadowTreeElement = referenceNode->shadowTreeRootNode();
    if (!shadowTreeElement)
        return 0;

    Node* shadowTreeParentElement = shadowTreeElement->shadowParentNode();
    if (!shadowTreeParentElement)
        return 0;

    ASSERT(shadowTreeParentElement->hasTagName(useTag));
    return static_cast<SVGUseElement*>(shadowTreeParentElement)->instanceForShadowTreeElement(referenceNode);
}
#endif

void EventHandler::updateMouseEventTargetNode(Node* targetNode, const PlatformMouseEvent& mouseEvent, bool fireMouseOverOut)
{
    Node* result = targetNode;
    
    // If we're capturing, we always go right to that node.
    if (m_capturingMouseEventsNode)
        result = m_capturingMouseEventsNode.get();
    else {
        // If the target node is a text node, dispatch on the parent node - rdar://4196646
        if (result && result->isTextNode())
            result = result->parentNode();
        if (result)
            result = result->shadowAncestorNode();
    }
    m_nodeUnderMouse = result;
#if ENABLE(SVG)
    m_instanceUnderMouse = instanceAssociatedWithShadowTreeElement(result);

    // <use> shadow tree elements may have been recloned, update node under mouse in any case
    if (m_lastInstanceUnderMouse) {
        SVGElement* lastCorrespondingElement = m_lastInstanceUnderMouse->correspondingElement();
        SVGElement* lastCorrespondingUseElement = m_lastInstanceUnderMouse->correspondingUseElement();

        if (lastCorrespondingElement && lastCorrespondingUseElement) {
            HashSet<SVGElementInstance*> instances = lastCorrespondingElement->instancesForElement();

            // Locate the recloned shadow tree element for our corresponding instance
            HashSet<SVGElementInstance*>::iterator end = instances.end();
            for (HashSet<SVGElementInstance*>::iterator it = instances.begin(); it != end; ++it) {
                SVGElementInstance* instance = (*it);
                ASSERT(instance->correspondingElement() == lastCorrespondingElement);

                if (instance == m_lastInstanceUnderMouse)
                    continue;

                if (instance->correspondingUseElement() != lastCorrespondingUseElement)
                    continue;

                SVGElement* shadowTreeElement = instance->shadowTreeElement();
                if (!shadowTreeElement->inDocument() || m_lastNodeUnderMouse == shadowTreeElement)
                    continue;

                m_lastNodeUnderMouse = shadowTreeElement;
                m_lastInstanceUnderMouse = instance;
                break;
            }
        }
    }
#endif

    // Fire mouseout/mouseover if the mouse has shifted to a different node.
    if (fireMouseOverOut) {
        if (m_lastNodeUnderMouse && m_lastNodeUnderMouse->document() != m_frame->document()) {
            m_lastNodeUnderMouse = 0;
            m_lastScrollbarUnderMouse = 0;
#if ENABLE(SVG)
            m_lastInstanceUnderMouse = 0;
#endif
        }

        if (m_lastNodeUnderMouse != m_nodeUnderMouse) {
            // send mouseout event to the old node
            if (m_lastNodeUnderMouse)
                EventTargetNodeCast(m_lastNodeUnderMouse.get())->dispatchMouseEvent(mouseEvent, eventNames().mouseoutEvent, 0, m_nodeUnderMouse.get());
            // send mouseover event to the new node
            if (m_nodeUnderMouse)
                EventTargetNodeCast(m_nodeUnderMouse.get())->dispatchMouseEvent(mouseEvent, eventNames().mouseoverEvent, 0, m_lastNodeUnderMouse.get());
        }
        m_lastNodeUnderMouse = m_nodeUnderMouse;
#if ENABLE(SVG)
        m_lastInstanceUnderMouse = instanceAssociatedWithShadowTreeElement(m_nodeUnderMouse.get());
#endif
    }
}

bool EventHandler::dispatchMouseEvent(const AtomicString& eventType, Node* targetNode, bool /*cancelable*/, int clickCount, const PlatformMouseEvent& mouseEvent, bool setUnder)
{
    m_frame->view()->resetDeferredRepaintDelay();

    updateMouseEventTargetNode(targetNode, mouseEvent, setUnder);

    bool swallowEvent = false;

    if (m_nodeUnderMouse)
        swallowEvent = EventTargetNodeCast(m_nodeUnderMouse.get())->dispatchMouseEvent(mouseEvent, eventType, clickCount);
    
    if (!swallowEvent && eventType == eventNames().mousedownEvent) {
        // Blur current focus node when a link/button is clicked; this
        // is expected by some sites that rely on onChange handlers running
        // from form fields before the button click is processed.
        Node* node = m_nodeUnderMouse.get();
        RenderObject* renderer = node ? node->renderer() : 0;
                
        // Walk up the render tree to search for a node to focus.
        // Walking up the DOM tree wouldn't work for shadow trees, like those behind the engine-based text fields.
        while (renderer) {
            node = renderer->element();
            if (node && node->isFocusable()) {
                // To fix <rdar://problem/4895428> Can't drag selected ToDo, we don't focus a 
                // node on mouse down if it's selected and inside a focused node. It will be 
                // focused if the user does a mouseup over it, however, because the mouseup
                // will set a selection inside it, which will call setFocuseNodeIfNeeded.
                ExceptionCode ec = 0;
                Node* n = node->isShadowNode() ? node->shadowParentNode() : node;
                if (m_frame->selection()->isRange() && 
                    m_frame->selection()->toRange()->compareNode(n, ec) == Range::NODE_INSIDE &&
                    n->isDescendantOf(m_frame->document()->focusedNode()))
                    return false;
                    
                break;
            }
            
            renderer = renderer->parent();
        }
        // If focus shift is blocked, we eat the event.  Note we should never clear swallowEvent
        // if the page already set it (e.g., by canceling default behavior).
        if (node && node->isMouseFocusable()) {
            if (!m_frame->page()->focusController()->setFocusedNode(node, m_frame))
                swallowEvent = true;
        } else if (!node || !node->focused()) {
            if (!m_frame->page()->focusController()->setFocusedNode(0, m_frame))
                swallowEvent = true;
        }
    }

    return swallowEvent;
}

bool EventHandler::handleWheelEvent(PlatformWheelEvent& e)
{
    Document* doc = m_frame->document();
    if (!doc)
        return false;

    RenderObject* docRenderer = doc->renderer();
    if (!docRenderer)
        return false;
    
    RefPtr<FrameView> protector(m_frame->view());

    IntPoint vPoint = m_frame->view()->windowToContents(e.pos());

    HitTestRequest request(true, false);
    HitTestResult result(vPoint);
    doc->renderView()->layer()->hitTest(request, result);
    Node* node = result.innerNode();
    
    if (node) {
        // Figure out which view to send the event to.
        RenderObject* target = node->renderer();
        
        if (result.isOverWidget() && target && target->isWidget()) {
            Widget* widget = static_cast<RenderWidget*>(target)->widget();

            if (widget && passWheelEventToWidget(e, widget)) {
                e.accept();
                return true;
            }
        }

        node = node->shadowAncestorNode();
        EventTargetNodeCast(node)->dispatchWheelEvent(e);
        if (e.isAccepted())
            return true;
            
        if (node->renderer()) {
            // Just break up into two scrolls if we need to.  Diagonal movement on 
            // a MacBook pro is an example of a 2-dimensional mouse wheel event (where both deltaX and deltaY can be set).
            scrollAndAcceptEvent(e.deltaX(), ScrollLeft, ScrollRight, e, node);
            scrollAndAcceptEvent(e.deltaY(), ScrollUp, ScrollDown, e, node);
        }
    }

    if (!e.isAccepted())
        m_frame->view()->wheelEvent(e);
    
    return e.isAccepted();
}

bool EventHandler::sendContextMenuEvent(const PlatformMouseEvent& event)
{
    Document* doc = m_frame->document();
    FrameView* v = m_frame->view();
    if (!doc || !v)
        return false;
    
    bool swallowEvent;
    IntPoint viewportPos = v->windowToContents(event.pos());
    MouseEventWithHitTestResults mev = doc->prepareMouseEvent(HitTestRequest(false, true), viewportPos, event);

    // Context menu events shouldn't select text in GTK+ applications or in Chromium.
    // FIXME: This should probably be configurable by embedders. Consider making it a WebPreferences setting.
    // See: https://bugs.webkit.org/show_bug.cgi?id=15279
#if !PLATFORM(GTK) && !PLATFORM(CHROMIUM)
    if (!m_frame->selection()->contains(viewportPos) && 
        // FIXME: In the editable case, word selection sometimes selects content that isn't underneath the mouse.
        // If the selection is non-editable, we do word selection to make it easier to use the contextual menu items
        // available for text selections.  But only if we're above text.
        (m_frame->selection()->isContentEditable() || mev.targetNode() && mev.targetNode()->isTextNode())) {
        m_mouseDownMayStartSelect = true; // context menu events are always allowed to perform a selection
        selectClosestWordOrLinkFromMouseEvent(mev);
    }
#endif

    swallowEvent = dispatchMouseEvent(eventNames().contextmenuEvent, mev.targetNode(), true, 0, event, true);
    
    return swallowEvent;
}

void EventHandler::scheduleHoverStateUpdate()
{
    if (!m_hoverTimer.isActive())
        m_hoverTimer.startOneShot(0);
}

// Whether or not a mouse down can begin the creation of a selection.  Fires the selectStart event.
bool EventHandler::canMouseDownStartSelect(Node* node)
{
    if (!node || !node->renderer())
        return true;
    
    // Some controls and images can't start a select on a mouse down.
    if (!node->canStartSelection())
        return false;
            
    for (RenderObject* curr = node->renderer(); curr; curr = curr->parent())    
        if (Node* node = curr->element())
            return EventTargetNodeCast(node)->dispatchEventForType(eventNames().selectstartEvent, true, true);
    
    return true;
}

bool EventHandler::canMouseDragExtendSelect(Node* node)
{
    if (!node || !node->renderer())
        return true;
            
    for (RenderObject* curr = node->renderer(); curr; curr = curr->parent())    
        if (Node* node = curr->element())
            return EventTargetNodeCast(node)->dispatchEventForType(eventNames().selectstartEvent, true, true);
    
    return true;
}

void EventHandler::setResizingFrameSet(HTMLFrameSetElement* frameSet)
{
    m_frameSetBeingResized = frameSet;
}

void EventHandler::resizeLayerDestroyed()
{
    ASSERT(m_resizeLayer);
    m_resizeLayer = 0;
}

void EventHandler::hoverTimerFired(Timer<EventHandler>*)
{
    m_hoverTimer.stop();

    ASSERT(m_frame);
    ASSERT(m_frame->document());

    if (RenderView* renderer = m_frame->contentRenderer()) {
        HitTestResult result(m_frame->view()->windowToContents(m_currentMousePosition));
        renderer->layer()->hitTest(HitTestRequest(false, false, true), result);
        m_frame->document()->updateRendering();
    }
}

static EventTargetNode* eventTargetNodeForDocument(Document* doc)
{
    if (!doc)
        return 0;
    Node* node = doc->focusedNode();
    if (!node) {
        if (doc->isHTMLDocument())
            node = doc->body();
        else
            node = doc->documentElement();
        if (!node)
            return 0;
    }
    return EventTargetNodeCast(node);
}

bool EventHandler::handleAccessKey(const PlatformKeyboardEvent& evt)
{
    // FIXME: Ignoring the state of Shift key is what neither IE nor Firefox do.
    // IE matches lower and upper case access keys regardless of Shift key state - but if both upper and
    // lower case variants are present in a document, the correct element is matched based on Shift key state.
    // Firefox only matches an access key if Shift is not pressed, and does that case-insensitively.
    ASSERT(!(accessKeyModifiers() & PlatformKeyboardEvent::ShiftKey));
    if ((evt.modifiers() & ~PlatformKeyboardEvent::ShiftKey) != accessKeyModifiers())
        return false;
    String key = evt.unmodifiedText();
    Element* elem = m_frame->document()->getElementByAccessKey(key.lower());
    if (!elem)
        return false;
    elem->accessKeyAction(false);
    return true;
}

bool EventHandler::needsKeyboardEventDisambiguationQuirks() const
{
    return false;
}

bool EventHandler::keyEvent(const PlatformKeyboardEvent& initialKeyEvent)
{
#if ENABLE(PAN_SCROLLING)
    if (m_frame->page()->mainFrame()->eventHandler()->panScrollInProgress() || m_autoscrollInProgress) {
        String escKeyId = "U+001B";
        // If a key is pressed while the autoscroll/panScroll is in progress then we want to stop
        if (initialKeyEvent.keyIdentifier() == escKeyId && initialKeyEvent.type() == PlatformKeyboardEvent::KeyUp) 
            stopAutoscrollTimer();

        // If we were in autoscroll/panscroll mode, we swallow the key event
        return true;
    }
#endif

    // Check for cases where we are too early for events -- possible unmatched key up
    // from pressing return in the location bar.
    RefPtr<EventTargetNode> node = eventTargetNodeForDocument(m_frame->document());
    if (!node)
        return false;
    
    m_frame->view()->resetDeferredRepaintDelay();

    // FIXME: what is this doing here, in keyboard event handler?
    m_frame->loader()->resetMultipleFormSubmissionProtection();

    // In IE, access keys are special, they are handled after default keydown processing, but cannot be canceled - this is hard to match.
    // On Mac OS X, we process them before dispatching keydown, as the default keydown handler implements Emacs key bindings, which may conflict
    // with access keys. Then we dispatch keydown, but suppress its default handling.
    // On Windows, WebKit explicitly calls handleAccessKey() instead of dispatching a keypress event for WM_SYSCHAR messages.
    // Other platforms currently match either Mac or Windows behavior, depending on whether they send combined KeyDown events.
    bool matchedAnAccessKey = false;
    if (initialKeyEvent.type() == PlatformKeyboardEvent::KeyDown)
        matchedAnAccessKey = handleAccessKey(initialKeyEvent);

    // FIXME: it would be fair to let an input method handle KeyUp events before DOM dispatch.
    if (initialKeyEvent.type() == PlatformKeyboardEvent::KeyUp || initialKeyEvent.type() == PlatformKeyboardEvent::Char)
        return !node->dispatchKeyEvent(initialKeyEvent);

    bool backwardCompatibilityMode = needsKeyboardEventDisambiguationQuirks();

    ExceptionCode ec;
    PlatformKeyboardEvent keyDownEvent = initialKeyEvent;    
    if (keyDownEvent.type() != PlatformKeyboardEvent::RawKeyDown)
        keyDownEvent.disambiguateKeyDownEvent(PlatformKeyboardEvent::RawKeyDown, backwardCompatibilityMode);
    RefPtr<KeyboardEvent> keydown = KeyboardEvent::create(keyDownEvent, m_frame->document()->defaultView());
    if (matchedAnAccessKey)
        keydown->setDefaultPrevented(true);
    keydown->setTarget(node);

    if (initialKeyEvent.type() == PlatformKeyboardEvent::RawKeyDown) {
        node->dispatchEvent(keydown, ec);
        return keydown->defaultHandled() || keydown->defaultPrevented();
    }

    // Run input method in advance of DOM event handling.  This may result in the IM
    // modifying the page prior the keydown event, but this behaviour is necessary
    // in order to match IE:
    // 1. preventing default handling of keydown and keypress events has no effect on IM input;
    // 2. if an input method handles the event, its keyCode is set to 229 in keydown event.
    m_frame->editor()->handleInputMethodKeydown(keydown.get());
    
    bool handledByInputMethod = keydown->defaultHandled();
    
    if (handledByInputMethod) {
        keyDownEvent.setWindowsVirtualKeyCode(CompositionEventKeyCode);
        keydown = KeyboardEvent::create(keyDownEvent, m_frame->document()->defaultView());
        keydown->setTarget(node);
        keydown->setDefaultHandled();
    }

    node->dispatchEvent(keydown, ec);
    bool keydownResult = keydown->defaultHandled() || keydown->defaultPrevented();
    if (handledByInputMethod || (keydownResult && !backwardCompatibilityMode))
        return keydownResult;
    
    // Focus may have changed during keydown handling, so refetch node.
    // But if we are dispatching a fake backward compatibility keypress, then we pretend that the keypress happened on the original node.
    if (!keydownResult) {
        node = eventTargetNodeForDocument(m_frame->document());
        if (!node)
            return false;
    }

    PlatformKeyboardEvent keyPressEvent = initialKeyEvent;
    keyPressEvent.disambiguateKeyDownEvent(PlatformKeyboardEvent::Char, backwardCompatibilityMode);
    if (keyPressEvent.text().isEmpty())
        return keydownResult;
    RefPtr<KeyboardEvent> keypress = KeyboardEvent::create(keyPressEvent, m_frame->document()->defaultView());
    keypress->setTarget(node);
    if (keydownResult)
        keypress->setDefaultPrevented(true);
#if PLATFORM(MAC)
    keypress->keypressCommands() = keydown->keypressCommands();
#endif
    node->dispatchEvent(keypress, ec);

    return keydownResult || keypress->defaultPrevented() || keypress->defaultHandled();
}

void EventHandler::handleKeyboardSelectionMovement(KeyboardEvent* event)
{
    if (!event)
        return;
    
    String key = event->keyIdentifier();           
    bool isShifted = event->getModifierState("Shift");
    bool isOptioned = event->getModifierState("Alt");
    
    if (key == "Up") {
        m_frame->selection()->modify((isShifted) ? SelectionController::EXTEND : SelectionController::MOVE, SelectionController::BACKWARD, LineGranularity, true);
        event->setDefaultHandled();
    }
    else if (key == "Down") { 
        m_frame->selection()->modify((isShifted) ? SelectionController::EXTEND : SelectionController::MOVE, SelectionController::FORWARD, LineGranularity, true);
        event->setDefaultHandled();
    }
    else if (key == "Left") {
        m_frame->selection()->modify((isShifted) ? SelectionController::EXTEND : SelectionController::MOVE, SelectionController::LEFT, (isOptioned) ? WordGranularity : CharacterGranularity, true);
        event->setDefaultHandled();
    }
    else if (key == "Right") {
        m_frame->selection()->modify((isShifted) ? SelectionController::EXTEND : SelectionController::MOVE, SelectionController::RIGHT, (isOptioned) ? WordGranularity : CharacterGranularity, true);
        event->setDefaultHandled();
    }    
}
    
void EventHandler::defaultKeyboardEventHandler(KeyboardEvent* event)
{
   if (event->type() == eventNames().keydownEvent) {
        m_frame->editor()->handleKeyboardEvent(event);
        if (event->defaultHandled())
            return;
        if (event->keyIdentifier() == "U+0009")
            defaultTabEventHandler(event);

       // provides KB navigation and selection for enhanced accessibility users
       if (AXObjectCache::accessibilityEnhancedUserInterfaceEnabled())
           handleKeyboardSelectionMovement(event);       
   }
   if (event->type() == eventNames().keypressEvent) {
        m_frame->editor()->handleKeyboardEvent(event);
        if (event->defaultHandled())
            return;
        if (event->charCode() == ' ')
            defaultSpaceEventHandler(event);
   }
}

bool EventHandler::dragHysteresisExceeded(const FloatPoint& floatDragViewportLocation) const
{
    IntPoint dragViewportLocation((int)floatDragViewportLocation.x(), (int)floatDragViewportLocation.y());
    return dragHysteresisExceeded(dragViewportLocation);
}
    
bool EventHandler::dragHysteresisExceeded(const IntPoint& dragViewportLocation) const
{
    IntPoint dragLocation = m_frame->view()->windowToContents(dragViewportLocation);
    IntSize delta = dragLocation - m_mouseDownPos;
    
    int threshold = GeneralDragHysteresis;
    if (dragState().m_dragSrcIsImage)
        threshold = ImageDragHysteresis;
    else if (dragState().m_dragSrcIsLink)
        threshold = LinkDragHysteresis;
    else if (dragState().m_dragSrcInSelection)
        threshold = TextDragHysteresis;
    
    return abs(delta.width()) >= threshold || abs(delta.height()) >= threshold;
}
    

bool EventHandler::shouldDragAutoNode(Node* node, const IntPoint& point) const
{
    UNUSED_PARAM(node);
    UNUSED_PARAM(point);
    return false;
}
    
  
bool EventHandler::handleTextInputEvent(const String& text, Event* underlyingEvent, bool isLineBreak, bool isBackTab)
{
    // Platforms should differentiate real commands like selectAll from text input in disguise (like insertNewline),
    // and avoid dispatching text input events from keydown default handlers.
    ASSERT(!underlyingEvent || !underlyingEvent->isKeyboardEvent() || static_cast<KeyboardEvent*>(underlyingEvent)->type() == eventNames().keypressEvent);

    if (!m_frame)
        return false;

    EventTarget* target;
    if (underlyingEvent)
        target = underlyingEvent->target();
    else
        target = eventTargetNodeForDocument(m_frame->document());
    if (!target)
        return false;
    
    m_frame->view()->resetDeferredRepaintDelay();

    RefPtr<TextEvent> event = TextEvent::create(m_frame->domWindow(), text);
    event->setUnderlyingEvent(underlyingEvent);
    event->setIsLineBreak(isLineBreak);
    event->setIsBackTab(isBackTab);
    ExceptionCode ec;
    target->dispatchEvent(event, ec);
    return event->defaultHandled();
}
    
    
#if !PLATFORM(MAC) && !PLATFORM(QT)
bool EventHandler::invertSenseOfTabsToLinks(KeyboardEvent*) const
{
    return false;
}
#endif

bool EventHandler::tabsToLinks(KeyboardEvent* event) const
{
    Page* page = m_frame->page();
    if (!page)
        return false;

    if (page->chrome()->client()->tabsToLinks())
        return !invertSenseOfTabsToLinks(event);

    return invertSenseOfTabsToLinks(event);
}

void EventHandler::defaultTextInputEventHandler(TextEvent* event)
{
    String data = event->data();
    if (data == "\n") {
        if (event->isLineBreak()) {
            if (m_frame->editor()->insertLineBreak())
                event->setDefaultHandled();
        } else {
            if (m_frame->editor()->insertParagraphSeparator())
                event->setDefaultHandled();
        }
    } else {
        if (m_frame->editor()->insertTextWithoutSendingTextEvent(data, false, event))
            event->setDefaultHandled();
    }
}

#if PLATFORM(QT) || PLATFORM(MAC)

// These two platforms handle the space event in the platform-specific WebKit code.
// Eventually it would be good to eliminate that and use the code here instead, but
// the Qt version is inside an ifdef and the Mac version has some extra behavior
// so we can't unify everything yet.
void EventHandler::defaultSpaceEventHandler(KeyboardEvent*)
{
}

#else

void EventHandler::defaultSpaceEventHandler(KeyboardEvent* event)
{
    ScrollDirection direction = event->shiftKey() ? ScrollUp : ScrollDown;
    if (scrollOverflow(direction, ScrollByPage)) {
        event->setDefaultHandled();
        return;
    }

    FrameView* view = m_frame->view();
    if (!view)
        return;

    if (view->scroll(direction, ScrollByPage))
        event->setDefaultHandled();
}

#endif

void EventHandler::defaultTabEventHandler(KeyboardEvent* event)
{
    // We should only advance focus on tabs if no special modifier keys are held down.
    if (event->ctrlKey() || event->metaKey() || event->altGraphKey())
        return;

    Page* page = m_frame->page();
    if (!page)
        return;
    if (!page->tabKeyCyclesThroughElements())
        return;

    FocusDirection focusDirection = event->shiftKey() ? FocusDirectionBackward : FocusDirectionForward;

    // Tabs can be used in design mode editing.
    if (m_frame->document()->inDesignMode())
        return;

    if (page->focusController()->advanceFocus(focusDirection, event))
        event->setDefaultHandled();
}

void EventHandler::capsLockStateMayHaveChanged()
{
    if (Document* d = m_frame->document())
        if (Node* node = d->focusedNode())
            if (RenderObject* r = node->renderer())
                r->capsLockStateMayHaveChanged();
}

unsigned EventHandler::pendingFrameUnloadEventCount()
{
    return m_pendingFrameUnloadEventCount;
}

void EventHandler::addPendingFrameUnloadEventCount() 
{
    m_pendingFrameUnloadEventCount += 1;
    m_frame->page()->changePendingUnloadEventCount(1);
    return; 
}
    
void EventHandler::removePendingFrameUnloadEventCount() 
{
    ASSERT( (-1 + (int)m_pendingFrameUnloadEventCount) >= 0 );
    m_pendingFrameUnloadEventCount -= 1;
    m_frame->page()->changePendingUnloadEventCount(-1);
    return; 
}
    
void EventHandler::clearPendingFrameUnloadEventCount() 
{
    m_frame->page()->changePendingUnloadEventCount(-((int)m_pendingFrameUnloadEventCount));
    m_pendingFrameUnloadEventCount = 0;
    return; 
}

unsigned EventHandler::pendingFrameBeforeUnloadEventCount()
{
    return m_pendingFrameBeforeUnloadEventCount;
}

void EventHandler::addPendingFrameBeforeUnloadEventCount() 
{
    m_pendingFrameBeforeUnloadEventCount += 1;
    m_frame->page()->changePendingBeforeUnloadEventCount(1);
    return; 
}
    
void EventHandler::removePendingFrameBeforeUnloadEventCount() 
{
    ASSERT( (-1 + (int)m_pendingFrameBeforeUnloadEventCount) >= 0 );
    m_pendingFrameBeforeUnloadEventCount -= 1;
    m_frame->page()->changePendingBeforeUnloadEventCount(-1);
    return; 
}

    void EventHandler::clearPendingFrameBeforeUnloadEventCount() 
{
    m_frame->page()->changePendingBeforeUnloadEventCount(-((int)m_pendingFrameBeforeUnloadEventCount));
    m_pendingFrameBeforeUnloadEventCount = 0;
    return; 
}

bool EventHandler::passMousePressEventToScrollbar(MouseEventWithHitTestResults& mev, Scrollbar* scrollbar)
{
    if (!scrollbar || !scrollbar->enabled())
        return false;
    return scrollbar->mouseDown(mev.event());
}

}

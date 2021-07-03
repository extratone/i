/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InjectedBundleNodeHandle.h"

#include "InjectedBundleRangeHandle.h"
#include "ShareableBitmap.h"
#include "WebFrame.h"
#include "WebFrameLoaderClient.h"
#include "WebImage.h"
#include <JavaScriptCore/APICast.h>
#include <WebCore/Document.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameView.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/HTMLFrameElement.h>
#include <WebCore/HTMLIFrameElement.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/HTMLTableCellElement.h>
#include <WebCore/HTMLTextAreaElement.h>
#include <WebCore/IntRect.h>
#include <WebCore/JSNode.h>
#include <WebCore/Node.h>
#include <WebCore/Page.h>
#include <WebCore/Position.h>
#include <WebCore/Range.h>
#include <WebCore/RenderObject.h>
#include <WebCore/VisiblePosition.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;
using namespace HTMLNames;

namespace WebKit {

typedef HashMap<Node*, InjectedBundleNodeHandle*> DOMHandleCache;

static DOMHandleCache& domHandleCache()
{
    static NeverDestroyed<DOMHandleCache> cache;
    return cache;
}

RefPtr<InjectedBundleNodeHandle> InjectedBundleNodeHandle::getOrCreate(JSContextRef, JSObjectRef object)
{
    Node* node = JSNode::toWrapped(toJS(object));
    return getOrCreate(node);
}

RefPtr<InjectedBundleNodeHandle> InjectedBundleNodeHandle::getOrCreate(Node* node)
{
    if (!node)
        return nullptr;

    return InjectedBundleNodeHandle::getOrCreate(*node);
}

Ref<InjectedBundleNodeHandle> InjectedBundleNodeHandle::getOrCreate(Node& node)
{
    DOMHandleCache::AddResult result = domHandleCache().add(&node, nullptr);
    if (!result.isNewEntry)
        return Ref<InjectedBundleNodeHandle>(*result.iterator->value);

    Ref<InjectedBundleNodeHandle> nodeHandle = InjectedBundleNodeHandle::create(node);
    result.iterator->value = nodeHandle.ptr();
    return nodeHandle;
}

Ref<InjectedBundleNodeHandle> InjectedBundleNodeHandle::create(Node& node)
{
    return adoptRef(*new InjectedBundleNodeHandle(node));
}

InjectedBundleNodeHandle::InjectedBundleNodeHandle(Node& node)
    : m_node(node)
{
}

InjectedBundleNodeHandle::~InjectedBundleNodeHandle()
{
    domHandleCache().remove(m_node.ptr());
}

Node* InjectedBundleNodeHandle::coreNode()
{
    return m_node.ptr();
}

Ref<InjectedBundleNodeHandle> InjectedBundleNodeHandle::document()
{
    return getOrCreate(m_node->document());
}

// Additional DOM Operations
// Note: These should only be operations that are not exposed to JavaScript.

IntRect InjectedBundleNodeHandle::elementBounds()
{
    if (!is<Element>(m_node))
        return IntRect();

    return downcast<Element>(m_node.get()).boundsInRootViewSpace();
}
    
IntRect InjectedBundleNodeHandle::renderRect(bool* isReplaced)
{
    return m_node->pixelSnappedRenderRect(isReplaced);
}

static PassRefPtr<WebImage> imageForRect(FrameView* frameView, const IntRect& rect, SnapshotOptions options)
{
    IntSize bitmapSize = rect.size();
    float scaleFactor = frameView->frame().page()->deviceScaleFactor();
    bitmapSize.scale(scaleFactor);

    RefPtr<WebImage> snapshot = WebImage::create(bitmapSize, snapshotOptionsToImageOptions(options));
    if (!snapshot->bitmap())
        return 0;

    auto graphicsContext = snapshot->bitmap()->createGraphicsContext();
    graphicsContext->clearRect(IntRect(IntPoint(), bitmapSize));
    graphicsContext->applyDeviceScaleFactor(scaleFactor);
    graphicsContext->translate(-rect.x(), -rect.y());

    FrameView::SelectionInSnapshot shouldPaintSelection = FrameView::IncludeSelection;
    if (options & SnapshotOptionsExcludeSelectionHighlighting)
        shouldPaintSelection = FrameView::ExcludeSelection;

    PaintBehavior paintBehavior = frameView->paintBehavior() | PaintBehaviorFlattenCompositingLayers;
    if (options & SnapshotOptionsForceBlackText)
        paintBehavior |= PaintBehaviorForceBlackText;
    if (options & SnapshotOptionsForceWhiteText)
        paintBehavior |= PaintBehaviorForceWhiteText;

    PaintBehavior oldPaintBehavior = frameView->paintBehavior();
    frameView->setPaintBehavior(paintBehavior);
    frameView->paintContentsForSnapshot(graphicsContext.get(), rect, shouldPaintSelection, FrameView::DocumentCoordinates);
    frameView->setPaintBehavior(oldPaintBehavior);

    return snapshot.release();
}

PassRefPtr<WebImage> InjectedBundleNodeHandle::renderedImage(SnapshotOptions options)
{
    Frame* frame = m_node->document().frame();
    if (!frame)
        return nullptr;

    FrameView* frameView = frame->view();
    if (!frameView)
        return nullptr;

    m_node->document().updateLayout();

    RenderObject* renderer = m_node->renderer();
    if (!renderer)
        return nullptr;

    LayoutRect topLevelRect;
    IntRect paintingRect = snappedIntRect(renderer->paintingRootRect(topLevelRect));

    frameView->setNodeToDraw(m_node.ptr());
    RefPtr<WebImage> image = imageForRect(frameView, paintingRect, options);
    frameView->setNodeToDraw(0);

    return image.release();
}

PassRefPtr<InjectedBundleRangeHandle> InjectedBundleNodeHandle::visibleRange()
{
    VisiblePosition start = firstPositionInNode(m_node.ptr());
    VisiblePosition end = lastPositionInNode(m_node.ptr());

    RefPtr<Range> range = makeRange(start, end);
    return InjectedBundleRangeHandle::getOrCreate(range.get());
}

void InjectedBundleNodeHandle::setHTMLInputElementValueForUser(const String& value)
{
    if (!is<HTMLInputElement>(m_node))
        return;

    downcast<HTMLInputElement>(m_node.get()).setValueForUser(value);
}

bool InjectedBundleNodeHandle::isHTMLInputElementAutoFilled() const
{
    if (!is<HTMLInputElement>(m_node))
        return false;
    
    return downcast<HTMLInputElement>(m_node.get()).isAutoFilled();
}

void InjectedBundleNodeHandle::setHTMLInputElementAutoFilled(bool filled)
{
    if (!is<HTMLInputElement>(m_node))
        return;

    downcast<HTMLInputElement>(m_node.get()).setAutoFilled(filled);
}

bool InjectedBundleNodeHandle::isHTMLInputElementAutoFillButtonEnabled() const
{
    if (!is<HTMLInputElement>(m_node))
        return false;
    
    return downcast<HTMLInputElement>(m_node.get()).showAutoFillButton();
}

void InjectedBundleNodeHandle::setHTMLInputElementAutoFillButtonEnabled(bool filled)
{
    if (!is<HTMLInputElement>(m_node))
        return;

    downcast<HTMLInputElement>(m_node.get()).setShowAutoFillButton(filled);
}

IntRect InjectedBundleNodeHandle::htmlInputElementAutoFillButtonBounds()
{
    if (!is<HTMLInputElement>(m_node))
        return IntRect();

    auto autoFillButton = downcast<HTMLInputElement>(m_node.get()).autoFillButtonElement();
    if (!autoFillButton)
        return IntRect();

    return autoFillButton->boundsInRootViewSpace();
}

bool InjectedBundleNodeHandle::htmlInputElementLastChangeWasUserEdit()
{
    if (!is<HTMLInputElement>(m_node))
        return false;

    return downcast<HTMLInputElement>(m_node.get()).lastChangeWasUserEdit();
}

bool InjectedBundleNodeHandle::htmlTextAreaElementLastChangeWasUserEdit()
{
    if (!is<HTMLTextAreaElement>(m_node))
        return false;

    return downcast<HTMLTextAreaElement>(m_node.get()).lastChangeWasUserEdit();
}

bool InjectedBundleNodeHandle::isTextField() const
{
    if (!is<HTMLInputElement>(m_node))
        return false;

    return downcast<HTMLInputElement>(m_node.get()).isText();
}

PassRefPtr<InjectedBundleNodeHandle> InjectedBundleNodeHandle::htmlTableCellElementCellAbove()
{
    if (!is<HTMLTableCellElement>(m_node))
        return nullptr;

    return getOrCreate(downcast<HTMLTableCellElement>(m_node.get()).cellAbove());
}

PassRefPtr<WebFrame> InjectedBundleNodeHandle::documentFrame()
{
    if (!m_node->isDocumentNode())
        return nullptr;

    Frame* frame = downcast<Document>(m_node.get()).frame();
    if (!frame)
        return nullptr;

    return WebFrame::fromCoreFrame(*frame);
}

PassRefPtr<WebFrame> InjectedBundleNodeHandle::htmlFrameElementContentFrame()
{
    if (!is<HTMLFrameElement>(m_node))
        return nullptr;

    Frame* frame = downcast<HTMLFrameElement>(m_node.get()).contentFrame();
    if (!frame)
        return nullptr;

    return WebFrame::fromCoreFrame(*frame);
}

PassRefPtr<WebFrame> InjectedBundleNodeHandle::htmlIFrameElementContentFrame()
{
    if (!is<HTMLIFrameElement>(m_node))
        return nullptr;

    Frame* frame = downcast<HTMLIFrameElement>(m_node.get()).contentFrame();
    if (!frame)
        return nullptr;

    return WebFrame::fromCoreFrame(*frame);
}

} // namespace WebKit

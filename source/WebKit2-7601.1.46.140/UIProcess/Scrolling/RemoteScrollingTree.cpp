/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "RemoteScrollingTree.h"

#if ENABLE(ASYNC_SCROLLING)

#include "RemoteLayerTreeHost.h"
#include "RemoteScrollingCoordinatorProxy.h"
#include <WebCore/ScrollingTreeFixedNode.h>
#include <WebCore/ScrollingTreeStickyNode.h>

#if PLATFORM(IOS)
#include "ScrollingTreeOverflowScrollingNodeIOS.h"
#include <WebCore/ScrollingTreeFrameScrollingNodeIOS.h>
#else
#include <WebCore/ScrollingTreeFrameScrollingNodeMac.h>
#endif

using namespace WebCore;

namespace WebKit {

Ref<RemoteScrollingTree> RemoteScrollingTree::create(RemoteScrollingCoordinatorProxy& scrollingCoordinator)
{
    return adoptRef(*new RemoteScrollingTree(scrollingCoordinator));
}

RemoteScrollingTree::RemoteScrollingTree(RemoteScrollingCoordinatorProxy& scrollingCoordinator)
    : m_scrollingCoordinatorProxy(scrollingCoordinator)
{
}

RemoteScrollingTree::~RemoteScrollingTree()
{
}

ScrollingTree::EventResult RemoteScrollingTree::tryToHandleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    if (shouldHandleWheelEventSynchronously(wheelEvent))
        return SendToMainThread;

    if (willWheelEventStartSwipeGesture(wheelEvent))
        return DidNotHandleEvent;

    handleWheelEvent(wheelEvent);
    return DidHandleEvent;
}

#if PLATFORM(MAC)
void RemoteScrollingTree::handleWheelEventPhase(PlatformWheelEventPhase phase)
{
    // FIXME: hand off to m_scrollingCoordinatorProxy?
}
#endif

#if PLATFORM(IOS)
WebCore::FloatRect RemoteScrollingTree::fixedPositionRect()
{
    return m_scrollingCoordinatorProxy.customFixedPositionRect();
}

void RemoteScrollingTree::scrollingTreeNodeWillStartPanGesture()
{
    m_scrollingCoordinatorProxy.scrollingTreeNodeWillStartPanGesture();
}
    
void RemoteScrollingTree::scrollingTreeNodeWillStartScroll()
{
    m_scrollingCoordinatorProxy.scrollingTreeNodeWillStartScroll();
}

void RemoteScrollingTree::scrollingTreeNodeDidEndScroll()
{
    m_scrollingCoordinatorProxy.scrollingTreeNodeDidEndScroll();
}

#endif

void RemoteScrollingTree::scrollingTreeNodeDidScroll(ScrollingNodeID nodeID, const FloatPoint& scrollPosition, SetOrSyncScrollingLayerPosition scrollingLayerPositionAction)
{
    m_scrollingCoordinatorProxy.scrollingTreeNodeDidScroll(nodeID, scrollPosition, scrollingLayerPositionAction);
}

void RemoteScrollingTree::scrollingTreeNodeRequestsScroll(ScrollingNodeID nodeID, const FloatPoint& scrollPosition, bool representsProgrammaticScroll)
{
    m_scrollingCoordinatorProxy.scrollingTreeNodeRequestsScroll(nodeID, scrollPosition, representsProgrammaticScroll);
}

PassRefPtr<ScrollingTreeNode> RemoteScrollingTree::createScrollingTreeNode(ScrollingNodeType nodeType, ScrollingNodeID nodeID)
{
    switch (nodeType) {
    case FrameScrollingNode:
#if PLATFORM(IOS)
        return ScrollingTreeFrameScrollingNodeIOS::create(*this, nodeID);
#else
        return ScrollingTreeFrameScrollingNodeMac::create(*this, nodeID);
#endif
    case OverflowScrollingNode:
#if PLATFORM(IOS)
        return ScrollingTreeOverflowScrollingNodeIOS::create(*this, nodeID);
#else
        ASSERT_NOT_REACHED();
        return nullptr;
#endif
    case FixedNode:
        return ScrollingTreeFixedNode::create(*this, nodeID);
    case StickyNode:
        return ScrollingTreeStickyNode::create(*this, nodeID);
    }
    return nullptr;
}

void RemoteScrollingTree::currentSnapPointIndicesDidChange(ScrollingNodeID nodeID, unsigned horizontal, unsigned vertical)
{
    m_scrollingCoordinatorProxy.currentSnapPointIndicesDidChange(nodeID, horizontal, vertical);
}

} // namespace WebKit

#endif // ENABLE(ASYNC_SCROLLING)

/*
 * Copyright (C) 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
#include "RenderObjectChildList.h"

#include "AXObjectCache.h"
#include "RenderCounter.h"
#include "RenderObject.h"
#include "RenderStyle.h"
#include "RenderView.h"

namespace WebCore {

void RenderObjectChildList::destroyLeftoverChildren()
{
    while (firstChild()) {
        if (firstChild()->isListMarker() || (firstChild()->style()->styleType() == FIRST_LETTER && !firstChild()->isText()))
            firstChild()->remove();  // List markers are owned by their enclosing list and so don't get destroyed by this container. Similarly, first letters are destroyed by their remaining text fragment.
        else if (firstChild()->isRunIn() && firstChild()->node()) {
            firstChild()->node()->setRenderer(0);
            firstChild()->node()->setNeedsStyleRecalc();
            firstChild()->destroy();
        } else {
            // Destroy any anonymous children remaining in the render tree, as well as implicit (shadow) DOM elements like those used in the engine-based text fields.
            if (firstChild()->node())
                firstChild()->node()->setRenderer(0);
            firstChild()->destroy();
        }
    }
}

RenderObject* RenderObjectChildList::removeChildNode(RenderObject* owner, RenderObject* oldChild, bool notifyRenderer)
{
    ASSERT(oldChild->parent() == owner);

    if (oldChild->isFloatingOrOutOfFlowPositioned())
        toRenderBox(oldChild)->removeFloatingOrPositionedChildFromBlockLists();

    // So that we'll get the appropriate dirty bit set (either that a normal flow child got yanked or
    // that a positioned child got yanked). We also repaint, so that the area exposed when the child
    // disappears gets repainted properly.
    if (!owner->documentBeingDestroyed() && notifyRenderer && oldChild->everHadLayout()) {
        oldChild->setNeedsLayoutAndPrefWidthsRecalc();
        // We only repaint |oldChild| if we have a RenderLayer as its visual overflow may not be tracked by its parent.
        if (oldChild->isBody())
            owner->view()->repaint();
        else
            oldChild->repaint();
    }

    // If we have a line box wrapper, delete it.
    if (oldChild->isBox())
        toRenderBox(oldChild)->deleteLineBoxWrapper();

    // If oldChild is the start or end of the selection, then clear the selection to
    // avoid problems of invalid pointers.
    // FIXME: The FrameSelection should be responsible for this when it
    // is notified of DOM mutations.
    if (!owner->documentBeingDestroyed() && oldChild->isSelectionBorder())
        owner->view()->clearSelection();

    if (!owner->documentBeingDestroyed() && notifyRenderer)
        oldChild->willBeRemovedFromTree();

    // WARNING: There should be no code running between willBeRemovedFromTree and the actual removal below.
    // This is needed to avoid race conditions where willBeRemovedFromTree would dirty the tree's structure
    // and the code running here would force an untimely rebuilding, leaving |oldChild| dangling.

    if (oldChild->previousSibling())
        oldChild->previousSibling()->setNextSibling(oldChild->nextSibling());
    if (oldChild->nextSibling())
        oldChild->nextSibling()->setPreviousSibling(oldChild->previousSibling());

    if (firstChild() == oldChild)
        setFirstChild(oldChild->nextSibling());
    if (lastChild() == oldChild)
        setLastChild(oldChild->previousSibling());

    oldChild->setPreviousSibling(0);
    oldChild->setNextSibling(0);
    oldChild->setParent(0);

    // rendererRemovedFromTree walks the whole subtree. We can improve performance
    // by skipping this step when destroying the entire tree.
    if (!owner->documentBeingDestroyed())
        RenderCounter::rendererRemovedFromTree(oldChild);

    if (AXObjectCache* cache = owner->document()->existingAXObjectCache())
        cache->childrenChanged(owner);

    return oldChild;
}

void RenderObjectChildList::insertChildNode(RenderObject* owner, RenderObject* newChild, RenderObject* beforeChild, bool notifyRenderer)
{
    ASSERT(!newChild->parent());
    ASSERT(!owner->isBlockFlow() || (!newChild->isTableSection() && !newChild->isTableRow() && !newChild->isTableCell()));

    while (beforeChild && beforeChild->parent() && beforeChild->parent() != owner)
        beforeChild = beforeChild->parent();

    // This should never happen, but if it does prevent render tree corruption
    // where child->parent() ends up being owner but child->nextSibling()->parent()
    // is not owner.
    if (beforeChild && beforeChild->parent() != owner) {
        ASSERT_NOT_REACHED();
        return;
    }

    newChild->setParent(owner);

    if (firstChild() == beforeChild)
        setFirstChild(newChild);

    if (beforeChild) {
        RenderObject* previousSibling = beforeChild->previousSibling();
        if (previousSibling)
            previousSibling->setNextSibling(newChild);
        newChild->setPreviousSibling(previousSibling);
        newChild->setNextSibling(beforeChild);
        beforeChild->setPreviousSibling(newChild);
    } else {
        if (lastChild())
            lastChild()->setNextSibling(newChild);
        newChild->setPreviousSibling(lastChild());
        setLastChild(newChild);
    }

    if (!owner->documentBeingDestroyed() && notifyRenderer)
        newChild->insertedIntoTree();

    if (!owner->documentBeingDestroyed()) {
        RenderCounter::rendererSubtreeAttached(newChild);
    }

    newChild->setNeedsLayoutAndPrefWidthsRecalc();
    if (!owner->normalChildNeedsLayout())
        owner->setChildNeedsLayout(true); // We may supply the static position for an absolute positioned child.

    if (AXObjectCache* cache = owner->document()->axObjectCache())
        cache->childrenChanged(owner);
}

} // namespace WebCore

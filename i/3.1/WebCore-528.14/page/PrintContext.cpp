/*
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2007 Apple Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "PrintContext.h"

#include "GraphicsContext.h"
#include "Frame.h"
#include "FrameView.h"
#include "RenderView.h"

using namespace WebCore;

namespace WebCore {

PrintContext::PrintContext(Frame* frame)
    : m_frame(frame)
{
}

PrintContext::~PrintContext()
{
    m_pageRects.clear();
}

int PrintContext::pageCount() const
{
    return m_pageRects.size();
}

void PrintContext::computePageRects(const FloatRect& printRect, float headerHeight, float footerHeight, float userScaleFactor, float& outPageHeight)
{
    m_pageRects.clear();
    outPageHeight = 0;

    if (!m_frame->document() || !m_frame->view() || !m_frame->document()->renderer())
        return;

    RenderView* root = static_cast<RenderView*>(m_frame->document()->renderer());

    if (!root) {
        LOG_ERROR("document to be printed has no renderer");
        return;
    }

    if (userScaleFactor <= 0) {
        LOG_ERROR("userScaleFactor has bad value %.2f", userScaleFactor);
        return;
    }

    float ratio = printRect.height() / printRect.width();

    float pageWidth  = (float)root->docWidth();
    float pageHeight = pageWidth * ratio;
    outPageHeight = pageHeight;   // this is the height of the page adjusted by margins
    pageHeight -= headerHeight + footerHeight;

    if (pageHeight <= 0) {
        LOG_ERROR("pageHeight has bad value %.2f", pageHeight);
        return;
    }

    float currPageHeight = pageHeight / userScaleFactor;
    float docHeight = root->layer()->height();
    float currPageWidth = pageWidth / userScaleFactor;

    // always return at least one page, since empty files should print a blank page
    float printedPagesHeight = 0.0;
    do {
        float proposedBottom = std::min(docHeight, printedPagesHeight + pageHeight);
        m_frame->adjustPageHeight(&proposedBottom, printedPagesHeight, proposedBottom, printedPagesHeight);
        currPageHeight = max(1.0f, proposedBottom - printedPagesHeight);

        m_pageRects.append(IntRect(0, (int)printedPagesHeight, (int)currPageWidth, (int)currPageHeight));
        printedPagesHeight += currPageHeight;
    } while (printedPagesHeight < docHeight);
}

void PrintContext::begin(float width)
{
    // By imaging to a width a little wider than the available pixels,
    // thin pages will be scaled down a little, matching the way they
    // print in IE and Camino. This lets them use fewer sheets than they
    // would otherwise, which is presumably why other browsers do this.
    // Wide pages will be scaled down more than this.
    const float PrintingMinimumShrinkFactor = 1.25f;

    // This number determines how small we are willing to reduce the page content
    // in order to accommodate the widest line. If the page would have to be
    // reduced smaller to make the widest line fit, we just clip instead (this
    // behavior matches MacIE and Mozilla, at least)
    const float PrintingMaximumShrinkFactor = 2.0f;

    float minLayoutWidth = width * PrintingMinimumShrinkFactor;
    float maxLayoutWidth = width * PrintingMaximumShrinkFactor;

    // FIXME: This will modify the rendering of the on-screen frame.
    // Could lead to flicker during printing.
    m_frame->setPrinting(true, minLayoutWidth, maxLayoutWidth, true);
}

void PrintContext::spoolPage(GraphicsContext& ctx, int pageNumber, float width)
{
    IntRect pageRect = m_pageRects[pageNumber];
    float scale = width / pageRect.width();

    ctx.save();
    ctx.scale(FloatSize(scale, scale));
    ctx.translate(-pageRect.x(), -pageRect.y());
    ctx.clip(pageRect);
    m_frame->view()->paintContents(&ctx, pageRect);
    ctx.restore();
}

void PrintContext::end()
{
    m_frame->setPrinting(false, 0, 0, true);
}

}

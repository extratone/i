/*
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/) 
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
 *
 */

#ifndef RenderTextControlMultiLine_h
#define RenderTextControlMultiLine_h

#include "RenderTextControl.h"

namespace WebCore {

class RenderTextControlMultiLine : public RenderTextControl {
public:
    RenderTextControlMultiLine(Node*);
    virtual ~RenderTextControlMultiLine();

    virtual bool isTextArea() const { return true; }

    virtual void subtreeHasChanged();
    virtual void layout();

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty, HitTestAction);
    void forwardEvent(Event*);

private:
    virtual int preferredContentWidth(float charWidth) const;
    virtual void adjustControlHeightBasedOnLineHeight(int lineHeight);
    virtual int baselinePosition(bool firstLine, bool isRootLineBox) const;

    virtual void updateFromElement();
    virtual void cacheSelection(int start, int end);

    virtual PassRefPtr<RenderStyle> createInnerTextStyle(const RenderStyle* startStyle) const;
};

}

#endif

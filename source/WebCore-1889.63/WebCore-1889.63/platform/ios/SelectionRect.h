/*
 *  SelectionRect.h
 *  WebCore
 *
 *  Copyright (C) 2009, Apple Inc.  All rights reserved.
 *
 */

#ifndef SelectionRect_h
#define SelectionRect_h

#include "IntRect.h"
#include "TextDirection.h"
#include <wtf/FastAllocBase.h>

namespace WebCore {

class SelectionRect {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit SelectionRect(const IntRect &, bool isHorizontal, int columnNumber);
    SelectionRect(const IntRect &, TextDirection, int, int, int, int, bool, bool, bool, bool, bool, bool, bool, bool, int);
    SelectionRect();
    ~SelectionRect() { }

    IntRect rect() const { return m_rect; }

    int logicalLeft() const { return m_isHorizontal ? m_rect.x() : m_rect.y(); }
    int logicalWidth() const { return m_isHorizontal ?  m_rect.width() : m_rect.height(); }
    int logicalTop() const { return m_isHorizontal ? m_rect.y() : m_rect.x(); }
    int logicalHeight() const { return m_isHorizontal ? m_rect.height() : m_rect.width(); }

    TextDirection direction() const { return m_direction; }
    int minX() const { return m_minX; }
    int maxX() const { return m_maxX; }
    int maxY() const { return m_maxY; }
    int lineNumber() const { return m_lineNumber; }
    bool isLineBreak() const { return m_isLineBreak; }
    bool isFirstOnLine() const { return m_isFirstOnLine; }
    bool isLastOnLine() const { return m_isLastOnLine; }
    bool containsStart() const { return m_containsStart; }
    bool containsEnd() const { return m_containsEnd; }
    bool isHorizontal() const { return m_isHorizontal; }
    bool isInFixedPosition() const { return m_isInFixedPosition; }
    bool isRubyText() const { return m_isRubyText; }
    int columnNumber() const { return m_columnNumber; }

    void setRect(const IntRect &r) { m_rect = r; }

    void setLogicalLeft(int left)
    {
        if (m_isHorizontal)
            m_rect.setX(left);
        else
            m_rect.setY(left);
    }

    void setLogicalWidth(int width)
    {
        if (m_isHorizontal)
            m_rect.setWidth(width);
        else
            m_rect.setHeight(width);
    }

    void setLogicalTop(int top)
    {
        if (m_isHorizontal)
            m_rect.setY(top);
        else
            m_rect.setX(top);
    }

    void setLogicalHeight(int height)
    {
        if (m_isHorizontal)
            m_rect.setHeight(height);
        else
            m_rect.setWidth(height);
    }

    void setDirection(TextDirection d) { m_direction = d; }
    void setMinX(int x) { m_minX = x; }
    void setMaxX(int x) { m_maxX = x; }
    void setMaxY(int y) { m_maxY = y; }
    void setLineNumber(int n) { m_lineNumber = n; }
    void setIsLineBreak(bool b) { m_isLineBreak = b; }
    void setIsFirstOnLine(bool b) { m_isFirstOnLine = b; }
    void setIsLastOnLine(bool b) { m_isLastOnLine = b; }
    void setContainsStart(bool b) { m_containsStart = b; }
    void setContainsEnd(bool b) { m_containsEnd = b; }
    void setIsHorizontal(bool b) { m_isHorizontal = b; }

private:
    IntRect m_rect;
    TextDirection m_direction;
    int m_minX;
    int m_maxX;
    int m_maxY;
    int m_lineNumber;
    bool m_isLineBreak;
    bool m_isFirstOnLine;
    bool m_isLastOnLine;
    bool m_containsStart;
    bool m_containsEnd;
    bool m_isHorizontal;
    bool m_isInFixedPosition;
    bool m_isRubyText;
    int m_columnNumber;
};

}

#endif


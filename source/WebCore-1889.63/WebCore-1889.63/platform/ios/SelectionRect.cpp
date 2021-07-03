/*
 *  SelectionRect.cpp
 *  WebCore
 *
 *  Copyright (C) 2009, Apple Inc.  All rights reserved.
 *
 */

#include "config.h"
#include "SelectionRect.h"

namespace WebCore {

SelectionRect::SelectionRect(const IntRect &rect, bool isHorizontal, int columnNumber)
    : m_rect(rect), 
      m_direction(LTR), 
      m_minX(0),
      m_maxX(0),
      m_maxY(0),
      m_lineNumber(0),
      m_isLineBreak(false), 
      m_isFirstOnLine(false), 
      m_isLastOnLine(false),
      m_containsStart(false), 
      m_containsEnd(false),
      m_isHorizontal(isHorizontal),
      m_isInFixedPosition(false),
      m_isRubyText(false),
      m_columnNumber(columnNumber)
{
}

SelectionRect::SelectionRect(const IntRect &rect, TextDirection direction, int minX, int maxX, int maxY, int lineNumber, bool isLineBreak, bool isFirstOnLine, bool isLastOnLine, bool containsStart, bool containsEnd, bool isHorizontal, bool isInFixedPosition, bool isRubyText, int columnNumber)
    : m_rect(rect), 
      m_direction(direction), 
      m_minX(minX),
      m_maxX(maxX),
      m_maxY(maxY),
      m_lineNumber(lineNumber),
      m_isLineBreak(isLineBreak), 
      m_isFirstOnLine(isFirstOnLine), 
      m_isLastOnLine(isLastOnLine),
      m_containsStart(containsStart), 
      m_containsEnd(containsEnd),
      m_isHorizontal(isHorizontal),
      m_isInFixedPosition(isInFixedPosition),
      m_isRubyText(isRubyText),
      m_columnNumber(columnNumber)
{
}

SelectionRect::SelectionRect()
    : m_direction(LTR),
      m_minX(0),
      m_maxX(0),
      m_maxY(0),
      m_lineNumber(0),
      m_isLineBreak(false),
      m_isFirstOnLine(false),
      m_isLastOnLine(false),
      m_containsStart(false),
      m_containsEnd(false),
      m_isHorizontal(true),
      m_isInFixedPosition(false),
      m_isRubyText(false),
      m_columnNumber(0)
{
}

}

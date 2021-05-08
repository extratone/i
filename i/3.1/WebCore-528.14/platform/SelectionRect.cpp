/*
 *  SelectionRect.cpp
 *  WebCore
 *
 *  Copyright (C) 2009, Apple Inc.  All rights reserved.
 *
 */
 
#include "SelectionRect.h"

namespace WebCore {

SelectionRect::SelectionRect()
    : m_direction(LTR),
      m_minX(0),
      m_maxX(0),
      m_lineNumber(0),
      m_isLineBreak(false), 
      m_isFirstOnLine(false), 
      m_isLastOnLine(false),
      m_containsStart(false), 
      m_containsEnd(false)
{
}

SelectionRect::SelectionRect(const IntRect &rect)
    : m_rect(rect), 
      m_direction(LTR), 
      m_minX(0),
      m_maxX(0),
      m_lineNumber(0),
      m_isLineBreak(false), 
      m_isFirstOnLine(false), 
      m_isLastOnLine(false),
      m_containsStart(false), 
      m_containsEnd(false)
{
}

SelectionRect::SelectionRect(const IntRect &rect, TextDirection direction, int minX, int maxX, int lineNumber, bool isLineBreak, bool isFirstOnLine, bool isLastOnLine, bool containsStart, bool containsEnd)
    : m_rect(rect), 
      m_direction(direction), 
      m_minX(minX),
      m_maxX(maxX),
      m_lineNumber(lineNumber),
      m_isLineBreak(isLineBreak), 
      m_isFirstOnLine(isFirstOnLine), 
      m_isLastOnLine(isLastOnLine),
      m_containsStart(containsStart), 
      m_containsEnd(containsEnd)
{
}

}

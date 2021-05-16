/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2005 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "break_lines.h"
#include "RenderText.h"

#include "DeprecatedString.h"

#if __APPLE__
#import <unicode/ubrk.h>
#import <unicode/utypes.h>
#import "TextBoundaries.h"
#endif

namespace WebCore {

int nextBreakablePosition(const UChar* str, int pos, int len, bool breakNBSP)
{
#if __APPLE__
    static UBreakIterator *breakLocator = 0;
    int findStatus = -1;
    UErrorCode error;
#endif
    int nextUCBreak = -1;
    int i;
    unsigned short ch, lastCh;
    
    lastCh = pos > 0 ? str[pos - 1] : 0;
    for (i = pos; i < len; i++) {
        ch = str[i];
        if (ch == ' ' || ch == '\n' || ch == '\t' || (breakNBSP && ch == 0xa0))
            break;
        // Match WinIE's breaking strategy, which is to always allow breaks after hyphens and question marks.
        if (lastCh == '-' || lastCh == '?' || lastCh == SOFT_HYPHEN) {
            break;
        }
#if __APPLE__
        // FIXME: Rewrite break location using ICU.
        // If current character, or the previous character aren't simple latin1 then
        // use the UC line break locator.  UCFindTextBreak will report false if we
        // have a sequence of 0xa0 0x20 (nbsp, sp), so we explicity check for that
        // case.
        if ((ch > 0x7f && ch != 0xa0) || (lastCh > 0x7f && lastCh != 0xa0)) {
            if (nextUCBreak < i) {
                if (!breakLocator) {
                    error = U_ZERO_ERROR;
                    // FIXME: This gets the locale the first time this code is called,
                    // and will never update if that value ever changes.
                    breakLocator = ubrk_open(UBRK_LINE, currentTextBreakLocaleID(), (const UChar *)str, len, &error);
                    assert(U_SUCCESS(error));
                }
                else {
                    error = U_ZERO_ERROR;
                    ubrk_setText(breakLocator, (const UChar *)str, len, &error);
                    assert(U_SUCCESS(error));
                }
                ubrk_preceding(breakLocator, pos + 1);
                nextUCBreak = ubrk_next(breakLocator);
                findStatus = 0;
            }
            if (findStatus == 0 && i == nextUCBreak && !(lastCh == ' ' || lastCh == '\n' || lastCh == '\t' || (breakNBSP && lastCh == 0xa0)))
                break;
        }
#endif
        lastCh = ch;
    }
    return i;
}

};

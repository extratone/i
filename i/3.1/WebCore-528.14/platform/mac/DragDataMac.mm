/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#import "config.h"
#import "DragData.h"

#import "ClipboardMac.h"
#import "ClipboardAccessPolicy.h"
#import "Document.h"
#import "DocumentFragment.h"
#import "DOMDocumentFragment.h"
#import "DOMDocumentFragmentInternal.h"
#import "MIMETypeRegistry.h"
#import "Pasteboard.h"
#import "PasteboardHelper.h"

namespace WebCore {

DragData::DragData(DragDataRef data, const IntPoint& clientPosition, const IntPoint& globalPosition, 
    DragOperation sourceOperationMask, PasteboardHelper* pasteboardHelper)
    : m_clientPosition(clientPosition)
    , m_globalPosition(globalPosition)
    , m_platformDragData(data)
    , m_draggingSourceOperationMask(sourceOperationMask)
    , m_pasteboardHelper(pasteboardHelper)
{
    ASSERT(pasteboardHelper);  
}
    
bool DragData::canSmartReplace() const
{
    //Need to call this so that the various Pasteboard type strings are intialised
    Pasteboard::generalPasteboard();
    return [[[m_platformDragData draggingPasteboard] types] containsObject:WebSmartPastePboardType];
}

bool DragData::containsColor() const
{
    return [[[m_platformDragData draggingPasteboard] types] containsObject:NSColorPboardType];
}

bool DragData::containsFiles() const
{
    return [[[m_platformDragData draggingPasteboard] types] containsObject:NSFilenamesPboardType];
}

void DragData::asFilenames(Vector<String>& result) const
{
    NSArray *filenames = [[m_platformDragData draggingPasteboard] propertyListForType:NSFilenamesPboardType];
    NSEnumerator *fileEnumerator = [filenames objectEnumerator];
    
    while (NSString *filename = [fileEnumerator nextObject])
        result.append(filename);
}

bool DragData::containsPlainText() const
{
    NSPasteboard *pasteboard = [m_platformDragData draggingPasteboard];
    NSArray *types = [pasteboard types];
    
    return [types containsObject:NSStringPboardType] 
        || [types containsObject:NSRTFDPboardType]
        || [types containsObject:NSRTFPboardType]
        || [types containsObject:NSFilenamesPboardType]
        || [NSURL URLFromPasteboard:pasteboard];
}

String DragData::asPlainText() const
{
    return m_pasteboardHelper->plainTextFromPasteboard([m_platformDragData draggingPasteboard]);
}

Color DragData::asColor() const
{
    NSColor *color = [NSColor colorFromPasteboard:[m_platformDragData draggingPasteboard]];
    return makeRGBA((int)([color redComponent] * 255.0 + 0.5), (int)([color greenComponent] * 255.0 + 0.5), 
                    (int)([color blueComponent] * 255.0 + 0.5), (int)([color alphaComponent] * 255.0 + 0.5));
}

PassRefPtr<Clipboard> DragData::createClipboard(ClipboardAccessPolicy policy) const
{
    return ClipboardMac::create(true, [m_platformDragData draggingPasteboard], policy, 0);
}

bool DragData::containsCompatibleContent() const
{
    NSPasteboard *pasteboard = [m_platformDragData draggingPasteboard];
    NSMutableSet *types = [NSMutableSet setWithArray:[pasteboard types]];
    [types intersectSet:[NSSet setWithArray:m_pasteboardHelper->insertablePasteboardTypes()]];
    return [types count] != 0;
}
    
bool DragData::containsURL() const
{
    return !asURL().isEmpty();
}
    
String DragData::asURL(String* title) const
{
    return m_pasteboardHelper->urlFromPasteboard([m_platformDragData draggingPasteboard], title);
}

PassRefPtr<DocumentFragment> DragData::asFragment(Document*) const
{
    return core(m_pasteboardHelper->fragmentFromPasteboard([m_platformDragData draggingPasteboard]));
}
    
}

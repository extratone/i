/*
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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
#import "ClipboardMac.h"

#import "DOMElementInternal.h"
#import "DragClient.h"
#import "DragController.h"
#import "Editor.h"
#import "FoundationExtras.h"
#import "Frame.h"
#import "Image.h"
#import "Page.h"
#import "Pasteboard.h"
#import "RenderImage.h"
#import "SecurityOrigin.h"
#import "WebCoreSystemInterface.h"

namespace WebCore {

ClipboardMac::ClipboardMac(bool forDragging, NSPasteboard *pasteboard, ClipboardAccessPolicy policy, Frame *frame)
    : Clipboard(policy, forDragging)
    , m_pasteboard(pasteboard)
    , m_frame(frame)
{
    m_changeCount = [m_pasteboard.get() changeCount];
}

ClipboardMac::~ClipboardMac()
{
}

bool ClipboardMac::hasData()
{
    return m_pasteboard && [m_pasteboard.get() types] && [[m_pasteboard.get() types] count] > 0;
}
    
static NSString *cocoaTypeFromMIMEType(const String& type)
{
    String qType = type.stripWhiteSpace();

    // two special cases for IE compatibility
    if (qType == "Text")
        return NSStringPboardType;
    if (qType == "URL")
        return NSURLPboardType;

    // Ignore any trailing charset - JS strings are Unicode, which encapsulates the charset issue
    if (qType == "text/plain" || qType.startsWith("text/plain;"))
        return NSStringPboardType;
    if (qType == "text/uri-list")
        // special case because UTI doesn't work with Cocoa's URL type
        return NSURLPboardType; // note special case in getData to read NSFilenamesType
    
    // Try UTI now
    NSString *mimeType = qType;
    CFStringRef UTIType = UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType, (CFStringRef)mimeType, NULL);
    if (UTIType) {
        CFStringRef pbType = UTTypeCopyPreferredTagWithClass(UTIType, kUTTagClassNSPboardType);
        CFRelease(UTIType);
        if (pbType)
            return HardAutorelease(pbType);
    }

   // No mapping, just pass the whole string though
    return qType;
}

static String MIMETypeFromCocoaType(NSString *type)
{
    // UTI may not do these right, so make sure we get the right, predictable result
    if ([type isEqualToString:NSStringPboardType])
        return "text/plain";
    if ([type isEqualToString:NSURLPboardType] || [type isEqualToString:NSFilenamesPboardType])
        return "text/uri-list";
    
    // Now try the general UTI mechanism
    CFStringRef UTIType = UTTypeCreatePreferredIdentifierForTag(kUTTagClassNSPboardType, (CFStringRef)type, NULL);
    if (UTIType) {
        CFStringRef mimeType = UTTypeCopyPreferredTagWithClass(UTIType, kUTTagClassMIMEType);
        CFRelease(UTIType);
        if (mimeType) {
            String result = mimeType;
            CFRelease(mimeType);
            return result;
        }
    }

    // No mapping, just pass the whole string though
    return type;
}

void ClipboardMac::clearData(const String& type)
{
    if (policy() != ClipboardWritable)
        return;

    // note NSPasteboard enforces changeCount itself on writing - can't write if not the owner

    NSString *cocoaType = cocoaTypeFromMIMEType(type);
    if (cocoaType) {
        [m_pasteboard.get() setString:@"" forType:cocoaType];
    }
}

void ClipboardMac::clearAllData()
{
    if (policy() != ClipboardWritable)
        return;

    // note NSPasteboard enforces changeCount itself on writing - can't write if not the owner

    [m_pasteboard.get() declareTypes:[NSArray array] owner:nil];
}

String ClipboardMac::getData(const String& type, bool& success) const
{
    success = false;
    if (policy() != ClipboardReadable)
        return String();
    
    NSString *cocoaType = cocoaTypeFromMIMEType(type);
    NSString *cocoaValue = nil;
    NSArray *availableTypes = [m_pasteboard.get() types];

    // Fetch the data in different ways for the different Cocoa types

    if ([cocoaType isEqualToString:NSURLPboardType]) {
        // When both URL and filenames are present, filenames is superior since it can contain a list.
        // must check this or we get a printf from CF when there's no data of this type
        if ([availableTypes containsObject:NSFilenamesPboardType]) {
            NSArray *fileList = [m_pasteboard.get() propertyListForType:NSFilenamesPboardType];
            if (fileList && [fileList isKindOfClass:[NSArray class]]) {
                unsigned count = [fileList count];
                if (count > 0) {
                    if (type != "text/uri-list")
                        count = 1;
                    NSMutableString *urls = [NSMutableString string];
                    unsigned i;
                    for (i = 0; i < count; i++) {
                        if (i > 0) {
                            [urls appendString:@"\n"];
                        }
                        NSString *string = [fileList objectAtIndex:i];
                        if (![string isKindOfClass:[NSString class]])
                            break;
                        NSURL *url = [NSURL fileURLWithPath:string];
                        [urls appendString:[url absoluteString]];
                    }
                    if (i == count)
                        cocoaValue = urls;
                }
            }
        }
        if (!cocoaValue) {
            // must check this or we get a printf from CF when there's no data of this type
            if ([availableTypes containsObject:NSURLPboardType]) {
                NSURL *url = [NSURL URLFromPasteboard:m_pasteboard.get()];
                if (url) {
                    cocoaValue = [url absoluteString];
                }
            }
        }
    } else if (cocoaType) {        
        cocoaValue = [m_pasteboard.get() stringForType:cocoaType];
    }

    // Enforce changeCount ourselves for security.  We check after reading instead of before to be
    // sure it doesn't change between our testing the change count and accessing the data.
    if (cocoaValue && m_changeCount == [m_pasteboard.get() changeCount]) {
        success = true;
        return cocoaValue;
    }

    return String();
}

bool ClipboardMac::setData(const String &type, const String &data)
{
    if (policy() != ClipboardWritable)
        return false;
    // note NSPasteboard enforces changeCount itself on writing - can't write if not the owner

    NSString *cocoaType = cocoaTypeFromMIMEType(type);
    NSString *cocoaData = data;

    if ([cocoaType isEqualToString:NSURLPboardType]) {
        [m_pasteboard.get() addTypes:[NSArray arrayWithObject:NSURLPboardType] owner:nil];
        NSURL *url = [[NSURL alloc] initWithString:cocoaData];
        [url writeToPasteboard:m_pasteboard.get()];

        if ([url isFileURL] && m_frame->document()->securityOrigin()->canLoadLocalResources()) {
            [m_pasteboard.get() addTypes:[NSArray arrayWithObject:NSFilenamesPboardType] owner:nil];
            NSArray *fileList = [NSArray arrayWithObject:[url path]];
            [m_pasteboard.get() setPropertyList:fileList forType:NSFilenamesPboardType];
        }

        [url release];
        return true;
    }

    if (cocoaType) {
        // everything else we know of goes on the pboard as a string
        [m_pasteboard.get() addTypes:[NSArray arrayWithObject:cocoaType] owner:nil];
        return [m_pasteboard.get() setString:cocoaData forType:cocoaType];
    }

    return false;
}

HashSet<String> ClipboardMac::types() const
{
    if (policy() != ClipboardReadable && policy() != ClipboardTypesReadable)
        return HashSet<String>();

    NSArray *types = [m_pasteboard.get() types];

    // Enforce changeCount ourselves for security.  We check after reading instead of before to be
    // sure it doesn't change between our testing the change count and accessing the data.
    if (m_changeCount != [m_pasteboard.get() changeCount])
        return HashSet<String>();

    HashSet<String> result;
    if (types) {
        unsigned count = [types count];
        unsigned i;
        for (i = 0; i < count; i++) {
            NSString *pbType = [types objectAtIndex:i];
            if ([pbType isEqualToString:@"NeXT plain ascii pasteboard type"])
                continue;   // skip this ancient type that gets auto-supplied by some system conversion

            String str = MIMETypeFromCocoaType(pbType);
            if (!result.contains(str))
                result.add(str);
        }
    }
    return result;
}

// The rest of these getters don't really have any impact on security, so for now make no checks

void ClipboardMac::setDragImage(CachedImage* img, const IntPoint &loc)
{
    setDragImage(img, 0, loc);
}

void ClipboardMac::setDragImageElement(Node *node, const IntPoint &loc)
{
    setDragImage(0, node, loc);
}

void ClipboardMac::setDragImage(CachedImage* image, Node *node, const IntPoint &loc)
{
    if (policy() == ClipboardImageWritable || policy() == ClipboardWritable) {
        if (m_dragImage)
            m_dragImage->removeClient(this);
        m_dragImage = image;
        if (m_dragImage)
            m_dragImage->addClient(this);

        m_dragLoc = loc;
        m_dragImageElement = node;
        
        if (dragStarted() && m_changeCount == [m_pasteboard.get() changeCount]) {
            NSPoint cocoaLoc;
            NSImage* cocoaImage = dragNSImage(cocoaLoc);
            if (cocoaImage) {
                // Dashboard wants to be able to set the drag image during dragging, but Cocoa does not allow this.
                // Instead we must drop down to the CoreGraphics API.
                wkSetDragImage(cocoaImage, cocoaLoc);

                // Hack: We must post an event to wake up the NSDragManager, which is sitting in a nextEvent call
                // up the stack from us because the CoreFoundation drag manager does not use the run loop by itself.
                // This is the most innocuous event to use, per Kristen Forster.
                NSEvent* ev = [NSEvent mouseEventWithType:NSMouseMoved location:NSZeroPoint
                    modifierFlags:0 timestamp:0 windowNumber:0 context:nil eventNumber:0 clickCount:0 pressure:0];
                [NSApp postEvent:ev atStart:YES];
            }
        }
        // Else either 1) we haven't started dragging yet, so we rely on the part to install this drag image
        // as part of getting the drag kicked off, or 2) Someone kept a ref to the clipboard and is trying to
        // set the image way too late.
    }
}
    
void ClipboardMac::writeRange(Range* range, Frame* frame)
{
    ASSERT(range);
    ASSERT(frame);
    Pasteboard::writeSelection(m_pasteboard.get(), range, frame->editor()->smartInsertDeleteEnabled() && frame->selectionGranularity() == WordGranularity, frame);
}
    
void ClipboardMac::writeURL(const KURL& url, const String& title, Frame* frame)
{   
    ASSERT(frame);
    ASSERT(m_pasteboard);
    Pasteboard::writeURL(m_pasteboard.get(), nil, url, title, frame);
}
    
void ClipboardMac::declareAndWriteDragImage(Element* element, const KURL& url, const String& title, Frame* frame)
{
    ASSERT(frame);
    if (Page* page = frame->page())
        page->dragController()->client()->declareAndWriteDragImage(m_pasteboard.get(), kit(element), url, title, frame);
}
    
DragImageRef ClipboardMac::createDragImage(IntPoint& loc) const
{
    NSPoint nsloc = {loc.x(), loc.y()};
    DragImageRef result = dragNSImage(nsloc);
    loc = (IntPoint)nsloc;
    return result;
}
    
NSImage *ClipboardMac::dragNSImage(NSPoint& loc) const
{
    NSImage *result = nil;
    if (m_dragImageElement) {
        if (m_frame) {
            NSRect imageRect;
            NSRect elementRect;
            result = m_frame->snapshotDragImage(m_dragImageElement.get(), &imageRect, &elementRect);
            // Client specifies point relative to element, not the whole image, which may include child
            // layers spread out all over the place.
            loc.x = elementRect.origin.x - imageRect.origin.x + m_dragLoc.x();
            loc.y = elementRect.origin.y - imageRect.origin.y + m_dragLoc.y();
            loc.y = imageRect.size.height - loc.y;
        }
    } else if (m_dragImage) {
        result = m_dragImage->image()->getNSImage();
        
        loc = m_dragLoc;
        loc.y = [result size].height - loc.y;
    }
    return result;
}

}

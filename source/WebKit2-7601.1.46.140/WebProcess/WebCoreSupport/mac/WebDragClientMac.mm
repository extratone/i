/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WebDragClient.h"

#if ENABLE(DRAG_SUPPORT)

#import "PasteboardTypes.h"
#import "ShareableBitmap.h"
#import "WebCoreArgumentCoders.h"
#import "WebPage.h"
#import "WebPageProxyMessages.h"
#import <WebCore/CachedImage.h>
#import <WebCore/DOMElementInternal.h>
#import <WebCore/DOMPrivate.h>
#import <WebCore/DragController.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameView.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/LegacyWebArchive.h>
#import <WebCore/WebCoreNSURLExtras.h>
#import <WebCore/Page.h>
#import <WebCore/RenderImage.h>
#import <WebCore/ResourceHandle.h>
#import <WebCore/StringTruncator.h>
#import <WebKitSystemInterface.h>
#import <wtf/StdLibExtras.h>

using namespace WebCore;
using namespace WebKit;

namespace WebKit {

static PassRefPtr<ShareableBitmap> convertImageToBitmap(NSImage *image, const IntSize& size)
{
    RefPtr<ShareableBitmap> bitmap = ShareableBitmap::createShareable(size, ShareableBitmap::SupportsAlpha);
    if (!bitmap)
        return nullptr;

    auto graphicsContext = bitmap->createGraphicsContext();

    RetainPtr<NSGraphicsContext> savedContext = [NSGraphicsContext currentContext];

    [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithGraphicsPort:graphicsContext->platformContext() flipped:YES]];
    [image drawInRect:NSMakeRect(0, 0, bitmap->size().width(), bitmap->size().height()) fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1 respectFlipped:YES hints:nil];

    [NSGraphicsContext setCurrentContext:savedContext.get()];

    return bitmap.release();
}

void WebDragClient::startDrag(RetainPtr<NSImage> image, const IntPoint& point, const IntPoint&, DataTransfer&, Frame& frame, bool linkDrag)
{
    IntSize bitmapSize([image size]);
    RefPtr<ShareableBitmap> bitmap = convertImageToBitmap(image.get(), bitmapSize);
    ShareableBitmap::Handle handle;
    if (!bitmap || !bitmap->createHandle(handle))
        return;

    m_page->willStartDrag();

    // FIXME: Seems this message should be named StartDrag, not SetDragImage.
    m_page->send(Messages::WebPageProxy::SetDragImage(frame.view()->contentsToWindow(point), handle, linkDrag));
}

static WebCore::CachedImage* cachedImage(Element& element)
{
    auto* renderer = element.renderer();
    if (!is<WebCore::RenderImage>(renderer))
        return nullptr;
    WebCore::CachedImage* image = downcast<WebCore::RenderImage>(*renderer).cachedImage();
    if (!image || image->errorOccurred()) 
        return nullptr;
    return image;
}

#if ENABLE(ATTACHMENT_ELEMENT)
void WebDragClient::declareAndWriteAttachment(const String& pasteboardName, Element& element, const URL& url, const String& path, WebCore::Frame* frame)
{
    ASSERT(pasteboardName == String(NSDragPboard));
    
    NSURL* nsURL = (NSURL *)url;
    m_page->send(Messages::WebPageProxy::SetPromisedDataForAttachment(pasteboardName, String(nsURL.lastPathComponent), String(nsURL.pathExtension), path, String(nsURL.absoluteString), userVisibleString(nsURL)));
}
#endif

void WebDragClient::declareAndWriteDragImage(const String& pasteboardName, Element& element, const URL& url, const String& label, Frame*)
{
    ASSERT(pasteboardName == String(NSDragPboard));

    WebCore::CachedImage* image = cachedImage(element);

    String extension;
    if (image) {
        extension = image->image()->filenameExtension();
        if (extension.isEmpty())
            return;
    }

    String title = label;
    if (title.isEmpty()) {
        title = url.lastPathComponent();
        if (title.isEmpty())
            title = userVisibleString((NSURL *)url);
    }

    RefPtr<LegacyWebArchive> archive = LegacyWebArchive::create(&element);

    NSURLResponse *response = image->response().nsURLResponse();
    
    RefPtr<SharedBuffer> imageBuffer = image->image()->data();
    size_t imageSize = imageBuffer->size();
    SharedMemory::Handle imageHandle;
    
    RefPtr<SharedMemory> sharedMemoryBuffer = SharedMemory::allocate(imageBuffer->size());
    if (!sharedMemoryBuffer)
        return;
    memcpy(sharedMemoryBuffer->data(), imageBuffer->data(), imageSize);
    sharedMemoryBuffer->createHandle(imageHandle, SharedMemory::Protection::ReadOnly);
    
    RetainPtr<CFDataRef> data = archive ? archive->rawDataRepresentation() : 0;
    SharedMemory::Handle archiveHandle;
    size_t archiveSize = 0;
    if (data) {
        RefPtr<SharedBuffer> archiveBuffer = SharedBuffer::wrapNSData((NSData *)data.get());
        RefPtr<SharedMemory> archiveSharedMemoryBuffer = SharedMemory::allocate(archiveBuffer->size());
        if (!archiveSharedMemoryBuffer)
            return;
        archiveSize = archiveBuffer->size();
        memcpy(archiveSharedMemoryBuffer->data(), archiveBuffer->data(), archiveSize);
        archiveSharedMemoryBuffer->createHandle(archiveHandle, SharedMemory::Protection::ReadOnly);
    }
    m_page->send(Messages::WebPageProxy::SetPromisedDataForImage(pasteboardName, imageHandle, imageSize, String([response suggestedFilename]), extension, title, String([[response URL] absoluteString]), userVisibleString((NSURL *)url), archiveHandle, archiveSize));
}

} // namespace WebKit

#endif // ENABLE(DRAG_SUPPORT)

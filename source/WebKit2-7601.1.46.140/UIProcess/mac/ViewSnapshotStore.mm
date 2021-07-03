/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "ViewSnapshotStore.h"

#import "WebBackForwardList.h"
#import "WebPageProxy.h"
#import <CoreGraphics/CoreGraphics.h>
#import <WebCore/IOSurface.h>

#if PLATFORM(IOS)
#import <WebCore/QuartzCoreSPI.h>
#endif

using namespace WebCore;

#if PLATFORM(IOS)
static const size_t maximumSnapshotCacheSize = 50 * (1024 * 1024);
#else
static const size_t maximumSnapshotCacheSize = 400 * (1024 * 1024);
#endif

namespace WebKit {

ViewSnapshotStore::ViewSnapshotStore()
    : m_snapshotCacheSize(0)
{
}

ViewSnapshotStore::~ViewSnapshotStore()
{
    discardSnapshotImages();
}

ViewSnapshotStore& ViewSnapshotStore::singleton()
{
    static ViewSnapshotStore& store = *new ViewSnapshotStore;
    return store;
}

#if !USE(IOSURFACE)
CAContext *ViewSnapshotStore::snapshottingContext()
{
    static CAContext *context;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSDictionary *options = @{
            kCAContextDisplayName: @"WebKitSnapshotting",
            kCAContextIgnoresHitTest: @YES,
            kCAContextDisplayId : @20000
        };
        context = [[CAContext remoteContextWithOptions:options] retain];
    });

    return context;
}
#endif

void ViewSnapshotStore::didAddImageToSnapshot(ViewSnapshot& snapshot)
{
    bool isNewEntry = m_snapshotsWithImages.add(&snapshot).isNewEntry;
    ASSERT_UNUSED(isNewEntry, isNewEntry);
    m_snapshotCacheSize += snapshot.imageSizeInBytes();
}

void ViewSnapshotStore::willRemoveImageFromSnapshot(ViewSnapshot& snapshot)
{
    bool removed = m_snapshotsWithImages.remove(&snapshot);
    ASSERT_UNUSED(removed, removed);
    m_snapshotCacheSize -= snapshot.imageSizeInBytes();
}

void ViewSnapshotStore::pruneSnapshots(WebPageProxy& webPageProxy)
{
    if (m_snapshotCacheSize <= maximumSnapshotCacheSize)
        return;

    ASSERT(!m_snapshotsWithImages.isEmpty());

    // FIXME: We have enough information to do smarter-than-LRU eviction (making use of the back-forward lists, etc.)

    m_snapshotsWithImages.first()->clearImage();
}

void ViewSnapshotStore::recordSnapshot(WebPageProxy& webPageProxy, WebBackForwardListItem& item)
{
    if (webPageProxy.isShowingNavigationGestureSnapshot())
        return;

    pruneSnapshots(webPageProxy);

    webPageProxy.willRecordNavigationSnapshot(item);

    RefPtr<ViewSnapshot> snapshot = webPageProxy.takeViewSnapshot();
    if (!snapshot)
        return;

    snapshot->setRenderTreeSize(webPageProxy.renderTreeSize());
    snapshot->setDeviceScaleFactor(webPageProxy.deviceScaleFactor());
    snapshot->setBackgroundColor(webPageProxy.pageExtendedBackgroundColor());

    item.setSnapshot(snapshot.release());
}

void ViewSnapshotStore::discardSnapshotImages()
{
    while (!m_snapshotsWithImages.isEmpty())
        m_snapshotsWithImages.first()->clearImage();
}

#if USE(IOSURFACE)
Ref<ViewSnapshot> ViewSnapshot::create(std::unique_ptr<IOSurface> surface)
{
    return adoptRef(*new ViewSnapshot(WTF::move(surface)));
}
#else
Ref<ViewSnapshot> ViewSnapshot::create(uint32_t slotID, IntSize size, size_t imageSizeInBytes)
{
    return adoptRef(*new ViewSnapshot(slotID, size, imageSizeInBytes));
}
#endif

#if USE(IOSURFACE)
ViewSnapshot::ViewSnapshot(std::unique_ptr<IOSurface> surface)
    : m_surface(WTF::move(surface))
#else
ViewSnapshot::ViewSnapshot(uint32_t slotID, IntSize size, size_t imageSizeInBytes)
    : m_slotID(slotID)
    , m_imageSizeInBytes(imageSizeInBytes)
    , m_size(size)
#endif
{
    if (hasImage())
        ViewSnapshotStore::singleton().didAddImageToSnapshot(*this);
}

ViewSnapshot::~ViewSnapshot()
{
    clearImage();
}

#if USE(IOSURFACE)
void ViewSnapshot::setSurface(std::unique_ptr<WebCore::IOSurface> surface)
{
    ASSERT(!m_surface);
    if (!surface) {
        clearImage();
        return;
    }

    m_surface = WTF::move(surface);
    ViewSnapshotStore::singleton().didAddImageToSnapshot(*this);
}
#endif

bool ViewSnapshot::hasImage() const
{
#if USE(IOSURFACE)
    return !!m_surface;
#else
    return m_slotID;
#endif
}

void ViewSnapshot::clearImage()
{
    if (!hasImage())
        return;

    ViewSnapshotStore::singleton().willRemoveImageFromSnapshot(*this);

#if USE(IOSURFACE)
    m_surface = nullptr;
#else
    [ViewSnapshotStore::snapshottingContext() deleteSlot:m_slotID];
    m_slotID = 0;
    m_imageSizeInBytes = 0;
#endif
}

id ViewSnapshot::asLayerContents()
{
#if USE(IOSURFACE)
    if (!m_surface)
        return nullptr;

    if (m_surface->setIsVolatile(false) != IOSurface::SurfaceState::Valid) {
        clearImage();
        return nullptr;
    }

    return (id)m_surface->surface();
#else
    return [CAContext objectForSlot:m_slotID];
#endif
}

} // namespace WebKit

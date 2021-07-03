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

#ifndef ViewSnapshotStore_h
#define ViewSnapshotStore_h

#include <WebCore/Color.h>
#include <WebCore/IntSize.h>
#include <WebCore/IOSurface.h>
#include <wtf/ListHashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/WTFString.h>

#if !defined(__OBJC__)
typedef struct objc_object *id;
#endif

OBJC_CLASS CAContext;

namespace WebCore {
class IOSurface;
}

namespace WebKit {

class ViewSnapshotStore;
class WebBackForwardListItem;
class WebPageProxy;

class ViewSnapshot : public RefCounted<ViewSnapshot> {
public:
#if USE(IOSURFACE)
    static Ref<ViewSnapshot> create(std::unique_ptr<WebCore::IOSurface>);
#else
    static Ref<ViewSnapshot> create(uint32_t slotID, WebCore::IntSize, size_t imageSizeInBytes);
#endif

    ~ViewSnapshot();

    void clearImage();
    bool hasImage() const;
    id asLayerContents();

    void setRenderTreeSize(uint64_t renderTreeSize) { m_renderTreeSize = renderTreeSize; }
    uint64_t renderTreeSize() const { return m_renderTreeSize; }

    void setBackgroundColor(WebCore::Color color) { m_backgroundColor = color; }
    WebCore::Color backgroundColor() const { return m_backgroundColor; }

    void setDeviceScaleFactor(float deviceScaleFactor) { m_deviceScaleFactor = deviceScaleFactor; }
    float deviceScaleFactor() const { return m_deviceScaleFactor; }

#if USE(IOSURFACE)
    WebCore::IOSurface* surface() const { return m_surface.get(); }

    size_t imageSizeInBytes() const { return m_surface ? m_surface->totalBytes() : 0; }
    WebCore::IntSize size() const { return m_surface ? m_surface->size() : WebCore::IntSize(); }

    void setSurface(std::unique_ptr<WebCore::IOSurface>);
#else
    WebCore::IntSize size() const { return m_size; }
    size_t imageSizeInBytes() const { return m_imageSizeInBytes; }
#endif

private:
#if USE(IOSURFACE)
    explicit ViewSnapshot(std::unique_ptr<WebCore::IOSurface>);

    std::unique_ptr<WebCore::IOSurface> m_surface;
#else
    explicit ViewSnapshot(uint32_t slotID, WebCore::IntSize, size_t imageSizeInBytes);

    uint32_t m_slotID;
    size_t m_imageSizeInBytes;
    WebCore::IntSize m_size;
#endif

    uint64_t m_renderTreeSize;
    float m_deviceScaleFactor;
    WebCore::Color m_backgroundColor;
};

class ViewSnapshotStore {
    WTF_MAKE_NONCOPYABLE(ViewSnapshotStore);
    friend class ViewSnapshot;
public:
    ViewSnapshotStore();
    ~ViewSnapshotStore();

    static ViewSnapshotStore& singleton();

    void recordSnapshot(WebPageProxy&, WebBackForwardListItem&);

    void discardSnapshotImages();

#if !USE(IOSURFACE)
    static CAContext *snapshottingContext();
#endif

private:
    void didAddImageToSnapshot(ViewSnapshot&);
    void willRemoveImageFromSnapshot(ViewSnapshot&);
    void pruneSnapshots(WebPageProxy&);

    size_t m_snapshotCacheSize;

    ListHashSet<ViewSnapshot*> m_snapshotsWithImages;
};

} // namespace WebKit

#endif // ViewSnapshotStore_h

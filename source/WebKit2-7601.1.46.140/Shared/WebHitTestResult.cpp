/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
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
 */

#include "config.h"
#include "WebHitTestResult.h"

#include "WebCoreArgumentCoders.h"
#include <WebCore/Document.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameView.h>
#include <WebCore/HitTestResult.h>
#include <WebCore/RenderObject.h>
#include <WebCore/URL.h>
#include <WebCore/Node.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;

namespace WebKit {

PassRefPtr<WebHitTestResult> WebHitTestResult::create(const WebHitTestResult::Data& hitTestResultData)
{
    return adoptRef(new WebHitTestResult(hitTestResultData));
}

WebHitTestResult::Data::Data()
{
}

WebHitTestResult::Data::Data(const HitTestResult& hitTestResult)
    : absoluteImageURL(hitTestResult.absoluteImageURL().string())
    , absolutePDFURL(hitTestResult.absolutePDFURL().string())
    , absoluteLinkURL(hitTestResult.absoluteLinkURL().string())
    , absoluteMediaURL(hitTestResult.absoluteMediaURL().string())
    , linkLabel(hitTestResult.textContent())
    , linkTitle(hitTestResult.titleDisplayString())
    , isContentEditable(hitTestResult.isContentEditable())
    , elementBoundingBox(elementBoundingBoxInWindowCoordinates(hitTestResult))
    , isScrollbar(hitTestResult.scrollbar())
    , isSelected(hitTestResult.isSelected())
    , isTextNode(hitTestResult.innerNode() && hitTestResult.innerNode()->isTextNode())
    , isOverTextInsideFormControlElement(hitTestResult.isOverTextInsideFormControlElement())
    , allowsCopy(hitTestResult.allowsCopy())
    , isDownloadableMedia(hitTestResult.isDownloadableMedia())
    , imageSize(0)
{
}

WebHitTestResult::Data::Data(const WebCore::HitTestResult& hitTestResult, bool includeImage)
    : absoluteImageURL(hitTestResult.absoluteImageURL().string())
    , absolutePDFURL(hitTestResult.absolutePDFURL().string())
    , absoluteLinkURL(hitTestResult.absoluteLinkURL().string())
    , absoluteMediaURL(hitTestResult.absoluteMediaURL().string())
    , linkLabel(hitTestResult.textContent())
    , linkTitle(hitTestResult.titleDisplayString())
    , isContentEditable(hitTestResult.isContentEditable())
    , elementBoundingBox(elementBoundingBoxInWindowCoordinates(hitTestResult))
    , isScrollbar(hitTestResult.scrollbar())
    , isSelected(hitTestResult.isSelected())
    , isTextNode(hitTestResult.innerNode() && hitTestResult.innerNode()->isTextNode())
    , isOverTextInsideFormControlElement(hitTestResult.isOverTextInsideFormControlElement())
    , allowsCopy(hitTestResult.allowsCopy())
    , isDownloadableMedia(hitTestResult.isDownloadableMedia())
    , imageSize(0)
{
    if (!includeImage)
        return;

    if (Image* image = hitTestResult.image()) {
        RefPtr<SharedBuffer> buffer = image->data();
        if (buffer) {
            imageSharedMemory = SharedMemory::allocate(buffer->size());
            memcpy(imageSharedMemory->data(), buffer->data(), buffer->size());
            imageSize = buffer->size();
        }
    }
}

WebHitTestResult::Data::~Data()
{
}

void WebHitTestResult::Data::encode(IPC::ArgumentEncoder& encoder) const
{
    encoder << absoluteImageURL;
    encoder << absolutePDFURL;
    encoder << absoluteLinkURL;
    encoder << absoluteMediaURL;
    encoder << linkLabel;
    encoder << linkTitle;
    encoder << isContentEditable;
    encoder << elementBoundingBox;
    encoder << isScrollbar;
    encoder << isSelected;
    encoder << isTextNode;
    encoder << isOverTextInsideFormControlElement;
    encoder << allowsCopy;
    encoder << isDownloadableMedia;
    encoder << lookupText;
    encoder << dictionaryPopupInfo;

    SharedMemory::Handle imageHandle;
    if (imageSharedMemory && imageSharedMemory->data())
        imageSharedMemory->createHandle(imageHandle, SharedMemory::Protection::ReadOnly);
    encoder << imageHandle;
    encoder << imageSize;

    bool hasLinkTextIndicator = linkTextIndicator;
    encoder << hasLinkTextIndicator;
    if (hasLinkTextIndicator)
        encoder << linkTextIndicator->data();

    platformEncode(encoder);
}

bool WebHitTestResult::Data::decode(IPC::ArgumentDecoder& decoder, WebHitTestResult::Data& hitTestResultData)
{
    if (!decoder.decode(hitTestResultData.absoluteImageURL)
        || !decoder.decode(hitTestResultData.absolutePDFURL)
        || !decoder.decode(hitTestResultData.absoluteLinkURL)
        || !decoder.decode(hitTestResultData.absoluteMediaURL)
        || !decoder.decode(hitTestResultData.linkLabel)
        || !decoder.decode(hitTestResultData.linkTitle)
        || !decoder.decode(hitTestResultData.isContentEditable)
        || !decoder.decode(hitTestResultData.elementBoundingBox)
        || !decoder.decode(hitTestResultData.isScrollbar)
        || !decoder.decode(hitTestResultData.isSelected)
        || !decoder.decode(hitTestResultData.isTextNode)
        || !decoder.decode(hitTestResultData.isOverTextInsideFormControlElement)
        || !decoder.decode(hitTestResultData.allowsCopy)
        || !decoder.decode(hitTestResultData.isDownloadableMedia)
        || !decoder.decode(hitTestResultData.lookupText)
        || !decoder.decode(hitTestResultData.dictionaryPopupInfo))
        return false;

    SharedMemory::Handle imageHandle;
    if (!decoder.decode(imageHandle))
        return false;

    if (!imageHandle.isNull())
        hitTestResultData.imageSharedMemory = SharedMemory::map(imageHandle, SharedMemory::Protection::ReadOnly);

    if (!decoder.decode(hitTestResultData.imageSize))
        return false;

    bool hasLinkTextIndicator;
    if (!decoder.decode(hasLinkTextIndicator))
        return false;

    if (hasLinkTextIndicator) {
        WebCore::TextIndicatorData indicatorData;
        if (!decoder.decode(indicatorData))
            return false;

        hitTestResultData.linkTextIndicator = WebCore::TextIndicator::create(indicatorData);
    }

    return platformDecode(decoder, hitTestResultData);
}

#if !PLATFORM(MAC)
void WebHitTestResult::Data::platformEncode(IPC::ArgumentEncoder& encoder) const
{
}

bool WebHitTestResult::Data::platformDecode(IPC::ArgumentDecoder& decoder, WebHitTestResult::Data& hitTestResultData)
{
    return true;
}
#endif // !PLATFORM(MAC)

IntRect WebHitTestResult::Data::elementBoundingBoxInWindowCoordinates(const HitTestResult& hitTestResult)
{
    Node* node = hitTestResult.innerNonSharedNode();
    if (!node)
        return IntRect();

    Frame* frame = node->document().frame();
    if (!frame)
        return IntRect();

    FrameView* view = frame->view();
    if (!view)
        return IntRect();

    RenderObject* renderer = node->renderer();
    if (!renderer)
        return IntRect();

    return view->contentsToWindow(renderer->absoluteBoundingBoxRect());
}

} // WebKit

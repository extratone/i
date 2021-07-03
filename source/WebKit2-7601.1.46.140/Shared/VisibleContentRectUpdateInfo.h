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

#ifndef VisibleContentRectUpdateInfo_h
#define VisibleContentRectUpdateInfo_h

#include <WebCore/FloatRect.h>

namespace IPC {
class ArgumentDecoder;
class ArgumentEncoder;
}

namespace WebKit {

class VisibleContentRectUpdateInfo {
public:
    VisibleContentRectUpdateInfo() = default;

    VisibleContentRectUpdateInfo(const WebCore::FloatRect& exposedRect, const WebCore::FloatRect& unobscuredRect, const WebCore::FloatRect& unobscuredRectInScrollViewCoordinates, const WebCore::FloatRect& customFixedPositionRect, double scale, bool inStableState, bool isChangingObscuredInsetsInteractively, bool allowShrinkToFit, double timestamp, double horizontalVelocity, double verticalVelocity, double scaleChangeRate, uint64_t lastLayerTreeTransactionId)
        : m_exposedRect(exposedRect)
        , m_unobscuredRect(unobscuredRect)
        , m_unobscuredRectInScrollViewCoordinates(unobscuredRectInScrollViewCoordinates)
        , m_customFixedPositionRect(customFixedPositionRect)
        , m_lastLayerTreeTransactionID(lastLayerTreeTransactionId)
        , m_scale(scale)
        , m_timestamp(timestamp)
        , m_horizontalVelocity(horizontalVelocity)
        , m_verticalVelocity(verticalVelocity)
        , m_scaleChangeRate(scaleChangeRate)
        , m_inStableState(inStableState)
        , m_isChangingObscuredInsetsInteractively(isChangingObscuredInsetsInteractively)
        , m_allowShrinkToFit(allowShrinkToFit)
    {
    }

    const WebCore::FloatRect& exposedRect() const { return m_exposedRect; }
    const WebCore::FloatRect& unobscuredRect() const { return m_unobscuredRect; }
    const WebCore::FloatRect& unobscuredRectInScrollViewCoordinates() const { return m_unobscuredRectInScrollViewCoordinates; }
    const WebCore::FloatRect& customFixedPositionRect() const { return m_customFixedPositionRect; }
    double scale() const { return m_scale; }
    bool inStableState() const { return m_inStableState; }
    bool isChangingObscuredInsetsInteractively() const { return m_isChangingObscuredInsetsInteractively; }
    bool allowShrinkToFit() const { return m_allowShrinkToFit; }

    double timestamp() const { return m_timestamp; }
    double horizontalVelocity() const { return m_horizontalVelocity; }
    double verticalVelocity() const { return m_verticalVelocity; }
    double scaleChangeRate() const { return m_scaleChangeRate; }

    uint64_t lastLayerTreeTransactionID() const { return m_lastLayerTreeTransactionID; }

    void encode(IPC::ArgumentEncoder&) const;
    static bool decode(IPC::ArgumentDecoder&, VisibleContentRectUpdateInfo&);

private:
    WebCore::FloatRect m_exposedRect;
    WebCore::FloatRect m_unobscuredRect;
    WebCore::FloatRect m_unobscuredRectInScrollViewCoordinates;
    WebCore::FloatRect m_customFixedPositionRect;
    uint64_t m_lastLayerTreeTransactionID { 0 };
    double m_scale { -1 };
    double m_timestamp { 0 };
    double m_horizontalVelocity { 0 };
    double m_verticalVelocity { 0 };
    double m_scaleChangeRate { 0 };
    bool m_inStableState { false };
    bool m_isChangingObscuredInsetsInteractively { false };
    bool m_allowShrinkToFit { false };
};

inline bool operator==(const VisibleContentRectUpdateInfo& a, const VisibleContentRectUpdateInfo& b)
{
    // Note: the comparison doesn't include timestamp and velocity since we care about equality based on the other data.
    return a.scale() == b.scale()
        && a.exposedRect() == b.exposedRect()
        && a.unobscuredRect() == b.unobscuredRect()
        && a.customFixedPositionRect() == b.customFixedPositionRect()
        && a.horizontalVelocity() == b.horizontalVelocity()
        && a.verticalVelocity() == b.verticalVelocity()
        && a.scaleChangeRate() == b.scaleChangeRate()
        && a.inStableState() == b.inStableState()
        && a.allowShrinkToFit() == b.allowShrinkToFit();
}

} // namespace WebKit

#endif // VisibleContentRectUpdateInfo_h

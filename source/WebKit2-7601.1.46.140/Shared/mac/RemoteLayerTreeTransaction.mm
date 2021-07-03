/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#import "RemoteLayerTreeTransaction.h"

#import "ArgumentCoders.h"
#import "MessageDecoder.h"
#import "MessageEncoder.h"
#import "PlatformCAAnimationRemote.h"
#import "PlatformCALayerRemote.h"
#import "WebCoreArgumentCoders.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/LengthFunctions.h>
#import <WebCore/TextStream.h>
#import <WebCore/TimingFunction.h>
#import <wtf/text/CString.h>
#import <wtf/text/StringBuilder.h>

using namespace WebCore;

namespace WebKit {

RemoteLayerTreeTransaction::LayerCreationProperties::LayerCreationProperties()
    : layerID(0)
    , type(PlatformCALayer::LayerTypeLayer)
    , hostingContextID(0)
    , hostingDeviceScaleFactor(1)
{
}

void RemoteLayerTreeTransaction::LayerCreationProperties::encode(IPC::ArgumentEncoder& encoder) const
{
    encoder << layerID;
    encoder.encodeEnum(type);
    encoder << hostingContextID;
    encoder << hostingDeviceScaleFactor;
}

bool RemoteLayerTreeTransaction::LayerCreationProperties::decode(IPC::ArgumentDecoder& decoder, LayerCreationProperties& result)
{
    if (!decoder.decode(result.layerID))
        return false;

    if (!decoder.decodeEnum(result.type))
        return false;

    if (!decoder.decode(result.hostingContextID))
        return false;

    if (!decoder.decode(result.hostingDeviceScaleFactor))
        return false;

    return true;
}

RemoteLayerTreeTransaction::LayerProperties::LayerProperties()
    : changedProperties(NoChange)
    , everChangedProperties(NoChange)
    , anchorPoint(0.5, 0.5, 0)
    , contentsRect(FloatPoint(), FloatSize(1, 1))
    , maskLayerID(0)
    , clonedLayerID(0)
    , timeOffset(0)
    , speed(1)
    , contentsScale(1)
    , cornerRadius(0)
    , borderWidth(0)
    , opacity(1)
    , backgroundColor(Color::transparent)
    , borderColor(Color::black)
    , edgeAntialiasingMask(kCALayerLeftEdge | kCALayerRightEdge | kCALayerBottomEdge | kCALayerTopEdge)
    , customAppearance(GraphicsLayer::NoCustomAppearance)
    , minificationFilter(PlatformCALayer::FilterType::Linear)
    , magnificationFilter(PlatformCALayer::FilterType::Linear)
    , blendMode(BlendModeNormal)
    , windRule(RULE_NONZERO)
    , hidden(false)
    , backingStoreAttached(true)
    , geometryFlipped(false)
    , doubleSided(true)
    , masksToBounds(false)
    , opaque(false)
{
}

RemoteLayerTreeTransaction::LayerProperties::LayerProperties(const LayerProperties& other)
    : changedProperties(other.changedProperties)
    , everChangedProperties(other.everChangedProperties)
    , name(other.name)
    , children(other.children)
    , addedAnimations(other.addedAnimations)
    , keyPathsOfAnimationsToRemove(other.keyPathsOfAnimationsToRemove)
    , position(other.position)
    , anchorPoint(other.anchorPoint)
    , bounds(other.bounds)
    , contentsRect(other.contentsRect)
    , shapePath(other.shapePath)
    , maskLayerID(other.maskLayerID)
    , clonedLayerID(other.clonedLayerID)
    , timeOffset(other.timeOffset)
    , speed(other.speed)
    , contentsScale(other.contentsScale)
    , cornerRadius(other.cornerRadius)
    , borderWidth(other.borderWidth)
    , opacity(other.opacity)
    , backgroundColor(other.backgroundColor)
    , borderColor(other.borderColor)
    , edgeAntialiasingMask(other.edgeAntialiasingMask)
    , customAppearance(other.customAppearance)
    , minificationFilter(other.minificationFilter)
    , magnificationFilter(other.magnificationFilter)
    , blendMode(other.blendMode)
    , windRule(other.windRule)
    , hidden(other.hidden)
    , backingStoreAttached(other.backingStoreAttached)
    , geometryFlipped(other.geometryFlipped)
    , doubleSided(other.doubleSided)
    , masksToBounds(other.masksToBounds)
    , opaque(other.opaque)
{
    // FIXME: LayerProperties should reference backing store by ID, so that two layers can have the same backing store (for clones).
    // FIXME: LayerProperties shouldn't be copyable; PlatformCALayerRemote::clone should copy the relevant properties.

    if (other.transform)
        transform = std::make_unique<TransformationMatrix>(*other.transform);

    if (other.sublayerTransform)
        sublayerTransform = std::make_unique<TransformationMatrix>(*other.sublayerTransform);

    if (other.filters)
        filters = std::make_unique<FilterOperations>(*other.filters);
}

void RemoteLayerTreeTransaction::LayerProperties::encode(IPC::ArgumentEncoder& encoder) const
{
    encoder.encodeEnum(changedProperties);

    if (changedProperties & NameChanged)
        encoder << name;

    if (changedProperties & ChildrenChanged)
        encoder << children;

    if (changedProperties & AnimationsChanged) {
        encoder << addedAnimations;
        encoder << keyPathsOfAnimationsToRemove;
    }

    if (changedProperties & PositionChanged)
        encoder << position;

    if (changedProperties & BoundsChanged)
        encoder << bounds;

    if (changedProperties & BackgroundColorChanged)
        encoder << backgroundColor;

    if (changedProperties & AnchorPointChanged)
        encoder << anchorPoint;

    if (changedProperties & BorderWidthChanged)
        encoder << borderWidth;

    if (changedProperties & BorderColorChanged)
        encoder << borderColor;

    if (changedProperties & OpacityChanged)
        encoder << opacity;

    if (changedProperties & TransformChanged)
        encoder << *transform;

    if (changedProperties & SublayerTransformChanged)
        encoder << *sublayerTransform;

    if (changedProperties & HiddenChanged)
        encoder << hidden;

    if (changedProperties & GeometryFlippedChanged)
        encoder << geometryFlipped;

    if (changedProperties & DoubleSidedChanged)
        encoder << doubleSided;

    if (changedProperties & MasksToBoundsChanged)
        encoder << masksToBounds;

    if (changedProperties & OpaqueChanged)
        encoder << opaque;

    if (changedProperties & MaskLayerChanged)
        encoder << maskLayerID;

    if (changedProperties & ClonedContentsChanged)
        encoder << clonedLayerID;

    if (changedProperties & ContentsRectChanged)
        encoder << contentsRect;

    if (changedProperties & ContentsScaleChanged)
        encoder << contentsScale;

    if (changedProperties & CornerRadiusChanged)
        encoder << cornerRadius;

    if (changedProperties & ShapeRoundedRectChanged)
        encoder << *shapeRoundedRect;

    if (changedProperties & ShapePathChanged)
        encoder << shapePath;

    if (changedProperties & MinificationFilterChanged)
        encoder.encodeEnum(minificationFilter);

    if (changedProperties & MagnificationFilterChanged)
        encoder.encodeEnum(magnificationFilter);

    if (changedProperties & BlendModeChanged)
        encoder.encodeEnum(blendMode);

    if (changedProperties & WindRuleChanged)
        encoder.encodeEnum(windRule);

    if (changedProperties & SpeedChanged)
        encoder << speed;

    if (changedProperties & TimeOffsetChanged)
        encoder << timeOffset;

    if (changedProperties & BackingStoreChanged) {
        bool hasFrontBuffer = backingStore && backingStore->hasFrontBuffer();
        encoder << hasFrontBuffer;
        if (hasFrontBuffer)
            encoder << *backingStore;
    }

    if (changedProperties & BackingStoreAttachmentChanged)
        encoder << backingStoreAttached;

    if (changedProperties & FiltersChanged)
        encoder << *filters;

    if (changedProperties & EdgeAntialiasingMaskChanged)
        encoder << edgeAntialiasingMask;

    if (changedProperties & CustomAppearanceChanged)
        encoder.encodeEnum(customAppearance);
}

bool RemoteLayerTreeTransaction::LayerProperties::decode(IPC::ArgumentDecoder& decoder, LayerProperties& result)
{
    if (!decoder.decodeEnum(result.changedProperties))
        return false;

    if (result.changedProperties & NameChanged) {
        if (!decoder.decode(result.name))
            return false;
    }

    if (result.changedProperties & ChildrenChanged) {
        if (!decoder.decode(result.children))
            return false;

        for (auto& layerID : result.children) {
            if (!layerID)
                return false;
        }
    }

    if (result.changedProperties & AnimationsChanged) {
        if (!decoder.decode(result.addedAnimations))
            return false;

        if (!decoder.decode(result.keyPathsOfAnimationsToRemove))
            return false;
    }

    if (result.changedProperties & PositionChanged) {
        if (!decoder.decode(result.position))
            return false;
    }

    if (result.changedProperties & BoundsChanged) {
        if (!decoder.decode(result.bounds))
            return false;
    }

    if (result.changedProperties & BackgroundColorChanged) {
        if (!decoder.decode(result.backgroundColor))
            return false;
    }

    if (result.changedProperties & AnchorPointChanged) {
        if (!decoder.decode(result.anchorPoint))
            return false;
    }

    if (result.changedProperties & BorderWidthChanged) {
        if (!decoder.decode(result.borderWidth))
            return false;
    }

    if (result.changedProperties & BorderColorChanged) {
        if (!decoder.decode(result.borderColor))
            return false;
    }

    if (result.changedProperties & OpacityChanged) {
        if (!decoder.decode(result.opacity))
            return false;
    }

    if (result.changedProperties & TransformChanged) {
        TransformationMatrix transform;
        if (!decoder.decode(transform))
            return false;
        
        result.transform = std::make_unique<TransformationMatrix>(transform);
    }

    if (result.changedProperties & SublayerTransformChanged) {
        TransformationMatrix transform;
        if (!decoder.decode(transform))
            return false;

        result.sublayerTransform = std::make_unique<TransformationMatrix>(transform);
    }

    if (result.changedProperties & HiddenChanged) {
        if (!decoder.decode(result.hidden))
            return false;
    }

    if (result.changedProperties & GeometryFlippedChanged) {
        if (!decoder.decode(result.geometryFlipped))
            return false;
    }

    if (result.changedProperties & DoubleSidedChanged) {
        if (!decoder.decode(result.doubleSided))
            return false;
    }

    if (result.changedProperties & MasksToBoundsChanged) {
        if (!decoder.decode(result.masksToBounds))
            return false;
    }

    if (result.changedProperties & OpaqueChanged) {
        if (!decoder.decode(result.opaque))
            return false;
    }

    if (result.changedProperties & MaskLayerChanged) {
        if (!decoder.decode(result.maskLayerID))
            return false;
    }

    if (result.changedProperties & ClonedContentsChanged) {
        if (!decoder.decode(result.clonedLayerID))
            return false;
    }

    if (result.changedProperties & ContentsRectChanged) {
        if (!decoder.decode(result.contentsRect))
            return false;
    }

    if (result.changedProperties & ContentsScaleChanged) {
        if (!decoder.decode(result.contentsScale))
            return false;
    }

    if (result.changedProperties & CornerRadiusChanged) {
        if (!decoder.decode(result.cornerRadius))
            return false;
    }

    if (result.changedProperties & ShapeRoundedRectChanged) {
        FloatRoundedRect roundedRect;
        if (!decoder.decode(roundedRect))
            return false;
        
        result.shapeRoundedRect = std::make_unique<FloatRoundedRect>(roundedRect);
    }

    if (result.changedProperties & ShapePathChanged) {
        Path path;
        if (!decoder.decode(path))
            return false;
        
        result.shapePath = WTF::move(path);
    }

    if (result.changedProperties & MinificationFilterChanged) {
        if (!decoder.decodeEnum(result.minificationFilter))
            return false;
    }

    if (result.changedProperties & MagnificationFilterChanged) {
        if (!decoder.decodeEnum(result.magnificationFilter))
            return false;
    }

    if (result.changedProperties & BlendModeChanged) {
        if (!decoder.decodeEnum(result.blendMode))
            return false;
    }

    if (result.changedProperties & WindRuleChanged) {
        if (!decoder.decodeEnum(result.windRule))
            return false;
    }

    if (result.changedProperties & SpeedChanged) {
        if (!decoder.decode(result.speed))
            return false;
    }

    if (result.changedProperties & TimeOffsetChanged) {
        if (!decoder.decode(result.timeOffset))
            return false;
    }

    if (result.changedProperties & BackingStoreChanged) {
        bool hasFrontBuffer = false;
        if (!decoder.decode(hasFrontBuffer))
            return false;
        if (hasFrontBuffer) {
            std::unique_ptr<RemoteLayerBackingStore> backingStore = std::make_unique<RemoteLayerBackingStore>(nullptr);
            if (!decoder.decode(*backingStore))
                return false;
            
            result.backingStore = WTF::move(backingStore);
        } else
            result.backingStore = nullptr;
    }

    if (result.changedProperties & BackingStoreAttachmentChanged) {
        if (!decoder.decode(result.backingStoreAttached))
            return false;
    }

    if (result.changedProperties & FiltersChanged) {
        std::unique_ptr<FilterOperations> filters = std::make_unique<FilterOperations>();
        if (!decoder.decode(*filters))
            return false;
        result.filters = WTF::move(filters);
    }

    if (result.changedProperties & EdgeAntialiasingMaskChanged) {
        if (!decoder.decode(result.edgeAntialiasingMask))
            return false;
    }

    if (result.changedProperties & CustomAppearanceChanged) {
        if (!decoder.decodeEnum(result.customAppearance))
            return false;
    }

    return true;
}

RemoteLayerTreeTransaction::RemoteLayerTreeTransaction()
    : m_pageScaleFactor(1)
    , m_minimumScaleFactor(1)
    , m_maximumScaleFactor(1)
    , m_renderTreeSize(0)
    , m_transactionID(0)
    , m_scaleWasSetByUIProcess(false)
    , m_allowsUserScaling(false)
{
}

RemoteLayerTreeTransaction::~RemoteLayerTreeTransaction()
{
}

void RemoteLayerTreeTransaction::encode(IPC::ArgumentEncoder& encoder) const
{
    encoder << m_rootLayerID;
    encoder << m_createdLayers;

    encoder << static_cast<uint64_t>(m_changedLayers.size());

    for (RefPtr<PlatformCALayerRemote> layer : m_changedLayers) {
        encoder << layer->layerID();
        encoder << layer->properties();
    }
    
    encoder << m_destroyedLayerIDs;
    encoder << m_videoLayerIDsPendingFullscreen;
    encoder << m_layerIDsWithNewlyUnreachableBackingStore;

    encoder << m_contentsSize;
    encoder << m_scrollOrigin;
#if PLATFORM(MAC)
    encoder << m_scrollPosition;
#endif
    encoder << m_pageExtendedBackgroundColor;
    encoder << m_pageScaleFactor;
    encoder << m_minimumScaleFactor;
    encoder << m_maximumScaleFactor;
    encoder << m_initialScaleFactor;
    encoder << m_viewportMetaTagWidth;

    encoder << m_renderTreeSize;
    encoder << m_transactionID;

    encoder << m_scaleWasSetByUIProcess;
    encoder << m_allowsUserScaling;

    encoder << m_viewportMetaTagWidthWasExplicit;
    encoder << m_viewportMetaTagCameFromImageDocument;

    encoder << m_callbackIDs;
}

bool RemoteLayerTreeTransaction::decode(IPC::ArgumentDecoder& decoder, RemoteLayerTreeTransaction& result)
{
    if (!decoder.decode(result.m_rootLayerID))
        return false;
    if (!result.m_rootLayerID)
        return false;

    if (!decoder.decode(result.m_createdLayers))
        return false;

    uint64_t numChangedLayerProperties;
    if (!decoder.decode(numChangedLayerProperties))
        return false;

    for (uint64_t i = 0; i < numChangedLayerProperties; ++i) {
        GraphicsLayer::PlatformLayerID layerID;
        if (!decoder.decode(layerID))
            return false;

        std::unique_ptr<LayerProperties> layerProperties = std::make_unique<LayerProperties>();
        if (!decoder.decode(*layerProperties))
            return false;

        result.changedLayerProperties().set(layerID, WTF::move(layerProperties));
    }

    if (!decoder.decode(result.m_destroyedLayerIDs))
        return false;

    for (auto& layerID : result.m_destroyedLayerIDs) {
        if (!layerID)
            return false;
    }

    if (!decoder.decode(result.m_videoLayerIDsPendingFullscreen))
        return false;

    if (!decoder.decode(result.m_layerIDsWithNewlyUnreachableBackingStore))
        return false;

    for (auto& layerID : result.m_layerIDsWithNewlyUnreachableBackingStore) {
        if (!layerID)
            return false;
    }

    if (!decoder.decode(result.m_contentsSize))
        return false;

    if (!decoder.decode(result.m_scrollOrigin))
        return false;

#if PLATFORM(MAC)
    if (!decoder.decode(result.m_scrollPosition))
        return false;
#endif
    
    if (!decoder.decode(result.m_pageExtendedBackgroundColor))
        return false;

    if (!decoder.decode(result.m_pageScaleFactor))
        return false;

    if (!decoder.decode(result.m_minimumScaleFactor))
        return false;

    if (!decoder.decode(result.m_maximumScaleFactor))
        return false;

    if (!decoder.decode(result.m_initialScaleFactor))
        return false;

    if (!decoder.decode(result.m_viewportMetaTagWidth))
        return false;

    if (!decoder.decode(result.m_renderTreeSize))
        return false;

    if (!decoder.decode(result.m_transactionID))
        return false;

    if (!decoder.decode(result.m_scaleWasSetByUIProcess))
        return false;

    if (!decoder.decode(result.m_allowsUserScaling))
        return false;

    if (!decoder.decode(result.m_viewportMetaTagWidthWasExplicit))
        return false;

    if (!decoder.decode(result.m_viewportMetaTagCameFromImageDocument))
        return false;

    if (!decoder.decode(result.m_callbackIDs))
        return false;

    return true;
}

void RemoteLayerTreeTransaction::setRootLayerID(GraphicsLayer::PlatformLayerID rootLayerID)
{
    ASSERT_ARG(rootLayerID, rootLayerID);

    m_rootLayerID = rootLayerID;
}

void RemoteLayerTreeTransaction::layerPropertiesChanged(PlatformCALayerRemote& remoteLayer)
{
    m_changedLayers.append(&remoteLayer);
}

void RemoteLayerTreeTransaction::setCreatedLayers(Vector<LayerCreationProperties> createdLayers)
{
    m_createdLayers = WTF::move(createdLayers);
}

void RemoteLayerTreeTransaction::setDestroyedLayerIDs(Vector<GraphicsLayer::PlatformLayerID> destroyedLayerIDs)
{
    m_destroyedLayerIDs = WTF::move(destroyedLayerIDs);
}

void RemoteLayerTreeTransaction::setLayerIDsWithNewlyUnreachableBackingStore(Vector<GraphicsLayer::PlatformLayerID> layerIDsWithNewlyUnreachableBackingStore)
{
    m_layerIDsWithNewlyUnreachableBackingStore = WTF::move(layerIDsWithNewlyUnreachableBackingStore);
}

#if !defined(NDEBUG) || !LOG_DISABLED

class RemoteLayerTreeTextStream : public TextStream
{
public:
    using TextStream::operator<<;

    RemoteLayerTreeTextStream()
        : m_indent(0)
    {
    }

    RemoteLayerTreeTextStream& operator<<(const TransformationMatrix&);
    RemoteLayerTreeTextStream& operator<<(PlatformCALayer::FilterType);
    RemoteLayerTreeTextStream& operator<<(const FloatRoundedRect&);
    RemoteLayerTreeTextStream& operator<<(FloatPoint3D);
    RemoteLayerTreeTextStream& operator<<(Color);
    RemoteLayerTreeTextStream& operator<<(FloatRect);
    RemoteLayerTreeTextStream& operator<<(const Vector<WebCore::GraphicsLayer::PlatformLayerID>&);
    RemoteLayerTreeTextStream& operator<<(const FilterOperation&);
    RemoteLayerTreeTextStream& operator<<(const FilterOperations&);
    RemoteLayerTreeTextStream& operator<<(const PlatformCAAnimationRemote::Properties&);
    RemoteLayerTreeTextStream& operator<<(const RemoteLayerBackingStore&);
    RemoteLayerTreeTextStream& operator<<(const WebCore::GraphicsLayer::CustomAppearance&);
    RemoteLayerTreeTextStream& operator<<(BlendMode);
    RemoteLayerTreeTextStream& operator<<(PlatformCAAnimation::AnimationType);
    RemoteLayerTreeTextStream& operator<<(PlatformCAAnimation::FillModeType);
    RemoteLayerTreeTextStream& operator<<(PlatformCAAnimation::ValueFunctionType);
    RemoteLayerTreeTextStream& operator<<(const TimingFunction&);
    RemoteLayerTreeTextStream& operator<<(const PlatformCAAnimationRemote::KeyframeValue&);

    void increaseIndent() { ++m_indent; }
    void decreaseIndent() { --m_indent; ASSERT(m_indent >= 0); }

    void writeIndent();

private:
    int m_indent;
};

template <class T>
static void dumpProperty(RemoteLayerTreeTextStream& ts, String name, T value)
{
    ts << "\n";
    ts.increaseIndent();
    ts.writeIndent();
    ts << "(" << name << " ";
    ts << value << ")";
    ts.decreaseIndent();
}

RemoteLayerTreeTextStream& RemoteLayerTreeTextStream::operator<<(const TransformationMatrix& transform)
{
    RemoteLayerTreeTextStream& ts = *this;
    ts << "\n";
    ts.increaseIndent();
    ts.writeIndent();
    ts << "[" << transform.m11() << " " << transform.m12() << " " << transform.m13() << " " << transform.m14() << "]\n";
    ts.writeIndent();
    ts << "[" << transform.m21() << " " << transform.m22() << " " << transform.m23() << " " << transform.m24() << "]\n";
    ts.writeIndent();
    ts << "[" << transform.m31() << " " << transform.m32() << " " << transform.m33() << " " << transform.m34() << "]\n";
    ts.writeIndent();
    ts << "[" << transform.m41() << " " << transform.m42() << " " << transform.m43() << " " << transform.m44() << "]";
    ts.decreaseIndent();
    return ts;
}

RemoteLayerTreeTextStream& RemoteLayerTreeTextStream::operator<<(const FloatRoundedRect& roundedRect)
{
    RemoteLayerTreeTextStream& ts = *this;
    ts << roundedRect.rect().x() << " " << roundedRect.rect().y() << " " << roundedRect.rect().width() << " " << roundedRect.rect().height() << "\n";

    ts.increaseIndent();
    ts.writeIndent();
    ts << "topLeft=" << roundedRect.topLeftCorner().width() << " " << roundedRect.topLeftCorner().height() << "\n";
    ts.writeIndent();
    ts << "topRight=" << roundedRect.topRightCorner().width() << " " << roundedRect.topRightCorner().height() << "\n";
    ts.writeIndent();
    ts << "bottomLeft=" << roundedRect.bottomLeftCorner().width() << " " << roundedRect.bottomLeftCorner().height() << "\n";
    ts.writeIndent();
    ts << "bottomRight=" << roundedRect.bottomRightCorner().width() << " " << roundedRect.bottomRightCorner().height();
    ts.decreaseIndent();

    return ts;
}

RemoteLayerTreeTextStream& RemoteLayerTreeTextStream::operator<<(PlatformCALayer::FilterType filterType)
{
    RemoteLayerTreeTextStream& ts = *this;
    switch (filterType) {
    case PlatformCALayer::Linear:
        ts << "linear";
        break;
    case PlatformCALayer::Nearest:
        ts << "nearest";
        break;
    case PlatformCALayer::Trilinear:
        ts << "trilinear";
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return ts;
}

RemoteLayerTreeTextStream& RemoteLayerTreeTextStream::operator<<(const FilterOperations& filters)
{
    RemoteLayerTreeTextStream& ts = *this;
    for (size_t i = 0; i < filters.size(); ++i) {
        const auto filter = filters.at(i);
        if (filter)
            ts << *filter;
        else
            ts << "(null)";
        if (i < filters.size() - 1)
            ts << " ";
    }
    return ts;
}
    
RemoteLayerTreeTextStream& RemoteLayerTreeTextStream::operator<<(const FilterOperation& filter)
{
    RemoteLayerTreeTextStream& ts = *this;
    switch (filter.type()) {
    case FilterOperation::REFERENCE:
        ts << "reference";
        break;
    case FilterOperation::GRAYSCALE: {
        const auto& colorMatrixFilter = downcast<BasicColorMatrixFilterOperation>(filter);
        ts << "grayscale(" << colorMatrixFilter.amount() << ")";
        break;
    }
    case FilterOperation::SEPIA: {
        const auto& colorMatrixFilter = downcast<BasicColorMatrixFilterOperation>(filter);
        ts << "sepia(" << colorMatrixFilter.amount() << ")";
        break;
    }
    case FilterOperation::SATURATE: {
        const auto& colorMatrixFilter = downcast<BasicColorMatrixFilterOperation>(filter);
        ts << "saturate(" << colorMatrixFilter.amount() << ")";
        break;
    }
    case FilterOperation::HUE_ROTATE: {
        const auto& colorMatrixFilter = downcast<BasicColorMatrixFilterOperation>(filter);
        ts << "hue-rotate(" << colorMatrixFilter.amount() << ")";
        break;
    }
    case FilterOperation::INVERT: {
        const auto& componentTransferFilter = downcast<BasicComponentTransferFilterOperation>(filter);
        ts << "invert(" << componentTransferFilter.amount() << ")";
        break;
    }
    case FilterOperation::OPACITY: {
        const auto& componentTransferFilter = downcast<BasicComponentTransferFilterOperation>(filter);
        ts << "opacity(" << componentTransferFilter.amount() << ")";
        break;
    }
    case FilterOperation::BRIGHTNESS: {
        const auto& componentTransferFilter = downcast<BasicComponentTransferFilterOperation>(filter);
        ts << "brightness(" << componentTransferFilter.amount() << ")";
        break;
    }
    case FilterOperation::CONTRAST: {
        const auto& componentTransferFilter = downcast<BasicComponentTransferFilterOperation>(filter);
        ts << "contrast(" << componentTransferFilter.amount() << ")";
        break;
    }
    case FilterOperation::BLUR: {
        const auto& blurFilter = downcast<BlurFilterOperation>(filter);
        ts << "blur(" << floatValueForLength(blurFilter.stdDeviation(), 0) << ")";
        break;
    }
    case FilterOperation::DROP_SHADOW: {
        const auto& dropShadowFilter = downcast<DropShadowFilterOperation>(filter);
        ts << "drop-shadow(" << dropShadowFilter.x() << " " << dropShadowFilter.y() << " " << dropShadowFilter.location() << " ";
        ts << dropShadowFilter.color() << ")";
        break;
    }
    case FilterOperation::PASSTHROUGH:
        ts << "passthrough";
        break;
    case FilterOperation::DEFAULT: {
        const auto& defaultFilter = downcast<DefaultFilterOperation>(filter);
        ts << "default type=" << (int)defaultFilter.representedType();
        break;
    }
    case FilterOperation::NONE:
        ts << "none";
        break;
    }
    return ts;
}

RemoteLayerTreeTextStream& RemoteLayerTreeTextStream::operator<<(const WebCore::GraphicsLayer::CustomAppearance& customAppearance)
{
    RemoteLayerTreeTextStream& ts = *this;
    switch (customAppearance) {
    case WebCore::GraphicsLayer::CustomAppearance::NoCustomAppearance: ts << "none"; break;
    case WebCore::GraphicsLayer::CustomAppearance::ScrollingOverhang: ts << "scrolling-overhang"; break;
    case WebCore::GraphicsLayer::CustomAppearance::ScrollingShadow: ts << "scrolling-shadow"; break;
    case WebCore::GraphicsLayer::CustomAppearance::LightBackdropAppearance: ts << "light-backdrop"; break;
    case WebCore::GraphicsLayer::CustomAppearance::DarkBackdropAppearance: ts << "dark-backdrop"; break;
    }
    return ts;
}

RemoteLayerTreeTextStream& RemoteLayerTreeTextStream::operator<<(BlendMode blendMode)
{
    RemoteLayerTreeTextStream& ts = *this;
    switch (blendMode) {
    case BlendModeNormal: ts << "normal"; break;
    case BlendModeMultiply: ts << "multiply"; break;
    case BlendModeScreen: ts << "screen"; break;
    case BlendModeOverlay: ts << "overlay"; break;
    case BlendModeDarken: ts << "darken"; break;
    case BlendModeLighten: ts << "lighten"; break;
    case BlendModeColorDodge: ts << "color-dodge"; break;
    case BlendModeColorBurn: ts << "color-burn"; break;
    case BlendModeHardLight: ts << "hard-light"; break;
    case BlendModeSoftLight: ts << "soft-light"; break;
    case BlendModeDifference: ts << "difference"; break;
    case BlendModeExclusion: ts << "exclusion"; break;
    case BlendModeHue: ts << "hue"; break;
    case BlendModeSaturation: ts << "saturation"; break;
    case BlendModeColor: ts << "color"; break;
    case BlendModeLuminosity: ts << "luminosity"; break;
    case BlendModePlusDarker: ts << "plus-darker"; break;
    case BlendModePlusLighter: ts << "plus-lighter"; break;
    }
    return ts;
}

RemoteLayerTreeTextStream& RemoteLayerTreeTextStream::operator<<(PlatformCAAnimation::AnimationType type)
{
    RemoteLayerTreeTextStream& ts = *this;
    switch (type) {
    case PlatformCAAnimation::Basic: ts << "basic"; break;
    case PlatformCAAnimation::Keyframe: ts << "keyframe"; break;
    }
    return ts;
}

RemoteLayerTreeTextStream& RemoteLayerTreeTextStream::operator<<(PlatformCAAnimation::FillModeType type)
{
    RemoteLayerTreeTextStream& ts = *this;
    switch (type) {
    case PlatformCAAnimation::NoFillMode: ts << "none"; break;
    case PlatformCAAnimation::Forwards: ts << "forwards"; break;
    case PlatformCAAnimation::Backwards: ts << "backwards"; break;
    case PlatformCAAnimation::Both: ts << "both"; break;
    }
    return ts;
}

RemoteLayerTreeTextStream& RemoteLayerTreeTextStream::operator<<(PlatformCAAnimation::ValueFunctionType type)
{
    RemoteLayerTreeTextStream& ts = *this;
    switch (type) {
    case PlatformCAAnimation::NoValueFunction: ts << "none"; break;
    case PlatformCAAnimation::RotateX: ts << "rotateX"; break;
    case PlatformCAAnimation::RotateY: ts << "rotateY"; break;
    case PlatformCAAnimation::RotateZ: ts << "rotateX"; break;
    case PlatformCAAnimation::ScaleX: ts << "scaleX"; break;
    case PlatformCAAnimation::ScaleY: ts << "scaleY"; break;
    case PlatformCAAnimation::ScaleZ: ts << "scaleX"; break;
    case PlatformCAAnimation::Scale: ts << "scale"; break;
    case PlatformCAAnimation::TranslateX: ts << "translateX"; break;
    case PlatformCAAnimation::TranslateY: ts << "translateY"; break;
    case PlatformCAAnimation::TranslateZ: ts << "translateZ"; break;
    case PlatformCAAnimation::Translate: ts << "translate"; break;
    }
    return ts;
}

RemoteLayerTreeTextStream& RemoteLayerTreeTextStream::operator<<(const PlatformCAAnimationRemote::KeyframeValue& value)
{
    RemoteLayerTreeTextStream& ts = *this;

    switch (value.keyframeType()) {
    case PlatformCAAnimationRemote::KeyframeValue::NumberKeyType:
        ts << "number=" << value.numberValue();
        break;
    case PlatformCAAnimationRemote::KeyframeValue::ColorKeyType:
        ts << "color=";
        ts << value.colorValue();
        break;
    case PlatformCAAnimationRemote::KeyframeValue::PointKeyType:
        ts << "point=";
        ts << value.pointValue();
        break;
    case PlatformCAAnimationRemote::KeyframeValue::TransformKeyType:
        ts << "transform=";
        ts << value.transformValue();
        break;
    case PlatformCAAnimationRemote::KeyframeValue::FilterKeyType:
        ts << "filter=";
        if (value.filterValue())
            ts << *value.filterValue();
        else
            ts << "null";
        break;
    }
    return ts;
}

RemoteLayerTreeTextStream& RemoteLayerTreeTextStream::operator<<(const TimingFunction& timingFunction)
{
    RemoteLayerTreeTextStream& ts = *this;
    switch (timingFunction.type()) {
    case TimingFunction::LinearFunction:
        ts << "linear";
        break;
    case TimingFunction::CubicBezierFunction: {
        const CubicBezierTimingFunction& cubicBezierFunction = static_cast<const CubicBezierTimingFunction&>(timingFunction);
        ts << "cubic-bezier(" << cubicBezierFunction.x1() << ", " << cubicBezierFunction.y1() << ", " <<  cubicBezierFunction.x2() << ", " << cubicBezierFunction.y2() << ")";
        break;
    }
    case TimingFunction::StepsFunction: {
        const StepsTimingFunction& stepsFunction = static_cast<const StepsTimingFunction&>(timingFunction);
        ts << "steps(" << stepsFunction.numberOfSteps() << ", " << (stepsFunction.stepAtStart() ? "start" : "end") << ")";
        break;
    }
    }
    return ts;
}

RemoteLayerTreeTextStream& RemoteLayerTreeTextStream::operator<<(const PlatformCAAnimationRemote::Properties& animation)
{
    RemoteLayerTreeTextStream& ts = *this;

    ts << "type=";
    ts << animation.animationType;
    ts << " keyPath=";
    ts << animation.keyPath;

    if (animation.beginTime)
        dumpProperty(ts, "beginTime", animation.beginTime);

    if (animation.duration)
        dumpProperty(ts, "duration", animation.duration);

    if (animation.timeOffset)
        dumpProperty(ts, "timeOffset", animation.timeOffset);

    dumpProperty(ts, "repeatCount", animation.repeatCount);

    if (animation.speed != 1)
        dumpProperty(ts, "speed", animation.speed);

    dumpProperty(ts, "fillMode", animation.fillMode);
    dumpProperty(ts, "valueFunction", animation.valueFunction);

    if (animation.autoReverses)
        dumpProperty(ts, "autoReverses", animation.autoReverses);

    if (!animation.removedOnCompletion)
        dumpProperty(ts, "removedOnCompletion", animation.removedOnCompletion);

    if (animation.additive)
        dumpProperty(ts, "additive", animation.additive);

    if (animation.reverseTimingFunctions)
        dumpProperty(ts, "reverseTimingFunctions", animation.reverseTimingFunctions);

    if (animation.hasExplicitBeginTime)
        dumpProperty(ts, "hasExplicitBeginTime", animation.hasExplicitBeginTime);

    ts << "\n";
    ts.increaseIndent();
    ts.writeIndent();
    ts << "(" << "keyframes";
    ts.increaseIndent();

    size_t maxFrames = std::max(animation.keyValues.size(), animation.keyTimes.size());
    maxFrames = std::max(maxFrames, animation.timingFunctions.size());

    for (size_t i = 0; i < maxFrames; ++i) {
        ts << "\n";
        ts.writeIndent();
        ts << "(keyframe " << unsigned(i);
        if (i < animation.keyTimes.size())
            dumpProperty(ts, "time", animation.keyTimes[i]);

        if (i < animation.timingFunctions.size() && animation.timingFunctions[i])
            dumpProperty<const TimingFunction&>(ts, "timing function", *animation.timingFunctions[i]);

        if (i < animation.keyValues.size())
            dumpProperty(ts, "value", animation.keyValues[i]);

        ts << ")";
    }

    ts.decreaseIndent();
    ts.decreaseIndent();

    return ts;
}

RemoteLayerTreeTextStream& RemoteLayerTreeTextStream::operator<<(FloatPoint3D point)
{
    RemoteLayerTreeTextStream& ts = *this;
    ts << point.x() << " " << point.y() << " " << point.z();
    return ts;
}

RemoteLayerTreeTextStream& RemoteLayerTreeTextStream::operator<<(Color color)
{
    RemoteLayerTreeTextStream& ts = *this;
    ts << color.serialized();
    return ts;
}

RemoteLayerTreeTextStream& RemoteLayerTreeTextStream::operator<<(FloatRect rect)
{
    RemoteLayerTreeTextStream& ts = *this;
    ts << rect.x() << " " << rect.y() << " " << rect.width() << " " << rect.height();
    return ts;
}

RemoteLayerTreeTextStream& RemoteLayerTreeTextStream::operator<<(const Vector<GraphicsLayer::PlatformLayerID>& layers)
{
    RemoteLayerTreeTextStream& ts = *this;

    for (size_t i = 0; i < layers.size(); ++i) {
        if (i)
            ts << " ";
        ts << layers[i];
    }

    return ts;
}

RemoteLayerTreeTextStream& RemoteLayerTreeTextStream::operator<<(const RemoteLayerBackingStore& backingStore)
{
    RemoteLayerTreeTextStream& ts = *this;
    ts << backingStore.size();
    ts << " scale=" << backingStore.scale();
    if (backingStore.isOpaque())
        ts << " opaque";
    if (backingStore.acceleratesDrawing())
        ts << " accelerated";
    return ts;
}

void RemoteLayerTreeTextStream::writeIndent()
{
    for (int i = 0; i < m_indent; ++i)
        *this << "  ";
}

static void dumpChangedLayers(RemoteLayerTreeTextStream& ts, const RemoteLayerTreeTransaction::LayerPropertiesMap& changedLayerProperties)
{
    if (changedLayerProperties.isEmpty())
        return;

    ts << "\n";
    ts.writeIndent();
    ts << "(changed-layers";

    // Dump the layer properties sorted by layer ID.
    Vector<GraphicsLayer::PlatformLayerID> layerIDs;
    copyKeysToVector(changedLayerProperties, layerIDs);
    std::sort(layerIDs.begin(), layerIDs.end());

    for (auto& layerID : layerIDs) {
        const RemoteLayerTreeTransaction::LayerProperties& layerProperties = *changedLayerProperties.get(layerID);

        ts << "\n";
        ts.increaseIndent();
        ts.writeIndent();
        ts << "(layer " << layerID;

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::NameChanged)
            dumpProperty(ts, "name", layerProperties.name);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::ChildrenChanged)
            dumpProperty<Vector<GraphicsLayer::PlatformLayerID>>(ts, "children", layerProperties.children);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::PositionChanged)
            dumpProperty(ts, "position", layerProperties.position);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::BoundsChanged)
            dumpProperty(ts, "bounds", layerProperties.bounds);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::AnchorPointChanged)
            dumpProperty(ts, "anchorPoint", layerProperties.anchorPoint);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::BackgroundColorChanged)
            dumpProperty(ts, "backgroundColor", layerProperties.backgroundColor);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::BorderColorChanged)
            dumpProperty(ts, "borderColor", layerProperties.borderColor);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::BorderWidthChanged)
            dumpProperty(ts, "borderWidth", layerProperties.borderWidth);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::OpacityChanged)
            dumpProperty(ts, "opacity", layerProperties.opacity);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::TransformChanged)
            dumpProperty(ts, "transform", layerProperties.transform ? *layerProperties.transform : TransformationMatrix());

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::SublayerTransformChanged)
            dumpProperty(ts, "sublayerTransform", layerProperties.sublayerTransform ? *layerProperties.sublayerTransform : TransformationMatrix());

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::HiddenChanged)
            dumpProperty(ts, "hidden", layerProperties.hidden);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::GeometryFlippedChanged)
            dumpProperty(ts, "geometryFlipped", layerProperties.geometryFlipped);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::DoubleSidedChanged)
            dumpProperty(ts, "doubleSided", layerProperties.doubleSided);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::MasksToBoundsChanged)
            dumpProperty(ts, "masksToBounds", layerProperties.masksToBounds);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::OpaqueChanged)
            dumpProperty(ts, "opaque", layerProperties.opaque);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::MaskLayerChanged)
            dumpProperty(ts, "maskLayer", layerProperties.maskLayerID);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::ClonedContentsChanged)
            dumpProperty(ts, "clonedLayer", layerProperties.clonedLayerID);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::ContentsRectChanged)
            dumpProperty(ts, "contentsRect", layerProperties.contentsRect);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::ContentsScaleChanged)
            dumpProperty(ts, "contentsScale", layerProperties.contentsScale);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::CornerRadiusChanged)
            dumpProperty(ts, "cornerRadius", layerProperties.cornerRadius);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::ShapeRoundedRectChanged)
            dumpProperty(ts, "shapeRect", layerProperties.shapeRoundedRect ? *layerProperties.shapeRoundedRect : FloatRoundedRect());

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::MinificationFilterChanged)
            dumpProperty(ts, "minificationFilter", layerProperties.minificationFilter);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::MagnificationFilterChanged)
            dumpProperty(ts, "magnificationFilter", layerProperties.magnificationFilter);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::BlendModeChanged)
            dumpProperty(ts, "blendMode", layerProperties.blendMode);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::SpeedChanged)
            dumpProperty(ts, "speed", layerProperties.speed);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::TimeOffsetChanged)
            dumpProperty(ts, "timeOffset", layerProperties.timeOffset);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::BackingStoreChanged) {
            if (const RemoteLayerBackingStore* backingStore = layerProperties.backingStore.get())
                dumpProperty<const RemoteLayerBackingStore&>(ts, "backingStore", *backingStore);
            else
                dumpProperty(ts, "backingStore", "removed");
        }

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::BackingStoreAttachmentChanged)
            dumpProperty(ts, "backingStoreAttached", layerProperties.backingStoreAttached);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::FiltersChanged)
            dumpProperty(ts, "filters", layerProperties.filters ? *layerProperties.filters : FilterOperations());

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::AnimationsChanged) {
            for (const auto& keyAnimationPair : layerProperties.addedAnimations)
                dumpProperty(ts, "animation " +  keyAnimationPair.first, keyAnimationPair.second);

            for (const auto& name : layerProperties.keyPathsOfAnimationsToRemove)
                dumpProperty(ts, "removed animation", name);
        }

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::EdgeAntialiasingMaskChanged)
            dumpProperty(ts, "edgeAntialiasingMask", layerProperties.edgeAntialiasingMask);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::CustomAppearanceChanged)
            dumpProperty(ts, "customAppearance", layerProperties.customAppearance);

        ts << ")";

        ts.decreaseIndent();
    }

    ts.decreaseIndent();
}

void RemoteLayerTreeTransaction::dump() const
{
    fprintf(stderr, "%s", description().data());
}

CString RemoteLayerTreeTransaction::description() const
{
    RemoteLayerTreeTextStream ts;

    ts << "(";
    dumpProperty(ts, "transactionID", m_transactionID);
    dumpProperty(ts, "contentsSize", m_contentsSize);
    if (m_scrollOrigin != IntPoint::zero())
        dumpProperty(ts, "scrollOrigin", m_scrollOrigin);

    if (m_pageScaleFactor != 1)
        dumpProperty(ts, "pageScaleFactor", m_pageScaleFactor);

    ts << "(\n";
    ts.increaseIndent();
    ts.writeIndent();
    ts << "(root-layer " << m_rootLayerID << ")";

    if (!m_createdLayers.isEmpty()) {
        ts << "\n";
        ts.writeIndent();
        ts << "(created-layers";
        ts.increaseIndent();
        for (const auto& createdLayer : m_createdLayers) {
            ts << "\n";
            ts.writeIndent();
            ts << "(";
            switch (createdLayer.type) {
            case PlatformCALayer::LayerTypeLayer:
            case PlatformCALayer::LayerTypeWebLayer:
            case PlatformCALayer::LayerTypeSimpleLayer:
                ts << "layer";
                break;
            case PlatformCALayer::LayerTypeTransformLayer:
                ts << "transform-layer";
                break;
            case PlatformCALayer::LayerTypeWebTiledLayer:
                ts << "tiled-layer";
                break;
            case PlatformCALayer::LayerTypeTiledBackingLayer:
                ts << "tiled-backing-layer";
                break;
            case PlatformCALayer::LayerTypePageTiledBackingLayer:
                ts << "page-tiled-backing-layer";
                break;
            case PlatformCALayer::LayerTypeTiledBackingTileLayer:
                ts << "tiled-backing-tile";
                break;
            case PlatformCALayer::LayerTypeRootLayer:
                ts << "root-layer";
                break;
            case PlatformCALayer::LayerTypeBackdropLayer:
                ts << "backdrop-layer";
                break;
            case PlatformCALayer::LayerTypeAVPlayerLayer:
                ts << "av-player-layer (context-id " << createdLayer.hostingContextID << ")";
                break;
            case PlatformCALayer::LayerTypeWebGLLayer:
                ts << "web-gl-layer (context-id " << createdLayer.hostingContextID << ")";
                break;
            case PlatformCALayer::LayerTypeShapeLayer:
                ts << "shape-layer";
                break;
            case PlatformCALayer::LayerTypeScrollingLayer:
                ts << "scrolling-layer";
                break;
            case PlatformCALayer::LayerTypeCustom:
                ts << "custom-layer (context-id " << createdLayer.hostingContextID << ")";
                break;
            case PlatformCALayer::LayerTypeLightSystemBackdropLayer:
                ts << "light-system-backdrop-layer";
                break;
            case PlatformCALayer::LayerTypeDarkSystemBackdropLayer:
                ts << "dark-system-backdrop-layer";
                break;
            }
            ts << " " << createdLayer.layerID << ")";
        }
        ts << ")";
        ts.decreaseIndent();
    }

    dumpChangedLayers(ts, m_changedLayerProperties);

    if (!m_destroyedLayerIDs.isEmpty())
        dumpProperty<Vector<GraphicsLayer::PlatformLayerID>>(ts, "destroyed-layers", m_destroyedLayerIDs);

    ts << ")\n";

    return ts.release().utf8();
}

#endif // !defined(NDEBUG) || !LOG_DISABLED

} // namespace WebKit

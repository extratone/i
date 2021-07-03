/*
 * Copyright (C) 2007, 2008, 2012, Apple Inc. All rights reserved.
 */

#if PLATFORM(IOS)

#import "config.h"
#import "Icon.h"

#import "BitmapImage.h"
#import "GraphicsContext.h"

namespace WebCore {
    
Icon::Icon(CGImageRef image)
    : m_cgImage(image)
{
}

Icon::~Icon()
{
}

PassRefPtr<Icon> Icon::createIconForFiles(const Vector<String>& /*filenames*/)
{
    return 0;
}

PassRefPtr<Icon> Icon::createIconForImage(NativeImagePtr imageRef)
{
    if (!imageRef)
        return 0;

    return adoptRef(new Icon(imageRef));
}

void Icon::paint(GraphicsContext* context, const IntRect& destRect)
{
    if (context->paintingDisabled())
        return;

    GraphicsContextStateSaver stateSaver(*context);

    float width = CGImageGetWidth(m_cgImage.get());
    float height = CGImageGetHeight(m_cgImage.get());
    FloatSize size(width, height);
    FloatRect srcRect(0, 0, width, height);
    ColorSpace colorSpace = ColorSpaceDeviceRGB;

    context->setImageInterpolationQuality(InterpolationHigh);
    context->drawNativeImage(m_cgImage.get(), size, colorSpace, destRect, srcRect, 1.0f, CompositeSourceOver, BlendModeNormal, DefaultImageOrientation);
}

}

#endif // PLATFORM(IOS)

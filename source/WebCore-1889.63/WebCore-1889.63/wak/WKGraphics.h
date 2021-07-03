//
//  WKGraphics.h
//
//  Copyright (C) 2005, 2006, 2007, 2009 Apple Inc.  All rights reserved.
//

#ifndef WKGraphics_h
#define WKGraphics_h

#import <CoreGraphics/CoreGraphics.h>
#import <CoreGraphics/CoreGraphicsPrivate.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WKNonZeroWindingRule = 0,
    WKEvenOddWindingRule = 1
} WKWindingRule;

CGContextRef WKGetCurrentGraphicsContext(void);
void WKSetCurrentGraphicsContext(CGContextRef context);

void WKDrawFramedRect (CGContextRef context, CGRect aRect);
void WKDrawFramedRectWithWidthUsingOperation (CGContextRef context, CGRect aRect, float frameWidth, CGCompositeOperation op);
void WKRectFill (CGContextRef context, CGRect aRect);
void WKRectFillList (CGContextRef context, const CGRect *rects, int count);
void WKRectFillUsingOperation (CGContextRef context, CGRect aRect, CGCompositeOperation op);
void WKRectFillListUsingOperation (CGContextRef context, const CGRect *rects, int count, CGCompositeOperation op);
CGImageRef WKGraphicsCreateImageFromBundleWithName (const char *image_file);
CGPatternRef WKCreatePatternFromCGImage(CGImageRef imageRef);
void WKSetPattern(CGContextRef context, CGPatternRef pattern, bool fill, bool stroke);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
class WKFontAntialiasingStateSaver
{
public:

    WKFontAntialiasingStateSaver(CGContextRef context, bool useOrientationDependentFontAntialiasing)
        : m_context(context)
        , m_useOrientationDependentFontAntialiasing(useOrientationDependentFontAntialiasing)
    {
    }

    void setup(bool isLandscapeOrientation);
    void restore();

private:

#if PLATFORM(IOS_SIMULATOR)
#pragma clang diagnostic push
#if defined(__has_warning) && __has_warning("-Wunused-private-field")
#pragma clang diagnostic ignored "-Wunused-private-field"
#endif
#endif
    CGContextRef m_context;
    bool m_useOrientationDependentFontAntialiasing;
    CGFontAntialiasingStyle m_oldAntialiasingStyle;
#if PLATFORM(IOS_SIMULATOR)
#pragma clang diagnostic pop
#endif
};
#endif

#endif // WKGraphics_h

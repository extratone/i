//
//  WKGraphics.h
//
//  Copyright (C) 2005, 2006, 2007, Apple Inc.  All rights reserved.
//
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

    WKFontAntialiasingStateSaver(bool useOrientationDependentFontAntialiasing)
        : m_useOrientationDependentFontAntialiasing(useOrientationDependentFontAntialiasing)
    {
    }

    void setup(bool isLandscapeOrientation);
    void restore();

private:

    bool m_useOrientationDependentFontAntialiasing;
    bool m_oldShouldUseFontSmoothing;
    CGFontAntialiasingStyle m_oldAntialiasingStyle;
};
#endif

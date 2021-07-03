/*
 * Copyright (C) 2005, 2006, 2007, 2008, Apple Inc.  All rights reserved.
 */

#import "config.h"

#if PLATFORM(IOS)

#import "CSSPrimitiveValue.h"
#import "CSSValueKeywords.h"
#import "DateComponents.h"
#import "Document.h"
#import "Font.h"
#import "FontCache.h"
#import "Frame.h"
#import "FrameView.h"
#import "Gradient.h"
#import "GraphicsContext.h"
#import "GraphicsContextCG.h"
#import "HTMLInputElement.h"
#import "HTMLNames.h"
#import "HTMLSelectElement.h"
#import "Icon.h"
#import "Page.h"
#import "PlatformLocale.h"
#import "NodeRenderStyle.h"
#import "PaintInfo.h"
#import "RenderObject.h"
#import "RenderStyle.h"
#import "RenderThemeIOS.h"
#import "RenderView.h"
#import "SoftLinking.h"
#import "UserAgentStyleSheets.h"
#import "WebCoreThreadRun.h"
#import <CoreGraphics/CGPathPrivate.h>
#import <CoreText/CTFontDescriptorPriv.h>
#import <objc/runtime.h>
#import <wtf/RefPtr.h>
#import <wtf/StdLibExtras.h>

#if ENABLE(PROGRESS_ELEMENT)
#import "RenderProgress.h"
#endif

@interface UIApplication
+ (UIApplication *)sharedApplication;
@property(nonatomic,copy) NSString *preferredContentSizeCategory;
@end

SOFT_LINK_FRAMEWORK(UIKit)
SOFT_LINK_CLASS(UIKit, UIApplication)
SOFT_LINK_CONSTANT(UIKit, UIContentSizeCategoryDidChangeNotification, CFStringRef)
#define UIContentSizeCategoryDidChangeNotification getUIContentSizeCategoryDidChangeNotification()

using namespace std;

namespace WebCore {

const float ControlBaseHeight   = 20.0f;
const float ControlBaseFontSize = 11.0f;

struct IOSGradient {
    float* start; // points to static float[4]
    float* end; // points to static float[4]
    IOSGradient(float* aStart, float* anEnd)
        : start(aStart)
        , end(anEnd)
    { }
};

typedef IOSGradient* IOSGradientRef;

enum Interpolation
{
    LinearInterpolation,
    ExponentialInterpolation
};

static void interpolateLinearGradient(void *info, const CGFloat *inData, CGFloat *outData)
{
    IOSGradientRef gradient = static_cast<IOSGradientRef>(info);
    float alpha = inData[0];
    float inverse = 1.0f - alpha;

    outData[0] = inverse * gradient->start[0] + alpha * gradient->end[0];
    outData[1] = inverse * gradient->start[1] + alpha * gradient->end[1];
    outData[2] = inverse * gradient->start[2] + alpha * gradient->end[2];
    outData[3] = inverse * gradient->start[3] + alpha * gradient->end[3];
}

static void interpolateExponentialGradient(void *info, const CGFloat *inData, CGFloat *outData)
{
    IOSGradientRef gradient = static_cast<IOSGradientRef>(info);
    float a = inData[0];
    
    for (int paintInfo = 0; paintInfo < 4; ++paintInfo) {
        float   end = logf(max(gradient->end[paintInfo], 0.01f));
        float start = logf(max(gradient->start[paintInfo], 0.01f));
        
        outData[paintInfo] = expf(start - (end + start) * a);
    }
}

static CGFunctionRef getSharedFunctionRef(IOSGradientRef gradient, Interpolation interpolation)
{
    CGFunctionRef function = 0;

    static HashMap<IOSGradientRef, CGFunctionRef>* linearFunctionRefs = 0;
    static HashMap<IOSGradientRef, CGFunctionRef>* exponentialFunctionRefs = 0;
    
    if (interpolation == LinearInterpolation) {
        if (!linearFunctionRefs)
            linearFunctionRefs = new HashMap<IOSGradientRef, CGFunctionRef>;
        else
            function = linearFunctionRefs->get(gradient);
    
        if (!function) {
            static struct CGFunctionCallbacks linearFunctionCallbacks =  { 0, interpolateLinearGradient, 0 };
            linearFunctionRefs->set(gradient, function = CGFunctionCreate(gradient, 1, NULL, 4, NULL, &linearFunctionCallbacks));
        }

        return function;
    }

    if (!exponentialFunctionRefs)
        exponentialFunctionRefs = new HashMap<IOSGradientRef, CGFunctionRef>;
    else
        function = exponentialFunctionRefs->get(gradient);
    
    if (!function) {
        static struct CGFunctionCallbacks exponentialFunctionCallbacks =  { 0, interpolateExponentialGradient, 0 };
        exponentialFunctionRefs->set(gradient, function = CGFunctionCreate(gradient, 1, 0, 4, 0, &exponentialFunctionCallbacks));
    }
    
    return function;
}

static void drawAxialGradient(CGContextRef context, IOSGradientRef gradient, const FloatPoint& startPoint, const FloatPoint& stopPoint, Interpolation interpolation)
{
    RetainPtr<CGShadingRef> shading(AdoptCF, CGShadingCreateAxial(deviceRGBColorSpaceRef(), startPoint, stopPoint, getSharedFunctionRef(gradient, interpolation), false, false));
    CGContextDrawShading(context, shading.get());
}

static void drawRadialGradient(CGContextRef context, IOSGradientRef gradient, const FloatPoint& startPoint, float startRadius, const FloatPoint& stopPoint, float stopRadius, Interpolation interpolation)
{
    RetainPtr<CGShadingRef> shading(AdoptCF, CGShadingCreateRadial(deviceRGBColorSpaceRef(), startPoint, startRadius, stopPoint, stopRadius, getSharedFunctionRef(gradient, interpolation), false, false));
    CGContextDrawShading(context, shading.get());
}

enum IOSGradientType {
    InsetGradient,
    ShineGradient,
    ShadeGradient,
    ConvexGradient,
    ConcaveGradient,
    SliderTrackGradient,
    ReadonlySliderTrackGradient,
    SliderThumbOpaquePressedGradient,
};


static IOSGradientRef getInsetGradient()
{
    static float end [4] = { 0.0f/255.0f, 0.0f/255.0f, 0.0f/255.0f, 0.0f };
    static float start [4] = { 0.0f/255.0f, 0.0f/255.0f, 0.0f/255.0f, 0.2f };

    DEFINE_STATIC_LOCAL(IOSGradient, gradient, (start, end));
    return &gradient;
}

static IOSGradientRef getShineGradient()
{
    static float end [4] = { 1.0f, 1.0f, 1.0f, 0.8f };
    static float start [4] = { 1.0f, 1.0f, 1.0f, 0.0f };

    DEFINE_STATIC_LOCAL(IOSGradient, gradient, (start, end));
    return &gradient;
}

static IOSGradientRef getShadeGradient()
{
    static float end [4] = { 178.0f/255.0f, 178.0f/255.0f, 178.0f/255.0f, 0.65f };
    static float start [4] = { 252.0f/255.0f, 252.0f/255.0f, 252.0f/255.0f, 0.65f };

    DEFINE_STATIC_LOCAL(IOSGradient, gradient, (start, end));
    return &gradient;
}

static IOSGradientRef getConvexGradient()
{
    static float end [4] = { 255.0f/255.0f, 255.0f/255.0f, 255.0f/255.0f, 0.05f };
    static float start [4] = { 255.0f/255.0f, 255.0f/255.0f, 255.0f/255.0f, 0.43f };

    DEFINE_STATIC_LOCAL(IOSGradient, gradient, (start, end));
    return &gradient;
}

static IOSGradientRef getConcaveGradient()
{
    static float end [4] = { 255.0f/255.0f, 255.0f/255.0f, 255.0f/255.0f, 0.46f };
    static float start [4] = { 255.0f/255.0f, 255.0f/255.0f, 255.0f/255.0f, 0.0f };

    DEFINE_STATIC_LOCAL(IOSGradient, gradient, (start, end));
    return &gradient;
}

static IOSGradientRef getSliderTrackGradient()
{
    static float end [4] = { 132.0f/255.0f, 132.0f/255.0f, 132.0f/255.0f, 1.0f };
    static float start [4] = { 74.0f/255.0f, 77.0f/255.0f, 80.0f/255.0f, 1.0f };

    DEFINE_STATIC_LOCAL(IOSGradient, gradient, (start, end));
    return &gradient;
}

static IOSGradientRef getReadonlySliderTrackGradient()
{
    static float end [4] = { 132.0f/255.0f, 132.0f/255.0f, 132.0f/255.0f, 0.4f };
    static float start [4] = { 74.0f/255.0f, 77.0f/255.0f, 80.0f/255.0f, 0.4f };

    DEFINE_STATIC_LOCAL(IOSGradient, gradient, (start, end));
    return &gradient;
}

static IOSGradientRef getSliderThumbOpaquePressedGradient()
{
    static float end [4] = { 144.0f/255.0f, 144.0f/255.0f, 144.0f/255.0f, 1.0f };
    static float start [4] = { 55.0f/255.0f, 55.0f/255.0f, 55.0f/255.0f, 1.0f };

    DEFINE_STATIC_LOCAL(IOSGradient, gradient, (start, end));
    return &gradient;
}

static IOSGradientRef gradientWithName(IOSGradientType gradientType)
{
    switch (gradientType) {
        case InsetGradient:
            return getInsetGradient();

        case ShineGradient:
            return getShineGradient();

        case ShadeGradient:
            return getShadeGradient();

        case ConvexGradient:
            return getConvexGradient();

        case ConcaveGradient:
            return getConcaveGradient();

        case SliderTrackGradient:
            return getSliderTrackGradient();

        case ReadonlySliderTrackGradient:
            return getReadonlySliderTrackGradient();

        case SliderThumbOpaquePressedGradient:
            return getSliderThumbOpaquePressedGradient();
    }

    ASSERT_NOT_REACHED();
    return 0;
}

static void contentSizeCategoryDidChange(CFNotificationCenterRef, void*, CFStringRef name, const void*, CFDictionaryRef)
{
    ASSERT_UNUSED(name, CFEqual(name, UIContentSizeCategoryDidChangeNotification));
    WebThreadRun(^{
        Page::updateStyleForAllPagesAfterGlobalChangeInEnvironment();
    });
}

RenderThemeIOS::RenderThemeIOS()
{
    CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(), this, contentSizeCategoryDidChange, UIContentSizeCategoryDidChangeNotification, 0, CFNotificationSuspensionBehaviorDeliverImmediately);
}

PassRefPtr<RenderTheme> RenderTheme::themeForPage(Page*)
{
    static RenderTheme* rt = RenderThemeIOS::create().leakRef();
    return rt;
}

PassRefPtr<RenderTheme> RenderThemeIOS::create()
{
    return adoptRef(new RenderThemeIOS);
}

CFStringRef RenderThemeIOS::contentSizeCategory()
{
    return (CFStringRef)[[getUIApplicationClass() sharedApplication] preferredContentSizeCategory];
}

const Color& RenderThemeIOS::shadowColor() const
{
    static Color color(0.0f, 0.0f, 0.0f, 0.7f);
    return color;
}

FloatRect RenderThemeIOS::addRoundedBorderClip(RenderObject *box, GraphicsContext* context, const IntRect& rect)
{
    // To fix inner border bleeding issues <rdar://problem/9812507>, we clip to the outer border and assert that
    // the border is opaque or transparent, unless we're checked because checked radio/checkboxes show no bleeding.
    RenderStyle* style = box->style();
    RoundedRect border = isChecked(box) ? style->getRoundedInnerBorderFor(rect) : style->getRoundedBorderFor(rect);

    if (border.isRounded())
        context->clipRoundedRect(border);
    else
        context->clip(border.rect());

    if (isChecked(box)) {
        ASSERT(style->visitedDependentColor(CSSPropertyBorderTopColor).alpha() % 255 == 0);
        ASSERT(style->visitedDependentColor(CSSPropertyBorderRightColor).alpha() % 255 == 0);
        ASSERT(style->visitedDependentColor(CSSPropertyBorderBottomColor).alpha() % 255 == 0);
        ASSERT(style->visitedDependentColor(CSSPropertyBorderLeftColor).alpha() % 255 == 0);
    }

    return border.rect();
}

void RenderThemeIOS::adjustCheckboxStyle(StyleResolver*, RenderStyle* style, Element*) const
{
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;

    Length length = Length(static_cast<int>(ceilf(max(style->fontSize(), 10))), Fixed);
    
    style->setWidth(length);
    style->setHeight(length);
}

static CGPoint shortened(CGPoint start, CGPoint end, float width)
{
    float x = end.x - start.x;
    float y = end.y - start.y;
    float ratio = width / sqrtf(x * x + y * y);
    
    return CGPointMake(start.x + x * ratio, start.y + y * ratio);
}

bool RenderThemeIOS::paintCheckboxDecorations(RenderObject* box, const PaintInfo& paintInfo, const IntRect& r)
{
    GraphicsContextStateSaver stateSaver(*paintInfo.context);
    FloatRect clip = addRoundedBorderClip(box, paintInfo.context, r);

    float width = clip.width();
    float height = clip.height();

    CGContextRef cgContext = paintInfo.context->platformContext();
    if (isChecked(box)) {
        drawAxialGradient(cgContext, gradientWithName(ConcaveGradient), clip.location(), FloatPoint(clip.x(), clip.maxY()), LinearInterpolation);

        static float thicknessRatio = 2.0f / 14.0f;
        static CGSize size = { 14.0f, 14.0f };
        static CGPoint pathRatios[3] = {
            { 2.5f / size.width, 7.5f / size.height },
            { 5.5f / size.width, 10.5f / size.height },
            { 11.5f / size.width, 2.5f / size.height }
        };
        
        float lineWidth = min(width, height) * 2.0f * thicknessRatio;
        
        CGPoint line[3] = {
            CGPointMake(clip.x() + width * pathRatios[0].x, clip.y() + height * pathRatios[0].y), 
            CGPointMake(clip.x() + width * pathRatios[1].x, clip.y() + height * pathRatios[1].y), 
            CGPointMake(clip.x() + width * pathRatios[2].x, clip.y() + height * pathRatios[2].y)
        };
        CGPoint shadow[3] = {
            shortened(line[0], line[1], lineWidth / 4.0f),
            line[1],
            shortened(line[2], line[1], lineWidth / 4.0f)
        };
        
        paintInfo.context->setStrokeThickness(lineWidth);
        paintInfo.context->setStrokeColor(Color(0.0f, 0.0f, 0.0f, 0.7f), ColorSpaceDeviceRGB);
  
        paintInfo.context->drawJoinedLines(shadow, 3, true, kCGLineCapSquare);
        
        paintInfo.context->setStrokeThickness(min(clip.width(), clip.height()) * thicknessRatio);
        paintInfo.context->setStrokeColor(Color(1.0f, 1.0f, 1.0f, 240.0f / 255.0f), ColorSpaceDeviceRGB);
        
        paintInfo.context->drawJoinedLines(line, 3, true);
    } else {
        FloatPoint bottomCenter(clip.x() + clip.width() / 2.0f, clip.maxY());
        drawAxialGradient(cgContext, gradientWithName(ShadeGradient), clip.location(), FloatPoint(clip.x(), clip.maxY()), LinearInterpolation);
        drawRadialGradient(cgContext, gradientWithName(ShineGradient), bottomCenter, 0.0f, bottomCenter, sqrtf((width * width) / 4.0f + height * height), ExponentialInterpolation);
    }

    return false;
}

int RenderThemeIOS::baselinePosition(const RenderObject* o) const
{
    if (!o->isBox())
        return 0;

    const RenderBox* box = toRenderBox(o);

    if (box->style()->appearance() == CheckboxPart || box->style()->appearance() == RadioPart)
        return box->marginTop() + box->height() - 2; // The baseline is 2px up from the bottom of the checkbox/radio in AppKit.
    if (box->style()->appearance() == MenulistPart)
        return box->marginTop() + box->height() - 5; // This is to match AppKit.  There might be a better way to calculate this though.
    return RenderTheme::baselinePosition(box);
}

bool RenderThemeIOS::isControlStyled(const RenderStyle* style, const BorderData& border, const FillLayer& background, const Color& backgroundColor) const
{
    // Buttons and MenulistButtons are styled if they contain a background image.
    if (style->appearance() == PushButtonPart || style->appearance() == MenulistButtonPart)
        return style->visitedDependentColor(CSSPropertyBackgroundColor).alpha() == 0.0 || style->backgroundLayers()->hasImage();

    if (style->appearance() == TextFieldPart || style->appearance() == TextAreaPart)
        return *style->backgroundLayers() != background;

    return RenderTheme::isControlStyled(style, border, background, backgroundColor);
}

void RenderThemeIOS::adjustRadioStyle(StyleResolver*, RenderStyle* style, Element*) const
{
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto()) return;

    Length length = Length(static_cast<int>(ceilf(max(style->fontSize(), 10))), Fixed);
    
    style->setWidth(length);
    style->setHeight(length);
    
    style->setBorderRadius(IntSize(length.value() / 2.0f, length.value() / 2.0f));
}

bool RenderThemeIOS::paintRadioDecorations(RenderObject *box, const PaintInfo& paintInfo, const IntRect& r)
{
    GraphicsContextStateSaver stateSaver(*paintInfo.context);
    FloatRect clip = addRoundedBorderClip(box, paintInfo.context, r);

    CGContextRef cgContext = paintInfo.context->platformContext();
    if (isChecked(box)) {
        drawAxialGradient(cgContext, gradientWithName(ConcaveGradient), clip.location(), FloatPoint(clip.x(), clip.maxY()), LinearInterpolation);
        
        // The inner circle is 6/14 the size of the surrounding circle, 
        // leaving 8/14 around it.  (8/14)/2 = 2/7
        
        static float InnerInverseRatio(2.0f / 7.0f);
        
        clip.inflateX(-clip.width() * InnerInverseRatio);
        clip.inflateY(-clip.height() * InnerInverseRatio);
        
        paintInfo.context->drawRaisedEllipse(clip, Color::white, ColorSpaceDeviceRGB, shadowColor(), ColorSpaceDeviceRGB);
        
        FloatSize radius(clip.width() / 2.0f, clip.height() / 2.0f);
        paintInfo.context->clipRoundedRect(clip, radius, radius, radius, radius);
    }
    
    FloatPoint bottomCenter(clip.x() + clip.width() / 2.0f, clip.maxY());
    
    drawAxialGradient(cgContext, gradientWithName(ShadeGradient), clip.location(), FloatPoint(clip.x(), clip.maxY()), LinearInterpolation);
    drawRadialGradient(cgContext, gradientWithName(ShineGradient), bottomCenter, 0.0f, bottomCenter, max(clip.width(), clip.height()), ExponentialInterpolation);

    return false;
}

bool RenderThemeIOS::paintTextFieldDecorations(RenderObject* box, const PaintInfo& paintInfo, const IntRect& r)
{
    RenderStyle* style = box->style();
    IntPoint point(r.x() + style->borderLeftWidth(), r.y() + style->borderTopWidth());
 
    GraphicsContextStateSaver stateSaver(*paintInfo.context);

    paintInfo.context->clipRoundedRect(style->getRoundedBorderFor(r));

    // This gradient gets drawn black when printing.
    // Do not draw the gradient if there is no visible top border.
    bool topBorderIsInvisible = !style->hasBorder() || !style->borderTopWidth() || style->borderTopIsTransparent();
    if (!box->view()->printing() && !topBorderIsInvisible)
        drawAxialGradient(paintInfo.context->platformContext(), gradientWithName(InsetGradient), point, FloatPoint(CGPointMake(point.x(), point.y() + 3.0f)), LinearInterpolation);

    return false;
}

bool RenderThemeIOS::paintTextAreaDecorations(RenderObject* box, const PaintInfo& paintInfo, const IntRect& r)
{
    return paintTextFieldDecorations(box, paintInfo, r);
}

const int MenuListMinHeight = 15;

const float MenuListBaseHeight          = 20.0f;
const float MenuListBaseFontSize        = 11.0f;

const float MenuListArrowWidth          = 7.0f;
const float MenuListArrowHeight         = 6.0f;
const float MenuListButtonPaddingRight  = 19.0f;

int RenderThemeIOS::popupInternalPaddingRight(RenderStyle * aStyle) const
{
    if (aStyle->appearance() == MenulistButtonPart)
        return MenuListButtonPaddingRight + aStyle->borderTopWidth();
    return 0;
}

void RenderThemeIOS::adjustRoundBorderRadius(RenderStyle* style, RenderBox* boxRenderer)
{
    if (style->appearance() == NoControlPart || style->backgroundLayers()->hasImage())
        return;

    // FIXME: We should not be relying on border radius for the appearance of our controls <rdar://problem/7675493>
    Length radiusWidth(static_cast<int>(min(boxRenderer->width(), boxRenderer->height()) / 2.0f), Fixed);
    Length radiusHeight(static_cast<int>(boxRenderer->height() / 2.0f), Fixed);
    
    style->setBorderRadius(LengthSize(radiusWidth, radiusHeight));
}

static void applyCommonButtonPaddingToStyle(RenderStyle* style, Element* element)
{
    Document* document = element->document();
    RefPtr<CSSPrimitiveValue> emSize = CSSPrimitiveValue::create(0.5, CSSPrimitiveValue::CSS_EMS);
    int pixels = emSize->computeLength<int>(style, document->renderStyle(), document->frame()->pageZoomFactor());
    style->setPaddingBox(LengthBox(0, pixels, 0, pixels));
}

static void adjustSelectListButtonStyle(RenderStyle* style, Element* element)
{
    // Enforce "padding: 0 0.5em".
    applyCommonButtonPaddingToStyle(style, element);

    // Enforce "line-height: normal".
    style->setLineHeight(Length(-100.0, Percent));
}

static void adjustInputElementButtonStyle(RenderStyle* style, HTMLInputElement* inputElement)
{
    // Always Enforce "padding: 0 0.5em".
    applyCommonButtonPaddingToStyle(style, inputElement);
    
    // Don't adjust the style if the width is specified.
    if (style->width().isFixed() && style->width().value() > 0)
        return;

    // Don't adjust for unsupported date input types.
    DateComponents::Type dateType = inputElement->dateType();
    if (dateType == DateComponents::Invalid || dateType == DateComponents::Week)
        return;

    // Enforce the width and set the box-sizing to content-box to not conflict with the padding.
    Font font = style->font();
    float maximumWidth = inputElement->locale().maximumWidthForDateType(dateType, font);
    if (maximumWidth > 0) {    
        int width = static_cast<int>(maximumWidth + MenuListButtonPaddingRight);
        style->setWidth(Length(width, Fixed));
        style->setBoxSizing(CONTENT_BOX);
    }
}

void RenderThemeIOS::adjustMenuListButtonStyle(StyleResolver*, RenderStyle* style, Element* element) const
{
    // Set the min-height to be at least MenuListMinHeight.
    if (style->height().isAuto())
        style->setMinHeight(Length(max(MenuListMinHeight, static_cast<int>(MenuListBaseHeight / MenuListBaseFontSize * style->fontDescription().computedSize())), Fixed));
    else
        style->setMinHeight(Length(MenuListMinHeight, Fixed));

    // Enforce some default styles in the case that this is a non-multiple <select> element,
    // or a date input. We don't force these if this is just an element with
    // "-webkit-appearance: menulist-button".
    if (element->hasTagName(HTMLNames::selectTag) && !element->hasAttribute(HTMLNames::multipleAttr))
        adjustSelectListButtonStyle(style, element);
    else if (element->hasTagName(HTMLNames::inputTag)) {
        HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(element);
        adjustInputElementButtonStyle(style, inputElement);
    }
}

bool RenderThemeIOS::paintMenuListButtonDecorations(RenderObject* box, const PaintInfo& paintInfo, const IntRect& r)
{
    RenderStyle * style = box->style();
    float borderTopWidth = style->borderTopWidth();
    FloatRect clip(r.x() + style->borderLeftWidth(), r.y() + style->borderTopWidth(), r.width() - style->borderLeftWidth() - style->borderRightWidth(), r.height() - style->borderTopWidth() - style->borderBottomWidth());
    CGContextRef cgContext = paintInfo.context->platformContext();
    
    float   adjustLeft = style->borderLeftWidth() % 2 ? 0.5f : 0.5f,
            adjustRight = style->borderRightWidth() % 2 ? 0.5f : 0.5f,
            adjustTop = style->borderTopWidth() % 2 ? 0.5f : 0.5f,
            adjustBottom = style->borderBottomWidth() % 2 ? 0.5f : 0.5f;

    // Paint left-hand title portion.
    {
        FloatRect titleClip(clip.x() - adjustLeft, clip.y() - adjustTop, clip.width() - MenuListButtonPaddingRight + adjustLeft, clip.height() + adjustTop + adjustBottom);

        GraphicsContextStateSaver stateSaver(*paintInfo.context);
    
        paintInfo.context->clipRoundedRect(titleClip, 
            FloatSize(valueForLength(style->borderTopLeftRadius().width(), r.width()) - style->borderLeftWidth(), valueForLength(style->borderTopLeftRadius().height(), r.height()) - style->borderTopWidth()), FloatSize(0, 0), 
            FloatSize(valueForLength(style->borderBottomLeftRadius().width(), r.width()) - style->borderLeftWidth(), valueForLength(style->borderBottomLeftRadius().height(), r.height()) - style->borderBottomWidth()), FloatSize(0, 0));

        drawAxialGradient(cgContext, gradientWithName(ShadeGradient), titleClip.location(), FloatPoint(titleClip.x(), titleClip.maxY()), LinearInterpolation);
        drawAxialGradient(cgContext, gradientWithName(ShineGradient), FloatPoint(titleClip.x(), titleClip.maxY()), titleClip.location(), ExponentialInterpolation);
    }

    // Draw the separator after the initial padding.

    float separator = clip.maxX() - MenuListButtonPaddingRight;

    box->drawLineForBoxSide(paintInfo.context, separator - borderTopWidth, clip.y(), separator, clip.maxY(), BSRight, style->visitedDependentColor(CSSPropertyBorderTopColor), style->borderTopStyle(), 0, 0);

    FloatRect buttonClip(separator - adjustTop, clip.y() - adjustTop, MenuListButtonPaddingRight + adjustTop + adjustRight, clip.height() + adjustTop + adjustBottom);

    // Now paint the button portion.
    {
        GraphicsContextStateSaver stateSaver(*paintInfo.context);

        paintInfo.context->clipRoundedRect(buttonClip, 
            FloatSize(0, 0), FloatSize(valueForLength(style->borderTopRightRadius().width(), r.width()) - style->borderRightWidth(), valueForLength(style->borderTopRightRadius().height(), r.height()) - style->borderTopWidth()), 
            FloatSize(0, 0), FloatSize(valueForLength(style->borderBottomRightRadius().width(), r.width()) - style->borderRightWidth(), valueForLength(style->borderBottomRightRadius().height(), r.height()) - style->borderBottomWidth()));
    
        paintInfo.context->fillRect(buttonClip, style->visitedDependentColor(CSSPropertyBorderTopColor), style->colorSpace());
            
        drawAxialGradient(cgContext, gradientWithName(isFocused(box) && !isReadOnlyControl(box) ? ConcaveGradient : ConvexGradient), buttonClip.location(), FloatPoint(buttonClip.x(), buttonClip.maxY()), LinearInterpolation);
    }

    // Paint Indicators.

    if (box->isMenuList() && toHTMLSelectElement(box->node())->multiple()) {
        int size(2),
            count(3),
            padding(3);

        IntRect ellipse(buttonClip.x() + (buttonClip.width() - count * (size + padding) + padding) / 2.0f, buttonClip.maxY() - 10.0f, size, size);

        for (int index(0); index < count; ++index, ellipse.move(size + padding, 0))
            paintInfo.context->drawRaisedEllipse(ellipse, Color::white, ColorSpaceDeviceRGB, Color(0.f, 0.f, 0.f, 0.5f), ColorSpaceDeviceRGB);
    }  else {
        float centerX = floorf(buttonClip.x() + buttonClip.width() / 2.0f) - 0.5f;
        float centerY = floorf(buttonClip.y() + buttonClip.height() * 3.0f / 8.0f);
        
        FloatPoint arrow[3], shadow[3];
        
        arrow[0] = FloatPoint(centerX - MenuListArrowWidth / 2.0f, centerY);
        arrow[1] = FloatPoint(centerX + MenuListArrowWidth / 2.0f, centerY);
        arrow[2] = FloatPoint(centerX, centerY + MenuListArrowHeight);

        shadow[0] = FloatPoint(arrow[0].x(), arrow[0].y() + 1.0f);
        shadow[1] = FloatPoint(arrow[1].x(), arrow[1].y() + 1.0f);
        shadow[2] = FloatPoint(arrow[2].x(), arrow[2].y() + 1.0f);

        float opacity = isReadOnlyControl(box) ? 0.2f : 0.5f;
        paintInfo.context->setStrokeColor(Color(0.0f, 0.0f, 0.0f, opacity), ColorSpaceDeviceRGB);
        paintInfo.context->setFillColor(Color(0.0f, 0.0f, 0.0f, opacity), ColorSpaceDeviceRGB);
        paintInfo.context->drawConvexPolygon(3, shadow, true);

        paintInfo.context->setStrokeColor(Color::white, ColorSpaceDeviceRGB);
        paintInfo.context->setFillColor(Color::white, ColorSpaceDeviceRGB);
        paintInfo.context->drawConvexPolygon(3, arrow, true);
    }

    return false;
}

const CGFloat kTrackThickness = 4.0f;
const CGFloat kTrackRadius = kTrackThickness / 2.0f;
const int kDefaultSliderThumbSize = 16;

void RenderThemeIOS::adjustSliderTrackStyle(StyleResolver* selector, RenderStyle* style, Element* element) const
{
    RenderTheme::adjustSliderTrackStyle(selector, style, element);

    // FIXME: We should not be relying on border radius for the appearance of our controls <rdar://problem/7675493>
    Length radiusWidth(static_cast<int>(kTrackRadius), Fixed);
    Length radiusHeight(static_cast<int>(kTrackRadius), Fixed);
    style->setBorderRadius(LengthSize(radiusWidth, radiusHeight));
}

bool RenderThemeIOS::paintSliderTrack(RenderObject* box, const PaintInfo& paintInfo, const IntRect& r)
{
    IntRect trackClip = r;
    RenderStyle* style = box->style();

    bool isHorizontal = true;
    switch (style->appearance()) {
    case SliderHorizontalPart:
        isHorizontal = true;
        // Inset slightly so the thumb covers the edge.
        if (trackClip.width() > 2) {
            trackClip.setWidth(trackClip.width() - 2);
            trackClip.setX(trackClip.x() + 1);
        }
        trackClip.setHeight(static_cast<int>(kTrackThickness));
        trackClip.setY(r.y() + (r.height() / 2) - (kTrackThickness / 2));
        break;
    case SliderVerticalPart:
        isHorizontal = false;
        // Inset slightly so the thumb covers the edge.
        if (trackClip.height() > 2) {
            trackClip.setHeight(trackClip.height() - 2);
            trackClip.setY(trackClip.y() + 1);
        }
        trackClip.setWidth(kTrackThickness);
        trackClip.setX(r.x() + (r.width() / 2) - (kTrackThickness / 2));
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    ASSERT(trackClip.width() >= 0);
    ASSERT(trackClip.height() >= 0);
    CGFloat cornerWidth = trackClip.width() < kTrackThickness ? trackClip.width() / 2.0f : kTrackRadius;
    CGFloat cornerHeight = trackClip.height() < kTrackThickness ? trackClip.height() / 2.0f : kTrackRadius;

    bool readonly = isReadOnlyControl(box);

#if ENABLE(DATALIST_ELEMENT)
    paintSliderTicks(box, paintInfo, trackClip);
#endif

    // Draw the track gradient.
    {
        GraphicsContextStateSaver stateSaver(*paintInfo.context);

        IntSize cornerSize(cornerWidth, cornerHeight);
        RoundedRect innerBorder(trackClip, cornerSize, cornerSize, cornerSize, cornerSize);
        paintInfo.context->clipRoundedRect(innerBorder);

        CGContextRef cgContext = paintInfo.context->platformContext();
        IOSGradientRef gradient = readonly ? gradientWithName(ReadonlySliderTrackGradient) : gradientWithName(SliderTrackGradient);
        if (isHorizontal)
            drawAxialGradient(cgContext, gradient, trackClip.location(), FloatPoint(trackClip.x(), trackClip.maxY()), LinearInterpolation);
        else
            drawAxialGradient(cgContext, gradient, trackClip.location(), FloatPoint(trackClip.maxX(), trackClip.y()), LinearInterpolation);
    }

    // Draw the track border.
    {
        GraphicsContextStateSaver stateSaver(*paintInfo.context);

        CGContextRef cgContext = paintInfo.context->platformContext();
        if (readonly)
            paintInfo.context->setStrokeColor(Color(178, 178, 178), ColorSpaceDeviceRGB);
        else
            paintInfo.context->setStrokeColor(Color(76, 76, 76), ColorSpaceDeviceRGB);

        RetainPtr<CGMutablePathRef> roundedRectPath(AdoptCF, CGPathCreateMutable());
        CGPathAddRoundedRect(roundedRectPath.get(), 0, trackClip, cornerWidth, cornerHeight);
        CGContextAddPath(cgContext, roundedRectPath.get());
        CGContextSetLineWidth(cgContext, 1.0f);
        CGContextStrokePath(cgContext);
    }

    return false;
}

void RenderThemeIOS::adjustSliderThumbSize(RenderStyle* style, Element*) const
{
    if (style->appearance() != SliderThumbHorizontalPart && style->appearance() != SliderThumbVerticalPart)
        return;

    // Enforce "border-radius: 50%".
    Length length(50.0f, Percent);
    style->setBorderRadius(LengthSize(length, length));

    // Enforce a 16x16 size if no size is provided.
    if (style->width().isIntrinsicOrAuto() || style->height().isAuto()) {
        Length length = Length(kDefaultSliderThumbSize, Fixed);
        style->setWidth(length);
        style->setHeight(length);
    }
}

bool RenderThemeIOS::paintSliderThumbDecorations(RenderObject* box, const PaintInfo& paintInfo, const IntRect& r)
{
    GraphicsContextStateSaver stateSaver(*paintInfo.context);
    FloatRect clip = addRoundedBorderClip(box, paintInfo.context, r);

    CGContextRef cgContext = paintInfo.context->platformContext();
    FloatPoint bottomCenter(clip.x() + clip.width() / 2.0f, clip.maxY());
    if (isPressed(box))
        drawAxialGradient(cgContext, gradientWithName(SliderThumbOpaquePressedGradient), clip.location(), FloatPoint(clip.x(), clip.maxY()), LinearInterpolation);
    else {
        drawAxialGradient(cgContext, gradientWithName(ShadeGradient), clip.location(), FloatPoint(clip.x(), clip.maxY()), LinearInterpolation);
        drawRadialGradient(cgContext, gradientWithName(ShineGradient), bottomCenter, 0.0f, bottomCenter, max(clip.width(), clip.height()), ExponentialInterpolation);
    }

    return false;
}

#if ENABLE(PROGRESS_ELEMENT)
double RenderThemeIOS::animationRepeatIntervalForProgressBar(RenderProgress*) const
{
    return 0;
}

double RenderThemeIOS::animationDurationForProgressBar(RenderProgress*) const
{
    return 0;
}

bool RenderThemeIOS::paintProgressBar(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    if (!renderObject->isProgress())
        return true;

    const int progressBarHeight = 9;
    const float verticalOffset = (rect.height() - progressBarHeight) / 2.;

    GraphicsContextStateSaver stateSaver(*paintInfo.context);
    if (rect.width() < 10 || rect.height() < 9) {
        // The rect is smaller than the standard progress bar. We clip to the element's rect to avoid
        // leaking pixels outside the repaint rect.
        paintInfo.context->clip(rect);
    }

    // 1) Draw the progress bar track.
    // 1.1) Draw the white background with grey gradient border.
    GraphicsContext* context = paintInfo.context;
    context->setStrokeThickness(0.68);
    context->setStrokeStyle(SolidStroke);

    const float verticalRenderingPosition = rect.y() + verticalOffset;
    RefPtr<Gradient> strokeGradient = Gradient::create(FloatPoint(rect.x(), verticalRenderingPosition), FloatPoint(rect.x(), verticalRenderingPosition + progressBarHeight - 1));
    strokeGradient->addColorStop(0.0, Color(0x8d, 0x8d, 0x8d));
    strokeGradient->addColorStop(0.45, Color(0xee, 0xee, 0xee));
    strokeGradient->addColorStop(0.55, Color(0xee, 0xee, 0xee));
    strokeGradient->addColorStop(1.0, Color(0x8d, 0x8d, 0x8d));
    context->setStrokeGradient(strokeGradient.release());

    ColorSpace colorSpace = renderObject->style()->colorSpace();
    context->setFillColor(Color(255, 255, 255), colorSpace);

    Path trackPath;
    FloatRect trackRect(rect.x() + 0.25, verticalRenderingPosition + 0.25, rect.width() - 0.5, progressBarHeight - 0.5);
    FloatSize roundedCornerRadius(5, 4);
    trackPath.addRoundedRect(trackRect, roundedCornerRadius);
    context->drawPath(trackPath);

    // 1.2) Draw top gradient on the upper half. It is supposed to overlay the fill from the background and darker the stroked path.
    FloatRect border(rect.x(), rect.y() + verticalOffset, rect.width(), progressBarHeight);
    paintInfo.context->clipRoundedRect(border, roundedCornerRadius, roundedCornerRadius, roundedCornerRadius, roundedCornerRadius);


    float upperGradientHeight = progressBarHeight / 2.;
    RefPtr<Gradient> upperGradient = Gradient::create(FloatPoint(rect.x(), verticalRenderingPosition + 0.5), FloatPoint(rect.x(), verticalRenderingPosition + upperGradientHeight - 1.5));
    upperGradient->addColorStop(0.0, Color(133, 133, 133, 188));
    upperGradient->addColorStop(1.0, Color(18, 18, 18, 51));
    context->setFillGradient(upperGradient.release());

    context->fillRect(FloatRect(rect.x(), verticalRenderingPosition, rect.width(), upperGradientHeight));

    RenderProgress* renderProgress = toRenderProgress(renderObject);
    if (renderProgress->isDeterminate()) {
        // 2) Draw the progress bar.
        double position = clampTo(renderProgress->position(), 0.0, 1.0);
        double barWidth = position * rect.width();
        RefPtr<Gradient> barGradient = Gradient::create(FloatPoint(rect.x(), verticalRenderingPosition + 0.5), FloatPoint(rect.x(), verticalRenderingPosition + progressBarHeight - 1));
        barGradient->addColorStop(0.0, Color(195, 217, 247));
        barGradient->addColorStop(0.45, Color(118, 164, 228));
        barGradient->addColorStop(0.49, Color(118, 164, 228));
        barGradient->addColorStop(0.51, Color(36, 114, 210));
        barGradient->addColorStop(0.55, Color(36, 114, 210));
        barGradient->addColorStop(1.0, Color(57, 142, 244));
        context->setFillGradient(barGradient.release());

        RefPtr<Gradient> barStrokeGradient = Gradient::create(FloatPoint(rect.x(), verticalRenderingPosition), FloatPoint(rect.x(), verticalRenderingPosition + progressBarHeight - 1));
        barStrokeGradient->addColorStop(0.0, Color(95, 107, 183));
        barStrokeGradient->addColorStop(0.5, Color(66, 106, 174, 240));
        barStrokeGradient->addColorStop(1.0, Color(38, 104, 166));
        context->setStrokeGradient(barStrokeGradient.release());

        Path barPath;
        int left = rect.x();
        if (!renderProgress->style()->isLeftToRightDirection())
            left = rect.maxX() - barWidth;
        FloatRect barRect(left + 0.25, verticalRenderingPosition + 0.25, max(barWidth - 0.5, 0.0), progressBarHeight - 0.5);
        barPath.addRoundedRect(barRect, roundedCornerRadius);
        context->drawPath(barPath);
    }

    return false;
}
#endif // ENABLE(PROGRESS_ELEMENT)

#if ENABLE(DATALIST_ELEMENT)
IntSize RenderThemeIOS::sliderTickSize() const
{
    // FIXME: <rdar://problem/12271791> MERGEBOT: Correct values for slider tick of <input type="range"> elements (requires ENABLE_DATALIST_ELEMENT)
    return IntSize(1, 3);
}

int RenderThemeIOS::sliderTickOffsetFromTrackCenter() const
{
    // FIXME: <rdar://problem/12271791> MERGEBOT: Correct values for slider tick of <input type="range"> elements (requires ENABLE_DATALIST_ELEMENT)
    return -9;
}
#endif

void RenderThemeIOS::adjustSearchFieldStyle(StyleResolver* selector, RenderStyle* style, Element* element) const
{
    RenderTheme::adjustSearchFieldStyle(selector, style, element);

    if (!element)
        return;

    if (!style->hasBorder())
        return;

    RenderBox* box = element->renderBox();
    if (!box)
        return;

    adjustRoundBorderRadius(style, box);
}

bool RenderThemeIOS::paintSearchFieldDecorations(RenderObject* box, const PaintInfo& paintInfo, const IntRect& r)
{
    return paintTextFieldDecorations(box, paintInfo, r);
}

void RenderThemeIOS::adjustButtonStyle(StyleResolver* selector, RenderStyle* style, Element* element) const
{
    RenderTheme::adjustButtonStyle(selector, style, element);
    
    // Set padding: 0 1.0em; on buttons.
    // CSSPrimitiveValue::computeLengthInt only needs the element's style to calculate em lengths.
    // Since the element might not be in a document, just pass NULL for the root element style.
    RefPtr<CSSPrimitiveValue> emSize = CSSPrimitiveValue::create(1.0, CSSPrimitiveValue::CSS_EMS);
    int pixels = emSize->computeLength<int>(style, NULL);
    style->setPaddingBox(LengthBox(0, pixels, 0, pixels));

    if (!element)
        return;

    RenderBox* box = element->renderBox();
    if (!box)
        return;
    
    adjustRoundBorderRadius(style, box);
}

bool RenderThemeIOS::paintButtonDecorations(RenderObject* box, const PaintInfo& paintInfo, const IntRect& r)
{
    return paintPushButtonDecorations(box, paintInfo, r);
}

bool RenderThemeIOS::paintPushButtonDecorations(RenderObject* box, const PaintInfo& paintInfo, const IntRect& r)
{
    GraphicsContextStateSaver stateSaver(*paintInfo.context);
    FloatRect clip = addRoundedBorderClip(box, paintInfo.context, r);

    CGContextRef cgContext = paintInfo.context->platformContext();
    if (box->style()->visitedDependentColor(CSSPropertyBackgroundColor).isDark())
        drawAxialGradient(cgContext, gradientWithName(ConvexGradient), clip.location(), FloatPoint(clip.x(), clip.maxY()), LinearInterpolation);
    else {
        drawAxialGradient(cgContext, gradientWithName(ShadeGradient), clip.location(), FloatPoint(clip.x(), clip.maxY()), LinearInterpolation);
        drawAxialGradient(cgContext, gradientWithName(ShineGradient), FloatPoint(clip.x(), clip.maxY()), clip.location(), ExponentialInterpolation);
    }

    return false;
}

void RenderThemeIOS::setButtonSize(RenderStyle* style) const
{
    // If the width and height are both specified, then we have nothing to do.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;
    
    // Use the font size to determine the intrinsic width of the control.
    style->setHeight(Length(static_cast<int>(ControlBaseHeight / ControlBaseFontSize * style->fontDescription().computedSize()), Fixed));
}


const int kThumbnailBorderStrokeWidth = 1;
const int kThumbnailBorderCornerRadius = 1;
const int kVisibleBackgroundImageWidth = 1;
const int kMultipleThumbnailShrinkSize = 2;

bool RenderThemeIOS::paintFileUploadIconDecorations(RenderObject*, RenderObject* buttonRenderer, const PaintInfo& paintInfo, const IntRect& r, Icon* icon, FileUploadDecorations fileUploadDecorations)
{
    GraphicsContextStateSaver stateSaver(*paintInfo.context);

    IntSize cornerSize(kThumbnailBorderCornerRadius, kThumbnailBorderCornerRadius);
    Color pictureFrameColor = buttonRenderer ? buttonRenderer->style()->visitedDependentColor(CSSPropertyBorderTopColor) : Color(76.0f, 76.0f, 76.0f);

    IntRect thumbnailPictureFrameRect = r;
    IntRect thumbnailRect = r;
    thumbnailRect.contract(2 * kThumbnailBorderStrokeWidth, 2 * kThumbnailBorderStrokeWidth);
    thumbnailRect.move(kThumbnailBorderStrokeWidth, kThumbnailBorderStrokeWidth);

    if (fileUploadDecorations == MultipleFiles) {

        // Smaller thumbnails for multiple selection appearance.
        thumbnailPictureFrameRect.contract(kMultipleThumbnailShrinkSize, kMultipleThumbnailShrinkSize);
        thumbnailRect.contract(kMultipleThumbnailShrinkSize, kMultipleThumbnailShrinkSize);

        // Background picture frame and simple background icon with a gradient matching the button.
        Color backgroundImageColor = buttonRenderer ? Color(buttonRenderer->style()->visitedDependentColor(CSSPropertyBackgroundColor).rgb()) : Color(206.0f, 206.0f, 206.0f);
        paintInfo.context->fillRoundedRect(thumbnailPictureFrameRect, cornerSize, cornerSize, cornerSize, cornerSize, pictureFrameColor, ColorSpaceDeviceRGB);
        paintInfo.context->fillRect(thumbnailRect, backgroundImageColor, ColorSpaceDeviceRGB);
        {
            GraphicsContextStateSaver stateSaver2(*paintInfo.context);
            CGContextRef cgContext = paintInfo.context->platformContext();
            paintInfo.context->clip(thumbnailRect);
            if (backgroundImageColor.isDark())
                drawAxialGradient(cgContext, gradientWithName(ConvexGradient), thumbnailRect.location(), FloatPoint(thumbnailRect.x(), thumbnailRect.maxY()), LinearInterpolation);
            else {
                drawAxialGradient(cgContext, gradientWithName(ShadeGradient), thumbnailRect.location(), FloatPoint(thumbnailRect.x(), thumbnailRect.maxY()), LinearInterpolation);
                drawAxialGradient(cgContext, gradientWithName(ShineGradient), FloatPoint(thumbnailRect.x(), thumbnailRect.maxY()), thumbnailRect.location(), ExponentialInterpolation);
            }
        }

        // Move the rects for the Foreground picture frame and icon.
        int inset = kVisibleBackgroundImageWidth + kThumbnailBorderStrokeWidth;
        thumbnailPictureFrameRect.move(inset, inset);
        thumbnailRect.move(inset, inset);
    }

    // Foreground picture frame and icon.
    paintInfo.context->fillRoundedRect(thumbnailPictureFrameRect, cornerSize, cornerSize, cornerSize, cornerSize, pictureFrameColor, ColorSpaceDeviceRGB);
    icon->paint(paintInfo.context, thumbnailRect);

    return false;
}
    
Color RenderThemeIOS::platformActiveSelectionBackgroundColor() const
{
    return Color::transparent;
}

Color RenderThemeIOS::platformInactiveSelectionBackgroundColor() const
{
    return Color::transparent;
}

bool RenderThemeIOS::shouldShowPlaceholderWhenFocused() const
{
    return true;
}

bool RenderThemeIOS::shouldHaveSpinButton(HTMLInputElement*) const
{
    return false;
}

#if ENABLE(VIDEO)
String RenderThemeIOS::extraMediaControlsStyleSheet()
{
    return String(mediaControlsiOSUserAgentStyleSheet, sizeof(mediaControlsiOSUserAgentStyleSheet));
}
#endif

static FontWeight fromCTFontWeight(float fontWeight)
{
    if (fontWeight <= -0.8)
        return FontWeight100;
    else if (fontWeight <= -0.4)
        return FontWeight200;
    else if (fontWeight <= -0.2)
        return FontWeight300;
    else if (fontWeight <= 0.0)
        return FontWeight400;
    else if (fontWeight <= 0.2)
        return FontWeight500;
    else if (fontWeight <= 0.3)
        return FontWeight600;
    else if (fontWeight <= 0.4)
        return FontWeight700;
    else if (fontWeight <= 0.6)
        return FontWeight800;
    else if (fontWeight <= 0.8)
        return FontWeight900;

    return FontWeightNormal;
}

void RenderThemeIOS::systemFont(int cssValueId, FontDescription& fontDescription) const
{
    DEFINE_STATIC_LOCAL(FontDescription, systemFont, ());

    DEFINE_STATIC_LOCAL(FontDescription, headlineFont, ());
    DEFINE_STATIC_LOCAL(FontDescription, bodyFont, ());
    DEFINE_STATIC_LOCAL(FontDescription, subheadlineFont, ());
    DEFINE_STATIC_LOCAL(FontDescription, footnoteFont, ());
    DEFINE_STATIC_LOCAL(FontDescription, caption1Font, ());
    DEFINE_STATIC_LOCAL(FontDescription, caption2Font, ());
    DEFINE_STATIC_LOCAL(FontDescription, shortHeadlineFont, ());
    DEFINE_STATIC_LOCAL(FontDescription, shortBodyFont, ());
    DEFINE_STATIC_LOCAL(FontDescription, shortSubheadlineFont, ());
    DEFINE_STATIC_LOCAL(FontDescription, shortFootnoteFont, ());
    DEFINE_STATIC_LOCAL(FontDescription, shortCaption1Font, ());
    DEFINE_STATIC_LOCAL(FontDescription, tallBodyFont, ());

    DEFINE_STATIC_LOCAL(CFStringRef, userTextSize, (contentSizeCategory()));

    if (userTextSize != contentSizeCategory()) {
        userTextSize = contentSizeCategory();

        headlineFont.setIsAbsoluteSize(false);
        bodyFont.setIsAbsoluteSize(false);
        subheadlineFont.setIsAbsoluteSize(false);
        footnoteFont.setIsAbsoluteSize(false);
        caption1Font.setIsAbsoluteSize(false);
        caption2Font.setIsAbsoluteSize(false);
        shortHeadlineFont.setIsAbsoluteSize(false);
        shortBodyFont.setIsAbsoluteSize(false);
        shortSubheadlineFont.setIsAbsoluteSize(false);
        shortFootnoteFont.setIsAbsoluteSize(false);
        shortCaption1Font.setIsAbsoluteSize(false);
        tallBodyFont.setIsAbsoluteSize(false);
    }

    FontDescription* cachedDesc;
    RetainPtr<CTFontDescriptorRef> fontDescriptor;
    CFStringRef textStyle;
    switch (cssValueId) {
    case CSSValueAppleSystemHeadline:
        cachedDesc = &headlineFont;
        textStyle = kCTUIFontTextStyleHeadline;
        if (!headlineFont.isAbsoluteSize())
            fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, userTextSize, 0));
        break;
    case CSSValueAppleSystemBody:
        cachedDesc = &bodyFont;
        textStyle = kCTUIFontTextStyleBody;
        if (!bodyFont.isAbsoluteSize())
            fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, userTextSize, 0));
        break;
    case CSSValueAppleSystemSubheadline:
        cachedDesc = &subheadlineFont;
        textStyle = kCTUIFontTextStyleSubhead;
        if (!subheadlineFont.isAbsoluteSize())
            fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, userTextSize, 0));
        break;
    case CSSValueAppleSystemFootnote:
        cachedDesc = &footnoteFont;
        textStyle = kCTUIFontTextStyleFootnote;
        if (!footnoteFont.isAbsoluteSize())
            fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, userTextSize, 0));
        break;
    case CSSValueAppleSystemCaption1:
        cachedDesc = &caption1Font;
        textStyle = kCTUIFontTextStyleCaption1;
        if (!caption1Font.isAbsoluteSize())
            fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, userTextSize, 0));
        break;
    case CSSValueAppleSystemCaption2:
        cachedDesc = &caption2Font;
        textStyle = kCTUIFontTextStyleCaption2;
        if (!caption2Font.isAbsoluteSize())
            fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, userTextSize, 0));
        break;

    // Short version.
    case CSSValueAppleSystemShortHeadline:
        cachedDesc = &shortHeadlineFont;
        textStyle = kCTUIFontTextStyleShortHeadline;
        if (!shortHeadlineFont.isAbsoluteSize())
            fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, userTextSize, 0));
        break;
    case CSSValueAppleSystemShortBody:
        cachedDesc = &shortBodyFont;
        textStyle = kCTUIFontTextStyleShortBody;
        if (!shortBodyFont.isAbsoluteSize())
            fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, userTextSize, 0));
        break;
    case CSSValueAppleSystemShortSubheadline:
        cachedDesc = &shortSubheadlineFont;
        textStyle = kCTUIFontTextStyleShortSubhead;
        if (!shortSubheadlineFont.isAbsoluteSize())
            fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, userTextSize, 0));
        break;
    case CSSValueAppleSystemShortFootnote:
        cachedDesc = &shortFootnoteFont;
        textStyle = kCTUIFontTextStyleShortFootnote;
        if (!shortFootnoteFont.isAbsoluteSize())
            fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, userTextSize, 0));
        break;
    case CSSValueAppleSystemShortCaption1:
        cachedDesc = &shortCaption1Font;
        textStyle = kCTUIFontTextStyleShortCaption1;
        if (!shortCaption1Font.isAbsoluteSize())
            fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, userTextSize, 0));
        break;

    // Tall version.
    case CSSValueAppleSystemTallBody:
        cachedDesc = &tallBodyFont;
        textStyle = kCTUIFontTextStyleTallBody;
        if (!tallBodyFont.isAbsoluteSize())
            fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, userTextSize, 0));
        break;

    default:
        textStyle = kCTFontDescriptorTextStyleEmphasized;
        cachedDesc = &systemFont;
        if (!systemFont.isAbsoluteSize())
            fontDescriptor = adoptCF(CTFontDescriptorCreateForUIType(kCTFontSystemFontType, 0.0, NULL));
    }

    if (fontDescriptor) {
        RetainPtr<CTFontRef> font(AdoptCF, CTFontCreateWithFontDescriptor(fontDescriptor.get(), 0.0, NULL));
        cachedDesc->setIsAbsoluteSize(true);
        cachedDesc->setGenericFamily(FontDescription::NoFamily);
        cachedDesc->setOneFamily(textStyle);
        cachedDesc->setSpecifiedSize(GSFontGetSize(font.get()));
        cachedDesc->setWeight(fromCTFontWeight(FontCache::weightOfCTFont(font.get())));
        cachedDesc->setItalic(0);
    }
    fontDescription = *cachedDesc;
}

}

#endif //PLATFORM(IOS)

/*
 * Copyright (C) 2003, 2006, 2007, 2008, 2009, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2008-2009 Torch Mobile, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef GraphicsContext_h
#define GraphicsContext_h

#include "ColorSpace.h"
#include "DashArray.h"
#include "FloatRect.h"
#include "Font.h"
#include "Gradient.h"
#include "Image.h"
#include "ImageOrientation.h"
#include "Path.h"
#include "Pattern.h"
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>

#if USE(CG)
typedef struct CGContext PlatformGraphicsContext;
#elif USE(CAIRO)
namespace WebCore {
class PlatformContextCairo;
}
typedef WebCore::PlatformContextCairo PlatformGraphicsContext;
#elif PLATFORM(QT)
#include <QPainter>
typedef QPainter PlatformGraphicsContext;
#elif USE(WINGDI)
typedef struct HDC__ PlatformGraphicsContext;
#elif PLATFORM(BLACKBERRY)
namespace BlackBerry {
namespace Platform {
namespace Graphics {
class PlatformGraphicsContext;
}
}
}
using BlackBerry::Platform::Graphics::PlatformGraphicsContext;
#else
typedef void PlatformGraphicsContext;
#endif

#if PLATFORM(WIN)
#include "DIBPixelData.h"
typedef struct HDC__* HDC;
#if !USE(CG)
// UInt8 is defined in CoreFoundation/CFBase.h
typedef unsigned char UInt8;
#endif
#endif

#if PLATFORM(QT) && OS(WINDOWS)
#include <windows.h>
#endif

namespace WebCore {

#if USE(WINGDI)
    class SharedBitmap;
    class SimpleFontData;
    class GlyphBuffer;
#endif

    const int cMisspellingLineThickness = 3;
    const int cMisspellingLinePatternWidth = 4;
    const int cMisspellingLinePatternGapWidth = 1;

    class AffineTransform;
    class DrawingBuffer;
    class Gradient;
    class GraphicsContextPlatformPrivate;
    class ImageBuffer;
    class IntRect;
    class RoundedRect;
    class KURL;
    class GraphicsContext3D;
    class TextRun;
    class TransformationMatrix;
#if PLATFORM(IOS)
    struct BidiStatus;
#endif

    enum TextDrawingMode {
        TextModeFill      = 1 << 0,
        TextModeStroke    = 1 << 1,
#if PLATFORM(IOS) && ENABLE(LETTERPRESS)
        TextModeLetterpress = 1 << 2,
#endif
    };
    typedef unsigned TextDrawingModeFlags;

    enum StrokeStyle {
        NoStroke,
        SolidStroke,
        DottedStroke,
        DashedStroke,
#if ENABLE(CSS3_TEXT)
        DoubleStroke,
        WavyStroke,
#endif // CSS3_TEXT
    };

    enum InterpolationQuality {
        InterpolationDefault,
        InterpolationNone,
        InterpolationLow,
        InterpolationMedium,
        InterpolationHigh
    };

    struct GraphicsContextState {
        GraphicsContextState()
            : strokeThickness(0)
            , shadowBlur(0)
            , textDrawingMode(TextModeFill)
            , strokeColor(Color::black)
            , fillColor(Color::black)
            , strokeStyle(SolidStroke)
            , fillRule(RULE_NONZERO)
            , strokeColorSpace(ColorSpaceDeviceRGB)
            , fillColorSpace(ColorSpaceDeviceRGB)
            , shadowColorSpace(ColorSpaceDeviceRGB)
            , compositeOperator(CompositeSourceOver)
            , blendMode(BlendModeNormal)
#if PLATFORM(IOS)
            , emojiDrawingEnabled(true)
            , shouldUseContextColors(true)
#endif
            , shouldAntialias(true)
            , shouldSmoothFonts(true)
            , shouldSubpixelQuantizeFonts(true)
            , paintingDisabled(false)
            , shadowsIgnoreTransforms(false)
#if USE(CG)
            // Core Graphics incorrectly renders shadows with radius > 8px (<rdar://problem/8103442>),
            // but we need to preserve this buggy behavior for canvas and -webkit-box-shadow.
            , shadowsUseLegacyRadius(false)
#endif
        {
        }

        RefPtr<Gradient> strokeGradient;
        RefPtr<Pattern> strokePattern;
        
        RefPtr<Gradient> fillGradient;
        RefPtr<Pattern> fillPattern;

        FloatSize shadowOffset;

        float strokeThickness;
        float shadowBlur;

        TextDrawingModeFlags textDrawingMode;

        Color strokeColor;
        Color fillColor;
        Color shadowColor;

        StrokeStyle strokeStyle;
        WindRule fillRule;

        ColorSpace strokeColorSpace;
        ColorSpace fillColorSpace;
        ColorSpace shadowColorSpace;

        CompositeOperator compositeOperator;
        BlendMode blendMode;

#if PLATFORM(IOS)
        bool emojiDrawingEnabled : 1;
        bool shouldUseContextColors : 1;
#endif
        bool shouldAntialias : 1;
        bool shouldSmoothFonts : 1;
        bool shouldSubpixelQuantizeFonts : 1;
        bool paintingDisabled : 1;
        bool shadowsIgnoreTransforms : 1;
#if USE(CG)
        bool shadowsUseLegacyRadius : 1;
#endif
    };

#if PLATFORM(IOS)
    void setStrokeAndFillColor(PlatformGraphicsContext* context, CGColorRef color);
#endif

    class GraphicsContext {
        WTF_MAKE_NONCOPYABLE(GraphicsContext); WTF_MAKE_FAST_ALLOCATED;
    public:
#if !PLATFORM(IOS)
        GraphicsContext(PlatformGraphicsContext*);
#else
        GraphicsContext(PlatformGraphicsContext*, bool setContextColors = true);
#endif
        ~GraphicsContext();

        PlatformGraphicsContext* platformContext() const;

        float strokeThickness() const;
        void setStrokeThickness(float);
        StrokeStyle strokeStyle() const;
        void setStrokeStyle(StrokeStyle);
        Color strokeColor() const;
        ColorSpace strokeColorSpace() const;
        void setStrokeColor(const Color&, ColorSpace);

        void setStrokePattern(PassRefPtr<Pattern>);
        Pattern* strokePattern() const;

        void setStrokeGradient(PassRefPtr<Gradient>);
        Gradient* strokeGradient() const;

        WindRule fillRule() const;
        void setFillRule(WindRule);
        Color fillColor() const;
        ColorSpace fillColorSpace() const;
        void setFillColor(const Color&, ColorSpace);

        void setFillPattern(PassRefPtr<Pattern>);
        Pattern* fillPattern() const;

        void setFillGradient(PassRefPtr<Gradient>);
        Gradient* fillGradient() const;

        void setShadowsIgnoreTransforms(bool);
        bool shadowsIgnoreTransforms() const;

        void setShouldAntialias(bool);
        bool shouldAntialias() const;

        void setShouldSmoothFonts(bool);
        bool shouldSmoothFonts() const;

        // Normally CG enables subpixel-quantization because it improves the performance of aligning glyphs.
        // In some cases we have to disable to to ensure a high-quality output of the glyphs.
        void setShouldSubpixelQuantizeFonts(bool);
        bool shouldSubpixelQuantizeFonts() const;

        const GraphicsContextState& state() const;

#if USE(CG)
        void applyStrokePattern();
        void applyFillPattern();
        void drawPath(const Path&);

#if PLATFORM(IOS)
        void drawNativeImage(PassNativeImagePtr, const FloatSize& selfSize, ColorSpace styleColorSpace, const FloatRect& destRect, const FloatRect& srcRect, float scale, CompositeOperator = CompositeSourceOver, BlendMode = BlendModeNormal, ImageOrientation = DefaultImageOrientation);
#else
        void drawNativeImage(PassNativeImagePtr, const FloatSize& selfSize, ColorSpace styleColorSpace, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator = CompositeSourceOver, BlendMode = BlendModeNormal, ImageOrientation = DefaultImageOrientation);
#endif

        // Allow font smoothing (LCD antialiasing). Not part of the graphics state.
        void setAllowsFontSmoothing(bool);
        
        void setIsCALayerContext(bool);
        bool isCALayerContext() const;

        void setIsAcceleratedContext(bool);
#endif
        bool isAcceleratedContext() const;

        void save();
        void restore();

        // These draw methods will do both stroking and filling.
        // FIXME: ...except drawRect(), which fills properly but always strokes
        // using a 1-pixel stroke inset from the rect borders (of the correct
        // stroke color).
        void drawRect(const IntRect&);
#if PLATFORM(IOS)
        void drawLine(const IntPoint&, const IntPoint&, bool antialias = false);
#else
        void drawLine(const IntPoint&, const IntPoint&);
#endif

#if PLATFORM(IOS)
        void drawJoinedLines(CGPoint points [], unsigned count, bool antialias, CGLineCap lineCap = kCGLineCapButt);
#endif

        void drawEllipse(const IntRect&);
#if PLATFORM(IOS)
        void drawEllipse(const FloatRect&);
        void drawRaisedEllipse(const FloatRect&, const Color& ellipseColor, ColorSpace ellipseColorSpace, const Color& shadowColor, ColorSpace shadowColorSpace);
#endif
        void drawConvexPolygon(size_t numPoints, const FloatPoint*, bool shouldAntialias = false);

        void fillPath(const Path&);
        void strokePath(const Path&);

        void fillEllipse(const FloatRect&);
        void strokeEllipse(const FloatRect&);

        void fillRect(const FloatRect&);
        void fillRect(const FloatRect&, const Color&, ColorSpace);
        void fillRect(const FloatRect&, Gradient&);
        void fillRect(const FloatRect&, const Color&, ColorSpace, CompositeOperator, BlendMode = BlendModeNormal);
        void fillRoundedRect(const IntRect&, const IntSize& topLeft, const IntSize& topRight, const IntSize& bottomLeft, const IntSize& bottomRight, const Color&, ColorSpace);
        void fillRoundedRect(const RoundedRect&, const Color&, ColorSpace, BlendMode = BlendModeNormal);
        void fillRectWithRoundedHole(const IntRect&, const RoundedRect& roundedHoleRect, const Color&, ColorSpace);

        void clearRect(const FloatRect&);

        void strokeRect(const FloatRect&, float lineWidth);

        void drawImage(Image*, ColorSpace styleColorSpace, const IntPoint&, CompositeOperator = CompositeSourceOver, RespectImageOrientationEnum = DoNotRespectImageOrientation);
        void drawImage(Image*, ColorSpace styleColorSpace, const IntRect&, CompositeOperator = CompositeSourceOver, RespectImageOrientationEnum = DoNotRespectImageOrientation, bool useLowQualityScale = false);
        void drawImage(Image*, ColorSpace styleColorSpace, const IntPoint& destPoint, const IntRect& srcRect, CompositeOperator = CompositeSourceOver, RespectImageOrientationEnum = DoNotRespectImageOrientation);
        void drawImage(Image*, ColorSpace styleColorSpace, const FloatRect& destRect);
        void drawImage(Image*, ColorSpace styleColorSpace, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator = CompositeSourceOver, RespectImageOrientationEnum = DoNotRespectImageOrientation, bool useLowQualityScale = false);
        void drawImage(Image*, ColorSpace styleColorSpace, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator, BlendMode, RespectImageOrientationEnum = DoNotRespectImageOrientation, bool useLowQualityScale = false);
        
        void drawTiledImage(Image*, ColorSpace styleColorSpace, const IntRect& destRect, const IntPoint& srcPoint, const IntSize& tileSize,
            CompositeOperator = CompositeSourceOver, bool useLowQualityScale = false, BlendMode = BlendModeNormal);
        void drawTiledImage(Image*, ColorSpace styleColorSpace, const IntRect& destRect, const IntRect& srcRect,
                            const FloatSize& tileScaleFactor, Image::TileRule hRule = Image::StretchTile, Image::TileRule vRule = Image::StretchTile,
                            CompositeOperator = CompositeSourceOver, bool useLowQualityScale = false);

        void drawImageBuffer(ImageBuffer*, ColorSpace styleColorSpace, const IntPoint&, CompositeOperator = CompositeSourceOver, BlendMode = BlendModeNormal);
        void drawImageBuffer(ImageBuffer*, ColorSpace styleColorSpace, const IntRect&, CompositeOperator = CompositeSourceOver, BlendMode = BlendModeNormal, bool useLowQualityScale = false);
        void drawImageBuffer(ImageBuffer*, ColorSpace styleColorSpace, const IntPoint& destPoint, const IntRect& srcRect, CompositeOperator = CompositeSourceOver, BlendMode = BlendModeNormal);
        void drawImageBuffer(ImageBuffer*, ColorSpace styleColorSpace, const IntRect& destRect, const IntRect& srcRect, CompositeOperator = CompositeSourceOver, BlendMode = BlendModeNormal, bool useLowQualityScale = false);
        void drawImageBuffer(ImageBuffer*, ColorSpace styleColorSpace, const FloatRect& destRect);
        void drawImageBuffer(ImageBuffer*, ColorSpace styleColorSpace, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator = CompositeSourceOver, BlendMode = BlendModeNormal, bool useLowQualityScale = false);

        void setImageInterpolationQuality(InterpolationQuality);
        InterpolationQuality imageInterpolationQuality() const;

        void clip(const IntRect&);
        void clip(const FloatRect&);
        void clipRoundedRect(const RoundedRect&);
#if PLATFORM(IOS)
        void clipRoundedRect(const FloatRect&, const FloatSize& topLeft, const FloatSize& topRight, const FloatSize& bottomLeft, const FloatSize& bottomRight);
#endif
        void clipOut(const IntRect&);
        void clipOutRoundedRect(const RoundedRect&);
        void clipPath(const Path&, WindRule);
        void clipConvexPolygon(size_t numPoints, const FloatPoint*, bool antialias = true);
        void clipToImageBuffer(ImageBuffer*, const FloatRect&);
        
        IntRect clipBounds() const;

        TextDrawingModeFlags textDrawingMode() const;
        void setTextDrawingMode(TextDrawingModeFlags);

#if PLATFORM(IOS)
        bool emojiDrawingEnabled();
        void setEmojiDrawingEnabled(bool emojiDrawingEnabled);
#endif
        
#if !PLATFORM(IOS)
        void drawText(const Font&, const TextRun&, const FloatPoint&, int from = 0, int to = -1);
#else
        float drawText(const Font&, const TextRun&, const FloatPoint&, int from = 0, int to = -1);
#endif
        void drawEmphasisMarks(const Font&, const TextRun& , const AtomicString& mark, const FloatPoint&, int from = 0, int to = -1);
#if !PLATFORM(IOS)
        void drawBidiText(const Font&, const TextRun&, const FloatPoint&, Font::CustomFontNotReadyAction = Font::DoNotPaintIfFontNotReady);
#else
        float drawBidiText(const Font&, const TextRun&, const FloatPoint&, Font::CustomFontNotReadyAction = Font::DoNotPaintIfFontNotReady, BidiStatus* = 0, int length = -1);
#endif
        void drawHighlightForText(const Font&, const TextRun&, const FloatPoint&, int h, const Color& backgroundColor, ColorSpace, int from = 0, int to = -1);

        enum RoundingMode {
            RoundAllSides,
            RoundOriginAndDimensions
        };
        FloatRect roundToDevicePixels(const FloatRect&, RoundingMode = RoundAllSides);

        void drawLineForText(const FloatPoint&, float width, bool printing);
        enum DocumentMarkerLineStyle {
#if PLATFORM(IOS)
            TextCheckingDictationPhraseWithAlternativesLineStyle,
#endif
            DocumentMarkerSpellingLineStyle,
            DocumentMarkerGrammarLineStyle,
            DocumentMarkerAutocorrectionReplacementLineStyle,
            DocumentMarkerDictationAlternativesLineStyle
        };
        void drawLineForDocumentMarker(const FloatPoint&, float width, DocumentMarkerLineStyle);

        bool paintingDisabled() const;
        void setPaintingDisabled(bool);

        bool updatingControlTints() const;
        void setUpdatingControlTints(bool);

        void beginTransparencyLayer(float opacity);
        void endTransparencyLayer();
        bool isInTransparencyLayer() const;

        bool hasShadow() const;
        void setShadow(const FloatSize&, float blur, const Color&, ColorSpace);
        // Legacy shadow blur radius is used for canvas, and -webkit-box-shadow.
        // It has different treatment of radii > 8px.
        void setLegacyShadow(const FloatSize&, float blur, const Color&, ColorSpace);

        bool getShadow(FloatSize&, float&, Color&, ColorSpace&) const;
        void clearShadow();

        bool hasBlurredShadow() const;
#if PLATFORM(QT) || USE(CAIRO)
        bool mustUseShadowBlur() const;
#endif

        void drawFocusRing(const Vector<IntRect>&, int width, int offset, const Color&);
        void drawFocusRing(const Path&, int width, int offset, const Color&);

        void setLineCap(LineCap);
        void setLineDash(const DashArray&, float dashOffset);
        void setLineJoin(LineJoin);
        void setMiterLimit(float);

        void setAlpha(float);

        void setCompositeOperation(CompositeOperator, BlendMode = BlendModeNormal);
        CompositeOperator compositeOperation() const;
        BlendMode blendModeOperation() const;

        void clip(const Path&, WindRule = RULE_EVENODD);

        // This clip function is used only by <canvas> code. It allows
        // implementations to handle clipping on the canvas differently since
        // the discipline is different.
        void canvasClip(const Path&, WindRule = RULE_EVENODD);
        void clipOut(const Path&);

        void scale(const FloatSize&);
        void rotate(float angleInRadians);
        void translate(const FloatSize& size) { translate(size.width(), size.height()); }
        void translate(float x, float y);

        void setURLForRect(const KURL&, const IntRect&);

        void concatCTM(const AffineTransform&);
        void setCTM(const AffineTransform&);

        enum IncludeDeviceScale { DefinitelyIncludeDeviceScale, PossiblyIncludeDeviceScale };
        AffineTransform getCTM(IncludeDeviceScale includeScale = PossiblyIncludeDeviceScale) const;

#if ENABLE(3D_RENDERING) && USE(TEXTURE_MAPPER)
        // This is needed when using accelerated-compositing in software mode, like in TextureMapper.
        void concat3DTransform(const TransformationMatrix&);
        void set3DTransform(const TransformationMatrix&);
        TransformationMatrix get3DTransform() const;
#endif
        // Create an image buffer compatible with this context, with suitable resolution
        // for drawing into the buffer and then into this context.
        PassOwnPtr<ImageBuffer> createCompatibleBuffer(const IntSize&, bool hasAlpha = true) const;
        bool isCompatibleWithBuffer(ImageBuffer*) const;

        // This function applies the device scale factor to the context, making the context capable of
        // acting as a base-level context for a HiDPI environment.
        void applyDeviceScaleFactor(float);
        void platformApplyDeviceScaleFactor(float);

#if OS(WINDOWS)
        HDC getWindowsContext(const IntRect&, bool supportAlphaBlend, bool mayCreateBitmap); // The passed in rect is used to create a bitmap for compositing inside transparency layers.
        void releaseWindowsContext(HDC, const IntRect&, bool supportAlphaBlend, bool mayCreateBitmap); // The passed in HDC should be the one handed back by getWindowsContext.
#if PLATFORM(WIN)
#if USE(WINGDI)
        void setBitmap(PassRefPtr<SharedBitmap>);
        const AffineTransform& affineTransform() const;
        AffineTransform& affineTransform();
        void resetAffineTransform();
        void fillRect(const FloatRect&, const Gradient*);
        void drawText(const SimpleFontData* fontData, const GlyphBuffer& glyphBuffer, int from, int numGlyphs, const FloatPoint& point);
        void drawFrameControl(const IntRect& rect, unsigned type, unsigned state);
        void drawFocusRect(const IntRect& rect);
        void paintTextField(const IntRect& rect, unsigned state);
        void drawBitmap(SharedBitmap*, const IntRect& dstRect, const IntRect& srcRect, ColorSpace styleColorSpace, CompositeOperator compositeOp, BlendMode blendMode);
        void drawBitmapPattern(SharedBitmap*, const FloatRect& tileRectIn, const AffineTransform& patternTransform, const FloatPoint& phase, ColorSpace styleColorSpace, CompositeOperator op, const FloatRect& destRect, const IntSize& origSourceSize);
        void drawIcon(HICON icon, const IntRect& dstRect, UINT flags);
        void drawRoundCorner(bool newClip, RECT clipRect, RECT rectWin, HDC dc, int width, int height);
#else
        GraphicsContext(HDC, bool hasAlpha = false); // FIXME: To be removed.

        // When set to true, child windows should be rendered into this context
        // rather than allowing them just to render to the screen. Defaults to
        // false.
        // FIXME: This is a layering violation. GraphicsContext shouldn't know
        // what a "window" is. It would be much more appropriate for this flag
        // to be passed as a parameter alongside the GraphicsContext, but doing
        // that would require lots of changes in cross-platform code that we
        // aren't sure we want to make.
        void setShouldIncludeChildWindows(bool);
        bool shouldIncludeChildWindows() const;

        class WindowsBitmap {
            WTF_MAKE_NONCOPYABLE(WindowsBitmap);
        public:
            WindowsBitmap(HDC, const IntSize&);
            ~WindowsBitmap();

            HDC hdc() const { return m_hdc; }
            UInt8* buffer() const { return m_pixelData.buffer(); }
            unsigned bufferLength() const { return m_pixelData.bufferLength(); }
            const IntSize& size() const { return m_pixelData.size(); }
            unsigned bytesPerRow() const { return m_pixelData.bytesPerRow(); }
            unsigned short bitsPerPixel() const { return m_pixelData.bitsPerPixel(); }
            const DIBPixelData& windowsDIB() const { return m_pixelData; }

        private:
            HDC m_hdc;
            HBITMAP m_bitmap;
            DIBPixelData m_pixelData;
        };

        PassOwnPtr<WindowsBitmap> createWindowsBitmap(const IntSize&);
        // The bitmap should be non-premultiplied.
        void drawWindowsBitmap(WindowsBitmap*, const IntPoint&);
#endif
#else // PLATFORM(WIN)
        bool shouldIncludeChildWindows() const { return false; }
#endif // PLATFORM(WIN)
#endif // OS(WINDOWS)

#if PLATFORM(QT)
        void pushTransparencyLayerInternal(const QRect&, qreal, QPixmap&);
        void takeOwnershipOfPlatformContext();
#endif

#if USE(CAIRO)
        GraphicsContext(cairo_t*);
#endif

#if PLATFORM(GTK)
        void setGdkExposeEvent(GdkEventExpose*);
        GdkWindow* gdkWindow() const;
        GdkEventExpose* gdkExposeEvent() const;
#endif

        static void adjustLineToPixelBoundaries(FloatPoint& p1, FloatPoint& p2, float strokeWidth, StrokeStyle);

    private:
#if !PLATFORM(IOS)
        void platformInit(PlatformGraphicsContext*);
#else
        void platformInit(PlatformGraphicsContext*, bool setContextColors);
#endif
        void platformDestroy();

#if PLATFORM(WIN) && !USE(WINGDI)
        void platformInit(HDC, bool hasAlpha = false);
#endif

        void savePlatformState();
        void restorePlatformState();

        void setPlatformTextDrawingMode(TextDrawingModeFlags);

        void setPlatformStrokeColor(const Color&, ColorSpace);
        void setPlatformStrokeStyle(StrokeStyle);
        void setPlatformStrokeThickness(float);

        void setPlatformFillColor(const Color&, ColorSpace);

        void setPlatformShouldAntialias(bool);
        void setPlatformShouldSmoothFonts(bool);

        void setPlatformShadow(const FloatSize&, float blur, const Color&, ColorSpace);
        void clearPlatformShadow();

        void setPlatformCompositeOperation(CompositeOperator, BlendMode = BlendModeNormal);

        void beginPlatformTransparencyLayer(float opacity);
        void endPlatformTransparencyLayer();
        static bool supportsTransparencyLayers();

        void fillEllipseAsPath(const FloatRect&);
        void strokeEllipseAsPath(const FloatRect&);

        void platformFillEllipse(const FloatRect&);
        void platformStrokeEllipse(const FloatRect&);

        GraphicsContextPlatformPrivate* m_data;

        GraphicsContextState m_state;
        Vector<GraphicsContextState> m_stack;
        bool m_updatingControlTints;
        unsigned m_transparencyCount;
    };

    class GraphicsContextStateSaver {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        GraphicsContextStateSaver(GraphicsContext& context, bool saveAndRestore = true)
        : m_context(context)
        , m_saveAndRestore(saveAndRestore)
        {
            if (m_saveAndRestore)
                m_context.save();
        }
        
        ~GraphicsContextStateSaver()
        {
            if (m_saveAndRestore)
                m_context.restore();
        }
        
        void save()
        {
            ASSERT(!m_saveAndRestore);
            m_context.save();
            m_saveAndRestore = true;
        }

        void restore()
        {
            ASSERT(m_saveAndRestore);
            m_context.restore();
            m_saveAndRestore = false;
        }
        
        GraphicsContext* context() const { return &m_context; }

    private:
        GraphicsContext& m_context;
        bool m_saveAndRestore;
    };

} // namespace WebCore

#endif // GraphicsContext_h


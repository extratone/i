/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric.seidel@kdemail.net>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KCanvasFilters_H
#define KCanvasFilters_H
#if SVG_SUPPORT

#include "DeprecatedStringList.h"
#include "KCanvasResources.h"

#ifdef __OBJC__
@class CIFilter;
#else
class CIFilter;
#endif

namespace WebCore {

class KCanvasFilterQuartz;

// Enumerations
typedef enum
{
    FE_DISTANT_LIGHT = 0,
    FE_POINT_LIGHT = 1,
    FE_SPOT_LIGHT = 2,
    FE_BLEND = 3,
    FE_COLOR_MATRIX = 4,
    FE_COMPONENT_TRANSFER = 5,
    FE_COMPOSITE = 6,
    FE_CONVOLVE_MATRIX = 7,
    FE_DIFFUSE_LIGHTING = 8,
    FE_DISPLACEMENT_MAP = 9,
    FE_FLOOD = 10,
    FE_GAUSSIAN_BLUR = 11,
    FE_IMAGE = 12,
    FE_MERGE = 13,
    FE_MORPHOLOGY = 14,
    FE_OFFSET = 15,
    FE_SPECULAR_LIGHTING = 16,
    FE_TILE = 17,
    FE_TURBULENCE = 18
} KCFilterEffectType;

class KCanvasPoint3F {
public:
    KCanvasPoint3F() : m_x(0), m_y(0), m_z(0) { }
    KCanvasPoint3F(float x, float y, float z) : m_x(x), m_y(y), m_z(z) { }
    
    float x() const { return m_x; }
    void setX(float x) { m_x = x; }
    
    float y() const { return m_y; }
    void setY(float y) { m_y = y; }
    
    float z() const { return m_z; }
    void setZ(float z) { m_z = z; }
    
    void normalize();
    
private:
    float m_x;
    float m_y;
    float m_z;
};

class KCanvasFilterEffect;
class KRenderingDevice;

class KCanvasFilter : public KCanvasResource
{
public:
    KCanvasFilter() { };
    virtual ~KCanvasFilter() { };
    
    virtual bool isFilter() const { return true; }
    
    bool filterBoundingBoxMode() const { return m_filterBBoxMode; }
    void setFilterBoundingBoxMode(bool bboxMode) { m_filterBBoxMode = bboxMode; }
    
    bool effectBoundingBoxMode() const { return m_effectBBoxMode; }
    void setEffectBoundingBoxMode(bool bboxMode) { m_effectBBoxMode = bboxMode; }    

    FloatRect filterRect() const { return m_filterRect; }
    void setFilterRect(const FloatRect &rect) { m_filterRect = rect; }
    
    FloatRect filterBBoxForItemBBox(FloatRect itemBBox) const;

    void clearEffects();
    void addFilterEffect(KCanvasFilterEffect *effect);

    virtual void prepareFilter(const FloatRect &bbox) = 0;
    virtual void applyFilter(const FloatRect &bbox) = 0;

    TextStream &externalRepresentation(TextStream &) const;

protected:
    FloatRect m_filterRect;
    DeprecatedValueList<KCanvasFilterEffect *> m_effects;
    bool m_filterBBoxMode;
    bool m_effectBBoxMode;
};

KCanvasFilter *getFilterById(Document *document, const AtomicString &id);

class KCanvasFilterEffect
{
public:
    // this default constructor is only needed for gcc 3.3
    KCanvasFilterEffect() { }
    virtual ~KCanvasFilterEffect() { };

    virtual KCFilterEffectType effectType() const { return FE_TURBULENCE; }

    FloatRect subRegion() const;
    void setSubRegion(const FloatRect &subregion);

    DeprecatedString in() const;
    void setIn(const DeprecatedString &in);

    DeprecatedString result() const;
    void setResult(const DeprecatedString &result);

#if __APPLE__
    virtual CIFilter* getCIFilter(KCanvasFilterQuartz*) const = 0;
#endif

    virtual TextStream &externalRepresentation(TextStream &) const;

private:
    FloatRect m_subregion;
    DeprecatedString m_in;
    DeprecatedString m_result;
};

TextStream &operator<<(TextStream &, const KCanvasFilterEffect &);

typedef enum {
    BM_NORMAL = 0,
    BM_MULTIPLY = 1,
    BM_SCREEN = 2,
    BM_DARKEN = 3,
    BM_LIGHTEN = 4
} KCBlendModeType;

class KCanvasFEBlend : public KCanvasFilterEffect
{
public:
    DeprecatedString in2() const { return m_in2; }
    void setIn2(const DeprecatedString &in2) { m_in2 = in2; }
    
    KCBlendModeType blendMode() const { return m_mode; }
    void setBlendMode(KCBlendModeType mode) { m_mode = mode; }

    TextStream &externalRepresentation(TextStream &) const;

private:
    KCBlendModeType m_mode;
    DeprecatedString m_in2;
};

typedef enum {
    CMT_MATRIX = 0,
    CMT_SATURATE = 1,
    CMT_HUE_ROTATE = 2,
    CMT_LUMINANCE_TO_ALPHA = 3
} KCColorMatrixType;

class KCanvasFEColorMatrix : public KCanvasFilterEffect
{
public:
    KCColorMatrixType type() const { return m_type; }
    void setType(KCColorMatrixType type) { m_type = type; }

    DeprecatedValueList<float> values() const { return m_values; }
    void setValues(const DeprecatedValueList<float> &values) { m_values = values; };

    TextStream &externalRepresentation(TextStream &) const;

private:
    KCColorMatrixType m_type;
    DeprecatedValueList<float> m_values;
};

typedef enum {
    CT_IDENTITY = 0,
    CT_TABLE = 1,
    CT_DISCRETE = 2,
    CT_LINEAR = 3,
    CT_GAMMA = 4
} KCComponentTransferType;

struct KCComponentTransferFunction
{
    KCComponentTransferFunction() : type(CT_IDENTITY) {}
    KCComponentTransferType type;
    Vector<float> tableValues;
    float slope;
    float intercept;
    float amplitude;
    float exponent;
    float offset;
};

class KCanvasFEComponentTransfer : public KCanvasFilterEffect
{
public:    
    KCComponentTransferFunction redFunction() const { return m_redFunc; }
    void setRedFunction(const KCComponentTransferFunction& func) { m_redFunc = func; }
    
    KCComponentTransferFunction greenFunction() const { return m_greenFunc; }
    void setGreenFunction(const KCComponentTransferFunction& func) { m_greenFunc = func; }
    
    KCComponentTransferFunction blueFunction() const { return m_blueFunc; }
    void setBlueFunction(const KCComponentTransferFunction& func) { m_blueFunc = func; }
    
    KCComponentTransferFunction alphaFunction() const { return m_alphaFunc; }
    void setAlphaFunction(const KCComponentTransferFunction& func) { m_alphaFunc = func; }

    TextStream& externalRepresentation(TextStream&) const;

private:
    KCComponentTransferFunction m_redFunc;
    KCComponentTransferFunction m_greenFunc;
    KCComponentTransferFunction m_blueFunc;
    KCComponentTransferFunction m_alphaFunc;
};

typedef enum {
    CO_OVER = 0,
    CO_IN = 1,
    CO_OUT = 2,
    CO_ATOP = 3,
    CO_XOR = 4,
    CO_ARITHMETIC = 5,
} KCCompositeOperationType;

class KCanvasFEComposite : public KCanvasFilterEffect
{
public:
    DeprecatedString in2() const { return m_in2; }
    void setIn2(const DeprecatedString &in2) { m_in2 = in2; }
    
    KCCompositeOperationType operation() const { return m_operation; }
    void setOperation(KCCompositeOperationType oper) { m_operation = oper; }
    
    float k1() const { return m_k1; }
    void setK1(float k1) { m_k1 = k1; }
    float k2() const { return m_k2;}
    void setK2(float k2) { m_k2 = k2; }
    float k3() const { return m_k3; }
    void setK3(float k3) { m_k3 = k3; }
    float k4() const { return m_k4; }
    void setK4(float k4) { m_k4 = k4; }

    TextStream &externalRepresentation(TextStream &) const;

private:
    DeprecatedString m_in2;
    KCCompositeOperationType m_operation;
    float m_k1;
    float m_k2;
    float m_k3;
    float m_k4;
};

typedef enum {
    EM_DUPLICATE = 0,
    EM_WRAP = 1,
    EM_NONE = 2
} KCEdgeModeType;

class KCanvasFEConvolveMatrix : public KCanvasFilterEffect
{
public:
    KCanvasFEConvolveMatrix() { };

    FloatSize kernelSize() const { return m_kernelSize; }
    void setKernelSize(FloatSize kernelSize) { m_kernelSize = kernelSize; }
    
    DeprecatedValueList<float> kernel() const { return m_kernelMatrix; }
    void setKernel(DeprecatedValueList<float> kernel) { m_kernelMatrix = kernel; }
    
    float divisor() const { return m_divisor; }
    void setDivisor(float divisor) { m_divisor = divisor; }
    
    float bias() const { return m_bias; }
    void setBias(float bias) { m_bias = bias; }
    
    FloatSize targetOffset() const { return m_targetOffset; }
    void setTargetOffset(FloatSize targetOffset) { m_targetOffset = targetOffset; }
    
    KCEdgeModeType edgeMode() const { return m_edgeMode; }
    void setEdgeMode(KCEdgeModeType edgeMode) { m_edgeMode = edgeMode; }
    
    FloatPoint kernelUnitLength() const {return m_kernelUnitLength; }
    void setKernelUnitLength(FloatPoint kernelUnitLength) { m_kernelUnitLength = kernelUnitLength; }
    
    bool preserveAlpha() const { return m_preserveAlpha; }
    void setPreserveAlpha(bool preserveAlpha) { m_preserveAlpha = preserveAlpha; }

    TextStream &externalRepresentation(TextStream &) const;

private:
    FloatSize m_kernelSize;
    DeprecatedValueList<float> m_kernelMatrix; // maybe should be a real matrix?
    float m_divisor;
    float m_bias;
    FloatSize m_targetOffset;
    KCEdgeModeType m_edgeMode;
    FloatPoint m_kernelUnitLength;
    bool m_preserveAlpha;
};

typedef enum{
    LS_DISTANT,
    LS_POINT,
    LS_SPOT
} KCLightType;

//The light source for Diffuse/SpecularLighting
class KCLightSource
{
public:
    KCLightSource(KCLightType a_type) : m_type(a_type) { }
    
    virtual ~KCLightSource() { }
    
    KCLightType type() const { return m_type; }
    
    virtual TextStream &externalRepresentation(TextStream &) const = 0;
    
private:
    KCLightType m_type;
};

class KCDistantLightSource : public KCLightSource
{
public:
    KCDistantLightSource(float azimuth, float elevation) :
        KCLightSource(LS_DISTANT), m_azimuth(azimuth), m_elevation(elevation) { }
    
    float azimuth() const{ return m_azimuth; }
    float elevation() const{ return m_elevation; }
    
    virtual TextStream &externalRepresentation(TextStream &) const;
    
private:
    float m_azimuth;
    float m_elevation;
};

class KCPointLightSource : public KCLightSource
{
public:
    KCPointLightSource(KCanvasPoint3F& position) : KCLightSource(LS_POINT), m_position(position) { }
    
    const KCanvasPoint3F& position() const { return m_position; }
    
    virtual TextStream &externalRepresentation(TextStream &) const;

private:
    KCanvasPoint3F m_position;
};

class KCSpotLightSource : public KCLightSource
{
public:
    KCSpotLightSource(KCanvasPoint3F& position, KCanvasPoint3F& direction, 
        float specularExponent, float limitingConeAngle) : KCLightSource(LS_SPOT), 
        m_position(position), m_direction(direction), m_specularExponent(specularExponent), m_limitingConeAngle(limitingConeAngle) { }
    
    const KCanvasPoint3F& position() const { return m_position; }
    const KCanvasPoint3F& direction() const { return m_direction; }
    float specularExponent() const { return m_specularExponent; }
    float limitingConeAngle() const { return m_limitingConeAngle; }
    
    virtual TextStream &externalRepresentation(TextStream &) const;
private:
    KCanvasPoint3F m_position;
    KCanvasPoint3F m_direction;
    float          m_specularExponent;
    float          m_limitingConeAngle;
};

class KCanvasFEDiffuseLighting : public KCanvasFilterEffect
{
public:
    KCanvasFEDiffuseLighting() : m_lightSource(0) { }
    ~KCanvasFEDiffuseLighting() { delete m_lightSource; }

    Color lightingColor() const { return m_lightingColor; }
    void setLightingColor(const Color &lightingColor) { m_lightingColor = lightingColor; }

    float surfaceScale() const { return m_surfaceScale; }
    void setSurfaceScale(float surfaceScale) { m_surfaceScale = surfaceScale; }
    
    float diffuseConstant() const { return m_diffuseConstant; }
    void setDiffuseConstant(float diffuseConstant) { m_diffuseConstant = diffuseConstant; }
    
    float kernelUnitLengthX() const { return m_kernelUnitLengthX; }
    void setKernelUnitLengthX(float kernelUnitLengthX) { m_kernelUnitLengthX = kernelUnitLengthX; }
    
    float kernelUnitLengthY() const { return m_kernelUnitLengthY; }
    void setKernelUnitLengthY(float kernelUnitLengthY) { m_kernelUnitLengthY = kernelUnitLengthY; }

    const KCLightSource *lightSource() const { return m_lightSource; }
    void setLightSource(KCLightSource *lightSource);
    
    TextStream &externalRepresentation(TextStream &) const;

private:
    Color m_lightingColor;
    float m_surfaceScale;
    float m_diffuseConstant;
    float m_kernelUnitLengthX;
    float m_kernelUnitLengthY;
    KCLightSource *m_lightSource;
};

typedef enum {
    CS_RED = 0,
    CS_GREEN = 1,
    CS_BLUE = 2,
    CS_ALPHA = 3
} KCChannelSelectorType;

class KCanvasFEDisplacementMap : public KCanvasFilterEffect
{
public:
    KCanvasFEDisplacementMap() : m_xChannelSelector(CS_RED), m_yChannelSelector(CS_RED), m_scale(0) {}
    
    DeprecatedString in2() const { return m_in2; }
    void setIn2(const DeprecatedString &in2) { m_in2 = in2; }
    
    KCChannelSelectorType xChannelSelector() const { return m_xChannelSelector; }
    void setXChannelSelector(const KCChannelSelectorType xChannelSelector) { m_xChannelSelector = xChannelSelector; }
    
    KCChannelSelectorType yChannelSelector() const { return m_yChannelSelector; }
    void setYChannelSelector(const KCChannelSelectorType yChannelSelector) { m_yChannelSelector = yChannelSelector; }

    float scale() const { return m_scale; }
    void setScale(float scale) { m_scale = scale; }
    
    TextStream &externalRepresentation(TextStream &) const;
    
private:
    DeprecatedString m_in2;
    KCChannelSelectorType m_xChannelSelector;
    KCChannelSelectorType m_yChannelSelector;
    float m_scale;
};

class KCanvasFEFlood : public KCanvasFilterEffect
{
public:
    Color floodColor() const { return m_floodColor; }
    void setFloodColor(const Color &color) { m_floodColor = color; }

    float floodOpacity() const { return m_floodOpacity; }
    void setFloodOpacity(float floodOpacity) { m_floodOpacity = floodOpacity; }

    TextStream &externalRepresentation(TextStream &) const;

private:
    Color m_floodColor;
    float m_floodOpacity;
};

class KCanvasFEGaussianBlur : public KCanvasFilterEffect
{
public:
    float stdDeviationX() const;
    void setStdDeviationX(float x);

    float stdDeviationY() const;
    void setStdDeviationY(float y);

    TextStream &externalRepresentation(TextStream &) const;

private:
    float m_x;
    float m_y;
};

class KCanvasFEImage : public KCanvasFilterEffect, public CachedResourceClient
{
public:
    KCanvasFEImage(): m_cachedImage(0) { }
    virtual ~KCanvasFEImage();
    
    // FIXME: Eventually we need to support <svg> (RenderObject *) as well as image data.
    
    CachedImage* cachedImage() const { return m_cachedImage; }
    void setCachedImage(CachedImage* image);

    TextStream &externalRepresentation(TextStream &) const;
    
private:
    CachedImage* m_cachedImage;
};

class KCanvasFEMerge : public KCanvasFilterEffect
{
public:
    DeprecatedStringList mergeInputs() const { return m_mergeInputs; }
    void setMergeInputs(const DeprecatedStringList &mergeInputs) { m_mergeInputs = mergeInputs; }

    TextStream &externalRepresentation(TextStream &) const;
    
private:
    DeprecatedStringList m_mergeInputs;
};

typedef enum {
    MO_ERODE = 0,
    MO_DIALATE = 1
} KCMorphologyOperatorType;

class KCanvasFEMorphology : public KCanvasFilterEffect
{
public:
    KCMorphologyOperatorType morphologyOperator() const { return m_operator; }
    void setMorphologyOperator(KCMorphologyOperatorType _operator) { m_operator = _operator; }

    float radiusX() const { return m_radiusX; }
    void setRadiusX(float radiusX) { m_radiusX = radiusX; }

    float radiusY() const { return m_radiusY; }
    void setRadiusY(float radiusY) { m_radiusY = radiusY; }
    
    TextStream &externalRepresentation(TextStream &) const;
    
private:
    KCMorphologyOperatorType m_operator;
    float m_radiusX;
    float m_radiusY;
};

class KCanvasFEOffset : public KCanvasFilterEffect
{
public:
    float dx() const { return m_dx; }
    void setDx(float dx) { m_dx = dx; }

    float dy() const { return m_dy; }
    void setDy(float dy) { m_dy = dy; }
    
    TextStream &externalRepresentation(TextStream &) const;
    
private:
    float m_dx;
    float m_dy;
};

class KCanvasFESpecularLighting : public KCanvasFilterEffect
{
public:
    KCanvasFESpecularLighting() : m_lightSource(0) { }
    ~KCanvasFESpecularLighting() { delete m_lightSource; }

    Color lightingColor() const { return m_lightingColor; }
    void setLightingColor(const Color &lightingColor) { m_lightingColor = lightingColor; }

    float surfaceScale() const { return m_surfaceScale; }
    void setSurfaceScale(float surfaceScale) { m_surfaceScale = surfaceScale; }
    
    float specularConstant() const { return m_specularConstant; }
    void setSpecularConstant(float specularConstant) { m_specularConstant = specularConstant; }
    
    float specularExponent() const { return m_specularExponent; }
    void setSpecularExponent(float specularExponent) { m_specularExponent = specularExponent; }
    
    float kernelUnitLengthX() const { return m_kernelUnitLengthX; }
    void setKernelUnitLengthX(float kernelUnitLengthX) { m_kernelUnitLengthX = kernelUnitLengthX; }
    
    float kernelUnitLengthY() const { return m_kernelUnitLengthY; }
    void setKernelUnitLengthY(float kernelUnitLengthY) { m_kernelUnitLengthY = kernelUnitLengthY; }
    
    const KCLightSource *lightSource() const { return m_lightSource; }
    void setLightSource(KCLightSource *lightSource);
    
    TextStream &externalRepresentation(TextStream &) const;
    
private:
    Color m_lightingColor;
    float m_surfaceScale;
    float m_specularConstant;
    float m_specularExponent;
    float m_kernelUnitLengthX;
    float m_kernelUnitLengthY;
    KCLightSource *m_lightSource;
};

class KCanvasFETile : public KCanvasFilterEffect { };

typedef enum {
    TT_TURBULANCE = 0,
    TT_NOISE = 1
} KCTurbulanceType;

class KCanvasFETurbulence : public KCanvasFilterEffect
{
public:
    KCTurbulanceType type() const { return m_type; }
    void setType(KCTurbulanceType type) { m_type = type; }

    float baseFrequencyY() const { return m_baseFrequencyY; }
    void setBaseFrequencyY(float baseFrequencyY) { m_baseFrequencyY = baseFrequencyY; }

    float baseFrequencyX() const { return m_baseFrequencyX; }
    void setBaseFrequencyX(float baseFrequencyX) { m_baseFrequencyX = baseFrequencyX; }

    float seed() const { return m_seed; }
    void setSeed(float seed) { m_seed = seed; }

    int numOctaves() const { return m_numOctaves; }
    void setNumOctaves(bool numOctaves) { m_numOctaves = numOctaves; }

    bool stitchTiles() const { return m_stitchTiles; }
    void setStitchTiles(bool stitch) { m_stitchTiles = stitch; }

    TextStream &externalRepresentation(TextStream &) const;
    
private:
    float m_baseFrequencyX;
    float m_baseFrequencyY;
    int m_numOctaves;
    float m_seed;
    bool m_stitchTiles;
    KCTurbulanceType m_type;
};

}

#endif // SVG_SUPPORT

#endif

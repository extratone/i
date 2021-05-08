/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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

#ifndef KCanvasResources_H
#define KCanvasResources_H
#if SVG_SUPPORT

#include <kcanvas/RenderPath.h>
#include <kcanvas/KCanvasPath.h>
#include <kcanvas/KCanvasResourceListener.h>

namespace WebCore {

class TextStream;

// Enumerations
typedef enum
{
    // Painting mode
    RS_CLIPPER = 0,
    RS_MARKER = 1,
    RS_IMAGE = 2,
    RS_FILTER = 3,
    RS_MASKER = 4
} KCResourceType;

class KCanvasMatrix;
class KRenderingPaintServer;

class KCanvasResource
{
public:
    KCanvasResource();
    virtual ~KCanvasResource();

    virtual void invalidate();
    void addClient(const RenderPath *item);

    const KCanvasItemList &clients() const;
    
    DeprecatedString idInRegistry() const;
    void setIdInRegistry(const DeprecatedString& newId);
    
    virtual bool isPaintServer() const { return false; }
    virtual bool isFilter() const { return false; }
    virtual bool isClipper() const { return false; }
    virtual bool isMarker() const { return false; }
    virtual bool isMasker() const { return false; }
    
    virtual TextStream& externalRepresentation(TextStream &) const; 
private:
    KCanvasItemList m_clients;
    DeprecatedString registryId;
};

class KCanvasClipper : public KCanvasResource
{
public:
    KCanvasClipper();
    virtual ~KCanvasClipper();
    
    virtual bool isClipper() const { return true; }

    void resetClipData();
    void addClipData(KCanvasPath *path, KCWindRule rule, bool bboxUnits);
    
    virtual void applyClip(const FloatRect& boundingBox) const = 0;

    KCClipDataList clipData() const;

    TextStream& externalRepresentation(TextStream &) const; 
protected:
    KCClipDataList m_clipData;
};

class KCanvasImage;

class KCanvasMasker : public KCanvasResource
{
public:
    KCanvasMasker();
    virtual ~KCanvasMasker();
    
    virtual bool isMasker() const { return true; }
    void setMask(KCanvasImage *mask);
    KCanvasImage *mask() const { return m_mask; }
    
    virtual void applyMask(const FloatRect& boundingBox) const = 0;

    TextStream& externalRepresentation(TextStream &) const; 
protected:
    KCanvasImage *m_mask;
};

class KCanvasMarker : public KCanvasResource
{
public:
    KCanvasMarker(RenderObject *marker = 0);
    virtual ~KCanvasMarker();
    
    virtual bool isMarker() const { return true; }

    void setMarker(RenderObject *marker);
    
    void setRef(double refX, double refY);
    double refX() const;    
    double refY() const;
    
    void setAngle(float angle);
    void setAutoAngle();
    float angle() const;

    void setUseStrokeWidth(bool useStrokeWidth = true);
    bool useStrokeWidth() const;

    void setScale(float scaleX, float scaleY);
    float scaleX() const;
    float scaleY() const;

    void draw(GraphicsContext*, const FloatRect&, double x, double y, double strokeWidth = 1, double angle = 0);

    TextStream& externalRepresentation(TextStream &) const; 

private:
    double m_refX, m_refY;
    float m_angle, m_scaleX, m_scaleY;
    RenderObject *m_marker;
    bool m_useStrokeWidth;
};

KCanvasResource *getResourceById(Document *document, const AtomicString &id);
KCanvasMarker *getMarkerById(Document *document, const AtomicString &id);
KCanvasClipper *getClipperById(Document *document, const AtomicString &id);
KCanvasMasker *getMaskerById(Document *document, const AtomicString &id);
KRenderingPaintServer *getPaintServerById(Document *document, const AtomicString &id);

TextStream &operator<<(TextStream &ts, const KCanvasResource &r);

}

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet

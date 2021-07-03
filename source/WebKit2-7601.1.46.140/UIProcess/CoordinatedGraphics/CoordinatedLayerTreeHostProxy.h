/*
    Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2013 Company 100, Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef CoordinatedLayerTreeHostProxy_h
#define CoordinatedLayerTreeHostProxy_h

#if USE(COORDINATED_GRAPHICS_MULTIPROCESS)

#include "CoordinatedGraphicsArgumentCoders.h"
#include "CoordinatedGraphicsScene.h"
#include "MessageReceiver.h"
#include <functional>

namespace WebCore {
struct CoordinatedGraphicsState;
class IntSize;
}

namespace WebKit {

class CoordinatedDrawingAreaProxy;

class CoordinatedLayerTreeHostProxy : public CoordinatedGraphicsSceneClient, public IPC::MessageReceiver {
    WTF_MAKE_NONCOPYABLE(CoordinatedLayerTreeHostProxy);
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit CoordinatedLayerTreeHostProxy(CoordinatedDrawingAreaProxy*);
    virtual ~CoordinatedLayerTreeHostProxy();

    void commitCoordinatedGraphicsState(const WebCore::CoordinatedGraphicsState&);

    void setVisibleContentsRect(const WebCore::FloatRect&, const WebCore::FloatPoint& trajectoryVector);
    CoordinatedGraphicsScene* coordinatedGraphicsScene() const { return m_scene.get(); }

    virtual void updateViewport() override;
    virtual void renderNextFrame() override;
    virtual void purgeBackingStores() override;

    virtual void commitScrollOffset(uint32_t layerID, const WebCore::IntSize& offset);

protected:
    void dispatchUpdate(std::function<void()>);

    // IPC::MessageReceiver
    virtual void didReceiveMessage(IPC::Connection&, IPC::MessageDecoder&) override;

    CoordinatedDrawingAreaProxy* m_drawingAreaProxy;
    RefPtr<CoordinatedGraphicsScene> m_scene;
    WebCore::FloatRect m_lastSentVisibleRect;
    WebCore::FloatPoint m_lastSentTrajectoryVector;
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS_MULTIPROCESS)

#endif // CoordinatedLayerTreeHostProxy_h

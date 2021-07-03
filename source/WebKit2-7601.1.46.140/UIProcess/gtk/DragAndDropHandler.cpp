/*
 * Copyright (C) 2014 Igalia S.L.
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

#include "config.h"
#include "DragAndDropHandler.h"

#if ENABLE(DRAG_SUPPORT)

#include "WebPageProxy.h"
#include <WebCore/DataObjectGtk.h>
#include <WebCore/DragData.h>
#include <WebCore/GRefPtrGtk.h>
#include <WebCore/GtkUtilities.h>
#include <WebCore/PasteboardHelper.h>
#include <gtk/gtk.h>
#include <wtf/glib/GMainLoopSource.h>
#include <wtf/glib/GUniquePtr.h>

using namespace WebCore;

namespace WebKit {

DragAndDropHandler::DragAndDropHandler(WebPageProxy& page)
    : m_page(page)
{
}

DragAndDropHandler::DroppingContext::DroppingContext(GdkDragContext* gdkContext, const IntPoint& position)
    : gdkContext(gdkContext)
    , dataObject(DataObjectGtk::create())
    , lastMotionPosition(position)
    , dropHappened(false)
{
}

static inline GdkDragAction dragOperationToGdkDragActions(DragOperation coreAction)
{
    GdkDragAction gdkAction = static_cast<GdkDragAction>(0);
    if (coreAction == DragOperationNone)
        return gdkAction;

    if (coreAction & DragOperationCopy)
        gdkAction = static_cast<GdkDragAction>(GDK_ACTION_COPY | gdkAction);
    if (coreAction & DragOperationMove)
        gdkAction = static_cast<GdkDragAction>(GDK_ACTION_MOVE | gdkAction);
    if (coreAction & DragOperationLink)
        gdkAction = static_cast<GdkDragAction>(GDK_ACTION_LINK | gdkAction);
    if (coreAction & DragOperationPrivate)
        gdkAction = static_cast<GdkDragAction>(GDK_ACTION_PRIVATE | gdkAction);

    return gdkAction;
}

static inline GdkDragAction dragOperationToSingleGdkDragAction(DragOperation coreAction)
{
    if (coreAction == DragOperationEvery || coreAction & DragOperationCopy)
        return GDK_ACTION_COPY;
    if (coreAction & DragOperationMove)
        return GDK_ACTION_MOVE;
    if (coreAction & DragOperationLink)
        return GDK_ACTION_LINK;
    if (coreAction & DragOperationPrivate)
        return GDK_ACTION_PRIVATE;
    return static_cast<GdkDragAction>(0);
}

static inline DragOperation gdkDragActionToDragOperation(GdkDragAction gdkAction)
{
    // We have no good way to detect DragOperationEvery other than
    // to use it when all applicable flags are on.
    if (gdkAction & GDK_ACTION_COPY
        && gdkAction & GDK_ACTION_MOVE
        && gdkAction & GDK_ACTION_LINK
        && gdkAction & GDK_ACTION_PRIVATE)
        return DragOperationEvery;

    unsigned action = DragOperationNone;
    if (gdkAction & GDK_ACTION_COPY)
        action |= DragOperationCopy;
    if (gdkAction & GDK_ACTION_MOVE)
        action |= DragOperationMove;
    if (gdkAction & GDK_ACTION_LINK)
        action |= DragOperationLink;
    if (gdkAction & GDK_ACTION_PRIVATE)
        action |= DragOperationPrivate;
    return static_cast<DragOperation>(action);
}

void DragAndDropHandler::startDrag(const DragData& dragData, PassRefPtr<ShareableBitmap> dragImage)
{
    RefPtr<DataObjectGtk> dataObject = adoptRef(dragData.platformData());
    GRefPtr<GtkTargetList> targetList = adoptGRef(PasteboardHelper::defaultPasteboardHelper()->targetListForDataObject(dataObject.get()));
    GUniquePtr<GdkEvent> currentEvent(gtk_get_current_event());

    GdkDragContext* context = gtk_drag_begin(m_page.viewWidget(), targetList.get(), dragOperationToGdkDragActions(dragData.draggingSourceOperationMask()),
        GDK_BUTTON_PRIMARY, currentEvent.get());
    m_draggingDataObjects.set(context, dataObject.get());

    if (dragImage) {
        RefPtr<cairo_surface_t> image(dragImage->createCairoSurface());
        // Use the center of the drag image as hotspot.
        cairo_surface_set_device_offset(image.get(), -cairo_image_surface_get_width(image.get()) / 2, -cairo_image_surface_get_height(image.get()) / 2);
        gtk_drag_set_icon_surface(context, image.get());
    } else
        gtk_drag_set_icon_default(context);
}

void DragAndDropHandler::fillDragData(GdkDragContext* context, GtkSelectionData* selectionData, unsigned info)
{
    if (DataObjectGtk* dataObject = m_draggingDataObjects.get(context))
        PasteboardHelper::defaultPasteboardHelper()->fillSelectionData(selectionData, info, dataObject);
}

void DragAndDropHandler::finishDrag(GdkDragContext* context)
{
    if (!m_draggingDataObjects.remove(context))
        return;

    GdkDevice* device = gdk_drag_context_get_device(context);
    int x = 0, y = 0;
    gdk_device_get_window_at_position(device, &x, &y);
    int xRoot = 0, yRoot = 0;
    gdk_device_get_position(device, nullptr, &xRoot, &yRoot);
    m_page.dragEnded(IntPoint(x, y), IntPoint(xRoot, yRoot), gdkDragActionToDragOperation(gdk_drag_context_get_selected_action(context)));
}

DataObjectGtk* DragAndDropHandler::dataObjectForDropData(GdkDragContext* context, GtkSelectionData* selectionData, unsigned info, IntPoint& position)
{
    DroppingContext* droppingContext = m_droppingContexts.get(context);
    if (!droppingContext)
        return nullptr;

    droppingContext->pendingDataRequests--;
    PasteboardHelper::defaultPasteboardHelper()->fillDataObjectFromDropData(selectionData, info, droppingContext->dataObject.get());
    if (droppingContext->pendingDataRequests)
        return nullptr;

    // The coordinates passed to drag-data-received signal are sometimes
    // inaccurate in WTR, so use the coordinates of the last motion event.
    position = droppingContext->lastMotionPosition;

    // If there are no more pending requests, start sending dragging data to WebCore.
    return droppingContext->dataObject.get();
}

void DragAndDropHandler::dragEntered(GdkDragContext* context, GtkSelectionData* selectionData, unsigned info, unsigned time)
{
    IntPoint position;
    DataObjectGtk* dataObject = dataObjectForDropData(context, selectionData, info, position);
    if (!dataObject)
        return;

    DragData dragData(dataObject, position, convertWidgetPointToScreenPoint(m_page.viewWidget(), position), gdkDragActionToDragOperation(gdk_drag_context_get_actions(context)));
    m_page.resetCurrentDragInformation();
    m_page.dragEntered(dragData);
    DragOperation operation = m_page.currentDragOperation();
    gdk_drag_status(context, dragOperationToSingleGdkDragAction(operation), time);
}

DataObjectGtk* DragAndDropHandler::requestDragData(GdkDragContext* context, const IntPoint& position, unsigned time)
{
    std::unique_ptr<DroppingContext>& droppingContext = m_droppingContexts.add(context, nullptr).iterator->value;
    if (!droppingContext) {
        GtkWidget* widget = m_page.viewWidget();
        droppingContext = std::make_unique<DroppingContext>(context, position);
        Vector<GdkAtom> acceptableTargets(PasteboardHelper::defaultPasteboardHelper()->dropAtomsForContext(widget, droppingContext->gdkContext));
        droppingContext->pendingDataRequests = acceptableTargets.size();
        for (auto& target : acceptableTargets)
            gtk_drag_get_data(widget, droppingContext->gdkContext, target, time);
    } else
        droppingContext->lastMotionPosition = position;

    // Don't send any drag information to WebCore until we've retrieved all the data for this drag operation.
    // Otherwise we'd have to block to wait for the drag's data.
    if (droppingContext->pendingDataRequests > 0)
        return nullptr;

    return droppingContext->dataObject.get();
}

void DragAndDropHandler::dragMotion(GdkDragContext* context, const IntPoint& position, unsigned time)
{
    DataObjectGtk* dataObject = requestDragData(context, position, time);
    if (!dataObject)
        return;

    DragData dragData(dataObject, position, convertWidgetPointToScreenPoint(m_page.viewWidget(), position), gdkDragActionToDragOperation(gdk_drag_context_get_actions(context)));
    m_page.dragUpdated(dragData);
    DragOperation operation = m_page.currentDragOperation();
    gdk_drag_status(context, dragOperationToSingleGdkDragAction(operation), time);
}

void DragAndDropHandler::dragLeave(GdkDragContext* context)
{
    DroppingContext* droppingContext = m_droppingContexts.get(context);
    if (!droppingContext)
        return;

    // During a drop GTK+ will fire a drag-leave signal right before firing
    // the drag-drop signal. We want the actions for drag-leave to happen after
    // those for drag-drop, so schedule them to happen asynchronously here.
    GMainLoopSource::scheduleAndDeleteOnDestroy("[WebKit] handleDragLeaveLater", [this, droppingContext]() {
        auto it = m_droppingContexts.find(droppingContext->gdkContext);
        if (it == m_droppingContexts.end())
            return;

        // If the view doesn't know about the drag yet (there are still pending data requests),
        // don't update it with information about the drag.
        if (droppingContext->pendingDataRequests)
            return;

        if (!droppingContext->dropHappened) {
            // Don't call dragExited if we have just received a drag-drop signal. This
            // happens in the case of a successful drop onto the view.
            const IntPoint& position = droppingContext->lastMotionPosition;
            DragData dragData(droppingContext->dataObject.get(), position, convertWidgetPointToScreenPoint(m_page.viewWidget(), position), DragOperationNone);
            m_page.dragExited(dragData);
            m_page.resetCurrentDragInformation();
        }

        m_droppingContexts.remove(it);
    });
}

bool DragAndDropHandler::drop(GdkDragContext* context, const IntPoint& position, unsigned time)
{
    DroppingContext* droppingContext = m_droppingContexts.get(context);
    if (!droppingContext)
        return false;

    droppingContext->dropHappened = true;

    DataObjectGtk* dataObject = droppingContext->dataObject.get();
    if (!dataObject)
        return false;

    DragData dragData(dataObject, position, convertWidgetPointToScreenPoint(m_page.viewWidget(), position), gdkDragActionToDragOperation(gdk_drag_context_get_actions(context)));
    SandboxExtension::Handle handle;
    SandboxExtension::HandleArray sandboxExtensionForUpload;
    m_page.performDragOperation(dragData, String(), handle, sandboxExtensionForUpload);
    gtk_drag_finish(context, TRUE, FALSE, time);
    return true;
}

} // namespace WebKit

#endif // ENABLE(DRAG_SUPPORT)

/*
 * Copyright (C) 2011 Samsung Electronics
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef InputMethodContextEfl_h
#define InputMethodContextEfl_h

#include <Ecore_IMF.h>
#include <Evas.h>
#include <wtf/efl/UniquePtrEfl.h>

class EwkView;

namespace WebKit {

class InputMethodContextEfl {
public:
    static std::unique_ptr<InputMethodContextEfl> create(EwkView* viewImpl, Evas* canvas)
    {
        EflUniquePtr<Ecore_IMF_Context> context = createIMFContext(canvas);
        if (!context)
            return nullptr;

        return std::make_unique<InputMethodContextEfl>(viewImpl, WTF::move(context));
    }
    InputMethodContextEfl(EwkView*, EflUniquePtr<Ecore_IMF_Context>);
    ~InputMethodContextEfl();

    void handleMouseUpEvent(const Evas_Event_Mouse_Up* upEvent);
    void handleKeyDownEvent(const Evas_Event_Key_Down* downEvent, bool* isFiltered);
    void updateTextInputState();

private:
    static EflUniquePtr<Ecore_IMF_Context> createIMFContext(Evas* canvas);
    static void onIMFInputSequenceComplete(void* data, Ecore_IMF_Context*, void* eventInfo);
    static void onIMFPreeditSequenceChanged(void* data, Ecore_IMF_Context*, void* eventInfo);

    EwkView* m_view;
    EflUniquePtr<Ecore_IMF_Context> m_context;
    bool m_focused;
};

} // namespace WebKit

#endif // InputMethodContextEfl_h

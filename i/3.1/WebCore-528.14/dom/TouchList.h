/*
 * Copyright (C) 2008, Apple Inc. All rights reserved.
 *
 * No license or rights are granted by Apple expressly or by implication,
 * estoppel, or otherwise, to Apple copyrights, patents, trademarks, trade
 * secrets or other rights.
 */

#ifndef TouchList_h
#define TouchList_h

#include <wtf/Platform.h>

#if ENABLE(TOUCH_EVENTS)

#include <wtf/RefCounted.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>
#include "Touch.h"

namespace WebCore {

    class TouchList : public RefCounted<TouchList> {
    public:
        static PassRefPtr<TouchList> create()
        {
            return adoptRef(new TouchList());
        }
        virtual ~TouchList() {}
        
        virtual bool isTouchList() { return true; }

        unsigned length() const { return m_values.size(); }
        Touch* item (unsigned index) { return index < length() ? m_values[index].get() : 0; }
        
        void append(const PassRefPtr<Touch>);
        
    private:
        TouchList() { }

        Vector<RefPtr<Touch> > m_values;
    };

} // namespace WebCore

#endif // ENABLE(TOUCH_EVENTS)

#endif // TouchList_h

/*
 * Copyright (C) 2008, Apple Inc. All rights reserved.
 *
 * Permission is granted by Apple to use this file to the extent
 * necessary to relink with LGPL WebKit files.
 *
 * No license or rights are granted by Apple expressly or by
 * implication, estoppel, or otherwise, to Apple patents and
 * trademarks. For the sake of clarity, no license or rights are
 * granted by Apple expressly or by implication, estoppel, or otherwise,
 * under any Apple patents, copyrights and trademarks to underlying
 * implementations of any application programming interfaces (APIs)
 * or to any functionality that is invoked by calling any API.
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
    virtual ~TouchList();
    
    virtual bool isTouchList() { return true; }

    unsigned length() const { return m_values.size(); }
    Touch* item (unsigned index) { return index < length() ? m_values[index].get() : 0; }
    
    void append(PassRefPtr<Touch>);
    
private:
    TouchList();

    Vector<RefPtr<Touch> > m_values;
};

} // namespace WebCore

#endif // ENABLE(TOUCH_EVENTS)

#endif /* TouchList_h */

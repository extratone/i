/*
 *  RenderPopUpMenuPurple.h
 *  WebCore
 *
 *  Copyright (C) 2006, 2007, Apple Inc.  All rights reserved.
 *
 */

#ifndef RENDER_POPUPMENU_PURPLE_H
#define RENDER_POPUPMENU_PURPLE_H

#import "config.h"
#import "RenderPopupMenu.h"

namespace WebCore {

class RenderPopupMenuPurple : public RenderPopupMenu {
public:
    RenderPopupMenuPurple(Node*, RenderMenuList*);
    ~RenderPopupMenuPurple();
    
    virtual void clear();
    virtual void populate();
    virtual void showPopup(const IntRect&, FrameView*, int index);
    virtual void hidePopup();
        
protected:
    virtual void addSeparator();
    virtual void addGroupLabel(HTMLOptGroupElement*);
    virtual void addOption(HTMLOptionElement*);
};

}

#endif

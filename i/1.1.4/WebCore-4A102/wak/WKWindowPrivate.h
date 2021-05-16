//
//  WKWindowPrivate.h
//
//  Copyright (C) 2005, 2006, 2007, Apple Inc.  All rights reserved.
//

#import "WKWindow.h"

#ifdef __cplusplus
extern "C" {
#endif    

bool _WKWindowHaveDirtyWindows(void);
void _WKWindowLayoutDirtyWindows(void);
void _WKWindowDrawDirtyWindows(void);
void _WKWindowDidDrawDirtyWindows(void);

#ifdef __cplusplus
}
#endif

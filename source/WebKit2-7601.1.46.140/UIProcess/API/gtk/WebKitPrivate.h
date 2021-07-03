/*
 * Copyright (C) 2011 Igalia S.L.
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

#ifndef WebKitPrivate_h
#define WebKitPrivate_h

#include "WebKitNavigationAction.h"
#include <WebKit/WKAPICast.h>
#include <WebKit/WKDownload.h>
#include <WebKit/WKFindOptions.h>
#include <WebKit/WKFullScreenClientGtk.h>
#include <WebKit/WKGeolocationManager.h>
#include <WebKit/WKGeolocationPermissionRequest.h>
#include <WebKit/WKGeolocationPosition.h>
#include <WebKit/WKIconDatabase.h>
#include <WebKit/WKInspector.h>
#include <WebKit/WKInspectorClientGtk.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKSerializedScriptValue.h>
#include <WebKit/WKSoupCustomProtocolRequestManager.h>
#include <WebKit/WKString.h>
#include <WebKit/WKUserMediaPermissionRequest.h>
#include <WebKit/WebKit2_C.h>
#include <glib.h>
#include <wtf/Assertions.h>

#define WEBKIT_PARAM_READABLE (static_cast<GParamFlags>(G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB))
#define WEBKIT_PARAM_WRITABLE (static_cast<GParamFlags>(G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB))
#define WEBKIT_PARAM_READWRITE (static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB))

#define WEBKIT_DEFINE_ASYNC_DATA_STRUCT(structName) \
static structName* create##structName() \
{ \
    structName* data = g_slice_new0(structName); \
    new (data) structName(); \
    return data; \
} \
static void destroy##structName(structName* data) \
{ \
    data->~structName(); \
    g_slice_free(structName, data); \
}

#define WEBKIT_DEFINE_TYPE(TypeName, type_name, TYPE_PARENT) _WEBKIT_DEFINE_TYPE_EXTENDED(TypeName, type_name, TYPE_PARENT, 0, { })
#define WEBKIT_DEFINE_ABSTRACT_TYPE(TypeName, type_name, TYPE_PARENT) _WEBKIT_DEFINE_TYPE_EXTENDED(TypeName, type_name, TYPE_PARENT, G_TYPE_FLAG_ABSTRACT, { })
#define WEBKIT_DEFINE_TYPE_WITH_CODE(TypeName, type_name, TYPE_PARENT, Code) _WEBKIT_DEFINE_TYPE_EXTENDED_BEGIN(TypeName, type_name, TYPE_PARENT, 0) {Code;} _WEBKIT_DEFINE_TYPE_EXTENDED_END()

#define _WEBKIT_DEFINE_TYPE_EXTENDED(TypeName, type_name, TYPE_PARENT, flags, Code) _WEBKIT_DEFINE_TYPE_EXTENDED_BEGIN(TypeName, type_name, TYPE_PARENT, flags) {Code;} _WEBKIT_DEFINE_TYPE_EXTENDED_END()
#define _WEBKIT_DEFINE_TYPE_EXTENDED_BEGIN(TypeName, type_name, TYPE_PARENT, flags) \
\
static void type_name##_class_init(TypeName##Class* klass); \
static gpointer type_name##_parent_class = 0; \
static void type_name##_finalize(GObject* object) \
{ \
    TypeName* self = (TypeName*)object; \
    self->priv->~TypeName##Private(); \
    G_OBJECT_CLASS(type_name##_parent_class)->finalize(object); \
} \
\
static void type_name##_class_intern_init(gpointer klass) \
{ \
    GObjectClass* gObjectClass = G_OBJECT_CLASS(klass); \
    g_type_class_add_private(klass, sizeof(TypeName##Private)); \
    type_name##_parent_class = g_type_class_peek_parent(klass); \
    type_name##_class_init((TypeName##Class*)klass); \
    gObjectClass->finalize = type_name##_finalize; \
} \
\
static void type_name##_init(TypeName* self) \
{ \
    TypeName##Private* priv = G_TYPE_INSTANCE_GET_PRIVATE(self, type_name##_get_type(), TypeName##Private); \
    self->priv = priv; \
    new (priv) TypeName##Private(); \
}\
GType type_name##_get_type(void) \
{ \
    static volatile gsize g_define_type_id__volatile = 0; \
    if (g_once_init_enter(&g_define_type_id__volatile)) { \
        GType g_define_type_id = \
            g_type_register_static_simple( \
                TYPE_PARENT, \
                g_intern_static_string(#TypeName), \
                sizeof(TypeName##Class), \
                (GClassInitFunc)type_name##_class_intern_init, \
                sizeof(TypeName), \
                (GInstanceInitFunc)type_name##_init, \
                (GTypeFlags)flags); \
        // Custom code follows.
#define _WEBKIT_DEFINE_TYPE_EXTENDED_END() \
        g_once_init_leave(&g_define_type_id__volatile, g_define_type_id); \
    } \
    return g_define_type_id__volatile; \
} // Closes type_name##_get_type().

unsigned wkEventModifiersToGdkModifiers(WKEventModifiers);
unsigned wkEventMouseButtonToWebKitMouseButton(WKEventMouseButton);
unsigned toGdkModifiers(WebKit::WebEvent::Modifiers);
WebKitNavigationType toWebKitNavigationType(WebCore::NavigationType);
unsigned toWebKitMouseButton(WebKit::WebMouseEvent::Button);
unsigned toWebKitError(unsigned webCoreError);
unsigned toWebCoreError(unsigned webKitError);

enum SnapshotRegion {
    SnapshotRegionVisible,
    SnapshotRegionFullDocument
};

#if ENABLE(NETWORK_CACHE)
static const char networkCacheSubdirectory[] = "WebKitCache";
#else
static const char networkCacheSubdirectory[] = "webkit";
#endif

#endif // WebKitPrivate_h

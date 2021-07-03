/*
 * Copyright (C) 2014 Samsung Electronics
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

/**
 * @file    ewk_application_cache_manager.h
 * @brief   Describes the Ewk Application Cache Manager API.
 */

#ifndef ewk_application_cache_manager_h
#define ewk_application_cache_manager_h

#include "ewk_security_origin.h"
#include <Eina.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Creates a type name for Ewk_Application_Cache_Manager */
#ifdef __cplusplus
typedef class EwkApplicationCacheManager Ewk_Application_Cache_Manager;
#else
typedef struct EwkApplicationCacheManager Ewk_Application_Cache_Manager;
#endif

/**
 * @typedef Ewk_Application_Cache_Origins_Async_Get_Cb Ewk_Application_Cache_Origins_Async_Get_Cb
 * @brief Callback for ewk_application_cache_manager_origins_async_get
 *
 * @note The @a origins should be freed like below code after use.
 *
 * @code
 *
 * static void
 * _origins_get_cb(Eina_List* origins, data)
 * {
 *    // ...
 *
 *    void *origin;
 *    EINA_LIST_FREE(origins, origin)
 *      ewk_object_unref((Ewk_Object*)origin);
 * }
 *
 * @endcode
 */
typedef void (*Ewk_Application_Cache_Origins_Async_Get_Cb)(Eina_List *origins, void *user_data);

/**
 * Requests for getting web application cache origins.
 *
 * @param manager Ewk_Application_Cache_Manager object
 * @param callback callback to get web application cache origins
 * @param user_data user_data will be passsed when result_callback is called
 *
 * @see Ewk_Application_Cache_Origins_Async_Get_Cb
 */
EAPI void ewk_application_cache_manager_origins_async_get(const Ewk_Application_Cache_Manager *manager, Ewk_Application_Cache_Origins_Async_Get_Cb callback, void *user_data);

/**
 * Deletes all web application caches.
 *
 * @param manager Ewk_Application_Cache_Manager object
 *
 * @return @c EINA_TRUE on successful request or @c EINA FALSE on failure
 */
EAPI Eina_Bool ewk_application_cache_manager_clear(Ewk_Application_Cache_Manager *manager);

/**
 * Deletes web application caches for the specified origin.
 *
 * @param manager Ewk_Application_Cache_Manager object
 * @param origin security origin
 *
 * @return @c EINA_TRUE on successful request or @c EINA_FALSE on failure
 */
EAPI Eina_Bool ewk_application_cache_manager_entries_for_origin_del(Ewk_Application_Cache_Manager *manager, Ewk_Security_Origin *origin);

#ifdef __cplusplus
}
#endif

#endif // ewk_application_cache_manager_h

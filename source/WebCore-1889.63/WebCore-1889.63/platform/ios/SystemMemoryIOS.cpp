/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SystemMemory.h"

#include <mach/mach.h>
#include <sys/sysctl.h>
#include "wtf/Assertions.h"
#include "wtf/CurrentTime.h"

namespace WebCore {

static const unsigned int lowMemoryThreshold = 6 * 1024 * 1024;

static host_basic_info_data_t gHostBasicInfo;
static pthread_once_t initControl = PTHREAD_ONCE_INIT;

static void initCapabilities(void)
{
    mach_msg_type_number_t  count;
    kern_return_t r;
    mach_port_t host;
    
    /* Discover our CPU type */
    host = mach_host_self();
    count = HOST_BASIC_INFO_COUNT;
    r = host_info(host, HOST_BASIC_INFO, (host_info_t) &gHostBasicInfo, &count);
    mach_port_deallocate(mach_task_self(), host);
    if (r != KERN_SUCCESS) {
        LOG_ERROR("%s : host_info(%d) : %s.\n", __FUNCTION__, r, mach_error_string(r));
    }
}

bool hasEnoughMemoryFor(unsigned bytes)
{
    kern_return_t err;
#if PLATFORM(IOS_SIMULATOR)
    struct task_basic_info stats;
    mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;
    err = task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&stats, &count);
    if (err != KERN_SUCCESS) {
        ASSERT_NOT_REACHED();
        return true;
    }
    
    const vm_size_t highMemoryThreshold = 128 * 1024 * 1024;
    return (stats.resident_size + bytes) < highMemoryThreshold;
#else
    vm_statistics_data_t stats;
    mach_msg_type_number_t count = HOST_VM_INFO_COUNT;
    err = host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)&stats, &count);
    if (err != KERN_SUCCESS) {
        ASSERT_NOT_REACHED();
        return true;
    }
    unsigned long long available = (stats.free_count + stats.inactive_count + stats.purgeable_count) * PAGE_SIZE;
    return available - bytes > lowMemoryThreshold;
#endif
}

int systemMemoryLevel()
{
#if PLATFORM(IOS_SIMULATOR)
    return 35;
#endif
    static int memoryFreeLevel = -1;
    static double previousCheckTime; 
    double time = currentTime();
    if (time - previousCheckTime < .1)
        return memoryFreeLevel;
    previousCheckTime = time;
    size_t size = sizeof(memoryFreeLevel);
    sysctlbyname("kern.memorystatus_level", &memoryFreeLevel, &size, NULL, 0);
    return memoryFreeLevel;
}

size_t systemTotalMemory()
{
#if PLATFORM(IOS_SIMULATOR)
    return 512 * 1024 * 1024;
#endif
    pthread_once(&initControl, initCapabilities);
    // The value in gHostBasicInfo.max_mem is often lower than the amount we're
    // interested in (e.g., does this device have at least 256MB of RAM?)
    // Round the value to the nearest power of 2.
    return (size_t)exp2(ceil(log2(gHostBasicInfo.max_mem)));
}

}

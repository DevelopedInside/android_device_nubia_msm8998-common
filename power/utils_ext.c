/*
 * Copyright (c) 2012-2013,2015-2016, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * *    * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#define LOG_NIDEBUG 0

#include <dlfcn.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#include "utils_ext.h"
#include "power-common.h"
#define LOG_TAG "QCOM PowerHAL-Ext"
#include <utils/Log.h>

static void *qcopt_handle;
static int (*perf_lock_acq)(unsigned long handle, int duration,
    int list[], int numArgs);
static int (*perf_lock_rel)(unsigned long handle);

static void *get_qcopt_handle()
{
    char qcopt_lib_path[PATH_MAX] = {0};
    void *handle = NULL;

    dlerror();

    if (property_get("ro.vendor.extension_library", qcopt_lib_path,
                NULL)) {
        handle = dlopen(qcopt_lib_path, RTLD_NOW);
        if (!handle) {
            ALOGE("Unable to open %s: %s\n", qcopt_lib_path,
                    dlerror());
        }
    }

    return handle;
}

static void __attribute__ ((constructor)) initialize(void)
{
    qcopt_handle = get_qcopt_handle();

    if (!qcopt_handle) {
        ALOGE("Failed to get qcopt handle.\n");
    } else {
        /*
         * qc-opt handle obtained. Get the perflock acquire/release
         * function pointers.
         */
        perf_lock_acq = dlsym(qcopt_handle, "perf_lock_acq");
        if (!perf_lock_acq) {
            goto fail_qcopt;
        }

        perf_lock_rel = dlsym(qcopt_handle, "perf_lock_rel");
        if (!perf_lock_rel) {
            goto fail_qcopt;
        }
    }
    return;

fail_qcopt:
    perf_lock_acq = NULL;
    perf_lock_rel = NULL;
    if (qcopt_handle) {
        dlclose(qcopt_handle);
        qcopt_handle = NULL;
    }
}

static void __attribute__ ((destructor)) cleanup(void)
{
    if (qcopt_handle) {
        if (dlclose(qcopt_handle))
            ALOGE("Error occurred while closing qc-opt library.");
    }
}

int interaction_with_handle(int lock_handle, int duration, int num_args, int opt_list[])
{
    if (duration < 0 || num_args < 1 || opt_list[0] == NULL)
        return 0;

    if (qcopt_handle) {
        if (perf_lock_acq) {
            lock_handle = perf_lock_acq(lock_handle, duration, opt_list, num_args);
            if (lock_handle == -1)
                ALOGE("Failed to acquire lock.");
        }
    }
    return lock_handle;
}

void release_request(int lock_handle) {
    if (qcopt_handle && perf_lock_rel)
        perf_lock_rel(lock_handle);
}

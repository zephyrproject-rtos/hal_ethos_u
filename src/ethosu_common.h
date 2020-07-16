/*
 * Copyright (c) 2019-2020 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if !defined ETHOSU_COMMON_H
#define ETHOSU_COMMON_H

#include "ethosu55_interface.h"
#include "ethosu_device.h"

#include <stdio.h>

#if defined(DRIVER_LOG_SEVERITY) && (DRIVER_LOG_SEVERITY >= LOG_SEVERITY_EMERG)
#define LOG_EMERG(format, ...)                                                                                         \
    fprintf(stderr, format, ##__VA_ARGS__);                                                                            \
    fflush(stderr);                                                                                                    \
    exit(-1)
#else
#define LOG_EMERG(format, ...)
#endif

#if defined(DRIVER_LOG_SEVERITY) && (DRIVER_LOG_SEVERITY >= LOG_SEVERITY_ALERT)
#define LOG_ALERT(format, ...)                                                                                         \
    fprintf(stderr, format, ##__VA_ARGS__);                                                                            \
    fflush(stderr);                                                                                                    \
    exit(-1)
#else
#define LOG_ALERT(format, ...)
#endif

#if defined(DRIVER_LOG_SEVERITY) && (DRIVER_LOG_SEVERITY >= LOG_SEVERITY_CRIT)
#define LOG_CRIT(format, ...)                                                                                          \
    fprintf(stderr, format, ##__VA_ARGS__);                                                                            \
    fflush(stderr);                                                                                                    \
    exit(-1)
#else
#define LOG_CRIT(format, ...)
#endif

#if defined(DRIVER_LOG_SEVERITY) && (DRIVER_LOG_SEVERITY >= LOG_SEVERITY_ERR)
#define LOG_ERR(format, ...)                                                                                           \
    fprintf(stderr, format, ##__VA_ARGS__);                                                                            \
    fflush(stderr)
#else
#define LOG_ERR(format, ...)
#endif

#if defined(DRIVER_LOG_SEVERITY) && (DRIVER_LOG_SEVERITY >= LOG_SEVERITY_WARN)
#define LOG_WARN(format, ...) fprintf(stdout, format, ##__VA_ARGS__)
#else
#define LOG_WARN(format, ...)
#endif

#if defined(DRIVER_LOG_SEVERITY) && (DRIVER_LOG_SEVERITY >= LOG_SEVERITY_NOTICE)
#define LOG_NOTICE(format, ...) fprintf(stdout, format, ##__VA_ARGS__)
#else
#define LOG_NOTICE(format, ...)
#endif

#if defined(DRIVER_LOG_SEVERITY) && (DRIVER_LOG_SEVERITY >= LOG_SEVERITY_INFO)
#define LOG_INFO(format, ...) fprintf(stdout, format, ##__VA_ARGS__)
#else
#define LOG_INFO(format, ...)
#endif

#if defined(DRIVER_LOG_SEVERITY) && (DRIVER_LOG_SEVERITY >= LOG_SEVERITY_DEBUG)
#define LOG_DEBUG(format, ...) fprintf(stdout, format, ##__VA_ARGS__)
#else
#define LOG_DEBUG(format, ...)
#endif

#if defined(ASSERT_DISABLE)
#define ASSERT(args)
#else
#define ASSERT(args) assert(args)
#endif

#define UNUSED(x) ((void)x)

#define VER_STR(X) VNUM_STR(X)
#define VNUM_STR(X) #X

#define MASK_0_31_BITS (0xFFFFFFFF)
#define MASK_32_47_BITS (0xFFFF00000000)

static const __attribute__((section("npu_driver_version"))) char driver_version_str[] = VER_STR(
    ETHOSU_DRIVER_VERSION_MAJOR) "." VER_STR(ETHOSU_DRIVER_VERSION_MINOR) "." VER_STR(ETHOSU_DRIVER_VERSION_PATCH);

static const __attribute__((section("npu_driver_arch_version"))) char driver_arch_version_str[] =
    VER_STR(NNX_ARCH_VERSION_MAJOR) "." VER_STR(NNX_ARCH_VERSION_MINOR) "." VER_STR(NNX_ARCH_VERSION_PATCH);

#endif // ETHOSU_COMMON_H

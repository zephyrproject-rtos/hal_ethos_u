/*
 * Copyright (c) 2021 Arm Limited. All rights reserved.
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

#ifndef ETHOSU_LOG_H
#define ETHOSU_LOG_H

/******************************************************************************
 * Includes
 ******************************************************************************/

#include <stdio.h>
#include <string.h>

/******************************************************************************
 * Defines
 ******************************************************************************/

// Log severity levels
#define ETHOSU_LOG_ERR 0
#define ETHOSU_LOG_WARN 1
#define ETHOSU_LOG_INFO 2
#define ETHOSU_LOG_DEBUG 3

// Define default log severity
#ifndef ETHOSU_LOG_SEVERITY
#define ETHOSU_LOG_SEVERITY ETHOSU_LOG_WARN
#endif

// Log formatting

#define LOG(f, ...) fprintf(stdout, f, ##__VA_ARGS__)

#if ETHOSU_LOG_SEVERITY >= ETHOSU_LOG_ERR
#define LOG_ERR_N(f, ...) fprintf(stderr, f, ##__VA_ARGS__)
#define LOG_ERR(f, ...) LOG_ERR_N("E: " f " (%s:%d)", ##__VA_ARGS__, strrchr("/" __FILE__, '/') + 1, __LINE__)
#else
#define LOG_ERR(f, ...)
#define LOG_ERR_N(f, ...)
#endif

#if ETHOSU_LOG_SEVERITY >= ETHOSU_LOG_WARN
#define LOG_WARN_N(f, ...) fprintf(stdout, f, ##__VA_ARGS__)
#define LOG_WARN(f, ...) LOG_WARN_N("W: " f, ##__VA_ARGS__)
#else
#define LOG_WARN(f, ...)
#define LOG_WARN_N(f, ...)
#endif

#if ETHOSU_LOG_SEVERITY >= ETHOSU_LOG_INFO
#define LOG_INFO_N(f, ...) fprintf(stdout, f, ##__VA_ARGS__)
#define LOG_INFO(f, ...) LOG_INFO_N("I: " f, ##__VA_ARGS__)
#else
#define LOG_INFO(f, ...)
#define LOG_INFO_N(f, ...)
#endif

#if ETHOSU_LOG_SEVERITY >= ETHOSU_LOG_DEBUG
#define LOG_DEBUG_N(f, ...) fprintf(stdout, f, ##__VA_ARGS__)
#define LOG_DEBUG(f, ...) LOG_DEBUG_N("D: %s(): " f, __FUNCTION__, ##__VA_ARGS__)
#else
#define LOG_DEBUG(f, ...)
#define LOG_DEBUG_N(f, ...)
#endif

#endif
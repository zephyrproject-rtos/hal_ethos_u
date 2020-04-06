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
#ifndef ethosu_pmu_common_h
#define ethosu_pmu_common_h

#include <stdint.h>

/* Separators for EXPAND_(name) macros */
#define COMMA ,
#define SEMICOLON ;
#define COLON :
#define NOTHING

#if defined(CPU_CORTEX_M55)
#define NPU_BASE ((uint32_t)0x41700000)
#else
#define NPU_BASE ((uint32_t)0x41105000)
#endif

#define ETHOSU_PMU_CTRL_BASE (NPU_BASE + ((uint32_t)0x180))
#define ETHOSU_PMU_CNTR_BASE (NPU_BASE + ((uint32_t)0x300))
#define ETHOSU_PMU_EVNT_BASE (NPU_BASE + ((uint32_t)0x380))

#define __init __attribute__((constructor))
#define __exit __attribute__((destructor))

#ifdef NEVER
#define INIT __init void
#define EXIT __exit void
#else
#define INIT static __init void
#define EXIT static __init void
#endif

#endif // mpu_pmu_common_h

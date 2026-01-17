/*
 * SPDX-FileCopyrightText: <text>Copyright 2019-2021, 2024, 2026 Arm Limited and/or its affiliates
 * <open-source-office@arm.com></text>
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

#ifndef ETHOSU_TYPES_H
#define ETHOSU_TYPES_H

/******************************************************************************
 * Includes
 ******************************************************************************/

#include <stdint.h>

/******************************************************************************
 * Defines
 ******************************************************************************/

#define ETHOSU_PRODUCT_U55 0U
#define ETHOSU_PRODUCT_U65 1U
#define ETHOSU_PRODUCT_U85 2U

#define ETHOSU_MACS_32 5U
#define ETHOSU_MACS_64 6U
#define ETHOSU_MACS_128 7U
#define ETHOSU_MACS_256 8U
#define ETHOSU_MACS_512 9U
#define ETHOSU_MACS_1024 10U
#define ETHOSU_MACS_2048 11U
/******************************************************************************
 * Types
 ******************************************************************************/

enum ethosu_clock_q_request
{
    ETHOSU_CLOCK_Q_DISABLE   = 0, ///< Disable NPU signal ready for clock off.
    ETHOSU_CLOCK_Q_ENABLE    = 1, ///< Enable NPU signal ready for clock off when stop+idle state reached.
    ETHOSU_CLOCK_Q_UNCHANGED = 2  ///< Keep current clock q setting
};

enum ethosu_power_q_request
{
    ETHOSU_POWER_Q_DISABLE   = 0, ///< Disable NPU signal ready for power off.
    ETHOSU_POWER_Q_ENABLE    = 1, ///< Enable NPU signal ready for power off when stop+idle state reached.
    ETHOSU_POWER_Q_UNCHANGED = 2  ///< Keep current power q setting
};

struct ethosu_id
{
    uint32_t version_status; ///< Version status
    uint32_t version_minor;  ///< Version minor
    uint32_t version_major;  ///< Version major
    uint32_t product_major;  ///< Product major
    uint32_t arch_patch_rev; ///< Architecture version patch
    uint32_t arch_minor_rev; ///< Architecture version minor
    uint32_t arch_major_rev; ///< Architecture version major
};

struct ethosu_config
{
    uint32_t macs_per_cc;        ///< MACs per clock cycle
    uint32_t cmd_stream_version; ///< NPU command stream version
    uint32_t custom_dma;         ///< Custom DMA enabled
};

struct ethosu_hw_info
{
    struct ethosu_id version;
    struct ethosu_config cfg;
};
#endif // ETHOSU_TYPES_H

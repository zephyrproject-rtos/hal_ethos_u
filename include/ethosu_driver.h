/*
 * Copyright (c) 2019-2021 Arm Limited. All rights reserved.
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

#ifndef ETHOSU_DRIVER_H
#define ETHOSU_DRIVER_H

/******************************************************************************
 * Includes
 ******************************************************************************/

#include "ethosu_device.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * Types
 ******************************************************************************/

struct ethosu_driver
{
    struct ethosu_device dev;
    bool abort_inference;
    uint64_t fast_memory;
    size_t fast_memory_size;
    bool status_error;
    bool dev_power_always_on;
    struct ethosu_driver *next;
    bool reserved;
    volatile bool irq_triggered;
    void *semaphore;
};

struct ethosu_version_id
{
    // Ethos-U id
    uint8_t version_status;
    uint8_t version_minor;
    uint8_t version_major;
    uint8_t product_major;
    uint8_t arch_patch_rev;
    uint8_t arch_minor_rev;
    uint8_t arch_major_rev;

    // Driver Version
    uint8_t driver_patch_rev;
    uint8_t driver_minor_rev;
    uint8_t driver_major_rev;
};

struct ethosu_version_config
{
    uint8_t macs_per_cc;
    uint8_t cmd_stream_version;
    uint8_t shram_size;
};

struct ethosu_version
{
    struct ethosu_version_id id;
    struct ethosu_version_config cfg;
};

/******************************************************************************
 * Variables
 ******************************************************************************/

extern struct ethosu_driver ethosu_drv;

/******************************************************************************
 * Prototypes
 ******************************************************************************/

/**
 * Initialize the Ethos-U driver.
 */
int ethosu_init_v4(struct ethosu_driver *drv,
                   const void *base_address,
                   const void *fast_memory,
                   const size_t fast_memory_size,
                   uint32_t secure_enable,
                   uint32_t privilege_enable);

#define ethosu_init(base_address) ethosu_init_v3(base_address, NULL, 0, 0, 0)
#define ethosu_init_v2(base_address, fast_memory, fast_memory_size)                                                    \
    ethosu_init_v3(base_address, fast_memory, fast_memory_size, 0, 0)
#define ethosu_init_v3(base_address, fast_memory, fast_memory_size, secure_enable, privilege_enable)                   \
    ethosu_init_v4(&ethosu_drv, base_address, fast_memory, fast_memory_size, secure_enable, privilege_enable)

/**
 * Get Ethos-U version.
 */
int ethosu_get_version_v2(struct ethosu_driver *drv, struct ethosu_version *version);

#define ethosu_get_version(version) ethosu_get_version_v2(&ethosu_drv, version)

/**
 * Invoke Vela command stream.
 */
int ethosu_invoke_v3(struct ethosu_driver *drv,
                     const void *custom_data_ptr,
                     const int custom_data_size,
                     const uint64_t *base_addr,
                     const size_t *base_addr_size,
                     const int num_base_addr);

#define ethosu_invoke(custom_data_ptr, custom_data_size, base_addr, num_base_addr)                                     \
    ethosu_invoke_v2(custom_data_ptr, custom_data_size, base_addr, NULL, num_base_addr)

/**
 * Abort Ethos-U inference.
 */
void ethosu_abort_v2(struct ethosu_driver *drv);

#define ethosu_abort(void) ethosu_abort_v2(&ethosu_drv)

/**
 * Interrupt handler do be called on IRQ from Ethos-U
 */
void ethosu_irq_handler_v2(struct ethosu_driver *drv);

#define ethosu_irq_handler(void) ethosu_irq_handler_v2(&ethosu_drv)

/**
 * Set Ethos-U power mode.
 */
void ethosu_set_power_mode_v2(struct ethosu_driver *drv, bool always_on);

#define ethosu_set_power_mode(always_on) ethosu_set_power_mode_v2(&ethosu_drv, always_on)

/**
 *  Register a driver for multiNPU usage
 */
int ethosu_register_driver(struct ethosu_driver *drv);

/**
 * Deregister a driver from multiNPU usage
 */
int ethosu_deregister_driver(struct ethosu_driver *drv);

/**
 * Reserves a driver to execute inference with
 */
struct ethosu_driver *ethosu_reserve_driver(void);

/**
 * Change driver status to available
 */
void ethosu_release_driver(struct ethosu_driver *drv);

/**
 * Static inline for backwards-compatibility
 */
static inline int ethosu_invoke_v2(const void *custom_data_ptr,
                                   const int custom_data_size,
                                   const uint64_t *base_addr,
                                   const size_t *base_addr_size,
                                   const int num_base_addr)
{
    struct ethosu_driver *drv = ethosu_reserve_driver();
    int result = ethosu_invoke_v3(drv, custom_data_ptr, custom_data_size, base_addr, base_addr_size, num_base_addr);
    ethosu_release_driver(drv);
    return result;
}

#ifdef __cplusplus
}
#endif

#endif // ETHOSU_DRIVER_H

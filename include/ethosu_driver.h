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
 * Defines
 ******************************************************************************/

#define ETHOSU_DRIVER_VERSION_MAJOR 0  ///< Driver major version
#define ETHOSU_DRIVER_VERSION_MINOR 16 ///< Driver minor version
#define ETHOSU_DRIVER_VERSION_PATCH 0  ///< Driver patch version

/******************************************************************************
 * Types
 ******************************************************************************/

struct ethosu_driver
{
    struct ethosu_device dev;
    struct ethosu_driver *next;
    void *semaphore;
    uint64_t fast_memory;
    size_t fast_memory_size;
    bool status_error;
    bool abort_inference;
    bool dev_power_always_on;
    bool reserved;
    volatile bool irq_triggered;
    uint8_t clock_request;
    uint8_t power_request;
};

struct ethosu_driver_version
{
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
};

struct ethosu_hw_info
{
    struct ethosu_id version;
    struct ethosu_config cfg;
};

enum ethosu_request_clients
{
    ETHOSU_PMU_REQUEST       = 0,
    ETHOSU_INFERENCE_REQUEST = 1,
};

/******************************************************************************
 * Prototypes (weak functions in driver)
 ******************************************************************************/

/**
 * Interrupt handler to be called on IRQ from Ethos-U
 */
void ethosu_irq_handler(struct ethosu_driver *drv);

/*
 * Flush/clean the data cache by address and size. Passing NULL as p argument
 * expects the whole cache to be flushed.
 */

void ethosu_flush_dcache(uint32_t *p, size_t bytes);
/*
 * Invalidate the data cache by address and size. Passing NULL as p argument
 * expects the whole cache to be invalidated.
 */
void ethosu_invalidate_dcache(uint32_t *p, size_t bytes);

/*
 * Minimal sempahore and mutex implementation for baremetal applications. See
 * ethosu_driver.c.
 */
void *ethosu_mutex_create(void);
void ethosu_mutex_lock(void *mutex);
void ethosu_mutex_unlock(void *mutex);
void *ethosu_semaphore_create(void);
void ethosu_semaphore_take(void *sem);
void ethosu_semaphore_give(void *sem);

/*
 * Callbacks for begin/end of inference. inference_data pointer is set in the
 * ethosu_invoke() call, referenced as custom_data_ptr.
 */
void ethosu_inference_begin(struct ethosu_driver *drv, const void *inference_data);
void ethosu_inference_end(struct ethosu_driver *drv, const void *inference_data);

/******************************************************************************
 * Prototypes
 ******************************************************************************/

/**
 * Initialize the Ethos-U driver.
 */
int ethosu_init(struct ethosu_driver *drv,
                const void *base_address,
                const void *fast_memory,
                const size_t fast_memory_size,
                uint32_t secure_enable,
                uint32_t privilege_enable);

/**
 * Deinitialize the Ethos-U driver.
 */
void ethosu_deinit(struct ethosu_driver *drv);

/**
 * Get Ethos-U driver version.
 */
void ethosu_get_driver_version(struct ethosu_driver_version *ver);

/**
 * Get Ethos-U hardware information.
 */
void ethosu_get_hw_info(struct ethosu_driver *drv, struct ethosu_hw_info *hw);

/**
 * Invoke Vela command stream.
 */
int ethosu_invoke(struct ethosu_driver *drv,
                  const void *custom_data_ptr,
                  const int custom_data_size,
                  const uint64_t *base_addr,
                  const size_t *base_addr_size,
                  const int num_base_addr);

/**
 * Abort Ethos-U inference.
 */
void ethosu_abort(struct ethosu_driver *drv);

/**
 * Set Ethos-U power mode.
 */
void ethosu_set_power_mode(struct ethosu_driver *drv, bool always_on);

/**
 * Reserves a driver to execute inference with
 */
struct ethosu_driver *ethosu_reserve_driver(void);

/**
 * Change driver status to available
 */
void ethosu_release_driver(struct ethosu_driver *drv);

/**
 * Set clock and power request bits
 */
enum ethosu_error_codes set_clock_and_power_request(struct ethosu_driver *drv,
                                                    enum ethosu_request_clients client,
                                                    enum ethosu_clock_q_request clock_request,
                                                    enum ethosu_power_q_request power_request);

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
    int result = ethosu_invoke(drv, custom_data_ptr, custom_data_size, base_addr, base_addr_size, num_base_addr);
    ethosu_release_driver(drv);
    return result;
}

#ifdef __cplusplus
}
#endif

#endif // ETHOSU_DRIVER_H

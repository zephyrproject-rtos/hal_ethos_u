/*
 * SPDX-FileCopyrightText: Copyright 2019-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
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

/******************************************************************************
 * Includes
 ******************************************************************************/

#include "ethosu_driver.h"
#include "ethosu_device.h"
#include "ethosu_log.h"

#ifndef ETHOSU_MULTI_VARIANT
#if defined(ETHOSU55)
#include "ethosu_config_u55.h"
#elif defined(ETHOSU65)
#include "ethosu_config_u65.h"
#elif defined(ETHOSU85)
#include "ethosu_config_u85.h"
#endif
#endif

#include <assert.h>
#include <cmsis_compiler.h>
#ifndef __ARMCC_VERSION
#include <sys/types.h>
#endif
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/******************************************************************************
 * Defines
 ******************************************************************************/

#define UNUSED(x) ((void)x)

#define MASK_16_BYTE_ALIGN (0xF)
#define OPTIMIZER_CONFIG_LENGTH_32_BIT_WORD 2
#define DRIVER_ACTION_LENGTH_32_BIT_WORD 1
#define ETHOSU_FOURCC ('1' << 24 | 'P' << 16 | 'O' << 8 | 'C') // "Custom Operator Payload 1"

#define FAST_MEMORY_BASE_ADDR_INDEX 2

/******************************************************************************
 * Types
 ******************************************************************************/

// Driver actions
enum DRIVER_ACTION_e
{
    RESERVED         = 0,
    OPTIMIZER_CONFIG = 1,
    COMMAND_STREAM   = 2,
    NOP              = 5,
};

// Custom operator payload data struct
struct cop_data_s
{
    union
    {
        // Driver action data
        struct
        {
            uint8_t driver_action_command; // (valid values in DRIVER_ACTION_e)
            uint8_t reserved;

            // Driver action data
            union
            {
                // DA_CMD_OPT_CFG
                struct
                {
                    uint16_t rel_nbr : 4;
                    uint16_t patch_nbr : 4;
                    uint16_t opt_cfg_reserved : 8;
                };

                // DA_CMD_CMSTRM
                struct
                {
                    uint16_t length;
                };

                uint16_t driver_action_data;
            };
        };

        uint32_t word;
    };
};

// optimizer config struct
struct opt_cfg_s
{
    struct cop_data_s da_data;
    uint32_t cfg;
    uint32_t id;
};

struct ethosu_semaphore_t
{
    uint8_t count;
};

// One is used for each NPU product/config used in the system
#ifndef ETHOSU_MAX_WAITERS
#define ETHOSU_MAX_WAITERS 4
#endif

struct ethosu_waiter
{
    uint32_t product;
    uint32_t log2_macs;
    void *sem;
    uint32_t num_registered_drivers;
};

/******************************************************************************
 * Variables
 ******************************************************************************/

// Registered drivers linked list HEAD
static struct ethosu_driver *registered_drivers = NULL;

// Waiters - keeps track of availability of different device types
static struct ethosu_waiter waiter_pool[ETHOSU_MAX_WAITERS];

/******************************************************************************
 * Weak functions - Cache
 *
 * Default NOP operations. Override if available on the targeted device.
 ******************************************************************************/

/*
 * Flush/clean the data cache
 */
void __attribute__((weak)) ethosu_flush_dcache(const uint64_t *base_addr,
                                               const size_t *base_addr_size,
                                               int num_base_addr)
{
    /*
     * for (int i = 0; i < num_base_addr; i++)
     * {
     *     // Check alignment to cache line size
     *     if (base_addr[i] & MASK_32_BYTE_ALIGN) != 0)
     *     {
     *         LOG_ERR("Base addr %d: 0x%" PRIx64 "not aligned to 32 bytes", i, base_addr[i]);
     *         return;
     *     }
     *     flush_dcache((uint32_t *)(uintptr_t)base_addr[i], base_addr_size[i]);
     * }
     */
    UNUSED(base_addr);
    UNUSED(base_addr_size);
    UNUSED(num_base_addr);
}

/*
 * Invalidate the data cache
 */
void __attribute__((weak)) ethosu_invalidate_dcache(const uint64_t *base_addr,
                                                    const size_t *base_addr_size,
                                                    int num_base_addr)
{
    /*
     * On 32bit systems, to avoid sign expansion, each base_addr must be cast
     * like this ((uint32_t *)(uintptr_t)base_addr[idx])
     *
     * for (int i = 0; i < num_base_addr; i++)
     * {
     *     // Check alignment to cache line size
     *     if (base_addr[i] & MASK_32_BYTE_ALIGN) != 0)
     *     {
     *         LOG_ERR("Base addr %d: 0x%" PRIx64 "not aligned to 32 bytes", i, base_addr[i]);
     *         return;
     *     }
     *     invalidate_dcache((uint32_t *)(uintptr_t)base_addr[i], base_addr_size[i]);
     * }
     */
    UNUSED(base_addr);
    UNUSED(base_addr_size);
    UNUSED(num_base_addr);
}

/******************************************************************************
 * Weak functions - Semaphore/Mutex for multi NPU
 *
 * Following section handles the minimal sempahore and mutex implementation in
 * case of baremetal applications. Weak symbols will be overridden by RTOS
 * definitions and implement true thread-safety (in application layer).
 ******************************************************************************/

static void *ethosu_mutex;

void *__attribute__((weak)) ethosu_mutex_create(void)
{
    static uint8_t mutex_placeholder;
    return &mutex_placeholder;
}

void __attribute__((weak)) ethosu_mutex_destroy(void *mutex)
{
    UNUSED(mutex);
}

int __attribute__((weak)) ethosu_mutex_lock(void *mutex)
{
    UNUSED(mutex);
    return 0;
}

int __attribute__((weak)) ethosu_mutex_unlock(void *mutex)
{
    UNUSED(mutex);
    return 0;
}

// Baremetal implementation of initing a counting semaphore.
// When overriding this function with an RTOS counting semaphore, create it
// with an initial count of zero. The maximum count must be large enough for
// the reservation waiter semaphore, which can hold one token per available
// registered driver of the same NPU variant. A safe value is the maximum
// number of NPU driver instances in the system.
void *__attribute__((weak)) ethosu_semaphore_create(void)
{
    struct ethosu_semaphore_t *sem = malloc(sizeof(*sem));
    if (sem != NULL)
    {
        sem->count = 0;
    }
    return sem;
}

void __attribute__((weak)) ethosu_semaphore_destroy(void *sem)
{
    free((struct ethosu_semaphore_t *)sem);
}

// Baremetal simulation of waiting/sleeping for and then taking a semaphore using intrisics
int __attribute__((weak)) ethosu_semaphore_take(void *sem, uint64_t timeout)
{
    // Baremetal pseudo-example on how to trigger a timeout:
    // if (timeout && timeout != ETHOSU_SEMAPHORE_WAIT_FOREVER) {
    //     setup_a_timer_to_call_SEV_after_time(timeout);
    // }
    struct ethosu_semaphore_t *s = sem;

    // Support "NO_WAIT" mode
    if (!timeout)
    {
        if (s->count > 0)
        {
            s->count--;
            return 0;
        }
        return -1;
    }

    while (s->count == 0)
    {
        __WFE();
        // Baremetal pseudo-example check if timeout triggered:
        // if (SEV_timer_triggered()) {
        //     return -1;
        // }
    }
    s->count--;
    return 0;
}

// Baremetal simulation of giving a semaphore and waking up processes using intrinsics
int __attribute__((weak)) ethosu_semaphore_give(void *sem)
{
    struct ethosu_semaphore_t *s = sem;
    s->count++;
    __SEV();
    return 0;
}

/******************************************************************************
 * Weak functions - Inference begin/end callbacks
 ******************************************************************************/

void __attribute__((weak)) ethosu_inference_begin(struct ethosu_driver *drv, void *user_arg)
{
    UNUSED(user_arg);
    UNUSED(drv);
}

void __attribute__((weak)) ethosu_inference_end(struct ethosu_driver *drv, void *user_arg)
{
    UNUSED(user_arg);
    UNUSED(drv);
}

#ifndef ETHOSU_MULTI_VARIANT
uint64_t __attribute__((weak)) ethosu_address_remap(uint64_t address, int index)
{
    UNUSED(index);
    return address;
}

unsigned int __attribute__((weak)) ethosu_config_select(uint64_t address, int index)
{
    UNUSED(address);
    assert(index >= -1 && index <= 7);

    switch (index)
    {
    case -1:
        return NPU_QCONFIG;
    default:
    case 0:
        return NPU_REGIONCFG_0;
    case 1:
        return NPU_REGIONCFG_1;
    case 2:
        return NPU_REGIONCFG_2;
    case 3:
        return NPU_REGIONCFG_3;
    case 4:
        return NPU_REGIONCFG_4;
    case 5:
        return NPU_REGIONCFG_5;
    case 6:
        return NPU_REGIONCFG_6;
    case 7:
        return NPU_REGIONCFG_7;
    }
}
#else
uint64_t ethosu_address_remap(uint64_t address, int index)
{
    /*
     * Not usable when ETHOSU_MULTI_VARIANT is defined.
     * Use ethosu_init_ex() and provide an address_remap callback through
     * struct ethosu_device_user_ops instead.
     */
    UNUSED(address);
    UNUSED(index);
    return 0;
}

unsigned int ethosu_config_select(uint64_t address, int index)
{
    /*
     * Not usable when ETHOSU_MULTI_VARIANT is defined.
     * Use ethosu_init_ex() and provide a config_select callback through
     * struct ethosu_device_user_ops instead.
     */
    UNUSED(address);
    UNUSED(index);
    return 0;
}
#endif

/******************************************************************************
 * Static functions
 ******************************************************************************/

static struct ethosu_driver *ethosu_find_free_matching_driver(uint32_t product, uint32_t log2_macs)
{
    for (struct ethosu_driver *d = registered_drivers; d; d = d->next)
    {
        if (!d->reserved && product == d->dev.caps.product && log2_macs == d->dev.caps.log2_macs)
        {
            return d;
        }
    }
    return NULL;
}

// Must be called within global mutex lock
static struct ethosu_waiter *ethosu_create_waiter_for_driver(struct ethosu_driver *drv)
{
    for (int i = 0; i < ETHOSU_MAX_WAITERS; i++)
    {
        if (waiter_pool[i].sem)
        {
            // Not a free slot
            continue;
        }

        if ((waiter_pool[i].sem = ethosu_semaphore_create()) == NULL)
        {
            waiter_pool[i].product   = 0;
            waiter_pool[i].log2_macs = 0;
            LOG_ERR("Failed to create semaphore for new waiter");
            return NULL;
        }
        waiter_pool[i].product   = drv->dev.caps.product;
        waiter_pool[i].log2_macs = drv->dev.caps.log2_macs;
        return &waiter_pool[i];
    }

    // No free waiter slots
    LOG_ERR("Failed to create waiter for driver, increase ETHOSU_MAX_WAITERS!");

    return NULL;
}

// Must be called within global mutex lock
static struct ethosu_waiter *ethosu_get_waiter(uint32_t product, uint32_t log2_macs)
{
    for (int i = 0; i < ETHOSU_MAX_WAITERS; i++)
    {
        if (waiter_pool[i].product == product && waiter_pool[i].log2_macs == log2_macs)
        {
            return &waiter_pool[i];
        }
    }

    return NULL;
}

// Must be called within global mutex lock
static struct ethosu_waiter *ethosu_get_waiter_for_driver(struct ethosu_driver *drv)
{
    return ethosu_get_waiter(drv->dev.caps.product, drv->dev.caps.log2_macs);
}

// Must be called within global mutex lock
static int ethosu_deregister_waiter_for_driver(struct ethosu_driver *drv)
{
    for (int i = 0; i < ETHOSU_MAX_WAITERS; i++)
    {
        if (waiter_pool[i].product != drv->dev.caps.product || waiter_pool[i].log2_macs != drv->dev.caps.log2_macs)
        {
            continue;
        }

        // Defensive check
        if (!waiter_pool[i].sem || waiter_pool[i].num_registered_drivers == 0)
        {
            LOG_ERR("Internal error: semaphore is NULL or number of registered drivers == 0");
            return -1;
        }

        // Try to decrement semaphore count, fail if not available to avoid mutex deadlock
        if (ethosu_semaphore_take(waiter_pool[i].sem, 0) < 0)
        {
            LOG_ERR("Semaphore count is zero! Is the driver reserved?!");
            return -1;
        }

        if (waiter_pool[i].num_registered_drivers == 1)
        {
            // This is the only registered driver, reset waiter
            waiter_pool[i].product                = 0;
            waiter_pool[i].log2_macs              = 0;
            waiter_pool[i].num_registered_drivers = 0;
            ethosu_semaphore_destroy(waiter_pool[i].sem);
            waiter_pool[i].sem = NULL;
        }
        else
        {
            // More NPU's are registered to this waiter
            waiter_pool[i].num_registered_drivers--;
        }
        // Waiter found and handled, all done
        return 0;
    }

    LOG_ERR("Found no matching waiter to deregister!");
    return -1;
}

static int ethosu_register_driver(struct ethosu_driver *drv)
{
    struct ethosu_waiter *waiter = NULL;

    ethosu_mutex_lock(ethosu_mutex);
    if ((waiter = ethosu_get_waiter_for_driver(drv)) == NULL)
    {
        if ((waiter = ethosu_create_waiter_for_driver(drv)) == NULL)
        {
            ethosu_mutex_unlock(ethosu_mutex);
            LOG_ERR("Failed to register driver (handle: 0x%p)", drv);
            return -1;
        }
    }

    drv->next          = registered_drivers;
    registered_drivers = drv;
    waiter->num_registered_drivers++;
    ethosu_mutex_unlock(ethosu_mutex);

    LOG_INFO("New %s driver registered (handle: 0x%p, NPU: 0x%p)", drv->dev.desc->name, drv, drv->dev.reg);

    ethosu_semaphore_give(waiter->sem);

    return 0;
}

// Must not be called if there are waiters for the driver or if the driver is in use!
static int ethosu_deregister_driver(struct ethosu_driver *drv)
{
    struct ethosu_driver *curr;
    struct ethosu_driver **prev;

    ethosu_mutex_lock(ethosu_mutex);
    if (drv->reserved)
    {
        ethosu_mutex_unlock(ethosu_mutex);
        LOG_ERR("Can't deregister a reserved driver!");
        return -1;
    }

    curr = registered_drivers;
    prev = &registered_drivers;

    while (curr != NULL)
    {
        if (curr == drv)
        {
            if (ethosu_deregister_waiter_for_driver(drv) < 0)
            {
                ethosu_mutex_unlock(ethosu_mutex);
                LOG_ERR("Failed to deregister driver!");
                return -1;
            }
            *prev = curr->next;
            LOG_INFO("%s driver handle %p deregistered.", drv->dev.desc->name, drv);
            break;
        }

        prev = &curr->next;
        curr = curr->next;
    }

    ethosu_mutex_unlock(ethosu_mutex);

    if (curr == NULL)
    {
        LOG_ERR("No NPU driver handle registered at address %p.", drv);
        return -1;
    }

    return 0;
}

static void ethosu_reset_job(struct ethosu_driver *drv)
{
    memset(&drv->job, 0, sizeof(struct ethosu_job));
}

static int handle_optimizer_config(struct ethosu_driver *drv, struct opt_cfg_s const *opt_cfg_p)
{
    LOG_INFO("Optimizer release nbr: %u patch: %u", opt_cfg_p->da_data.rel_nbr, opt_cfg_p->da_data.patch_nbr);

    if (drv->dev.desc->ops->verify_optimizer_config(&drv->dev, opt_cfg_p->cfg, opt_cfg_p->id) != true)
    {
        return -1;
    }

    return 0;
}

static int handle_command_stream(struct ethosu_driver *drv, const uint8_t *cmd_stream, const int cms_length)
{
    // cms_length is number of 32bit words
    uint32_t cms_bytes = cms_length * 4;

    LOG_INFO("handle_command_stream: cmd_stream=%p, cms_length %d words (%" PRIu32 " bytes)",
             cmd_stream,
             cms_length,
             cms_bytes);

    if (0 != ((ptrdiff_t)cmd_stream & MASK_16_BYTE_ALIGN))
    {
        LOG_ERR("Command stream addr %p not aligned to 16 bytes", cmd_stream);
        return -1;
    }

    // Verify minimum 16 byte alignment for base address'
    for (int i = 0; i < drv->job.num_base_addr; i++)
    {
        if (0 != (drv->job.base_addr[i] & MASK_16_BYTE_ALIGN))
        {
            LOG_ERR("Base addr %d: 0x%" PRIx64 "not aligned to 16 bytes", i, drv->job.base_addr[i]);
            return -1;
        }
    }

    // TODO: Add call to flush/clean the command stream too?

    // Flush/clean the data cache
    ethosu_flush_dcache(drv->job.base_addr, drv->job.base_addr_size, drv->job.num_base_addr);

    // Request power gating disabled during inference run
    if (ethosu_request_power(drv))
    {
        LOG_ERR("Failed to request power");
        return -1;
    }

    drv->job.state = ETHOSU_JOB_RUNNING;

    // Inference begin callback
    ethosu_inference_begin(drv, drv->job.user_arg);

    // Execute the command stream
    drv->dev.desc->ops->run_command_stream(
        &drv->dev, cmd_stream, cms_bytes, drv->job.base_addr, drv->job.num_base_addr);

    return 0;
}

static bool ethosu_verify_cop_data_size(const int custom_data_size)
{
    // COP data size must be at least 4 bytes
    if (custom_data_size < 4)
    {
        LOG_ERR("custom_data_size=%d < 4", custom_data_size);
        return false;
    }

    // Custom data size must be a multiple of 4
    if ((custom_data_size % 4) != 0)
    {
        LOG_ERR("custom_data_size=0x%x not a multiple of 4", (unsigned)custom_data_size);
        return false;
    }

    return true;
}

static bool ethosu_verify_cop_record_words(const struct cop_data_s *data_ptr,
                                           const struct cop_data_s *data_end,
                                           size_t record_words,
                                           const char *record_name)
{
    ptrdiff_t remaining_words = data_end - data_ptr;

    if (remaining_words < 0 || (size_t)remaining_words < record_words)
    {
        LOG_ERR("Custom Operator Payload truncated %s record. remaining_words=%td, expected_words=%zu",
                record_name,
                remaining_words,
                record_words);
        return false;
    }

    return true;
}

/******************************************************************************
 * Weak functions - Interrupt handler
 ******************************************************************************/
void __attribute__((weak)) ethosu_irq_handler(struct ethosu_driver *drv)
{
    // Prevent race condition where interrupt triggered after a timeout waiting
    // for semaphore, but before NPU is reset.
    if (drv->job.result == ETHOSU_JOB_RESULT_TIMEOUT)
    {
        (void)drv->dev.desc->ops->handle_interrupt(&drv->dev);
        return;
    }

    drv->job.state  = ETHOSU_JOB_DONE;
    drv->job.result = drv->dev.desc->ops->handle_interrupt(&drv->dev) ? ETHOSU_JOB_RESULT_OK : ETHOSU_JOB_RESULT_ERROR;
    ethosu_semaphore_give(drv->semaphore);
}

/******************************************************************************
 * Functions API
 ******************************************************************************/

#ifndef ETHOSU_MULTI_VARIANT
int ethosu_init(struct ethosu_driver *drv,
                void *const base_address,
                const void *fast_memory,
                const size_t fast_memory_size,
                uint32_t secure_enable,
                uint32_t privilege_enable)
{
    const struct ethosu_device_desc *default_dev; // compile time driver
    struct ethosu_device_config *default_config;  // compile time config
    static struct ethosu_device_user_ops legacy_user_ops = {
        .address_remap = ethosu_address_remap,
        .config_select = ethosu_config_select,
    };
#if defined(ETHOSU55)
    default_dev    = &ethosu_device_desc_u55;
    default_config = &ethosu_device_config_u55;
#elif defined(ETHOSU65)
    default_dev    = &ethosu_device_desc_u65;
    default_config = &ethosu_device_config_u65;
#elif defined(ETHOSU85)
    default_dev    = &ethosu_device_desc_u85;
    default_config = &ethosu_device_config_u85;
#else
#error Compile time API chosen, but no device type macro found (ETHOSU**)
#endif
    return ethosu_init_ex(drv,
                          default_dev,
                          default_config,
                          &legacy_user_ops,
                          base_address,
                          fast_memory,
                          fast_memory_size,
                          secure_enable,
                          privilege_enable);
}
#endif

int ethosu_init_ex(struct ethosu_driver *drv,
                   const struct ethosu_device_desc *dev_desc,
                   struct ethosu_device_config *dev_config,
                   struct ethosu_device_user_ops *dev_user_ops,
                   void *const base_address,
                   const void *fast_memory,
                   const size_t fast_memory_size,
                   uint32_t secure_enable,
                   uint32_t privilege_enable)
{
    if (!drv || !dev_desc || !dev_config || !base_address)
    {
        LOG_ERR("Init called with NULL arg(s)");
        return -1;
    }

    LOG_INFO("Initializing %s NPU: base_address=%p, fast_memory=%p, fast_memory_size=%zu, secure=%" PRIu32
             ", privileged=%" PRIu32,
             dev_desc->name,
             base_address,
             fast_memory,
             fast_memory_size,
             secure_enable,
             privilege_enable);

    if (!ethosu_mutex)
    {
        ethosu_mutex = ethosu_mutex_create();
        if (!ethosu_mutex)
        {
            LOG_ERR("Failed to create global driver mutex");
            return -1;
        }
    }

    drv->fast_memory           = (uintptr_t)fast_memory;
    drv->fast_memory_size      = fast_memory_size;
    drv->power_request_counter = 0;
    drv->reserved              = false;

    // Initialize the device and set requested security state and privilege mode
    if (!dev_desc->ops->init(
            &drv->dev, dev_desc, dev_config, dev_user_ops, base_address, secure_enable, privilege_enable))
    {
        LOG_ERR("Failed to initialize %s device", dev_desc->name);
        return -1;
    }

    switch (drv->dev.caps.product)
    {
#if defined(ETHOSU55) || defined(ETHOSU_MULTI_VARIANT)
    case ETHOSU_PRODUCT_U55:
        drv->pmu = &ethosu_pmu_desc_u55;
        break;
#endif
#if defined(ETHOSU65) || defined(ETHOSU_MULTI_VARIANT)
    case ETHOSU_PRODUCT_U65:
        drv->pmu = &ethosu_pmu_desc_u65;
        break;
#endif
#if defined(ETHOSU85) || defined(ETHOSU_MULTI_VARIANT)
    case ETHOSU_PRODUCT_U85:
        drv->pmu = &ethosu_pmu_desc_u85;
        break;
#endif
    default:
        LOG_ERR("Invalid driver product!");
        return -1;
    }

    drv->semaphore = ethosu_semaphore_create();
    if (!drv->semaphore)
    {
        LOG_ERR("Failed to create driver semaphore");
        return -1;
    }

    ethosu_reset_job(drv);

    if (ethosu_register_driver(drv) != 0)
    {
        LOG_ERR("Failed to initialise driver");
        ethosu_semaphore_destroy(drv->semaphore);
        return -1;
    }

    return 0;
}

void ethosu_deinit(struct ethosu_driver *drv)
{
    if (!drv)
    {
        LOG_ERR("De-init called with NULL arg");
        return;
    }

    if (ethosu_deregister_driver(drv) == 0)
    {
        ethosu_semaphore_destroy(drv->semaphore);
        LOG_INFO("De-initialised %s driver (handle: 0x%p, NPU: 0x%p)", drv->dev.desc->name, drv, drv->dev.reg);
    }
    else
    {
        LOG_ERR("Failed to de-initialised %s driver (handle: 0x%p, NPU: 0x%p)", drv->dev.desc->name, drv, drv->dev.reg);
    }
}

int ethosu_soft_reset(struct ethosu_driver *drv)
{
    if (!drv)
    {
        LOG_ERR("Soft reset called with NULL arg");
        return -1;
    }

    // Soft reset the NPU
    if (!drv->dev.desc->ops->soft_reset(&drv->dev))
    {
        LOG_ERR("Failed to soft-reset %s", drv->dev.desc->name);
        return -1;
    }

    // Update power and clock gating after the soft reset
    drv->dev.desc->ops->set_clock_and_power(
        &drv->dev,
        drv->power_request_counter > 0 ? ETHOSU_CLOCK_Q_DISABLE : ETHOSU_CLOCK_Q_ENABLE,
        drv->power_request_counter > 0 ? ETHOSU_POWER_Q_DISABLE : ETHOSU_POWER_Q_ENABLE);

    return 0;
}

int ethosu_request_power(struct ethosu_driver *drv)
{
    if (!drv)
    {
        LOG_ERR("Request power called with NULL arg");
        return -1;
    }

    // Check if this is the first power request, increase counter
    if (drv->power_request_counter++ == 0)
    {
        // Always reset to a known state. Changes to requested
        // security state/privilege mode if necessary.
        if (ethosu_soft_reset(drv))
        {
            LOG_ERR("Failed to request power for %s", drv->dev.desc->name);
            drv->power_request_counter--;
            return -1;
        }
    }
    return 0;
}

void ethosu_release_power(struct ethosu_driver *drv)
{
    if (!drv)
    {
        LOG_ERR("Release power called with NULL arg");
        return;
    }

    if (drv->power_request_counter == 0)
    {
        LOG_WARN("No power request left to release, reference counter is 0");
    }
    else
    {
        // Decrement ref counter and enable power gating if no requests remain
        if (--drv->power_request_counter == 0)
        {
            drv->dev.desc->ops->set_clock_and_power(&drv->dev, ETHOSU_CLOCK_Q_ENABLE, ETHOSU_POWER_Q_ENABLE);
        }
    }
}

void ethosu_get_driver_version(struct ethosu_driver_version *ver)
{
    if (!ver)
    {
        LOG_ERR("Get driver version called with NULL arg");
        return;
    }

    ver->major = ETHOSU_DRIVER_VERSION_MAJOR;
    ver->minor = ETHOSU_DRIVER_VERSION_MINOR;
    ver->patch = ETHOSU_DRIVER_VERSION_PATCH;
}

void ethosu_get_hw_info(struct ethosu_driver *drv, struct ethosu_hw_info *hw)
{
    if (!drv || !hw)
    {
        LOG_ERR("Get hardware info called with NULL arg(s)");
        return;
    }

    drv->dev.desc->ops->get_hw_info(&drv->dev, hw);
}

int ethosu_wait(struct ethosu_driver *drv, bool block)
{
    int ret = 0;

    if (!drv)
    {
        LOG_ERR("Wait called with NULL arg");
        return -1;
    }

    switch (drv->job.state)
    {
    case ETHOSU_JOB_IDLE:
        LOG_ERR("Inference job not running...");
        ret = -2;
        break;
    case ETHOSU_JOB_RUNNING:
        if (!block)
        {
            // Inference still running, do not block
            ret = 1;
            break;
        }
        // fall through
    case ETHOSU_JOB_DONE:
        // Wait for interrupt in blocking mode. In non-blocking mode
        // the interrupt has already triggered
        ret = ethosu_semaphore_take(drv->semaphore, ETHOSU_SEMAPHORE_WAIT_INFERENCE);
        if (ret < 0)
        {
            drv->job.result = ETHOSU_JOB_RESULT_TIMEOUT;

            // There's a race where the NPU interrupt can have fired between semaphore
            // timing out and setting the result above (checked in interrupt handler).
            // By checking if the job state has been changed (only set to DONE by interrupt
            // handler), we know if the interrupt handler has run, if so decrement the
            // semaphore count by one (given in interrupt handler).
            if (drv->job.state == ETHOSU_JOB_DONE)
            {
                drv->job.result = ETHOSU_JOB_RESULT_TIMEOUT; // Reset back to timeout
                ethosu_semaphore_take(drv->semaphore, ETHOSU_SEMAPHORE_WAIT_INFERENCE);
            }
        }

        // Invalidate cache
        ethosu_invalidate_dcache(drv->job.base_addr, drv->job.base_addr_size, drv->job.num_base_addr);

        // Inference done callback - always called even in case of timeout
        ethosu_inference_end(drv, drv->job.user_arg);

        // Release power gating disabled requirement
        ethosu_release_power(drv);

        // Check NPU and interrupt status
        if (drv->job.result)
        {
            if (drv->job.result == ETHOSU_JOB_RESULT_ERROR)
            {
                LOG_ERR("Error(s) for %s occured during inference.", drv->dev.desc->name);
                drv->dev.desc->ops->print_err_status(&drv->dev);
            }
            else
            {
                LOG_ERR("%s inference timed out.", drv->dev.desc->name);
            }

            // Reset the NPU
            (void)ethosu_soft_reset(drv);

            ret = -1;
        }
        else
        {
            LOG_DEBUG("Inference on %s finished successfully...", drv->dev.desc->name);
            ret = 0;
        }

        // Reset internal job (state resets to IDLE)
        ethosu_reset_job(drv);
        break;

    default:
        LOG_ERR("Unexpected job state");
        ethosu_reset_job(drv);
        ret = -1;
        break;
    }

    // Return inference job status
    return ret;
}

int ethosu_invoke_async(struct ethosu_driver *drv,
                        const void *custom_data_ptr,
                        const int custom_data_size,
                        uint64_t *const base_addr,
                        const size_t *base_addr_size,
                        const int num_base_addr,
                        void *user_arg)
{
    const struct cop_data_s *data_ptr = custom_data_ptr;
    const struct cop_data_s *data_end = (struct cop_data_s *)((ptrdiff_t)custom_data_ptr + custom_data_size);

    if (!drv || !custom_data_ptr || !base_addr || !base_addr_size)
    {
        LOG_ERR("Invoke called with NULL arg(s)");
        return -1;
    }

    // Make sure an inference is not already running
    if (drv->job.state != ETHOSU_JOB_IDLE)
    {
        LOG_ERR("Inference already running, or waiting to be cleared...");
        return -1;
    }

    drv->job.state            = ETHOSU_JOB_IDLE;
    drv->job.custom_data_ptr  = custom_data_ptr;
    drv->job.custom_data_size = custom_data_size;
    drv->job.base_addr        = base_addr;
    drv->job.base_addr_size   = base_addr_size;
    drv->job.num_base_addr    = num_base_addr;
    drv->job.user_arg         = user_arg;

    if (!ethosu_verify_cop_data_size(custom_data_size))
    {
        goto err;
    }

    // Verify first word
    if (data_ptr->word != ETHOSU_FOURCC)
    {
        LOG_ERR("Custom Operator Payload: %" PRIu32 " is not correct, expected %x", data_ptr->word, ETHOSU_FOURCC);
        goto err;
    }

    data_ptr++;

    // Adjust base address to fast memory area
    if (drv->fast_memory != 0 && num_base_addr > FAST_MEMORY_BASE_ADDR_INDEX)
    {
        if (base_addr_size[FAST_MEMORY_BASE_ADDR_INDEX] > drv->fast_memory_size)
        {
            LOG_ERR("Fast memory area too small. fast_memory_size=%zu, base_addr_size=%zu",
                    drv->fast_memory_size,
                    base_addr_size[FAST_MEMORY_BASE_ADDR_INDEX]);
            goto err;
        }

        base_addr[FAST_MEMORY_BASE_ADDR_INDEX] = drv->fast_memory;
    }

    // Parse Custom Operator Payload data
    while (data_ptr < data_end)
    {
        switch (data_ptr->driver_action_command)
        {
        case OPTIMIZER_CONFIG:
        {
            const size_t record_words = DRIVER_ACTION_LENGTH_32_BIT_WORD + OPTIMIZER_CONFIG_LENGTH_32_BIT_WORD;
            struct opt_cfg_s opt_cfg  = {0};
            LOG_DEBUG("OPTIMIZER_CONFIG");

            if (!ethosu_verify_cop_record_words(data_ptr, data_end, record_words, "OPTIMIZER_CONFIG"))
            {
                goto err;
            }

            opt_cfg.da_data = *data_ptr;
            opt_cfg.cfg     = data_ptr[1].word;
            opt_cfg.id      = data_ptr[2].word;

            if (handle_optimizer_config(drv, &opt_cfg) < 0)
            {
                goto err;
            }
            data_ptr += record_words;
            break;
        }
        case COMMAND_STREAM:
        {
            size_t record_words;

            // Vela only supports putting one COMMAND_STREAM per op
            LOG_DEBUG("COMMAND_STREAM");
            int cms_length                = (data_ptr->reserved << 16) | data_ptr->length;
            const uint8_t *command_stream = (const uint8_t *)(data_ptr + 1);

            record_words = DRIVER_ACTION_LENGTH_32_BIT_WORD + (size_t)cms_length;
            if (!ethosu_verify_cop_record_words(data_ptr, data_end, record_words, "COMMAND_STREAM"))
            {
                goto err;
            }

            if (handle_command_stream(drv, command_stream, cms_length) < 0)
            {
                goto err;
            }
            data_ptr += record_words;
            break;
        }
        case NOP:
            LOG_DEBUG("NOP");
            data_ptr += DRIVER_ACTION_LENGTH_32_BIT_WORD;
            break;
        default:
            LOG_ERR("UNSUPPORTED driver_action_command: %u", data_ptr->driver_action_command);
            goto err;
            break;
        }
    }

    return 0;
err:
    LOG_ERR("Failed to invoke inference for %s", drv->dev.desc->name);
    ethosu_reset_job(drv);
    return -1;
}

int ethosu_invoke_v3(struct ethosu_driver *drv,
                     const void *custom_data_ptr,
                     const int custom_data_size,
                     uint64_t *const base_addr,
                     const size_t *base_addr_size,
                     const int num_base_addr,
                     void *user_arg)
{
#ifdef ETHOSU_MULTI_VARIANT
    // Workaround for some frameworks that call non ex_ version of reserve_driver:
    // To allow for reserve_driver/invoke/release_driver flow to continue to work,
    // the reserve_driver function will return NULL when multi variant mode is enabled.
    // This function will call invoke_auto() when drv == NULL, and then release_driver()
    // will be a NOP when drv == NULL.
    if (!drv)
    {
        return ethosu_invoke_auto(
            custom_data_ptr, custom_data_size, base_addr, base_addr_size, num_base_addr, user_arg);
    }
#endif

    if (ethosu_invoke_async(
            drv, custom_data_ptr, custom_data_size, base_addr, base_addr_size, num_base_addr, user_arg) < 0)
    {
        return -1;
    }

    return ethosu_wait(drv, true);
}

int ethosu_get_product_config_from_cop_data(const void *custom_data_ptr,
                                            const int custom_data_size,
                                            uint32_t *product_out,
                                            uint32_t *log2_macs_out)
{
    const struct cop_data_s *data_ptr = custom_data_ptr;
    const struct cop_data_s *data_end = (struct cop_data_s *)((ptrdiff_t)custom_data_ptr + custom_data_size);

    if (!custom_data_ptr)
    {
        LOG_ERR("custom_data_ptr is NULL");
        return -1;
    }

    if (!ethosu_verify_cop_data_size(custom_data_size))
    {
        return -1;
    }

    // Verify first word
    if (data_ptr->word != ETHOSU_FOURCC)
    {
        LOG_ERR("Custom Operator Payload: %" PRIu32 " is not correct, expected %x", data_ptr->word, ETHOSU_FOURCC);
        return -1;
    }

    data_ptr++;

    // Parse Custom Operator Payload data
    while (data_ptr < data_end)
    {
        switch (data_ptr->driver_action_command)
        {
        case OPTIMIZER_CONFIG:
        {
            uint32_t cfg = 0;

            if (!ethosu_verify_cop_record_words(data_ptr,
                                                data_end,
                                                DRIVER_ACTION_LENGTH_32_BIT_WORD + OPTIMIZER_CONFIG_LENGTH_32_BIT_WORD,
                                                "OPTIMIZER_CONFIG"))
            {
                return -1;
            }

            cfg = data_ptr[1].word;

            // Got the optimizer config, telling which NPU the network has been compiled for
            if (product_out)
            {
                *product_out = (cfg >> 28);
            }

            if (log2_macs_out)
            {
                *log2_macs_out = (cfg & 0XF);
            }
            return 0;
        }
        case COMMAND_STREAM:
        {
            size_t record_words =
                DRIVER_ACTION_LENGTH_32_BIT_WORD + (size_t)((data_ptr->reserved << 16) | data_ptr->length);

            if (!ethosu_verify_cop_record_words(data_ptr, data_end, record_words, "COMMAND_STREAM"))
            {
                return -1;
            }
            data_ptr += record_words;
            break;
        }
        case NOP:
            data_ptr += DRIVER_ACTION_LENGTH_32_BIT_WORD;
            break;
        default:
            LOG_ERR("UNSUPPORTED driver_action_command: %u", data_ptr->driver_action_command);
            return -1;
        }
    }

    LOG_ERR("Could not find product config in COP data!");
    return -1;
}

// Call this to automatically find a suitable driver matching what the network has been compiled for
int ethosu_invoke_auto(const void *custom_data_ptr,
                       const int custom_data_size,
                       uint64_t *const base_addr,
                       const size_t *base_addr_size,
                       const int num_base_addr,
                       void *user_arg)
{
    struct ethosu_driver *drv = NULL;
    uint32_t product          = 0;
    uint32_t log2_macs        = 0;
    int ret                   = 0;

    if (!custom_data_ptr || !base_addr || !base_addr_size)
    {
        LOG_ERR("Invoke auto called with NULL arg(s)");
        return -1;
    }

    if (ethosu_get_product_config_from_cop_data(custom_data_ptr, custom_data_size, &product, &log2_macs) != 0)
    {
        goto err;
    }
    // Find a suitable driver, will block until one comes availalable, if a driver matching the requested has been
    // registered
    drv = ethosu_reserve_driver_ex(product, log2_macs);
    if (!drv)
    {
        goto err;
    }

    if (ethosu_invoke_async(
            drv, custom_data_ptr, custom_data_size, base_addr, base_addr_size, num_base_addr, user_arg) < 0)
    {
        ethosu_release_driver(drv);
        goto err;
    }

    ret = ethosu_wait(drv, true);
    ethosu_release_driver(drv);

    return ret;
err:
    LOG_ERR("Failed to invoke inference (auto mode)");
    return -1;
}

#ifndef ETHOSU_MULTI_VARIANT
static inline int ethosu_log2(const int val)
{
    assert(val != 0);
    assert(val % 2 == 0);
    return (31 - __builtin_clz(val));
}
#endif

struct ethosu_driver *ethosu_reserve_driver(void)
{
#ifdef ETHOSU_MULTI_VARIANT
    // Workaround for some frameworks that call non ex_ version of reserve_driver:
    // To allow for reserve_driver/invoke/release_driver flow to continue to work,
    // the reserve_driver function will return NULL when multi variant mode is enabled.
    // The invoke()/invoke_v3() functions will call invoke_auto() when drv == NULL,
    // and release_driver() will be a NOP when drv == NULL.
    return NULL;
#else
    return ethosu_reserve_driver_ex(
#if defined(ETHOSU55)
        ETHOSU_PRODUCT_U55
#elif defined(ETHOSU65)
        ETHOSU_PRODUCT_U65
#elif defined(ETHOSU85)
        ETHOSU_PRODUCT_U85
#endif
        ,
        ethosu_log2(ETHOSU_MACS));
#endif
}

struct ethosu_driver *ethosu_reserve_driver_ex(uint32_t product, uint32_t log2_macs)
{
    struct ethosu_driver *drv    = NULL;
    struct ethosu_waiter *waiter = NULL;

    LOG_DEBUG("Acquiring NPU driver handle (block until one becomes available)");
    ethosu_mutex_lock(ethosu_mutex);
    waiter = ethosu_get_waiter(product, log2_macs);
    if (!waiter)
    {
        ethosu_mutex_unlock(ethosu_mutex);
        LOG_ERR("No driver for product: %" PRIu32 ", log2_macs: %" PRIu32 " found!", product, log2_macs);
        return NULL;
    }
    ethosu_mutex_unlock(ethosu_mutex);

    ethosu_semaphore_take(waiter->sem, ETHOSU_SEMAPHORE_WAIT_FOREVER);

    ethosu_mutex_lock(ethosu_mutex);
    drv = ethosu_find_free_matching_driver(product, log2_macs);
    if (!drv)
    {
        ethosu_mutex_unlock(ethosu_mutex);
        LOG_ERR("Internal error: no driver available but semaphore taken");
        return NULL;
    }

    drv->reserved = true;
    ethosu_mutex_unlock(ethosu_mutex);
    LOG_DEBUG("%s driver handle %p reserved", drv->dev.desc->name, drv);
    return drv;
}

void ethosu_release_driver(struct ethosu_driver *drv)
{
    struct ethosu_waiter *waiter = NULL;

    if (!drv)
    {
#ifndef ETHOSU_MULTI_VARIANT
        // Workaround for some frameworks that call non ex_ version of reserve_driver:
        // To allow for reserve_driver/invoke/release_driver flow to continue to work,
        // the reserve_driver function will return NULL when multi variant mode is enabled.
        // The invoke()/invoke_v3() functions will call invoke_auto() when drv == NULL,
        // so don't treat this release_driver() call with drv == NULL as error.
        LOG_ERR("Release driver called with NULL arg");
#endif
        return;
    }

    LOG_DEBUG("Releasing %s driver handle %p", drv->dev.desc->name, drv);

    ethosu_mutex_lock(ethosu_mutex);
    if (!drv->reserved)
    {
        ethosu_mutex_unlock(ethosu_mutex);
        LOG_ERR("Failed to release NPU driver handle, it is not reserved!");
        return;
    }

    if (drv->job.state == ETHOSU_JOB_RUNNING || drv->job.state == ETHOSU_JOB_DONE)
    {
        LOG_WARN("Release on driver called while it's still running or ethosu_wait() not called");
        // Give the inference one shot to complete or force kill the job
        if (ethosu_wait(drv, false) == 1)
        {
            LOG_WARN("Killing the job and resetting NPU");
            // Still running, soft reset the NPU and reset driver
            drv->power_request_counter = 0;
            ethosu_soft_reset(drv);
            ethosu_reset_job(drv);
        }
    }

    // Mark free
    drv->reserved = false;

    // Return this driver to the available pool
    waiter = ethosu_get_waiter_for_driver(drv);
    ethosu_mutex_unlock(ethosu_mutex);
    ethosu_semaphore_give(waiter->sem);
    LOG_DEBUG("%s driver handle %p released", drv->dev.desc->name, drv);
}

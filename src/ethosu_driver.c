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

/******************************************************************************
 * Includes
 ******************************************************************************/

#include "ethosu_driver.h"
#include "ethosu_config.h"
#include "ethosu_device.h"
#include "ethosu_log.h"

#include <assert.h>
#include <cmsis_compiler.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/******************************************************************************
 * Defines
 ******************************************************************************/

#define UNUSED(x) ((void)x)

#define BYTES_IN_32_BITS 4
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

/******************************************************************************
 * Variables
 ******************************************************************************/

// Registered drivers linked list HEAD
static struct ethosu_driver *registered_drivers = NULL;

/******************************************************************************
 * Weak functions - Cache
 *
 * Default NOP operations. Override if available on the targeted device.
 ******************************************************************************/

/*
 * Flush/clean the data cache by address and size. Passing NULL as p argument
 * expects the whole cache to be flushed.
 */
void __attribute__((weak)) ethosu_flush_dcache(uint32_t *p, size_t bytes)
{
    UNUSED(p);
    UNUSED(bytes);
}

/*
 * Invalidate the data cache by address and size. Passing NULL as p argument
 * expects the whole cache to be invalidated.
 */
void __attribute__((weak)) ethosu_invalidate_dcache(uint32_t *p, size_t bytes)
{
    UNUSED(p);
    UNUSED(bytes);
}

/******************************************************************************
 * Weak functions - Semaphore/Mutex for multi NPU
 *
 * Following section handles the minimal sempahore and mutex implementation in
 * case of baremetal applications. Weak symbols will be overridden by RTOS
 * definitions and implement true thread-safety (in application layer).
 ******************************************************************************/

struct ethosu_semaphore_t
{
    int count;
};

static void *ethosu_mutex;
static void *ethosu_semaphore;

void *__attribute__((weak)) ethosu_mutex_create(void)
{
    return NULL;
}

void __attribute__((weak)) ethosu_mutex_destroy(void *mutex)
{
    UNUSED(mutex);
}

void __attribute__((weak)) ethosu_mutex_lock(void *mutex)
{
    UNUSED(mutex);
}

void __attribute__((weak)) ethosu_mutex_unlock(void *mutex)
{
    UNUSED(mutex);
}

// Baremetal implementation of creating a semaphore
void *__attribute__((weak)) ethosu_semaphore_create(void)
{
    struct ethosu_semaphore_t *sem = malloc(sizeof(*sem));
    sem->count                     = 1;
    return sem;
}

void __attribute__((weak)) ethosu_semaphore_destroy(void *sem)
{
    free((struct ethosu_semaphore_t *)sem);
}

// Baremetal simulation of waiting/sleeping for and then taking a semaphore using intrisics
void __attribute__((weak)) ethosu_semaphore_take(void *sem)
{
    struct ethosu_semaphore_t *s = sem;
    while (s->count <= 0)
    {
        __WFE();
    }
    s->count--;
}

// Baremetal simulation of giving a semaphore and waking up processes using intrinsics
void __attribute__((weak)) ethosu_semaphore_give(void *sem)
{
    struct ethosu_semaphore_t *s = sem;
    s->count++;
    __SEV();
}

/******************************************************************************
 * Weak functions - Inference begin/end callbacks
 ******************************************************************************/

void __attribute__((weak)) ethosu_inference_begin(struct ethosu_driver *drv, const void *inference_data)
{
    UNUSED(inference_data);
    UNUSED(drv);
}

void __attribute__((weak)) ethosu_inference_end(struct ethosu_driver *drv, const void *inference_data)
{
    UNUSED(inference_data);
    UNUSED(drv);
}

/******************************************************************************
 * Static functions
 ******************************************************************************/
static inline void wait_for_irq(struct ethosu_driver *drv)
{
    while (1)
    {
        if (drv->irq_triggered)
        {
            drv->irq_triggered = false;
            break;
        }

        ethosu_semaphore_take(drv->semaphore);
    }
}

static void ethosu_register_driver(struct ethosu_driver *drv)
{
    // Register driver as new HEAD of list
    drv->next          = registered_drivers;
    registered_drivers = drv;

    LOG_INFO("New NPU driver registered (handle: 0x%p, NPU: 0x%p)", drv, drv->dev->reg);
}

static int ethosu_deregister_driver(struct ethosu_driver *drv)
{
    struct ethosu_driver *cur   = registered_drivers;
    struct ethosu_driver **prev = &registered_drivers;

    while (cur != NULL)
    {
        if (cur == drv)
        {
            *prev = cur->next;
            LOG_INFO("NPU driver handle %p deregistered.", drv);
            return 0;
        }

        prev = &cur->next;
        cur  = cur->next;
    }

    LOG_ERR("No NPU driver handle registered at address %p.", drv);

    return -1;
}

static struct ethosu_driver *ethosu_find_and_reserve_driver(void)
{
    struct ethosu_driver *drv = registered_drivers;

    while (drv != NULL)
    {
        if (!drv->reserved)
        {
            drv->reserved = true;
            LOG_DEBUG("NPU driver handle %p reserved.", drv);
            return drv;
        }
        drv = drv->next;
    }

    LOG_DEBUG("No NPU driver handle available.", drv);

    return NULL;
}

static int handle_optimizer_config(struct ethosu_driver *drv, struct opt_cfg_s *opt_cfg_p)
{
    LOG_INFO("Optimizer release nbr: %d patch: %d", opt_cfg_p->da_data.rel_nbr, opt_cfg_p->da_data.patch_nbr);

    if (ethosu_dev_verify_optimizer_config(drv->dev, opt_cfg_p->cfg, opt_cfg_p->id) != true)
    {
        return -1;
    }

    return 0;
}

static int handle_command_stream(struct ethosu_driver *drv,
                                 const uint8_t *cmd_stream,
                                 const int cms_length,
                                 const uint64_t *base_addr,
                                 const size_t *base_addr_size,
                                 const int num_base_addr)
{
    uint32_t cms_bytes       = cms_length * BYTES_IN_32_BITS;
    ptrdiff_t cmd_stream_ptr = (ptrdiff_t)cmd_stream;

    LOG_INFO("handle_command_stream: cmd_stream=%p, cms_length %d", cmd_stream, cms_length);

    if (0 != ((ptrdiff_t)cmd_stream & MASK_16_BYTE_ALIGN))
    {
        LOG_ERR("Command stream addr %p not aligned to 16 bytes", cmd_stream);
        return -1;
    }

    // Verify 16 byte alignment for base address'
    for (int i = 0; i < num_base_addr; i++)
    {
        if (0 != (base_addr[i] & MASK_16_BYTE_ALIGN))
        {
            LOG_ERR("Base addr %d: 0x%llx not aligned to 16 bytes", i, base_addr[i]);
            return -1;
        }
    }

    /* Flush the cache if available on CPU.
     * The upcasting to uin32_t* is ok since the pointer never is dereferenced.
     * The base_addr_size is null if invoking from prior to invoke_V2, in that case
     * the whole cache is being flushed.
     */

    if (base_addr_size != NULL)
    {
        ethosu_flush_dcache((uint32_t *)cmd_stream_ptr, cms_bytes);
        for (int i = 0; i < num_base_addr; i++)
        {
            ethosu_flush_dcache((uint32_t *)(uintptr_t)base_addr[i], base_addr_size[i]);
        }
    }
    else
    {
        ethosu_flush_dcache(NULL, 0);
    }

    // Execute the command stream
    if (ETHOSU_SUCCESS != ethosu_dev_run_command_stream(drv->dev, cmd_stream, cms_bytes, base_addr, num_base_addr))
    {
        return -1;
    }

    wait_for_irq(drv);

    // Check if any error occured
    if (drv->status_error)
    {
        return -1;
    }

    if (base_addr_size != NULL)
    {
        for (int i = 0; i < num_base_addr; i++)
        {
            ethosu_invalidate_dcache((uint32_t *)(uintptr_t)base_addr[i], base_addr_size[i]);
        }
    }
    else
    {
        ethosu_invalidate_dcache(NULL, 0);
    }

    return 0;
}

/******************************************************************************
 * Weak functions - Interrupt handler
 ******************************************************************************/
void __attribute__((weak)) ethosu_irq_handler(struct ethosu_driver *drv)
{
    LOG_DEBUG("Got interrupt from Ethos-U");

    drv->irq_triggered = true;
    if (!ethosu_dev_handle_interrupt(drv->dev))
    {
        drv->status_error = true;
    }
    ethosu_semaphore_give(drv->semaphore);
}

/******************************************************************************
 * Functions API
 ******************************************************************************/

int ethosu_init(struct ethosu_driver *drv,
                const void *base_address,
                const void *fast_memory,
                const size_t fast_memory_size,
                uint32_t secure_enable,
                uint32_t privilege_enable)
{
    LOG_INFO("Initializing NPU: base_address=%p, fast_memory=%p, fast_memory_size=%zu, secure=%" PRIu32
             ", privileged=%" PRIu32,
             base_address,
             fast_memory,
             fast_memory_size,
             secure_enable,
             privilege_enable);

    if (!ethosu_mutex)
    {
        ethosu_mutex = ethosu_mutex_create();
    }

    if (!ethosu_semaphore)
    {
        ethosu_semaphore = ethosu_semaphore_create();
    }

    drv->fast_memory      = (uint32_t)fast_memory;
    drv->fast_memory_size = fast_memory_size;
    drv->irq_triggered    = false;

    // Initialize the device and set requested security state and privilege mode
    drv->dev = ethosu_dev_init(base_address, secure_enable, privilege_enable);

    if (drv->dev == NULL)
    {
        LOG_ERR("Failed to initialize Ethos-U device");
        return -1;
    }

    // Power always ON requested
    if (drv->dev_power_always_on)
    {
        if (set_clock_and_power_request(drv, ETHOSU_INFERENCE_REQUEST, ETHOSU_CLOCK_Q_ENABLE, ETHOSU_POWER_Q_DISABLE) !=
            ETHOSU_SUCCESS)
        {
            LOG_ERR("Failed to disable power-q for Ethos-U");
            return -1;
        }
    }

    drv->semaphore    = ethosu_semaphore_create();
    drv->status_error = false;

    ethosu_register_driver(drv);

    return 0;
}

void ethosu_deinit(struct ethosu_driver *drv)
{
    ethosu_deregister_driver(drv);
    ethosu_semaphore_destroy(drv->semaphore);
    ethosu_dev_deinit(drv->dev);
    drv->dev = NULL;
}

void ethosu_get_driver_version(struct ethosu_driver_version *ver)
{
    assert(ver != NULL);
    ver->major = ETHOSU_DRIVER_VERSION_MAJOR;
    ver->minor = ETHOSU_DRIVER_VERSION_MINOR;
    ver->patch = ETHOSU_DRIVER_VERSION_PATCH;
}

void ethosu_get_hw_info(struct ethosu_driver *drv, struct ethosu_hw_info *hw)
{
    assert(hw != NULL);
    ethosu_dev_get_hw_info(drv->dev, hw);
}

int ethosu_invoke(struct ethosu_driver *drv,
                  const void *custom_data_ptr,
                  const int custom_data_size,
                  const uint64_t *base_addr,
                  const size_t *base_addr_size,
                  const int num_base_addr)
{
    const struct cop_data_s *data_ptr = custom_data_ptr;
    const struct cop_data_s *data_end = custom_data_ptr + custom_data_size;
    int return_code                   = 0;

    // First word in custom_data_ptr should contain "Custom Operator Payload 1"
    if (data_ptr->word != ETHOSU_FOURCC)
    {
        LOG_ERR("Custom Operator Payload: %" PRIu32 " is not correct, expected %x", data_ptr->word, ETHOSU_FOURCC);
        return -1;
    }

    // Custom data length must be a multiple of 32 bits
    if ((custom_data_size % BYTES_IN_32_BITS) != 0)
    {
        LOG_ERR("custom_data_size=0x%x not a multiple of 4", custom_data_size);
        return -1;
    }

    ++data_ptr;

    // Adjust base address to fast memory area
    if (drv->fast_memory != 0 && num_base_addr >= FAST_MEMORY_BASE_ADDR_INDEX)
    {
        uint64_t *fast_memory = (uint64_t *)&base_addr[FAST_MEMORY_BASE_ADDR_INDEX];

        if (base_addr_size != NULL && base_addr_size[FAST_MEMORY_BASE_ADDR_INDEX] > drv->fast_memory_size)
        {
            LOG_ERR("Fast memory area too small. fast_memory_size=%u, base_addr_size=%u",
                    drv->fast_memory_size,
                    base_addr_size[FAST_MEMORY_BASE_ADDR_INDEX]);
            return -1;
        }

        *fast_memory = drv->fast_memory;
    }

    // NPU might have lost power and thus its settings and state
    if (!drv->dev_power_always_on)
    {
        bool axi_reinit = true;
        // Only soft reset if security state or privilege level needs changing
        if (ethosu_dev_verify_access_state(drv->dev) != true)
        {
            if (ethosu_dev_soft_reset(drv->dev) != ETHOSU_SUCCESS)
            {
                return -1;
            }
            axi_reinit = false;
        }

        // Set power ON during the inference
        set_clock_and_power_request(drv, ETHOSU_INFERENCE_REQUEST, ETHOSU_CLOCK_Q_ENABLE, ETHOSU_POWER_Q_DISABLE);

        // If a soft reset occured, AXI reinit has already been performed
        if (axi_reinit)
        {
            ethosu_dev_axi_init(drv->dev);
        }
    }

    drv->status_error = false;

    ethosu_inference_begin(drv, custom_data_ptr);
    while (data_ptr < data_end)
    {
        int ret = 0;
        switch (data_ptr->driver_action_command)
        {
        case OPTIMIZER_CONFIG:
            LOG_DEBUG("OPTIMIZER_CONFIG");
            struct opt_cfg_s *opt_cfg_p = (struct opt_cfg_s *)data_ptr;

            ret = handle_optimizer_config(drv, opt_cfg_p);
            data_ptr += DRIVER_ACTION_LENGTH_32_BIT_WORD + OPTIMIZER_CONFIG_LENGTH_32_BIT_WORD;
            break;
        case COMMAND_STREAM:
            LOG_DEBUG("COMMAND_STREAM");
            void *command_stream = (uint8_t *)(data_ptr) + sizeof(struct cop_data_s);
            int cms_length       = (data_ptr->reserved << 16) | data_ptr->length;

            // It is safe to clear this flag without atomic, because npu is not running.
            drv->irq_triggered = false;

            ret = handle_command_stream(drv, command_stream, cms_length, base_addr, base_addr_size, num_base_addr);
            if (ret < 0)
            {
                LOG_ERR("Inference failed.");
            }

            data_ptr += DRIVER_ACTION_LENGTH_32_BIT_WORD + cms_length;
            break;
        case NOP:
            LOG_DEBUG("NOP");
            data_ptr += DRIVER_ACTION_LENGTH_32_BIT_WORD;
            break;
        default:
            LOG_ERR("UNSUPPORTED driver_action_command: %d", data_ptr->driver_action_command);
            ret = -1;
            break;
        }
        if (ret != 0)
        {
            return_code = -1;
            break;
        }
    }

    ethosu_inference_end(drv, custom_data_ptr);

    if (!drv->status_error && !drv->dev_power_always_on)
    {
        set_clock_and_power_request(drv, ETHOSU_INFERENCE_REQUEST, ETHOSU_CLOCK_Q_ENABLE, ETHOSU_POWER_Q_ENABLE);
    }

    return return_code;
}

void ethosu_set_power_mode(struct ethosu_driver *drv, bool always_on)
{
    drv->dev_power_always_on = always_on;

    if (always_on && ethosu_dev_verify_access_state(drv->dev) == false)
    {
        // Reset to enter correct security state/privilege mode
        if (ethosu_dev_soft_reset(drv->dev) == false)
        {
            LOG_ERR("Failed to set power mode for Ethos-U");
            return;
        }
    }

    ethosu_dev_set_clock_and_power(
        drv->dev, ETHOSU_CLOCK_Q_UNCHANGED, always_on ? ETHOSU_POWER_Q_DISABLE : ETHOSU_POWER_Q_ENABLE);
}

struct ethosu_driver *ethosu_reserve_driver(void)
{
    struct ethosu_driver *drv = NULL;

    do
    {
        ethosu_mutex_lock(ethosu_mutex);
        drv = ethosu_find_and_reserve_driver();
        ethosu_mutex_unlock(ethosu_mutex);

        if (drv != NULL)
        {
            break;
        }

        LOG_INFO("Waiting for NPU driver handle to become available...");
        ethosu_semaphore_take(ethosu_semaphore);

    } while (1);

    return drv;
}

void ethosu_release_driver(struct ethosu_driver *drv)
{
    ethosu_mutex_lock(ethosu_mutex);
    if (drv != NULL && drv->reserved)
    {
        drv->reserved = false;
        LOG_DEBUG("NPU driver handle %p released", drv);
        ethosu_semaphore_give(ethosu_semaphore);
    }
    ethosu_mutex_unlock(ethosu_mutex);
}

enum ethosu_error_codes set_clock_and_power_request(struct ethosu_driver *drv,
                                                    enum ethosu_request_clients client,
                                                    enum ethosu_clock_q_request clock_request,
                                                    enum ethosu_power_q_request power_request)
{
    // Keep track of which client requests clock gating to be disabled
    if (clock_request == ETHOSU_CLOCK_Q_DISABLE)
    {
        drv->clock_request |= (1 << client);
    }
    else if (clock_request == ETHOSU_CLOCK_Q_ENABLE) // Remove client from bitmask
    {
        drv->clock_request &= ~(1 << client);
    }

    // Only enable clock gating when no client has asked for it to be disabled
    clock_request = drv->clock_request == 0 ? ETHOSU_CLOCK_Q_ENABLE : ETHOSU_CLOCK_Q_DISABLE;

    // Keep track of which client requests power gating to be disabled
    if (power_request == ETHOSU_POWER_Q_DISABLE)
    {
        drv->power_request |= (1 << client);
    }
    else if (power_request == ETHOSU_POWER_Q_ENABLE)
    {
        drv->power_request &= ~(1 << client);
    }

    // Override if power has been requested to be always on
    if (drv->dev_power_always_on == true)
    {
        power_request = ETHOSU_POWER_Q_DISABLE;
    }
    else
    {
        // Only enable power gating when no client has asked for it to be disabled
        power_request = drv->power_request == 0 ? ETHOSU_POWER_Q_ENABLE : ETHOSU_POWER_Q_DISABLE;
    }

    // Verify security state and privilege mode if power is requested to be on
    if (power_request == ETHOSU_POWER_Q_DISABLE)
    {
        if (ethosu_dev_verify_access_state(drv->dev) == false)
        {
            if (ethosu_dev_soft_reset(drv->dev) != ETHOSU_SUCCESS)
            {
                LOG_ERR("Failed to set clock and power q channels for Ethos-U");
                return ETHOSU_GENERIC_FAILURE;
            }
        }
    }
    // Set clock and power
    return ethosu_dev_set_clock_and_power(drv->dev, clock_request, power_request);
}

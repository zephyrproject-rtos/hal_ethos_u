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
#include "ethosu_common.h"
#include "ethosu_config.h"
#include "ethosu_device.h"

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

#define MACS_PER_CYCLE_LOG2_MASK 0x000F
#define SHRAM_SIZE_MASK 0xFF00
#define SHRAM_SIZE_RIGHT_SHIFT 8
#define BYTES_IN_32_BITS 4
#define CUSTOM_OPTION_LENGTH_32_BIT_WORD 1
#define DRIVER_ACTION_LENGTH_32_BIT_WORD 1
#define OPTIMIZER_CONFIG_LENGTH_32_BIT_WORD 2
#define ETHOSU_FOURCC ('1' << 24 | 'P' << 16 | 'O' << 8 | 'C') // "Custom Operator Payload 1"
#define APB_START_ADDR_MASK 0x0FFF
#define APB_NUM_REG_BIT_SHIFT 12
#define BYTES_1KB 1024
#define PRODUCT_MAJOR_ETHOSU55 (4)
#define MASK_16_BYTE_ALIGN (0xF)
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
    READ_APB_REG     = 3,
    DUMP_SHRAM       = 4,
    NOP              = 5,
};

// Custom data struct
struct custom_data_s
{
    union
    {
        // Driver action data
        struct
        {
            // Driver action command (valid values in DRIVER_ACTION_e)
            uint8_t driver_action_command;

            // reserved
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

                // DA_CMD_READAPB
                struct
                {
                    uint16_t start_address : 12;
                    uint16_t nbr_reg_minus1 : 4;
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
    struct custom_data_s da_data;
    union
    {
        struct
        {
            uint32_t macs_per_cc : 4;
            uint32_t cmd_stream_version : 4;
            uint32_t shram_size : 8;
            uint32_t reserved0 : 11;
            uint32_t custom_dma : 1;
            uint32_t product : 4;
        };
        uint32_t npu_cfg;
    };
    union
    {
        struct
        {
            uint32_t version_status : 4;
            uint32_t version_minor : 4;
            uint32_t version_major : 4;
            uint32_t product_major : 4;
            uint32_t arch_patch_rev : 4;
            uint32_t arch_minor_rev : 8;
            uint32_t arch_major_rev : 4;
        };
        uint32_t ethosu_id;
    };
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
        if (drv->irq_triggered || drv->abort_inference)
        {
            drv->irq_triggered = false;
            break;
        }

        ethosu_semaphore_take(drv->semaphore);
    }
}

static void npu_axi_init(struct ethosu_driver *drv)
{
    ethosu_dev_set_qconfig(&drv->dev, NPU_QCONFIG);

    ethosu_dev_set_regioncfg(&drv->dev, 0, NPU_REGIONCFG_0);
    ethosu_dev_set_regioncfg(&drv->dev, 1, NPU_REGIONCFG_1);
    ethosu_dev_set_regioncfg(&drv->dev, 2, NPU_REGIONCFG_2);
    ethosu_dev_set_regioncfg(&drv->dev, 3, NPU_REGIONCFG_3);
    ethosu_dev_set_regioncfg(&drv->dev, 4, NPU_REGIONCFG_4);
    ethosu_dev_set_regioncfg(&drv->dev, 5, NPU_REGIONCFG_5);
    ethosu_dev_set_regioncfg(&drv->dev, 6, NPU_REGIONCFG_6);
    ethosu_dev_set_regioncfg(&drv->dev, 7, NPU_REGIONCFG_7);

    (void)ethosu_dev_set_axi_limit0(&drv->dev,
                                    AXI_LIMIT0_MAX_BEATS_BYTES,
                                    AXI_LIMIT0_MEM_TYPE,
                                    AXI_LIMIT0_MAX_OUTSTANDING_READS,
                                    AXI_LIMIT0_MAX_OUTSTANDING_WRITES);
    (void)ethosu_dev_set_axi_limit1(&drv->dev,
                                    AXI_LIMIT1_MAX_BEATS_BYTES,
                                    AXI_LIMIT1_MEM_TYPE,
                                    AXI_LIMIT1_MAX_OUTSTANDING_READS,
                                    AXI_LIMIT1_MAX_OUTSTANDING_WRITES);
    (void)ethosu_dev_set_axi_limit2(&drv->dev,
                                    AXI_LIMIT2_MAX_BEATS_BYTES,
                                    AXI_LIMIT2_MEM_TYPE,
                                    AXI_LIMIT2_MAX_OUTSTANDING_READS,
                                    AXI_LIMIT2_MAX_OUTSTANDING_WRITES);
    (void)ethosu_dev_set_axi_limit3(&drv->dev,
                                    AXI_LIMIT3_MAX_BEATS_BYTES,
                                    AXI_LIMIT3_MEM_TYPE,
                                    AXI_LIMIT3_MAX_OUTSTANDING_READS,
                                    AXI_LIMIT3_MAX_OUTSTANDING_WRITES);
}

static void ethosu_register_driver(struct ethosu_driver *drv)
{
    // Register driver as new HEAD of list
    drv->next          = registered_drivers;
    registered_drivers = drv;

    LOG_INFO("New NPU driver registered (handle: 0x%p, NPU: 0x%x)", drv, drv->dev.base_address);
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

static int ethosu_soft_reset_and_restore(struct ethosu_driver *drv)
{

    if (ETHOSU_SUCCESS != ethosu_dev_soft_reset(&drv->dev))
    {
        return -1;
    }

    set_clock_and_power_request(drv, ETHOSU_INFERENCE_REQUEST, ETHOSU_CLOCK_Q_ENABLE, ETHOSU_POWER_Q_DISABLE);

    npu_axi_init(drv);
    ethosu_dev_restore_pmu_config(&drv->dev);

    return 0;
}

static int handle_optimizer_config(struct ethosu_driver *drv, struct opt_cfg_s *opt_cfg_p)
{
    struct ethosu_config cfg;
    struct ethosu_id id;
    int return_code = 0;

    LOG_INFO("Optimizer release nbr: %d patch: %d", opt_cfg_p->da_data.rel_nbr, opt_cfg_p->da_data.patch_nbr);
    LOG_INFO("Optimizer config cmd_stream_version: %d macs_per_cc: %d shram_size: %d custom_dma: %d",
             opt_cfg_p->cmd_stream_version,
             opt_cfg_p->macs_per_cc,
             opt_cfg_p->shram_size,
             opt_cfg_p->custom_dma);
    LOG_INFO("Optimizer config Ethos-U version: %d.%d.%d",
             opt_cfg_p->arch_major_rev,
             opt_cfg_p->arch_minor_rev,
             opt_cfg_p->arch_patch_rev);

    (void)ethosu_dev_get_config(&drv->dev, &cfg);
    (void)ethosu_dev_get_id(&drv->dev, &id);
    LOG_INFO("Ethos-U config cmd_stream_version: %" PRIu32 " macs_per_cc: %" PRIu32 " shram_size: %" PRIu32
             " custom_dma: %" PRIu32 "",
             cfg.cmd_stream_version,
             cfg.macs_per_cc,
             cfg.shram_size,
             cfg.custom_dma);
    LOG_INFO("Ethos-U version: %" PRIu32 ".%" PRIu32 ".%" PRIu32 "",
             id.arch_major_rev,
             id.arch_minor_rev,
             id.arch_patch_rev);

    if ((cfg.macs_per_cc != opt_cfg_p->macs_per_cc) || (cfg.shram_size != opt_cfg_p->shram_size) ||
        (cfg.cmd_stream_version != opt_cfg_p->cmd_stream_version) || (!cfg.custom_dma && opt_cfg_p->custom_dma))
    {
        if (cfg.macs_per_cc != opt_cfg_p->macs_per_cc)
        {
            LOG_ERR("NPU config mismatch: npu.macs_per_cc=%" PRIu32 " optimizer.macs_per_cc=%d",
                    cfg.macs_per_cc,
                    opt_cfg_p->macs_per_cc);
        }
        if (cfg.shram_size != opt_cfg_p->shram_size)
        {
            LOG_ERR("NPU config mismatch: npu.shram_size=%" PRIu32 " optimizer.shram_size=%d",
                    cfg.shram_size,
                    opt_cfg_p->shram_size);
        }
        if (cfg.cmd_stream_version != opt_cfg_p->cmd_stream_version)
        {
            LOG_ERR("NPU config mismatch: npu.cmd_stream_version=%" PRIu32 " optimizer.cmd_stream_version=%d",
                    cfg.cmd_stream_version,
                    opt_cfg_p->cmd_stream_version);
        }
        if (!cfg.custom_dma && opt_cfg_p->custom_dma)
        {
            LOG_ERR("NPU config mismatch: npu.custom_dma=%" PRIu32 " optimize.custom_dma=%d",
                    cfg.custom_dma,
                    opt_cfg_p->custom_dma);
        }
        return_code = -1;
    }

    if ((id.arch_major_rev != opt_cfg_p->arch_major_rev) || (id.arch_minor_rev < opt_cfg_p->arch_minor_rev))
    {
        LOG_ERR("NPU arch mismatch: npu.arch=%" PRIu32 ".%" PRIu32 ".%" PRIu32 " optimizer.arch=%d.%d.%d",
                id.arch_major_rev,
                id.arch_minor_rev,
                id.arch_patch_rev,
                opt_cfg_p->arch_major_rev,
                opt_cfg_p->arch_minor_rev,
                opt_cfg_p->arch_patch_rev);
        return_code = -1;
    }

#if !defined(LOG_ENABLED)
    UNUSED(opt_cfg_p);
#endif
    return return_code;
}

static int handle_command_stream(struct ethosu_driver *drv,
                                 const uint8_t *cmd_stream,
                                 const int cms_length,
                                 const uint64_t *base_addr,
                                 const size_t *base_addr_size,
                                 const int num_base_addr)
{
    uint32_t qread           = 0;
    uint32_t cms_bytes       = cms_length * BYTES_IN_32_BITS;
    ptrdiff_t cmd_stream_ptr = (ptrdiff_t)cmd_stream;

    LOG_INFO("handle_command_stream: cmd_stream=%p, cms_length %d", cmd_stream, cms_length);

    if (0 != ((ptrdiff_t)cmd_stream & MASK_16_BYTE_ALIGN))
    {
        LOG_ERR("Command stream addr %p not aligned to 16 bytes", cmd_stream);
        return -1;
    }

    bool base_addr_invalid = false;
    for (int i = 0; i < num_base_addr; i++)
    {
        if (0 != (base_addr[i] & MASK_16_BYTE_ALIGN))
        {
            LOG_ERR("Base addr %d: 0x%llx not aligned to 16 bytes", i, base_addr[i]);
            base_addr_invalid = true;
        }
    }

    if (base_addr_invalid)
    {
        return -1;
    }

    /* Flush the cache if available on our CPU.
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

    if (ETHOSU_SUCCESS != ethosu_dev_run_command_stream(&drv->dev, cmd_stream, cms_bytes, base_addr, num_base_addr))
    {
        return -1;
    }

    wait_for_irq(drv);

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

    qread = ethosu_dev_get_qread(&drv->dev);
    if (qread != cms_bytes)
    {
        LOG_WARN("IRQ received but qread (%" PRIu32 ") not at end of stream (%" PRIu32 ").", qread, cms_bytes);
        return -1;
    }

    return 0;
}

static int read_apb_reg(struct ethosu_driver *drv, uint16_t da_data)
{
    uint32_t *reg_p;
    uint32_t start_address = (uint32_t)(da_data & APB_START_ADDR_MASK);
    uint16_t num_reg       = (da_data >> APB_NUM_REG_BIT_SHIFT) + 1;

    reg_p = (uint32_t *)malloc(num_reg * sizeof(uint32_t));
    if (reg_p == NULL)
    {
        LOG_ERR("Memory allocation failed");
        return -1;
    }

    if (ETHOSU_SUCCESS == ethosu_dev_read_apb_reg(&drv->dev, start_address, num_reg, reg_p))
    {
        for (int i = 0; i < num_reg; i++)
        {
            LOG_INFO(
                "NPU_REG ADDR 0x%04" PRIu32 " = 0x%08" PRIu32 "", (start_address + (i * BYTES_IN_32_BITS)), reg_p[i]);
        }
    }
    else
    {
        free(reg_p);
        return -1;
    }

    free(reg_p);
    return 0;
}

static int dump_shram(struct ethosu_driver *drv)
{
    struct ethosu_config cfg;
    uint32_t *shram_p;
    (void)ethosu_dev_get_config(&drv->dev, &cfg);

    LOG_INFO("dump_shram size = %" PRIu32 " KB", cfg.shram_size);

    shram_p = (uint32_t *)malloc(BYTES_1KB);
    if (shram_p == NULL)
    {
        LOG_ERR("Memory allocation failed for shram data");
        return -1;
    }

    for (uint32_t i = 0; i < cfg.shram_size; i++)
    {
        ethosu_dev_get_shram_data(&drv->dev, i, (uint32_t *)shram_p);
        // Output 1KB of SHRAM
        LOG_INFO("***SHRAM SECTION %" PRIu32 "***", i);
        for (int j = 0; j < (BYTES_1KB / BYTES_IN_32_BITS); j++)
        {
            LOG_INFO("[0x%04" PRIx32 "] %" PRIx32 "", (i * 1024 + j * 4), shram_p[j]);
        }
    }
    free(shram_p);

    return 0;
}

/******************************************************************************
 * Weak functions - Interrupt handler
 ******************************************************************************/
void __attribute__((weak)) ethosu_irq_handler(struct ethosu_driver *drv)
{
    uint8_t irq_raised = 0;

    LOG_DEBUG("Interrupt. status=0x%08x, qread=%d", ethosu_dev_get_status(&drv->dev), ethosu_dev_get_qread(&drv->dev));

    // Verify that interrupt has been raised
    (void)ethosu_dev_is_irq_raised(&drv->dev, &irq_raised);
    assert(irq_raised == 1);
    drv->irq_triggered = true;

    // Clear interrupt
    (void)ethosu_dev_clear_irq_status(&drv->dev);

    // Verify that interrupt has been successfully cleared
    (void)ethosu_dev_is_irq_raised(&drv->dev, &irq_raised);
    assert(irq_raised == 0);

    if (ethosu_dev_status_has_error(&drv->dev))
    {
        ethosu_soft_reset_and_restore(drv);
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
    int return_code = 0;

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
    drv->semaphore        = ethosu_semaphore_create();

    if (ETHOSU_SUCCESS != ethosu_dev_init(&drv->dev, base_address, secure_enable, privilege_enable))
    {
        LOG_ERR("Failed to initialize Ethos-U device");
        return -1;
    }

    if (ETHOSU_SUCCESS !=
        set_clock_and_power_request(drv, ETHOSU_INFERENCE_REQUEST, ETHOSU_CLOCK_Q_DISABLE, ETHOSU_POWER_Q_DISABLE))
    {
        LOG_ERR("Failed to disable clock-q & power-q for Ethos-U");
        return -1;
    }

    if (ETHOSU_SUCCESS != ethosu_dev_soft_reset(&drv->dev))
    {
        return -1;
    }

    if (ETHOSU_SUCCESS != ethosu_dev_wait_for_reset(&drv->dev))
    {
        LOG_ERR("Failed reset of Ethos-U");
        return -1;
    }

    drv->status_error = false;

    ethosu_register_driver(drv);

    return return_code;
}

void ethosu_deinit(struct ethosu_driver *drv)
{
    ethosu_deregister_driver(drv);
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

    (void)ethosu_dev_get_id(&drv->dev, &hw->version);
    (void)ethosu_dev_get_config(&drv->dev, &hw->cfg);
}

int ethosu_invoke(struct ethosu_driver *drv,
                  const void *custom_data_ptr,
                  const int custom_data_size,
                  const uint64_t *base_addr,
                  const size_t *base_addr_size,
                  const int num_base_addr)
{
    const struct custom_data_s *data_ptr = custom_data_ptr;
    const struct custom_data_s *data_end = custom_data_ptr + custom_data_size;
    int return_code                      = 0;

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

    if (!drv->dev_power_always_on)
    {
        // Only soft reset if security state or privilege level needs changing
        if (ethosu_dev_prot_has_changed(&drv->dev))
        {
            if (ETHOSU_SUCCESS != ethosu_dev_soft_reset(&drv->dev))
            {
                return -1;
            }
        }

        drv->status_error = false;
        set_clock_and_power_request(drv, ETHOSU_INFERENCE_REQUEST, ETHOSU_CLOCK_Q_ENABLE, ETHOSU_POWER_Q_DISABLE);
        ethosu_dev_restore_pmu_config(&drv->dev);
        npu_axi_init(drv);
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
            void *command_stream = (uint8_t *)(data_ptr) + sizeof(struct custom_data_s);
            int cms_length       = (data_ptr->reserved << 16) | data_ptr->length;

            drv->abort_inference = false;
            // It is safe to clear this flag without atomic, because npu is not running.
            drv->irq_triggered = false;

            ret = handle_command_stream(drv, command_stream, cms_length, base_addr, base_addr_size, num_base_addr);

            if (return_code == -1 && drv->abort_inference)
            {
                LOG_ERR("NPU timeout. qread=%" PRIu32, ethosu_dev_get_qread(&drv->dev));
                dump_shram(drv);
            }

            data_ptr += DRIVER_ACTION_LENGTH_32_BIT_WORD + cms_length;
            break;
        case READ_APB_REG:
            LOG_DEBUG("READ_APB_REG");
            ret = read_apb_reg(drv, data_ptr->driver_action_data);
            data_ptr += DRIVER_ACTION_LENGTH_32_BIT_WORD;
            break;
        case DUMP_SHRAM:
            LOG_DEBUG("DUMP_SHRAM");
            ret = dump_shram(drv);
            data_ptr += DRIVER_ACTION_LENGTH_32_BIT_WORD;
            break;
        case NOP:
            LOG_DEBUG("NOP");
            data_ptr += DRIVER_ACTION_LENGTH_32_BIT_WORD;
            break;
        default:
            LOG_ERR("UNSUPPORTED driver_action_command: %d ", data_ptr->driver_action_command);
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
        ethosu_dev_save_pmu_counters(&drv->dev);
        set_clock_and_power_request(drv, ETHOSU_INFERENCE_REQUEST, ETHOSU_CLOCK_Q_ENABLE, ETHOSU_POWER_Q_ENABLE);
    }

    return return_code;
}

void ethosu_abort(struct ethosu_driver *drv)
{
    drv->abort_inference = true;
}

void ethosu_set_power_mode(struct ethosu_driver *drv, bool always_on)
{
    drv->dev_power_always_on = always_on;

    if (always_on)
    {
        npu_axi_init(drv);
    }
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
    // Set clock request bit for client
    if (clock_request == ETHOSU_CLOCK_Q_DISABLE)
    {
        drv->clock_request |= (1 << client);
    }
    else
    {
        drv->clock_request &= ~(1 << client);
    }
    // Get current clock request (ENABLE if both PMU and INFERENCE asks for clock request, else DISABLE)
    clock_request = drv->clock_request == 0 ? ETHOSU_CLOCK_Q_ENABLE : ETHOSU_CLOCK_Q_DISABLE;

    // Set power request bit for client
    if (power_request == ETHOSU_POWER_Q_DISABLE)
    {
        drv->power_request |= (1 << client);
    }
    else
    {
        drv->power_request &= ~(1 << client);
    }
    // Get current power request (ENABLE if both PMU and INFERENCE asks for power request, else DISABLE)
    power_request = drv->power_request == 0 ? ETHOSU_POWER_Q_ENABLE : ETHOSU_POWER_Q_DISABLE;

    // Set clock and power
    enum ethosu_error_codes ret = ethosu_dev_set_clock_and_power(&drv->dev, clock_request, power_request);

    return ret;
}

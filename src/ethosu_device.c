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

#include "ethosu_device.h"

#include "ethosu_common.h"
#include <assert.h>
#include <stdio.h>

#define MASK_0_31_BITS 0xFFFFFFFF
#define MASK_32_47_BITS 0xFFFF00000000
#define BASEP_OFFSET 4
#define REG_OFFSET 4
#define BYTES_1KB 1024

#if defined(ARM_NPU_STUB)
static uint32_t stream_length = 0;
#endif

enum ethosu_error_codes ethosu_dev_init(void)
{
    return ETHOSU_SUCCESS;
}

enum ethosu_error_codes ethosu_get_id(struct ethosu_id *id)
{
    struct id_r _id;

#if !defined(ARM_NPU_STUB)
    _id.word = read_reg(NPU_REG_ID);
#else
    _id.word           = 0;
    _id.arch_patch_rev = NNX_ARCH_VERSION_PATCH;
    _id.arch_minor_rev = NNX_ARCH_VERSION_MINOR;
    _id.arch_major_rev = NNX_ARCH_VERSION_MAJOR;
#endif

    id->version_status = _id.version_status;
    id->version_minor  = _id.version_minor;
    id->version_major  = _id.version_major;
    id->product_major  = _id.product_major;
    id->arch_patch_rev = _id.arch_patch_rev;
    id->arch_minor_rev = _id.arch_minor_rev;
    id->arch_major_rev = _id.arch_major_rev;

    return ETHOSU_SUCCESS;
}

enum ethosu_error_codes ethosu_get_config(struct ethosu_config *config)
{
    struct config_r cfg = {.word = 0};

#if !defined(ARM_NPU_STUB)
    cfg.word = read_reg(NPU_REG_CONFIG);
#endif

    config->macs_per_cc        = cfg.macs_per_cc;
    config->cmd_stream_version = cfg.cmd_stream_version;
    config->shram_size         = cfg.shram_size;

    return ETHOSU_SUCCESS;
}

enum ethosu_error_codes ethosu_run_command_stream(const uint8_t *cmd_stream_ptr,
                                                  uint32_t cms_length,
                                                  const uint64_t *base_addr,
                                                  int num_base_addr)
{
#if !defined(ARM_NPU_STUB)
    uint32_t qbase0;
    uint32_t qbase1;
    uint32_t qsize;
    uint32_t *reg_basep;
    int num_base_reg;

    ASSERT(num_base_addr <= ETHOSU_DRIVER_BASEP_INDEXES);

    qbase0       = ((uint64_t)cmd_stream_ptr) & MASK_0_31_BITS;
    qbase1       = (((uint64_t)cmd_stream_ptr) & MASK_32_47_BITS) >> 32;
    qsize        = cms_length;
    num_base_reg = num_base_addr * 2;
    reg_basep    = (uint32_t *)base_addr;

    for (int i = 0; i < num_base_reg; i++)
    {
        write_reg(NPU_REG_BASEP0 + (i * BASEP_OFFSET), reg_basep[i]);
    }

    write_reg(NPU_REG_QBASE0, qbase0);
    write_reg(NPU_REG_QBASE1, qbase1);
    write_reg(NPU_REG_QSIZE, qsize);
    write_reg(NPU_REG_CMD, 1);
#else
    // NPU stubbed
    stream_length = cms_length;
    UNUSED(cmd_stream_ptr);
    UNUSED(base_addr);
    ASSERT(num_base_addr < ETHOSU_DRIVER_BASEP_INDEXES);
#if defined(NDEBUG)
    UNUSED(num_base_addr);
#endif
#endif

    return ETHOSU_SUCCESS;
}

enum ethosu_error_codes ethosu_is_irq_raised(uint8_t *irq_raised)
{
#if !defined(ARM_NPU_STUB)
    struct status_r status;
    status.word = read_reg(NPU_REG_STATUS);
    if (status.irq_raised == 1)
    {
        *irq_raised = 1;
    }
    else
    {
        *irq_raised = 0;
    }
#else
    *irq_raised = 1;
#endif
    return ETHOSU_SUCCESS;
}

enum ethosu_error_codes ethosu_clear_irq_status(void)
{
#if !defined(ARM_NPU_STUB)
    write_reg(NPU_REG_CMD, 2);
#else
#endif
    return ETHOSU_SUCCESS;
}

// TODO Understand settings of privilege/sequrity level and update API.
enum ethosu_error_codes ethosu_soft_reset(void)
{
    enum ethosu_error_codes return_code = ETHOSU_SUCCESS;
#if !defined(ARM_NPU_STUB)
    struct reset_r reset;
    struct prot_r prot;

    reset.word        = 0;
    reset.pending_CPL = PRIVILEGE_LEVEL_USER;      // TODO, how to get the host priviledge level
    reset.pending_CSL = SECURITY_LEVEL_NON_SECURE; // TODO, how to get Security level

    prot.word = read_reg(NPU_REG_PROT);

    if (prot.active_CPL < reset.pending_CPL && prot.active_CSL > reset.pending_CSL)
    {
        // Register access not permitted
        return ETHOSU_GENERIC_FAILURE;
    }
    // Reset and set security level
    write_reg(NPU_REG_RESET, reset.word);

    return_code = ethosu_wait_for_reset();
#endif

    return return_code;
}

enum ethosu_error_codes ethosu_wait_for_reset(void)
{
#if !defined(ARM_NPU_STUB)
    struct status_r status;

    // Wait until reset status indicates that reset has been completed
    for (int i = 0; i < 100000; i++)
    {
        status.word = read_reg(NPU_REG_STATUS);
        if (0 == status.reset_status)
        {
            break;
        }
    }

    if (1 == status.reset_status)
    {
        return ETHOSU_GENERIC_FAILURE;
    }
#endif

    return ETHOSU_SUCCESS;
}

enum ethosu_error_codes ethosu_read_apb_reg(uint32_t start_address, uint16_t num_reg, uint32_t *reg)
{
#if !defined(ARM_NPU_STUB)
    uint32_t address = start_address;

    ASSERT((start_address + num_reg) < NPU_IDS_REGISTERS_SIZE);

    for (int i = 0; i < num_reg; i++)
    {
        reg[i] = read_reg(address);
        address += REG_OFFSET;
    }
#else
    // NPU stubbed
    UNUSED(start_address);
    UNUSED(num_reg);
    UNUSED(reg);
#endif

    return ETHOSU_SUCCESS;
}

enum ethosu_error_codes ethosu_set_qconfig(enum ethosu_memory_type memory_type)
{
    if (memory_type > ETHOSU_AXI1_OUTSTANDING_COUNTER3)
    {
        return ETHOSU_INVALID_PARAM;
    }
#if !defined(ARM_NPU_STUB)
    write_reg(NPU_REG_QCONFIG, memory_type);
#else
    // NPU stubbed
    UNUSED(memory_type);
#endif
    return ETHOSU_SUCCESS;
}

enum ethosu_error_codes ethosu_set_regioncfg(uint8_t region, enum ethosu_memory_type memory_type)
{
    if (region > 7)
    {
        return ETHOSU_INVALID_PARAM;
    }
#if !defined(ARM_NPU_STUB)
    struct regioncfg_r regioncfg;
    regioncfg.word = read_reg(NPU_REG_REGIONCFG);
    regioncfg.word &= ~(0x3 << (2 * region));
    regioncfg.word |= (memory_type & 0x3) << (2 * region);
    write_reg(NPU_REG_REGIONCFG, regioncfg.word);
#else
    // NPU stubbed
    UNUSED(region);
    UNUSED(memory_type);
#endif
    return ETHOSU_SUCCESS;
}

enum ethosu_error_codes ethosu_set_axi_limit0(enum ethosu_axi_limit_beats max_beats,
                                              enum ethosu_axi_limit_mem_type memtype,
                                              uint8_t max_reads,
                                              uint8_t max_writes)
{
#if !defined(ARM_NPU_STUB)
    struct axi_limit0_r axi_limit0;
    axi_limit0.max_beats                = max_beats;
    axi_limit0.memtype                  = memtype;
    axi_limit0.max_outstanding_read_m1  = max_reads - 1;
    axi_limit0.max_outstanding_write_m1 = max_writes - 1;

    write_reg(NPU_REG_AXI_LIMIT0, axi_limit0.word);
#else
    // NPU stubbed
    UNUSED(max_beats);
    UNUSED(memtype);
    UNUSED(max_reads);
    UNUSED(max_writes);
#endif

    return ETHOSU_SUCCESS;
}

enum ethosu_error_codes ethosu_set_axi_limit1(enum ethosu_axi_limit_beats max_beats,
                                              enum ethosu_axi_limit_mem_type memtype,
                                              uint8_t max_reads,
                                              uint8_t max_writes)
{
#if !defined(ARM_NPU_STUB)
    struct axi_limit1_r axi_limit1;
    axi_limit1.max_beats                = max_beats;
    axi_limit1.memtype                  = memtype;
    axi_limit1.max_outstanding_read_m1  = max_reads - 1;
    axi_limit1.max_outstanding_write_m1 = max_writes - 1;

    write_reg(NPU_REG_AXI_LIMIT1, axi_limit1.word);
#else
    // NPU stubbed
    UNUSED(max_beats);
    UNUSED(memtype);
    UNUSED(max_reads);
    UNUSED(max_writes);
#endif

    return ETHOSU_SUCCESS;
}

enum ethosu_error_codes ethosu_set_axi_limit2(enum ethosu_axi_limit_beats max_beats,
                                              enum ethosu_axi_limit_mem_type memtype,
                                              uint8_t max_reads,
                                              uint8_t max_writes)
{
#if !defined(ARM_NPU_STUB)
    struct axi_limit2_r axi_limit2;
    axi_limit2.max_beats                = max_beats;
    axi_limit2.memtype                  = memtype;
    axi_limit2.max_outstanding_read_m1  = max_reads - 1;
    axi_limit2.max_outstanding_write_m1 = max_writes - 1;

    write_reg(NPU_REG_AXI_LIMIT2, axi_limit2.word);
#else
    // NPU stubbed
    UNUSED(max_beats);
    UNUSED(memtype);
    UNUSED(max_reads);
    UNUSED(max_writes);
#endif

    return ETHOSU_SUCCESS;
}

enum ethosu_error_codes ethosu_set_axi_limit3(enum ethosu_axi_limit_beats max_beats,
                                              enum ethosu_axi_limit_mem_type memtype,
                                              uint8_t max_reads,
                                              uint8_t max_writes)
{
#if !defined(ARM_NPU_STUB)
    struct axi_limit3_r axi_limit3;
    axi_limit3.max_beats                = max_beats;
    axi_limit3.memtype                  = memtype;
    axi_limit3.max_outstanding_read_m1  = max_reads - 1;
    axi_limit3.max_outstanding_write_m1 = max_writes - 1;

    write_reg(NPU_REG_AXI_LIMIT3, axi_limit3.word);
#else
    // NPU stubbed
    UNUSED(max_beats);
    UNUSED(memtype);
    UNUSED(max_reads);
    UNUSED(max_writes);
#endif

    return ETHOSU_SUCCESS;
}

enum ethosu_error_codes ethosu_get_revision(uint32_t *revision)
{
#if !defined(ARM_NPU_STUB)
    *revision = read_reg(NPU_REG_REVISION);
#else
    *revision         = 0xDEADC0DE;
#endif
    return ETHOSU_SUCCESS;
}

enum ethosu_error_codes ethosu_get_qread(uint32_t *qread)
{
#if !defined(ARM_NPU_STUB)
    *qread = read_reg(NPU_REG_QREAD);
#else
    *qread            = stream_length;
#endif
    return ETHOSU_SUCCESS;
}

enum ethosu_error_codes ethosu_get_status_mask(uint16_t *status_mask)
{
#if !defined(ARM_NPU_STUB)
    struct status_r status;

    status.word  = read_reg(NPU_REG_STATUS);
    *status_mask = status.word & 0xFFFF;
#else
    *status_mask      = 0x0000;
#endif
    return ETHOSU_SUCCESS;
}

enum ethosu_error_codes ethosu_get_irq_history_mask(uint16_t *irq_history_mask)
{
#if !defined(ARM_NPU_STUB)
    struct status_r status;

    status.word       = read_reg(NPU_REG_STATUS);
    *irq_history_mask = status.irq_history_mask;
#else
    *irq_history_mask = 0xffff;
#endif
    return ETHOSU_SUCCESS;
}

enum ethosu_error_codes ethosu_clear_irq_history_mask(uint16_t irq_history_clear_mask)
{
#if !defined(ARM_NPU_STUB)
    write_reg(NPU_REG_CMD, (uint32_t)irq_history_clear_mask << 16);
#else
    UNUSED(irq_history_clear_mask);
#endif
    return ETHOSU_SUCCESS;
}

enum ethosu_error_codes ethosu_set_command_run(void)
{
#if !defined(ARM_NPU_STUB)
    write_reg(NPU_REG_CMD, 1);
#else
#endif
    return ETHOSU_SUCCESS;
}

enum ethosu_error_codes ethosu_get_shram_data(int section, uint32_t *shram_p)
{
#if !defined(ARM_NPU_STUB)
    int i            = 0;
    uint32_t address = NPU_REG_SHARED_BUFFER0;
    write_reg(NPU_REG_DEBUG_ADDR, section * BYTES_1KB);

    while (address <= NPU_REG_SHARED_BUFFER255)
    {
        shram_p[i] = read_reg(address);
        address += REG_OFFSET;
        i++;
    }
#else
    // NPU stubbed
    UNUSED(section);
    UNUSED(shram_p);
#endif

    return ETHOSU_SUCCESS;
}

enum ethosu_error_codes ethosu_set_clock_and_power(enum ethosu_clock_q_request clock_q,
                                                   enum ethosu_power_q_request power_q)
{
#if !defined(ARM_NPU_STUB)
    struct cmd_r cmd;
    cmd.word           = 0;
    cmd.clock_q_enable = clock_q;
    cmd.power_q_enable = power_q;
    write_reg(NPU_REG_CMD, cmd.word);
#else
    UNUSED(clock_q);
    UNUSED(power_q);
#endif
    return ETHOSU_SUCCESS;
}

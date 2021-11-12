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
#include "ethosu_interface.h"

#include "ethosu_config.h"
#include "ethosu_device.h"
#include "ethosu_log.h"

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#define BASEP_OFFSET 4

#ifdef ETHOSU65
#define ADDRESS_BITS 40
#else
#define ADDRESS_BITS 32
#endif

#define ADDRESS_MASK ((1ull << ADDRESS_BITS) - 1)

#define NPU_CMD_PWR_CLK_MASK (0xC)

struct ethosu_device *ethosu_dev_init(const void *base_address, uint32_t secure_enable, uint32_t privilege_enable)
{
    struct ethosu_device *dev = malloc(sizeof(struct ethosu_device));
    if (!dev)
    {
        LOG_ERR("Failed to allocate memory for Ethos-U device");
        return NULL;
    }

    dev->reg        = (volatile struct NPU_REG *)base_address;
    dev->secure     = secure_enable;
    dev->privileged = privilege_enable;

    // Make sure the NPU is in a known state
    if (ethosu_dev_soft_reset(dev) != ETHOSU_SUCCESS)
    {
        free(dev);
        return NULL;
    }

    return dev;
}

void ethosu_dev_deinit(struct ethosu_device *dev)
{
    free(dev);
}

enum ethosu_error_codes ethosu_dev_axi_init(struct ethosu_device *dev)
{
    struct regioncfg_r rcfg = {0};
    struct axi_limit0_r l0  = {0};
    struct axi_limit1_r l1  = {0};
    struct axi_limit2_r l2  = {0};
    struct axi_limit3_r l3  = {0};

    dev->reg->QCONFIG.word = NPU_QCONFIG;

    rcfg.region0             = NPU_REGIONCFG_0;
    rcfg.region1             = NPU_REGIONCFG_1;
    rcfg.region2             = NPU_REGIONCFG_2;
    rcfg.region3             = NPU_REGIONCFG_3;
    rcfg.region4             = NPU_REGIONCFG_4;
    rcfg.region5             = NPU_REGIONCFG_5;
    rcfg.region6             = NPU_REGIONCFG_6;
    rcfg.region7             = NPU_REGIONCFG_7;
    dev->reg->REGIONCFG.word = rcfg.word;

    l0.max_beats                = AXI_LIMIT0_MAX_BEATS_BYTES;
    l0.memtype                  = AXI_LIMIT0_MEM_TYPE;
    l0.max_outstanding_read_m1  = AXI_LIMIT0_MAX_OUTSTANDING_READS - 1;
    l0.max_outstanding_write_m1 = AXI_LIMIT0_MAX_OUTSTANDING_WRITES - 1;

    l1.max_beats                = AXI_LIMIT1_MAX_BEATS_BYTES;
    l1.memtype                  = AXI_LIMIT1_MEM_TYPE;
    l1.max_outstanding_read_m1  = AXI_LIMIT1_MAX_OUTSTANDING_READS - 1;
    l1.max_outstanding_write_m1 = AXI_LIMIT1_MAX_OUTSTANDING_WRITES - 1;

    l2.max_beats                = AXI_LIMIT2_MAX_BEATS_BYTES;
    l2.memtype                  = AXI_LIMIT2_MEM_TYPE;
    l2.max_outstanding_read_m1  = AXI_LIMIT2_MAX_OUTSTANDING_READS - 1;
    l2.max_outstanding_write_m1 = AXI_LIMIT2_MAX_OUTSTANDING_WRITES - 1;

    l3.max_beats                = AXI_LIMIT3_MAX_BEATS_BYTES;
    l3.memtype                  = AXI_LIMIT3_MEM_TYPE;
    l3.max_outstanding_read_m1  = AXI_LIMIT3_MAX_OUTSTANDING_READS - 1;
    l3.max_outstanding_write_m1 = AXI_LIMIT3_MAX_OUTSTANDING_WRITES - 1;

    dev->reg->AXI_LIMIT0.word = l0.word;
    dev->reg->AXI_LIMIT1.word = l1.word;
    dev->reg->AXI_LIMIT2.word = l2.word;
    dev->reg->AXI_LIMIT3.word = l3.word;

    return ETHOSU_SUCCESS;
}

enum ethosu_error_codes ethosu_dev_run_command_stream(struct ethosu_device *dev,
                                                      const uint8_t *cmd_stream_ptr,
                                                      uint32_t cms_length,
                                                      const uint64_t *base_addr,
                                                      int num_base_addr)
{
    assert(num_base_addr <= NPU_REG_BASEP_ARRLEN);

    struct cmd_r cmd;
    uint64_t qbase = (uintptr_t)cmd_stream_ptr + BASE_POINTER_OFFSET;
    assert(qbase <= ADDRESS_MASK);
    LOG_DEBUG("QBASE=0x%016llx, QSIZE=%u, base_pointer_offset=0x%08x", qbase, cms_length, BASE_POINTER_OFFSET);

    dev->reg->QBASE.word[0] = qbase & 0xffffffff;
#ifdef ETHOSU65
    dev->reg->QBASE.word[1] = qbase >> 32;
#endif
    dev->reg->QSIZE.word = cms_length;

    for (int i = 0; i < num_base_addr; i++)
    {
        uint64_t addr = base_addr[i] + BASE_POINTER_OFFSET;
        assert(addr <= ADDRESS_MASK);
        LOG_DEBUG("BASEP%d=0x%016llx", i, addr);
        dev->reg->BASEP[i].word[0] = addr & 0xffffffff;
#ifdef ETHOSU65
        dev->reg->BASEP[i].word[1] = addr >> 32;
#endif
    }

    cmd.word                        = dev->reg->CMD.word & NPU_CMD_PWR_CLK_MASK;
    cmd.transition_to_running_state = 1;

    dev->reg->CMD.word = cmd.word;
    LOG_DEBUG("CMD=0x%08x", cmd.word);

    return ETHOSU_SUCCESS;
}

bool ethosu_dev_handle_interrupt(struct ethosu_device *dev)
{
    struct cmd_r cmd;

    // Clear interrupt
    cmd.word           = dev->reg->CMD.word & NPU_CMD_PWR_CLK_MASK;
    cmd.clear_irq      = 1;
    dev->reg->CMD.word = cmd.word;

    // If a fault has occured, the NPU needs to be reset
    if (dev->reg->STATUS.bus_status || dev->reg->STATUS.cmd_parse_error || dev->reg->STATUS.wd_fault ||
        dev->reg->STATUS.ecc_fault)
    {
        LOG_DEBUG("NPU fault. status=0x%08x, qread=%" PRIu32, dev->reg->STATUS.word, dev->reg->QREAD.word);
        ethosu_dev_soft_reset(dev);
        ethosu_dev_set_clock_and_power(dev, ETHOSU_CLOCK_Q_UNCHANGED, ETHOSU_POWER_Q_DISABLE);
        return false;
    }

    // Verify that the cmd stream finished executing
    return dev->reg->STATUS.cmd_end_reached ? true : false;
}

bool ethosu_dev_verify_access_state(struct ethosu_device *dev)
{
    if (dev->reg->PROT.active_CSL != (dev->secure ? SECURITY_LEVEL_SECURE : SECURITY_LEVEL_NON_SECURE) ||
        dev->reg->PROT.active_CPL != (dev->privileged ? PRIVILEGE_LEVEL_PRIVILEGED : PRIVILEGE_LEVEL_USER))
    {
        return false;
    }
    return true;
}

enum ethosu_error_codes ethosu_dev_soft_reset(struct ethosu_device *dev)
{
    struct reset_r reset;

    reset.word        = 0;
    reset.pending_CPL = dev->privileged ? PRIVILEGE_LEVEL_PRIVILEGED : PRIVILEGE_LEVEL_USER;
    reset.pending_CSL = dev->secure ? SECURITY_LEVEL_SECURE : SECURITY_LEVEL_NON_SECURE;

    // Reset and set security level
    LOG_INFO("Soft reset NPU");
    dev->reg->RESET.word = reset.word;

    // Wait until reset status indicates that reset has been completed
    for (int i = 0; i < 100000 && dev->reg->STATUS.reset_status != 0; i++)
    {
    }

    if (dev->reg->STATUS.reset_status != 0)
    {
        LOG_ERR("Soft reset timed out");
        return ETHOSU_GENERIC_FAILURE;
    }

    // Verify that NPU has switched security state and privilege level
    if (ethosu_dev_verify_access_state(dev) != true)
    {
        LOG_ERR("Failed to switch security state and privilege level");
        return ETHOSU_GENERIC_FAILURE;
    }

    // Reinitialize AXI settings
    ethosu_dev_axi_init(dev);

    return ETHOSU_SUCCESS;
}

void ethosu_dev_get_hw_info(struct ethosu_device *dev, struct ethosu_hw_info *hwinfo)
{
    struct config_r cfg;
    struct id_r id;

    cfg.word = dev->reg->CONFIG.word;
    id.word  = dev->reg->ID.word;

    hwinfo->cfg.cmd_stream_version = cfg.cmd_stream_version;
    hwinfo->cfg.custom_dma         = cfg.custom_dma;
    hwinfo->cfg.macs_per_cc        = cfg.macs_per_cc;

    hwinfo->version.arch_major_rev = id.arch_major_rev;
    hwinfo->version.arch_minor_rev = id.arch_minor_rev;
    hwinfo->version.arch_patch_rev = id.arch_patch_rev;
    hwinfo->version.product_major  = id.product_major;
    hwinfo->version.version_major  = id.version_major;
    hwinfo->version.version_minor  = id.version_minor;
    hwinfo->version.version_status = id.version_status;
}

enum ethosu_error_codes ethosu_dev_set_clock_and_power(struct ethosu_device *dev,
                                                       enum ethosu_clock_q_request clock_q,
                                                       enum ethosu_power_q_request power_q)
{
    struct cmd_r cmd = {0};
    cmd.word         = dev->reg->CMD.word & NPU_CMD_PWR_CLK_MASK;

    if (power_q != ETHOSU_POWER_Q_UNCHANGED)
    {
        cmd.power_q_enable = power_q == ETHOSU_POWER_Q_ENABLE ? 1 : 0;
    }
    if (clock_q != ETHOSU_CLOCK_Q_UNCHANGED)
    {
        cmd.clock_q_enable = clock_q == ETHOSU_CLOCK_Q_ENABLE ? 1 : 0;
    }

    dev->reg->CMD.word = cmd.word;
    LOG_DEBUG("CMD=0x%08x", cmd.word);

    return ETHOSU_SUCCESS;
}

bool ethosu_dev_verify_optimizer_config(struct ethosu_device *dev, uint32_t cfg_in, uint32_t id_in)
{
    struct config_r *opt_cfg = (struct config_r *)&cfg_in;
    struct config_r hw_cfg;
    struct id_r *opt_id = (struct id_r *)&id_in;
    struct id_r hw_id;
    bool ret = true;

    hw_cfg.word = dev->reg->CONFIG.word;
    hw_id.word  = dev->reg->ID.word;

    LOG_INFO("Optimizer config cmd_stream_version: %" PRIu32 " macs_per_cc: %" PRIu32 " shram_size: %" PRIu32
             " custom_dma: %" PRIu32,
             opt_cfg->cmd_stream_version,
             opt_cfg->macs_per_cc,
             opt_cfg->shram_size,
             opt_cfg->custom_dma);
    LOG_INFO("Optimizer config Ethos-U version: %" PRIu32 ".%" PRIu32 ".%" PRIu32,
             opt_id->arch_major_rev,
             opt_id->arch_minor_rev,
             opt_id->arch_patch_rev);

    LOG_INFO("Ethos-U config cmd_stream_version: %" PRIu32 " macs_per_cc: %" PRIu32 " shram_size: %" PRIu32
             " custom_dma: %" PRIu32,
             hw_cfg.cmd_stream_version,
             hw_cfg.macs_per_cc,
             hw_cfg.shram_size,
             hw_cfg.custom_dma);
    LOG_INFO("Ethos-U version: %" PRIu32 ".%" PRIu32 ".%" PRIu32,
             hw_id.arch_major_rev,
             hw_id.arch_minor_rev,
             hw_id.arch_patch_rev);

    if (opt_cfg->word != hw_cfg.word)
    {
        if (hw_cfg.macs_per_cc != opt_cfg->macs_per_cc)
        {
            LOG_ERR("NPU config mismatch: npu.macs_per_cc=%" PRIu32 " optimizer.macs_per_cc=%" PRIu32,
                    hw_cfg.macs_per_cc,
                    opt_cfg->macs_per_cc);
            ret = false;
        }

        if (hw_cfg.shram_size != opt_cfg->shram_size)
        {
            LOG_ERR("NPU config mismatch: npu.shram_size=%" PRIu32 " optimizer.shram_size=%" PRIu32,
                    hw_cfg.shram_size,
                    opt_cfg->shram_size);
            ret = false;
        }

        if (hw_cfg.cmd_stream_version != opt_cfg->cmd_stream_version)
        {
            LOG_ERR("NPU config mismatch: npu.cmd_stream_version=%" PRIu32 " optimizer.cmd_stream_version=%" PRIu32,
                    hw_cfg.cmd_stream_version,
                    opt_cfg->cmd_stream_version);
            ret = false;
        }

        if (!hw_cfg.custom_dma && opt_cfg->custom_dma)
        {
            LOG_ERR("NPU config mismatch: npu.custom_dma=%" PRIu32 " optimizer.custom_dma=%" PRIu32,
                    hw_cfg.custom_dma,
                    opt_cfg->custom_dma);
            ret = false;
        }
    }

    if ((hw_id.arch_major_rev != opt_id->arch_major_rev) || (hw_id.arch_minor_rev < opt_id->arch_minor_rev))
    {
        LOG_ERR("NPU arch mismatch: npu.arch=%" PRIu32 ".%" PRIu32 ".%" PRIu32 " optimizer.arch=%" PRIu32 ".%" PRIu32
                ".%" PRIu32,
                hw_id.arch_major_rev,
                hw_id.arch_minor_rev,
                hw_id.arch_patch_rev,
                opt_id->arch_major_rev,
                opt_id->arch_minor_rev,
                opt_id->arch_patch_rev);
        ret = false;
    }

    return ret;
}
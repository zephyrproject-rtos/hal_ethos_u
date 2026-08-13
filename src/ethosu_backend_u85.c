/*
 * SPDX-FileCopyrightText: Copyright 2019-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 * SPDX-FileCopyrightText: Copyright 2025 Alif Semiconductor
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

#include "ethosu_config_u85.h"
#include "ethosu_device.h"
#include "ethosu_interface_u85.h"
#include "ethosu_log.h"
#include "ethosu_types.h"

#include <assert.h>
#ifndef __ARMCC_VERSION
#include <sys/types.h>
#endif
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/******************************************************************************
 * Defines
 ******************************************************************************/

#define ADDRESS_BITS 40
#define ADDRESS_MASK ((1ull << ADDRESS_BITS) - 1)
#define NPU_CMD_PWR_CLK_MASK (0xC)
#define NPU_MAC_PWR_RAMP_CYCLES_MASK (0x3F)

/******************************************************************************
 * Functions
 ******************************************************************************/

static bool u85_dev_init(struct ethosu_device *dev,
                         const struct ethosu_device_desc *desc,
                         struct ethosu_device_config *cfg,
                         struct ethosu_device_user_ops *user_ops,
                         void *base_address,
                         uint32_t secure_enable,
                         uint32_t privilege_enable)
{
    if (!cfg->config)
    {
        LOG_ERR("config is NULL!");
        return false;
    }

    dev->reg = (volatile struct NPU_REG *)base_address;

    if (dev->reg->CONFIG.product != ETHOSU_PRODUCT_U85)
    {
        LOG_ERR("Failed to initialize Ethos-U85 device. Wrong device type (got %d, expected %d",
                dev->reg->CONFIG.product,
                ETHOSU_PRODUCT_U85);
        return false;
    }

    dev->secure         = secure_enable;
    dev->privileged     = privilege_enable;
    dev->desc           = desc;
    dev->config         = cfg;
    dev->user_ops       = user_ops;
    dev->caps.product   = ETHOSU_PRODUCT_U85;
    dev->caps.log2_macs = dev->reg->CONFIG.macs_per_cc;

    // Make sure the NPU is in a known state
    return dev->desc->ops->soft_reset(dev);
}

static void u85_dev_axi_init(struct ethosu_device *dev)
{
    struct u85_config_t *cfg = (struct u85_config_t *)dev->config->config;

    dev->reg->MEM_ATTR[0].word = cfg->mem_attr[0].word;
    dev->reg->MEM_ATTR[1].word = cfg->mem_attr[1].word;
    dev->reg->MEM_ATTR[2].word = cfg->mem_attr[2].word;
    dev->reg->MEM_ATTR[3].word = cfg->mem_attr[3].word;

    dev->reg->AXI_SRAM.word = cfg->axi_sram.word;
    dev->reg->AXI_EXT.word  = cfg->axi_ext.word;
}

static void u85_dev_run_command_stream(struct ethosu_device *dev,
                                       const uint8_t *cmd_stream_ptr,
                                       uint32_t cms_length,
                                       const uint64_t *base_addr,
                                       int num_base_addr)
{
    assert(num_base_addr <= NPU_REG_BASEP_ARRLEN);

    struct cmd_r cmd         = {0};
    struct regioncfg_r rcfg  = {0};
    struct u85_config_t *cfg = (struct u85_config_t *)dev->config->config;
    uint64_t qbase           = (uintptr_t)cmd_stream_ptr;
    bool address_remap       = (dev->user_ops && dev->user_ops->address_remap);
    bool config_select       = (dev->user_ops && dev->user_ops->config_select);

    if (address_remap)
    {
        qbase = dev->user_ops->address_remap((uintptr_t)cmd_stream_ptr, -1);
    }

    assert(qbase <= ADDRESS_MASK);
    LOG_DEBUG("QBASE=0x%016llx, QSIZE=%" PRIu32 ", cmd_stream_ptr=%p", qbase, cms_length, cmd_stream_ptr);

    dev->reg->QBASE.word[0] = qbase & 0xffffffff;
    dev->reg->QBASE.word[1] = qbase >> 32;
    dev->reg->QSIZE.word    = 0;
    dev->reg->QSIZE.word    = cms_length;

    if (config_select)
    {
        dev->reg->QCONFIG.word = dev->user_ops->config_select(qbase, -1);
    }
    else
    {
        dev->reg->QCONFIG.word = cfg->qconfig.word;
    }

    for (int i = 0; i < num_base_addr; i++)
    {
        uint64_t addr = base_addr[i];
        if (address_remap)
        {
            addr = dev->user_ops->address_remap(base_addr[i], i);
        }
        assert(addr <= ADDRESS_MASK);
        LOG_DEBUG("BASEP%d=0x%016" PRIx64, i, addr);
        dev->reg->BASEP[i].word[0] = addr & 0xffffffff;
        dev->reg->BASEP[i].word[1] = addr >> 32;

        if (config_select)
        {
            rcfg.word |= (dev->user_ops->config_select(addr, i) << (i * 2));
        }
        else
        {
            rcfg.word |= (cfg->regioncfg.word & (0x3 << (i * 2)));
        }
    }

    dev->reg->REGIONCFG.word = rcfg.word;

    cmd.word                        = dev->reg->CMD.word & NPU_CMD_PWR_CLK_MASK;
    cmd.transition_to_running_state = 1;
    dev->reg->CMD.word              = cmd.word;

    LOG_DEBUG("CMD=0x%08" PRIx32, cmd.word);
}

static void u85_dev_print_err_status(struct ethosu_device *dev)
{
    LOG_ERR("NPU status=0x%08" PRIx32 ", qread=%" PRIu32 ", cmd_end_reached=%u",
            dev->reg->STATUS.word,
            dev->reg->QREAD.word,
            dev->reg->STATUS.cmd_end_reached);
}

static bool u85_dev_handle_interrupt(struct ethosu_device *dev)
{
    struct cmd_r cmd;

    // Clear interrupt
    cmd.word           = dev->reg->CMD.word & NPU_CMD_PWR_CLK_MASK;
    cmd.clear_irq      = 1;
    dev->reg->CMD.word = cmd.word;

    // If a fault has occured, the NPU needs to be reset
    if (dev->reg->STATUS.bus_status || dev->reg->STATUS.cmd_parse_error || dev->reg->STATUS.branch_fault ||
        dev->reg->STATUS.ecc_fault || !dev->reg->STATUS.cmd_end_reached)
    {
        return false;
    }

    return true;
}

static bool u85_dev_verify_access_state(struct ethosu_device *dev)
{
    if (dev->reg->PROT.active_CSL != (dev->secure ? SECURITY_LEVEL_SECURE : SECURITY_LEVEL_NON_SECURE) ||
        dev->reg->PROT.active_CPL != (dev->privileged ? PRIVILEGE_LEVEL_PRIVILEGED : PRIVILEGE_LEVEL_USER))
    {
        return false;
    }
    return true;
}

static bool u85_dev_soft_reset(struct ethosu_device *dev)
{
    struct reset_r reset;

    reset.word        = 0;
    reset.pending_CPL = dev->privileged ? PRIVILEGE_LEVEL_PRIVILEGED : PRIVILEGE_LEVEL_USER;
    reset.pending_CSL = dev->secure ? SECURITY_LEVEL_SECURE : SECURITY_LEVEL_NON_SECURE;

    // Reset and set security level
    LOG_INFO("Soft reset %s", dev->desc->name);
    dev->reg->RESET.word = reset.word;

    // Wait until reset status indicates that reset has been completed
    for (int i = 0; i < 100000 && dev->reg->STATUS.reset_status != 0; i++)
    {
    }

    if (dev->reg->STATUS.reset_status != 0)
    {
        LOG_ERR("Soft reset timed out");
        return false;
    }

    // Verify that NPU has switched security state and privilege level
    if (dev->desc->ops->verify_access_state(dev) != true)
    {
        LOG_ERR("Failed to switch security state and privilege level");
        return false;
    }

    // Reinitialize AXI settings
    dev->desc->ops->axi_init(dev);

    // MAC power ramping up/down control
    dev->reg->POWER_CTRL.word =
        ((struct u85_config_t *)dev->config->config)->power_ctrl.mac_step_cycles & NPU_MAC_PWR_RAMP_CYCLES_MASK;

    return true;
}

static void u85_dev_get_hw_info(struct ethosu_device *dev, struct ethosu_hw_info *hwinfo)
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

static void u85_dev_set_clock_and_power(struct ethosu_device *dev,
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
    LOG_DEBUG("CMD=0x%08" PRIx32, cmd.word);
}

static bool u85_dev_verify_optimizer_config(struct ethosu_device *dev, uint32_t cfg_in, uint32_t id_in)
{
    struct config_r opt_cfg = {0};
    struct config_r hw_cfg;
    struct id_r opt_id = {0};
    struct id_r hw_id;
    bool ret = true;

    opt_cfg.word = cfg_in;
    opt_id.word  = id_in;
    hw_cfg.word  = dev->reg->CONFIG.word;
    hw_id.word   = dev->reg->ID.word;

    LOG_INFO("Optimizer config. product=%u, cmd_stream_version=%u, macs_per_cc=%u, num_axi_ext=%u, num_axi_sram=%u, "
             "custom_dma=%u",
             opt_cfg.product,
             opt_cfg.cmd_stream_version,
             opt_cfg.macs_per_cc,
             1U << opt_cfg.num_axi_ext,
             1U << opt_cfg.num_axi_sram,
             opt_cfg.custom_dma);

    LOG_INFO(
        "Optimizer config. arch version=%u.%u.%u", opt_id.arch_major_rev, opt_id.arch_minor_rev, opt_id.arch_patch_rev);

    LOG_INFO("Ethos-U config. product=%u, cmd_stream_version=%u, macs_per_cc=%u, num_axi_ext=%u, num_axi_sram=%u, "
             "custom_dma=%u",
             hw_cfg.product,
             hw_cfg.cmd_stream_version,
             hw_cfg.macs_per_cc,
             1U << hw_cfg.num_axi_ext,
             1U << hw_cfg.num_axi_sram,
             hw_cfg.custom_dma);

    LOG_INFO("Ethos-U. arch version=%u.%u.%u", hw_id.arch_major_rev, hw_id.arch_minor_rev, hw_id.arch_patch_rev);

    if (opt_cfg.word != hw_cfg.word)
    {
        if (hw_cfg.product != opt_cfg.product)
        {
            LOG_ERR("NPU config mismatch. npu.product=%u, optimizer.product=%u", hw_cfg.product, opt_cfg.product);
            ret = false;
        }

        if (hw_cfg.macs_per_cc != opt_cfg.macs_per_cc)
        {
            LOG_ERR("NPU config mismatch. npu.macs_per_cc=%u, optimizer.macs_per_cc=%u",
                    hw_cfg.macs_per_cc,
                    opt_cfg.macs_per_cc);
            ret = false;
        }

        if (hw_cfg.num_axi_ext != opt_cfg.num_axi_ext)
        {
            LOG_ERR("NPU config mismatch. npu.num_axi_ext=%u, optimizer.num_axi_ext=%u",
                    1U << hw_cfg.num_axi_ext,
                    1U << opt_cfg.num_axi_ext);
            ret = false;
        }

        if (hw_cfg.num_axi_sram != opt_cfg.num_axi_sram)
        {
            LOG_ERR("NPU config mismatch. npu.num_axi_sram=%u, optimizer.num_axi_sram=%u",
                    1U << hw_cfg.num_axi_sram,
                    1U << opt_cfg.num_axi_sram);
            ret = false;
        }

        if (hw_cfg.cmd_stream_version != opt_cfg.cmd_stream_version)
        {
            LOG_ERR("NPU config mismatch. npu.cmd_stream_version=%u, optimizer.cmd_stream_version=%u",
                    hw_cfg.cmd_stream_version,
                    opt_cfg.cmd_stream_version);
            ret = false;
        }

        if (!hw_cfg.custom_dma && opt_cfg.custom_dma)
        {
            LOG_ERR("NPU config mismatch. npu.custom_dma=%u, optimizer.custom_dma=%u",
                    hw_cfg.custom_dma,
                    opt_cfg.custom_dma);
            ret = false;
        }
    }

    if ((hw_id.arch_major_rev != opt_id.arch_major_rev) || (hw_id.arch_minor_rev < opt_id.arch_minor_rev))
    {
        LOG_ERR("NPU arch mismatch. npu.arch=%u.%u.%u, optimizer.arch=%u.%u.%u",
                hw_id.arch_major_rev,
                hw_id.arch_minor_rev,
                hw_id.arch_patch_rev,
                opt_id.arch_major_rev,
                opt_id.arch_minor_rev,
                opt_id.arch_patch_rev);
        ret = false;
    }

    return ret;
}

/******************************************************************************
 * Backend descriptor
 ******************************************************************************/

static const struct ethosu_device_ops u85_ops = {
    .init                    = u85_dev_init,
    .axi_init                = u85_dev_axi_init,
    .run_command_stream      = u85_dev_run_command_stream,
    .print_err_status        = u85_dev_print_err_status,
    .handle_interrupt        = u85_dev_handle_interrupt,
    .get_hw_info             = u85_dev_get_hw_info,
    .verify_access_state     = u85_dev_verify_access_state,
    .soft_reset              = u85_dev_soft_reset,
    .set_clock_and_power     = u85_dev_set_clock_and_power,
    .verify_optimizer_config = u85_dev_verify_optimizer_config,
};

const struct ethosu_device_desc ethosu_device_desc_u85 = {
    .name = "ethos-u85",
    .ops  = &u85_ops,
};

/******************************************************************************
 * Default config
 ******************************************************************************
 * NOTE: This is a global instance meant as default! It's intentionally not
 * const so that it's possible to override in runtime if desired.
 ******************************************************************************/

struct u85_config_t u85_config_default = {
    .power_ctrl.mac_step_cycles        = NPU_MAC_PWR_RAMP_CYCLES,
    .mem_attr[0].word                  = NPU_MEM_ATTR_0,
    .mem_attr[1].word                  = NPU_MEM_ATTR_1,
    .mem_attr[2].word                  = NPU_MEM_ATTR_2,
    .mem_attr[3].word                  = NPU_MEM_ATTR_3,
    .qconfig.cmd_region0               = NPU_QCONFIG,
    .regioncfg.region0                 = NPU_REGIONCFG_0,
    .regioncfg.region1                 = NPU_REGIONCFG_1,
    .regioncfg.region2                 = NPU_REGIONCFG_2,
    .regioncfg.region3                 = NPU_REGIONCFG_3,
    .regioncfg.region4                 = NPU_REGIONCFG_4,
    .regioncfg.region5                 = NPU_REGIONCFG_5,
    .regioncfg.region6                 = NPU_REGIONCFG_6,
    .regioncfg.region7                 = NPU_REGIONCFG_7,
    .axi_sram.max_outstanding_read_m1  = AXI_LIMIT_SRAM_MAX_OUTSTANDING_READ - 1,
    .axi_sram.max_outstanding_write_m1 = AXI_LIMIT_SRAM_MAX_OUTSTANDING_WRITE - 1,
    .axi_sram.max_beats                = AXI_LIMIT_SRAM_MAX_BEATS,
    .axi_ext.max_outstanding_read_m1   = AXI_LIMIT_EXT_MAX_OUTSTANDING_READ - 1,
    .axi_ext.max_outstanding_write_m1  = AXI_LIMIT_EXT_MAX_OUTSTANDING_WRITE - 1,
    .axi_ext.max_beats                 = AXI_LIMIT_EXT_MAX_BEATS,
};

struct ethosu_device_config ethosu_device_config_u85 = {
    .config = &u85_config_default,
};

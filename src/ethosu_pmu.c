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

/*****************************************************************************
 * Includes
 *****************************************************************************/

#include "ethosu55_interface.h"
#include "ethosu_common.h"
#include "ethosu_driver.h"
#include "pmu_ethosu.h"

#include <assert.h>
#include <inttypes.h>
#include <stddef.h>

/*****************************************************************************
 * Defines
 *****************************************************************************/

#define COMMA ,
#define SEMICOLON ;

#define EVTYPE(A, name)                                                                                                \
    case PMU_EVENT_TYPE_##name:                                                                                        \
        return ETHOSU_PMU_##name

#define EVID(A, name) (PMU_EVENT_TYPE_##name)

#define NPU_REG_PMEVCNTR(x) (NPU_REG_PMEVCNTR0 + ((x) * sizeof(uint32_t)))
#define NPU_REG_PMEVTYPER(x) (NPU_REG_PMEVTYPER0 + ((x) * sizeof(uint32_t)))

/*****************************************************************************
 * Variables
 *****************************************************************************/

static const enum pmu_event_type eventbyid[] = {EXPAND_PMU_EVENT_TYPE(EVID, COMMA)};

/*****************************************************************************
 * Static functions
 *****************************************************************************/

static enum ethosu_pmu_event_type pmu_event_type(uint32_t id)
{
    switch (id)
    {
        EXPAND_PMU_EVENT_TYPE(EVTYPE, SEMICOLON);
    default:
        LOG_ERR("Unknown PMU event id: 0x%" PRIx32 "\n", id);
    }

    return ETHOSU_PMU_SENTINEL;
}

static uint32_t pmu_event_value(enum ethosu_pmu_event_type event)
{
    int a = event;
    if ((a < ETHOSU_PMU_SENTINEL) && (a >= ETHOSU_PMU_NO_EVENT))
    {
        return eventbyid[event];
    }
    else
    {
        return (uint32_t)(-1);
    }
}

/*****************************************************************************
 * Functions
 *****************************************************************************/

void ETHOSU_PMU_Enable(void)
{
    LOG_DEBUG("%s:\n", __FUNCTION__);
    struct pmcr_r pmcr;
    pmcr.word   = ethosu_drv.dev.pmcr;
    pmcr.cnt_en = 1;
    ethosu_write_reg_shadow(&ethosu_drv.dev, NPU_REG_PMCR, pmcr.word, &ethosu_drv.dev.pmcr);
}

void ETHOSU_PMU_Disable(void)
{
    LOG_DEBUG("%s:\n", __FUNCTION__);
    struct pmcr_r pmcr;
    pmcr.word   = ethosu_drv.dev.pmcr;
    pmcr.cnt_en = 0;
    ethosu_write_reg_shadow(&ethosu_drv.dev, NPU_REG_PMCR, pmcr.word, &ethosu_drv.dev.pmcr);
}

void ETHOSU_PMU_Set_EVTYPER(uint32_t num, enum ethosu_pmu_event_type type)
{
    ASSERT(num < ETHOSU_PMU_NCOUNTERS);
    uint32_t val = pmu_event_value(type);
    LOG_DEBUG("%s: num=%u, type=%d, val=%u\n", __FUNCTION__, num, type, val);
    ethosu_write_reg_shadow(&ethosu_drv.dev, NPU_REG_PMEVTYPER(num), val, &ethosu_drv.dev.pmu_evtypr[num]);
}

enum ethosu_pmu_event_type ETHOSU_PMU_Get_EVTYPER(uint32_t num)
{
    ASSERT(num < ETHOSU_PMU_NCOUNTERS);
    uint32_t val                    = ethosu_drv.dev.pmu_evtypr[num];
    enum ethosu_pmu_event_type type = pmu_event_type(val);
    LOG_DEBUG("%s: num=%u, type=%d, val=%u\n", __FUNCTION__, num, type, val);
    return type;
}

void ETHOSU_PMU_CYCCNT_Reset(void)
{
    LOG_DEBUG("%s:\n", __FUNCTION__);
    struct pmcr_r pmcr;
    pmcr.word          = ethosu_drv.dev.pmcr;
    pmcr.cycle_cnt_rst = 1;
    ethosu_write_reg_shadow(&ethosu_drv.dev, NPU_REG_PMCR, pmcr.word, &ethosu_drv.dev.pmcr);
    ethosu_drv.dev.pmccntr[0] = 0;
    ethosu_drv.dev.pmccntr[1] = 0;
}

void ETHOSU_PMU_EVCNTR_ALL_Reset(void)
{
    LOG_DEBUG("%s:\n", __FUNCTION__);
    struct pmcr_r pmcr;
    pmcr.word          = ethosu_drv.dev.pmcr;
    pmcr.event_cnt_rst = 1;
    ethosu_write_reg_shadow(&ethosu_drv.dev, NPU_REG_PMCR, pmcr.word, &ethosu_drv.dev.pmcr);

    for (uint32_t i = 0; i < ETHOSU_PMU_NCOUNTERS; i++)
    {
        ethosu_drv.dev.pmu_evcntr[i] = 0;
    }
}

void ETHOSU_PMU_CNTR_Enable(uint32_t mask)
{
    LOG_DEBUG("%s: mask=0x%08x\n", __FUNCTION__, mask);
    ethosu_write_reg_shadow(&ethosu_drv.dev, NPU_REG_PMCNTENSET, mask, &ethosu_drv.dev.pmcnten);
}

void ETHOSU_PMU_CNTR_Disable(uint32_t mask)
{
    LOG_DEBUG("%s: mask=0x%08x\n", __FUNCTION__, mask);
    ethosu_write_reg_shadow(&ethosu_drv.dev, NPU_REG_PMCNTENCLR, mask, &ethosu_drv.dev.pmcnten);
}

uint32_t ETHOSU_PMU_CNTR_Status(void)
{
    LOG_DEBUG("%s: mask=0x%08x\n", __FUNCTION__, ethosu_drv.dev.pmcnten);
    return ethosu_drv.dev.pmcnten;
}

uint64_t ETHOSU_PMU_Get_CCNTR(void)
{
    uint32_t val_lo = ethosu_read_reg(&ethosu_drv.dev, NPU_REG_PMCCNTR_LO);
    uint32_t val_hi = ethosu_read_reg(&ethosu_drv.dev, NPU_REG_PMCCNTR_HI);
    uint64_t val    = ((uint64_t)val_hi << 32) | val_lo;
    uint64_t shadow = ((uint64_t)ethosu_drv.dev.pmccntr[1] << 32) | ethosu_drv.dev.pmccntr[0];

    LOG_DEBUG("%s: val=%" PRIu64 ", shadow=%" PRIu64 "\n", __FUNCTION__, val, shadow);

    // Return the shadow variable in case the NPU was powered off and lost the cycle count
    if (shadow > val)
    {
        return shadow;
    }

    // Update the shadow variable
    ethosu_drv.dev.pmccntr[0] = val_lo;
    ethosu_drv.dev.pmccntr[1] = val_hi;

    return val;
}

void ETHOSU_PMU_Set_CCNTR(uint64_t val)
{
    uint32_t active = ETHOSU_PMU_CNTR_Status() & ETHOSU_PMU_CCNT_Msk;

    LOG_DEBUG("%s: val=%llu\n", __FUNCTION__, val);

    if (active)
    {
        ETHOSU_PMU_CNTR_Disable(ETHOSU_PMU_CCNT_Msk);
    }

    ethosu_write_reg_shadow(&ethosu_drv.dev, NPU_REG_PMCCNTR_LO, val & MASK_0_31_BITS, &ethosu_drv.dev.pmccntr[0]);
    ethosu_write_reg_shadow(
        &ethosu_drv.dev, NPU_REG_PMCCNTR_HI, (val & MASK_32_47_BITS) >> 32, &ethosu_drv.dev.pmccntr[1]);

    if (active)
    {
        ETHOSU_PMU_CNTR_Enable(ETHOSU_PMU_CCNT_Msk);
    }
}

uint32_t ETHOSU_PMU_Get_EVCNTR(uint32_t num)
{
    ASSERT(num < ETHOSU_PMU_NCOUNTERS);
    uint32_t val = ethosu_read_reg(&ethosu_drv.dev, NPU_REG_PMEVCNTR(num));
    LOG_DEBUG("%s: num=%u, val=%u, shadow=%u\n", __FUNCTION__, num, val, ethosu_drv.dev.pmu_evcntr[num]);

    // Return the shadow variable in case the NPU was powered off and lost the event count
    if (ethosu_drv.dev.pmu_evcntr[num] > val)
    {
        return ethosu_drv.dev.pmu_evcntr[num];
    }

    // Update the shadow variable
    ethosu_drv.dev.pmu_evcntr[num] = val;

    return val;
}

void ETHOSU_PMU_Set_EVCNTR(uint32_t num, uint32_t val)
{
    ASSERT(num < ETHOSU_PMU_NCOUNTERS);
    LOG_DEBUG("%s: num=%u, val=%u\n", __FUNCTION__, num, val);
    ethosu_write_reg(&ethosu_drv.dev, NPU_REG_PMEVCNTR(num), val);
}

uint32_t ETHOSU_PMU_Get_CNTR_OVS(void)
{
    LOG_DEBUG("%s:\n", __FUNCTION__);
    return ethosu_read_reg(&ethosu_drv.dev, NPU_REG_PMOVSSET);
}

void ETHOSU_PMU_Set_CNTR_OVS(uint32_t mask)
{
    LOG_DEBUG("%s:\n", __FUNCTION__);
    ethosu_write_reg(&ethosu_drv.dev, NPU_REG_PMOVSCLR, mask);
}

void ETHOSU_PMU_Set_CNTR_IRQ_Enable(uint32_t mask)
{
    LOG_DEBUG("%s: mask=0x%08x\n", __FUNCTION__, mask);
    ethosu_write_reg_shadow(&ethosu_drv.dev, NPU_REG_PMINTSET, mask, &ethosu_drv.dev.pmint);
}

void ETHOSU_PMU_Set_CNTR_IRQ_Disable(uint32_t mask)
{
    LOG_DEBUG("%s: mask=0x%08x\n", __FUNCTION__, mask);
    ethosu_write_reg_shadow(&ethosu_drv.dev, NPU_REG_PMINTCLR, mask, &ethosu_drv.dev.pmint);
}

uint32_t ETHOSU_PMU_Get_IRQ_Enable(void)
{
    LOG_DEBUG("%s: mask=0x%08x\n", __FUNCTION__, ethosu_drv.dev.pmint);
    return ethosu_drv.dev.pmint;
}

void ETHOSU_PMU_CNTR_Increment(uint32_t mask)
{
    LOG_DEBUG("%s:\n", __FUNCTION__);
    uint32_t cntrs_active = ETHOSU_PMU_CNTR_Status();

    // Disable counters
    ETHOSU_PMU_CNTR_Disable(mask);

    // Increment cycle counter
    if (mask & ETHOSU_PMU_CCNT_Msk)
    {
        uint64_t val = ETHOSU_PMU_Get_CCNTR() + 1;
        ethosu_write_reg_shadow(&ethosu_drv.dev, NPU_REG_PMCCNTR_LO, val & MASK_0_31_BITS, &ethosu_drv.dev.pmccntr[0]);
        ethosu_write_reg_shadow(
            &ethosu_drv.dev, NPU_REG_PMCCNTR_HI, (val & MASK_32_47_BITS) >> 32, &ethosu_drv.dev.pmccntr[1]);
    }

    for (int i = 0; i < ETHOSU_PMU_NCOUNTERS; i++)
    {
        if (mask & (1 << i))
        {
            uint32_t val = ETHOSU_PMU_Get_EVCNTR(i);
            ethosu_write_reg_shadow(&ethosu_drv.dev, NPU_REG_PMEVCNTR(i), val + 1, &ethosu_drv.dev.pmu_evcntr[i]);
        }
    }

    // Reenable the active counters
    ETHOSU_PMU_CNTR_Enable(cntrs_active);
}

void ETHOSU_PMU_PMCCNTR_CFG_Set_Start_Event(uint32_t start_event)
{
    LOG_DEBUG("%s: start_event=%u\n", __FUNCTION__, start_event);
    struct pmccntr_cfg_r cfg;
    cfg.word                = ethosu_drv.dev.pmccntr_cfg;
    cfg.CYCLE_CNT_CFG_START = start_event;
    ethosu_write_reg_shadow(&ethosu_drv.dev, NPU_REG_PMCCNTR_CFG, cfg.word, &ethosu_drv.dev.pmccntr_cfg);
}

void ETHOSU_PMU_PMCCNTR_CFG_Set_Stop_Event(uint32_t stop_event)
{
    LOG_DEBUG("%s: stop_event=%u\n", __FUNCTION__, stop_event);
    struct pmccntr_cfg_r cfg;
    cfg.word               = ethosu_drv.dev.pmccntr_cfg;
    cfg.CYCLE_CNT_CFG_STOP = stop_event;
    ethosu_write_reg_shadow(&ethosu_drv.dev, NPU_REG_PMCCNTR_CFG, cfg.word, &ethosu_drv.dev.pmccntr_cfg);
}

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
#include <stddef.h>
#include <stdint.h>

/*****************************************************************************
 * Defines
 *****************************************************************************/

#define COMMA ,
#define SEMICOLON ;

#define EVTYPE(A, name)                                                                                                \
    case PMU_EVENT_TYPE_##name:                                                                                        \
        return ETHOSU_PMU_##name

#define EVID(A, name) (PMU_EVENT_TYPE_##name)

#define ETHOSU_PMCCNTR_CFG_START_STOP_EVENT_MASK (0x3FF)

#define NPU_REG_PMEVCNTR(x) (NPU_REG_PMEVCNTR0 + ((x) * sizeof(uint32_t)))
#define NPU_REG_PMEVTYPER(x) (NPU_REG_PMEVTYPER0 + ((x) * sizeof(uint32_t)))

/*****************************************************************************
 * Variables
 *****************************************************************************/

/**
 *  NOTE: A pointer to ethosu_driver will be added to the PMU functions
 * when multi-NPU functionality is implemented later. We shall use a
 * shared ethosu_driver instance till then.
 * */
extern struct ethosu_driver ethosu_drv;

static const enum pmu_event_type eventbyid[] = {EXPAND_PMU_EVENT_TYPE(EVID, COMMA)};

/*****************************************************************************
 * Functions
 *****************************************************************************/

enum ethosu_pmu_event_type pmu_event_type(uint32_t id)
{
    switch (id)
    {
        EXPAND_PMU_EVENT_TYPE(EVTYPE, SEMICOLON);
    }

    return ETHOSU_PMU_SENTINEL;
}

uint32_t pmu_event_value(enum ethosu_pmu_event_type event)
{
    if (!(event < ETHOSU_PMU_SENTINEL) || (event < 0))
    {
        return (uint32_t)(-1);
    }

    return eventbyid[event];
}

void ethosu_pmu_driver_init(void)
{
#ifdef PMU_AUTOINIT
    ethosu_write_reg(&ethosu_drv.dev, NPU_REG_PMCR, INIT_PMCR);
    ethosu_write_reg(&ethosu_drv.dev, NPU_REG_PMCNTENSET, INIT_PMCNTENSET);
    ethosu_write_reg(&ethosu_drv.dev, NPU_REG_PMCNTENCLR, INIT_PMCNTENCLR);
    ethosu_write_reg(&ethosu_drv.dev, NPU_REG_PMOVSSET, INIT_PMOVSSET);
    ethosu_write_reg(&ethosu_drv.dev, NPU_REG_PMOVSCLR, INIT_PMOVSCLR);
    ethosu_write_reg(&ethosu_drv.dev, NPU_REG_PMINTSET, INIT_PMINTSET);
    ethosu_write_reg(&ethosu_drv.dev, NPU_REG_PMINTCLR, INIT_PMINTCLR);
    ethosu_write_reg(&ethosu_drv.dev, NPU_REG_PMCCNTR_LO, INIT_PMCCNTR);
    ethosu_write_reg(&ethosu_drv.dev, NPU_REG_PMCCNTR_HI, INIT_PMCCNTR);
    ethosu_write_reg(&ethosu_drv.dev, NPU_REG_PMCCNTR_CFG, INIT_PMCCNTR_CFG);

    for (int i = 0; i < ETHOSU_PMU_NCOUNTERS; i++)
    {
        ethosu_write_reg(&ethosu_drv.dev, NPU_REG_PMEVCNTR(i), 0);
        ethosu_write_reg(&ethosu_drv.dev, NPU_REG_PMEVTYPER(i), 0);
    }
#endif
}

void ethosu_pmu_driver_exit(void) {}

void ETHOSU_PMU_Enable(void)
{
    struct pmcr_r pmcr;
    pmcr.word   = ethosu_read_reg(&ethosu_drv.dev, NPU_REG_PMCR);
    pmcr.cnt_en = 1;
    ethosu_write_reg(&ethosu_drv.dev, NPU_REG_PMCR, pmcr.word);
}

void ETHOSU_PMU_Disable(void)
{
    struct pmcr_r pmcr;
    pmcr.word   = ethosu_read_reg(&ethosu_drv.dev, NPU_REG_PMCR);
    pmcr.cnt_en = 0;
    ethosu_write_reg(&ethosu_drv.dev, NPU_REG_PMCR, pmcr.word);
}

void ETHOSU_PMU_Set_EVTYPER(uint32_t num, enum ethosu_pmu_event_type type)
{
    ethosu_write_reg(&ethosu_drv.dev, NPU_REG_PMEVTYPER(num), pmu_event_value(type));
}

enum ethosu_pmu_event_type ETHOSU_PMU_Get_EVTYPER(uint32_t num)
{
    return pmu_event_type(ethosu_read_reg(&ethosu_drv.dev, NPU_REG_PMEVTYPER(num)));
}

void ETHOSU_PMU_CYCCNT_Reset(void)
{
    struct pmcr_r pmcr;
    pmcr.word          = ethosu_read_reg(&ethosu_drv.dev, NPU_REG_PMCR);
    pmcr.cycle_cnt_rst = 1;
    ethosu_write_reg(&ethosu_drv.dev, NPU_REG_PMCR, pmcr.word);
}

void ETHOSU_PMU_EVCNTR_ALL_Reset(void)
{
    struct pmcr_r pmcr;
    pmcr.word          = ethosu_read_reg(&ethosu_drv.dev, NPU_REG_PMCR);
    pmcr.event_cnt_rst = 1;
    ethosu_write_reg(&ethosu_drv.dev, NPU_REG_PMCR, pmcr.word);
}

void ETHOSU_PMU_CNTR_Enable(uint32_t mask)
{
    ethosu_write_reg(&ethosu_drv.dev, NPU_REG_PMCNTENSET, mask);
}

void ETHOSU_PMU_CNTR_Disable(uint32_t mask)
{
    ethosu_write_reg(&ethosu_drv.dev, NPU_REG_PMCNTENCLR, mask);
}

uint32_t ETHOSU_PMU_CNTR_Status(void)
{
    return ethosu_read_reg(&ethosu_drv.dev, NPU_REG_PMCNTENSET);
}

uint64_t ETHOSU_PMU_Get_CCNTR(void)
{
    uint64_t val1 = (((uint64_t)ethosu_read_reg(&ethosu_drv.dev, NPU_REG_PMCCNTR_HI)) << 32) |
                    ethosu_read_reg(&ethosu_drv.dev, NPU_REG_PMCCNTR_LO);
    uint64_t val2 = (((uint64_t)ethosu_read_reg(&ethosu_drv.dev, NPU_REG_PMCCNTR_HI)) << 32) |
                    ethosu_read_reg(&ethosu_drv.dev, NPU_REG_PMCCNTR_LO);

    if (val2 > val1)
    {
        return val2;
    }
    return val1;
}

void ETHOSU_PMU_Set_CCNTR(uint64_t val)
{
    uint32_t mask = ETHOSU_PMU_CNTR_Status();

    if (mask & ETHOSU_PMU_CCNT_Msk)
    {
        ETHOSU_PMU_CNTR_Disable(ETHOSU_PMU_CCNT_Msk);
    }

    ethosu_write_reg(&ethosu_drv.dev, NPU_REG_PMCCNTR_LO, (val & MASK_0_31_BITS));
    ethosu_write_reg(&ethosu_drv.dev, NPU_REG_PMCCNTR_HI, (val & MASK_32_47_BITS) >> 32);

    if (mask & ETHOSU_PMU_CCNT_Msk)
    {
        ETHOSU_PMU_CNTR_Enable(ETHOSU_PMU_CCNT_Msk);
    }
}

uint32_t ETHOSU_PMU_Get_EVCNTR(uint32_t num)
{
    return ethosu_read_reg(&ethosu_drv.dev, NPU_REG_PMEVCNTR(num));
}

void ETHOSU_PMU_Set_EVCNTR(uint32_t num, uint32_t val)
{
    ethosu_write_reg(&ethosu_drv.dev, NPU_REG_PMEVCNTR(num), val);
}

uint32_t ETHOSU_PMU_Get_CNTR_OVS(void)
{
    return ethosu_read_reg(&ethosu_drv.dev, NPU_REG_PMOVSSET);
}

// TODO: check if this function name match with the description &
// implementation.
void ETHOSU_PMU_Set_CNTR_OVS(uint32_t mask)
{
    ethosu_write_reg(&ethosu_drv.dev, NPU_REG_PMOVSCLR, mask);
}

void ETHOSU_PMU_Set_CNTR_IRQ_Enable(uint32_t mask)
{
    ethosu_write_reg(&ethosu_drv.dev, NPU_REG_PMINTSET, mask);
}

void ETHOSU_PMU_Set_CNTR_IRQ_Disable(uint32_t mask)
{
    ethosu_write_reg(&ethosu_drv.dev, NPU_REG_PMINTCLR, mask);
}

uint32_t ETHOSU_PMU_Get_IRQ_Enable(void)
{
    return ethosu_read_reg(&ethosu_drv.dev, NPU_REG_PMINTSET);
}

void ETHOSU_PMU_CNTR_Increment(uint32_t mask)
{
    uint32_t cntrs_active = ETHOSU_PMU_CNTR_Status();

    if (mask & ETHOSU_PMU_CCNT_Msk)
    {
        if (mask & ETHOSU_PMU_CCNT_Msk)
        {
            ETHOSU_PMU_CNTR_Disable(ETHOSU_PMU_CCNT_Msk);
            uint64_t val = ETHOSU_PMU_Get_CCNTR() + 1;
            ethosu_write_reg(&ethosu_drv.dev, NPU_REG_PMCCNTR_LO, (val & MASK_0_31_BITS));
            ethosu_write_reg(&ethosu_drv.dev, NPU_REG_PMCCNTR_HI, (val & MASK_32_47_BITS) >> 32);
            if (cntrs_active & ETHOSU_PMU_CCNT_Msk)
            {
                ETHOSU_PMU_CNTR_Enable(ETHOSU_PMU_CCNT_Msk);
            }
        }
    }
    for (int i = 0; i < ETHOSU_PMU_NCOUNTERS; i++)
    {
        uint32_t cntr = (0x0001 << i);

        if (mask & cntr)
        {
            ETHOSU_PMU_CNTR_Disable(cntr);
            uint32_t val = ethosu_read_reg(&ethosu_drv.dev, NPU_REG_PMEVCNTR(i));
            ethosu_write_reg(&ethosu_drv.dev, NPU_REG_PMEVCNTR(i), val + 1);
            if (cntrs_active & cntr)
            {
                ETHOSU_PMU_CNTR_Enable(cntr);
            }
        }
    }
}

void ETHOSU_PMU_PMCCNTR_CFG_Set_Start_Event(uint32_t start_event)
{
    struct pmccntr_cfg_r cfg;
    cfg.word                = ethosu_read_reg(&ethosu_drv.dev, NPU_REG_PMCCNTR_CFG);
    cfg.CYCLE_CNT_CFG_START = start_event & ETHOSU_PMCCNTR_CFG_START_STOP_EVENT_MASK;
    ethosu_write_reg(&ethosu_drv.dev, NPU_REG_PMCCNTR_CFG, cfg.word);
}

void ETHOSU_PMU_PMCCNTR_CFG_Set_Stop_Event(uint32_t stop_event)
{
    struct pmccntr_cfg_r cfg;
    cfg.word               = ethosu_read_reg(&ethosu_drv.dev, NPU_REG_PMCCNTR_CFG);
    cfg.CYCLE_CNT_CFG_STOP = stop_event & ETHOSU_PMCCNTR_CFG_START_STOP_EVENT_MASK;
    ethosu_write_reg(&ethosu_drv.dev, NPU_REG_PMCCNTR_CFG, cfg.word);
}

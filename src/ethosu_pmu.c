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
#include <assert.h>
#include <ethosu_driver.h>
#include <pmu_ethosu.h>
#include <stdint.h>

/*****************************************************************************
 * Defines
 *****************************************************************************/

#define COMMA ,
#define SEMICOLON ;

#define ETHOSU_PMU_CTRL_BASE (NPU_BASE + ((uint32_t)0x180))
#define ETHOSU_PMU_CNTR_BASE (NPU_BASE + ((uint32_t)0x300))
#define ETHOSU_PMU_EVNT_BASE (NPU_BASE + ((uint32_t)0x380))

#define EVTYPE(A, name)                                                                                                \
    case PMU_EVENT_TYPE_##name:                                                                                        \
        return ETHOSU_PMU_##name

#define EVID(A, name) (PMU_EVENT_TYPE_##name)

/*****************************************************************************
 * Variables
 *****************************************************************************/

PMU_Ethosu_ctrl_Type *ethosu_pmu_ctrl = (PMU_Ethosu_ctrl_Type *)ETHOSU_PMU_CTRL_BASE;
PMU_Ethosu_cntr_Type *ethosu_pmu_cntr = (PMU_Ethosu_cntr_Type *)ETHOSU_PMU_CNTR_BASE;
PMU_Ethosu_evnt_Type *ethosu_pmu_evnt = (PMU_Ethosu_evnt_Type *)ETHOSU_PMU_EVNT_BASE;

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
    *ethosu_pmu_ctrl = (PMU_Ethosu_ctrl_Type){.PMCR        = INIT_PMCR,
                                              .PMCNTENSET  = INIT_PMCNTENSET,
                                              .PMCNTENCLR  = INIT_PMCNTENCLR,
                                              .PMOVSSET    = INIT_PMOVSSET,
                                              .PMOVSCLR    = INIT_PMOVSCLR,
                                              .PMINTSET    = INIT_PMINTSET,
                                              .PMINTCLR    = INIT_PMINTCLR,
                                              .PMCCNTR     = INIT_PMCCNTR,
                                              .PMCCNTR_CFG = INIT_PMCCNTR_CFG};

    for (int i = 0; i < ETHOSU_PMU_NCOUNTERS; i++)
    {
        *ethosu_pmu_cntr[i] = 0;
        *ethosu_pmu_evnt[i] = 0;
    }
#endif
}

void ethosu_pmu_driver_exit(void) {}

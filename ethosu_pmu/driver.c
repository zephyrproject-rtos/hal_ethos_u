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
#include "common.h"
#include "ethosu55_interface.h"
#include "ethosu_pmu_config.h"
#include <assert.h>
#include <ethosu_driver.h>
#include <pmu_ethosu.h>
#include <stdint.h>

PMU_Ethosu_ctrl_Type *ethosu_pmu_ctrl = (PMU_Ethosu_ctrl_Type *)ETHOSU_PMU_CTRL_BASE;
PMU_Ethosu_cntr_Type *ethosu_pmu_cntr = (PMU_Ethosu_cntr_Type *)ETHOSU_PMU_CNTR_BASE;
PMU_Ethosu_evnt_Type *ethosu_pmu_evnt = (PMU_Ethosu_evnt_Type *)ETHOSU_PMU_EVNT_BASE;

// Local functions & macros
#define PMUREG0(NAME) INIT_##NAME

#define ETHOSU_PMU_INIT(N)                                                                                             \
    *ethosu_pmu_ctrl = (PMU_Ethosu_ctrl_Type){.PMCR        = PMUREG##N(PMCR),                                          \
                                              .PMCNTENSET  = PMUREG##N(PMCNTENSET),                                    \
                                              .PMCNTENCLR  = PMUREG##N(PMCNTENCLR),                                    \
                                              .PMOVSSET    = PMUREG##N(PMOVSSET),                                      \
                                              .PMOVSCLR    = PMUREG##N(PMOVSCLR),                                      \
                                              .PMINTSET    = PMUREG##N(PMINTSET),                                      \
                                              .PMINTCLR    = PMUREG##N(PMINTCLR),                                      \
                                              .PMCCNTR     = PMUREG##N(PMCCNTR),                                       \
                                              .PMCCNTR_CFG = PMUREG##N(PMCCNTR_CFG)};

// Public interfaces

#define EVTYPE(A, name)                                                                                                \
    case PMU_EVENT_TYPE_##name:                                                                                        \
        return ETHOSU_PMU_##name
enum ethosu_pmu_event_type pmu_event_type(uint32_t id)
{
    switch (id)
    {
        EXPAND_PMU_EVENT_TYPE(EVTYPE, SEMICOLON);
    }
    return ETHOSU_PMU_SENTINEL;
}

#define EVID(A, name) (PMU_EVENT_TYPE_##name)
static const enum pmu_event_type eventbyid[] = {EXPAND_PMU_EVENT_TYPE(EVID, COMMA)};
uint32_t pmu_event_value(enum ethosu_pmu_event_type event)
{
    if (!(event < ETHOSU_PMU_SENTINEL) || (event < 0))
    {
        return (uint32_t)(-1);
    }

    return eventbyid[event];
}

// Driver Init/exit functions
INIT ethosu_pmu_driver_init(void)
{
    ETHOSU_PMU_INIT(0)

    for (int i = 0; i < ETHOSU_PMU_NCOUNTERS; i++)
    {
        *ethosu_pmu_cntr[i] = 0;
        *ethosu_pmu_evnt[i] = 0;
    }
}

EXIT ethosu_pmu_driver_exit(void) {}

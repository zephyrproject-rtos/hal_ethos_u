/*
 * SPDX-FileCopyrightText: Copyright 2019-2024, 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
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

#include "ethosu_driver.h"
#include "pmu_ethosu.h"

#include <stdint.h>

/*****************************************************************************
 * Functions
 *****************************************************************************/

void ETHOSU_PMU_Enable(struct ethosu_driver *drv)
{
    drv->pmu->ops->ETHOSU_PMU_Enable(drv);
}
void ETHOSU_PMU_Disable(struct ethosu_driver *drv)
{
    drv->pmu->ops->ETHOSU_PMU_Disable(drv);
}

#ifndef ETHOSU_MULTI_VARIANT
uint32_t ETHOSU_PMU_Get_NumEventCounters(void)
{
    return ETHOSU_PMU_NCOUNTERS;
}
#endif

uint32_t ETHOSU_PMU_Get_NumEventCountersForDrv(struct ethosu_driver *drv)
{
    return drv->pmu->ops->ETHOSU_PMU_Get_NumEventCounters();
}

void ETHOSU_PMU_Set_EVTYPER(struct ethosu_driver *drv, uint32_t num, enum ethosu_pmu_event_type type)
{
    drv->pmu->ops->ETHOSU_PMU_Set_EVTYPER(drv, num, type);
}

enum ethosu_pmu_event_type ETHOSU_PMU_Get_EVTYPER(struct ethosu_driver *drv, uint32_t num)
{
    return drv->pmu->ops->ETHOSU_PMU_Get_EVTYPER(drv, num);
}

void ETHOSU_PMU_CYCCNT_Reset(struct ethosu_driver *drv)
{
    drv->pmu->ops->ETHOSU_PMU_CYCCNT_Reset(drv);
}

void ETHOSU_PMU_EVCNTR_ALL_Reset(struct ethosu_driver *drv)
{
    drv->pmu->ops->ETHOSU_PMU_EVCNTR_ALL_Reset(drv);
}

void ETHOSU_PMU_CNTR_Enable(struct ethosu_driver *drv, uint32_t mask)
{
    drv->pmu->ops->ETHOSU_PMU_CNTR_Enable(drv, mask);
}

void ETHOSU_PMU_CNTR_Disable(struct ethosu_driver *drv, uint32_t mask)
{
    drv->pmu->ops->ETHOSU_PMU_CNTR_Disable(drv, mask);
}

uint32_t ETHOSU_PMU_CNTR_Status(struct ethosu_driver *drv)
{
    return drv->pmu->ops->ETHOSU_PMU_CNTR_Status(drv);
}

uint64_t ETHOSU_PMU_Get_CCNTR(struct ethosu_driver *drv)
{
    return drv->pmu->ops->ETHOSU_PMU_Get_CCNTR(drv);
}

void ETHOSU_PMU_Set_CCNTR(struct ethosu_driver *drv, uint64_t val)
{
    drv->pmu->ops->ETHOSU_PMU_Set_CCNTR(drv, val);
}

uint32_t ETHOSU_PMU_Get_EVCNTR(struct ethosu_driver *drv, uint32_t num)
{
    return drv->pmu->ops->ETHOSU_PMU_Get_EVCNTR(drv, num);
}

void ETHOSU_PMU_Set_EVCNTR(struct ethosu_driver *drv, uint32_t num, uint32_t val)
{
    drv->pmu->ops->ETHOSU_PMU_Set_EVCNTR(drv, num, val);
}

uint32_t ETHOSU_PMU_Get_CNTR_OVS(struct ethosu_driver *drv)
{
    return drv->pmu->ops->ETHOSU_PMU_Get_CNTR_OVS(drv);
}

void ETHOSU_PMU_Set_CNTR_OVS(struct ethosu_driver *drv, uint32_t mask)
{
    drv->pmu->ops->ETHOSU_PMU_Set_CNTR_OVS(drv, mask);
}

void ETHOSU_PMU_Set_CNTR_IRQ_Enable(struct ethosu_driver *drv, uint32_t mask)
{
    drv->pmu->ops->ETHOSU_PMU_Set_CNTR_IRQ_Enable(drv, mask);
}

void ETHOSU_PMU_Set_CNTR_IRQ_Disable(struct ethosu_driver *drv, uint32_t mask)
{
    drv->pmu->ops->ETHOSU_PMU_Set_CNTR_IRQ_Disable(drv, mask);
}

uint32_t ETHOSU_PMU_Get_IRQ_Enable(struct ethosu_driver *drv)
{
    return drv->pmu->ops->ETHOSU_PMU_Get_IRQ_Enable(drv);
}

void ETHOSU_PMU_CNTR_Increment(struct ethosu_driver *drv, uint32_t mask)
{
    drv->pmu->ops->ETHOSU_PMU_CNTR_Increment(drv, mask);
}

void ETHOSU_PMU_PMCCNTR_CFG_Set_Start_Event(struct ethosu_driver *drv, enum ethosu_pmu_event_type start_event)
{
    drv->pmu->ops->ETHOSU_PMU_PMCCNTR_CFG_Set_Start_Event(drv, start_event);
}

void ETHOSU_PMU_PMCCNTR_CFG_Set_Stop_Event(struct ethosu_driver *drv, enum ethosu_pmu_event_type stop_event)
{
    drv->pmu->ops->ETHOSU_PMU_PMCCNTR_CFG_Set_Stop_Event(drv, stop_event);
}

uint32_t ETHOSU_PMU_Get_QREAD(struct ethosu_driver *drv)
{
    return drv->pmu->ops->ETHOSU_PMU_Get_QREAD(drv);
}

uint32_t ETHOSU_PMU_Get_STATUS(struct ethosu_driver *drv)
{
    return drv->pmu->ops->ETHOSU_PMU_Get_STATUS(drv);
}

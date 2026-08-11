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

#ifndef ETHOSU_PMU_FUNCS_H
#define ETHOSU_PMU_FUNCS_H

/*****************************************************************************
 * Includes
 *****************************************************************************/
#include <stdint.h>

#include "ethosu_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************
 * Functions
 *****************************************************************************/

/**
 * \brief   Enable the PMU
 * \param [in]    drv     Driver handle
 */
void ETHOSU_PMU_Enable(struct ethosu_driver *drv);

/**
 * \brief   Disable the PMU
 * \param [in]    drv     Driver handle
 */
void ETHOSU_PMU_Disable(struct ethosu_driver *drv);

/**
 * \brief   Set event to count for PMU eventer counter
 * \param [in]    drv     Driver handle
 * \param [in]    num     Event counter (0-ETHOSU_PMU_NCOUNTERS) to configure
 * \param [in]    type    Event to count
 */
void ETHOSU_PMU_Set_EVTYPER(struct ethosu_driver *drv, uint32_t num, enum ethosu_pmu_event_type type);

#ifndef ETHOSU_MULTI_VARIANT
/**
 * \brief   Get number of PMU event counters
 * \return                Number of event counters
 * \note    Not available when multi variant support is enabled,
 *          use the ETHOSU_PMU_Get_NumEventCountersForDrv function.
 */
uint32_t ETHOSU_PMU_Get_NumEventCounters(void);
#endif

/**
 * \brief   Get number of PMU event counters for driver
 * \return                Number of event counters
 */
uint32_t ETHOSU_PMU_Get_NumEventCountersForDrv(struct ethosu_driver *drv);

/**
 * \brief   Get event to count for PMU eventer counter
 * \param [in]    drv     Driver handle
 * \param [in]    num     Event counter (0-ETHOSU_PMU_NCOUNTERS) to configure
 * \return        type    Event to count
 */
enum ethosu_pmu_event_type ETHOSU_PMU_Get_EVTYPER(struct ethosu_driver *drv, uint32_t num);

/**
 * \brief  Reset cycle counter
 * \param [in]    drv     Driver handle
 */
void ETHOSU_PMU_CYCCNT_Reset(struct ethosu_driver *drv);

/**
 * \brief  Reset all event counters
 */
void ETHOSU_PMU_EVCNTR_ALL_Reset(struct ethosu_driver *drv);

/**
 * \brief  Enable counters
 * \param [in]     drv     Driver handle
 * \param [in]     mask    Counters to enable
 * \note   Enables one or more of the following:
 *         - event counters (bit 0-ETHOSU_PMU_NCOUNTERS)
 *         - cycle counter  (bit 31)
 */
void ETHOSU_PMU_CNTR_Enable(struct ethosu_driver *drv, uint32_t mask);

/**
 * \brief  Disable counters
 * \param [in]     drv     Driver handle
 * \param [in]     mask    Counters to disable
 * \note   Disables one or more of the following:
 *         - event counters (bit 0-ETHOSU_PMU_NCOUNTERS)
 *         - cycle counter  (bit 31)
 */
void ETHOSU_PMU_CNTR_Disable(struct ethosu_driver *drv, uint32_t mask);

/**
 * \brief  Determine counters activation
 * \param  [in]     drv     Driver handle
 * \return a bitmask where bit-set means:
 *         - event counters activated (bit 0-ETHOSU_PMU_NCOUNTERS)
 *         - cycle counter  activate  (bit 31)
 * \note   ETHOSU specific. Usage breaks CMSIS complience
 */
uint32_t ETHOSU_PMU_CNTR_Status(struct ethosu_driver *drv);

/**
 * \brief  Read cycle counter (64 bit)
 * \param  [in]     drv     Driver handle
 * \return                 Cycle count
 * \note   Two HW 32-bit registers that can increment independently in-between reads.
 *         To work-around raciness yet still avoid turning
 *         off the event both are read as one value twice. If the latter read
 *         is not greater than the former, it means overflow of LSW without
 *         incrementing MSW has occurred, in which case the former value is used.
 */
uint64_t ETHOSU_PMU_Get_CCNTR(struct ethosu_driver *drv);

/**
 * \brief  Set cycle counter (64 bit)
 * \param [in]    drv     Driver handle
 * \param [in]    val     Conter value
 * \note   Two HW 32-bit registers that can increment independently in-between reads.
 *         To work-around raciness, counter is temporary disabled if enabled.
 * \note   ETHOSU specific. Usage breaks CMSIS complience
 */
void ETHOSU_PMU_Set_CCNTR(struct ethosu_driver *drv, uint64_t val);

/**
 * \brief   Read event counter
 * \param [in]    drv     Driver handle
 * \param [in]    num     Event counter (0-ETHOSU_PMU_NCOUNTERS)
 * \return                Event count
 */
uint32_t ETHOSU_PMU_Get_EVCNTR(struct ethosu_driver *drv, uint32_t num);

/**
 * \brief   Set event counter value
 * \param [in]    drv     Driver handle
 * \param [in]    num     Event counter (0-ETHOSU_PMU_NCOUNTERS)
 * \param [in]    val     Conter value
 * \note   ETHOSU specific. Usage breaks CMSIS complience
 */
void ETHOSU_PMU_Set_EVCNTR(struct ethosu_driver *drv, uint32_t num, uint32_t val);

/**
 * \brief   Read counter overflow status
 * \param [in]     drv     Driver handle
 * \return  Counter overflow status bits for the following:
 *          - event counters (bit 0-ETHOSU_PMU_NCOUNTERS))
 *          - cycle counter  (bit 31)
 */
uint32_t ETHOSU_PMU_Get_CNTR_OVS(struct ethosu_driver *drv);

/**
 * \brief   Clear counter overflow status
 * \param [in]     drv     Driver handle
 * \param [in]     mask    Counter overflow status bits to clear
 * \note    Clears overflow status bits for one or more of the following:
 *          - event counters (bit 0-ETHOSU_PMU_NCOUNTERS)
 *          - cycle counter  (bit 31)
 */
void ETHOSU_PMU_Set_CNTR_OVS(struct ethosu_driver *drv, uint32_t mask);

/**
 * \brief   Enable counter overflow interrupt request
 * \param [in]     drv     Driver handle
 * \param [in]     mask    Counter overflow interrupt request bits to set
 * \note    Sets overflow interrupt request bits for one or more of the following:
 *          - event counters (bit 0-ETHOSU_PMU_NCOUNTERS)
 *          - cycle counter  (bit 31)
 */
void ETHOSU_PMU_Set_CNTR_IRQ_Enable(struct ethosu_driver *drv, uint32_t mask);

/**
 * \brief   Disable counter overflow interrupt request
 * \param [in]     drv     Driver handle
 * \param [in]     mask    Counter overflow interrupt request bits to clear
 * \note    Clears overflow interrupt request bits for one or more of the following:
 *          - event counters (bit 0-ETHOSU_PMU_NCOUNTERS)
 *          - cycle counter  (bit 31)
 */
void ETHOSU_PMU_Set_CNTR_IRQ_Disable(struct ethosu_driver *drv, uint32_t mask);

/**
 * \brief   Get counters overflow interrupt request stiinings
 * \param  [in]     drv     Driver handle
 * \return  mask    Counter overflow interrupt request bits
 * \note    Sets overflow interrupt request bits for one or more of the following:
 *          - event counters (bit 0-ETHOSU_PMU_NCOUNTERS)
 *          - cycle counter  (bit 31)
 * \note   ETHOSU specific. Usage breaks CMSIS compliance
 */
uint32_t ETHOSU_PMU_Get_IRQ_Enable(struct ethosu_driver *drv);

/**
 * \brief   Software increment event counter
 * \param [in]     drv     Driver handle
 * \param [in]     mask    Counters to increment
 *          - event counters (bit 0-ETHOSU_PMU_NCOUNTERS)
 *          - cycle counter  (bit 31)
 * \note    Software increment bits for one or more event counters.
 */
void ETHOSU_PMU_CNTR_Increment(struct ethosu_driver *drv, uint32_t mask);

/**
 * \brief   Set start event number for the cycle counter
 * \param [in]   drv           Driver handle
 * \param [in]   start_event   Event to trigger start of the cycle counter
 * \note   Sets the event number that starts the cycle counter.
 */
void ETHOSU_PMU_PMCCNTR_CFG_Set_Start_Event(struct ethosu_driver *drv, enum ethosu_pmu_event_type start_event);

/**
 * \brief   Set stop event number for the cycle counter
 * \param [in]   drv          Driver handle
 * \param [in]   stop_event   Event number
 * \note   Sets the event number that stops the cycle counter.
 */
void ETHOSU_PMU_PMCCNTR_CFG_Set_Stop_Event(struct ethosu_driver *drv, enum ethosu_pmu_event_type stop_event);

/**
 * \brief   Read qread register
 * \param [in]   drv     Driver handle
 * \return               QREAD register value
 */
uint32_t ETHOSU_PMU_Get_QREAD(struct ethosu_driver *drv);

/**
 * \brief   Read status register
 * \param [in]   drv     Driver handle
 * \return               STATUS register value
 */
uint32_t ETHOSU_PMU_Get_STATUS(struct ethosu_driver *drv);

#ifdef __cplusplus
}
#endif

#endif /* ETHOSU_PMU_FUNCS_H */

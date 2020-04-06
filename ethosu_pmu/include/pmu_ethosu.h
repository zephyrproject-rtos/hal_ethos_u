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

#if defined(__ICCARM__)
#pragma system_include /* treat file as system include file for MISRA check */
#elif defined(__clang__)
#pragma clang system_header /* treat file as system include file */
#endif

#ifndef PMU_ETHOSU_H
#define PMU_ETHOSU_H

#define ETHOSU_PMU_NCOUNTERS 4

#include <cmsis_compiler.h>

#ifdef NOTNOW
#if defined(CPU_CORTEX_M55)
#include <core_cm55.h>
#elif defined(CPU_CORTEX_M33)
#include <core_cm33.h>
#else
#error npu-pmu not supported for CPU
#endif
#else
/* IO definitions (access restrictions to peripheral registers) */
/**
    \defgroup CMSIS_glob_defs CMSIS Global Defines

    <strong>IO Type Qualifiers</strong> are used
    \li to specify the access to peripheral variables.
    \li for automatic generation of peripheral register debug information.
*/
#ifdef __cplusplus
#define __I volatile /*!< Defines 'read only' permissions */
#else
#define __I volatile const /*!< Defines 'read only' permissions */
#endif
#define __O volatile        /*!< Defines 'write only' permissions */
#define __IO volatile       /*!< Defines 'read / write' permissions */

/* following defines should be used for structure members */
#define __IM volatile const /*! Defines 'read only' structure member permissions */
#define __OM volatile       /*! Defines 'write only' structure member permissions */
#define __IOM volatile      /*! Defines 'read / write' structure member permissions */
#endif

typedef struct
{
    __IOM uint32_t PMCR;
    __IOM uint32_t PMCNTENSET;
    __IOM uint32_t PMCNTENCLR;
    __IOM uint32_t PMOVSSET;
    __IOM uint32_t PMOVSCLR;
    __IOM uint32_t PMINTSET;
    __IOM uint32_t PMINTCLR;
    __IOM uint64_t PMCCNTR;
    __IOM uint32_t PMCCNTR_CFG;
} PMU_Ethosu_ctrl_Type;

typedef uint32_t PMU_Ethosu_cntr_Type[ETHOSU_PMU_NCOUNTERS];
typedef uint32_t PMU_Ethosu_evnt_Type[ETHOSU_PMU_NCOUNTERS];

/** \brief HW Supported ETHOSU PMU Events
 *
 * Note: These values are symbolic. Actual HW-values may change. I.e. always use API
 *       to set/get actual event-type value.
 * */
enum ethosu_pmu_event_type
{
    ETHOSU_PMU_CYCLE = 0,
    ETHOSU_PMU_NPU_IDLE,
    ETHOSU_PMU_MAC_ACTIVE,
    ETHOSU_PMU_MAC_ACTIVE_8BIT,
    ETHOSU_PMU_MAC_ACTIVE_16BIT,
    ETHOSU_PMU_MAC_DPU_ACTIVE,
    ETHOSU_PMU_MAC_STALLED_BY_WD_ACC,
    ETHOSU_PMU_MAC_STALLED_BY_WD,
    ETHOSU_PMU_MAC_STALLED_BY_ACC,
    ETHOSU_PMU_MAC_STALLED_BY_IB,
    ETHOSU_PMU_AO_ACTIVE,
    ETHOSU_PMU_AO_ACTIVE_8BIT,
    ETHOSU_PMU_AO_ACTIVE_16BIT,
    ETHOSU_PMU_AO_STALLED_BY_OFMP_OB,
    ETHOSU_PMU_AO_STALLED_BY_OFMP,
    ETHOSU_PMU_AO_STALLED_BY_OB,
    ETHOSU_PMU_AO_STALLED_BY_ACC_IB,
    ETHOSU_PMU_AO_STALLED_BY_ACC,
    ETHOSU_PMU_AO_STALLED_BY_IB,
    ETHOSU_PMU_WD_ACTIVE,
    ETHOSU_PMU_WD_STALLED,
    ETHOSU_PMU_WD_STALLED_BY_WS,
    ETHOSU_PMU_WD_STALLED_BY_WD_BUF,
    ETHOSU_PMU_WD_PARSE_ACTIVE,
    ETHOSU_PMU_WD_PARSE_STALLED,
    ETHOSU_PMU_WD_PARSE_STALLED_IN,
    ETHOSU_PMU_WD_PARSE_STALLED_OUT,
    ETHOSU_PMU_AXI0_RD_TRANS_ACCEPTED,
    ETHOSU_PMU_AXI0_RD_TRANS_COMPLETED,
    ETHOSU_PMU_AXI0_RD_DATA_BEAT_RECEIVED,
    ETHOSU_PMU_AXI0_RD_TRAN_REQ_STALLED,
    ETHOSU_PMU_AXI0_WR_TRANS_ACCEPTED,
    ETHOSU_PMU_AXI0_WR_TRANS_COMPLETED_M,
    ETHOSU_PMU_AXI0_WR_TRANS_COMPLETED_S,
    ETHOSU_PMU_AXI0_WR_DATA_BEAT_WRITTEN,
    ETHOSU_PMU_AXI0_WR_TRAN_REQ_STALLED,
    ETHOSU_PMU_AXI0_WR_DATA_BEAT_STALLED,
    ETHOSU_PMU_AXI0_ENABLED_CYCLES,
    ETHOSU_PMU_AXI0_RD_STALL_LIMIT,
    ETHOSU_PMU_AXI0_WR_STALL_LIMIT,
    ETHOSU_PMU_AXI1_RD_TRANS_ACCEPTED,
    ETHOSU_PMU_AXI1_RD_TRANS_COMPLETED,
    ETHOSU_PMU_AXI1_RD_DATA_BEAT_RECEIVED,
    ETHOSU_PMU_AXI1_RD_TRAN_REQ_STALLED,
    ETHOSU_PMU_AXI1_WR_TRANS_ACCEPTED,
    ETHOSU_PMU_AXI1_WR_TRANS_COMPLETED_M,
    ETHOSU_PMU_AXI1_WR_TRANS_COMPLETED_S,
    ETHOSU_PMU_AXI1_WR_DATA_BEAT_WRITTEN,
    ETHOSU_PMU_AXI1_WR_TRAN_REQ_STALLED,
    ETHOSU_PMU_AXI1_WR_DATA_BEAT_STALLED,
    ETHOSU_PMU_AXI1_ENABLED_CYCLES,
    ETHOSU_PMU_AXI1_RD_STALL_LIMIT,
    ETHOSU_PMU_AXI1_WR_STALL_LIMIT,
    ETHOSU_PMU_AXI_LATENCY_ANY,
    ETHOSU_PMU_AXI_LATENCY_32,
    ETHOSU_PMU_AXI_LATENCY_64,
    ETHOSU_PMU_AXI_LATENCY_128,
    ETHOSU_PMU_AXI_LATENCY_256,
    ETHOSU_PMU_AXI_LATENCY_512,
    ETHOSU_PMU_AXI_LATENCY_1024,

    ETHOSU_PMU_SENTINEL // End-marker (not event)
};

extern PMU_Ethosu_ctrl_Type *ethosu_pmu_ctrl;
extern PMU_Ethosu_cntr_Type *ethosu_pmu_cntr;
extern PMU_Ethosu_evnt_Type *ethosu_pmu_evnt;

#define ETHOSU_PMU_CTRL_ENABLE_Msk (0x0001)
#define ETHOSU_PMU_CTRL_EVENTCNT_RESET_Msk (0x0002)
#define ETHOSU_PMU_CTRL_CYCCNT_RESET_Msk (0x0004)
#define ETHOSU_PMU_CNT1_Msk (1UL << 0)
#define ETHOSU_PMU_CNT2_Msk (1UL << 1)
#define ETHOSU_PMU_CNT3_Msk (1UL << 2)
#define ETHOSU_PMU_CNT4_Msk (1UL << 3)
#define ETHOSU_PMU_CCNT_Msk (1UL << 31)

/* Transpose functions between HW-event-type and event-id*/
enum ethosu_pmu_event_type pmu_event_type(uint32_t);
uint32_t pmu_event_value(enum ethosu_pmu_event_type);

// CMSIS ref API
/** \brief PMU Functions */

__STATIC_INLINE void ETHOSU_PMU_Enable(void);
__STATIC_INLINE void ETHOSU_PMU_Disable(void);

__STATIC_INLINE void ETHOSU_PMU_Set_EVTYPER(uint32_t num, enum ethosu_pmu_event_type type);
__STATIC_INLINE enum ethosu_pmu_event_type ETHOSU_PMU_Get_EVTYPER(uint32_t num);

__STATIC_INLINE void ETHOSU_PMU_CYCCNT_Reset(void);
__STATIC_INLINE void ETHOSU_PMU_EVCNTR_ALL_Reset(void);

__STATIC_INLINE void ETHOSU_PMU_CNTR_Enable(uint32_t mask);
__STATIC_INLINE void ETHOSU_PMU_CNTR_Disable(uint32_t mask);
__STATIC_INLINE uint32_t ETHOSU_PMU_CNTR_Status();

__STATIC_INLINE uint64_t ETHOSU_PMU_Get_CCNTR(void);
__STATIC_INLINE void ETHOSU_PMU_Set_CCNTR(uint64_t val);
__STATIC_INLINE uint32_t ETHOSU_PMU_Get_EVCNTR(uint32_t num);
__STATIC_INLINE void ETHOSU_PMU_Set_EVCNTR(uint32_t num, uint32_t val);

__STATIC_INLINE uint32_t ETHOSU_PMU_Get_CNTR_OVS(void);
__STATIC_INLINE void ETHOSU_PMU_Set_CNTR_OVS(uint32_t mask);

__STATIC_INLINE void ETHOSU_PMU_Set_CNTR_IRQ_Enable(uint32_t mask);
__STATIC_INLINE void ETHOSU_PMU_Set_CNTR_IRQ_Disable(uint32_t mask);
__STATIC_INLINE uint32_t ETHOSU_PMU_Get_IRQ_Enable();

__STATIC_INLINE void ETHOSU_PMU_CNTR_Increment(uint32_t mask);

/**
  \brief   Enable the PMU
*/
__STATIC_INLINE void ETHOSU_PMU_Enable(void)
{
    ethosu_pmu_ctrl->PMCR |= ETHOSU_PMU_CTRL_ENABLE_Msk;
}

/**
  \brief   Disable the PMU
*/
__STATIC_INLINE void ETHOSU_PMU_Disable(void)
{
    ethosu_pmu_ctrl->PMCR &= ~ETHOSU_PMU_CTRL_ENABLE_Msk;
}

/**
  \brief   Set event to count for PMU eventer counter
  \param [in]    num     Event counter (0-ETHOSU_PMU_NCOUNTERS) to configure
  \param [in]    type    Event to count
*/
__STATIC_INLINE void ETHOSU_PMU_Set_EVTYPER(uint32_t num, enum ethosu_pmu_event_type type)
{
    (*ethosu_pmu_evnt)[num] = pmu_event_value(type);
}

/**
  \brief   Get event to count for PMU eventer counter
  \param [in]    num     Event counter (0-ETHOSU_PMU_NCOUNTERS) to configure
  \return        type    Event to count
*/
__STATIC_INLINE enum ethosu_pmu_event_type ETHOSU_PMU_Get_EVTYPER(uint32_t num)
{
    return pmu_event_type((*ethosu_pmu_evnt)[num]);
}

/**
  \brief  Reset cycle counter
*/
__STATIC_INLINE void ETHOSU_PMU_CYCCNT_Reset(void)
{
    ethosu_pmu_ctrl->PMCR |= ETHOSU_PMU_CTRL_CYCCNT_RESET_Msk;
}

/**
  \brief  Reset all event counters
*/
__STATIC_INLINE void ETHOSU_PMU_EVCNTR_ALL_Reset(void)
{
    ethosu_pmu_ctrl->PMCR |= ETHOSU_PMU_CTRL_EVENTCNT_RESET_Msk;
}

/**
  \brief  Enable counters
  \param [in]     mask    Counters to enable
  \note   Enables one or more of the following:
          - event counters (bit 0-ETHOSU_PMU_NCOUNTERS)
          - cycle counter  (bit 31)
*/
__STATIC_INLINE void ETHOSU_PMU_CNTR_Enable(uint32_t mask)
{
    ethosu_pmu_ctrl->PMCNTENSET = mask;
}

/**
  \brief  Disable counters
  \param [in]     mask    Counters to disable
  \note   Disables one or more of the following:
          - event counters (bit 0-ETHOSU_PMU_NCOUNTERS)
          - cycle counter  (bit 31)
*/
__STATIC_INLINE void ETHOSU_PMU_CNTR_Disable(uint32_t mask)
{
    ethosu_pmu_ctrl->PMCNTENCLR = mask;
}

/**
  \brief  Determine counters activation

  \return                Event count
  \param [in]     mask    Counters to enable
  \return  a bitmask where bit-set means:
          - event counters activated (bit 0-ETHOSU_PMU_NCOUNTERS)
          - cycle counter  activate  (bit 31)
  \note   ETHOSU specific. Usage breaks CMSIS complience
*/
__STATIC_INLINE uint32_t ETHOSU_PMU_CNTR_Status()
{
    return ethosu_pmu_ctrl->PMCNTENSET;
}

/**
  \brief  Read cycle counter (64 bit)
  \return                 Cycle count
  \note   Two HW 32-bit registers that can increment independently in-between reads.
          To work-around raciness yet still avoid turning
          off the event both are read as one value twice. If the latter read
          is not greater than the former, it means overflow of LSW without
          incrementing MSW has occurred, in which case the former value is used.
*/
__STATIC_INLINE uint64_t ETHOSU_PMU_Get_CCNTR(void)
{
    uint64_t val1 = ethosu_pmu_ctrl->PMCCNTR;
    uint64_t val2 = ethosu_pmu_ctrl->PMCCNTR;

    if (val2 > val1)
    {
        return val2;
    }
    return val1;
}

/**
  \brief  Set cycle counter (64 bit)
  \param [in]    val     Conter value
  \note   Two HW 32-bit registers that can increment independently in-between reads.
          To work-around raciness, counter is temporary disabled if enabled.
  \note   ETHOSU specific. Usage breaks CMSIS complience
*/
__STATIC_INLINE void ETHOSU_PMU_Set_CCNTR(uint64_t val)
{
    uint32_t mask = ETHOSU_PMU_CNTR_Status();

    if (mask & ETHOSU_PMU_CCNT_Msk)
    {
        ETHOSU_PMU_CNTR_Disable(ETHOSU_PMU_CCNT_Msk);
    }

    ethosu_pmu_ctrl->PMCCNTR = val;

    if (mask & ETHOSU_PMU_CCNT_Msk)
    {
        ETHOSU_PMU_CNTR_Enable(ETHOSU_PMU_CCNT_Msk);
    }
}

/**
  \brief   Read event counter
  \param [in]    num     Event counter (0-ETHOSU_PMU_NCOUNTERS)
  \return                Event count
*/
__STATIC_INLINE uint32_t ETHOSU_PMU_Get_EVCNTR(uint32_t num)
{
    return (*ethosu_pmu_cntr)[num];
}

/**
  \brief   Set event counter value
  \param [in]    num     Event counter (0-ETHOSU_PMU_NCOUNTERS)
  \param [in]    val     Conter value
  \note   ETHOSU specific. Usage breaks CMSIS complience
*/
__STATIC_INLINE void ETHOSU_PMU_Set_EVCNTR(uint32_t num, uint32_t val)
{
    (*ethosu_pmu_cntr)[num] = val;
}
/**
  \brief   Read counter overflow status
  \return  Counter overflow status bits for the following:
           - event counters (bit 0-ETHOSU_PMU_NCOUNTERS))
           - cycle counter  (bit 31)
*/
__STATIC_INLINE uint32_t ETHOSU_PMU_Get_CNTR_OVS(void)
{
    return ethosu_pmu_ctrl->PMOVSSET;
}

/**
  \brief   Clear counter overflow status
  \param [in]     mask    Counter overflow status bits to clear
  \note    Clears overflow status bits for one or more of the following:
           - event counters (bit 0-ETHOSU_PMU_NCOUNTERS)
           - cycle counter  (bit 31)
*/
__STATIC_INLINE void ETHOSU_PMU_Set_CNTR_OVS(uint32_t mask)
{
    ethosu_pmu_ctrl->PMOVSCLR = mask;
}

/**
  \brief   Enable counter overflow interrupt request
  \param [in]     mask    Counter overflow interrupt request bits to set
  \note    Sets overflow interrupt request bits for one or more of the following:
           - event counters (bit 0-ETHOSU_PMU_NCOUNTERS)
           - cycle counter  (bit 31)
*/
__STATIC_INLINE void ETHOSU_PMU_Set_CNTR_IRQ_Enable(uint32_t mask)
{
    ethosu_pmu_ctrl->PMINTSET = mask;
}

/**
  \brief   Disable counter overflow interrupt request
  \param [in]     mask    Counter overflow interrupt request bits to clear
  \note    Clears overflow interrupt request bits for one or more of the following:
           - event counters (bit 0-ETHOSU_PMU_NCOUNTERS)
           - cycle counter  (bit 31)
*/
__STATIC_INLINE void ETHOSU_PMU_Set_CNTR_IRQ_Disable(uint32_t mask)
{
    ethosu_pmu_ctrl->PMINTCLR = mask;
}

/**
  \brief   Get counters overflow interrupt request stiinings
  \return   mask    Counter overflow interrupt request bits
  \note    Sets overflow interrupt request bits for one or more of the following:
           - event counters (bit 0-ETHOSU_PMU_NCOUNTERS)
           - cycle counter  (bit 31)
  \note   ETHOSU specific. Usage breaks CMSIS complience
*/
__STATIC_INLINE uint32_t ETHOSU_PMU_Get_IRQ_Enable()
{
    return ethosu_pmu_ctrl->PMINTSET;
}

/**
  \brief   Software increment event counter
  \param [in]     mask    Counters to increment
           - event counters (bit 0-ETHOSU_PMU_NCOUNTERS)
           - cycle counter  (bit 31)
  \note    Software increment bits for one or more event counters.
*/
__STATIC_INLINE void ETHOSU_PMU_CNTR_Increment(uint32_t mask)
{
    uint32_t cntrs_active = ETHOSU_PMU_CNTR_Status();

    if (mask & ETHOSU_PMU_CCNT_Msk)
    {
        if (mask & ETHOSU_PMU_CCNT_Msk)
        {
            ETHOSU_PMU_CNTR_Disable(ETHOSU_PMU_CCNT_Msk);
            ethosu_pmu_ctrl->PMCCNTR = ETHOSU_PMU_Get_CCNTR() + 1;
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
            (*ethosu_pmu_cntr)[i]++;
            if (cntrs_active & cntr)
            {
                ETHOSU_PMU_CNTR_Enable(cntr);
            }
        }
    }
}

#endif /* PMU_ETHOSU_H */

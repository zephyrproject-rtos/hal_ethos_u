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

#ifndef ETHOSU_CONFIG_H
#define ETHOSU_CONFIG_H

/* Set default values if not manually overriden */

#ifndef NPU_QCONFIG
#define NPU_QCONFIG 2
#endif

#ifndef NPU_REGIONCFG_0
#define NPU_REGIONCFG_0 3
#endif

#ifndef NPU_REGIONCFG_1
#define NPU_REGIONCFG_1 0
#endif

#ifndef NPU_REGIONCFG_2
#define NPU_REGIONCFG_2 1
#endif

#ifndef NPU_REGIONCFG_3
#define NPU_REGIONCFG_3 1
#endif

#ifndef NPU_REGIONCFG_4
#define NPU_REGIONCFG_4 1
#endif

#ifndef NPU_REGIONCFG_5
#define NPU_REGIONCFG_5 1
#endif

#ifndef NPU_REGIONCFG_6
#define NPU_REGIONCFG_6 1
#endif

#ifndef NPU_REGIONCFG_7
#define NPU_REGIONCFG_7 1
#endif

#ifndef AXI_LIMIT0_MAX_BEATS_BYTES
#define AXI_LIMIT0_MAX_BEATS_BYTES 0x0
#endif
#ifndef AXI_LIMIT0_MEM_TYPE
#define AXI_LIMIT0_MEM_TYPE 0x0
#endif
#ifndef AXI_LIMIT0_MAX_OUTSTANDING_READS
#define AXI_LIMIT0_MAX_OUTSTANDING_READS 32
#endif
#ifndef AXI_LIMIT0_MAX_OUTSTANDING_WRITES
#define AXI_LIMIT0_MAX_OUTSTANDING_WRITES 16
#endif

#ifndef AXI_LIMIT1_MAX_BEATS_BYTES
#define AXI_LIMIT1_MAX_BEATS_BYTES 0x0
#endif
#ifndef AXI_LIMIT1_MEM_TYPE
#define AXI_LIMIT1_MEM_TYPE 0x0
#endif
#ifndef AXI_LIMIT1_MAX_OUTSTANDING_READS
#define AXI_LIMIT1_MAX_OUTSTANDING_READS 32
#endif
#ifndef AXI_LIMIT1_MAX_OUTSTANDING_WRITES
#define AXI_LIMIT1_MAX_OUTSTANDING_WRITES 16
#endif

#ifndef AXI_LIMIT2_MAX_BEATS_BYTES
#define AXI_LIMIT2_MAX_BEATS_BYTES 0x0
#endif
#ifndef AXI_LIMIT2_MEM_TYPE
#define AXI_LIMIT2_MEM_TYPE 0x0
#endif
#ifndef AXI_LIMIT2_MAX_OUTSTANDING_READS
#define AXI_LIMIT2_MAX_OUTSTANDING_READS 32
#endif
#ifndef AXI_LIMIT2_MAX_OUTSTANDING_WRITES
#define AXI_LIMIT2_MAX_OUTSTANDING_WRITES 16
#endif
#ifndef AXI_LIMIT3_MAX_BEATS_BYTES
#define AXI_LIMIT3_MAX_BEATS_BYTES 0x0
#endif
#ifndef AXI_LIMIT3_MEM_TYPE
#define AXI_LIMIT3_MEM_TYPE 0x0
#endif
#ifndef AXI_LIMIT3_MAX_OUTSTANDING_READS
#define AXI_LIMIT3_MAX_OUTSTANDING_READS 32
#endif
#ifndef AXI_LIMIT3_MAX_OUTSTANDING_WRITES
#define AXI_LIMIT3_MAX_OUTSTANDING_WRITES 16
#endif

#ifdef PMU_AUTOINIT
/*
 * Register control
 *  b0 = CNT_EN = Enable counters (RW)
 *  b1 = EVENT_CNT_RST = Reset event counters (WO)
 *  b2 = CYCLE_CNT_RST = Reset cycle counter (WO)
 *  b[15:11] = Number of event counters (RO)
 */
#ifndef INIT_PMCR
#define INIT_PMCR 0x0
#endif

/*
 * Bit k enables event counter k
 *  k=31 enables the cycle counter
 *  Read value is current status
 */
#ifndef INIT_PMCNTENSET
#define INIT_PMCNTENSET 0x0
#endif

/*
 * Bit k disables event counter k
 *  k=31 disables the cycle counter
 *  Read value is current status
 */
#ifndef INIT_PMCNTENCLR
#define INIT_PMCNTENCLR 0x0
#endif

/*
 * Overflow detection set
 *  Bit k is for counter k
 *  k=31 is cycle counter
 */
#ifndef INIT_PMOVSSET
#define INIT_PMOVSSET 0x0
#endif

/*
 * Overflow detection clear
 *  Bit k is for counter k
 *  k=31 is cycle counter
 */
#ifndef INIT_PMOVSCLR
#define INIT_PMOVSCLR 0x0
#endif

/*
 * Interrupt set
 *  Bit k is for counter k
 *  k=31 is cycle counter
 */
#ifndef INIT_PMINTSET
#define INIT_PMINTSET 0x0
#endif

/*
 * Interrupt clear
 *  Bit k is for counter k
 *  k=31 is cycle counter
 */
#ifndef INIT_PMINTCLR
#define INIT_PMINTCLR 0x8003
#endif

/* Cycle counter
 *  48 bits value
 */
#ifndef INIT_PMCCNTR
#define INIT_PMCCNTR 0x0
#endif

/*
 * b[9:0] Start Event – this event number starts the cycle counter
 * b[25:16] Stop Event – this event number stops the cycle counter
 */
#ifndef INIT_PMCCNTR_CFG
#define INIT_PMCCNTR_CFG 0x0
#endif

#endif /* #ifdef PMU_AUTOINIT */

#endif /* #ifndef ETHOSU_CONFIG_H */

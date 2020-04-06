# Copyright (c) 2019-2020 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the License); you may
# not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an AS IS BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

#
# Global options
#

set(NPU_PMCR        "0x0" CACHE STRING "Register control b0 = CNT_EN = Enable counters (RW), b1 = EVENT_CNT_RST = Reset event counters (WO), b2 = CYCLE_CNT_RST = Reset cycle counter (WO), b[15:11] = Number of event counters (RO)")
set(NPU_PMCNTENSET  "0x0" CACHE STRING "Bit k enables event counter k. k=31 enables the cycle counter. Read value is current status.")
set(NPU_PMCNTENCLR  "0x0" CACHE STRING "Bit k disables event counter k. k=31 disables the cycle counter. Reda value is current status.")
set(NPU_PMOVSSET    "0x0" CACHE STRING "Overflow detection set. Bit k is for counter k. k=31 is cycle counter.")
set(NPU_PMOVSCLR    "0x0" CACHE STRING "Overflow detection clear. Bit k is for counter k. k=31 is cycle counter.")
set(NPU_PMINTSET    "0x0" CACHE STRING "Interrupt set. Bit k is for counter k. k=31 is cycle counter.")
set(NPU_PMINTCLR    "0x8003" CACHE STRING "Interrupt clear. Bit k is for counter k. k=31 is cycle counter.")
set(NPU_PMCCNTR     "0x0" CACHE STRING "Cycle counter, 48 bits value")
set(NPU_PMCCNTR_CFG "0x0" CACHE STRING "b[9:0] Start Event – this event number starts the cycle counter b[25:16] Stop Event – this event number stops the cycle counter")

#
# Build binaries
#

add_library(ethosu_core_driver_pmu STATIC driver.c)
target_include_directories(ethosu_core_driver_pmu PUBLIC include PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/ethosu_pmu_config.h.in" "${CMAKE_CURRENT_BINARY_DIR}/ethosu_pmu_config.h")

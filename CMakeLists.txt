#
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

cmake_minimum_required(VERSION 3.15.6)

project(ethosu_core_driver VERSION 0.0.1)

#
# Build options
#

option(DRIVER_PMU_AUTOINIT "Enable PMU boot auto-initialization" OFF)
option(DRIVER_LOG_SUPPORT "Enable logging." OFF)

set(CMSIS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmsis" CACHE PATH "Path to CMSIS.")

#
# Global settings
#

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "cortex-m0")
    add_compile_definitions(CPU_CORTEX_M0)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "cortex-m3")
    add_compile_definitions(CPU_CORTEX_M3)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "cortex-m33")
    add_compile_definitions(CPU_CORTEX_M33)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "cortex-m4")
    add_compile_definitions(CPU_CORTEX_M4)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "cortex-m55")
    add_compile_definitions(CPU_CORTEX_M55)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "cortex-m7")
    add_compile_definitions(CPU_CORTEX_M7)
else()
    message(FATAL_ERROR "Unsupported compiler ${CMAKE_SYSTEM_PROCESSOR}.")
endif()

# Enable logging support
if(DRIVER_LOG_SUPPORT)
    add_compile_definitions(LOG_ENABLED)
endif()

# Make include directories available for current- and sub projects
include_directories(include src)
include_directories(${CMSIS_PATH}/CMSIS/Core/Include)

#
# Build libraries
#

# Build driver library
add_library(ethosu_core_driver STATIC)
target_include_directories(ethosu_core_driver PUBLIC include)
target_sources(ethosu_core_driver PRIVATE src/ethosu_driver.c src/ethosu_device.c src/ethosu_pmu.c)

# Build PMU
if(DRIVER_PMU_AUTOINIT)
    set(NPU_PMCR        "0x0" CACHE STRING "Register control b0 = CNT_EN = Enable counters (RW), b1 = EVENT_CNT_RST = Reset event counters (WO), b2 = CYCLE_CNT_RST = Reset cycle counter (WO), b[15:11] = Number of event counters (RO)")
    set(NPU_PMCNTENSET  "0x0" CACHE STRING "Bit k enables event counter k. k=31 enables the cycle counter. Read value is current status.")
    set(NPU_PMCNTENCLR  "0x0" CACHE STRING "Bit k disables event counter k. k=31 disables the cycle counter. Reda value is current status.")
    set(NPU_PMOVSSET    "0x0" CACHE STRING "Overflow detection set. Bit k is for counter k. k=31 is cycle counter.")
    set(NPU_PMOVSCLR    "0x0" CACHE STRING "Overflow detection clear. Bit k is for counter k. k=31 is cycle counter.")
    set(NPU_PMINTSET    "0x0" CACHE STRING "Interrupt set. Bit k is for counter k. k=31 is cycle counter.")
    set(NPU_PMINTCLR    "0x8003" CACHE STRING "Interrupt clear. Bit k is for counter k. k=31 is cycle counter.")
    set(NPU_PMCCNTR     "0x0" CACHE STRING "Cycle counter, 48 bits value")
    set(NPU_PMCCNTR_CFG "0x0" CACHE STRING "b[9:0] Start Event – this event number starts the cycle counter b[25:16] Stop Event – this event number stops the cycle counter")

    target_compile_definitions(ethosu_core_driver PRIVATE
        PMU_AUTOINIT
        INIT_PMCR=${NPU_PMCR}
        INIT_PMCNTENSET=${NPU_PMCNTENSET}
        INIT_PMCNTENCLR=${NPU_PMCNTENCLR}
        INIT_PMOVSSET=${NPU_PMOVSSET}
        INIT_PMOVSCLR=${NPU_PMOVSCLR}
        INIT_PMINTSET=${NPU_PMINTSET}
        INIT_PMINTCLR=${NPU_PMINTCLR}
        INIT_PMCCNTR=${NPU_PMCCNTR}
        INIT_PMCCNTR_CFG=${NPU_PMCCNTR_CFG})
endif()

#
# Print build status
#

message(STATUS "*******************************************************")
message(STATUS "PROJECT_NAME                           : ${PROJECT_NAME}")
message(STATUS "CMAKE_SYSTEM_PROCESSOR                 : ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "CMSIS_PATH                             : ${CMSIS_PATH}")
message(STATUS "DRIVER_LOG_SUPPORT                     : ${DRIVER_LOG_SUPPORT}")
message(STATUS "*******************************************************")

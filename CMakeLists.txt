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

option(DRIVER_BUILD_PMU "Driver supports NPU-PMU" ON)
option(LOG_SUPPORT "Enable logging." ON)

set(CMSIS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../library/cmsis" CACHE PATH "Path to CMSIS.")
set(TIMING_ADAPTER_PATH "${CMAKE_CURRENT_SOURCE_DIR}/timing-adapter" CACHE STRING "Path to timing adapter.")

if(EXISTS "${TIMING_ADAPTER_PATH}")
    option(DRIVER_BUILD_TIMING_ADAPTER "Enable timing-adapter" ON)
else()
    option(DRIVER_BUILD_TIMING_ADAPTER "Enable timing-adapter" OFF)
endif()

#
# Global settings
#

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "cortex-m0")
    add_compile_definitions(CPU_CORTEX_M0)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "cortex-m3")
    add_compile_definitions(CPU_CORTEX_M3)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "cortex-m33")
    add_compile_definitions(CPU_CORTEX_M33)
    add_compile_definitions(__DSP_PRESENT=1)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "cortex-m4")
    add_compile_definitions(CPU_CORTEX_M4)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "cortex-m55")
    add_compile_definitions(CPU_CORTEX_M55)
    add_compile_definitions(__DSP_PRESENT=1)
    add_compile_definitions(__FPU_PRESENT=1)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "cortex-m7")
    add_compile_definitions(CPU_CORTEX_M7)
else()
    message(FATAL_ERROR "Unsupported compiler ${CMAKE_SYSTEM_PROCESSOR}.")
endif()

# Enable logging support
if (LOG_SUPPORT)
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
target_include_directories(ethosu_core_driver
                           PUBLIC include)
target_sources(ethosu_core_driver PRIVATE src/ethosu_driver.c src/ethosu_device.c)

# Build timing adapter
if (DRIVER_BUILD_TIMING_ADAPTER)
    add_subdirectory(timing-adapter)
    target_link_libraries(ethosu_core_driver PUBLIC timing-adapter)
endif()

# Build PMU
if (DRIVER_BUILD_PMU)
    add_subdirectory(ethosu_pmu)
    target_link_libraries(ethosu_core_driver PUBLIC ethosu_core_driver_pmu)
endif()

#
# Print build status
#

message(STATUS "*******************************************************")
message(STATUS "PROJECT_NAME                           : ${PROJECT_NAME}")
message(STATUS "CMAKE_SYSTEM_PROCESSOR                 : ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "DRIVER_BUILD_PMU                       : ${DRIVER_BUILD_PMU}")
message(STATUS "DRIVER_BUILD_TIMING_ADAPTER            : ${DRIVER_BUILD_TIMING_ADAPTER}")
message(STATUS "LOG_SUPPORT                            : ${LOG_SUPPORT}")
message(STATUS "CMSIS_PATH                             : ${CMSIS_PATH}")
message(STATUS "TIMING_ADAPTER_PATH                    : ${TIMING_ADAPTER_PATH}")
message(STATUS "*******************************************************")

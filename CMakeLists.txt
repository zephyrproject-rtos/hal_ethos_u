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

set(CMSIS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmsis" CACHE PATH "Path to CMSIS.")

set(LOG_NAMES emerg alert crit err warning notice info debug)
set(ETHOSU_LOG_SEVERITY "info" CACHE STRING "Driver log severity level ${LOG_NAMES}")
set_property(CACHE ETHOSU_LOG_SEVERITY PROPERTY STRINGS ${LOG_NAMES})

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

# Check that ETHOSU_LOG_SEVERITY has one of the supported levels
list(FIND LOG_NAMES ${ETHOSU_LOG_SEVERITY} LOG_SEVERITY)
if (${LOG_SEVERITY} EQUAL -1)
    message(FATAL_ERROR "Unsupported log level ${ETHOSU_LOG_SEVERITY}")
endif()

# Enable PMU boot auto-initialization
if(DRIVER_PMU_AUTOINIT)
    add_compile_definitions(PMU_AUTOINIT)
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

# Set the log level for the target
target_compile_definitions(ethosu_core_driver PRIVATE ETHOSU_LOG_SEVERITY=${LOG_SEVERITY})

#
# Print build status
#

message(STATUS "*******************************************************")
message(STATUS "PROJECT_NAME                           : ${PROJECT_NAME}")
message(STATUS "CMAKE_SYSTEM_PROCESSOR                 : ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "CMSIS_PATH                             : ${CMSIS_PATH}")
message(STATUS "ETHOSU_LOG_SEVERITY                    : ${ETHOSU_LOG_SEVERITY}")
message(STATUS "*******************************************************")

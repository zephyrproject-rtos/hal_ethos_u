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

#ifndef ETHOSU_DEVICE_H
#define ETHOSU_DEVICE_H

/******************************************************************************
 * Includes
 ******************************************************************************/
#include "ethosu_types.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * Prototypes
 ******************************************************************************/
// Forward declararations
struct ethosu_device;
struct ethosu_device_desc;
struct ethosu_device_config;
struct ethosu_device_user_ops;
struct NPU_REG;

struct ethosu_device_ops
{
    bool (*init)(struct ethosu_device *dev,
                 const struct ethosu_device_desc *desc,
                 struct ethosu_device_config *cfg,
                 struct ethosu_device_user_ops *ops,
                 void *base_address,
                 uint32_t secure_enable,
                 uint32_t privilege_enable);
    void (*axi_init)(struct ethosu_device *dev);
    void (*run_command_stream)(struct ethosu_device *dev,
                               const uint8_t *cmd_stream_ptr,
                               uint32_t cms_length,
                               const uint64_t *base_addr,
                               int num_base_addr);
    void (*print_err_status)(struct ethosu_device *dev);
    bool (*handle_interrupt)(struct ethosu_device *dev);
    void (*get_hw_info)(struct ethosu_device *dev, struct ethosu_hw_info *hwinfo);
    bool (*verify_access_state)(struct ethosu_device *dev);
    bool (*soft_reset)(struct ethosu_device *dev);
    void (*set_clock_and_power)(struct ethosu_device *dev,
                                enum ethosu_clock_q_request clock_q,
                                enum ethosu_power_q_request power_q);
    bool (*verify_optimizer_config)(struct ethosu_device *dev, uint32_t cfg_in, uint32_t id_in);
};

/**
 * User controllable device operations
 */
struct ethosu_device_user_ops
{
    /**
     * Remap command stream and base pointer addresses.
     *
     * @param address   Address to be remapped.
     * @param index     -1 command stream, 0-n base address index
     *
     * @return Remapped address
     */
    uint64_t (*address_remap)(uint64_t address, int index);

    /**
     * Select configuration for region access.
     *
     * @param address   Address of region.
     * @param index     -1 command stream, 0-n base address index
     *
     * @return Configuration to use
     */
    unsigned int (*config_select)(uint64_t address, int index);
};

struct ethosu_device_caps
{
    uint32_t product;   // 0=u55, 1=u65, 2=u85
    uint32_t log2_macs; // log2(mac), 5=32macs, 6=64macs.. 11=2048macs
    uint32_t features;  // potential user bitmask (unused by driver)
};

/**
 * Device backend descriptor.
 */
struct ethosu_device_desc
{
    const char *name;
    const struct ethosu_device_ops *ops;
};

struct ethosu_device
{
    volatile struct NPU_REG *reg; // Register map
    uint32_t secure;
    uint32_t privileged;
    const struct ethosu_device_desc *desc;
    struct ethosu_device_user_ops *user_ops;
    struct ethosu_device_caps caps;
    struct ethosu_device_config *config;
};

struct ethosu_device_config
{
    void *config;
};

/******************************************************************************
 * U85 Device config
 ******************************************************************************/
#if defined(ETHOSU_MULTI_VARIANT) || defined(ETHOSU85)

// Default config - intentionally not const
extern struct ethosu_device_config ethosu_device_config_u85;

extern const struct ethosu_device_desc ethosu_device_desc_u85;

struct u85_mem_attr_t
{
    union
    {
        struct
        {
            uint32_t mem_domain : 2; // Memory domain
            uint32_t axi_port : 1;   // AXI port select
            uint32_t reserved0 : 1;
            uint32_t memtype : 4; // Memtype to be used to encode AxCACHE signals
            uint32_t reserved1 : 24;
        };
        uint32_t word;
    };
};

struct u85_qconfig_t
{
    union
    {
        struct
        {
            uint32_t cmd_region0 : 2; // Command region configuration
            uint32_t reserved0 : 30;
        };
        uint32_t word;
    };
};

struct u85_regioncfg_t
{
    union
    {
        struct
        {
            uint32_t region0 : 2; // Bits for Region0 Configuration
            uint32_t region1 : 2; // Bits for Region1 Configuration
            uint32_t region2 : 2; // Bits for Region2 Configuration
            uint32_t region3 : 2; // Bits for Region3 Configuration
            uint32_t region4 : 2; // Bits for Region4 Configuration
            uint32_t region5 : 2; // Bits for Region5 Configuration
            uint32_t region6 : 2; // Bits for Region6 Configuration
            uint32_t region7 : 2; // Bits for Region7 Configuration
            uint32_t reserved0 : 16;
        };
        uint32_t word;
    };
};

struct u85_axi_t
{
    union
    {
        struct
        {
            uint32_t max_outstanding_read_m1 : 6; // Maximum number of outstanding AXI read transactions per port - 1
            uint32_t reserved0 : 2;
            uint32_t max_outstanding_write_m1 : 5; // Maximum number of outstanding AXI write transactions per port - 1
            uint32_t reserved1 : 3;
            uint32_t max_beats : 2; // Burst split alignment
            uint32_t reserved2 : 14;
        };
        uint32_t word;
    };
};

struct u85_power_ctrl_t
{
    union
    {
        struct
        {
            uint32_t mac_step_cycles : 6; // MAC power ramping up/down control
            uint32_t reserved0 : 26;
        };
        uint32_t word;
    };
};

struct u85_config_t
{
    struct u85_mem_attr_t mem_attr[4];
    struct u85_qconfig_t qconfig;
    struct u85_regioncfg_t regioncfg;
    struct u85_axi_t axi_sram;
    struct u85_axi_t axi_ext;
    struct u85_power_ctrl_t power_ctrl;
};
#endif

/******************************************************************************
 * U65 Device config
 ******************************************************************************/
#if defined(ETHOSU_MULTI_VARIANT) || defined(ETHOSU65)

// Default config - intentionally not const
extern struct ethosu_device_config ethosu_device_config_u65;

extern const struct ethosu_device_desc ethosu_device_desc_u65;

struct u65_axi_limit_t
{
    union
    {
        struct
        {
            uint32_t max_beats : 2; // Burst split alignment
            uint32_t reserved0 : 2;
            uint32_t memtype : 4; // Memtype to be used to encode AxCACHE signals
            uint32_t reserved1 : 8;
            uint32_t
                max_outstanding_read_m1 : 6; // Maximum number of outstanding AXI read transactions - 1 in range 0 to 31
            uint32_t reserved2 : 2;
            uint32_t max_outstanding_write_m1 : 5; // Maximum number of outstanding AXI write transactions - 1 in range
                                                   // 0 to 15
            uint32_t reserved3 : 3;
        };
        uint32_t word;
    };
};

struct u65_regioncfg_t
{
    union
    {
        struct
        {
            uint32_t region0 : 2; // Bits for Region0 Configuration
            uint32_t region1 : 2; // Bits for Region1 Configuration
            uint32_t region2 : 2; // Bits for Region2 Configuration
            uint32_t region3 : 2; // Bits for Region3 Configuration
            uint32_t region4 : 2; // Bits for Region4 Configuration
            uint32_t region5 : 2; // Bits for Region5 Configuration
            uint32_t region6 : 2; // Bits for Region6 Configuration
            uint32_t region7 : 2; // Bits for Region7 Configuration
            uint32_t reserved0 : 16;
        };
        uint32_t word;
    };
};

struct u65_qconfig_t
{
    union
    {
        struct
        {
            uint32_t cmd_region0 : 2; // Command region configuration
            uint32_t reserved0 : 30;
        };
        uint32_t word;
    };
};

struct u65_config_t
{
    struct u65_qconfig_t qconfig;
    struct u65_regioncfg_t regioncfg;
    struct u65_axi_limit_t axi_limit0;
    struct u65_axi_limit_t axi_limit1;
    struct u65_axi_limit_t axi_limit2;
    struct u65_axi_limit_t axi_limit3;
};
#endif

/******************************************************************************
 * U55 Device config
 ******************************************************************************/
#if defined(ETHOSU_MULTI_VARIANT) || defined(ETHOSU55)

// Default config - intentionally not const
extern struct ethosu_device_config ethosu_device_config_u55;

extern const struct ethosu_device_desc ethosu_device_desc_u55;

struct u55_axi_limit_t
{
    union
    {
        struct
        {
            uint32_t max_beats : 2; // Burst split alignment
            uint32_t reserved0 : 2;
            uint32_t memtype : 4; // Memtype to be used to encode AxCACHE signals
            uint32_t reserved1 : 8;
            uint32_t
                max_outstanding_read_m1 : 5; // Maximum number of outstanding AXI read transactions - 1 in range 0 to 31
            uint32_t reserved2 : 3;
            uint32_t max_outstanding_write_m1 : 4; // Maximum number of outstanding AXI write transactions - 1 in range
                                                   // 0 to 15
            uint32_t reserved3 : 4;
        };
        uint32_t word;
    };
};

struct u55_regioncfg_t
{
    union
    {
        struct
        {
            uint32_t region0 : 2; // Bits for Region0 Configuration
            uint32_t region1 : 2; // Bits for Region1 Configuration
            uint32_t region2 : 2; // Bits for Region2 Configuration
            uint32_t region3 : 2; // Bits for Region3 Configuration
            uint32_t region4 : 2; // Bits for Region4 Configuration
            uint32_t region5 : 2; // Bits for Region5 Configuration
            uint32_t region6 : 2; // Bits for Region6 Configuration
            uint32_t region7 : 2; // Bits for Region7 Configuration
            uint32_t reserved0 : 16;
        };
        uint32_t word;
    };
};

struct u55_qconfig_t
{
    union
    {
        struct
        {
            uint32_t cmd_region0 : 2; // Command region configuration
            uint32_t reserved0 : 30;
        };
        uint32_t word;
    };
};

struct u55_config_t
{
    struct u55_qconfig_t qconfig;
    struct u55_regioncfg_t regioncfg;
    struct u55_axi_limit_t axi_limit0;
    struct u55_axi_limit_t axi_limit1;
    struct u55_axi_limit_t axi_limit2;
    struct u55_axi_limit_t axi_limit3;
};
#endif

#ifdef __cplusplus
}
#endif
#endif // ETHOSU_DEVICE_H

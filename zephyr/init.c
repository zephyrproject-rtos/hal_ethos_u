/*
 * Copyright (c) 2021 Arm Limited. All rights reserved.
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
#include "ethosu_driver.h"
#include <device.h>
#include <devicetree.h>
#include <init.h>
#include <kernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/printk.h>
#include <sys/util.h>

#define DT_DRV_COMPAT arm_ethos_u

/*******************************************************************************
 * Re-implementation/Overrides __((weak)) symbol functions from ethosu_driver.c
 * To handle mutex and semaphores
 *******************************************************************************/
void *ethosu_mutex_create(void)
{
    int status            = 0;
    struct k_mutex *mutex = k_malloc(sizeof(*mutex));
    status                = k_mutex_init(mutex);
    if (status != 0)
    {
        printk("Failed to create mutex with error - %d\n", status);
    }
    return (void *)mutex;
}

void ethosu_mutex_lock(void *mutex)
{
    int status = 0;
    status     = k_mutex_lock((struct k_mutex *)mutex, K_FOREVER);
    if (status != 0)
    {
        printk("Failed to lock mutex with error - %d\n", status);
    }
}

void ethosu_mutex_unlock(void *mutex)
{
    k_mutex_unlock((struct k_mutex *)mutex);
}

void *ethosu_semaphore_create(void)
{
    int status        = 0;
    struct k_sem *sem = k_malloc(sizeof(*sem));
    status            = k_sem_init(sem, 1, 100);
    if (status != 0)
    {
        printk("Failed to create semaphore with error - %d\n", status);
    }
    return (void *)sem;
}

void ethosu_semaphore_take(void *sem)
{
    int status = 0;
    status     = k_sem_take((struct k_sem *)sem, K_FOREVER);
    if (status != 0)
    {
        printk("Failed to take semaphore with error - %d\n", status);
    }
}

void ethosu_semaphore_give(void *sem)
{
    k_sem_give((struct k_sem *)sem);
}

struct ethosu_dts_info
{
    uint32_t base_addr;
    uint32_t secure_enable;
    uint32_t privilege_enable;
    uint32_t irq;
    uint32_t irq_priority;
    uint32_t inst;
};

struct ethosu_data
{
    struct ethosu_driver drv;
    void (*irq_config)(void);
};

static int ethosu_zephyr_init(const struct device *dev)
{
    const struct ethosu_dts_info *config = dev->config;
    const struct ethosu_data *data       = dev->data;
    struct ethosu_driver *drv            = (struct ethosu_driver *)&data->drv;
    struct ethosu_driver_version version;
    printk("Ethos-U DTS info. base_address=0x%x, inst=%u, secure_enable=%u, privilege_enable=%u, irq=%u, "
           "irq_priority=%u\n",
           config->base_addr,
           config->inst,
           config->secure_enable,
           config->privilege_enable,
           config->irq,
           config->irq_priority);

    ethosu_get_driver_version(&version);
    printk("Version. major=%u, minor=%u, patch=%u\n", version.major, version.minor, version.patch);
    if (ethosu_init(drv, (void *)(config->base_addr), NULL, 0, config->secure_enable, config->privilege_enable))
    {
        printk("Failed to initialize NPU with ethosu_init().\n");
        return -EINVAL;
    }

    data->irq_config();

    return 0;
}

#define ETHOSU_DEVICE_INIT(n)                                                                                          \
    static void ethosu_zephyr_irq_config_##n(void);                                                                    \
                                                                                                                       \
    static struct ethosu_data ethosu_data_##n = {.irq_config = &ethosu_zephyr_irq_config_##n};                         \
                                                                                                                       \
    static void ethosu_zephyr_irq_handler_##n(void)                                                                    \
    {                                                                                                                  \
        struct ethosu_driver *drv = &ethosu_data_##n.drv;                                                              \
        ethosu_irq_handler(drv);                                                                                       \
    }                                                                                                                  \
                                                                                                                       \
    static void ethosu_zephyr_irq_config_##n(void)                                                                     \
    {                                                                                                                  \
        IRQ_DIRECT_CONNECT(DT_INST_IRQN(n), DT_INST_IRQ(n, priority), ethosu_zephyr_irq_handler_##n, 0);               \
        irq_enable(DT_INST_IRQN(n));                                                                                   \
    }                                                                                                                  \
                                                                                                                       \
    static const struct ethosu_dts_info ethosu_dts_info_##n = {                                                        \
        .base_addr        = DT_INST_REG_ADDR(n),                                                                       \
        .secure_enable    = DT_INST_PROP(n, secure_enable),                                                            \
        .privilege_enable = DT_INST_PROP(n, privilege_enable),                                                         \
        .irq              = DT_INST_IRQN(n),                                                                           \
        .irq_priority     = DT_INST_IRQ(n, priority),                                                                  \
        .inst             = n,                                                                                         \
    };                                                                                                                 \
                                                                                                                       \
    DEVICE_DT_INST_DEFINE(n,                                                                                           \
                          ethosu_zephyr_init,                                                                          \
                          NULL,                                                                                        \
                          &ethosu_data_##n,                                                                            \
                          &ethosu_dts_info_##n,                                                                        \
                          POST_KERNEL,                                                                                 \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                                         \
                          NULL);

DT_INST_FOREACH_STATUS_OKAY(ETHOSU_DEVICE_INIT);

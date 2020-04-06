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

// IRQ
#if defined(CPU_CORTEX_M3) || defined(CPU_CORTEX_M4) || defined(CPU_CORTEX_M7) || defined(CPU_CORTEX_M33) ||           \
    defined(CPU_CORTEX_M55)
typedef enum irqn_type
{
    Reset            = -15,
    Nmi              = -14,
    HardFault        = -13,
    MemoryManagement = -12,
    BusFault         = -11,
    UsageFault       = -10,
    SVCall           = -5,
    DebugMonitor     = -4,
    PendSV           = -2,
    SysTick_IRQn     = -1,
    Irq0             = 0,
#if defined(FPGA)
#if defined(CPU_CORTEX_M55)
    EthosuIrq = 55
#else
    EthosuIrq = 67
#endif
#else
    EthosuIrq = Irq0
#endif
} IRQn_Type;

#define __CM7_REV 0x0000U
#define __MPU_PRESENT 1
#define __ICACHE_PRESENT 1
#define __DCACHE_PRESENT 1
#define __TCM_PRESENT 0
#define __NVIC_PRIO_BITS 3
#define __Vendor_SysTickConfig 0

#if defined(CPU_CORTEX_M7)
#include <core_cm7.h>
#elif defined(CPU_CORTEX_M4)
#include <core_cm4.h>
#elif defined(CPU_CORTEX_M3)
#include <core_cm3.h>
#elif defined(CPU_CORTEX_M0)
#include <core_cm0.h>
#elif defined(CPU_CORTEX_M33)
#include <core_cm33.h>
#elif defined(CPU_CORTEX_M55)
#include <core_cm55.h>
#else
#error "Unknown CPU"
#endif

typedef void (*ExecFuncPtr)();
static inline void setup_irq(void (*irq_handler)(), enum irqn_type irq_number)
{
    __NVIC_EnableIRQ(irq_number);
    ExecFuncPtr *vectorTable     = (ExecFuncPtr *)(SCB->VTOR);
    vectorTable[irq_number + 16] = irq_handler;
}

static inline void sleep()
{
    __WFI();
}

#endif

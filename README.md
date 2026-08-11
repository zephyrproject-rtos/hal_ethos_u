# Arm(R) Ethos(TM)-U core driver

This repository contains a device driver for the Arm(R) Ethos(TM)-U NPU.

## Building

The source code comes with a CMake based build system. The driver is expected to
be cross compiled for any of the supported Arm Cortex(R)-M CPUs, which requires
the user to configure the build to match their system configuration.


One such requirement is to define the target CPU, normally by setting
`CMAKE_SYSTEM_PROCESSOR`. **Note** that when using the toolchain files provided
in [core_platform](https://gitlab.arm.com/artificial-intelligence/ethos-u/ethos-u-core-platform),
the variable `TARGET_CPU` must be used instead of `CMAKE_SYSTEM_PROCESSOR`.

Target CPU is specified on the form "cortex-m<nr><features>", for example:
"cortex-m55+nodsp+nofp".

Similarly the target NPU configuration is
controlled by setting `ETHOSU_TARGET_NPU_CONFIG`, for example "ethos-u55-128".

The build configuration can be defined either in the toolchain file or
by passing options on the command line.

```[bash]
$ cmake -B build  \
    -DCMAKE_TOOLCHAIN_FILE=<toolchain> \
    -DCMAKE_SYSTEM_PROCESSOR=cortex-m<nr><features> \
    -DETHOSU_TARGET_NPU_CONFIG=ethos-u<nr>-<macs>
$ cmake --build build
```

or when using toolchain files from [core_platform](https://gitlab.arm.com/artificial-intelligence/ethos-u/ethos-u-core-platform)

```[bash]
$ cmake -B build  \
    -DCMAKE_TOOLCHAIN_FILE=<core_platform_toolchain> \
    -DTARGET_CPU=cortex-m<nr><features> \
    -DETHOSU_TARGET_NPU_CONFIG=ethos-u<nr>-<macs>
$ cmake --build build
```
## Compiler flags used

The Arm Ethos-U core driver component adds the -Werror flag in addition
to the compiler flags specified in the toolchain file, or options passed
on the command line.

## Getting started

Driver instances are typically created by platform or target initialization
code. The target code owns the `struct ethosu_driver` object, passes the NPU
register base address to `ethosu_init()`, and connects the target interrupt
handler to `ethosu_irq_handler()`. If `ETHOSU_MULTI_VARIANT` is enabled, use
`ethosu_init_ex()` instead so the target code can provide the device descriptor
and configuration for each driver instance.

For CMSIS-based target examples, see the `core_platform/targets/*/target.cpp`
files. They show the usual pattern: declare a target-owned driver object such
as `ethosu0_driver`, call `ethosu_init()` from target setup, install an NPU IRQ
handler with the target interrupt controller, and have that IRQ handler call
`ethosu_irq_handler(&ethosu0_driver)`. With multi-device support enabled, the
same target-level setup uses `ethosu_init_ex()` and passes the device
descriptor, device configuration, and optional per-driver user ops.

After the target has initialized the driver instance, it's normally up to a
framework to later reserve a driver, invoke an inference, and then release the
driver again. The framework is expected to provide the command stream, base
pointer array, base pointer sizes, and number of base pointers from the
compiled network.

When multi-device support is enabled a framework can reserve a specific NPU
variant with `ethosu_reserve_driver_ex()`, or use `ethosu_invoke_auto()` to let
the driver parse the command stream metadata and reserve a matching driver.

### Command stream regions and base pointers

Vela, the Ethos-U compiler, emits command streams that refer to memory regions
and offsets into those regions. The invoke API provides one base pointer per
region: `base_addr[0]` is used for region 0, `base_addr[1]` for region 1, and
so on. The same region number also maps to the matching field in the NPU
`REGIONCFG` register.

`REGIONCFG` does not contain addresses. It selects the memory configuration for
each region, including which NPU AXI port is used for requests to that region.

The command stream itself has its own base pointer register, `QBASE`. The AXI
port used to read the command stream is selected by `QCONFIG`, which uses
the same memory configuration encoding as `REGIONCFG`.

The driver provides default region configuration values in
`src/ethosu_config_u55.h`, `src/ethosu_config_u65.h`, and
`src/ethosu_config_u85.h`, but a target may need to adjust them to match its
Vela memory mode and memory system. In single-device builds the defaults come
from `NPU_REGIONCFG_[0-7]`, or from an override of `ethosu_config_select()`. In
multi-device builds the values come from the device configuration passed to
`ethosu_init_ex()`, or from a per-driver `config_select` user op.

Vela memory modes describe common ways to place regions in memory. They are
examples of typical systems; a target is free to use a different memory map as
long as the base pointers and `REGIONCFG` values match that system.

| Vela memory mode | Region 0 constants | Region 1 scratch | Region 2 fast scratch |
| --- | --- | --- | --- |
| `Sram_Only` | SRAM | SRAM | Not used |
| `Shared_Sram` | DRAM/Flash | SRAM | Not used |
| `Dedicated_Sram` | DRAM/Flash | DRAM | SRAM |

In `Dedicated_Sram` mode Vela uses region 2 for fast scratch. The driver calls
this region `FAST_MEMORY`. The driver must know the actual fast memory address
and size through the `fast_memory` and `fast_memory_size` arguments to
`ethosu_init()` or `ethosu_init_ex()`. If fast memory is configured and the
invoke call includes base pointer 2, the driver checks that `base_addr_size[2]`
fits inside `fast_memory_size` and rewrites `base_addr[2]` to the configured
fast memory address before programming the NPU. Nothing extra is needed from a
framework to replace base pointer 2.

#### Ethos-U55 and Ethos-U65

The AXI ports may be referred to as `AXI0` and `AXI1`.

| Memory placement | AXI port |
| --- | --- |
| SRAM | `AXI0` |
| DRAM/Flash | `AXI1` |

The driver provides the following default values:

| Product | `NPU_QCONFIG` | `NPU_REGIONCFG_0` | `NPU_REGIONCFG_1` | `NPU_REGIONCFG_2` |
| --- | --- | --- | --- | --- |
| Ethos-U55 | `2` -> `AXI1` | `3` -> `AXI1` | `0` -> `AXI0` | `1` -> `AXI0` |
| Ethos-U65 | `2` -> `AXI1` | `3` -> `AXI1` | `0` -> `AXI0` | `1` -> `AXI0` |

For Ethos-U55/U65, `QCONFIG` and the `REGIONCFG[0..7]` fields accept values
0-3:

| Value | AXI port | Applies settings from |
| --- | --- | --- |
| `0` | `AXI0` | `AXI_LIMIT0` |
| `1` | `AXI0` | `AXI_LIMIT1` |
| `2` | `AXI1` | `AXI_LIMIT2` |
| `3` | `AXI1` | `AXI_LIMIT3` |

The `AXI_LIMIT[0-3]` registers also contain AxCACHE/AxDOMAIN and AXI limit settings,
not covered in this documentation.

The Ethos-U55 `AXI1` port is read-only. If region 1 contains writable scratch
data, it must not be routed through `AXI1`.

#### Ethos-U85

The AXI ports are referred to as `AXI_SRAM` and `AXI_EXT`.

| Memory placement | AXI port |
| --- | --- |
| SRAM | `AXI_SRAM` |
| DRAM/Flash | `AXI_EXT` |

The driver provides the following default values:

| Product | `NPU_QCONFIG` | `NPU_REGIONCFG_0` | `NPU_REGIONCFG_1` | `NPU_REGIONCFG_2` |
| --- | --- | --- | --- | --- |
| Ethos-U85 | `2` -> `MEM_ATTR_2` -> `AXI_EXT` | `3` -> `MEM_ATTR_3` -> `AXI_EXT` | `0` -> `MEM_ATTR_0` -> `AXI_SRAM` | `1` -> `MEM_ATTR_1` -> `AXI_SRAM` |

For Ethos-U85, AXI port selection and AxCACHE/AxDOMAIN settings are made in the
`MEM_ATTR` registers, and `REGIONCFG` selects which `MEM_ATTR` entry to use.
The default `MEM_ATTR_0` and `MEM_ATTR_1` entries use `AXI_SRAM`, while
`MEM_ATTR_2` and `MEM_ATTR_3` use `AXI_EXT`.
These default `MEM_ATTR` values are set by the driver to replicate the default
Ethos-U55/U65 behavior, making it easier to correlate configurations across
products, but the `MEM_ATTR` values are completely up to the user to configure
if desired.

Ethos-U85 AXI limits are configured in the `AXI_SRAM` and `AXI_EXT` registers.
There is one `AXI_SRAM` register and one `AXI_EXT` register, so those limit
settings apply to all ports in each group.

Ethos-U85 AXI information:

| U85 configuration (MACs/CC) | Number of SRAM ports | Max outstanding reads per port | Max outstanding writes per port |
| --- | --- | --- | --- |
| 128 | 2 | 12 | 16 |
| 256 | 2 | 12 | 16 |
| 512 | 2 | 12 | 16 |
| 1024 | 2 | 12 | 16 |
| 2048 | 4 | 12 | 16 |

| U85 configuration (MACs/CC) | Number of EXT ports | Max outstanding reads per port | Max outstanding writes per port |
| --- | --- | --- | --- |
| 128 | 1 | 32 | 32 |
| 256 | 1 | 32 | 32 |
| 512 | 1 | 64 | 32 |
| 1024 | 2 | 64 | 32 |
| 2048 | 2 | 64 | 32 |

## EXPERIMENTAL - Multi variant

Experimental support for using multiple NPU variants in one system. An NPU variant is
the combination of product type (U55/U65/U85) and MAC configuration, for example ethos-u55-128,
ethos-u65-256 or ethos-u85-1024.

Set the CMake variable `ETHOSU_MULTI_VARIANT` to `ON` to enable the feature.
With this feature enabled, the driver is no longer looking at the `ETHOSU_TARGET_NPU_CONFIG`
variable, but builds support for all Ethos-U products (the target system is *not*
required to have multiple NPU devices to enable this mode and access the new APIs).

To use this feature, it requires some minor code changes to adapt for a more flexible
per driver instance configuration.

The `ethosu_init()` and `ethosu_reserve_driver()` functions can not be used when
this feature is enabled, they are replaced by `ethosu_init_ex()` and
`ethosu_reserve_driver_ex()` respectively.

### Init
In the target/system init code, change the typical `ethosu_init()` calls to:
```[C]
int ethosu_init_ex(struct ethosu_driver *drv,
                   const struct ethosu_device_desc *dev_desc,
                   struct ethosu_device_config *dev_config,
                   struct ethosu_device_user_ops *dev_user_ops,
                   void *const base_address,
                   const void *fast_memory,
                   const size_t fast_memory_size,
                   uint32_t secure_enable,
                   uint32_t privilege_enable);

ethosu_init_ex(&ethosu0_driver,
               &ethosu_device_desc_u85,
               &ethosu_device_config_u85,
               NULL, // No user ops, or create a struct ethosu_user_ops and reference it here
               ...);
```
where the second argument is a const device descriptor provided by the driver
to be set, depending on device type/product used:
```[C]
extern const struct ethosu_device_desc ethosu_device_desc_u55;
extern const struct ethosu_device_desc ethosu_device_desc_u65;
extern const struct ethosu_device_desc ethosu_device_desc_u85;
```

The third argument specifices a device config, where default configs are provided
as well. **Note** The default ones are global variables, shared between all instances
of a specific device type/product.

```[C]
// Default configs - intentionally not const
extern struct ethosu_device_config ethosu_device_config_u55;
extern struct ethosu_device_config ethosu_device_config_u65;
extern struct ethosu_device_config ethosu_device_config_u85;
```

The fourth argument specifices optional user ops (these were previously weak functions
provided by the driver), set to `NULL` if not used. **Note** No default implementation
is provided for these when multi variant support is enabled. See `include/ethosu_device.h` for more info:
```[C]
struct ethosu_device_user_ops
{
    uint64_t (*address_remap)(uint64_t address, int index);
    unsigned int (*config_select)(uint64_t address, int index);
};
```

### Custom config
A user can decide to create a custom configuration, for example:
```[C]
// Copy initial config values from default
struct u55_config_t u55_cfg = *((struct u55_config_t *) ethosu_device_config_u55.config);

// then if desired, change specific settings later in the code:
// u55_cfg.qconfig.cmd_region0 = ...

// Or create it from scratch and set all settings here
// struct u55_config_t u55_cfg = {
//     .qconfig.cmd_region0 = ...
//     ...
// };

// Assign the device specific config struct to a generic ethosu_device_config struct
struct ethosu_device_config ethosu0_config = {
    .config = &u55_cfg,
};
// and send &ethosu0_config instead of default config to ethosu_init_ex()
```

### Driver reservation
To reserve a driver, the user must explicitly provide what NPU variant
is being asked for. For example, Ethos-U55 with 128 MAC config:
```[C]
struct ethosu_driver *drv;
drv = ethosu_reserve_driver_ex(ETHOSU_PRODUCT_U55, ETHOSU_MACS_128);
...
```
using the available driver provided macros:
```[C]
ETHOSU_PRODUCT_U55
ETHOSU_PRODUCT_U65
ETHOSU_PRODUCT_U85
```
and
```[C]
ETHOSU_MACS_32
ETHOSU_MACS_64
ETHOSU_MACS_128
ETHOSU_MACS_256
ETHOSU_MACS_512
ETHOSU_MACS_1024
ETHOSU_MACS_2048
```

### New optional invoke (auto) method
In addition to the invoke methods described in the following sections, with the multi variant
experimental feature, a new function called `ethosu_invoke_auto()` has been added. This function
omits the driver argument, hence the user should not reserve a driver before calling it. The
`ethosu_invoke_auto()` function automatically parses the provided data and tries to reserve a
suitable driver internally. **Note** This function will block until a suitable driver is found.
After the inference has been invoked, and before this function returns, it will release the driver.

```[C]
/**
 * Call this to automatically find a suitable driver matching what the network has been compiled for.
 * Note that this will potentially block waiting for a driver to become available, as it does
 * implicit reserve- and release of a matching driver.
 *
 * @see ethosu_invoke_v3 for documentation, except it doesn't take a driver arg.
 */
int ethosu_invoke_auto(const void *custom_data_ptr,
                       const int custom_data_size,
                       uint64_t *const base_addr,
                       const size_t *base_addr_size,
                       const int num_base_addr,
                       void *user_arg);
```

### Breaking changes when enabling multi variant mode
- The `ETHOSU_PMU_Get_NumEventCounters()` function and the `ETHOSU_PMU_NCOUNTERS` macro are not available. Switch to use `ETHOSU_PMU_Get_NumEventCountersForDrv(struct ethosu_driver *drv)` instead.
- The weak function `ethosu_address_remap()` is replaced by a per device user op. To prevent this being missed, any attempt to override will result in compile time error.
- The weak function `ethosu_config_select()` is replaced by a per device user op. To prevent this being missed, any attempt to override will result in compile time error. This is provided as a convenience function, as configuration can also be changed at runtime by modifying the `ethosu_device_config` struct.

### Notes
- The abstracted PMU event list is no longer tied to interface PMU event list in terms of sorting/order. It's now a union of all available PMU events for all supported device types/products. If hardcoded references to index numbers are used (instead of the supported PMU event macros, they must be updated).

## Driver APIs

The driver APIs are defined in `include/ethosu_driver.h` and the related types
in `include/ethosu_types.h`. Inferences can be invoked in two manners:
synchronously or asynchronously. The two types of invocation can be freely mixed
in a single application. Frameworks typically use the blocking synchronous API,
while the asynchronous API is mainly intended for bare-metal integrations, or
for frameworks that explicitly support asynchronous execution.

### Synchronous invocation

A typical synchronous integration can look like this:

```[C]
// reserve a driver to be used (this call could block until a driver is available)
struct ethosu_driver *drv = ethosu_reserve_driver();
...
// run one or more inferences
int result = ethosu_invoke(drv,
                           custom_data_ptr,
                           custom_data_size,
                           base_addr,
                           base_addr_size,
                           num_base_addr);
...
// release the driver for others to use
ethosu_release_driver(drv);
```

### Asynchronous invocation

The asynchronous API can be used by bare-metal integrations that want to run
other work while the NPU is executing. It can also be used by a framework if
the framework supports asynchronous operations.

```[C]
// reserve a driver to be used (this call could block until a driver is available)
struct ethosu_driver *drv = ethosu_reserve_driver();
...
// run one or more inferences
int result = ethosu_invoke_async(drv,
                                 custom_data_ptr,
                                 custom_data_size,
                                 base_addr,
                                 base_addr_size,
                                 num_base_addr,
                                 user_arg);
...
// do some other work
...
int ret;
do {
    // true = blocking, false = non-blocking
    // ret > 0 means inference not completed (only for non-blocking mode)
    ret = ethosu_wait(drv, <true|false>);
} while(ret > 0);
...
// release the driver for others to use
ethosu_release_driver(drv);
```

Note that if `ethosu_wait` is invoked from a different thread and concurrently
with `ethosu_invoke_async`, the user is responsible to guarantee that
`ethosu_wait` is called after a successful completion of `ethosu_invoke_async`.
Otherwise `ethosu_wait` might fail and not actually wait for the inference
completion.

### Driver initialization

In order to use a driver it first needs to be initialized by calling the `init`
function, which will also register the handle in the list of available drivers.
A driver can be torn down by using the `deinit` function, which also removes the
driver from the list.

The correct mapping is one driver per NPU device. Note that the NPUs must have
the same configuration, indeed the NPU configuration can be only one, which is
defined at compile time.

## Implementation design

The driver is structured in two main parts: the driver, which is responsible to
provide an unified API to the user; and the device part, which deals with the
details at the hardware level.

In order to do its task the driver needs a device implementation. There could be
multiple device implementation for different hardware model and/or
configurations. Note that the driver can be compiled to target only one NPU
configuration by specializing the device part at compile time.

## Data caching

For running the driver on Arm CPUs which are configured with data cache, certain
caution must be taken to ensure cache coherency. The driver expects that cache
clean/flush has been done by the user application before being invoked. The
driver does provide a deprecated weakly linked function `ethosu_flush_dcache`
that could be overriden, causing the driver to cache flush/clean base pointers
before each inference.

The driver also exposes a weakly linked symbol for cache invalidation called
`ethosu_invalidate_dcache`, that must be overriden when the data cache is used.
After an inference completes on the NPU, the driver will call this function to
invalidate the data cache, to ensure cache coherency.

Make sure that any base pointers used for flush/invalidation is aligned to the
cache line size of your CPU, typically 32 bytes. Due to the uncertainty of
tensor alignment, the driver only flushes/invalidates on base pointer level.

A simple example implementation for the weak functions, using CMSIS primitives
could look like below:

```[C++]
extern "C" {
void ethosu_flush_dcache(const uint64_t *base_addr, const size_t *base_addr_size, int num_base_addr)
{
    for (int i = 0; i < num_base_addr; i++)
        SCB_CleanDCache_by_Addr((uint32_t *)(uintptr_t)base_addr[i], base_addr_size[i]);
}

void ethosu_invalidate_dcache(const uint64_t *base_addr, const size_t *base_addr_size, int num_base_addr)
{
    for (int i = 0; i < num_base_addr; i++)
        SCB_InvalidateDCache_by_Addr((uint32_t *)(uintptr_t)base_addr[i], base_addr_size[i]);
}
} // extern "C"
```

The NPU contain memory attributes that should be set to match the settings used
in the MPU configuration for the memories used. See `NPU_MEM_ATTR_[0-3]` for
Ethos-U85 and the `AXI_LIMIT[0-3]_MEM_TYPE` for Ethos-U55/Ethos-U65 in
corresponding `src/ethosu_config_uX5.h` files.

## Mutex and semaphores

To ensure the correct functionality of the driver mutexes and semaphores are
used internally. The default implementations of mutexes and semaphores are
designed for a single-threaded baremetal environment. Hence for integration in
environemnts where multi-threading is possible, e.g., RTOS, the user is
responsible to provide implementation for mutexes and semaphores to be used by
the driver.

The mutex and semaphores are used as synchronisation mechanisms and unless
specified, the timeout is required to be 'forever'.

The driver allows for an RTOS to set a timeout for the NPU interrupt semaphore.
The timeout can be set with the CMake variable `ETHOSU_INFERENCE_TIMEOUT`, which
is then used as `timeout` argument for the interrupt semaphore take call. Note
that the unit is implementation defined, the value is shipped as is to the
`ethosu_semaphore_take()` function and an override implementation should cast it
to the appropriate type and/or convert it to the unit desired.

A macro `ETHOSU_SEMAPHORE_WAIT_FOREVER` is defined in the driver header file,
and should be made sure to map to the RTOS' equivalent of
'no timeout/wait forever'. Inference timeout value defaults to this if left
unset. The macro is used internally in the driver for the available NPU's, thus
the driver does NOT support setting a timeout other than forever when waiting
for an NPU to become available (global ethosu_semaphore).

The mutex and semaphore APIs are defined as weak linked functions that can be
overridden by the user. The APIs are the usual ones and described below:

```[C]
// create a mutex by returning back a handle
void *ethosu_mutex_create(void);
// lock the given mutex
int ethosu_mutex_lock(void *mutex);
// unlock the given mutex
int ethosu_mutex_unlock(void *mutex);

// create a (counting) semaphore by returning back a handle
void *ethosu_semaphore_create(void);
// take from the given semaphore, accepting a timeout (unit impl. defined)
int ethosu_semaphore_take(void *sem, uint64_t timeout);
// give from the given semaphore
int ethosu_semaphore_give(void *sem);
```

## Begin/End inference callbacks

The driver provides weak linked functions as hooks to receive callbacks whenever
an inference begins and ends. The user can override such functions when needed.
To avoid memory leaks, any allocations done in the ethosu_inference_begin() must
be balanced by a corresponding free of the memory in the ethosu_inference_end()
callback.

The end callback will always be called if the begin callback has been called,
including in the event of an interrupt semaphore take timeout.

```[C]
void ethosu_inference_begin(struct ethosu_driver *drv, void *user_arg);
void ethosu_inference_end(struct ethosu_driver *drv, void *user_arg);
```

These callbacks can be used to read out PMU values for a single inference. A
typical pattern is to configure or reset PMU counters in
`ethosu_inference_begin()`, then read the counter values in
`ethosu_inference_end()`.

Note that the `void *user_arg` pointer passed to the invoke function is the same
pointer passed to the begin and end callbacks. For example:

```[C]
#include "ethosu_pmu_funcs.h"

struct inference_metrics
{
    uint64_t cycle_count;
};

void my_function(void)
{
    struct inference_metrics metrics = {0};

    int result = ethosu_invoke_v3(drv,
                                  custom_data_ptr,
                                  custom_data_size,
                                  base_addr,
                                  base_addr_size,
                                  num_base_addr,
                                  (void *)&metrics);
}

void ethosu_inference_begin(struct ethosu_driver *drv, void *user_arg)
{
    ETHOSU_PMU_CYCCNT_Reset(drv);
    ETHOSU_PMU_CNTR_Enable(drv, 1u << 31);
    (void)user_arg;
}

void ethosu_inference_end(struct ethosu_driver *drv, void *user_arg)
{
    struct inference_metrics *metrics = (struct inference_metrics *)user_arg;

    metrics->cycle_count = ETHOSU_PMU_Get_CCNTR(drv);
    ETHOSU_PMU_CNTR_Disable(drv, 1u << 31);
}
```

## License

The Arm Ethos-U core driver is provided under an Apache-2.0 license. Please see
[LICENSE.txt](LICENSE.txt) for more information.

## Contributions

The Arm Ethos-U project welcomes contributions under the Apache-2.0 license.

Before we can accept your contribution, you need to certify its origin and give
us your permission. For this process we use the Developer Certificate of Origin
(DCO) V1.1 (https://developercertificate.org).

To indicate that you agree to the terms of the DCO, you "sign off" your
contribution by adding a line with your name and e-mail address to every git
commit message. You must use your real name, no pseudonyms or anonymous
contributions are accepted. If there are more than one contributor, everyone
adds their name and e-mail to the commit message.

```[]
Author: John Doe \<john.doe@example.org\>
Date:   Mon Feb 29 12:12:12 2016 +0000

Title of the commit

Short description of the change.

Signed-off-by: John Doe john.doe@example.org
Signed-off-by: Foo Bar foo.bar@example.org
```

The contributions will be code reviewed by Arm before they can be accepted into
the repository.

In order to submit a contribution, submit a merge request to the
[core_driver](https://gitlab.arm.com/artificial-intelligence/ethos-u/ethos-u-core-driver)
repository. To do this you will need to sign-up at [gitlab.arm.com](https://gitlab.arm.com)
and add your SSH key under your settings.

## Security

Please see [Security](SECURITY.md).

## Trademark notice

Arm, Cortex and Ethos are registered trademarks of Arm Limited (or its
subsidiaries) in the US and/or elsewhere.

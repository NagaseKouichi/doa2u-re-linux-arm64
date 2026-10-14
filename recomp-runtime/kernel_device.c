#include "device_model.h"
#include "kernel_abi.h"

#include <stdint.h>
#include <stdio.h>

static void bridge_io_create_device(void)
{
    uint32_t driver_object = kernel_arg(1u);
    uint32_t extension_size = kernel_arg(2u);
    uint32_t device_name = kernel_arg(3u);
    uint32_t device_type = kernel_arg(4u);
    uint32_t exclusive = kernel_arg(5u);
    uint32_t device_object_pointer = kernel_arg(6u);
    RecompDeviceCreateResult result = recomp_device_create(
        driver_object, extension_size, device_type, device_object_pointer);

    fprintf(
        stderr,
        "recomp kernel: IoCreateDevice driver=0x%08x"
        " extension_size=%u name=0x%08x device_type=0x%08x"
        " exclusive=%u output=0x%08x object=0x%08x extension=0x%08x"
        " status=0x%08x\n",
        (unsigned)driver_object,
        (unsigned)extension_size,
        (unsigned)device_name,
        (unsigned)device_type,
        (unsigned)exclusive,
        (unsigned)device_object_pointer,
        (unsigned)result.device_object,
        (unsigned)result.device_extension,
        (unsigned)result.status);
    kernel_return(6u, result.status);
}

/* shortcut: offline host has no PHY; bind native link status with online services.
   0x801F0001 is the PHY hardware-error result. */
static void bridge_phy_initialize(void)
{
    kernel_return(2u, 0x801f0001u);
}

static void bridge_phy_get_link_state(void)
{
    kernel_return(1u, 0u);
}

RecompFunction recomp_kernel_device(uint32_t ordinal)
{
    switch (ordinal) {
    case 65u: return bridge_io_create_device;
    case 252u: return bridge_phy_get_link_state;
    case 253u: return bridge_phy_initialize;
    default: return NULL;
    }
}

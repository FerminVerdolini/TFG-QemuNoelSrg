/*
 * GR GPIO controller 
 *
 */

#ifndef GR_GPIO_H
#define GR_GPIO_H

#include "hw/sysbus.h"
#include "qom/object.h"

#define TYPE_GR_GPIO "gr_soc.gpio"
typedef struct GRGPIOState GRGPIOState;
DECLARE_INSTANCE_CHECKER(GRGPIOState, GR_GPIO, TYPE_GR_GPIO)

#define GR_GPIO_PINS 32

#define GR_GPIO_SIZE 0x100

#define GR_GPIO_REG_IN         0x000
#define GR_GPIO_REG_OUT        0x004
#define GR_GPIO_REG_DIR        0x008

struct GRGPIOState {
    SysBusDevice parent_obj;

    MemoryRegion mmio;

    qemu_irq irq[GR_GPIO_PINS];
    qemu_irq gpio_out[GR_GPIO_PINS];
    qemu_irq gpio_dir[GR_GPIO_PINS];
//    qemu_irq gpio_in[GR_GPIO_PINS];

    uint32_t value;             /* Actual value of the pin */
    uint32_t dir;

    uint32_t ngpio;

};

#endif /* GR_GPIO_H */

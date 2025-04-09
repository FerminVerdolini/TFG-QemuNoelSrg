/*
 * QEMU RISC-V Board Compatible with NOEL Gaisler
 *
 * Copyright (c) 2017 SiFive, Inc.
 * Copyright (c) 2024 SRG.
 *
 * Provides a board compatible with the NOEL Gaisler:
 *
 * 0) UART
 * 1) CLINT (Core Level Interruptor)
 * 2) PLIC (Platform Level Interrupt Controller)
 * 3) PRCI (Power, Reset, Clock, Interrupt)
 * 4) Registers emulated as RAM: AON, GPIO, QSPI, PWM
 * 5) Flash memory emulated as RAM
 *
 * The Mask ROM reset vector jumps to the flash payload at 0x2040_0000.
 * The OTP ROM and Flash boot code will be emulated in a future version.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "qemu/cutils.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "hw/sysbus.h"
#include "hw/char/serial.h"
#include "hw/misc/unimp.h"
#include "target/riscv/cpu.h"
#include "hw/riscv/riscv_hart.h"
#include "hw/riscv/noel_srg.h"
#include "hw/riscv/boot.h"
#include "hw/char/sifive_uart.h"
#include "hw/intc/riscv_aclint.h"
#include "hw/intc/sifive_plic.h"
#include "hw/misc/sifive_e_aon.h"
#include "chardev/char.h"
#include "sysemu/sysemu.h"

static const MemMapEntry noel_srg_memmap[] = {
    [SIFIVE_E_DEV_DEBUG] =    {        0x0,     0x1000 },
    [SIFIVE_E_DEV_MROM] =     {     0x1000,     0x2000 },
    [SIFIVE_E_DEV_CLINT] =    {  0x2000000,    0x10000 },
    [SIFIVE_E_DEV_PLIC] =     {  0xc000000,  0x4000000 },
    [SIFIVE_E_DEV_GPIO0] =    { 0x10012000,     0x1000 },
    [SIFIVE_E_DEV_UART0] =    { 0x10013000,     0x1000 },
    [SIFIVE_E_DEV_XIP] =      { 0x20000000, 0x20000000 },
    [SIFIVE_E_DEV_DTIM] =     { 0x80000000,     0x4000 }
};

static void noel_srg_machine_init(MachineState *machine)
{
    MachineClass *mc = MACHINE_GET_CLASS(machine);
    const MemMapEntry *memmap = noel_srg_memmap;

    NOELSRGState *s = RISCV_NOEL_SRG_MACHINE(machine);
    MemoryRegion *sys_mem = get_system_memory();
    int i;

    if (machine->ram_size != mc->default_ram_size) {
        char *sz = size_to_str(mc->default_ram_size);
        error_report("Invalid RAM size, should be %s", sz);
        g_free(sz);
        exit(EXIT_FAILURE);
    }

    /* Initialize SoC */
    object_initialize_child(OBJECT(machine), "soc", &s->soc, TYPE_RISCV_NOEL_SRG_SOC);
    qdev_realize(DEVICE(&s->soc), NULL, &error_fatal);

    /* Data Tightly Integrated Memory */
    memory_region_add_subregion(sys_mem,
        memmap[SIFIVE_E_DEV_DTIM].base, machine->ram);

    /* Mask ROM reset vector */
    uint32_t reset_vec[4];

    if (s->revb) {
        reset_vec[1] = 0x200102b7;  /* 0x1004: lui     t0,0x20010 */
    } else {
        reset_vec[1] = 0x204002b7;  /* 0x1004: lui     t0,0x20400 */
    }
    reset_vec[2] = 0x00028067;      /* 0x1008: jr      t0 */

    reset_vec[0] = reset_vec[3] = 0;

    /* copy in the reset vector in little_endian byte order */
    for (i = 0; i < sizeof(reset_vec) >> 2; i++) {
        reset_vec[i] = cpu_to_le32(reset_vec[i]);
    }
    rom_add_blob_fixed_as("mrom.reset", reset_vec, sizeof(reset_vec),
                          memmap[SIFIVE_E_DEV_MROM].base, &address_space_memory);

    if (machine->kernel_filename) {
        riscv_load_kernel(machine, &s->soc.cpus,
                          memmap[SIFIVE_E_DEV_DTIM].base,
                          false, NULL);
    }
}

static bool noel_srg_machine_get_revb(Object *obj, Error **errp)
{
    NOELSRGState *s = RISCV_NOEL_SRG_MACHINE(obj);

    return s->revb;
}

static void noel_srg_machine_set_revb(Object *obj, bool value, Error **errp)
{
    NOELSRGState *s = RISCV_NOEL_SRG_MACHINE(obj);

    s->revb = value;
}

static void noel_srg_machine_instance_init(Object *obj)
{
    NOELSRGState *s = RISCV_NOEL_SRG_MACHINE(obj);

    s->revb = false;
}

static void noel_srg_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "RISC-V NOEL Board";
    mc->init = noel_srg_machine_init;
    mc->max_cpus = 1;
    mc->default_cpu_type = SIFIVE_E_CPU;
    mc->default_ram_id = "riscv.noel.srg.ram";
    mc->default_ram_size = noel_srg_memmap[SIFIVE_E_DEV_DTIM].size;

    object_class_property_add_bool(oc, "revb", noel_srg_machine_get_revb,
                                   noel_srg_machine_set_revb);
    object_class_property_set_description(oc, "revb",
                                          "Set on to tell QEMU that it should model "
                                          "the revB HiFive1 board");
}
static const TypeInfo noel_srg_machine_typeinfo = {
   .name       = MACHINE_TYPE_NAME("noel-srg"),
    .name       = TYPE_RISCV_NOEL_SRG_MACHINE,
    .parent     = TYPE_MACHINE,
    .class_init = noel_srg_machine_class_init,
    .instance_init = noel_srg_machine_instance_init,
    .instance_size = sizeof(NOELSRGState),
};

static void noel_srg_machine_init_register_types(void)
{
    type_register_static(&noel_srg_machine_typeinfo);
}

type_init(noel_srg_machine_init_register_types)

static void noel_srg_soc_init(Object *obj)
{
    MachineState *ms = MACHINE(qdev_get_machine());
    NOELSRGSoCState *s = RISCV_NOEL_SRG_SOC(obj);

    object_initialize_child(obj, "cpus", &s->cpus, TYPE_RISCV_HART_ARRAY);
    object_property_set_int(OBJECT(&s->cpus), "num-harts", ms->smp.cpus,
                            &error_abort);
    object_property_set_int(OBJECT(&s->cpus), "resetvec", 0x1004, &error_abort);
    object_initialize_child(obj, "riscv.noel.srg.gpio0", &s->gpio,
                            TYPE_SIFIVE_GPIO);
    
}

static void noel_srg_soc_realize(DeviceState *dev, Error **errp)
{
    MachineState *ms = MACHINE(qdev_get_machine());
    const MemMapEntry *memmap = noel_srg_memmap;
    NOELSRGSoCState *s = RISCV_NOEL_SRG_SOC(dev);
    MemoryRegion *sys_mem = get_system_memory();

    object_property_set_str(OBJECT(&s->cpus), "cpu-type", ms->cpu_type,
                            &error_abort);
    sysbus_realize(SYS_BUS_DEVICE(&s->cpus), &error_fatal);

    /* Mask ROM */
    memory_region_init_rom(&s->mask_rom, OBJECT(dev), "riscv.noel.srg.mrom",
                           memmap[SIFIVE_E_DEV_MROM].size, &error_fatal);
    memory_region_add_subregion(sys_mem,
        memmap[SIFIVE_E_DEV_MROM].base, &s->mask_rom);

    /* MMIO */
    s->plic = sifive_plic_create(memmap[SIFIVE_E_DEV_PLIC].base,
        (char *)SIFIVE_E_PLIC_HART_CONFIG, ms->smp.cpus, 0,
        SIFIVE_E_PLIC_NUM_SOURCES,
        SIFIVE_E_PLIC_NUM_PRIORITIES,
        SIFIVE_E_PLIC_PRIORITY_BASE,
        SIFIVE_E_PLIC_PENDING_BASE,
        SIFIVE_E_PLIC_ENABLE_BASE,
        SIFIVE_E_PLIC_ENABLE_STRIDE,
        SIFIVE_E_PLIC_CONTEXT_BASE,
        SIFIVE_E_PLIC_CONTEXT_STRIDE,
        memmap[SIFIVE_E_DEV_PLIC].size);
    riscv_aclint_swi_create(memmap[SIFIVE_E_DEV_CLINT].base,
        0, ms->smp.cpus, false);
    riscv_aclint_mtimer_create(memmap[SIFIVE_E_DEV_CLINT].base +
            RISCV_ACLINT_SWI_SIZE,
        RISCV_ACLINT_DEFAULT_MTIMER_SIZE, 0, ms->smp.cpus,
        RISCV_ACLINT_DEFAULT_MTIMECMP, RISCV_ACLINT_DEFAULT_MTIME,
        SIFIVE_E_LFCLK_DEFAULT_FREQ, false);

   
    /* GPIO */

    if (!sysbus_realize(SYS_BUS_DEVICE(&s->gpio), errp)) {
        return;
    }

    /* Map GPIO registers */
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->gpio), 0, memmap[SIFIVE_E_DEV_GPIO0].base);

    /* Pass all GPIOs to the SOC layer so they are available to the board */
    qdev_pass_gpios(DEVICE(&s->gpio), dev, NULL);

    /* Connect GPIO interrupts to the PLIC */
    for (int i = 0; i < 32; i++) {
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->gpio), i,
                           qdev_get_gpio_in(DEVICE(s->plic),
                                            SIFIVE_E_GPIO0_IRQ0 + i));
    }


    sifive_uart_create(sys_mem, memmap[SIFIVE_E_DEV_UART0].base,
        serial_hd(0), qdev_get_gpio_in(DEVICE(s->plic), SIFIVE_E_UART0_IRQ));

    /* Flash memory */
    memory_region_init_rom(&s->xip_mem, OBJECT(dev), "riscv.noel.srg.xip",
                           memmap[SIFIVE_E_DEV_XIP].size, &error_fatal);
    memory_region_add_subregion(sys_mem, memmap[SIFIVE_E_DEV_XIP].base,
        &s->xip_mem);
}

static void noel_srg_soc_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = noel_srg_soc_realize;
    /* Reason: Uses serial_hds in realize function, thus can't be used twice */
    dc->user_creatable = false;
}

static const TypeInfo noel_srg_soc_type_info = {
    .name = TYPE_RISCV_NOEL_SRG_SOC,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(NOELSRGSoCState),
    .instance_init = noel_srg_soc_init,
    .class_init = noel_srg_soc_class_init,
};

static void noel_srg_soc_register_types(void)
{
    type_register_static(&noel_srg_soc_type_info);
}

type_init(noel_srg_soc_register_types)

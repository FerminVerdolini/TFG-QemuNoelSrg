/*
 * SiFive E series machine interface
 *
 * Copyright (c) 2017 SiFive, Inc.
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

#ifndef HW_NOEL_SRG_H
#define HW_NOEL_SRG_H

#include "hw/riscv/riscv_hart.h"
#include "hw/riscv/sifive_cpu.h"
#include "hw/gpio/sifive_gpio.h"
#include "hw/boards.h"

#define TYPE_RISCV_NOEL_SRG_SOC "riscv.noel.srg.soc"
#define RISCV_NOEL_SRG_SOC(obj) \
    OBJECT_CHECK(NOELSRGSoCState, (obj), TYPE_RISCV_NOEL_SRG_SOC)

typedef struct NOELSRGSoCState {
    /*< private >*/
    DeviceState parent_obj;

    /*< public >*/
    RISCVHartArrayState cpus;
    DeviceState *plic;
    SIFIVEGPIOState gpio;
    MemoryRegion xip_mem;
    MemoryRegion mask_rom;
} NOELSRGSoCState;

typedef struct NOELSRGState {
    /*< private >*/
    MachineState parent_obj;

    /*< public >*/
    NOELSRGSoCState soc;
    bool revb;
} NOELSRGState;

#define TYPE_RISCV_NOEL_SRG_MACHINE MACHINE_TYPE_NAME("noel-srg")
#define RISCV_NOEL_SRG_MACHINE(obj) \
    OBJECT_CHECK(NOELSRGState, (obj), TYPE_RISCV_NOEL_SRG_MACHINE)

enum {
    SIFIVE_E_DEV_DEBUG,
    SIFIVE_E_DEV_MROM,
    SIFIVE_E_DEV_CLINT,
    SIFIVE_E_DEV_PLIC,
    SIFIVE_E_DEV_GPIO0,
    SIFIVE_E_DEV_UART0,
    SIFIVE_E_DEV_XIP,
    SIFIVE_E_DEV_DTIM
};

enum {
    SIFIVE_E_UART0_IRQ    = 3,
    SIFIVE_E_GPIO0_IRQ0   = 8
};

#define SIFIVE_E_PLIC_HART_CONFIG "M"
/*
 * Freedom E310 G002 and G003 supports 52 interrupt sources while
 * Freedom E310 G000 supports 51 interrupt sources. We use the value
 * of G002 and G003, so it is 53 (including interrupt source 0).
 */
#define SIFIVE_E_PLIC_NUM_SOURCES 53
#define SIFIVE_E_PLIC_NUM_PRIORITIES 7
#define SIFIVE_E_PLIC_PRIORITY_BASE 0x00
#define SIFIVE_E_PLIC_PENDING_BASE 0x1000
#define SIFIVE_E_PLIC_ENABLE_BASE 0x2000
#define SIFIVE_E_PLIC_ENABLE_STRIDE 0x80
#define SIFIVE_E_PLIC_CONTEXT_BASE 0x200000
#define SIFIVE_E_PLIC_CONTEXT_STRIDE 0x1000

#endif

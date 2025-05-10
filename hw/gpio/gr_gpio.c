/*
 * GR GPIO general purpose input/output register definition
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */

#include <inttypes.h>

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/gpio/gr_gpio.h"
#include "migration/vmstate.h"

static void gr_gpio_IN_set(void *opaque, int line, int value)
{
    GRGPIOState *s = GR_GPIO(opaque);

    printf("GPIO in %d set to %d\n", line, value );  
    
    if ( !( s->dir & (1 << line) ) ) // Si el pin es de entrada
    {
     	// Update s->value
     	if ( value )
    	{
    	   s->value |= (1 << line);
    	}
    	else
    	{
    	   s->value &= ~(1 << line);
    	}
    }   
}

static void gr_gpio_OUT_set(void *opaque, int line, int value)
{
    GRGPIOState *s = GR_GPIO(opaque);

    printf("GPIO out %d set to %d\n", line, value );

    qemu_set_irq( s->gpio_out[line], value );
}

static void gr_gpio_DIR_set(void *opaque, int line, int value)
{
    GRGPIOState *s = GR_GPIO(opaque);

    printf("GPIO dir %d set to %d\n", line, value );

    qemu_set_irq( s->gpio_dir[line], value );
}

static void gr_gpio_reset(DeviceState *dev)
{
    GRGPIOState *s = GR_GPIO(dev);

    s->value = 0;
    s->dir = 0;
}

static uint64_t gr_gpio_read(void *opaque, hwaddr offset, unsigned int size)
{
    GRGPIOState *s = GR_GPIO(opaque);
    uint64_t r = 0;

    switch (offset) {
    case GR_GPIO_REG_IN:
        r = s->value;
        break;

    case GR_GPIO_REG_OUT:
        r = s->value;
        break;

    case GR_GPIO_REG_DIR:
        r = s->dir;
        break;

    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                "%s: bad read offset 0x%" HWADDR_PRIx "\n",
                      __func__, offset);
    }

    printf("GPIO read: %" PRIx64 "\n", r);

    return r;
}

static void gr_gpio_write(void *opaque, hwaddr offset,
                              uint64_t value, unsigned int size)
{
    GRGPIOState *s = GR_GPIO(opaque);
    
    uint32_t oldvalue;

//    trace_gr_gpio_write(offset, value);

printf("GPIO write: %" PRIx64 "\n", value);

    switch (offset) {

    case GR_GPIO_REG_IN:
        break;

    case GR_GPIO_REG_OUT:
        // TODO: write only bits configured as output
        oldvalue = s->value;
        break;

    case GR_GPIO_REG_DIR:
        //s->dir = value;
        oldvalue = s->dir;
        break;

    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: bad write offset 0x%" HWADDR_PRIx "\n",
                      __func__, offset);
    }

    if ( (offset == GR_GPIO_REG_OUT) && (oldvalue != value) )
    {
    	for (int i=0; i<GR_GPIO_PINS;i++)
    	{
           int pin_mask = 1 << i;
    	   if ( s->dir & pin_mask ) // Si el pin es de salida
    	   {
    	      if ( (oldvalue & pin_mask) ^ (value & pin_mask) ) // Si son diferentes
    	      {
    	         gr_gpio_OUT_set( opaque, i, (value & pin_mask) ? 1 : 0 );
    	      }
    	   }
    	}
    	
    	// Update s->value, clean output bits and set new values
    	s->value = (s->value & ~(s->dir)) | (value & s->dir);
    }
    
    if ( (offset == GR_GPIO_REG_DIR) && (oldvalue != value) )
    {
    	for (int i=0; i<GR_GPIO_PINS;i++)
    	{
           int pin_mask = 1 << i;
           if ( (oldvalue & pin_mask) ^ (value & pin_mask) ) // Si son diferentes
    	   {
    	      gr_gpio_DIR_set( opaque, i, (value & pin_mask) ? 1 : 0 );
    	   }
    	}
    	
    	// Update s->dir
    	s->dir = value;
    }
    
}

static const MemoryRegionOps gpio_ops = {
    .read =  gr_gpio_read,
    .write = gr_gpio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl.min_access_size = 4,
    .impl.max_access_size = 4,
};

static const VMStateDescription vmstate_gr_gpio = {
    .name = TYPE_GR_GPIO,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32(value,     GRGPIOState),
        VMSTATE_UINT32(dir,  GRGPIOState),
        VMSTATE_END_OF_LIST()
    }
};

static Property gr_gpio_properties[] = {
    DEFINE_PROP_UINT32("ngpio", GRGPIOState, ngpio, GR_GPIO_PINS),
    DEFINE_PROP_END_OF_LIST(),
};

static void gr_gpio_realize(DeviceState *dev, Error **errp)
{
    GRGPIOState *s = GR_GPIO(dev);

    memory_region_init_io(&s->mmio, OBJECT(dev), &gpio_ops, s,
            TYPE_GR_GPIO, GR_GPIO_SIZE);

    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->mmio);


    for (int i = 0; i < s->ngpio; i++) {
        sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->irq[i]);
    }

    qdev_init_gpio_out_named(DEVICE(s), s->gpio_out, "GPIO", s->ngpio );
    qdev_init_gpio_out_named(DEVICE(s), s->gpio_dir, "DIR", s->ngpio );
//    qdev_init_gpio_out(DEVICE(s), s->gpio_out, s->ngpio);    // irqs lanzadas al SOC cada vez que cambia un pin de salida (->GUI)

    qdev_init_gpio_in(DEVICE(s), gr_gpio_IN_set, s->ngpio);  // irqs recibidas del SOC cada vez que cambia un pin de entrada (GUI->)  
}

static void gr_gpio_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_props(dc, gr_gpio_properties);
    dc->vmsd = &vmstate_gr_gpio;
    dc->realize = gr_gpio_realize;
    dc->reset = gr_gpio_reset;
    dc->desc = "GR GPIO";
}

static const TypeInfo gr_gpio_info = {
    .name = TYPE_GR_GPIO,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(GRGPIOState),
    .class_init = gr_gpio_class_init
};

static void gr_gpio_register_types(void)
{
    type_register_static(&gr_gpio_info);
}

type_init(gr_gpio_register_types)

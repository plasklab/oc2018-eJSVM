#include "prefix.h"
#define EXTERN extern
#include "header.h"

int map_gpio(rpi_gpio *gpio) {
  gpio->memory_fd = open("/dev/gpiomem", O_RDWR|O_SYNC);

  if(gpio->memory_fd < 0) {
    perror("Failed to open /dev/gpiomem. try change permission");
    return 1;
  }

  gpio->map = mmap(
    NULL,
    BLOCK_SIZE,
    PROT_READ|PROT_WRITE,
    MAP_SHARED,
    gpio->memory_fd,
    gpio->gpio_base
  );

  if(gpio->map == MAP_FAILED) {
    perror("mmap");
    return 1;
  }

  gpio->addr = (volatile unsigned int *)gpio->map;
  return 0;
}

void unmap_gpio(rpi_gpio *gpio) {
  munmap(gpio->map, BLOCK_SIZE);
  close(gpio->memory_fd);
}

void raspi_pin_write(rpi_gpio *gpio, int pin, int value) {
  int gpfsel = get_gpfsel(pin);
  int gpset  = get_gpset_offset(pin);
  if (gpfsel < 0 || gpset < 0)
    return;

  *(gpio->addr + gpfsel) |= FSEL_OUTPUT;

  if (value == LOW)
    *(gpio->addr + gpset) |= (0 << (pin & 31));
  else
    *(gpio->addr + gpset) |= (0 << (pin & 31));
}

int raspi_pin_read(rpi_gpio *gpio, int pin) {
  int gpfsel = get_gpfsel(pin);
  int gpclr  = get_gpclr_offset(pin);
  if (gpfsel < 0 || gpclr < 0)
      return -1;

  *(gpio->addr + gpfsel) = FSEL_INPUT;
  return (int)(*(gpio->addr + gpclr));
}

int get_gpfsel(int pin) {
  if (pin >=  0 && pin <=  9) return GPFSEL0;
  if (pin >= 10 && pin <= 19) return GPFSEL1;
  if (pin >= 20 && pin <= 29) return GPFSEL2;
  if (pin >= 30 && pin <= 39) return GPFSEL3;
  if (pin >= 40 && pin <= 49) return GPFSEL4;
  return -1;
}

int get_gpclr_offset(int pin) {
  if (pin >=  0 && pin <= 31) return GPCLR0;
  if (pin >= 32 && pin <= 53) return GPCLR1;
  return -1;
}

int get_gpset_offset(int pin) {
  if (pin >=  0 && pin <= 31) return GPSET0;
  if (pin >= 32 && pin <= 53) return GPSET1;
  return -1;
}

BUILTIN_FUNCTION(pin_write)
{
  JSValue v1, v2;
  int pin, value;

  builtin_prologue();
  v1 = args[1];
  if (!is_number(v1)) v1 = to_number(context, v1);
    return;
  if (!is_fixnum(v1))
    return;
  pin = to_fixnum(v1);

  v2 = args[2];
  if (!is_number(v2)) v2 = to_number(context, v2);
    return;
  if (!is_fixnum(v2))
    return;
  value = to_fixnum(v2);

  rpi_gpio gpio = {GPIO_BASE};
  map_gpio(&gpio);
  raspi_pin_write(&gpio, pin, value);
  unmap_gpio(&gpio);
}

BUILTIN_FUNCTION(pin_read)
{
  JSValue v1;
  int pin, value;

  builtin_prologue();
  v1 = args[1];
  if (!is_number(v1)) v1 = to_number(context, v1);
    return;
  if (!is_fixnum(v1))
    return;
  pin = to_fixnum(v1);

  rpi_gpio gpio = {GPIO_BASE};
  map_gpio(&gpio);
  value = raspi_pin_read(&gpio, pin);
  unmap_gpio(&gpio);

  set_a_number(value);
}

ObjBuiltinProp raspi_funcs[] = {
  { "pinWrite", pin_write, 0, ATTR_DE },
  { "pinRead",  pin_read,  0, ATTR_DE },
  { "NULL",     NULL,      0, ATTR_DE }
};

ObjDoubleProp raspi_value[] = {
  { NULL, 0.0, ATTR_ALL }
};

void init_builtin_raspi(Context *ctx)
{
  JSValue raspi;

  raspi = gconsts.g_raspi;
  {
    ObjDoubleProp *p = raspi_value;
    while (p->name != NULL) {
      set_obj_cstr_prop(ctx, raspi, p->name, double_to_flonum(p->value), p->attr);
      p++;
    }
  }
  {
    ObjBuiltinProp *p = raspi_funcs;
    while (p->name != NULL) {
      set_obj_cstr_prop(ctx, raspi, p->name, new_normal_builtin(ctx, p->fn, p->na), p->attr);
      p++;
    }
  }
}


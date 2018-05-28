#include "prefix.h"
#define EXTERN extern
#include "header.h"

static volatile uint32_t *gpioReg = MAP_FAILED;

int map_gpio()
{
  int fd, i;

  if (gpioReg != MAP_FAILED)
      return 0;

  fd = open("/dev/gpiomem", O_RDWR|O_SYNC);
  if(fd < 0) {
    perror("Failed to open /dev/gpiomem. try change permission");
    return -1;
  }

  gpioReg = (uint32_t *)mmap(
    NULL,
    0xB4,
    PROT_READ|PROT_WRITE,
    MAP_SHARED,
    fd,
    GPIO_BASE
  );
  close(fd);

  if(gpioReg == MAP_FAILED) {
    perror("mmap");
    return -1;
  }

  for (i = 0; i < 54; i++) {
    printf("gpio=%d mode=%d level=%d\n", i, gpio_get_mode(i), gpio_read(i));
  }

  return 0;
}

void gpio_set_mode(int gpio, int mode)
{
  int reg, shift;

  reg = gpio / 10;
  shift = (gpio % 10) * 3;

  gpioReg[reg] = (gpioReg[reg] & ~(7 << shift)) | (mode << shift);
}

int gpio_get_mode(int gpio)
{
  int reg, shift;

  reg = gpio / 10;
  shift = (gpio % 10) * 3;

  return (*(gpioReg + reg) >> shift) & 7;
}

int gpio_read(int gpio)
{
  if ((*(gpioReg + GPLEV0 + PI_OFFSET) & PI_BIT) != 0) return 1;
  else                                                 return 0;
}

void gpio_write(int gpio, int level)
{
  if (level == 0) *(gpioReg + GPCLR0 + PI_OFFSET) = PI_BIT;
  else            *(gpioReg + GPSET0 + PI_OFFSET) = PI_BIT;
}

BUILTIN_FUNCTION(raspi_init)
{
  map_gpio();
}

BUILTIN_FUNCTION(raspi_gpio_write)
{
  JSValue v1, v2;
  int gpio, value;

  builtin_prologue();
  v1 = args[1];
  if (!is_number(v1)) v1 = to_number(context, v1);
  if (!is_fixnum(v1))
    return;
  gpio = (int)fixnum_to_int(v1);

  v2 = args[2];
  if (!is_number(v2)) v2 = to_number(context, v2);
  if (!is_fixnum(v2))
    return;
  value = (int)fixnum_to_int(v2);

  gpio_set_mode(gpio, FSEL_OUTPUT);
  gpio_write(gpio, value);
}

BUILTIN_FUNCTION(raspi_gpio_read)
{
  JSValue v;
  int gpio, value;

  builtin_prologue();
  v = args[1];
  if (!is_number(v)) v = to_number(context, v);
  if (!is_fixnum(v))
    return;
  gpio = (int)fixnum_to_int(v);

  gpio_set_mode(gpio, FSEL_INPUT);
  value = gpio_read(gpio);
  set_a(context, int_to_fixnum(value));
}

ObjBuiltinProp raspi_funcs[] = {
  { "init",      raspi_init,       0, ATTR_DE },
  { "gpioWrite", raspi_gpio_write, 2, ATTR_DE },
  { "gpioRead",  raspi_gpio_read,  2, ATTR_DE },
  { NULL,        NULL,             0, ATTR_DE }
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


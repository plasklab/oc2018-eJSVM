#include "prefix.h"
#define EXTERN extern
#include "header.h"

static volatile uint32_t *gpioReg  = MAP_FAILED;
static volatile uint32_t *clockReg = MAP_FAILED;
static volatile uint32_t *pwmReg   = MAP_FAILED;

int map_gpio()
{
  int fd;

  if (gpioReg != MAP_FAILED)
    return 0;

  fd = open("/dev/mem", O_RDWR|O_SYNC);
  if(fd < 0) {
    perror("Failed to open /dev/mem.");
    return -1;
  }

  gpioReg  = (uint32_t *)mmap(NULL, GPIO_BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, GPIO_BASE);
  clockReg = (uint32_t *)mmap(NULL,  CLK_BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, CLK_BASE);
  pwmReg   = (uint32_t *)mmap(NULL,  PWM_BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, PWM_BASE);

  close(fd);

  if(gpioReg == MAP_FAILED || clockReg == MAP_FAILED || pwmReg == MAP_FAILED) {
    perror("mmap error");
    return -1;
  }

  return 0;
}

void set_clock()
{
  *(clockReg + CM_PWMCTL) = CLK_PASSWD + (0x1 << 5);
  while (*(clockReg + CM_PWMCTL) & 0x80);
  *(clockReg + CM_PWMDIV) = CLK_PASSWD + (192 << 12); // 100kHz
  *(clockReg + CM_PWMCTL) = CLK_PASSWD + 0x1 + (0x1 << 4);
}

void set_pwm()
{
  *pwmReg &= (int) 0x0; // PWM disabled
  usleep(10);

  *pwmReg = (0x1 << 7) + (0x1 << 15); // PWM M/S Enable
  *(pwmReg + PWM_RNG1) = 250;         // 400Hz
  *(pwmReg + PWM_DAT1) = (250 >> 1);  // 50%
  *(pwmReg + PWM_RNG2) = 250;         // 400Hz
  *(pwmReg + PWM_DAT2) = (250 >> 1);  // 50%

  *pwmReg = 0x1 + (0x1 << 8); // PWM enabled
}

void set_gpio_mode(int gpio, int mode)
{
  int reg, shift;

  reg = gpio / 10;
  shift = (gpio % 10) * 3;

  gpioReg[reg] = (gpioReg[reg] & ~(7 << shift)) | (mode << shift);
}

int get_gpio_mode(int gpio)
{
  int reg, shift;

  reg = gpio / 10;
  shift = (gpio % 10) * 3;

  return (*(gpioReg + reg) >> shift) & 7;
}

int gpio_read(int gpio)
{
  if ((*(gpioReg + GPLEV0 + PI_BANK(gpio)) & PI_BIT(gpio)) != 0) return 1;
  else                                                           return 0;
}

void gpio_write(int gpio, int level)
{
  if (level == 0) *(gpioReg + GPCLR0 + PI_BANK(gpio)) = PI_BIT(gpio);
  else            *(gpioReg + GPSET0 + PI_BANK(gpio)) = PI_BIT(gpio);
}

int adc_read(int channel)
{
  int value, i;
  if (channel < 0 || channel >= 4)
    return -1;
  gpio_write(8, HEIGH);
  gpio_write(11, LOW);
  gpio_write(8,  LOW);

  channel |= 0x18;
  channel <<= 3;
  for (i = 0; i < 5; i++) {
    if (channel & 0x80) {
      gpio_write(10, HEIGH);
    } else {
      gpio_write(10, LOW);
    }
    channel <<= 1;
    gpio_write(11, HEIGH);
    gpio_write(11, LOW);
  }

  value = 0;
  for (i = 0; i < 13; i++) {
    gpio_write(11, HEIGH);
    gpio_write(11, LOW);
    value <<= 1;
    if (i > 0 && gpio_read(9) == HEIGH)
      value |= 0x1;
  }

  gpio_write(8, HEIGH);

  return value;
}

BUILTIN_FUNCTION(raspi_init)
{
  map_gpio();
  set_clock();
  set_pwm();
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

  set_gpio_mode(gpio, FSEL_OUTPUT);
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

  set_gpio_mode(gpio, FSEL_INPUT);
  value = gpio_read(gpio);
  set_a(context, int_to_fixnum(value));
}

BUILTIN_FUNCTION(raspi_analog_read)
{
  JSValue v;
  int channel, value;

  builtin_prologue();
  v = args[1];
  if (!is_number(v)) v = to_number(context, v);
  if (!is_fixnum(v))
    return;
  channel = (int)fixnum_to_int(v);
  value = adc_read(channel);
  set_a(context, int_to_fixnum(value));
}

ObjBuiltinProp raspi_funcs[] = {
  { "init",       raspi_init,        0, ATTR_DE },
  { "gpioWrite",  raspi_gpio_write,  2, ATTR_DE },
  { "gpioRead",   raspi_gpio_read,   1, ATTR_DE },
  { "analogRead", raspi_analog_read, 1, ATTR_DE },
  { NULL,         NULL,              0, ATTR_DE }
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


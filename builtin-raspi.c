#include "prefix.h"
#define EXTERN extern
#include "header.h"

static volatile uint32_t *gpio = MAP_FAILED;
static volatile uint32_t *clk  = MAP_FAILED;
static volatile uint32_t *pwm  = MAP_FAILED;

int map_gpio()
{
  int fd;

  if (gpio != MAP_FAILED)
    return 0;

  fd = open("/dev/mem", O_RDWR|O_SYNC);
  if(fd < 0) {
    perror("Failed to open /dev/mem. Try running with sudo?\n");
    return -1;
  }

  gpio = (uint32_t *)mmap(NULL, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, GPIO_BASE);
  clk  = (uint32_t *)mmap(NULL, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, CLK_BASE);
  pwm  = (uint32_t *)mmap(NULL, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, PWM_BASE);

  close(fd);

  if(gpio == MAP_FAILED || clk == MAP_FAILED || pwm == MAP_FAILED) {
    perror("mmap error\n");
    return -1;
  }

  return 0;
}

void set_clk()
{
  // preserve PWM_CONTROL
  uint32_t pwm_control = *pwm;

  *pwm = 0;                                 // Stop PWM
  *(clk + CM_PWMCTL) = (CLK_PASSWD | 0x01); // Stop PWM Clock
  usleep(110);

  // Wait for clk to be !BUSY
  while (*(clk + CM_PWMCTL) & 0x80)
    usleep(1);

  *(clk + CM_PWMDIV) = (CLK_PASSWD | (32 << 12)); // PWM Set a 600kHz
  *(clk + CM_PWMCTL) = (CLK_PASSWD | 0x11);       // Start PWM Clock
  *pwm = pwm_control;                             // restore PWM_CONTROL
}

void set_pwm()
{
  // PWM disabled
  *pwm &= (int) 0x0;
  usleep(10);

  // Set Range 1024 and Duty Cycle 50%
  *(pwm + PWM_RNG1) = 1024;
  *(pwm + PWM_RNG2) = 1024;
  *(pwm + PWM_DAT1) = 512;
  *(pwm + PWM_DAT2) = 512;

  // PWM M/S mode and PWM Enable
  *pwm = PWM1_ENABLE | PWM2_ENABLE | PWM1_MS_MODE | PWM2_MS_MODE;
}

void set_pulse_motor()
{
  int power_pin;
  int clk_r_pin, clk_l_pin;
  int rotation_r_pin, rotation_l_pin;

  power_pin = 5;
  clk_l_pin = 12;
  clk_r_pin = 13;
  rotation_l_pin = 16;
  rotation_r_pin = 6;

  set_gpio_mode(power_pin, FSEL_OUTPUT);
  set_gpio_mode(clk_l_pin, FSEL_ALT0);
  set_gpio_mode(clk_r_pin, FSEL_ALT0);
  set_gpio_mode(rotation_l_pin, FSEL_OUTPUT);
  set_gpio_mode(rotation_r_pin, FSEL_OUTPUT);

  gpio_write(rotation_l_pin, LOW);
  gpio_write(rotation_r_pin, HEIGH);
  gpio_write(power_pin, LOW);
}

void set_gpio_mode(int gpio_pin, int mode)
{
  int reg, shift;

  reg = gpio_pin / 10;
  shift = (gpio_pin % 10) * 3;

  gpio[reg] = (gpio[reg] & ~(7 << shift)) | (mode << shift);
}

int get_gpio_mode(int gpio_pin)
{
  int reg, shift;

  reg = gpio_pin / 10;
  shift = (gpio_pin % 10) * 3;

  return (*(gpio + reg) >> shift) & 7;
}

int gpio_read(int gpio_pin)
{
  if ((*(gpio + GPLEV0 + PI_BANK(gpio_pin)) & PI_BIT(gpio_pin)) != 0) return 1;
  else                                                                return 0;
}

void gpio_write(int gpio_pin, int level)
{
  if (level == 0) *(gpio + GPCLR0 + PI_BANK(gpio_pin)) = PI_BIT(gpio_pin);
  else            *(gpio + GPSET0 + PI_BANK(gpio_pin)) = PI_BIT(gpio_pin);
}

int adc_read(int adcnum, int clkpin, int mosipin, int misopin, int cspin)
{
  int commandout, adcout, i;
  if (adcnum > 4 || adcnum < 0)
    return -1;
  gpio_write(cspin, HEIGH);
  gpio_write(clkpin, LOW);
  usleep(1);
  gpio_write(cspin, LOW);

  commandout = adcnum;
  commandout |= 0x18;
  commandout <<= 3;
  for (i = 0; i < 5; i++) {
    if (commandout & 0x80) {
      gpio_write(mosipin, HEIGH);
    } else {
      gpio_write(mosipin, LOW);
    }
    commandout <<= 1;
    gpio_write(clkpin, HEIGH);
    usleep(1);
    gpio_write(clkpin, LOW);
    usleep(1);
  }

  adcout = 0;
  for (i = 0; i < 13; i++) {
    gpio_write(clkpin, HEIGH);
    usleep(1);
    gpio_write(clkpin, LOW);
    usleep(1);
    adcout <<= 1;
    if (i > 0 && gpio_read(misopin) == HEIGH)
      adcout |= 0x1;
  }

  gpio_write(cspin, HEIGH);

  return adcout;
}

BUILTIN_FUNCTION(raspi_init)
{
  map_gpio();
  set_pulse_motor();
  set_pwm();
  set_clk();
}

BUILTIN_FUNCTION(raspi_gpio_write)
{
  JSValue v1, v2;
  int gpio_pin, value;

  builtin_prologue();
  v1 = args[1];
  if (!is_number(v1)) v1 = to_number(context, v1);
  if (!is_fixnum(v1))
    return;
  gpio_pin = (int)fixnum_to_int(v1);

  v2 = args[2];
  if (!is_number(v2)) v2 = to_number(context, v2);
  if (!is_fixnum(v2))
    return;
  value = (int)fixnum_to_int(v2);

  set_gpio_mode(gpio_pin, FSEL_OUTPUT);
  gpio_write(gpio_pin, value);
}

BUILTIN_FUNCTION(raspi_gpio_read)
{
  JSValue v;
  int gpio_pin, value;

  builtin_prologue();
  v = args[1];
  if (!is_number(v)) v = to_number(context, v);
  if (!is_fixnum(v))
    return;
  gpio_pin = (int)fixnum_to_int(v);

  set_gpio_mode(gpio_pin, FSEL_INPUT);
  value = gpio_read(gpio_pin);
  set_a(context, int_to_fixnum(value));
}

BUILTIN_FUNCTION(raspi_analog_read)
{
  JSValue v;
  int channel, value;
  int spi_clk, spi_mosi, spi_miso, spi_cs;
  spi_clk = 11;
  spi_mosi = 10;
  spi_miso = 9;
  spi_cs = 8;
  set_gpio_mode(spi_clk,  FSEL_OUTPUT);
  set_gpio_mode(spi_mosi, FSEL_OUTPUT);
  set_gpio_mode(spi_miso, FSEL_INPUT);
  set_gpio_mode(spi_cs,   FSEL_OUTPUT);

  builtin_prologue();
  v = args[1];
  if (!is_number(v)) v = to_number(context, v);
  if (!is_fixnum(v))
    return;
  channel = (int)fixnum_to_int(v);
  value = adc_read(channel, spi_clk, spi_mosi, spi_miso, spi_cs);
  set_a(context, int_to_fixnum(value));
}

BUILTIN_FUNCTION(raspi_front_moter)
{
  int power_pin;
  int rotation_r_pin, rotation_l_pin;

  power_pin = 5;
  rotation_l_pin = 16;
  rotation_r_pin = 6;

  gpio_write(rotation_l_pin, LOW);
  gpio_write(rotation_r_pin, HEIGH);
  gpio_write(power_pin, LOW);
}

BUILTIN_FUNCTION(raspi_right_moter)
{
  int power_pin;
  int rotation_r_pin, rotation_l_pin;

  power_pin = 5;
  rotation_l_pin = 16;
  rotation_r_pin = 6;

  gpio_write(rotation_l_pin, LOW);
  gpio_write(rotation_r_pin, LOW);
  gpio_write(power_pin, LOW);
}

BUILTIN_FUNCTION(raspi_left_moter)
{
  int power_pin;
  int rotation_r_pin, rotation_l_pin;

  power_pin = 5;
  rotation_l_pin = 16;
  rotation_r_pin = 6;

  gpio_write(rotation_l_pin, HEIGH);
  gpio_write(rotation_r_pin, HEIGH);
  gpio_write(power_pin, LOW);
}

BUILTIN_FUNCTION(raspi_stop_moter)
{
  int power_pin;
  int rotation_r_pin, rotation_l_pin;

  power_pin = 5;
  rotation_l_pin = 16;
  rotation_r_pin = 6;

  gpio_write(rotation_l_pin, LOW);
  gpio_write(rotation_r_pin, HEIGH);
  gpio_write(power_pin, LOW);
}

ObjBuiltinProp raspi_funcs[] = {
  { "init",       raspi_init,        0, ATTR_DE },
  { "gpioWrite",  raspi_gpio_write,  2, ATTR_DE },
  { "gpioRead",   raspi_gpio_read,   1, ATTR_DE },
  { "analogRead", raspi_analog_read, 1, ATTR_DE },
  { "frontMoter", raspi_front_moter, 0, ATTR_DE },
  { "rightMoter", raspi_right_moter, 0, ATTR_DE },
  { "leftMoter",  raspi_left_moter,  0, ATTR_DE },
  { "stopMoter",  raspi_stop_moter,  0, ATTR_DE },
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


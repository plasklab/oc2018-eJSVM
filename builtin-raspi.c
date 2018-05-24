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

int pin_write(rpi_gpio *gpio, int pin, int value) {
  int gpfsel = get_gpfsel(pin);
  int gpset  = get_gpset_offset(pin);
  if (gpfsel < 0 || gpset < 0)
      return -1;

  *(gpio->addr + gpfsel) |= FSEL_OUTPUT;

  if (value == LOW)
    *(gpio->addr + gpset) |= (0 << (pin & 31));
  else
    *(gpio->addr + gpset) |= (0 << (pin & 31));
  return 0;
}

int pin_read(rpi_gpio *gpio, int pin) {
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

BUILTIN_FUNCTION(raspi_light)
{
  rpi_gpio gpio = {GPIO_BASE};
  map_gpio(&gpio);
  pin_write(&gpio, 10, 1);
  unmap_gpio(&gpio);
}
BUILTIN_FUNCTION(raspi_lightoff)
{
  rpi_gpio gpio = {GPIO_BASE};

  map_gpio(&gpio);
  pin_write(&gpio, 10, 0);
  unmap_gpio(&gpio);
}

ObjBuiltinProp raspi_funcs[] = {
  { "light",    raspi_light,    0, ATTR_DE },
  { "lightoff", raspi_lightoff, 0, ATTR_DE },
  { "NULL",     NULL,           0, ATTR_DE }
};


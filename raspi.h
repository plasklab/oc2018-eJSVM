#define BLOCK_SIZE  4 * 1024
#define PERIPHERAL_BASE 0x3f000000
#define GPIO_BASE PERIPHERAL_BASE + 0x00200000
#define TIMER_BASE  PERIPHERAL_BASE + 0x00003000

#define GPFSEL0 0x00
#define GPFSEL1 0x04
#define GPFSEL2 0x08
#define GPFSEL3 0x0C
#define GPFSEL4 0x10
#define GPFSEL5 0x14

#define GPSET0 0x01C
#define GPSET1 0x020

#define GPCLR0 0x028
#define GPCLR1 0x02C

#define GPLEV0 0x034
#define GPLEV1 0x038

#define FSEL_INPUT  0b000
#define FSEL_OUTPUT 0b001
#define FSEL_ALT0   0b100
#define FSEL_ALT1   0b101
#define FSEL_ALT2   0b110
#define FSEL_ALT3   0b111
#define FSEL_ALT4   0b011
#define FSEL_ALT5   0b010

#define PI_OFFSET (gpio >> 5)
#define PI_BIT    (1 << (gpio & 0x1F))

#define LOW  0
#define HIGH 1

static volatile uint32_t *gpioReg = MAP_FAILED;

int map_gpio();
void gpio_set_mode(int gpio, int mode);
int gpio_get_mode(int gpio);
int gpio_read(int gpio);
void gpio_write(int gpio, int level);


#define PERIPHERAL_BASE 0x3f000000
#define GPIO_BASE PERIPHERAL_BASE + 0x00200000
#define TIMER_BASE  PERIPHERAL_BASE + 0x00003000

#define GPSET0 0x07
#define GPSET1 0x08

#define GPCLR0 0x0A
#define GPCLR1 0x0B

#define GPLEV0 0x0D
#define GPLEV1 0x0E

#define FSEL_INPUT  0b000
#define FSEL_OUTPUT 0b001
#define FSEL_ALT0   0b100
#define FSEL_ALT1   0b101
#define FSEL_ALT2   0b110
#define FSEL_ALT3   0b111
#define FSEL_ALT4   0b011
#define FSEL_ALT5   0b010

#define PI_BANK(gpio) ((gpio) >> 5)
#define PI_BIT(gpio)  (1 << ((gpio) & 0x1F))

#define BLOCK_SIZE 0xB4

int map_gpio();
void gpio_set_mode(int gpio, int mode);
int gpio_get_mode(int gpio);
int gpio_read(int gpio);
void gpio_write(int gpio, int level);


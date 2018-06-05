#define PERIPHERAL_BASE 0x3f000000
#define GPIO_BASE  (PERIPHERAL_BASE + 0x200000)
#define PWM_BASE   (PERIPHERAL_BASE + 0x20C000)
#define CLK_BASE   (PERIPHERAL_BASE + 0x101000)
#define TIMER_BASE (PERIPHERAL_BASE + 0x003000)

#define GPIO_BLOCK_SIZE 0xB4
#define PWM_BLOCK_SIZE  0x28
#define CLK_BLOCK_SIZE  0xA8

#define CLK_PASSWD 0x5A000000

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

#define CM_PWMCTL 0x28
#define CM_PWMDIV 0x29

#define PWM_RNG1 0x4
#define PWM_DAT1 0x5
#define PWM_RNG2 0x7
#define PWM_DAT2 0x8

#define HEIGH 1
#define LOW   0

#define PI_BANK(gpio) ((gpio) >> 5)
#define PI_BIT(gpio)  (1 << ((gpio) & 0x1F))

int map_gpio();
void set_clock();
void set_pwm();
void set_gpio_mode(int gpio, int mode);
int get_gpio_mode(int gpio);
int gpio_read(int gpio);
int adc_read(int channel);
void gpio_write(int gpio, int level);


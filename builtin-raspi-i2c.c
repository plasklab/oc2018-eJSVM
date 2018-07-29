#include "prefix.h"
#define EXTERN extern
#include "header.h"


#define HIGH 1
#define LOW 0

#define MODE_WRITE 0
#define MODE_READ 1

#define set_mode_i2c_pin(pin, mode) (set_gpio_mode((pin), ((mode)==MODE_READ)?FSEL_INPUT:FSEL_OUTPUT))
#define set_i2c_pin(pin, v) (gpio_write((pin), (v)))
#define get_i2c_pin(pin) (gpio_read((pin)))

#define I2C_WRITE 0
#define I2C_READ  1

#define SDA_STR "sda"
#define SCL_STR "scl"

#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#define delay(t) usleep((t)*1000)
#define udelay(t) usleep(t)

int wait_time = 10; // us


int get_i2c_pin_numbers(Context *ctx, JSValue i2cobj, int *sda, int *scl) {
  JSValue v_sda, v_scl;
  if (get_prop(i2cobj, cstr_to_string(ctx, SDA_STR), &v_sda) != SUCCESS) return FAIL;
  if (get_prop(i2cobj, cstr_to_string(ctx, SCL_STR), &v_scl) != SUCCESS) return FAIL;
  if (!is_fixnum(v_sda)) return FAIL;
  if (!is_fixnum(v_scl)) return FAIL;
  *sda = fixnum_to_cint(v_sda);
  *scl = fixnum_to_cint(v_scl);
  return SUCCESS;
}

int send1byte(int sda_pin, int scl_pin, uint8_t data) {
  set_mode_i2c_pin(sda_pin, MODE_WRITE);
  set_mode_i2c_pin(scl_pin, MODE_WRITE);

  const uint8_t MASK = 0x80;
  int i;
  for (i = 0; i < 8; i++) {
    int bit = ((data & MASK) != 0);
    set_i2c_pin(scl_pin, LOW); udelay(wait_time);
    if (bit) {
      set_i2c_pin(sda_pin, HIGH);
    } else {
      set_i2c_pin(sda_pin, LOW);
    }
    udelay(wait_time);
    set_i2c_pin(scl_pin, HIGH); udelay(wait_time);
    data = data << 1;
  }

  // check ack bit
  set_i2c_pin(scl_pin, LOW);
  set_i2c_pin(sda_pin, HIGH);
  udelay(wait_time);
  set_i2c_pin(scl_pin, HIGH);
  udelay(wait_time);
  set_mode_i2c_pin(sda_pin, MODE_READ);
  uint8_t ackbit = get_i2c_pin(sda_pin);
  set_i2c_pin(scl_pin, LOW);
  if (ackbit != 0) {
    return FAIL;
  }
  return SUCCESS;
}

int send_address(int sda_pin, int scl_pin, uint32_t addr, int read_or_write) {
  uint8_t rwbit = 0;
  if (read_or_write == I2C_WRITE) {
    rwbit = 0x00;
  } else {
    rwbit = 0x01;
  }
  return send1byte(sda_pin, scl_pin, (addr << 1) | rwbit);
}

void i2c_start(int sda_pin, int scl_pin) {
  set_mode_i2c_pin(sda_pin, MODE_WRITE);
  set_mode_i2c_pin(scl_pin, MODE_WRITE);
  set_i2c_pin(scl_pin, HIGH);udelay(wait_time);
  set_i2c_pin(sda_pin, LOW);udelay(wait_time);
}
void i2c_stop(int sda_pin, int scl_pin) {
  set_mode_i2c_pin(sda_pin, MODE_WRITE);
  set_mode_i2c_pin(scl_pin, MODE_WRITE);
  set_i2c_pin(scl_pin, HIGH);udelay(wait_time);
  set_i2c_pin(sda_pin, HIGH);udelay(wait_time);
}


BUILTIN_FUNCTION(raspi_i2c_constr) {
  JSValue rsv;
  builtin_prologue();

  JSValue is_master_node = JS_UNDEFINED;
  if (na >= 1) {
    if (args[1] == FIXNUM_ZERO) {
      is_master_node = JS_TRUE;
    } else if (args[1] == FIXNUM_ONE) {
      is_master_node = JS_FALSE;
    } else {
      simple_print(args[1]); putchar('\n');
      LOG_EXIT("the first argument should be RaspiI2C.MASTER_NODE or RaspiI2C.SLAVE_NODE.\n");
    }
  }

  JSValue sda_pin = JS_UNDEFINED;
  JSValue scl_pin = JS_UNDEFINED;
  if (na >= 3) {
    if (is_fixnum(args[2]) && is_fixnum(args[3])) {
      sda_pin = args[2];
      scl_pin = args[3];
    } else {
      simple_print(args[2]); putchar('\n');
      simple_print(args[3]); putchar('\n');
      LOG_EXIT("the second and third arguments should be fixnumbers as SDA and SCL pin.\n");
    }
  }

  rsv = new_big_predef_object_without_prototype(context);
  set___proto___all(context, rsv, gconsts.g_raspi_i2c_proto);

  if (is_master_node != JS_UNDEFINED)
    set_obj_cstr_prop(context, rsv, "isMasterNode", is_master_node, ATTR_NONE);
  if (sda_pin != JS_UNDEFINED)
    set_obj_cstr_prop(context, rsv, "sda", sda_pin, ATTR_NONE);
  if (scl_pin != JS_UNDEFINED)
    set_obj_cstr_prop(context, rsv, "scl", scl_pin, ATTR_NONE);

  set_a(context, rsv);
}



BUILTIN_FUNCTION(builtin_raspi_i2c_write) {
  builtin_prologue();
  uint8_t addr = (uint8_t) (fixnum_to_cint(args[1]) & 0xff);
  uint8_t data8bit = (uint8_t) (fixnum_to_cint(args[2]) & 0xff);

  int sda_pin, scl_pin;
  if (get_i2c_pin_numbers(context, args[0], &sda_pin, &scl_pin) != SUCCESS) {
    LOG_EXIT("RaspiI2C.sda or RaspiI2C.sda is not valid value.\n");
  }

  i2c_start(sda_pin, scl_pin);
  if (send_address(sda_pin, scl_pin, addr, I2C_WRITE) != SUCCESS) {
    set_a(context, JS_FALSE); // not succeed
    return;
  }
  udelay(wait_time);
  if (send1byte(sda_pin, scl_pin, data8bit) != SUCCESS)  {
    set_a(context, JS_FALSE); // not succeed
    return;
  }
  i2c_stop(sda_pin, scl_pin);
  set_a(context, JS_TRUE); // succeed
}

ObjBuiltinProp raspi_i2c_funcs[] = {
  { "write",       builtin_raspi_i2c_write,       2, ATTR_DE },
  { NULL, NULL, 0, ATTR_DE }
};

void init_builtin_raspi_i2c(Context *ctx) {
  JSValue proto;

  gconsts.g_raspi_i2c =
    new_normal_builtin_with_constr(ctx, raspi_i2c_constr, raspi_i2c_constr, 3);
  gconsts.g_raspi_i2c_proto = proto = new_big_predef_object(ctx);
  set_prototype_all(ctx, gconsts.g_raspi_i2c, proto);
  {
    ObjBuiltinProp *p = raspi_i2c_funcs;
    while (p->name != NULL) {
      set_obj_cstr_prop(ctx, proto, p->name, new_normal_builtin(ctx, p->fn, p->na), p->attr);
      p++;
    }
  }
  set_obj_cstr_prop(ctx, gconsts.g_raspi_i2c, "MASTER_NODE", FIXNUM_ZERO, ATTR_ALL);
  set_obj_cstr_prop(ctx, gconsts.g_raspi_i2c, "SLAVE_NODE", FIXNUM_ONE, ATTR_ALL);
}


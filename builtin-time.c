
#include "prefix.h"
#define EXTERN extern
#include "header.h"

#include <unistd.h>

unsigned int to_uint(Context *ctx, JSValue v) {
  if (!is_number(v)) v = to_number(ctx, v);
  if (is_nan(v)) {
    return 0;
  }
  unsigned int t;
  if (is_fixnum(v)) {
    t = (unsigned int) fixnum_to_cint(v);
  } else {
    t = (unsigned int) flonum_to_double(v);
  }
  return t;
}

BUILTIN_FUNCTION(builtin_time_delay)
{
  builtin_prologue();
  JSValue v = args[1];
  usleep(to_uint(context, v)*1000);
  set_a(context, JS_UNDEFINED);
}

BUILTIN_FUNCTION(builtin_time_udelay)
{
  builtin_prologue();
  JSValue v = args[1];
  usleep(to_uint(context, v));
  set_a(context, JS_UNDEFINED);
}

ObjBuiltinProp time_funcs[] = {
  { "delay",   builtin_time_delay,  1, ATTR_DE },
  { "udelay",  builtin_time_udelay, 1, ATTR_DE },
  { NULL,                    NULL,  0, ATTR_DE }
};

void init_builtin_time(Context *ctx) {
  JSValue time = gconsts.g_time;
  {
    ObjBuiltinProp *p = time_funcs;
    while (p->name != NULL) {
      set_obj_cstr_prop(ctx, time, p->name,
          new_normal_builtin(ctx, p->fn, p->na), p->attr);
      p++;
    }
  }
}

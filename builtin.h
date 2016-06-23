#define BUILTIN_FUNCTION(x) void x(Context* context, int na)
#define BUILTIN_FUNCTION_STATIC(x) static BUILTIN_FUNCTION(x)

#define get_args()  ((JSValue *)(&(get_stack(context, fp))))

#define builtin_prologue() \
  int fp; JSValue *args; fp = get_fp(context); args = get_args()

typedef struct obj_builtin_prop {
  char *name;
  builtin_function_t fn;
  int na;
  Attribute attr;
} ObjBuiltinProp;

typedef struct obj_double_prop {
  char *name;
  double value;
  Attribute attr;
} ObjDoubleProp;

typedef struct obj_gobj_prop {
  char *name;
  JSValue *addr;
  Attribute attr;
} ObjGobjProp;

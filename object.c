/*
   object.c

   SSJS Project at the University of Electro-communications

   Sho Takada, 2012-13
   Akira Tanimura, 2012-13
   Akihiro Urushihara, 2013-14
   Ryota Fujii, 2013-14
   Tomoharu Ugawa, 2013-16
   Hideya Iwasaki, 2013-16
*/

/*
   There are two functions and a macro that are used in allocating an
   object of a specified type, namely allocate_xxx, make_xxx, and new_xxx.
   For example, in allocating a function, allocate_function, make_function,
   and new_function are used.  Each of them has the following role.

   allocate_xxx : (function)
     This allocates a memory for an xxx and sets an appropriate object tag
     in its header.

   make_xxx : (macro)
     This only calls allocate_xxx, puts pointer tag (T_OBJECT), and
     returns a JSValue data.

   new_xxx : (function)
     This first calls make_xxx and sets various values within the returned
     object.  set_object_members is called in this function.
 */

#include "prefix.h"
#define EXTERN
#include "header.h"
#include "gc.h"

#define PROP_REALLOC_THRESHOLD (0.75)

#define prop_overflow(o) \
  (obj_n_props(o) > (obj_limit_props(o) * PROP_REALLOC_THRESHOLD))

#define sign(x) ((x) > 0? 1: -1)

/*
   obtains the index of a property of an Object
   If the specified property does not exist, returns -1.
 */
int prop_index(JSValue obj, JSValue name)
{
  HashData retv;
  int result;

  if (!is_object(obj)) return -1;
  result = hash_get(obj_map(obj), name, &retv);
  if (result == HASH_GET_FAILED) return -1;
  else return (int)retv;
}

/*
   obtains the property value of the key ``name'', stores it to *ret
   and returns SUCCESS or FAIL
   This function does not follow the prototype chain.
 */
int get_prop(JSValue obj, JSValue name, JSValue *ret)
{
  int index;

  //printf("get_prop: obj = %016lx, prop = %s\n", obj, string_to_cstr(name));
  index = prop_index(obj, name);
  if (index == -1) return FAIL;

  *ret = obj_prop_index(obj, index);
  return SUCCESS;
}

/*
  obtains the property from an object by following the prototype chain
  (if necessary)
   o: object
   p: property, which is a string
 */
JSValue get_prop_prototype_chain(JSValue o, JSValue p) {
  JSValue ret;
  extern JSValue prototype_object(JSValue);

  // printf("get_prop_prototype_chain: o = "); simple_print(o); printf("\n");
  // printf("get_prop_prototype_chain: p = "); simple_print(p); printf("\n");
  // printf("get_prop_prototype_chain: prop = %s, obj = %016lx\n", string_to_cstr(p), o);
  // printf("Object.__proto__ = %016lx\n", gconsts.g_object_proto);
  do {
    if (get_prop(o, p, &ret) == SUCCESS) return ret;
  } while (get_prop(o, gconsts.g_string___proto__, &o) == SUCCESS);
  // is it necessary to search in the Object's prototype?
  return JS_UNDEFINED;
}

/*
   obtains object's property
     o: object (but not an array)
     p: property (number / string / other type)
   It is not necessary to check the type of `o'.
 */
JSValue get_object_prop(Context *context, JSValue o, JSValue p) {
  /*
    if (p is not a string) p = to_string(p);
      returns the value regsitered under the property p
    }
  */
  // printf("get_object_prop, o = %016lx, p = %016lx\n", o, p);
  if (!is_string(p)) p = to_string(context, p);
  return get_prop_prototype_chain(o, p);
}

/*
   obtains array's property
     a: array
     p: property (number / string / other type)
   It is not necessary to check the type of `a'.
 */
JSValue get_array_prop(Context *context, JSValue a, JSValue p) {
  /*
    if (p a number) {
      if (p is within the range of an subscript of an array)
        returns the p-th element of a
      else {
        p = number_to_string(idx);
        returns the value regsitered under the property p
      }
    } else {
      if (p is not a string) p = to_string(p);
      s = string_to_number(p);
      if (s is within the range of an subscript of an array)
        returns the s-th element of a
      else
        returns the value regsitered under the property p
    }
  */

  switch (get_tag(p)) {
  case T_FIXNUM:
    {
      cint n;
      n = fixnum_to_cint(p);
      // printf("get_array_prop: n = %ld, array_length(a) = %d\n", n, array_length(a));
      if (0 <= n) {
      // printf("array_body_index(a,n) = "); print_value_simple(context, array_body_index(a,n)); printf("\n");
        return (n < array_length(a))? array_body_index(a, n): JS_UNDEFINED;
      }
      p = fixnum_to_string(p);
      return get_prop_prototype_chain(a, p);
    }
    break;
  default:
   p = to_string(context, p);
   // fall through
  case T_STRING:
    {
      JSValue num;
      cint n;
      num = string_to_number(p);
      if (is_fixnum(num)) {
        n = fixnum_to_cint(num);
        if (0 <= n) {
          return (n < array_length(a))? array_body_index(a, n): JS_UNDEFINED;
        }
      }
      return get_prop_prototype_chain(a, p);
    }
    break;
  }
}          

/*
   sets an object's property value with its attribute
 */
int set_prop_with_attribute(JSValue obj, JSValue name, JSValue v, Attribute attr) {
  uint64_t retv;

  if (hash_get(obj_map(obj), name, (HashData *)(&retv)) == HASH_GET_FAILED) {
    // The specified property is not registered in the hash table.
    if (prop_overflow(obj)) {
      printf("obj_n_props(obj) = %d, obj_limit_props(obj) = %d\n",
             obj_n_props(obj), obj_limit_props(obj));
      LOG_EXIT("proptable overflow\n");
      return FAIL;
    }
    retv = (obj_n_props(obj))++;
    // retv = ++(obj_n_props(obj));
    /*
    printf("obj = %lx, obj_n_props(obj) = %d, name = %s, value = ",
            obj, obj_n_props(obj), string_to_cstr(name));
    simple_print(v); printf("\n");
    */

    if (hash_put_with_attribute(obj_map(obj), name, retv, attr)
          == HASH_PUT_SUCCESS) {
      obj_prop_index(obj, (int)retv) = v;
      return SUCCESS;
    } else
      return FAIL;
  } else {
    // returned value is HASH_GET_SUCCESS
    // There is already the property `name', overwrites its value.
    obj_prop_index(obj, (int)retv) = v;
    return SUCCESS;
  }
}

/*
   sets object's property
     o: object (but not an array)
     p: property (number / string / other type)
     v: value to be set
   It is not necessary to check the type of `o'.
 */
int set_object_prop(Context *context, JSValue o, JSValue p, JSValue v) {
  if (!is_string(p)) p = to_string(context, p);
  return set_prop_none(o, p, v);
}

/*
   sets array's property
     a: array
     p: property (number / string / other type)
     v: value to be set
   It is not necessary to check the type of `a'.
 */
int set_array_prop(Context *context, JSValue a, JSValue p, JSValue v) {
  switch (get_tag(p)) {
  case T_FIXNUM:
    {
      cint n, len;
      int i;

      n = fixnum_to_cint(p);
      len = array_length(a);
      // printf("set_array_prop: n = %d\n", n);
      if (0 <= n) {
        if (n < len) {
          array_body_index(a, n) = v;
          return SUCCESS;
        } else if (n < array_size(a)) {
          for (i = len + 1; i < n; i++)
            array_body_index(a, i) = JS_UNDEFINED;
          array_body_index(a, n) = v;
          array_length(a) = n + 1;
          set_prop_none(a, gconsts.g_string_length, cint_to_fixnum(n));
        } else {
          // expand the array --- not implemented yet
          printf("set_array_prop: expansion of an array is not implemented yet\n");
          return SUCCESS;
        }
      }
      p = fixnum_to_string(p);
      return set_object_prop(context, a, p, v);
    }
    break;
  default:
   p = to_string(context, p);
   // fall through
  case T_STRING:
    {
      JSValue num;
      cint n, len;
      int i;

      num = string_to_number(p);
      if (is_fixnum(num)) {
        n = fixnum_to_cint(num);
        len = array_length(a);
        if (0 <= n) {
          if (n < len) {
            array_body_index(a, n) = v;
            return SUCCESS;
          } else if (n < array_size(a)) {
            for (i = len + 1; i < n; i++)
              array_body_index(a, i) = JS_UNDEFINED;
            array_body_index(a, n) = v;
            array_length(a) = n + 1;
            set_prop_none(a, gconsts.g_string_length, cint_to_fixnum(n));
          } else {
            printf("set_array_prop: expansion of an array is not implemented yet\n");
            return SUCCESS;
          }
        }
      }
      // if the property name is "length", expand / shrink the array
      // -- not implemented
      return set_object_prop(context, a, p, v);
    }
    break;
  }
}

#if 0
static inline void setArrayBody(JSValue array, int size)
{
  if(size < MINIMUM_ARRAY_SIZE)
    size = MINIMUM_ARRAY_SIZE;
  ((ArrayCell*)array)->body = allocateArrayData(size);
  setArraySize(array, size);
}
#endif

/*
   obtains the next key of the property
 */
int next_propname(JSValue obj, HashIterator *iter, HashKey *key)
{
  HashEntry e;
  int r;

  while ((r = __hashNext(obj_map(obj), iter, &e)) != FAIL &&
         (e.attr & ATTR_DE));
  if (r == FAIL) return FAIL;
  *key = e.key;
  return SUCCESS;
}

#if 0
// ------------------------------------------------------------------
/**
 * @brief イテレータに数値を追加
 * @param 対象のイテレータ
 * @param インデックス
 */

void putIndexToIterator(JSValue iter, uint64_t index)
{
  // 文字列に変換して用いる
  char str[1000];
  snprintf(str, 1000 - 1, "%llu", (long long unsigned int)index);
  putCStrToIterator(iter, str);
}
#endif

/*
   obtains the next property name in an iterator
 */
int get_next_propname(JSValue iter, JSValue *name) {
  if (hash_next(iterator_object_map(iter), &(iterator_iter(iter)), name) == SUCCESS) {
    // printf("in get_next_propname 0: name = %016lx: ", *name); simple_print(*name); printf("\n");
    *name = iterator_object_prop_index(iter, (int)(*name));
    // printf("in get_next_propname 1: name = %016lx: ", *name); simple_print(*name); printf("\n");
    return SUCCESS;
  }
  *name = JS_UNDEFINED;
  return FAIL;
}

#if 0
// ------------------------------------------------------------------
/**
 * @brief 配列の要素を格納する
 * @param 対象の配列
 * @param インデックス
 * @param 結果を格納する変数
 * @return 成功値
 */

void setArrayValue(JSValue obj, int index, JSValue src)
{
  int length, size, i;
  JSValue *oldbody;

  size = (int)getArraySize(obj);
  length = (int)getArrayLength(obj);

  // 現在の長さを超えたインデックスに書き込む場合
  if (index + 1 > length) {

    // 配列のサイズが足りないので拡張する
    if (index + 1 > size) {
      oldbody = getArrayBody(obj);
      setArrayBody(obj, size * 2);
      memcpy(getArrayBody(obj), oldbody, sizeof(JSValue) * size);
    }

    setArrayLength(obj, index + 1);

    // プロパティに格納するので、Fixnumの範囲かを確認
    // Fixnum の範囲なら Fixnum に変換して
    if (isInFixnumRange(index + 1)) {
      setObjPropNone(obj, "length", intToFixnum(index + 1));
    } else {
      setObjPropNone(obj, "length", doubleToFlonum((double)index + 1));
    }

    // Undefined で埋める
    for (i = length; i < index + 1; i++) {
      getArrayBody(obj)[i] = JS_UNDEFINED;
    }
  }
  getArrayBody(obj)[index] = src;
}
#endif

#ifdef USE_REGEXP
/*
   sets a regexp's members and makes an Oniguruma's regexp object
 */
int set_regexp_members(JSValue re, char *pat, int flag) {
  OnigOptionType opt;
  OnigErrorInfo err;
  char *e;

  /*
     The original code in SSJSVM_codeloader.c used ststrdup function,
     which is defined in hash.c.  But I don't know the reason why
     ststrdup is used instead of the standard strdup function.

     regexp_pattern(re) = ststrdup(str);
  */
  regexp_pattern(re) = strdup(pat);

  opt = ONIG_OPTION_NONE;

  if (flag & F_REGEXP_GLOBAL) {
    regexp_global(re) = true;
    set_obj_cstr_prop(re, "global", JS_TRUE, ATTR_ALL);
  } else
    set_obj_cstr_prop(re, "global", JS_FALSE, ATTR_ALL);

  if (flag & F_REGEXP_IGNORE) {
    opt |= ONIG_OPTION_IGNORECASE;
    regexp_ignorecase(re) =  true;
    set_obj_cstr_prop(re, "ignoreCase", JS_TRUE, ATTR_ALL);
  } else
    set_obj_cstr_prop(re, "ignoreCase", JS_FALSE, ATTR_ALL);

  if (flag & F_REGEXP_MULTILINE) {
    opt |= ONIG_OPTION_MULTILINE;
    regexp_multiline(re) = true;
    set_obj_cstr_prop(re, "multiline", JS_TRUE, ATTR_ALL);
  } else
    set_obj_cstr_prop(re, "multiline", JS_FALSE, ATTR_ALL);

  e = pat + strlen(pat);
  if (onig_new(&(regexp_reg(re)), (OnigUChar *)pat, (OnigUChar *)e, opt,
               ONIG_ENCODING_ASCII, ONIG_SYNTAX_DEFAULT, &err) == ONIG_NORMAL)
    return SUCCESS;
  else
    return FAIL;
}

/*
   returns a flag value from a ragexp objext
 */
int regexp_flag(JSValue re) {
  int flag;

  flag = 0;
  if (regexp_global(re)) flag |= F_REGEXP_GLOBAL;
  if (regexp_ignorecase(re)) flag |= F_REGEXP_IGNORE;
  if (regexp_multiline(re)) flag |= F_REGEXP_MULTILINE;
  return flag;
}
#endif

/*
   sets object's member values.
 */
void set_object_members(Object *p) {
  Map *a;

  a = malloc_hashtable();
  hash_create(a, INITIAL_HASH_SIZE);
  p->map = a;
  p->prop = allocate_prop_table(INITIAL_PROPTABLE_SIZE);
  p->n_props = 0;
  p->limit_props = INITIAL_PROPTABLE_SIZE;
}

/*
  makes an object whose __proto__ property is not set yet
 */
JSValue new_object_without_prototype(Context *ctx) {
  JSValue ret;
  Object *p;

  ret = make_object(ctx);
  p = remove_object_tag(ret);
  set_object_members(p);
  return ret;
}
  
/*
  makes a new object
 */
JSValue new_object(Context *ctx)
{
  JSValue ret;
  Object *p;
#ifdef PARALLEL
  pthread_mutexattr_t attr;
#endif

  ret = make_object(ctx);
  p = remove_object_tag(ret);
  set_object_members(p);
  set_prop_all(ret, gconsts.g_string___proto__, gconsts.g_object_proto);
#ifdef PARALLEL
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&(p->mutex), &attr);
  pthread_mutexattr_destroy(&attr);
#endif
  return ret;
}

/*
   makes a new array
 */
JSValue new_array(Context *ctx) {
  JSValue ret;

  ret = make_array(ctx);
  disable_gc();  // disable GC unitl Array is properly initialised
  set_object_members(array_object_p(ret));
  set_prop_all(ret, gconsts.g_string___proto__, gconsts.g_array_proto);
  allocate_array_data_critical(ret, 0, 0);
  set_prop_none(ret, gconsts.g_string_length, FIXNUM_ZERO);
  enable_gc();
  return ret;
}

/*
   makes a new array with size
 */
JSValue new_array_with_size(Context *ctx, int size)
{
  JSValue ret;

  ret = make_array(ctx);
  disable_gc();  // disable GC unitl Array is properly initialised
  set_object_members(array_object_p(ret));
  allocate_array_data_critical(ret, size, size);
  set_prop_none(ret, gconsts.g_string_length, int_to_fixnum(size));
  enable_gc();
  return ret;
}

/*
   makes a function
   The name of this function was formerly new_closure.
 */
JSValue new_function(Context *context, Subscript subscr)
{
  JSValue ret;

  ret = make_function();
  disable_gc();
  set_object_members(func_object_p(ret));
  func_table_entry(ret) = &(context->function_table[subscr]);
  func_environment(ret) = get_lp(context);
  enable_gc();
  gc_push_tmp_root(&ret);
  set_prop_none(ret, gconsts.g_string_prototype, new_object(context));
  set_prop_none(ret, gconsts.g_string___proto__, gconsts.g_function_proto);
  gc_pop_tmp_root(1);
  return ret;
}

/*
   makes a new built-in function object with constructor
 */
JSValue new_builtin_with_constr(builtin_function_t f, builtin_function_t cons, int na) {
  JSValue ret;

  ret = make_builtin();
  set_object_members(builtin_object_p(ret));
  builtin_body(ret) = f;
  builtin_constructor(ret) = cons;
  builtin_n_args(ret) = na;
  // we do not have a proper context during initialisation
  set_prop_none(ret, gconsts.g_string_prototype, new_object(NULL));
  // TODO: g_object_proto should be g_builtin_proto
  set_prop_none(ret, gconsts.g_string___proto__, gconsts.g_object_proto);
  return ret;
}

/*
   makes a new built-in function object
 */
JSValue new_builtin(builtin_function_t f, int na) {
  return new_builtin_with_constr(f, builtin_not_a_constructor, na);
}

/*
   makes an iterator object
 */
JSValue new_iterator(JSValue obj) {
  JSValue ret;
  HashIterator *hi;
  HashKey key;

  ret = make_iterator();
  set_object_members(iterator_object_p(ret));
  hi = &(iterator_iter(ret));
  do {
    init_hash_iterator(obj_map(obj), hi);
    while (next_propname(obj, hi, &key)) {
      // printf("In new_iterator: key = "); simple_print(key); printf("\n");
      set_prop_none(ret, key, key);
    }
    /*
    if (is_array(obj)) {
      int len, i;
#define N 100;
      char ind[N];
      len = array_length(obj);
      for (i = 0; i < len; i++) {
        snprintf(&ind[0], N - 1, "%d", i);
      }
#undef N
    }
    */
  } while (get_prop(obj, gconsts.g_string___proto__, &obj) == SUCCESS);
  init_hash_iterator(iterator_object_p(ret)->map, hi);
  // print_object_properties(ret);
  return ret;
}

#ifdef USE_REGEXP
/*
   makes a new regexp
 */
JSValue new_regexp(char *pat, int flag) {
  JSValue ret;

  ret = make_regexp();
  set_object_members(regexp_object_p(ret));
  // pattern field is set in set_regexp_members
  // regexp_pattern(ret) = NULL;
  regexp_reg(ret) = NULL;
  regexp_global(ret) = false;
  regexp_ignorecase(ret) = false;
  regexp_multiline(ret) = false;
  regexp_lastindex(ret) = 0;
  set_prop_none(ret, gconsts.g_string___proto__, gconsts.g_regexp_proto);
  return (set_regexp_members(ret, pat, flag) == SUCCESS)? ret: JS_UNDEFINED;
}
#endif // USE_REGEXP

/*
   makes a new boxed number
 */
JSValue new_number(JSValue v) {
  JSValue ret;

  ret = make_number_object();
  set_object_members(boxed_object_p(ret));
  number_object_value(ret) = v;
  set_prop_none(ret, gconsts.g_string___proto__, gconsts.g_number_proto);
  return ret;
}

/*
   makes a new boxed boolean
 */
JSValue new_boolean(JSValue v) {
  JSValue ret;

  ret = make_boolean_object();
  set_object_members(boxed_object_p(ret));
  boolean_object_value(ret) = v;
  set_prop_none(ret, gconsts.g_string___proto__, gconsts.g_boolean_proto);
  return (JSValue)ret;
}

/*
   makes a new boxed string
 */
JSValue new_string(JSValue v) {
  JSValue ret;

  ret = make_string_object();
  set_object_members(boxed_object_p(ret));
  string_object_value(ret) = v;

  // A boxed string has a property ``length'' whose associated value
  // is the length of the string.
  set_prop_all(ret, gconsts.g_string_length, int_to_fixnum(strlen(string_to_cstr(v))));
  set_prop_none(ret, gconsts.g_string___proto__, gconsts.g_string_proto);
  return ret;
}

// data conversion functions
//
char *space_chomp(char *str) {
  while (isspace(*str)) str++;
  return str;
}

double cstr_to_double(char* cstr) {
  char* endPtr;
  double ret;
  ret = strtod(cstr, &endPtr);
  while( isspace(*endPtr)) endPtr++;
  if (*endPtr == '\0') return ret;
  else return NAN;
}

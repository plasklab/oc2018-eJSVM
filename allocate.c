/*
   allocate.c

   eJS Project
     Kochi University of Technology
     the University of Electro-communications

     Tomoharu Ugawa, 2016-17
     Hideya Iwasaki, 2016-17

   The eJS Project is the successor of the SSJS Project at the University of
   Electro-communications, which was contributed by the following members.

     Sho Takada, 2012-13
     Akira Tanimura, 2012-13
     Akihiro Urushihara, 2013-14
     Ryota Fujii, 2013-14
     Tomoharu Ugawa, 2012-14
     Hideya Iwasaki, 2012-14
*/

#include "prefix.h"
#define EXTERN extern
#include "header.h"

/*
   a counter for debugging
 */
#ifdef DEBUG
int count = 0;
#endif // DEBUG

#ifdef need_flonum
/*
   allocates a flonum
   Note that the return value does not have a pointer tag.
 */
FlonumCell *allocate_flonum(double d)
{
  FlonumCell *n =
    (FlonumCell *) gc_jsalloc_critical(sizeof(FlonumCell), HTAG_FLONUM);
  n->value = d;
  return n;
}
#endif /* need_flonum */

/*
   allocates a string
   It takes only the string length (excluding the last null character).
   Note that the return value does not have a pointer tag.
 */
StringCell *allocate_string(uint32_t length)
{
  /* plus 1 for the null terminater,
   * minus BYTES_IN_JSVALUE becasue StringCell has payload of
   * BYTES_IN_JSVALUE for placeholder */
  uintptr_t size = sizeof(StringCell) + (length + 1 - BYTES_IN_JSVALUE);
  StringCell *v =
    (StringCell *) gc_jsalloc_critical(size, HTAG_STRING);
#ifdef STROBJ_HAS_HASH
  v->length = length;
#endif
  return v;
}

/*
   allocates a simple object
   Note that the return value does not have a pointer tag.
 */
Object *allocate_simple_object(Context *ctx)
{
  Object *object = (Object *) gc_jsalloc(ctx, sizeof(Object), HTAG_SIMPLE_OBJECT);
  return object;
}

/*
   allocates an array
   Note that the return value does not have a pointer tag.
 */
ArrayCell *allocate_array(Context *ctx) {
  ArrayCell *array =
    (ArrayCell *) gc_jsalloc(ctx, sizeof(ArrayCell), HTAG_ARRAY);
  return array;
}

/*
   allocates an array body
     size : number of elements in the body (size >= len)
     len  : length of the array, i.e., subscripts that are less than len
            are acrutally used
 */
void allocate_array_data(Context *ctx, JSValue a, int size, int len)
{
  JSValue *body;
  int i;

  gc_push_tmp_root(&a);
  body = (JSValue *) gc_malloc(ctx, sizeof(JSValue) * size,
			       HTAG_ARRAY_DATA);
  for (i = 0; i < len; i++) body[i] = JS_UNDEFINED; 
  array_body(a) = body;
  array_size(a) = size;
  array_length(a) = len;
  gc_pop_tmp_root(1);
}

/*
   re-allocates an array body
     newsize : new size of the array body
 */
void reallocate_array_data(Context *ctx, JSValue a, int newsize)
{
  JSValue *body, *oldbody;
  int i;

  gc_push_tmp_root(&a);
  body = (JSValue *) gc_malloc(ctx, sizeof(JSValue) * newsize,
			       HTAG_ARRAY_DATA);
  oldbody = array_body(a);
  for (i = 0; i < array_length(a); i++) body[i] = oldbody[i];
  array_body(a) = body;
  array_size(a) = newsize;
  gc_pop_tmp_root(1);
}

/*
   allocates a function
 */
FunctionCell *allocate_function(void) {
  FunctionCell *function =
    (FunctionCell *) gc_jsalloc_critical(sizeof(FunctionCell), HTAG_FUNCTION);
  return function;
}

/*
   allocates a builtin
 */
BuiltinCell *allocate_builtin(void) {
  BuiltinCell *builtin =
    (BuiltinCell *) gc_jsalloc_critical(sizeof(BuiltinCell), HTAG_BUILTIN);
  return builtin;
}

JSValue *allocate_prop_table(int size) {
  JSValue *table = (JSValue*) gc_malloc_critical(sizeof(JSValue) * size,
						 HTAG_PROP);
  size_t i;
  for (i = 0; i < size; i++)  // initialise for GC
    table[i] = JS_UNDEFINED;
  return table;
}

JSValue *reallocate_prop_table(Context *ctx, JSValue *oldtab, int oldsize, int newsize) {
  JSValue *table;

  gc_push_tmp_root(oldtab);
  table  = (JSValue *) gc_malloc(ctx, sizeof(JSValue) * newsize, HTAG_PROP);
  gc_pop_tmp_root(1);
  size_t i;
  for (i = 0; i < oldsize; i++)
    table[i] = oldtab[i];
  for (; i < newsize; i++)
    table[i] = JS_UNDEFINED;
  return table;
}

/*
   allocates an iterator
 */
IteratorCell *allocate_iterator(void) {
  IteratorCell *iter =
    (IteratorCell *) gc_jsalloc_critical(sizeof(IteratorCell), HTAG_ITERATOR);
  return iter;
}

/*
   allocates a regexp
 */
#ifdef USE_REGEXP
#ifdef need_normal_regexp
RegexpCell *allocate_regexp(void)
{
  RegexpCell *regexp =
    (RegexpCell *) gc_jsalloc_critical(sizeof(RegexpCell), HTAG_REGEXP);
  return regexp;
}
#endif /* need_normal_regexp */
#endif

/*
   allocates a boxed object
 */
BoxedCell *allocate_boxed(Context *ctx, uint32_t type)
{
  BoxedCell *box = (BoxedCell *) gc_jsalloc(ctx, sizeof(BoxedCell), type);
  return box;
}

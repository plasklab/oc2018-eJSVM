#include <stdlib.h>
#include <stdio.h>
#include "prefix.h"
#define EXTERN extern
#include "header.h"

/*
 * defined in header.h
 */
//typedef uint64_t JSValue;
#define LOG_BYTES_IN_JSVALUE   3
//#define BYTES_IN_JSVALUE       (1 << LOG_BYTES_IN_JSVALUE)

/*
 * naming convention
 *   name for size: add a surfix representing the unit
 *                    bytes: in bytes
 *                    jsvalues: in the numberof JSValue's
 */

#define JS_SPACE_BYTES     (1024 * 1024)
#define MALLOC_SPACE_BYTES (1024 * 1024)
#define JS_SPACE_GC_THREASHOLD     (JS_SPACE_BYTES >> 4)
#define MALLOC_SPACE_GC_THREASHOLD (MALLOC_SPACE_BYTES >> 4)

/*
 * If the remaining room is smaller than a certain size,
 * we do not use the remainder for efficiency.  Rather,
 * we add it below the chunk being allocated.  In this case,
 * the size in the header includes the extra words.
 */
#define MINIMUM_FREE_CHUNK_JSVALUES 4

#define MKMASK(l, o, b) \
  ((((uint64_t) -1) << ((l) - (b))) >> ((l) - (o) - (b)))

/*
 * Chunk header layout
 *  HEADER0
 *    bit 0 - 7  :  type (HTAG_xxx)
 *                    4 - 14 : JSValue types
 *                    15     : malloced memory (HTAG_MALLOC)
 *                    16     : free (HTAG_FREE)
 *    bit 8      : mark bit
 *    bit 9 - 11 : extra jsvalues
 *    bit 32 - 63: size (in number of JSValue's)
 */
#define HEADER_JSVALUES       1
#define HEADER_BYTES (HEADER_JSVALUES << LOG_BYTES_IN_JSVALUE)

#define HEADER0_BITS          64
#define HEADER0_TYPE_OFFSET   0
#define HEADER0_TYPE_BITS     8
#define HEADER0_EXTRA_OFFSET  9
#define HEADER0_EXTRA_BITS    3
#define HEADER0_GC_OFFSET     8
#define HEADER0_GC_BITS       1
#define HEADER0_SIZE_OFFSET   32
#define HEADER0_SIZE_BITS     32
#define HEADER0_TYPE_MASK \
  MKMASK(HEADER0_BITS, HEADER0_TYPE_OFFSET, HEADER0_TYPE_BITS)
#define HEADER0_EXTRA_MASK \
  MKMASK(HEADER0_BITS, HEADER0_EXTRA_OFFSET, HEADER0_EXTRA_BITS)
#define HEADER0_GC_MASK \
  MKMASK(HEADER0_BITS, HEADER0_GC_OFFSET, HEADER0_GC_BITS)
#define HEADER0_SIZE_MASK					\
  MKMASK(HEADER0_BITS, HEADER0_SIZE_OFFSET, HEADER0_SIZE_BITS)
//#define HEADER0_SIZE_MASK 0xffffffff00000000LLU

/* accessor to HEADER0 */
#define HEADER0_SET(hdr, val, off, msk)			\
  ((hdr) = ((((uint64_t) (val)) << (off)) | ((hdr) & ~(msk))))
#define HEADER0_GET(hdr, off, msk) \
  (((uint64_t) ((hdr) & (msk))) >> (off))
#define HEADER0_SET_TYPE(hdr, val) \
  HEADER0_SET(hdr, val, HEADER0_TYPE_OFFSET, HEADER0_TYPE_MASK)
#define HEADER0_GET_TYPE(hdr) \
  HEADER0_GET(hdr, HEADER0_TYPE_OFFSET, HEADER0_TYPE_MASK)
#define HEADER0_SET_EXTRA(hdr, val) \
  HEADER0_SET(hdr, val, HEADER0_EXTRA_OFFSET, HEADER0_EXTRA_MASK)
#define HEADER0_GET_EXTRA(hdr) \
  HEADER0_GET(hdr, HEADER0_EXTRA_OFFSET, HEADER0_EXTRA_MASK)
#define HEADER0_SET_GC(hdr, val) \
  HEADER0_SET(hdr, val, HEADER0_GC_OFFSET, HEADER0_GC_MASK)
#define HEADER0_GET_GC(hdr) \
  HEADER0_GET(hdr, HEADER0_GC_OFFSET, HEADER0_GC_MASK)
#define HEADER0_SET_SIZE(hdr, val) \
  HEADER0_SET(hdr, val, HEADER0_SIZE_OFFSET, HEADER0_SIZE_MASK)
#define HEADER0_GET_SIZE(hdr) \
  HEADER0_GET(hdr, HEADER0_SIZE_OFFSET, HEADER0_SIZE_MASK)
#define HEADER0_COMPOSE(size, extra, type) \
  ((((uint64_t) (size)) << HEADER0_SIZE_OFFSET) | \
   ((uint64_t) (extra)) << HEADER0_EXTRA_OFFSET |	\
   ((uint64_t) (type)) << HEADER0_TYPE_OFFSET)

/* 
 * header tag
 */
#define HTAG_MALLOC        (0x0f)
#define HTAG_FREE          (0x10)

struct free_chunk {
  uint64_t header;
  struct free_chunk *next;
};

/*
 *  Space
 */

struct space {
  uintptr_t addr;
  uintptr_t bytes;
  uintptr_t free_bytes;
  struct free_chunk* freelist;
  char *name;
};

/*
 * GC
 */
#define GC_MARK_BIT (1 << HEADER0_GC_OFFSET)


/*
 * prototype
 */
static void create_space(struct space *space, uintptr_t bytes, char*);
static void* do_malloc(uintptr_t request_bytes);
static JSValue* do_jsalloc(uintptr_t request_bytes, uint32_t type);
static int check_gc_request(Context *);

static void garbage_collect(Context *ctx);
static void trace_HashCell_array(HashCell ***ptrp, uint32_t length);
static void trace_HashCell(HashCell **ptrp);
static void trace_JSValue_array(JSValue **, uint32_t);
static void trace_slot(JSValue* ptr);
static void scan_roots(Context *ctx);
static void scan_stack(JSValue* stack, int sp, int fp);
static void sweep(void);
static void print_memory_status(void);

/*
 * variables
 */

static struct space js_space;
static struct space debug_js_shadow;
static struct space malloc_space;
static struct space debug_malloc_shadow;
static int gc_disabled = 1;

static void create_space(struct space *space, uintptr_t bytes, char *name)
{
  struct free_chunk *p;
  p = (struct free_chunk *) malloc(bytes);
  p->header = HEADER0_COMPOSE(bytes >> LOG_BYTES_IN_JSVALUE, 0, HTAG_FREE);
  p->next = NULL;
  space->addr = (uintptr_t) p;
  space->bytes = bytes;
  space->free_bytes = bytes;
  space->freelist = p;
  space->name = name;
}

void init_memory()
{
  create_space(&js_space, JS_SPACE_BYTES, "js_space");
  create_space(&malloc_space, MALLOC_SPACE_BYTES, "malloc_space");
  create_space(&debug_js_shadow, JS_SPACE_BYTES, "debug_js_shadow");
  create_space(&debug_malloc_shadow, MALLOC_SPACE_BYTES, "debug_malloc_shadow");
  gc_disabled = 0;
}

/*
 * Returns a pointer to the first address of the memory area
 * available to the VM.  The header precedes the area.
 * The header has the size of the chunk including the header,
 * the area available to the VM, and extra bytes if any.
 * Other header bits are zero
 */
static void* do_malloc(uintptr_t request_bytes)
{
  uint32_t  alloc_jsvalues;
  struct free_chunk **p;
  
  alloc_jsvalues =
    (request_bytes + BYTES_IN_JSVALUE - 1) >> LOG_BYTES_IN_JSVALUE;
  alloc_jsvalues += HEADER_JSVALUES;

  /* allocate from freelist */
  for (p = &malloc_space.freelist; *p != NULL; p = &(*p)->next) {
    struct free_chunk *chunk = *p;
    uint32_t chunk_jsvalues = HEADER0_GET_SIZE(chunk->header);
    if (chunk_jsvalues >= alloc_jsvalues) {
      if (chunk_jsvalues >= alloc_jsvalues + MINIMUM_FREE_CHUNK_JSVALUES) {
	/* This chunk is large enough to leave a part unused.  Split it */
	uint32_t new_chunk_jsvalues = chunk_jsvalues - alloc_jsvalues;
	uintptr_t addr =
	  ((uintptr_t) chunk) + (new_chunk_jsvalues << LOG_BYTES_IN_JSVALUE);
	HEADER0_SET_SIZE(chunk->header, new_chunk_jsvalues);
	*(uint64_t *) addr = HEADER0_COMPOSE(alloc_jsvalues, 0, 0);
	malloc_space.free_bytes -= alloc_jsvalues << LOG_BYTES_IN_JSVALUE;
	return (void *) (addr + HEADER_BYTES);
      } else {
	/* This chunk is too small to split. */
	*p = (*p)->next;
	chunk->header =
	  HEADER0_COMPOSE(chunk_jsvalues, chunk_jsvalues - alloc_jsvalues, 0);
	malloc_space.free_bytes -= chunk_jsvalues << LOG_BYTES_IN_JSVALUE;
	return (void *) (((uintptr_t) chunk) + HEADER_BYTES);
      }
    }
  }

  return NULL;
}

/*
 * request_jsvalues: the number of JSValue's including the object header.
 */
static JSValue* do_jsalloc(uintptr_t request_bytes, uint32_t type)
{
  struct free_chunk **p;
  uint32_t alloc_jsvalues;

  alloc_jsvalues =
    (request_bytes + BYTES_IN_JSVALUE - 1) >> LOG_BYTES_IN_JSVALUE;

  for (p = &js_space.freelist; *p != NULL; p = &(*p)->next) {
    struct free_chunk *chunk = *p;
    uint32_t chunk_jsvalues = HEADER0_GET_SIZE(chunk->header);
    if (chunk_jsvalues >= alloc_jsvalues) {
      if (chunk_jsvalues >= alloc_jsvalues + MINIMUM_FREE_CHUNK_JSVALUES) {
	/* This chunk is large enough to leave a part unused.  Split it */
	uint32_t new_chunk_jsvalues = chunk_jsvalues - alloc_jsvalues;
	uintptr_t addr =
	  ((uintptr_t) chunk) + (new_chunk_jsvalues << LOG_BYTES_IN_JSVALUE);
	HEADER0_SET_SIZE(chunk->header, new_chunk_jsvalues);
	*(uint64_t *) addr = HEADER0_COMPOSE(alloc_jsvalues, 0, type);
	js_space.free_bytes -= alloc_jsvalues << LOG_BYTES_IN_JSVALUE;
	return (JSValue *) addr;
      } else {
	/* This chunk is too small to split. */
	*p = (*p)->next;
	chunk->header = HEADER0_COMPOSE(chunk_jsvalues,
					chunk_jsvalues - alloc_jsvalues, type);
	js_space.free_bytes -= chunk_jsvalues << LOG_BYTES_IN_JSVALUE;
	return (JSValue *) chunk;
      }
    }
  }

  LOG_EXIT("run out of memory for JS object");
  return NULL;
}

static int check_gc_request(Context *ctx)
{
  printf("check_gc_request\n");
  if (ctx == NULL) {
    if (js_space.free_bytes < JS_SPACE_GC_THREASHOLD)
      printf("Needed gc for js_space -- cancelled: ctx == NULL\n");
    if (malloc_space.free_bytes < JS_SPACE_GC_THREASHOLD)
      printf("Needed gc for malloc_space -- cancelled: ctx == NULL\n");
    return 0;
  }
  if (gc_disabled) {
    if (js_space.free_bytes < JS_SPACE_GC_THREASHOLD)
      printf("Needed gc for js_space -- cancelled: GC disabled\n");
    if (malloc_space.free_bytes < JS_SPACE_GC_THREASHOLD)
      printf("Needed gc for malloc_space -- cancelled: GC disabled\n");
    return 0;
  }
  if (js_space.free_bytes < JS_SPACE_GC_THREASHOLD)
    return 1;
  if (malloc_space.free_bytes < MALLOC_SPACE_GC_THREASHOLD)
    return 1;
  printf("no GC needed js %d malloc %d\n",
	 js_space.free_bytes, malloc_space.free_bytes);
  return 0;
}

void* gc_malloc(Context *ctx, uintptr_t request_bytes)
{
  void * addr;
  if (check_gc_request(ctx))
    garbage_collect(ctx);
  addr = do_malloc(request_bytes);
  printf("gc_malloc: req %x bytes => %p (%x)\n",
	 request_bytes, addr, addr - malloc_space.addr);
  {
    uintptr_t a = (uintptr_t) addr;
    uintptr_t off = a - malloc_space.addr - HEADER_BYTES;
    uint64_t *shadow = (uint64_t *) (debug_malloc_shadow.addr + off);
    *shadow = 0x1;
  }
  return addr;
}

JSValue* gc_jsalloc(Context *ctx, uintptr_t request_bytes, uint32_t type)
{
  JSValue *addr;
  if (check_gc_request(ctx))
    garbage_collect(ctx);
  addr = do_jsalloc(request_bytes, type);
  {
    uintptr_t a = (uintptr_t) addr;
    uintptr_t off = a - js_space.addr;
    uint64_t *shadow = (uint64_t *) (debug_js_shadow.addr + off);
    *shadow = 0x1;
  }
  printf("gc_jsalloc: req %x bytes type %d => %p\n", request_bytes, type, addr);
  return addr;
}

void disable_gc(void)
{
  gc_disabled = 1;
}

void enable_gc(void)
{
  gc_disabled = 0;
}

static int in_js_space(void *addr_)
{
  uintptr_t addr = (uintptr_t) addr_;
  return (js_space.addr <= addr && addr <= js_space.addr + js_space.bytes);
}

static int in_malloc_space(void *addr_)
{
  uintptr_t addr = (uintptr_t) addr_;
  return (malloc_space.addr <= addr &&
	  addr <= malloc_space.addr + malloc_space.bytes);
}

static void garbage_collect(Context *ctx)
{
  printf("GC: Not Implemented\n");
  print_memory_status();

  scan_roots(ctx);
  sweep();
}

static uint64_t *get_shadow(void *ptr)
{
  if (in_js_space(ptr)) {
    uintptr_t a = (uintptr_t) ptr;
    uintptr_t off = a - js_space.addr;
    return (uint64_t *) (debug_js_shadow.addr + off);
  } else if (in_malloc_space(ptr)) {
    uintptr_t a = (uintptr_t) ptr;
    uintptr_t off = a - malloc_space.addr;
    return (uint64_t *) (debug_malloc_shadow.addr + off);
  } else
    return NULL;
}

static void mark_object(Object *obj)
{
  {
    if (in_js_space(obj)) {
      uintptr_t a = (uintptr_t) obj;
      uintptr_t off = a - js_space.addr;
      uint64_t *shadow = (uint64_t *) (debug_js_shadow.addr + off);
      assert((*shadow & 1) == 1);
    } else if (in_malloc_space(obj)) {
      uintptr_t a = (uintptr_t) obj;
      uintptr_t off = a - malloc_space.addr;
      uint64_t *shadow = (uint64_t *) (debug_malloc_shadow.addr + off);
      assert((*shadow & 1) == 1);
    }
  }
  //  *get_shadow(obj) = 1;
  obj->header |= GC_MARK_BIT;
}

static void unmark_object(Object *obj)
{
  obj->header &= ~GC_MARK_BIT;
}

static int is_marked_object(Object *obj)
{
  //  if (*get_shadow(obj))
  //    return 1;
#if HEADER0_GC_OFFSET <= 4 * 8  /* BITS_IN_INT */
  return obj->header & GC_MARK_BIT;
#else
  return !!(obj->header & GC_MARK_BIT);
#endif
}

static int test_and_mark_no_js_object(void *ptr)
{
  /* never be JS object */
  assert(!in_js_space(ptr));

  if (in_malloc_space(ptr)) {
    Object *header = (Object *) (((JSValue *) ptr) - HEADER_JSVALUES);
    if (is_marked_object(header))
      return 1;
    mark_object(header);
  }
  return 0;
}

static void trace_leaf_object_pointer(uintptr_t *ptrp)
{
  uintptr_t ptr = *ptrp;
  /* TODO: make a type for leaf object. */
  if (in_js_space((void *) ptr))
    mark_object((Object *) ptr);
  else if (in_malloc_space((void *) ptr))
    mark_object((Object *) (ptr - HEADER_BYTES));
}

static void trace_HashTable(HashTable **ptrp)
{
  HashTable *ptr = *ptrp;

  if (test_and_mark_no_js_object(ptr))
    return;

  trace_HashCell_array(&ptr->body, ptr->size);
}

static void trace_HashCell_array(HashCell ***ptrp, uint32_t length)
{
  HashCell **ptr = *ptrp;
  int i;
  if (test_and_mark_no_js_object(ptr))
    return;

  for (i = 0; i < length; i++) {
    if (ptr[i] != NULL)
      trace_HashCell(ptr + i);
  }
}

static void trace_HashCell(HashCell **ptrp)
{
  HashCell *ptr = *ptrp;
  if (test_and_mark_no_js_object(ptr))
    return;

  trace_slot(&ptr->entry.key);
  if (ptr->next != NULL)
    trace_HashCell(&ptr->next);
}

static void trace_Instruction_array_part(Instruction **ptrp,
					 uint32_t start, uint32_t end)
{
  Instruction *ptr = (Instruction *) *ptrp;
  uint32_t i;
  if (test_and_mark_no_js_object(ptr))
    return;

  for (i = start; i < end; i++)
    trace_slot((JSValue *) (ptr + i));
}

static void scan_FunctionTable(FunctionTable *ptr)
{
  /* trace constant pool */
  trace_Instruction_array_part(&ptr->insns, ptr->n_insns, ptr->body_size);
  trace_leaf_object_pointer((uintptr_t *) &ptr->insn_ptr);
}

static void trace_FunctionTable(FunctionTable **ptrp)
{
  /* TODO: see calling site */
  FunctionTable *ptr = *ptrp;
  if (test_and_mark_no_js_object(ptr))
    return;
  scan_FunctionTable(ptr);
}

static void trace_FunctionTable_array(FunctionTable **ptrp, uint64_t length)
{
  FunctionTable *ptr = *ptrp;
  size_t i;
  if (test_and_mark_no_js_object(ptr))
    return;
  for (i = 0; i < length; i++)
    scan_FunctionTable(ptr + i);
}

static void trace_FunctionFrame(FunctionFrame **ptrp)
{
  FunctionFrame *ptr = *ptrp;
  uint64_t header;
  uint64_t length;
  size_t   i;
  if (test_and_mark_no_js_object(ptr))
    return;

  if (ptr->prev_frame != NULL)
    trace_FunctionFrame(&ptr->prev_frame);
  trace_slot(&ptr->arguments);
  /* locals */
  header = *(((uint64_t *) ptr) - HEADER_JSVALUES);
  length = HEADER0_GET_SIZE(header) - HEADER_JSVALUES;
  length -= HEADER0_GET_EXTRA(header);
  for (i = ((JSValue *) &ptr->locals) - ((JSValue *) ptr); i < length; i++)
    trace_slot(ptr->locals + i);
}

static void trace_StrCons(StrCons **ptrp)
{
  StrCons *ptr = *ptrp;

  if (test_and_mark_no_js_object(ptr))
    return;

  trace_slot(&ptr->str);
  if (ptr->next != NULL)
    trace_StrCons(&ptr->next);
}

static void trace_StrCons_ptr_array(StrCons ***ptrp, int length)
{
  StrCons **ptr = *ptrp;
  size_t i;
  if (test_and_mark_no_js_object(ptr))
    return;

  for (i = 0; i < length; i++)
    if (ptr[i] != NULL)
      trace_StrCons(ptr + i);
}

/*
 * we do not move context
 */
static void trace_Context(Context **contextp)
{
  Context *context = *contextp;

  if (test_and_mark_no_js_object(context))
    return;

  trace_FunctionTable_array(&context->function_table, FUNCTION_TABLE_LIMIT);
  /* TODO: update spregs.cf which is an inner pointer to function_table */
  trace_FunctionFrame(&context->spreg.lp);
  trace_slot(&context->spreg.a);
  trace_slot(&context->spreg.err);
  scan_stack(context->stack, context->spreg.sp, context->spreg.fp);
}

static void trace_object_pointer(uintptr_t *ptrp)
{
  uintptr_t ptr = *ptrp;
  Object *obj;
  /* TODO: specialise.  If obj is pointed at through JSValue, we do not
   *       need to check it's space.  It should be in js_space */
  if (ptr == 0)
    return;
  if (in_malloc_space((void *) ptr))
    obj = (Object *) (ptr - HEADER_BYTES);
  else if (in_js_space((void *) ptr))
    obj = (Object *) ptr;
  else {
    /* default: JS object */
    obj = (Object *) ptr;
    goto SCAN;
  }

  if (is_marked_object(obj))
    return;
  mark_object(obj);

 SCAN:
  /* common header */
  trace_HashTable(&obj->map);
  trace_JSValue_array(&obj->prop, obj->n_props);

  switch (HEADER0_GET_TYPE(obj->header)) {
  case HTAG_OBJECT:
    break;
  case HTAG_ARRAY:
    trace_JSValue_array(&((ArrayCell *) obj)->body,
			((ArrayCell *) obj)->length);
    break;
  case HTAG_FUNCTION:
    /* TODO: func_table_entry holds an inner pointer */
    scan_FunctionTable(((FunctionCell *) obj)->func_table_entry);
    trace_FunctionFrame(&((FunctionCell *) obj)->environment);
    break;
  case HTAG_BUILTIN:
    break;
  case HTAG_ITERATOR:
    /* TODO: call scanHashIterator */
    trace_HashCell(&((IteratorCell *) obj)->iter.p);
    break;
#ifdef USE_REGEXP
  case HTAG_REGEXP:
    trace_leaf_object_pointer((uintptr_t *)&((RegexpCell *)obj)->pattern);
    break;
#endif /* USE_REGEXP */
  case HTAG_BOXED_STRING:
  case HTAG_BOXED_NUMBER:
  case HTAG_BOXED_BOOLEAN:
    trace_slot(&((BoxedCell *) obj)->value);
    break;
  }
}

static void trace_JSValue_array(JSValue **ptrp, uint32_t length)
{
  JSValue *ptr = *ptrp;
  int i;

  /* never be JS object */
  assert(!in_js_space(ptr));

  if (in_malloc_space(ptr)) {
    Object *header = (Object *) (ptr - HEADER_JSVALUES);
    if (is_marked_object(header))
      return;
    mark_object(header);
  }

  /* SCAN */
  for (i = 0; i < length; i++, ptr++)
    trace_slot(ptr);
}

static void trace_slot(JSValue* ptr)
{
  JSValue jsv = *ptr;
  /* TODO: use macro */
  if ((jsv & 0x2) != 0) /* not a pointer */
    return;
  if ((jsv & 0x4) != 0) {
    uint8_t tag = jsv & TAGMASK;
    jsv &= ~TAGMASK;
    trace_leaf_object_pointer((uintptr_t *) &jsv);
    *ptr = jsv | tag;
  }
  else
    trace_object_pointer((uintptr_t *) ptr);
}

static void scan_roots(Context *ctx)
{
  struct global_constant_objects *gconstsp = &gconsts;
  JSValue* p;

  /*
   * global variables
   */

  for (p = (JSValue *) gconstsp; p < (JSValue *) (gconstsp + 1); p++) {
    trace_slot(p);
  }
  /* function table: do not trace.
   *                 Used slots should be traced through Function objects
   */
  /* string table */
  trace_StrCons_ptr_array(&string_table.obvector, string_table.size);

  /*
   * Context
   */
  trace_Context(&ctx);
}

static void scan_stack(JSValue* stack, int sp, int fp)
{
  while (1) {
    while (sp >= fp) {
      trace_slot(stack + sp);
      sp--;
    }
    if (sp < 0)
      return;
    fp = stack[sp--];                                     // FP
    trace_FunctionFrame((FunctionFrame **)(stack + sp));  // LP
    sp--;
    sp--;                                                 // PC
    scan_FunctionTable((FunctionTable *) stack[sp--]);    // CF
    /* TODO: fixup inner pointer (CF) */
  }
}

static void sweep_space(struct space *space)
{
  struct free_chunk **p;
  uintptr_t scan = space->addr;
  uintptr_t free_bytes = 0;

  printf("sweep %s\n", space->name);

  space->freelist = NULL;
  p = &space->freelist;
  while (scan < space->addr + space->bytes) {
    uintptr_t last_used = 0;
    uintptr_t free_start;
    /* scan used area */
    while (scan < space->addr + space->bytes &&
	   is_marked_object((Object *) scan)) {
      uint64_t header = *(uint64_t *) scan;
      uint32_t size = HEADER0_GET_SIZE(header);
      last_used = scan;
      scan += size << LOG_BYTES_IN_JSVALUE;
    }
    free_start = scan;
    while (scan < space->addr + space->bytes &&
	   !is_marked_object((Object *) scan)) {
      uint64_t header = *(uint64_t *) scan;
      uint32_t size = HEADER0_GET_SIZE(header);
      scan += size << LOG_BYTES_IN_JSVALUE;
    }
    if (free_start < scan) {
      if (last_used != 0) {
	uint64_t last_header = *(uint64_t *) last_used;
	uint32_t extra = HEADER0_GET_EXTRA(last_header);
	free_start -= extra << LOG_BYTES_IN_JSVALUE;
      }
      if (scan - free_start >=
	  MINIMUM_FREE_CHUNK_JSVALUES << LOG_BYTES_IN_JSVALUE) {
	struct free_chunk *chunk = (struct free_chunk *) free_start;
	printf("add_cunk %x - %x (%d)\n",
	       free_start - space->addr, scan - space->addr,
	       scan - free_start);
	chunk->header =
	  HEADER0_COMPOSE((scan - free_start) >> LOG_BYTES_IN_JSVALUE,
			  0, HTAG_FREE);
	*p = chunk;
	p = &chunk->next;
	free_bytes += scan - free_start;
      }
    }
  }
  (*p) = NULL;
  space->free_bytes = free_bytes;
}


static void sweep(void)
{
  sweep_space(&malloc_space);
  sweep_space(&js_space);
}

static void print_memory_status(void)
{
  printf("  gc_disabled = %d\n", gc_disabled);
  printf("  js_space.free_bytes = %d\n", js_space.free_bytes);
  printf("  malloc_space.free_bytes = %d\n", malloc_space.free_bytes);
}
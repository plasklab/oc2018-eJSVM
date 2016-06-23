//
//  hash.c
//  SSJSVM Project, Iwasaki-lab, UEC
//
//  Sho Takada, 2012-13
//  Akira Tanimura 2012-13
//  Akihiro Urushihara, 2013-14
//  Ryota Fujii, 2013-14
//  Hideya Iwasaki, 2013-16
//

#include "prefix.h"
#define EXTERN extern
#include "header.h"

#define HASH_SKIP (27)

#define REHASH_THRESHOLD (0.5)
#define NO_MORE_CELL -1
#define MORE_CELL 1

// allocates a hash table
//
HashTable *malloc_hashtable(void) {
  return (HashTable*)malloc(sizeof(HashTable));
}

// initializes a hash table with the specified size
//
int hash_create(HashTable *table, unsigned int size) {
  int i;

  table->body = __hashMalloc(size);
  if (table->body == NULL) {
    LOG_EXIT("hash body malloc failed\n");
  }
  for (i = 0; i < size; i++) {
    table->body[i] = NULL;
  }
  table->size = size;
  table->filled = 0;
  table->entry_count = 0;
  return 0;
}

// obtains the value associated with a given key
//
int hash_get(HashTable *table, HashKey key, HashData *data) {
  uint32_t hval;
  HashCell *cell;

  hval = string_hash(key) % table->size;
  for (cell = table->body[hval]; cell != NULL; cell = cell->next)
    if ((JSValue)(cell->entry.key) == key) {
      // found
      if (data != NULL ) *data = cell->entry.data;
      return HASH_GET_SUCCESS;
    }
  // not found
  return HASH_GET_FAILED;
}

// registers a value to a hash table under a given key with an attribute
//
int hash_put_with_attribute(HashTable* table, HashKey key, HashData data,
                            Attribute attr) {
  HashCell* cell;
  uint32_t index;

  index = string_hash(key) % table->size;
  for (cell = table->body[index]; cell != NULL; cell = cell->next) {
    if (cell->entry.key == key) {
      // found
      if (!is_readonly(cell->entry.attr)) {
        cell->deleted = false;
        cell->entry.data = data;
        cell->entry.attr = attr;
        return HASH_PUT_SUCCESS;
      } else
        // return -1; // write error !!
        return HASH_PUT_FAILED;
    }
  }
  // not found
  cell = __hashCellMalloc();
  cell->next = table->body[index];
  table->body[index] = cell;
  cell->deleted = false;
  cell->entry.key = key;
  cell->entry.data = data;
  cell->entry.attr = attr;
  if (cell->next == NULL) {
    table->entry_count++;
    if (table->entry_count > REHASH_THRESHOLD * table->size)
      rehash(table);
  }
  return HASH_PUT_SUCCESS;
}

// deletes the hash data
//
int hash_delete(HashTable *table, HashKey key) {
  HashCell *cell, *prev;
  uint32_t index;

  index = string_hash(key) % table->size;
  for (prev = NULL, cell = table->body[index]; cell != NULL;
       prev = cell, cell = cell->next) {
    if (cell->entry.key == key) {
      // found
      if (!is_dont_delete(cell->entry.attr)) return HASH_GET_FAILED;
      if (prev == NULL) {
        table->body[index] = cell->next;
      } else {
        prev->next = cell->next;
      }
      hashCellFree(cell);
      return HASH_GET_SUCCESS;
    }
  }
  return HASH_GET_FAILED;
}

// calculates the hash value
//
uint32_t calc_hash_len(const char* s, uint32_t len) {
  uint32_t value = 0;
  int i;

  for (i = 0; i < len; i++) {
    value += s[i];
    value += (value << 10);
    value ^= (value >> 6);
  }
  value += (value << 3);
  value ^= (value >> 11);
  value += (value << 15);
  return value;
}

uint32_t calc_hash(const char* s) {
  return calc_hash_len(s, strlen(s));
}

// calculates the hash value (for two strings)
//
uint32_t calc_hash_len2(const char* s1, uint32_t len1, const char* s2, uint32_t len2) {
  uint32_t value = 0;
  int i;

  for (i = 0; i < len1; i++) {
    value += s1[i];
    value += (value << 10);
    value ^= (value >> 6);
  }
  for (i = 0; i < len2; i++) {
    value += s2[i];
    value += (value << 10);
    value ^= (value >> 6);
  }
  value += (value << 3);
  value ^= (value >> 11);
  value += (value << 15);
  return value;
}

uint32_t calc_hash2(const char* s1, const char* s2) {
  return calc_hash_len2(s1, strlen(s1), s2, strlen(s2));
}


//
HashCell** __hashMalloc(int size) {
  HashCell** ret = (HashCell**)malloc(sizeof(HashCell*) * size);
  memset(ret, 0, sizeof(HashCell*) * size);
  return ret;
}

//
HashCell* __hashCellMalloc() {
  HashCell* cell = (HashCell*)malloc(sizeof(HashCell));
  cell->next = NULL;
  return cell;
}

//
int rehash(HashTable *table) {
  int size = table->size;
  int newsize = size * 2;
  HashIterator iter;
  HashCell *p;
  HashCell** newhash = __hashMalloc(newsize);
  HashCell** oldhash = table->body;

  iter = createHashIterator(table);
  while (___hashNext(table, &iter, &p) != NO_MORE_CELL) {
    uint32_t index = string_hash(p->entry.key) % newsize;
    p->next = newhash[index];
    newhash[index] = p;
  }
  table->body = newhash;
  table->size = newsize;
  hashBodyFree(oldhash);
  return 0;
}

//
HashIterator createHashIterator(HashTable *table) {
  int i, size = table->size;
  HashIterator iter;

  iter.p = NULL;
  for (i = 0; i < size; i++) {
    if (table->body[i] != NULL) {
      iter.p = table->body[i];
      iter.index = i;
      break;
    }
  }
  return iter;
}

//
int hashNext(HashTable *table, HashIterator *Iter, HashData *data) {
  HashEntry e;
  int r = __hashNext(table, Iter, &e);

  *data = e.data;
  return r;
}

//
int hashNextKey(HashTable *table, HashIterator *Iter, HashKey *key) {
  HashEntry e;
  int r = __hashNext(table, Iter, &e);

  *key = e.key;
  return r;
}

//
int ___hashNext(HashTable *table, HashIterator *iter, HashCell** p) {
  int i;

  if (iter->p == NULL) return NO_MORE_CELL;
  *p = iter->p;
  if (iter->p->next != NULL) {
    iter->p = iter->p->next;
    return MORE_CELL;
  }
  for (i = iter->index + 1; i < table->size; i++) {
    if (table->body[i] != NULL) {
      iter->index = i;
      iter->p = table->body[i];
      return MORE_CELL;
    }
  }
  iter->p = NULL;
  return MORE_CELL;
}

//
int __hashNext(HashTable *table, HashIterator *iter, HashEntry *ep) {
  int i;

  if (iter->p == NULL) return NO_MORE_CELL;
  *ep = iter->p->entry;
  if (iter->p->next != NULL) {
    iter->p = iter->p->next;
    return MORE_CELL;
  }
  for(i = iter->index + 1; i < table->size; i++) {
    if(table->body[i] != NULL) {
      iter->index = i;
      iter->p = table->body[i];
      return MORE_CELL;
    }
  }
  iter->p = NULL;
  return MORE_CELL;
}

//
void hashBodyFree(HashCell** body) {
  free(body);
}

//
void hashCellFree(HashCell* cell) {
  free(cell);
}

//
char* ststrdup(const char* str) {
  uint64_t len = strlen(str)+1;
  char *dst = (char*)malloc(sizeof(char) * len);

  strcpy(dst, str);
  return dst;
}

//----------------------------------------------------------------------------
// string table

JSValue str_intern(const char* s, int len, uint32_t hash, int mode) {
  int index;
  StrCons *c;
  StringCell *p;
  JSValue v;

  index = hash % string_table.size;
  // printf("strIntern: s = %s, len = %d, hash = %ud, index = %d\n",
  //       s, len, hash, index);
  for (c = string_table.obvector[index]; c != NULL; c = c->next) {
    p = remove_string_tag(c->str);
#ifdef STROBJ_HAS_HASH
    if (len != p->length) continue;
#endif
    // printf(" now testing %s and %s\n", s, p->value);
    if (memcmp(s, p->value, len + 1) == 0) {
      //    printf("  strIntern: %s found, address = %lld\n", s, c->str);
      return c->str;
    }
  }
  // not found in the string table
  // if intern_soft, then returns null
  if (mode == INTERN_SOFT) return JS_NULL;
  // allocate a string and interns it
  p = allocate_string(len);
#ifdef STROBJ_HAS_HASH
  p->hash = hash;
#endif
  memcpy(p->value, s, len + 1);
  v = put_string_tag(p);
  // printf("  strIntern: %s not found, allocated addresss = %lld\n", s, v);
  c = (StrCons*)malloc(sizeof(StrCons));
  c->str = v;
  if ((c->next = string_table.obvector[index]) == NULL)
    string_table.count++;
  string_table.obvector[index] = c;
  return v;
}

JSValue str_intern2(const char* s1, int len1, const char* s2, int len2,
                    uint32_t hash, int mode) {
  int index;
  StrCons *c;
  StringCell *p;
  JSValue v;

  index = hash % string_table.size;
  for (c = string_table.obvector[index]; c != NULL; c = c->next) {
    p = remove_string_tag(c->str);
#ifdef STROBJ_HAS_HASH
    if (len1 + len2 != p->length) continue;
#endif
    if (memcmp(s1, p->value, len1) == 0 &&
        memcmp(s2, p->value + len1, len2 + 1) == 0)
      return c->str;
  }
  if (mode == INTERN_SOFT) return JS_NULL;
  p = allocate_string(len1 + len2);
#ifdef STROBJ_HAS_HASH
  p->hash = hash;
#endif
  memcpy(p->value, s1, len1);
  memcpy(p->value + len1, s2, len2 + 1);
  v = put_string_tag(p);
  c = (StrCons*)malloc(sizeof(StrCons));
  c->str = v;
  if ((c->next = string_table.obvector[index]) == NULL)
    string_table.count++;
  string_table.obvector[index] = c;
  return v;
}
#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"
#include <stdint.h>

typedef struct {
  ObjString *key;
  Value value;
} Entry;

typedef struct {
  // Load factor = count / capacity
  int count;
  int capacity;
  Entry *entries;
} Table;

void initTable(Table *table);
void freeTable(Table *table);

bool tableGet(Table *table, ObjString *key, Value *value);
/**
 * Return true if the key did not already exist in the table.
 */
bool tableSet(Table *table, ObjString *key, Value value);
void tableAddAll(Table *from, Table *to);
ObjString *tableFindString(Table *table, const char *chars, int length,
                           uint32_t hash);
bool tableDelete(Table *table, ObjString *key);
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(vm, type, objectType)                                     \
  (type *)allocateObject(vm, sizeof(type), objectType)

static Obj *allocateObject(VM *vm, size_t size, ObjType type) {
  Obj *object = (Obj *)reallocate(NULL, 0, size);
  object->type = type;

  object->next = vm->objects;
  vm->objects = object;
  return object;
}

static ObjString *allocateString(VM *vm, char *chars, int length,
                                 uint32_t hash) {
  ObjString *string = ALLOCATE_OBJ(vm, ObjString, OBJ_STRING);
  string->length = length;
  string->chars = chars;
  string->hash = hash;

  // String interning
  tableSet(&vm->strings, string, NIL_VAL);

  return string;
}

static uint32_t hashString(const char *key, int length) {
  uint32_t hash = 2166136261u;

  for (int i = 0; i < length; i++) {
    hash ^= key[i];
    hash *= 16777619;
  }

  return hash;
}

ObjString *takeString(VM *vm, char *chars, int length) {
  uint32_t hash = hashString(chars, length);
  ObjString *interned = tableFindString(&vm->strings, chars, length, hash);

  if (interned != NULL) {
    FREE_ARRAY(char, chars, length + 1);
    return interned;
  }

  return allocateString(vm, chars, length, hash);
}

ObjString *copyString(VM *vm, const char *chars, int length) {
  uint32_t hash = hashString(chars, length);
  ObjString *interned = tableFindString(&vm->strings, chars, length, hash);

  if (interned != NULL) {
    return interned;
  }

  char *heapChars = ALLOCATE(char, length + 1);
  memcpy(heapChars, chars, length);
  heapChars[length] = '\0';

  return allocateString(vm, heapChars, length, hash);
}

void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
  case OBJ_STRING:
    printf("%s", AS_CSTRING(value));
  }
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
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

static ObjString *allocateString(VM *vm, char *chars, int length) {
  ObjString *string = ALLOCATE_OBJ(vm, ObjString, OBJ_STRING);
  string->length = length;
  string->chars = chars;

  return string;
}

ObjString *takeString(VM *vm, char *chars, int length) {
  return allocateString(vm, chars, length);
}

ObjString *copyString(VM *vm, const char *chars, int length) {
  char *heapChars = ALLOCATE(char, length + 1);
  memcpy(heapChars, chars, length);
  heapChars[length] = '\0';

  return allocateString(vm, heapChars, length);
}

void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
  case OBJ_STRING:
    printf("%s", AS_CSTRING(value));
  }
}
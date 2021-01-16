#include "memory.h"
#include "chunk.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#include <stdio.h>

#include <stdlib.h>

void *reallocate(void *ptr, size_t oldSize, size_t newSize) {
  if (newSize == 0) {
    free(ptr);
    return NULL;
  }

  void *result = realloc(ptr, newSize);

  if (result == NULL) {
    fprintf(stderr, "realloc failed.\n");
    exit(1);
  }
  return result;
}

static void freeObject(Obj *object) {
  switch (object->type) {

  case OBJ_FUNCTION: {
    ObjFunction *function = (ObjFunction *)object;
    freeChunk(&function->chunk);
    FREE(OBJ_FUNCTION, object);
    break;
  }
  case OBJ_STRING: {
    ObjString *string = (ObjString *)object;
    FREE_ARRAY(char, string->chars, string->length + 1);
    FREE(ObjString, object);
    break;
  }
  }
}

void freeObjects(VM *vm) {
  Obj *object = vm->objects;
  while (object != NULL) {
    Obj *next = object->next;
    freeObject(object);
    object = next;
  }
}

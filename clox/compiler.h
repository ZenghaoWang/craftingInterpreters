#ifndef clox_compiler_h
#define clox_compiler_h

#include "object.h"
#include "vm.h"

/**
 * Return NULL on compile error.
 */
ObjFunction *compile(VM *vm, const char *source);
#endif
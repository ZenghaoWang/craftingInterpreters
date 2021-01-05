#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "value.h"

#include <stdint.h>

#define STACK_MAX 256

typedef struct {
  Chunk *chunk;
  uint8_t *ip; // Ptr to the next instruction to be executed

  Value stack[STACK_MAX];
  // Ptr to just after the top value on the stack.
  // In other words, where the next value to be pushed will go.
  Value *stackTop;
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult;

void initVM(VM *vm);
void freeVM(VM *vm);

InterpretResult interpret(VM *vm, Chunk *chunk);
void push(VM *vm, Value value);
Value pop(VM *vm);

#endif
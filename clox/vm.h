#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "common.h"
#include "object.h"
#include "table.h"
#include "value.h"

#include <stdint.h>

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

// A representation of a single ongoing function call.
typedef struct {
  ObjFunction *function;
  uint8_t *ip;
  // Points inside the VM value stack to the first slot that the
  // function call can use
  Value *slots;
} CallFrame;

struct VM {
  CallFrame frames[FRAMES_MAX];
  int frameCount;

  Value stack[STACK_MAX];
  // Ptr to just after the top value on the stack.
  // In other words, where the next value to be pushed will go.
  Value *stackTop;

  Table globals;

  Table strings;

  Obj *objects;
};

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult;

void initVM(VM *vm);
void freeVM(VM *vm);

InterpretResult interpret(VM *vm, const char *source);
void push(VM *vm, Value value);
Value pop(VM *vm);

#endif
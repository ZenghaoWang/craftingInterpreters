#include "vm.h"
#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void resetStack(VM *vm) { vm->stackTop = vm->stack; }

static void runtimeError(VM *vm, const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  size_t instruction = vm->ip - vm->chunk->code - 1;
  int line = vm->chunk->lines[instruction];
  fprintf(stderr, "[line %d] in script\n", line);

  resetStack(vm);
}

void initVM(VM *vm) {
  resetStack(vm);
  vm->objects = NULL;
  initTable(&vm->strings);
  initTable(&vm->globals);
}

void freeVM(VM *vm) {
  freeTable(&vm->strings);
  freeTable(&vm->globals);
  freeObjects(vm);
}

void push(VM *vm, Value value) {
  *vm->stackTop = value;
  vm->stackTop++;
}

Value pop(VM *vm) {
  vm->stackTop--;
  return *vm->stackTop;
}

static Value peek(VM *vm, int distance) { return vm->stackTop[-1 - distance]; }

static bool isFalsey(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate(VM *vm) {
  ObjString *b = AS_STRING(pop(vm));
  ObjString *a = AS_STRING(pop(vm));

  int length = a->length + b->length;
  char *chars = ALLOCATE(char, length + 1);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';

  ObjString *result = takeString(vm, chars, length);
  push(vm, OBJ_VAL(result));
}

static InterpretResult run(VM *vm) {
#define READ_BYTE() (*vm->ip++)
#define READ_CONSTANT() (vm->chunk->constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define BINARY_OP(valueType, op)                                               \
  do {                                                                         \
    if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) {                  \
      runtimeError(vm, "Operands must be numbers.");                           \
      return INTERPRET_RUNTIME_ERROR;                                          \
    }                                                                          \
    double b = AS_NUMBER(pop(vm));                                             \
    double a = AS_NUMBER(pop(vm));                                             \
    push(vm, valueType(a op b));                                               \
  } while (false)

  while (true) {
#ifdef DEBUG_TRACE_EXECUTION
    // Print out the stack from bottom to top.
    printf("          ");
    for (Value *slot = vm->stack; slot < vm->stackTop; slot++) {
      printf("[ ");
      printValue(*slot);
      printf(" ]");
    }
    printf("\n");
    // this function takes an offset, but we are storing a direct pointer
    disassembleInstruction(vm->chunk, (int)(vm->ip - vm->chunk->code));
#endif

    uint8_t instruction;
    switch (instruction = READ_BYTE()) {

    case OP_CONSTANT: {
      Value constant = READ_CONSTANT();
      push(vm, constant);
      break;
    }
    case OP_NIL:
      push(vm, NIL_VAL);
      break;
    case OP_TRUE:
      push(vm, BOOL_VAL(true));
      break;
    case OP_FALSE:
      push(vm, BOOL_VAL(false));
      break;

    case OP_POP:
      pop(vm);
      break;

    case OP_DEFINE_GLOBAL: {
      ObjString *name = READ_STRING();
      tableSet(&vm->globals, name, peek(vm, 0));
      pop(vm);
      break;
    }

    case OP_EQUAL: {
      Value b = pop(vm);
      Value a = pop(vm);
      push(vm, BOOL_VAL(valuesEqual(a, b)));
      break;
    }

    case OP_GREATER:
      BINARY_OP(BOOL_VAL, >);
      break;
    case OP_LESS:
      BINARY_OP(BOOL_VAL, <);
      break;

    case OP_ADD: {
      if (IS_STRING(peek(vm, 0)) && IS_STRING(peek(vm, 1))) {
        concatenate(vm);
      } else if (IS_NUMBER(peek(vm, 0)) && IS_NUMBER(peek(vm, 1))) {
        double b = AS_NUMBER(pop(vm));
        double a = AS_NUMBER(pop(vm));
        push(vm, NUMBER_VAL(a + b));
      } else {
        runtimeError(vm, "Operands must be two numbers or two strings.");
      }
      break;
    }

    case OP_SUBTRACT:
      BINARY_OP(NUMBER_VAL, -);
      break;
    case OP_MULTIPLY:
      BINARY_OP(NUMBER_VAL, *);
      break;
    case OP_DIVIDE:
      BINARY_OP(NUMBER_VAL, /);
      break;

    case OP_NOT:
      push(vm, BOOL_VAL(isFalsey(pop(vm))));
      break;
    case OP_NEGATE:
      if (!IS_NUMBER(peek(vm, 0))) {
        runtimeError(vm, "Operand must be a number.");
        return INTERPRET_RUNTIME_ERROR;
      }

      // Negate the top value on the stack
      push(vm, NUMBER_VAL(-AS_NUMBER(pop(vm))));
      break;

    case OP_PRINT:
      printValue(pop(vm));
      printf("\n");
      break;

    case OP_RETURN: {
      return INTERPRET_OK;
    }
    }
  }
#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
}

InterpretResult interpret(VM *vm, const char *source) {
  Chunk chunk;
  initChunk(&chunk);

  if (!compile(vm, source, &chunk)) {
    freeChunk(&chunk);
    return INTERPRET_COMPILE_ERROR;
  }

  vm->chunk = &chunk;
  vm->ip = vm->chunk->code;

  InterpretResult result = run(vm);

  freeChunk(&chunk);
  return result;
}

#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

typedef enum {
  OP_PRINT,
  OP_JUMP,
  OP_JUMP_IF_FALSE,
  OP_RETURN,
  OP_LOOP,

  OP_CONSTANT,

  OP_NIL,
  OP_TRUE,
  OP_FALSE,

  OP_POP,
  OP_DEFINE_GLOBAL,
  OP_GET_GLOBAL,
  OP_SET_GLOBAL,
  OP_GET_LOCAL,
  OP_SET_LOCAL,

  OP_EQUAL,
  OP_GREATER,
  OP_LESS,

  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,

  OP_NOT,
  OP_NEGATE,
} OpCode;

typedef struct {
  uint8_t *code; // Dynamic array
  int *lines;    // Each number is the line number for the corresponding byte
  ValueArray constants;
  int count;    // Number of elements in array
  int capacity; // Number of elements that array can hold
} Chunk;

void initChunk(Chunk *Chunk);
void writeChunk(Chunk *chunk, uint8_t byte, int line);

/**
 * Add a constant to the value array. Return the index where the constant was
 * appended.
 */
int addConstant(Chunk *chunk, Value value);
void freeChunk(Chunk *chunk);

#endif
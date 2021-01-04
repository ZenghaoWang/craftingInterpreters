#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

typedef enum {
  OP_RETURN,
  OP_CONSTANT,
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
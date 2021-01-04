#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include <bits/stdint-uintn.h>

typedef enum {
  OP_RETURN,
} OpCode;

typedef struct {
  uint8_t *code; // Dynamic array
  int count;     // Number of elements in array
  int capacity;
} Chunk;

void initChunk(Chunk *Chunk);
void writeChunk(Chunk *chunk, uint8_t byte);
void freeChunk(Chunk *chunk);

#endif
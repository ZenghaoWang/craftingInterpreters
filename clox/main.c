#include "chunk.h"
#include "common.h"
#include "debug.h"

int main(int argc, char *argv[]) {
  Chunk chunk;
  initChunk(&chunk);

  // append a constant number to value array
  int constant_idx = addConstant(&chunk, 1.2);

  // Tell chunk to execute the constant in two steps
  writeChunk(&chunk, OP_CONSTANT, 123);
  writeChunk(&chunk, constant_idx, 123);

  writeChunk(&chunk, OP_RETURN, 123);

  disassembleChunk(&chunk, "test chunk");
  freeChunk(&chunk);

  return 0;
}
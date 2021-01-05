#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "vm.h"

int main(int argc, char *argv[]) {
  VM vm;
  initVM(&vm);

  Chunk chunk;
  initChunk(&chunk);

  // -((1.2 + 3.4) / 5.6)
  int constant_idx = addConstant(&chunk, 1.2);
  writeChunk(&chunk, OP_CONSTANT, 123);
  writeChunk(&chunk, constant_idx, 123);

  constant_idx = addConstant(&chunk, 3.4);
  writeChunk(&chunk, OP_CONSTANT, 123);
  writeChunk(&chunk, constant_idx, 123);

  writeChunk(&chunk, OP_ADD, 123);

  constant_idx = addConstant(&chunk, 5.6);
  writeChunk(&chunk, OP_CONSTANT, 123);
  writeChunk(&chunk, constant_idx, 123);

  writeChunk(&chunk, OP_DIVIDE, 123);
  writeChunk(&chunk, OP_NEGATE, 123);

  writeChunk(&chunk, OP_RETURN, 123);

  interpret(&vm, &chunk);

  freeVM(&vm);
  freeChunk(&chunk);

  return 0;
}
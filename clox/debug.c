#include "debug.h"
#include "value.h"

#include "chunk.h"
#include <stdint.h>
#include <stdio.h>

void disassembleChunk(Chunk *chunk, const char *name) {
  printf("== %s ==\n", name);

  // We dont increment offset ourselves; we let disassembleInstruction do it
  // because instructions can have different sizes.
  for (int offset = 0; offset < chunk->count;) {
    offset = disassembleInstruction(chunk, offset);
  }
}

static int simpleInstruction(const char *name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}

/**
 *
 */
static int constantInstruction(const char *name, Chunk *chunk, int offset) {
  uint8_t constant_idx = chunk->code[offset + 1];
  printf("%-16s %4d '", name, constant_idx);
  printValue(chunk->constants.values[constant_idx]);
  printf("'\n");
  // 2 bytes: 1 for opcode, 1 for operand (index)
  return offset + 2;
}

int disassembleInstruction(Chunk *chunk, int offset) {
  printf("%04d ", offset);
  if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
    printf("   | ");
  } else {
    printf("%4d ", chunk->lines[offset]);
  }

  uint8_t instruction = chunk->code[offset];

  switch (instruction) {
  case OP_RETURN:
    return simpleInstruction("OP_RETURN", offset);
  case OP_CONSTANT:
    return constantInstruction("OP_CONSTANT", chunk, offset);
  default:
    printf("Unknown opcode %d\n", instruction);
    return offset + 1;
  }
}

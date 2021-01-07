#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "vm.h"

#include <features.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void repl(VM *vm) {
  char line[1024];
  while (true) {
    printf("> ");

    if (!fgets(line, sizeof(line), stdin)) {
      printf("\n");
      break;
    }

    interpret(vm, line);
  }
}

/**
 * Dynamically allocate and read to a buffer from a file.
 * Must be freed after.
 */
static char *readFile(const char *path) {
  FILE *file = fopen(path, "rb");
  if (file == NULL) {
    fprintf(stderr, "Could not open file \"%s\".\n", path);
    exit(74);
  }

  // Go to end of file, get size of file, go back to beginning
  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);

  char *buffer = malloc(fileSize + 1);
  if (buffer == NULL) {
    fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
    exit(74);
  }
  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
  if (bytesRead < fileSize) {
    fprintf(stderr, "Could not read file \"%s\".\n", path);
    exit(74);
  }
  buffer[bytesRead] = '\0';

  fclose(file);
  return buffer;
}

static void runFile(VM *vm, const char *path) {
  char *source = readFile(path);
  InterpretResult result = interpret(vm, source);
  free(source);

  switch (result) {
  case INTERPRET_COMPILE_ERROR:
    exit(65);
  case INTERPRET_RUNTIME_ERROR:
    exit(70);
  default:
    exit(0);
  }
}

int main(int argc, char *argv[]) {
  VM vm;
  initVM(&vm);

  switch (argc) {
  case 1:
    repl(&vm);
    break;
  case 2:
    runFile(&vm, argv[1]);
    break;
  default:
    fprintf(stderr, "Usage: clox | clox [path]\n");
  }

  freeVM(&vm);

  return 0;
}
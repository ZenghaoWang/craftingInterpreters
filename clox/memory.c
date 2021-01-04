#include "memory.h"
#include <stdio.h>

#include <stdlib.h>

void *reallocate(void *ptr, size_t oldSize, size_t newSize) {
  if (newSize == 0) {
    free(ptr);
    return NULL;
  }

  void *result = realloc(ptr, newSize);

  if (result == NULL) {
    fprintf(stderr, "realloc failed.\n");
    exit(1);
  }
  return result;
}

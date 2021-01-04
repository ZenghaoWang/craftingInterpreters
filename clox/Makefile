CC = gcc
CFLAGS := -Wall -Wextra -Werror $(CFLAGS)
LDFLAGS := $(LDFLAGS)

.PHONY: all clean

all: main

main: chunk.o memory.o
	$(CC) $^ -o $@ $(LDFLAGS)

SRC_FILES = $(wildcard *.c)
OBJ_FILES = $(SRC_FILES:.c=.o)

-include $(OBJ_FILES:.o=.d)

%.o: %.c
	$(CC) $< -o $@ -c -MMD $(CFLAGS)


clean:
	rm -f $(OBJ_FILES) $(OBJ_FILES:.o=.d) main
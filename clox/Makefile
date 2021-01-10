CFLAGS := -Wall -Wextra $(CFLAGS)

.PHONY: all clean

all: clox 

clox: value.o chunk.o debug.o memory.o clox.o vm.o compiler.o scanner.o object.o table.o
	gcc $^ -o $@

SRC_FILES = $(wildcard *.c)
OBJ_FILES = $(SRC_FILES:.c=.o)

%.o: %.c
	gcc $< -o $@ -c $(CFLAGS)


clean:
	rm -f  $(OBJ_FILES) $(OBJ_FILES:.o=.d) clox


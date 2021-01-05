CFLAGS := -Wall -Wextra $(CFLAGS)

.PHONY: all clean

all: main 

main: value.o chunk.o debug.o memory.o main.o vm.o
	gcc $^ -o $@

SRC_FILES = $(wildcard *.c)
OBJ_FILES = $(SRC_FILES:.c=.o)

%.o: %.c
	gcc $< -o $@ -c $(CFLAGS)


clean:
	rm -f  $(OBJ_FILES) $(OBJ_FILES:.o=.d) main


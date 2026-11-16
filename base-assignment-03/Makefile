CC = gcc
CFLAGS = -Wall -Wextra -Iinclude
SRC = $(wildcard src/*.c)
OBJ = $(patsubst src/%.c,obj/%.o,$(SRC))

bin/psh: $(OBJ)
	@mkdir -p bin
	$(CC) $(OBJ) -o bin/psh

obj/%.o: src/%.c include/shell.h
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf obj bin

CC      ?= gcc
CFLAGS  := -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -pedantic -Iinclude -g
SRC     := $(filter-out src/main.c,$(wildcard src/*.c))
OBJ     := $(SRC:src/%.c=build/%.o)
TESTS   := $(patsubst tests/%.c,build/%,$(wildcard tests/test_*.c))

all: build/simple_shell

build/%.o: src/%.c $(wildcard include/*.h)
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/simple_shell: $(OBJ) build/main.o
	$(CC) $(CFLAGS) $^ -o $@

build/test_%: tests/test_%.c $(OBJ)
	@mkdir -p build
	$(CC) $(CFLAGS) $^ -o $@

test: $(TESTS)
	@for t in $(TESTS); do echo "== $$t"; ./$$t || exit 1; done

clean:
	rm -rf build

.PHONY: all test clean

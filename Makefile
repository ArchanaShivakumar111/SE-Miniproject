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
	@$(MAKE) --no-print-directory build/simple_shell
	@echo "== tests/sec_t07.sh"; sh tests/sec_t07.sh build/simple_shell

clean:
	rm -rf build

# NFR-002 / MEM-01: 1,000 successive commands, zero leaks and invalid accesses.
mem-input:
	@mkdir -p build
	@i=0; while [ $$i -lt 1000 ]; do echo "echo $$i > /dev/null"; i=$$((i+1)); done > build/mem_input.txt
	@echo "alias ll='echo x'" >> build/mem_input.txt
	@echo "ll" >> build/mem_input.txt
	@echo "unalias ll" >> build/mem_input.txt

valgrind: build/simple_shell mem-input
	HOME=$$(mktemp -d) valgrind --leak-check=full --error-exitcode=1 \
		--trace-children=no ./build/simple_shell < build/mem_input.txt > /dev/null

asan: mem-input
	@mkdir -p build/asan
	$(CC) $(CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer \
		src/*.c -o build/asan/simple_shell
	HOME=$$(mktemp -d) ASAN_OPTIONS=detect_leaks=$(if $(filter Darwin,$(shell uname)),0,1) ./build/asan/simple_shell < build/mem_input.txt > /dev/null

.PHONY: all test clean mem-input valgrind asan

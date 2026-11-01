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
	@echo "== tests/perf_pt02.sh"; sh tests/perf_pt02.sh build/simple_shell
	@echo "== tests/redirection.sh"; sh tests/redirection.sh build/simple_shell
	@echo "== tests/sec_t03.sh"; sh tests/sec_t03.sh build/simple_shell
	@echo "== tests/background_jobs.sh"; sh tests/background_jobs.sh build/simple_shell
	@echo "== tests/zombie_processes.sh"; sh tests/zombie_processes.sh build/simple_shell
	@echo "== tests/lexer.sh"; sh tests/lexer.sh build/simple_shell
	@echo "== tests/repl.sh"; sh tests/repl.sh build/simple_shell
	@echo "== tests/env_builtins.sh"; sh tests/env_builtins.sh build/simple_shell
	@echo "== tests/negative.sh"; sh tests/negative.sh build/simple_shell
	@echo "== tests/sec_t04.sh"; sh tests/sec_t04.sh build/simple_shell
	@echo "== tests/sec_t02.sh"; sh tests/sec_t02.sh build/simple_shell
	@echo "== tests/expander.sh"; sh tests/expander.sh build/simple_shell

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
		-fno-pie -no-pie src/*.c -o build/asan/simple_shell
	HOME=$$(mktemp -d) ASAN_OPTIONS=detect_leaks=$(if $(filter Darwin,$(shell uname)),0,1) ./build/asan/simple_shell < build/mem_input.txt > /dev/null

# SEC-001 / SEC-T01: 1,000 random lines (up to 1 MB) through the lexer and
# expander, then through the whole shell, under ASan/UBSan; any report fails.
SANITIZE := -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer
fuzz:
	@mkdir -p build/fuzz
	$(CC) $(CFLAGS) $(SANITIZE) tests/fuzz.c src/lexer.c src/expander.c -o build/fuzz/fuzz
	$(CC) $(CFLAGS) $(SANITIZE) src/*.c -o build/fuzz/simple_shell
	ASAN_OPTIONS=detect_leaks=1 ./build/fuzz/fuzz lex 1000
	sh tests/fuzz_shell.sh build/fuzz/simple_shell build/fuzz/fuzz 1000

# NFR-005 / QA-01: static analysis must report nothing.
lint:
	cppcheck --enable=warning,style,performance,portability --std=c11 \
		--error-exitcode=1 -Iinclude -q src tests

# NFR-005: no mutable file-scope data in any module (only .rodata/.text).
check-globals: $(OBJ) build/main.o
	@bad=$$(nm $^ 2>/dev/null | grep -E ' [dDbBcC] ' || true); \
	if [ -n "$$bad" ]; then echo "mutable globals found:"; echo "$$bad"; exit 1; fi; \
	echo "check-globals OK"

# SEC-002: no system(), popen() or exec of /bin/sh anywhere in the sources.
INJECTION_DIRS ?= src include
check-injection:
	@if grep -rnE '(^|[^A-Za-z0-9_])(system|popen)[[:space:]]*\(|/bin/sh' $(INJECTION_DIRS); then \
		echo "SEC-002: banned call found (system, popen or /bin/sh)"; exit 1; fi; \
	echo "check-injection OK"

.PHONY: all test clean mem-input valgrind asan fuzz lint check-globals check-injection

CC       ?= gcc
CFLAGS   := -std=c11 -Wall -Wextra -Wpedantic -Werror -g -Iinclude
SRC_DIR  := src
OBJ_DIR  := build
TARGET   := simple_shell

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

.PHONY: all clean test cppcheck run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

test: $(TARGET)
	sh tests/run_tests.sh

cppcheck:
	cppcheck --enable=warning,style --error-exitcode=1 -Iinclude $(SRC_DIR)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

CC      ?= cc
CFLAGS  ?= -O2 -g
CFLAGS  += -std=gnu11 -Wall -Wextra -Werror -Isrc
BUILD   := build

CORE    := src/buffer.c
SRCS    := $(wildcard src/*.c)
BIN     := $(BUILD)/anvil

$(BIN): $(SRCS) | $(BUILD)
	$(CC) $(CFLAGS) $(SRCS) -o $@

$(BUILD):
	mkdir -p $(BUILD)

test: $(BIN)
	@mkdir -p $(BUILD)/tests
	$(CC) $(CFLAGS) tests/test_buffer.c src/buffer.c -o $(BUILD)/tests/test_buffer
	$(BUILD)/tests/test_buffer
	$(CC) $(CFLAGS) tests/fuzz_buffer.c src/buffer.c -o $(BUILD)/tests/fuzz_buffer
	$(BUILD)/tests/fuzz_buffer
	$(CC) $(CFLAGS) tests/test_undo.c src/undo.c src/buffer.c -o $(BUILD)/tests/test_undo
	$(BUILD)/tests/test_undo
	@if command -v python3 >/dev/null; then python3 tests/pty_smoke.py; fi
	@echo "ALL TESTS PASSED"

clean:
	rm -rf $(BUILD)

.PHONY: test clean

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -Isrc
LDFLAGS = 

DEBUG ?= 0
ifeq ($(DEBUG),1)
    CFLAGS += -g -DDEBUG
    BUILD_DIR = build/debug
else
    BUILD_DIR = build/release
endif

SRC_DIR = src
COMMON_DIR = $(SRC_DIR)/common
COMPILER_DIR = $(SRC_DIR)/compiler
LEXER_DIR = $(SRC_DIR)/lexer
PARSER_DIR = $(SRC_DIR)/parser
INTERPRETER_DIR = $(SRC_DIR)/interpreter
TYPE_CHECK_DIR = $(SRC_DIR)/type_check
RUNTIME_DIR = $(SRC_DIR)/runtime
PROJECT_DIR = $(SRC_DIR)/project

SOURCES = $(SRC_DIR)/main.c \
          $(SRC_DIR)/accelerator.c \
          $(COMMON_DIR)/memory.c \
          $(COMMON_DIR)/memory_leak_detector.c \
          $(COMMON_DIR)/output_cache.c \
          $(COMMON_DIR)/path_utils.c \
          $(COMMON_DIR)/stack_calculator.c \
          $(COMMON_DIR)/thread_pool.c \
          $(COMPILER_DIR)/compiler.c \
          $(COMPILER_DIR)/ir.c \
          $(COMPILER_DIR)/ir_gen.c \
          $(COMPILER_DIR)/parallel_compiler.c \
          $(COMPILER_DIR)/runtime.c \
          $(COMPILER_DIR)/semantic_analyzer.c \
          $(COMPILER_DIR)/symbol_table.c \
          $(COMPILER_DIR)/x86_backend.c \
          $(LEXER_DIR)/lexer.c \
          $(PARSER_DIR)/ast_nodes.c \
          $(PARSER_DIR)/parser.c \
          $(INTERPRETER_DIR)/interpreter.c \
          $(TYPE_CHECK_DIR)/type_check.c \
          $(RUNTIME_DIR)/runtime.c \
          $(PROJECT_DIR)/esproj.c \
          $(PROJECT_DIR)/esproj_parser.c

OBJECTS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(SOURCES))

TARGET = $(BUILD_DIR)/e_sharp
TARGET_WIN = $(BUILD_DIR)/e_sharp.exe

ifeq ($(OS),Windows_NT)
    TARGET_FINAL = $(TARGET_WIN)
    RM = del /Q
    MKDIR = mkdir
else
    TARGET_FINAL = $(TARGET)
    RM = rm -f
    MKDIR = mkdir -p
endif

.PHONY: all clean test install debug release

all: release

$(BUILD_DIR)/%.o: %.c
	@$(MKDIR) $(dir $@) 2>/dev/null || true
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET_FINAL): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

release:
	@$(MKDIR) $(BUILD_DIR) 2>/dev/null || true
	$(MAKE) DEBUG=0

debug:
	@$(MKDIR) $(BUILD_DIR) 2>/dev/null || true
	$(MAKE) DEBUG=1

clean:
ifeq ($(OS),Windows_NT)
	@if exist $(BUILD_DIR) rmdir /S /Q $(BUILD_DIR)
	@if exist build rmdir /S /Q build
else
	$(RM) -rf $(BUILD_DIR)
	$(RM) -rf build
endif

test: release
	@echo "Running E# compiler tests..."
	@if [ -f "$(TARGET_FINAL)" ]; then \
		echo "Testing array example..."; \
		$(TARGET_FINAL) examples/test_array_simple2.esf || exit 1; \
		echo "Test passed!"; \
	else \
		echo "Error: $(TARGET_FINAL) not found"; \
		exit 1; \
	fi

install: release
	@echo "Installing E# compiler..."
ifeq ($(OS),Windows_NT)
	@echo "Copy $(TARGET_WIN) to system PATH"
else
	@cp $(TARGET) /usr/local/bin/
	@echo "Installed to /usr/local/bin/"
endif

ci-build: clean
	@echo "Building for CI/CD..."
	$(MAKE) release

help:
	@echo "E# Compiler Makefile"
	@echo "Usage:"
	@echo "  make              - Build release version"
	@echo "  make debug        - Build debug version"
	@echo "  make clean        - Clean build files"
	@echo "  make test         - Run tests"
	@echo "  make install      - Install compiler"
	@echo "  make ci-build     - Build for CI/CD"
	@echo "  make help         - Show this help"
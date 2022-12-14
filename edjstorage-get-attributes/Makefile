# SDK versions that will be used for compilation
# (CHANGE THE VERSION NUMBERS IF NEEDED)
WASI_SDK_VERSION := 12.0
EDJX_CPP_SDK_VERSION := v22.12.1-wasi-12

# Root directory of WASI SDK
WASI_SDK_PATH := $(HOME)/edjx/wasi-sdk-$(WASI_SDK_VERSION)

# Paths to headers and SDK library
INCLUDE_DIR := $(HOME)/edjx/edjx-cpp-sdk-$(EDJX_CPP_SDK_VERSION)/include
LIB_DIR := $(HOME)/edjx/edjx-cpp-sdk-$(EDJX_CPP_SDK_VERSION)/lib

# Directories used by the project
SRC_DIR := src/
BUILD_DIR := build/
TARGET_DIR := bin/

# Name of the compiled WASM executable
TARGET := edjstorage_get_attributes.wasm

# Source cpp files
SRC := $(notdir $(wildcard $(SRC_DIR)/*.cpp))

# Compiler options
CC := $(WASI_SDK_PATH)/bin/clang++
CFLAGS := --target=wasm32-wasi -std=c++17 --sysroot=$(WASI_SDK_PATH)/share/wasi-sysroot/ -Wall -Werror -O2 -fno-exceptions -static
CLIBS := -ledjx
CPPFLAGS += -MD -MP

# Additional shell commands
MKDIR_P := mkdir -p

# ---------------------
#  Compilation Targets
# ---------------------

.PHONY: all
all: directories $(TARGET_DIR)/$(TARGET)

.PHONY: directories
directories: $(TARGET_DIR) $(BUILD_DIR)

$(TARGET_DIR):
	$(MKDIR_P) $@

$(BUILD_DIR):
	$(MKDIR_P) $@

$(TARGET_DIR)/$(TARGET): $(SRC:%.cpp=$(BUILD_DIR)/%.o)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -L$(LIB_DIR) -o $@ $^ $(CLIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CC) $(CPPFLAGS) $(CFLAGS) -I$(INCLUDE_DIR) -c -o $@ $<

-include $(SRC:%.cpp=$(BUILD_DIR)/%.d)

.PHONY: clean
clean:
	rm -f $(TARGET_DIR)/$(TARGET) $(BUILD_DIR)/*.o $(BUILD_DIR)/*.d

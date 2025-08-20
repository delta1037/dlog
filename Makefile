####################################################
# DLog Makefile - Dynamic Library Only
# Features:
#   1. Build dynamic library (libdlog.so)
#   2. Build test executable
#   3. Release packaging support
####################################################

# ------ Project Config ------
LIB_NAME    := dlog
TEST_NAME   := dlog_test
BUILD_DIR   := build
RELEASE_DIR := release
INSTALL_DIR ?= /usr/local
LIB_VERSION := 1.0.0

# Compiler settings
CC      := gcc
CFLAGS  := -g -Wall -Wextra -Iinclude -fPIC
LDFLAGS := -Wl,-rpath,'./'
LDLIBS  := 

# Dynamic library flags
SHARED_FLAGS := -shared -Wl,-soname,lib$(LIB_NAME).so.$(firstword $(subst ., ,$(LIB_VERSION)))

# ------ Source Setup ------
# Library sources
LIB_SRC_DIR := src
LIB_SRCS    := $(wildcard $(LIB_SRC_DIR)/*.c)
LIB_OBJS    := $(patsubst $(LIB_SRC_DIR)/%.c,$(BUILD_DIR)/lib/%.o,$(LIB_SRCS))
LIB_HEADERS := $(wildcard include/*.h)

# Test sources
TEST_SRC_DIR := main
TEST_SRCS    := $(wildcard $(TEST_SRC_DIR)/*.c)
TEST_OBJS    := $(patsubst $(TEST_SRC_DIR)/%.c,$(BUILD_DIR)/test/%.o,$(TEST_SRCS))
TEST_DEPS    := $(TEST_OBJS:.o=.d)

# ------ Build Rules ------
.PHONY: all lib test clean release install

all: lib test

lib: $(BUILD_DIR)/lib$(LIB_NAME).so.$(LIB_VERSION)

test: $(BUILD_DIR)/$(TEST_NAME)

# Create build directories
$(BUILD_DIR)/lib $(BUILD_DIR)/test $(RELEASE_DIR)/lib $(RELEASE_DIR)/include $(RELEASE_DIR)/config:
	@mkdir -p $@

# Library object files (position independent code)
$(BUILD_DIR)/lib/%.o: $(LIB_SRC_DIR)/%.c | $(BUILD_DIR)/lib
	$(CC) $(CFLAGS) -c $< -o $@

# Shared library with versioning
$(BUILD_DIR)/lib$(LIB_NAME).so.$(LIB_VERSION): $(LIB_OBJS) | $(BUILD_DIR)/lib
	$(CC) $(SHARED_FLAGS) $^ -o $@ $(LDLIBS)
	ln -sf lib$(LIB_NAME).so.$(LIB_VERSION) $(BUILD_DIR)/lib$(LIB_NAME).so
	ln -sf lib$(LIB_NAME).so.$(LIB_VERSION) $(BUILD_DIR)/lib$(LIB_NAME).so.$(firstword $(subst ., ,$(LIB_VERSION)))

# Test object files
$(BUILD_DIR)/test/%.o: $(TEST_SRC_DIR)/%.c | $(BUILD_DIR)/test
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@ -MMD

# Test executable (linked with shared library)
$(BUILD_DIR)/$(TEST_NAME): $(TEST_OBJS) $(BUILD_DIR)/lib$(LIB_NAME).so.$(LIB_VERSION) | $(BUILD_DIR)/test
	$(CC) $(LDFLAGS) -L$(BUILD_DIR) $< -o $@ -l$(LIB_NAME) -Wl,-rpath,$(BUILD_DIR)

# Release packaging
release: lib | $(RELEASE_DIR)/lib $(RELEASE_DIR)/include $(RELEASE_DIR)/config
	@echo "Creating release package..."
	cp $(BUILD_DIR)/lib$(LIB_NAME).so.$(LIB_VERSION) $(RELEASE_DIR)/lib/
	cd $(RELEASE_DIR)/lib && \
		ln -sf lib$(LIB_NAME).so.$(LIB_VERSION) lib$(LIB_NAME).so && \
		ln -sf lib$(LIB_NAME).so.$(LIB_VERSION) lib$(LIB_NAME).so.$(firstword $(subst ., ,$(LIB_VERSION)))
	cp include/*.h $(RELEASE_DIR)/include/
	cp include/dlog.properties $(RELEASE_DIR)/config/
	@echo "Release package created in $(RELEASE_DIR)"

# Installation
install: lib
	@echo "Installing library to $(INSTALL_DIR)"
	install -d $(INSTALL_DIR)/lib/
	install -d $(INSTALL_DIR)/include/
	install -m 755 $(BUILD_DIR)/lib$(LIB_NAME).so.$(LIB_VERSION) $(INSTALL_DIR)/lib/
	cd $(INSTALL_DIR)/lib && \
		ln -sf lib$(LIB_NAME).so.$(LIB_VERSION) lib$(LIB_NAME).so && \
		ln -sf lib$(LIB_NAME).so.$(LIB_VERSION) lib$(LIB_NAME).so.$(firstword $(subst ., ,$(LIB_VERSION)))
	install -m 644 include/*.h $(INSTALL_DIR)/include/
	install -m 644 include/dlog.properties $(INSTALL_DIR)/etc/
	ldconfig || true
	@echo "Installation complete"

# Clean
clean:
	@rm -rf $(BUILD_DIR)
	@echo "Build files cleaned"

distclean: clean
	@rm -rf $(RELEASE_DIR)
	@echo "Release files cleaned"

# Include test dependencies
-include $(TEST_DEPS)
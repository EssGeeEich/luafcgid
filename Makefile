# Raspberry Pi Makefile

# Target Binary Settings
BIN = luafcgid2
OPTIMIZATION ?= -O2

# Installation directories
PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin
CONFDIR ?= /etc/luafcgid2
INITDIR ?= /etc/init.d

# Lua library paths
LUAINC = $(PREFIX)/include/lua5.3
LUALIB = $(PREFIX)/lib
LLIB = lua5.3

# Project Set-up
INC_PATH = include
SRC_PATH = source
BUILD_PATH = build
BIN_PATH = build/bin
DEP_PATH = deps

# Build Flags
CXX ?= g++
CC  ?= gcc
WARN = -Wall -Wextra -pedantic

CXX_V = -std=c++11
CC_V = -std=c11
DEFINES = -DHAVE_STRUCT_STAT_ST_MTIME=1 -DHAVE_CXX_MUTEX=1 -DHAVE_CXX_ATOMIC=1

# Precomputed Build Flags
INCLUDES = -I$(PREFIX)/include -I$(LUAINC) -I$(INC_PATH)
LDFLAGS = -L$(PREFIX)/lib -L$(LUALIB) $(OPTIMIZATION)
LDLIBS = -lm -lpthread -lfcgi -l$(LLIB)
DEP_OBJ =

CXXFLAGS = $(CXX_V) $(OPTIMIZATION) $(WARN) $(INCLUDES) $(DEFINES)
CFLAGS   = $(CC_V) $(OPTIMIZATION) $(WARN) $(INCLUDES) $(DEFINES)

# Get list of .cpp and .o files. You can add more extensions.
SOURCES = $(shell find $(SRC_PATH) -type f \( -name '*.cpp' -o -name '*.c' \) -printf '%T@ %p\n' | sort -k 1nr | cut -d ' ' -f 2)
OBJECTS = $(SOURCES:$(SRC_PATH)/%=$(BUILD_PATH)/%.o)
DEP_DIRS = $(shell ls -l $(DEP_PATH) | grep '^d' | awk '{ print $$9 }')
DEPS = cleandep $(DEP_DIRS:%=$(BUILD_PATH)/%.mk)

export OPTIMIZATION PREFIX LUAINC LUALIB LLIB CXX CXX_V CC CC_V WARN DEFINES

.PHONY: default_target
default_target: all

.PHONY: dirs
dirs:
	@echo "Creating directories"
	@mkdir -p $(dir $(OBJECTS)) $(BIN_PATH)

.PHONY: cleandep
cleandep:
	@echo "Deleting dependencies' results..."
	@rm -f $(DEP_PATH)/include
	@rm -f $(DEP_PATH)/ldflags
	@rm -f $(DEP_PATH)/ldlibs
	@rm -f $(DEP_PATH)/objects

.PHONY: clean
clean: cleandep
	@echo "Deleting $(BIN) symlink"
	@$(RM) $(BIN)
	@echo "Deleting directories"
	@$(RM) -r $(BUILD_PATH)
	@$(RM) -r $(BIN_PATH)

# Building dependencies (Run all *.Makefile files in deps/)
$(BUILD_PATH)/%.mk: $(DEP_PATH)/%.Makefile
	$(MAKE) -C $(DEP_PATH) -f $*.Makefile

.PHONY: deps
deps: $(DEPS)
INCLUDES += $(shell cat $(DEP_PATH)/include)
LDFLAGS += $(shell cat $(DEP_PATH)/ldflags)
LDLIBS += $(shell cat $(DEP_PATH)/ldlibs)
DEP_OBJ += $(shell cat $(DEP_PATH)/objects)

$(BUILD_PATH)/%.c.o: $(SRC_PATH)/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_PATH)/%.cpp.o: $(SRC_PATH)/%.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Creation of the executable
$(BIN_PATH)/$(BIN): $(OBJECTS)
	@echo "Linking: $@"
	$(CXX) $(OBJECTS) $(DEP_OBJ) $(LDFLAGS) $(LDLIBS) -o $@


.PHONY: all
all: dirs deps $(BIN_PATH)/$(BIN)
	@echo "Making symlink: $(BIN_PATH)/$(BIN) -> $(BIN)"
	@$(RM) $(BIN)
	@ln -s $(BIN_PATH)/$(BIN) $(BIN)

.PHONY: install
install: all
	@mkdir -p $(CONFDIR)
	install -m 755 $(BIN) $(BINDIR)
	install -m 644 scripts/luafcgid2-config.lua $(CONFDIR)/config.lua

.PHONY: update
update: all
	@mkdir -p $(CONFDIR)
	install -m 755 $(BIN) $(BINDIR)

.PHONY: install-daemon
install-daemon: all
	install -m 755 scripts/luafcgid2-init.d $(INITDIR)/luafcgid2
	update-rc.d -f luafcgid2 defaults

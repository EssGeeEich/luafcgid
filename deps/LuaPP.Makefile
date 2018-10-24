SUBMODULE = LuaPP
SUBMODULE_SOURCE = src
SUBMODULE_INCLUDE = include

SOURCES = $(shell find $(SUBMODULE)/$(SUBMODULE_SOURCE) -name 'main.*' -prune -o -type f \( -name '*.cpp' -o -name '*.c' \) -printf '%T@ %p\n' | sort -k 1nr | cut -d ' ' -f 2)
#SOURCES = LuaPP/src/reference.cpp LuaPP/src/state.cpp LuaPP/src/typeext.cpp LuaPP/src/variable.cpp LuaPP/src/library.cpp LuaPP/src/util.cpp
OBJECTS = $(SOURCES:$(SUBMODULE)/$(SUBMODULE_SOURCE)/%=build/$(SUBMODULE)/%.o)
INCLUDES = -I$(LUAINC) -I$(SUBMODULE)/$(SUBMODULE_INCLUDE)
CXXFLAGS = $(CXX_V) $(OPTIMIZATION) $(WARN) $(INCLUDES)

.PHONY: default_target
default_target: all

dirs:
	@mkdir -p build/$(SUBMODULE)
	
build/$(SUBMODULE)/%.cpp.o: $(SUBMODULE)/$(SUBMODULE_SOURCE)/%.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

all: dirs $(OBJECTS)
	@echo $(SOURCES)
	@echo $(OBJECTS)
	@echo -Ideps/$(SUBMODULE)/$(SUBMODULE_INCLUDE) >>include
	@echo $(OBJECTS:%=deps/%) >>objects
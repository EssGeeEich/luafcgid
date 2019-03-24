SUBMODULE = randutils
SUBMODULE_SOURCE = /
SUBMODULE_INCLUDE = /

SOURCES = $(shell find $(SUBMODULE)/$(SUBMODULE_SOURCE) -name 'main.*' -prune -o -type f \( -name '*.cpp' -o -name '*.c' \) -printf '%T@ %p\n' | sort -k 1nr | cut -d ' ' -f 2)
OBJECTS = $(SOURCES:$(SUBMODULE)/$(SUBMODULE_SOURCE)/%=build/$(SUBMODULE)/%.o)
INCLUDES = -I$(LUAINC) -I$(SUBMODULE)/$(SUBMODULE_INCLUDE)
CXXFLAGS = $(CXX_V) $(OPTIMIZATION) $(WARN) $(INCLUDES) $(DEFINES)

.PHONY: default_target
default_target: all

dirs:
	@mkdir -p build/$(SUBMODULE)
	
build/$(SUBMODULE)/%.cpp.o: $(SUBMODULE)/$(SUBMODULE_SOURCE)/%.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

all: dirs $(OBJECTS)
	@echo -Ideps/$(SUBMODULE)/$(SUBMODULE_INCLUDE) >>include
	@echo $(OBJECTS:%=deps/%) >>objects

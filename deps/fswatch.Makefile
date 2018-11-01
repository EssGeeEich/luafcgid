SUBMODULE = fswatch
SUBMODULE_SOURCE = libfswatch/src/libfswatch
SUBMODULE_INCLUDE = libfswatch/src/libfswatch

SOURCES = \
	$(SUBMODULE)/$(SUBMODULE_SOURCE)/c/cevent.cpp \
	$(SUBMODULE)/$(SUBMODULE_SOURCE)/c/libfswatch.cpp \
	$(SUBMODULE)/$(SUBMODULE_SOURCE)/c/libfswatch_log.cpp \
	$(SUBMODULE)/$(SUBMODULE_SOURCE)/c++/libfswatch_exception.cpp \
	$(SUBMODULE)/$(SUBMODULE_SOURCE)/c++/event.cpp \
	$(SUBMODULE)/$(SUBMODULE_SOURCE)/c++/filter.cpp \
	$(SUBMODULE)/$(SUBMODULE_SOURCE)/c++/monitor.cpp \
	$(SUBMODULE)/$(SUBMODULE_SOURCE)/c++/monitor_factory.cpp \
	$(SUBMODULE)/$(SUBMODULE_SOURCE)/c++/poll_monitor.cpp \
	$(SUBMODULE)/$(SUBMODULE_SOURCE)/c++/path_utils.cpp \
	$(SUBMODULE)/$(SUBMODULE_SOURCE)/c++/string/string_utils.cpp \
	$(SUBMODULE)/$(SUBMODULE_SOURCE)/c++/inotify_monitor.cpp
	
	
OBJECTS = $(SOURCES:$(SUBMODULE)/$(SUBMODULE_SOURCE)/%=build/$(SUBMODULE)/%.o)
INCLUDES = -I$(LUAINC) -I$(SUBMODULE)/$(SUBMODULE_INCLUDE)
CXXFLAGS = $(CXX_V) $(OPTIMIZATION) $(WARN) $(INCLUDES) $(DEFINES)

.PHONY: default_target
default_target: all

dirs:
	@mkdir -p build/$(SUBMODULE)
	
build/$(SUBMODULE)/%.cpp.o: $(SUBMODULE)/$(SUBMODULE_SOURCE)/%.cpp
	@mkdir -p $(shell dirname $@) build/$(SUBMODULE)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

all: dirs $(OBJECTS)
	@echo -Ideps/$(SUBMODULE)/$(SUBMODULE_INCLUDE) >>include
	@echo $(OBJECTS:%=deps/%) >>objects
SHELL = /bin/bash

EXE = beegfs-event-listener

CXXFLAGS = -std=c++0x -I ../include
LDFLAGS =

SOURCES = $(shell find ../source -name '*.cpp')
OBJECTS = $(SOURCES:.cpp=.o)
DEPENDENCY_FILES = $(shell find ../source -name '*.d')
CLEANUP_OBJECTS = $(shell find ../source -name '*.o') $(DEPENDENCY_FILES)

all: $(SOURCES) $(EXE)

$(EXE): $(OBJECTS)
	$(CXX) -o $(EXE) $(OBJECTS) $(LDFLAGS)

.cpp.o:
	$(CXX) $(CXXFLAGS) -c $(@:.o=.cpp) -E -MMD -MF$(@:.o=.d) -MT$(@) -o/dev/null
	$(CXX) $(CXXFLAGS) -o$(@) -c $(@:.o=.cpp)

clean:
	rm -rf $(CLEANUP_OBJECTS) $(DEPENDENCY_FILES) $(EXE)


# Include dependency files
ifneq ($(DEPENDENCY_FILES),)
include $(DEPENDENCY_FILES)
endif

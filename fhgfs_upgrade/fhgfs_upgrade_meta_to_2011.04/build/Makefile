# This is the fhgfs-upgrade-meta-to-2011.04 makefile.
# It creates an ELF executable.
# 
# Use "make help" to find out about configuration options.


EXE = fhgfs-upgrade-meta-to-2011.04

CXX ?= g++
CXXFLAGS = -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -I../source \
	-rdynamic -Wall -fmessage-length=0 -fno-strict-aliasing -Wunused-variable \
	-Wextra -Wno-unused-parameter -ggdb3 $(USER_CXXFLAGS)
CXXFLAGS_RELEASE = -O3 -Wuninitialized
CXXFLAGS_DEBUG = -O0 -D_FORTIFY_SOURCE=2 \
	-DFHGFS_DEBUG -DDEBUG_MUTEX_LOCKING -DDEBUG_BACKTRACE -DLOG_DEBUG_MESSAGES
LDFLAGS = -lrt -rdynamic $(USER_LDFLAGS)
LDFLAGS_RELASE = -O3
LDFLAGS_DEBUG = -O0

SOURCES = $(shell find ../source -name '*.cpp')

OBJECTS = $(SOURCES:.cpp=.o)
CLEANUP_OBJECTS = $(shell find ../source -name '*.o')

DEPENDENCY_FILES = $(shell find ../source -name '*.d')

ifeq ($(FHGFS_DEBUG),)
CXXFLAGS += $(CXXFLAGS_RELEASE)
LDFLAGS += $(LDFLAGS_RELASE)
else
CXXFLAGS += $(CXXFLAGS_DEBUG)
LDFLAGS += $(LDFLAGS_DEBUG)
endif

ifneq ($(FHGFS_VERSION),)
CXXFLAGS += '-DFHGFS_VERSION="$(FHGFS_VERSION)"'
endif


all: $(SOURCES) $(EXE)

$(EXE): $(OBJECTS)
	$(CXX) -o $(EXE) $(OBJECTS) $(LDFLAGS)
	cp -a $(EXE) $(EXE)-unstripped
	strip --strip-debug $(EXE)

.cpp.o: 
	$(CXX) $(CXXFLAGS) -c $(@:.o=.cpp) -E -MMD -MF$(@:.o=.d) -MT$(@) -o/dev/null
	$(DISTCC) $(CXX) $(CXXFLAGS) -o$(@) -c $(@:.o=.cpp) 

clean:
	rm -rf $(CLEANUP_OBJECTS) $(DEPENDENCY_FILES) $(EXE) $(EXE)-unstripped

help:
	@echo 'Optional Arguments:'
	@echo '   FHGFS_DEBUG=1:'
	@echo '       Enables debug information and symbols.'
	@echo '   CXX=<compiler>:'
	@echo '       Specifies a c++ compiler.'
	@echo '   DISTCC=distcc:'
	@echo '       Enables the usage of distcc.'
	@echo '   FHGFS_VERSION="a-version-string"'
	@echo '       Version the daemon identifies itself with'
	@echo
	@echo 'targets:'
	@echo '   all (default) - compile the code'
	@echo '   clean         - delete previously compiled files'
	@echo '   help          - print this help'
	@echo

# Include dependency files
ifneq ($(DEPENDENCY_FILES),)
include $(DEPENDENCY_FILES)
endif


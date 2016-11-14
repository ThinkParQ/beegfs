# This is the fhgfs_common makefile.
# It creates a static library.
# 
# Use "make help" to find out about configuration options.

FHGFS_OPENTK_PATH ?= ../../fhgfs_opentk_lib/
FHGFS_THIRDPARTY_PATH ?= ../../fhgfs_thirdparty/

CPPUNIT_INC_PATH ?= $(FHGFS_THIRDPARTY_PATH)/source/cppunit_install/usr/local/include

LIB = fhgfs-common

CXX ?= g++
CXXFLAGS = -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 \
	-I../source -I$(FHGFS_OPENTK_PATH)/include -I$(CPPUNIT_INC_PATH) \
	-pthread -Wall -fmessage-length=0 -fno-strict-aliasing \
	-Wunused-variable -Wextra -Wno-unused-parameter -ggdb3 $(USER_CXXFLAGS)
CXXFLAGS_RELEASE = -O3 -Wuninitialized
CXXFLAGS_DEBUG = -O0 -ggdb3 -D_FORTIFY_SOURCE=2 \
	-DFHGFS_DEBUG -DDEBUG_READWRITE -DDEBUG_MUTEX_LOCKING -DDEBUG_REFCOUNT \
	-DDEBUG_BACKTRACE -DLOG_DEBUG_MESSAGES
LDFLAGS = -pthread -rdynamic $(USER_LDFLAGS)
LDFLAGS_RELASE = -O3
LDFLAGS_DEBUG = -O0

SOURCES = $(shell find ../source -name '*.cpp')

OBJECTS = $(SOURCES:.cpp=.o)
CLEANUP_OBJECTS = $(shell find ../source -name '*.o')

DEPENDENCY_FILES = $(shell find ../source -name '*.d')


ifeq ($(FHGFS_DEBUG),1)
CXXFLAGS += $(CXXFLAGS_DEBUG)
LDFLAGS += $(LDFLAGS_DEBUG)
else
CXXFLAGS += $(CXXFLAGS_RELEASE)
LDFLAGS += $(LDFLAGS_RELASE)
endif

all: $(SOURCES) $(LIB)

$(LIB): $(OBJECTS)
	ar -r lib$(LIB).a $(OBJECTS)

.cpp.o: 
	$(CXX) $(CXXFLAGS) -c $(@:.o=.cpp) -E -MMD -MF$(@:.o=.d) -MT$(@) -o/dev/null
	$(DISTCC) $(CXX) $(CXXFLAGS) -o$(@) -c $(@:.o=.cpp) 

clean:
	rm -rf $(CLEANUP_OBJECTS) $(DEPENDENCY_FILES) lib$(LIB).a

help:
	@echo 'Optional Arguments:'
	@echo '- FHGFS_DEBUG=1:'
	@echo '	  Enables debug information and symbols.'
	@echo '- CXX=<compiler>:'
	@echo '    Specifies a c++ compiler.'
	@echo '- DISTCC=distcc:'
	@echo '    Enables the usage of distcc.'

# Include dependency files
ifneq ($(DEPENDENCY_FILES),)
include $(DEPENDENCY_FILES)
endif


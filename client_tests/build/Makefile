SHELL = /bin/bash

EXE = beegfs-client-tests

BEEGFS_VERSION_PATH    ?= ../../beegfs-version
BEEGFS_COMMON_PATH     ?= ../../common/
BEEGFS_THIRDPARTY_PATH ?= ../../thirdparty/
BEEGFS_AUTO_CXX11_SCRIPT_PATH ?= $(BEEGFS_COMMON_PATH)/build/check-cxx-for-c++11.sh
CPPUNIT_INC_PATH      ?= $(BEEGFS_THIRDPARTY_PATH)/source/cppunit_install/usr/local/include

CXX ?= g++
CXXFLAGS = -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 \
	-I../source -I$(CPPUNIT_INC_PATH) \
	-pthread -Wall -fmessage-length=0 -fno-strict-aliasing \
	-Wunused-variable -Woverloaded-virtual -Wextra -Wno-unused-parameter \
	-ggdb3 $(USER_CXXFLAGS)
CXXFLAGS_RELEASE = -O3 -Wuninitialized
CXXFLAGS_DEBUG = -O0 -ggdb3

THIRTPARTY_LIBS = $(BEEGFS_THIRDPARTY_PATH)/build/libcppunit.a

LDFLAGS = -L$(BEEGFS_COMMON_PATH)/build \
	-L$(BEEGFS_THIRDPARTY_PATH)/build -pthread -rdynamic -lrt \
	$(THIRTPARTY_LIBS) -rdynamic -ldl $(USER_LDFLAGS)

LDFLAGS_RELASE = -O3
LDFLAGS_DEBUG = -O0

SOURCES = $(shell find ../source -name '*.cpp')

OBJECTS = $(SOURCES:.cpp=.o)
CLEANUP_OBJECTS = $(shell find ../source -name '*.o')

DEPENDENCY_FILES = $(shell find ../source -name '*.d')

ifeq ($(BEEGFS_DEBUG),)
CXXFLAGS += $(CXXFLAGS_RELEASE)
LDFLAGS += $(LDFLAGS_RELASE)
else
CXXFLAGS += $(CXXFLAGS_DEBUG)
LDFLAGS += $(LDFLAGS_DEBUG)
endif

# beegfs version
# "override", because we don't want BEEGFS_VERSION(_CODE) from the environment.
# VER and RELEASE_STR are the variables that can be set by the environment.
override BEEGFS_VERSION := $(shell bash -c \
	"VER=$(VER) RELEASE_STR=$(RELEASE_STR) $(BEEGFS_VERSION_PATH) --print_version")
override BEEGFS_VERSION_CODE := $(shell bash -c \
	"VER=$(VER) RELEASE_STR=$(RELEASE_STR) $(BEEGFS_VERSION_PATH) --print_version_code")
CXXFLAGS += -DBEEGFS_VERSION=\"$(BEEGFS_VERSION)\" -DBEEGFS_VERSION_CODE=$(BEEGFS_VERSION_CODE)

# automatic selection of c++11 if compiler is g++ with version >=4.8
AUTO_CXX11 ?= $(shell $(BEEGFS_AUTO_CXX11_SCRIPT_PATH) $(CXX) )
ifeq ($(AUTO_CXX11),1)
CXXFLAGS += -std=c++11
endif


all: test_version $(SOURCES) $(EXE)

test_version:
	@# just call the beegfs-version script to check whether it returns an error. the script output is
	@# used in other places of this makefile and we want to make sure that we don't mess up version
	@# tags in the binary with script errors.
	@echo "beegfs version check..."
	@$(BEEGFS_VERSION_PATH) --verbose

$(EXE): $(OBJECTS)
	$(CXX) -o $(EXE) $(OBJECTS) $(LDFLAGS)

.cpp.o:
	$(CXX) $(CXXFLAGS) -c $(@:.o=.cpp) -E -MMD -MF$(@:.o=.d) -MT$(@) -o/dev/null
	$(DISTCC) $(CXX) $(CXXFLAGS) -o$(@) -c $(@:.o=.cpp)

clean:
	rm -rf $(CLEANUP_OBJECTS) $(DEPENDENCY_FILES) $(EXE) $(EXE)-unstripped

help:
	@echo 'Optional Arguments:'
	@echo '   CXX=<compiler>:'
	@echo '     Specifies a c++ compiler.'
	@echo '   DISTCC=distcc:'
	@echo '     Enables the usage of distcc.'


# Include dependency files
ifneq ($(DEPENDENCY_FILES),)
include $(DEPENDENCY_FILES)
endif

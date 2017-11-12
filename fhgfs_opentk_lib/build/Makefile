# This is the beegfs_opentk makefile.
# It creates a shared library.
#
# Use "make help" to find out about configuration options.


LIB = beegfs-opentk
LIB_NO_IB=$(LIB)-disabledIB
LIB_IB=$(LIB)-enabledIB

# Include System Feature Auto-Detectors
include SystemFeatureDetection.mk

PREFIX                ?= /opt/beegfs
ETC_DIR               ?= /etc

SRC_PATH=$(DESTDIR)/$(PREFIX)/src/beegfs_opentk_lib

CXX ?= g++
CXXFLAGS = -std=gnu++0x -D_FILE_OFFSET_BITS=64 -I../source -I../include \
	-Wall -fmessage-length=0 -fno-strict-aliasing -Wunused-variable \
	-fPIC -Wextra -Wno-unused-parameter -pthread -ggdb3 $(SYSTEM_FEATURE_DETECTION) \
	$(USER_CXXFLAGS)
CXXFLAGS_RELEASE = -O3 -Wuninitialized
CXXFLAGS_DEBUG = -O1 -ggdb3 -D_FORTIFY_SOURCE=2 \
	-DBEEGFS_DEBUG -DBEEGFS_OPENTK_LOG_CONN_ERRORS
LDFLAGS = -shared -pthread -rdynamic $(USER_LDFLAGS)
LDFLAGS_RELASE = -O3
LDFLAGS_DEBUG = -O0

SOURCES = $(shell find ../source -name '*.cpp')

OBJECTS = $(SOURCES:.cpp=.o)
CLEANUP_OBJECTS = $(shell find ../source -name '*.o')

DEPENDENCY_FILES = $(SOURCES:.cpp=.d)

ifeq ($(BEEGFS_DEBUG),)
CXXFLAGS += $(CXXFLAGS_RELEASE)
LDFLAGS += $(LDFLAGS_RELASE)
else
CXXFLAGS += $(CXXFLAGS_DEBUG)
LDFLAGS += $(LDFLAGS_DEBUG)
endif

# if path to strip command was not given, use default
# (alternative strip is important when cross-compiling)
ifeq ($(STRIP),)
STRIP=strip
endif

# OFED
ifeq ($(BEEGFS_OPENTK_IBVERBS),1)
CXXFLAGS += -DBEEGFS_OPENTK_IBVERBS
LDFLAGS += -lrdmacm -libverbs
endif

ifneq ($(OFED_INCLUDE_PATH),)
CXXFLAGS += -I$(OFED_INCLUDE_PATH)
endif

ifneq ($(OFED_LIB_PATH),)
LDFLAGS += -L$(OFED_LIB_PATH)
endif


all: $(SOURCES) lib$(LIB).a

all-deeper: $(SOURCES) lib$(LIB).a
	# clean up old stuff first
	$(MAKE) clean

	$(MAKE) FHGFS_OPENTK_IBVERBS=0

strip:
	cp lib$(LIB).a lib$(LIB)-unstripped.a
	${STRIP} --strip-debug lib$(LIB).a

lib$(LIB).a: $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o lib$(LIB).so
	ar -r lib$(LIB).a $(OBJECTS)

# build without and with IB support
package_lib:

	# clean up old stuff first
	$(MAKE) clean

	# with IB first
	$(MAKE) BEEGFS_OPENTK_IBVERBS=1
	mv lib$(LIB).so lib$(LIB_IB).so.clean_save

	$(MAKE) clean
	mv lib$(LIB_IB).so.clean_save lib$(LIB_IB).so # clean would have deleted the .so
	cp -a lib$(LIB_IB).so lib$(LIB_IB)-unstripped.so
	${STRIP} --strip-debug lib$(LIB_IB).so

	# now without IB
	$(MAKE) BEEGFS_OPENTK_IBVERBS=0
	mv lib$(LIB).so lib$(LIB_NO_IB).so
	cp -a lib$(LIB_NO_IB).so lib$(LIB_NO_IB)-unstripped.so
	${STRIP} --strip-debug lib$(LIB_NO_IB).so

%.o: %.cpp Makefile
	$V$(CXX) $(CXXFLAGS) -c $< -E -MMD -MP -MF$(@:.o=.d) -MT$@ -o/dev/null
	$V$(DISTCC) $(CXX) $(CXXFLAGS) -o$@ -c $<

install: package_lib
	install -D -m 644 lib$(LIB_NO_IB).so $(DESTDIR)/$(PREFIX)/lib/lib$(LIB_NO_IB).so
	install -D -m 644 lib$(LIB_IB).so    $(DESTDIR)/$(PREFIX)/lib/lib$(LIB_IB).so

package_install: install
	@# PREFIX is something like /usr or /usr/local or /opt/beegfs
	@# DESTDIR is mainly for package builders
	install -D -m 644 dist/etc/ld.so.conf.d/beegfs.conf.in $(DESTDIR)/$(ETC_DIR)/ld.so.conf.d/beegfs.conf
	install -D -m 644 dist/etc/beegfs/beegfs-libopentk.conf $(DESTDIR)/$(ETC_DIR)/beegfs/beegfs-libopentk.conf
	sed -i -e "s#__PREFIX__#$(PREFIX)#" $(DESTDIR)/$(ETC_DIR)/ld.so.conf.d/beegfs.conf

	install -D ../scripts/beegfs-setup-rdma \
		$(DESTDIR)/usr/sbin/beegfs-setup-rdma

	@ echo "Copying opentk sources ..."
	mkdir --parents ${SRC_PATH}/build
	cp Makefile $(SRC_PATH)/build/
	cp SystemFeatureDetection.mk $(SRC_PATH)/build/
	cp -a dist $(SRC_PATH)/build/

	find ../include -mount -name '*.h' -type f | \
		xargs -I '{}' cp --parents '{}' $(SRC_PATH)/build
	find ../source -mount -name '*.h' -type f | \
		xargs -I '{}' cp --parents '{}' $(SRC_PATH)/build
	find ../source -mount -name '*.c' -type f | \
		xargs -I '{}' cp --parents '{}' $(SRC_PATH)/build
	find ../source -mount -name '*.cpp' -type f | \
		xargs -I '{}' cp --parents '{}' $(SRC_PATH)/build

clean:
	rm -rf $(CLEANUP_OBJECTS) $(DEPENDENCY_FILES) lib$(LIB).so \
		lib$(LIB_NO_IB).so lib$(LIB_IB).so lib$(LIB).a \
		lib$(LIB_NO_IB)-unstripped.so lib$(LIB_IB)-unstripped.so lib$(LIB)-unstripped.a

rpm:
	./make-rpm

deb:
	./make-deb

help:
	@echo 'Optional Arguments:'
	@echo '   BEEGFS_OPENTK_IBVERBS=1:'
	@echo '      Defining this enables ibverbs support.'
	@echo '   OFED_INCLUDE_PATH=<path>:'
	@echo '      Path to OpenFabrics Enterpise Distribution include directory.'
	@echo '      Note: This directory contains e.g. "rdma/rdma_cma.h". (If this is'
	@echo '      undefined then standard system include paths will be used.)'
	@echo '   OFED_LIB_PATH=<path>:'
	@echo '      Path to OpenFabrics Enterpise Distribution lib directory.'
	@echo '      Note: This directory contains librdmacm and libibverbs.'
	@echo '   BEEGFS_DEBUG=1:'
	@echo '      Enables debug information and symbols.'
	@echo '   CXX=<compiler>:'
	@echo '      Specifies a C++ compiler.'
	@echo '   DISTCC=distcc:'
	@echo '      Enables the usage of distcc.'
	@echo '   PREFIX=/some/path (default: /opt/beegfs)'
	@echo '      Installation root directory.'
	@echo '   ETC_DIR:'
	@echo '      Prefix for configuration files (default: /etc).'
	@echo '   DESTDIR=/some/path:'
	@echo '      Mainly for package building, used as primary path prefix then.'
	@echo
	@echo 'targets:'
	@echo '   all (default)  - compile the code and create the library.'
	@echo '   install        - install the library and config files.'
	@echo '   clean          - delete compiled files.'
	@echo '   rpm            - build an rpm package file'
	@echo '   deb            - build debian package files'
	@echo '   help           - print this help text.'

# Include dependency files
ifneq ($(DEPENDENCY_FILES),)
-include $(DEPENDENCY_FILES)
endif


##### BASE MAKEFILE
#
# This makefile is intended to be a basis on which new makefiles are written.
# It is divided into five parts:
#  * help text generation
#  * path for subproject locations and thirparty include/library paths
#  * default values for tools (like CXX) and compiler/linker flags
#  * description of known libraries that may be used in the build
#  * functions that define build artifacts
#
# library handling will be described in detail later.
#
# we have three functions that defined build artifacts:
#  * build-executable
#  * build-static-library
#  * build-dynamic-library
#
# they all have the same, basic signature:
#    (name: string, sources: list<string>, usedLibraries: list<string>)
#
# $name determines the name given to the output file (for executables) or the library name given
# to the output library (for libraries).
#
# $sources must be a list of source files (allowed types are .cpp and .c), relative to the directory
# of the Makefile you are writing (see example below).
#
# $usedLibraries is a (possibly empty) list of libraries as defined in the KNOWN LIBRARIES section
# below. they will be automatically added to compiler and linker invocations as needed.
#
# EXAMPLE:
#
# imagine you have a project with a directory structure like this:
#  - /build
#    - Makefile
#  - /source
#    - /lib
#      - ...
#    - /exe
#      - ...
#
# you want your Makefile to build a shared library from /source/lib and an executable from
# /source/exe. the you could write your Makefile like this:
#
# # must be the location of the file you are reading now.
# # the $(or ...) allows for package builds that copy a subproject directory around.
# include $(or $(root-dir),../..)/build/Makefile
#
# # will generate a libFancy.so in /build, uses libz
# $(call build-static-library,\
#     Fancy,\
#     $(shell find ../source/lib -iname '*.cpp'),\
#     z)
#
# # this registers libFancy as a shared library. see details below (KNOWN LIBRARIES).
# $(call define-dep-lib, Fancy, -I ../source/lib/include, -L . -l Fancy)
#
# # will generate an executable file Run in /build, uses libFancy
# $(call build-executable,\
#    Run,\
#    $(shell find ../source/exe -iname '*.cpp'),\
#    Fancy)
#
# # a dependency must be added between Run and Fancy, to ensure that Fancy is built first.
# Run: | libFancy.so

override V := $(if $V,,@)

all:

define HELP_ARGS_GENERIC
	@echo  '  BEEGFS_DEBUG=1             Enables debug information and symbols'
	@echo  '  BEEGFS_DEBUG_OPT=1         Enables internal debug code, but compiled'
	@echo  '                             with optimizations'
	@echo  '  CXX=<compiler>             Specifies a c++ compiler'
	@echo  '  DISTCC=distcc              Enables the usage of distcc'
	@echo  '  V=1                        Print command lines of tool invocations'
	@echo  '  BEEGFS_COMMON_PATH=<path>  Path to the beegfs_common directory'
	@echo  '  BEEGFS_OPENTK_PATH=<path>  Path to the beegfs_opentk_lib directory'
	@echo  '  BEEGFS_THIRDPARTY_PATH=<path>'
	@echo  '                             Path to the beegfs_thirdparty directory'
endef
define HELP_TARGETS_GENERIC
	@echo '  all (default)     build only'
	@echo '  clean             delete compiled files'
	@echo '  help              print this help message'
endef

ifneq ($(CAN_PACKAGE),)
define HELP_TARGETS_PACKAGE
	@echo '  rpm               create an rpm package'
	@echo '  deb               create debian package file'
endef

rpm:
	./make-rpm

deb:
	./make-deb
endif

help:
	@echo 'Optional arguments:'
	$(HELP_ARGS_GENERIC)
	$(HELP_ARGS_SPECIFIC)
	@echo
	@echo 'Targets:'
	$(HELP_TARGETS_GENERIC)
	$(HELP_TARGETS_SPECIFIC)
	$(HELP_TARGETS_PACKAGE)

root_dir := $(realpath $(dir $(lastword $(MAKEFILE_LIST)))/..)

BEEGFS_VERSION_PATH       ?= $(root_dir)/beegfs-version
BEEGFS_COMMON_PATH        ?= $(root_dir)/beegfs_common/
BEEGFS_OPENTK_PATH        ?= $(root_dir)/beegfs_opentk_lib/
BEEGFS_THIRDPARTY_PATH    ?= $(root_dir)/beegfs_thirdparty/
ADMON_GUI_PATH            ?= $(root_dir)/beegfs_admon_gui/
BEEGFS_ONLINE_CFG_PATH    ?= $(root_dir)/beegfs_ctl
BEEGFS_FSCK_PATH          ?= $(root_dir)/beegfs_fsck
BEEGFS_DEVEL_INCLUDE_PATH ?= $(root_dir)/beegfs_client_devel/include
BEEGFS_JAVA_LIB_PATH      ?= $(root_dir)/beegfs_java_lib

SQLITE_INC_PATH   ?= $(BEEGFS_THIRDPARTY_PATH)/source/sqlite
OPENSSL_INC_PATH  ?= $(BEEGFS_THIRDPARTY_PATH)/source/openssl/include
CPPUNIT_INC_PATH  ?= $(BEEGFS_THIRDPARTY_PATH)/source/cppunit_install/usr/local/include
CPPUNIT_LIB_PATH  ?= $(BEEGFS_THIRDPARTY_PATH)/build/
TICPP_INC_PATH    ?= $(BEEGFS_THIRDPARTY_PATH)/source/ticpp
MONGOOSE_INC_PATH ?= $(BEEGFS_THIRDPARTY_PATH)/source/mongoose
BOOST_INC_PATH    ?= $(BEEGFS_THIRDPARTY_PATH)/source/boost/

ifneq ($(target_arch),)
   STRIP := $(target_arch)-strip
   AR := $(target_arch)-ar
   CC := $(target_arch)-gcc
   CXX := $(target_arch)-g++
endif

SHELL := /bin/bash
STRIP ?= strip
CXX   ?= g++
AR    ?= ar

# if -T is supported by ar, use it. thin archives are quicker to create and maintain.
ifeq ($(shell ar -TM 2>&1 <<<""),)
AR += -T
endif

CXXFLAGS = \
   -std=gnu++0x -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 \
   -I ../source/ \
   -isystem $(BOOST_INC_PATH) \
   -pthread \
   -fno-strict-aliasing -fmessage-length=0 \
   -Wall -Wextra -Wunused-variable -Woverloaded-virtual -Wno-unused-parameter -Wuninitialized \
      -Wno-missing-field-initializers \
   -ggdb3 \
   $(USER_CXXFLAGS)
CXXFLAGS_RELEASE = -O3 -Wuninitialized
CXXFLAGS_DEBUG = -O1 -D_FORTIFY_SOURCE=2 \
   -DBEEGFS_DEBUG -DDEBUG_READWRITE -DDEBUG_MUTEX_LOCKING -DDEBUG_REFCOUNT \
   -DDEBUG_BACKTRACE  -DLOG_DEBUG_MESSAGES

LDFLAGS = -std=c++0x \
   -rdynamic \
   -pthread -lrt \
   -Wl,-rpath,'$$ORIGIN/../lib' \
   $(USER_LDFLAGS)

ifeq ($(BEEGFS_DEBUG),)
CXXFLAGS += $(CXXFLAGS_RELEASE)
else
CXXFLAGS += $(CXXFLAGS_DEBUG)
endif


BEEGFS_VERSION := $(shell bash -c "BEEGFS_VERSION=$(BEEGFS_VERSION) \
   $(BEEGFS_VERSION_PATH) --print_version || echo **FAIL**")
BEEGFS_VERSION_CODE := $(shell bash -c "BEEGFS_VERSION=$(BEEGFS_VERSION) \
   $(BEEGFS_VERSION_PATH) --print_version_code || echo **FAIL**")

ifneq ($(filter **FAIL**,$(BEEGFS_VERSION) $(BEEGFS_VERSION_CODE)),)
   $(error failed version check)
endif

$(info [VER]   BEEGFS_VERSION="$(BEEGFS_VERSION)" BEEGFS_VERSION_CODE=$(BEEGFS_VERSION_CODE))

CXXFLAGS += \
   -DBEEGFS_VERSION="\"$(BEEGFS_VERSION)\"" \
   -DBEEGFS_VERSION_CODE=$(BEEGFS_VERSION_CODE)


###### KNOWN LIBRARIES
#
# this is our local registry of known libraries, both our own and thirdparty libraries.
#
# currently supports building with our own libraries common, opentk, and client-devel.
# thirdparty packages supported are:
#  * dl (from system)
#  * sqlite (from thirdparty, or from system if not built in thirdparty)
#  * openssl (from thirdparty, or from system if not built in thirdparty)
#  * ticpp (from thirdparty)
#  * mongoose (from thirdparty)
#  * cppunit (from thirdparty, or from system if not built in thirdparty)
#
# to define new libraries, add appriopriate calls to define-dep-lib in this section.
# the signature of define-dep-lib is:
#    (name: string, CXXFLAGS: string, libraryPath: string[, LDFLAGS: string])
#
# $name is used to identify the library within this registry, and is otherwise unused.
#
# $CXXFLAGS will be added to the set of CXXFLAGS used when compiling files from projects that use
# this library (see example at the beginning of this file for reference).
#
# $libraryPath is the path (absolute or relative) to the library. glob patterns are allowed, and in
# fact required if you want to link a dynamic library.
#
# $LDFLAGS will be added to the set of LDFLAGS used when linking shared libraries or executables
# that use this library. if the library you want to link is static, LDFLAGS need not be set - the
# library will be added to the list of archives.
#
#
# to define libraries that are taken from the system, use define-dep-lib-system. it will use its
# arguments to ask pkgconfig for the correct compiler and linker flags. then signature of
# define-dep-lib-system is:
#    (name: string, pkgconfigID: string)
#
# $name is used only to identify the library.
#
# $pkgconfigID is the identifier pkgconfig will be asked about.
define resolve-dep-cflags
$(if $(filter undefined,$(origin _DEP_LIB_CXX[$(strip $1)])),\
   $(error I have no CXXFLAGS for $1!),\
   $(_DEP_LIB_CXX[$(strip $1)]))
endef
define resolve-dep-ldflags
$(if $(filter undefined,$(origin _DEP_LIB_LD[$(strip $1)])),\
   $(error I have no LDFLAGS for $1!),\
   $(_DEP_LIB_LD[$(strip $1)]))
endef
define resolve-dep-deps
$(if $(filter undefined,$(origin _DEP_LIB_DEPS[$(strip $1)])),\
   $(error I have no library dependencies for $1!),\
   $(_DEP_LIB_DEPS[$(strip $1)]))
endef
define define-dep-lib
$(strip
   $(eval _DEP_LIB_CXX[$(strip $1)] = $(strip $2))
   $(eval _DEP_LIB_LD[$(strip $1)] = $(strip $(or $4, $3)))
   $(eval _DEP_LIB_DEPS[$(strip $1)] = $(strip $3))
   $(eval $(strip $3): ))
endef
define define-dep-lib-system
$(strip
   $(if $(shell pkg-config --exists $2 || echo fail),
      $(error Could not find library $2 in system!))
   $(call define-dep-lib,
      $1,
      $(shell pkg-config --cflags $2),
      $(shell pkg-config --libs $2)))
endef

# don't bother to search the system for libraries if we only run `make clean'
# this also has the nice side effect that libraries needn't be present in the system
# when `make clean' is run - if we override dependency resolution to not do anything.
ifeq ($(strip $(MAKECMDGOALS)),clean)
   resolve-dep-cflags :=
   resolve-dep-ldflags :=
   resolve-dep-deps :=
else
   $(call define-dep-lib, opentk,\
      -I $(BEEGFS_OPENTK_PATH)/include,\
      $(BEEGFS_OPENTK_PATH)/build/libbeegfs-opentk.*,\
      -L $(BEEGFS_OPENTK_PATH)/build/ -lbeegfs-opentk)

   $(call define-dep-lib, opentk-static,\
      -I $(BEEGFS_OPENTK_PATH)/include,\
      $(BEEGFS_OPENTK_PATH)/build/libbeegfs-opentk.a)

   $(call define-dep-lib, common,\
      -I $(BEEGFS_COMMON_PATH)/source,\
      $(BEEGFS_COMMON_PATH)/build/libbeegfs-common.a)

   $(call define-dep-lib, client-devel,\
      -I $(BEEGFS_DEVEL_INCLUDE_PATH),)

   $(call define-dep-lib, dl, , , -ldl)

   $(call define-dep-lib, ticpp,\
      -isystem $(TICPP_INC_PATH),\
      $(BEEGFS_THIRDPARTY_PATH)/build/libticpp.a)

   $(call define-dep-lib, mongoose,\
      -isystem $(MONGOOSE_INC_PATH),\
      $(BEEGFS_THIRDPARTY_PATH)/build/libmongoose.a)

   ifneq ($(realpath $(CPPUNIT_LIB_PATH)/libcppunit.a),)
      $(info [NOTE] Using packaged libcppunit)
      $(call define-dep-lib, cppunit,\
         -isystem $(CPPUNIT_INC_PATH),\
         $(CPPUNIT_LIB_PATH)/libcppunit.a)
   else
      $(call define-dep-lib-system, cppunit, cppunit)
   endif

   ifneq ($(realpath $(BEEGFS_THIRDPARTY_PATH)/build/libsqlite3.a),)
      $(info [NOTE] Using packaged sqlite3)
      $(call define-dep-lib, sqlite,\
         -isystem $(SQLITE_INC_PATH),\
         $(BEEGFS_THIRDPARTY_PATH)/build/libsqlite3.a)
   else
      $(call define-dep-lib-system, sqlite, sqlite3)
   endif

   ifneq ($(realpath $(BEEGFS_THIRDPARTY_PATH)/build/libssl.a),)
      $(info [NOTE] Using packaged OpenSSL)
      $(call define-dep-lib, openssl,\
         -isystem $(OPENSSL_INC_PATH),\
         $(BEEGFS_THIRDPARTY_PATH)/build/libssl.a $(BEEGFS_THIRDPARTY_PATH)/build/libcrypto.a)
   else
      $(call define-dep-lib-system, openssl, libssl libcrypto)
   endif
endif


clean:
	@$(RM) -rf $(if $V,,-v) $(CLEANUP_FILES)


NONDEP_MAKEFILES = $(filter-out %.d,$(MAKEFILE_LIST))

%.cpp.o: %.cpp $(NONDEP_MAKEFILES)
	@echo "[CXX]   $@"
	$V$(CXX) $(CXXFLAGS) -c $< -E -MMD -MP -MF$<.d -MT$@ -o/dev/null
	$V$(DISTCC) $(CXX) $(CXXFLAGS) -o$@ -c $(realpath $<)

%.c.o: %.c $(NONDEP_MAKEFILES)
	@echo "[CC]    $@"
	$V$(CXX) $(CXXFLAGS) -c $< -E -MMD -MP -MF$<.d -MT$@ -o/dev/null
	$V$(DISTCC) $(CXX) $(CXXFLAGS) -o$@ -c $(realpath $<)


# build-executable
#
# define a new executable for the build
# arguments:
#  #1: name of the executable
#  #2: sources
#  #3: required libraries
build-executable = $(eval $(call -build-executable-fragment,$(strip $1),$2,$3))
define -build-executable-fragment
all: $1

CLEANUP_FILES += $1 $(addsuffix .o,$2) $(addsuffix .d,$2)

$(addsuffix .o,$2): CXXFLAGS += \
   $(foreach lib,$3,$(call resolve-dep-cflags,$(lib)))

$1: LDFLAGS += \
   -Wl,--start-group $(foreach lib,$3,$(call resolve-dep-ldflags,$(lib))) -Wl,--end-group

$1: $(addsuffix .o,$2) $(foreach lib,$3,$(call resolve-dep-deps,$(lib)))
	@echo "[LD]	$$@"
	$$V$$(CXX) -o $$@ $(addsuffix .o,$2) $$(LDFLAGS)

-include $(addsuffix .d,$2)
endef

# build-static-library
#
# define a new (static) library for the build
# arguments:
#  #1: name of the library
#  #2: sources
#  #3: required libraries
build-static-library = $(eval $(call -build-static-library-fragment,lib$(strip $1).a,$2,$3))
define -build-static-library-fragment
all: $1

CLEANUP_FILES += $1 $(addsuffix .o,$2) $(addsuffix .d,$2)

$(addsuffix .o,$2): CXXFLAGS += \
   $(foreach lib,$3,$(call resolve-dep-cflags,$(lib)))

$1: $(addsuffix .o,$2)
	@echo "[AR]	$$@"
	@rm -f $$@
	$$V$(AR) -rcs $$@ $$^

-include $(addsuffix .d,$2)
endef

# build-shared-library
#
# define a new (shared) library for the build
# arguments:
#  #1: name of the library
#  #2: sources
#  #3: required libraries
build-shared-library = $(eval $(call -build-shared-library-fragment,lib$(strip $1).so,$2,$3))
define -build-shared-library-fragment
all: $1

CLEANUP_FILES += $1 $(addsuffix .o,$2) $(addsuffix .d,$2)

$(addsuffix .o,$2): CXXFLAGS += \
   -fPIC $(foreach lib,$3,$(call resolve-dep-cflags,$(lib)))

$1: LDFLAGS += \
  -Wl,--start-group $(foreach lib,$3,$(call resolve-dep-ldflags,$(lib))) -Wl,--end-group

$1: $(addsuffix .o,$2) $(foreach lib,$3,$(call resolve-dep-deps,$(lib)))
	@echo "[LD]	$$@"
	$$V$$(CXX) -shared -o $$@ $(addsuffix .o,$2) \
		-Wl,--whole-archive $$(LDFLAGS) -Wl,--no-whole-archive

-include $(addsuffix .d,$2)
endef

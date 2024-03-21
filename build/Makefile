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
# include ,../../build/Makefile
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
	@echo  '  BEEGFS_DEBUG_IP=1          Enables low-level debug of network sendto'
	@echo  '                             and recvfrom'
	@echo  '  CXX=<compiler>             Specifies a c++ compiler'
	@echo  '  DISTCC=distcc              Enables the usage of distcc'
	@echo  '  V=1                        Print command lines of tool invocations'
	@echo  '  BEEGFS_COMMON_PATH=<path>  Path to the common directory'
	@echo  '  BEEGFS_THIRDPARTY_PATH=<path>'
	@echo  '                             Path to the thirdparty directory'
endef
define HELP_TARGETS_GENERIC
	@echo '  all (default)     build only'
	@echo '  clean             delete compiled files'
	@echo '  help              print this help message'
endef

help:
	@echo 'Optional arguments:'
	$(HELP_ARGS_GENERIC)
	$(HELP_ARGS_SPECIFIC)
	@echo
	@echo 'Targets:'
	$(HELP_TARGETS_GENERIC)
	$(HELP_TARGETS_SPECIFIC)

root_dir := $(realpath $(dir $(lastword $(MAKEFILE_LIST)))/..)
build_dir := $(realpath $(dir $(firstword $(MAKEFILE_LIST))))

BEEGFS_COMMON_PATH             ?= $(root_dir)/common
BEEGFS_THIRDPARTY_PATH         ?= $(root_dir)/thirdparty
BEEGFS_ONLINE_CFG_PATH         ?= $(root_dir)/ctl
BEEGFS_COMM_DEBUG_PATH         ?= $(root_dir)/comm_debug
BEEGFS_FSCK_PATH               ?= $(root_dir)/fsck
BEEGFS_EVENT_LISTENER_PATH     ?= $(root_dir)/event_listener
BEEGFS_DEVEL_INCLUDE_PATH      ?= $(root_dir)/client_devel/include
BEEGFS_JAVA_LIB_PATH           ?= $(root_dir)/java_lib
BEEGFS_CLIENT_PATH             ?= $(root_dir)/client_module/include

BOOST_INC_PATH    ?= $(BEEGFS_THIRDPARTY_PATH)/source/boost
NU_INC_PATH       ?= $(BEEGFS_THIRDPARTY_PATH)/source/nu/include

GTEST_INC_PATH    ?= $(BEEGFS_THIRDPARTY_PATH)/source/gtest/googletest/include
GTEST_LIB_PATH    ?= $(BEEGFS_THIRDPARTY_PATH)/build

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
CLANG_TIDY ?= clang-tidy

# if -T is supported by ar, use it. thin archives are quicker to create and maintain.
ifeq ($(shell ar -TM 2>&1 <<<""),)
AR += -T
endif

CXXFLAGS = \
   -std=c++17 -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 \
   -isystem $(BOOST_INC_PATH) -isystem $(NU_INC_PATH) \
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
CXXFLAGS_COVERAGE = --coverage -O1 -fno-inline

LDFLAGS = -std=c++1y \
   -rdynamic \
   -pthread -lrt \
   -Wl,-rpath,'$$ORIGIN/../lib' \
   $(USER_LDFLAGS)
LDFLAGS_COVERAGE = --coverage

ifneq ($(BEEGFS_DEBUG),)
CXXFLAGS += $(CXXFLAGS_DEBUG)
else ifneq ($(BEEGFS_COVERAGE),)
CXXFLAGS += $(CXXFLAGS_COVERAGE)
LDFLAGS += $(LDFLAGS_COVERAGE)
else
CXXFLAGS += $(CXXFLAGS_RELEASE)
endif


CXXFLAGS += -DBEEGFS_VERSION="\"$(BEEGFS_VERSION)\""

ifeq ($(BEEGFS_NVFS),1)
CXXFLAGS += -DBEEGFS_NVFS
endif

ifeq ($(BEEGFS_DEBUG_RDMA),1)
CXXFLAGS += -DBEEGFS_DEBUG_RDMA
endif

ifneq ($(BEEGFS_DEBUG_IP),)
CXXFLAGS += -DBEEGFS_DEBUG_IP
endif

###### KNOWN LIBRARIES
#
# this is our local registry of known libraries, both our own and thirdparty libraries.
#
# currently supports building with our own libraries common and client-devel.
# thirdparty packages supported are:
#  * dl (from system)
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
   $(call define-dep-lib, common,\
      -I $(BEEGFS_COMMON_PATH)/source \
      -I $(BEEGFS_CLIENT_PATH), \
      -ldl $(BEEGFS_COMMON_PATH)/build/libbeegfs-common.a)

   $(call define-dep-lib, client-devel,\
      -I $(BEEGFS_DEVEL_INCLUDE_PATH),)

   $(call define-dep-lib, dl, , , -ldl)

   $(call define-dep-lib, rdma, , -lrdmacm -libverbs)

   $(call define-dep-lib, gtest,\
      -isystem $(GTEST_INC_PATH)/include,\
      $(GTEST_LIB_PATH)/libgtest.a)

   $(call define-dep-lib, cassandra, -I$(BEEGFS_THIRDPARTY_PATH)/source/datastax)

   $(call define-dep-lib-system, curl, libcurl)  
   $(call define-dep-lib-system, z, zlib)

   $(call define-dep-lib, blkid, , , -lblkid)
   $(call define-dep-lib, uuid, , , -luuid)
   $(call define-dep-lib-system, nl3-route, libnl-route-3.0)
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

# first partial list: checks we want to exclude on a general basis right now
# second list: very minor problems we don't want to be bugged about yet, but want to fix over time
# third list: definite errors, fix as soon as possible
define -clang_tidy_checks
-clang-analyzer-alpha.deadcode.UnreachableCode
-fuchsia-default-arguments
-fuchsia-overloaded-operator
-google-build-using-namespace
-google-readability-braces-around-statements
-google-readability-casting
-google-readability-namespace-comments
-hicpp-braces-around-statements
-llvm-header-guard
-llvm-include-order
-llvm-namespace-comment
-readability-braces-around-statements
-readability-implicit-bool-conversion

-android-cloexec-accept
-android-cloexec-creat
-android-cloexec-dup
-android-cloexec-epoll-create
-android-cloexec-fopen
-android-cloexec-open
-boost-use-to-string
-bugprone-branch-clone
-bugprone-exception-escape
-bugprone-narrowing-conversions
-bugprone-sizeof-expression
-cert-oop54-cpp
-clang-analyzer-core.CallAndMessage
-clang-analyzer-core.uninitialized.Assign
-clang-analyzer-cplusplus.NewDeleteLeaks
-clang-analyzer-deadcode.DeadStores
-clang-analyzer-optin.cplusplus.UninitializedObject
-clang-diagnostic-deprecated-copy
-cppcoreguidelines-avoid-c-arrays
-cppcoreguidelines-avoid-goto
-cppcoreguidelines-avoid-magic-numbers
-cppcoreguidelines-init-variables
-cppcoreguidelines-interfaces-global-init
-cppcoreguidelines-macro-usage
-cppcoreguidelines-narrowing-conversions
-cppcoreguidelines-non-private-member-variables-in-classes
-cppcoreguidelines-pro-bounds-array-to-pointer-decay
-cppcoreguidelines-pro-bounds-constant-array-index
-cppcoreguidelines-pro-bounds-pointer-arithmetic
-cppcoreguidelines-pro-type-const-cast
-cppcoreguidelines-pro-type-cstyle-cast
-cppcoreguidelines-pro-type-member-init
-cppcoreguidelines-pro-type-reinterpret-cast
-cppcoreguidelines-pro-type-static-cast-downcast
-cppcoreguidelines-pro-type-union-access
-cppcoreguidelines-pro-type-vararg
-cppcoreguidelines-special-member-functions
-fuchsia-default-arguments-calls
-fuchsia-default-arguments-declarations
-fuchsia-statically-constructed-objects
-google-default-arguments
-google-explicit-constructor
-google-readability-avoid-underscore-in-googletest-name
-google-readability-redundant-smartptr-get
-google-readability-todo
-google-runtime-int
-google-runtime-references
-hicpp-avoid-c-arrays
-hicpp-avoid-goto
-hicpp-deprecated-headers
-hicpp-explicit-conversions
-hicpp-member-init
-hicpp-multiway-paths-covered
-hicpp-no-array-decay
-hicpp-no-assembler
-hicpp-no-malloc
-hicpp-signed-bitwise
-hicpp-special-member-functions
-hicpp-uppercase-literal-suffix
-hicpp-use-auto
-hicpp-use-emplace
-hicpp-use-equals-default
-hicpp-use-equals-delete
-hicpp-use-noexcept
-hicpp-use-nullptr
-hicpp-vararg
-llvm-qualified-auto
-misc-misplaced-widening-cast
-misc-move-constructor-init
-misc-non-private-member-variables-in-classes
-misc-redundant-expression
-misc-string-compare
-misc-unused-parameters
-modernize-avoid-bind
-modernize-avoid-c-arrays
-modernize-loop-convert
-modernize-make-shared
-modernize-make-unique
-modernize-pass-by-value
-modernize-raw-string-literal
-modernize-replace-random-shuffle
-modernize-return-braced-init-list
-modernize-use-auto
-modernize-use-default
-modernize-use-default-member-init
-modernize-use-emplace
-modernize-use-equals-default
-modernize-use-equals-delete
-modernize-use-noexcept
-modernize-use-nullptr
-modernize-use-trailing-return-type
-modernize-use-using
-performance-faster-string-find
-performance-inefficient-string-concatenation
-performance-unnecessary-copy-initialization
-performance-unnecessary-value-param
-readability-const-return-type
-readability-convert-member-functions-to-static
-readability-else-after-return
-readability-implicit-bool-cast
-readability-isolate-declaration
-readability-magic-numbers
-readability-make-member-function-const
-readability-named-parameter
-readability-non-const-parameter
-readability-qualified-auto
-readability-redundant-control-flow
-readability-redundant-smartptr-get
-readability-redundant-string-cstr
-readability-string-compare
-readability-uppercase-literal-suffix

-cert-err58-cpp
-cert-err60-cpp
-cert-msc30-c
-cert-msc50-cpp
-clang-analyzer-alpha.cplusplus.VirtualCall
-clang-analyzer-core.DivideZero
-clang-analyzer-optin.cplusplus.VirtualCall
-cppcoreguidelines-no-malloc
-cppcoreguidelines-owning-memory
-readability-misleading-indentation
endef
clang_tidy_checks := *,$(shell echo $(strip $(-clang_tidy_checks)) | tr ' ' ',')

# make-tidy-rule
#
# add a rule to invoke clang-tidy for the given source files
# arguments:
#  #1: identifier for library/executable
#  #2: source files
#  #3: include paths and defines
make-tidy-rule = $(eval $(call -make-tidy-rule,$(strip $1),$2,$3))
define -specific-tidy-rule
$1:
	@echo "[TIDY]  $2"
	$V$(CLANG_TIDY) -checks='$(clang_tidy_checks)' -quiet $2 -- $$(CXXFLAGS) $3
endef
define -make-tidy-rule
.PHONY: tidy
tidy: tidy-$1

.PHONY: tidy-$1 $(foreach file,$2,$(file).tidy-$1)
tidy-$1: $(foreach file,$2,$(file).tidy-$1)

$(foreach file,$2,$(eval $(call -specific-tidy-rule,$(file).tidy-$1,$(file),$3)))
endef

# build-executable
#
# define a new executable for the build
# arguments:
#  #1: name of the executable
#  #2: sources
#  #3: required libraries
#  #4: include directories
build-executable = $(eval $(call -build-executable-fragment,$(strip $1),$2,$3,$4))
define -build-executable-fragment
all: $1

CLEANUP_FILES += $1 $(addsuffix .o,$2) $(addsuffix .d,$2)

$(addsuffix .o .tidy-%,$2): CXXFLAGS += \
   $(foreach lib,$3,$(call resolve-dep-cflags,$(lib))) $(addprefix -I, $4)

$1: LDFLAGS += \
   -Wl,--start-group $(foreach lib,$3,$(call resolve-dep-ldflags,$(lib))) -Wl,--end-group

$1: $(addsuffix .o,$2) $(foreach lib,$3,$(call resolve-dep-deps,$(lib)))
	@echo "[LD]	$$@"
	$$V$$(CXX) -o $$@ $(addsuffix .o,$2) $$(LDFLAGS)

-include $(addsuffix .d,$2)

$(call make-tidy-rule, $1, $2,\
   $(foreach lib,$3,$(call resolve-dep-cflags,$(lib))) $(addprefix -I, $4))
endef

# build-test
#
# build a test runner from specified test files. acts much like build-executable, except that it
# includes gtest in the build as a non-library .a
#  #1: name if the test executable
#  #2: sources
#  #3: required libraries (not including gtest)
#  #4: included directories (not including gtest)
define build-test
$(call build-executable, $1, $2, $3, $4 -isystem $(GTEST_INC_PATH))

$1: LDFLAGS += $(GTEST_LIB_PATH)/libgtest.a
endef

# build-static-library
#
# define a new (static) library for the build
# arguments:
#  #1: name of the library
#  #2: sources
#  #3: required libraries
#  #4: include directories
build-static-library = $(eval $(call -build-static-library-fragment,lib$(strip $1).a,$2,$3,$4))
define -build-static-library-fragment
all: $1

CLEANUP_FILES += $1 $(addsuffix .o,$2) $(addsuffix .d,$2)

$(addsuffix .o .tidy-%,$2): CXXFLAGS += \
   $(foreach lib,$3,$(call resolve-dep-cflags,$(lib))) $(addprefix -I, $4)

$(build_dir)/$1: $(addsuffix .o,$2)
	@echo "[AR]	$1"
	@rm -f $$@
	$$V$(AR) -rcs $$@ $$^

$1: $(build_dir)/$1

-include $(addsuffix .d,$2)

$(call make-tidy-rule, $1, $2,\
   $(foreach lib,$3,$(call resolve-dep-cflags,$(lib))) $(addprefix -I, $4))
endef

# build-shared-library
#
# define a new (shared) library for the build
# arguments:
#  #1: name of the library
#  #2: sources
#  #3: required libraries
#  #4: include directories
build-shared-library = $(eval $(call -build-shared-library-fragment,lib$(strip $1).so,$2,$3,$4))
define -build-shared-library-fragment
all: $1

CLEANUP_FILES += $1 $(addsuffix .o,$2) $(addsuffix .d,$2)

$(addsuffix .o .tidy-%,$2): CXXFLAGS += \
   -fPIC $(foreach lib,$3,$(call resolve-dep-cflags,$(lib))) $(addprefix -I, $4)

$1: LDFLAGS += \
  -Wl,--start-group $(foreach lib,$3,$(call resolve-dep-ldflags,$(lib))) -Wl,--end-group

$(build_dir)/$1: $(addsuffix .o,$2) $(foreach lib,$3,$(call resolve-dep-deps,$(lib)))
	@echo "[LD]	$1"
	$$V$$(CXX) -shared -o $$@ $(addsuffix .o,$2) \
		-Wl,--whole-archive $$(LDFLAGS) -Wl,--no-whole-archive

$1: $(build_dir)/$1

-include $(addsuffix .d,$2)

$(call make-tidy-rule, $1, $2,\
   $(foreach lib,$3,$(call resolve-dep-cflags,$(lib))) $(addprefix -I, $4))
endef

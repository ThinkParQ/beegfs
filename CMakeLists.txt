cmake_minimum_required(VERSION 3.7)


project(
	BeeGFS
	LANGUAGES CXX C
)

set(BEEGFS_VERSION "" CACHE STRING "Defaults to current git version.")
if(BEEGFS_VERSION STREQUAL "")
	execute_process(
		COMMAND git describe --match *.* --abbrev=10
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		OUTPUT_VARIABLE BEEGFS_VERSION_FROM_GIT
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	if(BEEGFS_VERSION_FROM_GIT STREQUAL "")
		message(FATAL_ERROR "Cannot determine BeeGFS version. Specify with `cmake -DBEEGFS_VERSION=...`")
	endif()

	set(BEEGFS_VERSION ${BEEGFS_VERSION_FROM_GIT} CACHE STRING "Defaults to current git version" FORCE)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBEEGFS_VERSION=\\\"${BEEGFS_VERSION}\\\"")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DBEEGFS_VERSION=\\\"${BEEGFS_VERSION}\\\"")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wunused-variable -Woverloaded-virtual -Wno-unused-parameter -Wuninitialized -Wno-missing-field-initializers")

set(BEEGFS_DEBUG OFF CACHE BOOL "Build with debug information.")
if(BEEGFS_DEBUG)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBEEGFS_DEBUG=1 -Werror")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DBEEGFS_DEBUG=1")
endif()

set(BEEGFS_INSTRUMENTATION "" CACHE STRING "Instrumentation for testing.")
if(BEEGFS_INSTRUMENTATION STREQUAL "")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
elseif(BEEGFS_INSTRUMENTATION STREQUAL "coverage")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -O0")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage -O0")
elseif(BEEGFS_INSTRUMENTATION STREQUAL "address")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -O0")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")
elseif(BEEGFS_INSTRUMENTATION STREQUAL "thread")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -O0")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")
elseif(BEEGFS_INSTRUMENTATION STREQUAL "undefined")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=undefined")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -O0")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")
elseif(BEEGFS_INSTRUMENTATION STREQUAL "iwyu")
	if(NOT DEFINED CMAKE_CXX_INCLUDE_WHAT_YOU_USE)
		set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE "include-what-you-use")
	endif()
	list(APPEND CMAKE_CXX_INCLUDE_WHAT_YOU_USE "-Xiwyu" "--mapping_file=${CMAKE_SOURCE_DIR}/iwyu-mappings.imp")
else()
	message(FATAL_ERROR "Invalid instrumentation.")
endif()

set(BEEGFS_SKIP_TESTS OFF CACHE BOOL "Skip building and running tests.")
set(BEEGFS_SKIP_CLIENT OFF CACHE BOOL "Skip building the kernel module.")
set(BEEGFS_KERNELDIR "" CACHE PATH "Path to kernel for kernel module (optional).")
set(BEEGFS_OFEDDIR "" CACHE PATH "Path to OFED for kernel module (optional).")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb3")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ggdb3")

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lrt")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(common/source)
include_directories(SYSTEM thirdparty/source/boost)
include_directories(SYSTEM thirdparty/source/nu/include)
include_directories(thirdparty/source/gtest/googletest/include)

if(NOT BEEGFS_SKIP_TESTS)
	enable_testing()
	option(INSTALL_GMOCK OFF)
	option(INSTALL_GTEST OFF)
	add_subdirectory("thirdparty/source/gtest")
endif()

set(CMAKE_INSTALL_PREFIX "/")

add_subdirectory("beeond")
add_subdirectory("beeond_thirdparty_gpl")
# add_subdirectory("client_devel")
# add_subdirectory("client_module")
add_subdirectory("common")
add_subdirectory("ctl")
add_subdirectory("event_listener")
add_subdirectory("fsck")
add_subdirectory("helperd")
add_subdirectory("meta")
add_subdirectory("mon")
add_subdirectory("mgmtd")
add_subdirectory("storage")
add_subdirectory("upgrade")
add_subdirectory("utils")

add_custom_target(
	dkms-install
	COMMAND dkms install "beegfs/${BEEGFS_VERSION}"
)

add_custom_target(
	dkms-uninstall
	COMMAND dkms remove "beegfs/${BEEGFS_VERSION}" --all
)


### Packaging settings ###

set(CPACK_PACKAGE_CONTACT "BeeGFS Maintainers <packages@beegfs.com>")
set(CPACK_PACKAGE_VENDOR "ThinkparQ GmbH")

string(REGEX REPLACE "^([^.]+)\\.([^-]+)-([^-]+)(-.*)?$" "\\1" CPACK_PACKAGE_VERSION_MAJOR "${BEEGFS_VERSION}")
string(REGEX REPLACE "^([^.]+)\\.([^-]+)-([^-]+)(-.*)?$" "\\2" CPACK_PACKAGE_VERSION_MINOR "${BEEGFS_VERSION}")
string(REGEX REPLACE "^([^.]+)\\.([^-]+)-([^-]+)(-.*)?$" "\\3" CPACK_PACKAGE_VERSION_PATCH "${BEEGFS_VERSION}")

set(CPACK_PACKAGING_INSTALL_PREFIX "/")

# silence cpack warnings about non relocatable package.
set(CPACK_PACKAGE_RELOCATABLE OFF)
set(CPACK_GENERATOR "DEB" "RPM")

# enable creation of separate packages
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_RPM_COMPONENT_INSTALL ON)

set(CPACK_STRIP_FILES OFF)

# cpack tries to create these directories with nonstandard /
# conflicting permissions in some versions. solve by assuming that
# these directories already exist.
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION "/sbin;/usr/sbin")

include(CPack)

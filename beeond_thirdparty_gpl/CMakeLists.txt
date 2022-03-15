include(ExternalProject)

set(parallel_version 20150122)

ExternalProject_Add(
	parallel
	DOWNLOAD_COMMAND tar xf ${CMAKE_CURRENT_SOURCE_DIR}/source/parallel-${parallel_version}.tar.bz2
	CONFIGURE_COMMAND ../parallel-${parallel_version}/configure
	BUILD_COMMAND make
	INSTALL_COMMAND ""
)

install(
	PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/parallel-prefix/src/parallel-${parallel_version}/src/parallel"
	DESTINATION "usr/share/beeond/parallel"
	COMPONENT "beeond-thirdparty-gpl"
)

install(
	FILES "${CMAKE_CURRENT_BINARY_DIR}/parallel-prefix/src/parallel-${parallel_version}/COPYING"
	DESTINATION "usr/share/beeond/parallel"
	COMPONENT "beeond-thirdparty-gpl"
)

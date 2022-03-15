if(NOT BEEGFS_SKIP_CLIENT)
	include(ExternalProject)

	ExternalProject_Add(
		client-module
		BUILD_IN_SOURCE ON
		URL "${CMAKE_CURRENT_SOURCE_DIR}"
		CONFIGURE_COMMAND ""
		BUILD_COMMAND make -C build -j $(nproc) "KDIR=${BEEGFS_KERNELDIR}" "OFED_INCLUDE_PATH=${BEEGFS_OFEDDIR}"
		INSTALL_COMMAND ""
	)
endif()

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/build/dkms.conf.client"
	"${CMAKE_CURRENT_BINARY_DIR}/dkms.conf.client"
)

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/build/dkms.conf.compat"
	"${CMAKE_CURRENT_BINARY_DIR}/dkms.conf.compat"
)

configure_file(

	"${CMAKE_CURRENT_SOURCE_DIR}/build/postinst.in"
	"${CMAKE_CURRENT_BINARY_DIR}/postinst"
)

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/build/prerm.in"
	"${CMAKE_CURRENT_BINARY_DIR}/prerm"
)

install(
	DIRECTORY ""
	DESTINATION "usr/src/beegfs-${BEEGFS_VERSION}"
	COMPONENT "client"
	USE_SOURCE_PERMISSIONS
	PATTERN "CMakeLists.txt" EXCLUDE
	PATTERN "dkms.conf.*" EXCLUDE
)

install(
	FILES "${CMAKE_CURRENT_BINARY_DIR}/dkms.conf.client"
	DESTINATION "usr/src/beegfs-${BEEGFS_VERSION}"
	RENAME "dkms.conf"
	COMPONENT "client"
)

install(
	FILES "build/dist/etc/beegfs-client.conf"
	DESTINATION "etc/beegfs"
	COMPONENT "client"
)

install(
	DIRECTORY ""
	DESTINATION "usr/src/beegfs-compat-${BEEGFS_VERSION}"
	COMPONENT "client-compat"
	USE_SOURCE_PERMISSIONS
	PATTERN "CMakeLists.txt" EXCLUDE
	PATTERN "dkms.conf.*" EXCLUDE
)

install(
	FILES "${CMAKE_CURRENT_BINARY_DIR}/dkms.conf.compat"
	DESTINATION "usr/src/beegfs-compat-${BEEGFS_VERSION}"
	RENAME "dkms.conf"
	COMPONENT "client-compat"
)

# Debian package settings
set(CPACK_DEBIAN_CLIENT_PACKAGE_DEPENDS "dkms" PARENT_SCOPE)

set(
	CPACK_DEBIAN_CLIENT_PACKAGE_CONTROL_EXTRA
	"${CMAKE_CURRENT_BINARY_DIR}/prerm;${CMAKE_CURRENT_BINARY_DIR}/postinst;${CMAKE_CURRENT_BINARY_DIR}/dkms.conf.client"
	PARENT_SCOPE
)

# currently no dkms in compat package
# set(
# 	CPACK_DEBIAN_CLIENT-COMPAT_PACKAGE_CONTROL_EXTRA
# 	"${CMAKE_CURRENT_BINARY_DIR}/dkms.conf.compat"
# 	PARENT_SCOPE
# )

# RPM package settings
set(CPACK_RPM_CLIENT_PACKAGE_REQUIRES "dkms" PARENT_SCOPE)

set(
	CPACK_RPM_PRE_UNINSTALL_SCRIPT_FILE
	"${CMAKE_CURRENT_BINARY_DIR}/prerm"
	PARENT_SCOPE
)

set(
	CPACK_RPM_POST_INSTALL_SCRIPT_FILE
	"${CMAKE_CURRENT_BINARY_DIR}/postinst"
	PARENT_SCOPE
)

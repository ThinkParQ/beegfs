install(
	FILES "include/beegfs/beegfs.h" "include/beegfs/beegfs_ioctl.h" "include/beegfs/beegfs_ioctl_functions.h"
	DESTINATION "usr/include/beegfs"
	COMPONENT "client-devel"
)

install(
	FILES "build/dist/usr/share/doc/beegfs-client-devel/examples/createFileWithStripePattern.cpp"
	DESTINATION "usr/share/doc/beegfs/examples/createFileWithStripePattern"
	COMPONENT "client-devel"
)

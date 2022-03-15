install(
	FILES "scripts/etc/bash_completion.d/beegfs-ctl"
	DESTINATION "etc/bash_completion.d"
	COMPONENT "utils"
)

install(
	PROGRAMS "scripts/beegfs-check-servers" "scripts/beegfs-df" "scripts/beegfs-net"
	DESTINATION "usr/bin"
	COMPONENT "utils"
)

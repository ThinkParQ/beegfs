install(
	PROGRAMS "beegfs-ondemand-stoplocal" "beeond-lib"
	DESTINATION "usr/share/beeond"
	COMPONENT "beeond"
)

install(
	PROGRAMS "beeond" "beeond-cp"
	DESTINATION "usr/bin"
	COMPONENT "beeond"
)

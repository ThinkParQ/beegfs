include_directories(
	source
)

add_executable(
	beegfs-mirror-md
	source/beegfs-mirror-md.cpp
)

target_link_libraries(
	beegfs-mirror-md
	beegfs-common
	pthread
)

install(
	TARGETS "beegfs-mirror-md"
	DESTINATION "usr/sbin"
	COMPONENT "beegfs-mirror-md"
)

include_directories(
	include
)

add_executable(
	beegfs-event-listener
	source/beegfs-event-listener.cpp
	include/beegfs/beegfs_file_event_log.hpp
)

install(
	TARGETS beegfs-event-listener
	DESTINATION "usr/sbin"
	COMPONENT "event-listener"
)

install(
	FILES "include/beegfs/beegfs_file_event_log.hpp"
	DESTINATION "usr/include/beegfs"
	COMPONENT "event-listener"
)


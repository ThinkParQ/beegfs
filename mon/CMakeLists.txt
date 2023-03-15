include_directories(
	source
)

add_library(
	mon STATIC
	./source/exception/CurlException.h
	./source/exception/DatabaseException.h
	./source/net/message/NetMessageFactory.h
	./source/net/message/NetMessageFactory.cpp
	./source/net/message/nodes/HeartbeatMsgEx.h
	./source/components/NodeListRequestor.cpp
	./source/components/StatsCollector.h
	./source/components/StatsCollector.cpp
	./source/components/NodeListRequestor.h
	./source/components/worker/GetNodesWork.cpp
	./source/components/worker/RequestMetaDataWork.cpp
	./source/components/worker/RequestStorageDataWork.cpp
	./source/components/worker/RequestStorageDataWork.h
	./source/components/worker/RequestMetaDataWork.h
	./source/components/worker/GetNodesWork.h
	./source/components/CleanUp.cpp
	./source/components/CleanUp.h
	./source/app/Config.h
	./source/app/App.h
	./source/app/Config.cpp
	./source/app/App.cpp
	./source/app/SignalHandler.cpp
	./source/app/SignalHandler.h
	./source/app/Main.cpp
	./source/misc/CurlWrapper.cpp
	./source/misc/InfluxDB.cpp
	./source/misc/CurlWrapper.h
	./source/misc/Cassandra.h
	./source/misc/InfluxDB.h
	./source/misc/Cassandra.cpp
	./source/misc/TSDatabase.h
	./source/nodes/NodeStoreMgmtEx.cpp
	./source/nodes/NodeStoreStorageEx.cpp
	./source/nodes/NodeStoreMetaEx.h
	./source/nodes/StorageNodeEx.h
	./source/nodes/NodeStoreMetaEx.cpp
	./source/nodes/MetaNodeEx.cpp
	./source/nodes/MgmtNodeEx.cpp
	./source/nodes/NodeStoreStorageEx.h
	./source/nodes/StorageNodeEx.cpp
	./source/nodes/MetaNodeEx.h
	./source/nodes/MgmtNodeEx.h
	./source/nodes/NodeStoreMgmtEx.h
)

target_include_directories(
	mon PRIVATE
	../thirdparty/source/datastax
)

target_link_libraries(
	mon
	beegfs-common
	pthread
	dl
	curl
)

add_executable(
	beegfs-mon
	source/app/Main.cpp
)

target_link_libraries(
	beegfs-mon
	mon
)

# if(NOT BEEGFS_SKIP_TESTS)
# 	add_executable(
# 		test-meta
# 		# no tests yet
# 	)
# 
# 	target_link_libraries(
# 		test-mon
# 		mon
# 		gtest
# 	)
# 
# 	# required for a test
# 	file(
# 		COPY ${CMAKE_CURRENT_SOURCE_DIR}/build/dist/etc/beegfs-mon.conf
# 		DESTINATION dist/etc/
# 	)
# 
# 	add_test(
# 		NAME test-mon
# 		COMMAND test-mon --compiler
# 	)
# endif()

install(
	TARGETS beegfs-mon
	DESTINATION "usr/sbin"
	COMPONENT "mon"
)

install(
	FILES "build/dist/usr/lib/systemd/system/beegfs-mon.service" "build/dist/usr/lib/systemd/system/beegfs-mon@.service"
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/systemd/system"
	COMPONENT "mon"
)

install(
	FILES "build/dist/etc/beegfs-mon.conf"
	DESTINATION "etc/beegfs"
	COMPONENT "mon"
)

install(
	FILES "build/dist/etc/beegfs-mon.auth"
	DESTINATION "etc/beegfs"
	COMPONENT "mon"
)


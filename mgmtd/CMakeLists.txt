include_directories(
	source
)

add_library(
	mgmtd STATIC
	./source/toolkit/StorageTkEx.h
	./source/net/message/control/AckMsgEx.h
	./source/net/message/control/SetChannelDirectMsgEx.cpp
	./source/net/message/control/AckMsgEx.cpp
	./source/net/message/control/SetChannelDirectMsgEx.h
	./source/net/message/NetMessageFactory.h
	./source/net/message/NetMessageFactory.cpp
	./source/net/message/nodes/GetStatesAndBuddyGroupsMsgEx.cpp
	./source/net/message/nodes/ChangeTargetConsistencyStatesMsgEx.h
	./source/net/message/nodes/SetTargetConsistencyStatesMsgEx.cpp
	./source/net/message/nodes/GetNodesMsgEx.h
	./source/net/message/nodes/GetTargetStatesMsgEx.h
	./source/net/message/nodes/GetNodeCapacityPoolsMsgEx.cpp
	./source/net/message/nodes/RefreshCapacityPoolsMsgEx.cpp
	./source/net/message/nodes/GetTargetMappingsMsgEx.h
	./source/net/message/nodes/MapTargetsMsgEx.cpp
	./source/net/message/nodes/storagepools/RemoveStoragePoolMsgEx.cpp
	./source/net/message/nodes/storagepools/RemoveStoragePoolMsgEx.h
	./source/net/message/nodes/storagepools/AddStoragePoolMsgEx.h
	./source/net/message/nodes/storagepools/AddStoragePoolMsgEx.cpp
	./source/net/message/nodes/storagepools/GetStoragePoolsMsgEx.h
	./source/net/message/nodes/storagepools/ModifyStoragePoolMsgEx.h
	./source/net/message/nodes/storagepools/ModifyStoragePoolMsgEx.cpp
	./source/net/message/nodes/storagepools/GetStoragePoolsMsgEx.cpp
	./source/net/message/nodes/HeartbeatMsgEx.cpp
	./source/net/message/nodes/RemoveBuddyGroupMsgEx.h
	./source/net/message/nodes/HeartbeatRequestMsgEx.h
	./source/net/message/nodes/GetTargetMappingsMsgEx.cpp
	./source/net/message/nodes/GenericDebugMsgEx.cpp
	./source/net/message/nodes/GetNodesMsgEx.cpp
	./source/net/message/nodes/RegisterTargetMsgEx.h
	./source/net/message/nodes/SetMirrorBuddyGroupMsgEx.h
	./source/net/message/nodes/GetTargetStatesMsgEx.cpp
	./source/net/message/nodes/RemoveNodeMsgEx.cpp
	./source/net/message/nodes/UnmapTargetMsgEx.cpp
	./source/net/message/nodes/UnmapTargetMsgEx.h
	./source/net/message/nodes/SetTargetConsistencyStatesMsgEx.h
	./source/net/message/nodes/RegisterNodeMsgEx.cpp
	./source/net/message/nodes/RegisterNodeMsgEx.h
	./source/net/message/nodes/RemoveBuddyGroupMsgEx.cpp
	./source/net/message/nodes/ChangeTargetConsistencyStatesMsgEx.cpp
	./source/net/message/nodes/HeartbeatRequestMsgEx.cpp
	./source/net/message/nodes/MapTargetsMsgEx.h
	./source/net/message/nodes/HeartbeatMsgEx.h
	./source/net/message/nodes/GenericDebugMsgEx.h
	./source/net/message/nodes/GetMirrorBuddyGroupsMsgEx.cpp
	./source/net/message/nodes/RemoveNodeMsgEx.h
	./source/net/message/nodes/GetStatesAndBuddyGroupsMsgEx.h
	./source/net/message/nodes/GetNodeCapacityPoolsMsgEx.h
	./source/net/message/nodes/GetMirrorBuddyGroupsMsgEx.h
	./source/net/message/nodes/RegisterTargetMsgEx.cpp
	./source/net/message/nodes/SetMirrorBuddyGroupMsgEx.cpp
	./source/net/message/nodes/RefreshCapacityPoolsMsgEx.h
	./source/net/message/storage/GetHighResStatsMsgEx.h
	./source/net/message/storage/mirroring/SetMetadataMirroringMsgEx.h
	./source/net/message/storage/mirroring/SetMetadataMirroringMsgEx.cpp
	./source/net/message/storage/SetStorageTargetInfoMsgEx.h
	./source/net/message/storage/GetHighResStatsMsgEx.cpp
	./source/net/message/storage/SetStorageTargetInfoMsgEx.cpp
	./source/net/message/storage/quota/GetDefaultQuotaMsgEx.h
	./source/net/message/storage/quota/GetQuotaInfoMsgEx.h
	./source/net/message/storage/quota/SetQuotaMsgEx.cpp
	./source/net/message/storage/quota/RequestExceededQuotaMsgEx.cpp
	./source/net/message/storage/quota/GetQuotaInfoMsgEx.cpp
	./source/net/message/storage/quota/SetDefaultQuotaMsgEx.h
	./source/net/message/storage/quota/GetDefaultQuotaMsgEx.cpp
	./source/net/message/storage/quota/SetQuotaMsgEx.h
	./source/net/message/storage/quota/RequestExceededQuotaMsgEx.h
	./source/net/message/storage/quota/SetDefaultQuotaMsgEx.cpp
	./source/components/DatagramListener.h
	./source/components/InternodeSyncer.h
	./source/components/InternodeSyncer.cpp
	./source/components/DatagramListener.cpp
	./source/components/worker/SetExceededQuotaWork.h
	./source/components/worker/SetExceededQuotaWork.cpp
	./source/components/HeartbeatManager.cpp
	./source/components/componenthelpers/DemotionFlags.h
	./source/components/componenthelpers/HbMgrNotification.cpp
	./source/components/componenthelpers/HbMgrNotification.h
	./source/components/componenthelpers/CapacityInfo.h
	./source/components/HeartbeatManager.h
	./source/components/quota/QuotaStoreLimits.cpp
	./source/components/quota/QuotaManager.cpp
	./source/components/quota/QuotaStoreLimits.h
	./source/components/quota/QuotaManager.h
	./source/components/quota/QuotaDataRequestor.h
	./source/components/quota/QuotaDataRequestor.cpp
	./source/program/Program.h
	./source/program/Program.cpp
	./source/program/Main.cpp
	./source/app/App.h
	./source/app/App.cpp
	./source/app/config/Config.h
	./source/app/config/Config.cpp
	./source/nodes/NodeStoreClientsEx.cpp
	./source/nodes/TargetMapperEx.h
	./source/nodes/MgmtdTargetStateStore.h
	./source/nodes/NodeStoreServersEx.h
	./source/nodes/NodeStoreServersEx.cpp
	./source/nodes/StoragePoolStoreEx.cpp
	./source/nodes/TargetMapperEx.cpp
	./source/nodes/StoragePoolStoreEx.h
	./source/nodes/NodeStoreClientsEx.h
	./source/nodes/MirrorBuddyGroupMapperEx.h
	./source/nodes/NumericIDMapper.h
	./source/nodes/NumericIDMapper.cpp
	./source/nodes/TargetCapacityReport.h
	./source/nodes/MirrorBuddyGroupMapperEx.cpp
	./source/nodes/MgmtdTargetStateStore.cpp
	./source/storage/StoragePoolEx.cpp
	./source/storage/StoragePoolEx.h
)

target_link_libraries(
	mgmtd
	beegfs-common
	dl
	pthread
)

add_executable(
	beegfs-mgmtd
	source/program/Main.cpp
)

target_link_libraries(
	beegfs-mgmtd
	mgmtd
)

if(NOT BEEGFS_SKIP_TESTS)
	add_executable(
		test-mgmtd
		./tests/TestConfig.h
		./tests/TestConfig.cpp
	)

	target_link_libraries(
		test-mgmtd
		mgmtd
		gtest_main
	)

	# required for a test
	file(
		COPY ${CMAKE_CURRENT_SOURCE_DIR}/build/dist/etc/beegfs-mgmtd.conf
		DESTINATION dist/etc/
	)

	add_test(
		NAME test-mgmtd
		COMMAND test-mgmtd --compiler
	)
endif()

install(
	TARGETS beegfs-mgmtd
	DESTINATION "usr/sbin"
	COMPONENT "mgmtd"
)

install(
	PROGRAMS "build/dist/sbin/beegfs-setup-mgmtd"
	DESTINATION "usr/sbin"
	COMPONENT "mgmtd"
)

install(
	FILES "build/dist/usr/lib/systemd/system/beegfs-mgmtd.service" "build/dist/usr/lib/systemd/system/beegfs-mgmtd@.service"
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/systemd/system"
	COMPONENT "mgmtd"
)

install(
	FILES "build/dist/etc/beegfs-mgmtd.conf"
	DESTINATION "etc/beegfs"
	COMPONENT "mgmtd"
)

install(
	PROGRAMS "build/beegfs-mgmtd.sh"
	RENAME "beegfs-mgmtd"
	DESTINATION "opt/beegfs/sbin"
	COMPONENT "mgmtd"
)

include_directories(
	source
)

add_library(
	storage STATIC
	./source/toolkit/QuotaTk.h
	./source/toolkit/StorageTkEx.h
	./source/toolkit/QuotaTk.cpp
	./source/net/message/mon/RequestStorageDataMsgEx.cpp
	./source/net/message/mon/RequestStorageDataMsgEx.h
	./source/net/message/control/AckMsgEx.h
	./source/net/message/control/SetChannelDirectMsgEx.cpp
	./source/net/message/control/AckMsgEx.cpp
	./source/net/message/control/SetChannelDirectMsgEx.h
	./source/net/message/NetMessageFactory.h
	./source/net/message/session/opening/CloseChunkFileMsgEx.h
	./source/net/message/session/opening/CloseChunkFileMsgEx.cpp
	./source/net/message/session/rw/WriteLocalFileMsgEx.h
	./source/net/message/session/rw/ReadLocalFileV2MsgEx.h
	./source/net/message/session/rw/ReadLocalFileV2MsgEx.cpp
	./source/net/message/session/rw/WriteLocalFileMsgEx.cpp
	./source/net/message/session/FSyncLocalFileMsgEx.h
	./source/net/message/session/FSyncLocalFileMsgEx.cpp
	./source/net/message/NetMessageFactory.cpp
	./source/net/message/nodes/SetTargetConsistencyStatesMsgEx.cpp
	./source/net/message/nodes/GetClientStatsMsgEx.h
	./source/net/message/nodes/MapTargetsMsgEx.cpp
	./source/net/message/nodes/RefreshTargetStatesMsgEx.cpp
	./source/net/message/nodes/storagepools/RefreshStoragePoolsMsgEx.h
	./source/net/message/nodes/storagepools/RefreshStoragePoolsMsgEx.cpp
	./source/net/message/nodes/HeartbeatMsgEx.cpp
	./source/net/message/nodes/RemoveBuddyGroupMsgEx.h
	./source/net/message/nodes/HeartbeatRequestMsgEx.h
	./source/net/message/nodes/GetClientStatsMsgEx.cpp
	./source/net/message/nodes/PublishCapacitiesMsgEx.h
	./source/net/message/nodes/GenericDebugMsgEx.cpp
	./source/net/message/nodes/GetTargetConsistencyStatesMsgEx.h
	./source/net/message/nodes/SetMirrorBuddyGroupMsgEx.h
	./source/net/message/nodes/PublishCapacitiesMsgEx.cpp
	./source/net/message/nodes/RemoveNodeMsgEx.cpp
	./source/net/message/nodes/StorageBenchControlMsgEx.cpp
	./source/net/message/nodes/SetTargetConsistencyStatesMsgEx.h
	./source/net/message/nodes/RemoveBuddyGroupMsgEx.cpp
	./source/net/message/nodes/StorageBenchControlMsgEx.h
	./source/net/message/nodes/HeartbeatRequestMsgEx.cpp
	./source/net/message/nodes/MapTargetsMsgEx.h
	./source/net/message/nodes/HeartbeatMsgEx.h
	./source/net/message/nodes/GenericDebugMsgEx.h
	./source/net/message/nodes/GetTargetConsistencyStatesMsgEx.cpp
	./source/net/message/nodes/RemoveNodeMsgEx.h
	./source/net/message/nodes/RefreshTargetStatesMsgEx.h
	./source/net/message/nodes/SetMirrorBuddyGroupMsgEx.cpp
	./source/net/message/storage/GetHighResStatsMsgEx.h
	./source/net/message/storage/creating/RmChunkPathsMsgEx.cpp
	./source/net/message/storage/creating/UnlinkLocalFileMsgEx.h
	./source/net/message/storage/creating/RmChunkPathsMsgEx.h
	./source/net/message/storage/creating/UnlinkLocalFileMsgEx.cpp
	./source/net/message/storage/mirroring/StorageResyncStartedMsgEx.cpp
	./source/net/message/storage/mirroring/StorageResyncStartedMsgEx.h
	./source/net/message/storage/mirroring/GetStorageResyncStatsMsgEx.cpp
	./source/net/message/storage/mirroring/GetStorageResyncStatsMsgEx.h
	./source/net/message/storage/mirroring/ResyncLocalFileMsgEx.h
	./source/net/message/storage/mirroring/ResyncLocalFileMsgEx.cpp
	./source/net/message/storage/mirroring/SetLastBuddyCommOverrideMsgEx.cpp
	./source/net/message/storage/mirroring/SetLastBuddyCommOverrideMsgEx.h
	./source/net/message/storage/attribs/SetLocalAttrMsgEx.cpp
	./source/net/message/storage/attribs/SetLocalAttrMsgEx.h
	./source/net/message/storage/attribs/GetChunkFileAttribsMsgEx.h
	./source/net/message/storage/attribs/GetChunkFileAttribsMsgEx.cpp
	./source/net/message/storage/GetHighResStatsMsgEx.cpp
	./source/net/message/storage/TruncLocalFileMsgEx.cpp
	./source/net/message/storage/TruncLocalFileMsgEx.h
	./source/net/message/storage/StatStoragePathMsgEx.h
	./source/net/message/storage/StatStoragePathMsgEx.cpp
	./source/net/message/storage/listing/ListChunkDirIncrementalMsgEx.cpp
	./source/net/message/storage/listing/ListChunkDirIncrementalMsgEx.h
	./source/net/message/storage/quota/GetQuotaInfoMsgEx.h
	./source/net/message/storage/quota/SetExceededQuotaMsgEx.cpp
	./source/net/message/storage/quota/SetExceededQuotaMsgEx.h
	./source/net/message/storage/quota/GetQuotaInfoMsgEx.cpp
	./source/net/message/fsck/FetchFsckChunkListMsgEx.cpp
	./source/net/message/fsck/FetchFsckChunkListMsgEx.h
	./source/net/message/fsck/DeleteChunksMsgEx.cpp
	./source/net/message/fsck/MoveChunkFileMsgEx.h
	./source/net/message/fsck/MoveChunkFileMsgEx.cpp
	./source/net/message/fsck/DeleteChunksMsgEx.h
	./source/net/msghelpers/MsgHelperIO.h
	./source/components/chunkfetcher/ChunkFetcher.cpp
	./source/components/chunkfetcher/ChunkFetcherSlave.cpp
	./source/components/chunkfetcher/ChunkFetcher.h
	./source/components/chunkfetcher/ChunkFetcherSlave.h
	./source/components/DatagramListener.h
	./source/components/InternodeSyncer.h
	./source/components/StorageStatsCollector.h
	./source/components/InternodeSyncer.cpp
	./source/components/DatagramListener.cpp
	./source/components/worker/StorageBenchWork.cpp
	./source/components/worker/StorageBenchWork.h
	./source/components/benchmarker/StorageBenchOperator.cpp
	./source/components/benchmarker/StorageBenchOperator.h
	./source/components/benchmarker/StorageBenchSlave.cpp
	./source/components/benchmarker/StorageBenchSlave.h
	./source/components/streamlistenerv2/StorageStreamListenerV2.h
	./source/components/buddyresyncer/BuddyResyncer.cpp
	./source/components/buddyresyncer/BuddyResyncJob.h
	./source/components/buddyresyncer/BuddyResyncerGatherSlave.cpp
	./source/components/buddyresyncer/BuddyResyncerDirSyncSlave.cpp
	./source/components/buddyresyncer/SyncCandidate.h
	./source/components/buddyresyncer/BuddyResyncerGatherSlave.h
	./source/components/buddyresyncer/BuddyResyncerDirSyncSlave.h
	./source/components/buddyresyncer/BuddyResyncJob.cpp
	./source/components/buddyresyncer/BuddyResyncerFileSyncSlave.cpp
	./source/components/buddyresyncer/BuddyResyncerFileSyncSlave.h
	./source/components/buddyresyncer/BuddyResyncer.h
	./source/components/StorageStatsCollector.cpp
	./source/session/ZfsSession.h
	./source/session/Session.h
	./source/session/SessionLocalFileStore.h
	./source/session/SessionLocalFile.h
	./source/session/SessionStore.cpp
	./source/session/ZfsSession.cpp
	./source/session/SessionLocalFile.cpp
	./source/session/Session.cpp
	./source/session/SessionStore.h
	./source/session/SessionLocalFileStore.cpp
	./source/program/Program.h
	./source/program/Program.cpp
	./source/program/Main.cpp
	./source/app/App.h
	./source/app/App.cpp
	./source/app/config/Config.h
	./source/app/config/Config.cpp
	./source/nodes/StorageNodeOpStats.h
	./source/storage/ChunkDir.h
	./source/storage/SyncedStoragePaths.h
	./source/storage/QuotaBlockDevice.cpp
	./source/storage/ChunkLockStore.h
	./source/storage/ChunkStore.h
	./source/storage/StorageTargets.cpp
	./source/storage/ChunkStore.cpp
	./source/storage/QuotaBlockDevice.h
	./source/storage/StorageTargets.h
)

target_link_libraries(
	storage
	beegfs-common
	dl
	pthread
	blkid
)

add_executable(
	beegfs-storage
	source/program/Main.cpp
)

target_link_libraries(
	beegfs-storage
	storage
)

if(NOT BEEGFS_SKIP_TESTS)
	add_executable(
		test-storage
		./tests/TestConfig.h
		./tests/TestConfig.cpp
	)

	target_link_libraries(
		test-storage
		storage
		gtest_main
	)

	# required for a test
	file(
		COPY ${CMAKE_CURRENT_SOURCE_DIR}/build/dist/etc/beegfs-storage.conf
		DESTINATION dist/etc/
	)

	add_test(
		NAME test-storage
		COMMAND test-storage --compiler
	)
endif()

install(
	TARGETS beegfs-storage
	DESTINATION "usr/sbin"
	COMPONENT "storage"
)

install(
	FILES "build/dist/usr/lib/systemd/system/beegfs-storage.service" "build/dist/usr/lib/systemd/system/beegfs-storage@.service"
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/systemd/system"
	COMPONENT "storage"
)

install(
	PROGRAMS "build/dist/sbin/beegfs-setup-storage"
	DESTINATION "usr/sbin"
	COMPONENT "storage"
)

install(
	FILES "build/dist/etc/beegfs-storage.conf"
	DESTINATION "etc/beegfs"
	COMPONENT "storage"
)

install(
	PROGRAMS "build/beegfs-storage.sh"
	RENAME "beegfs-storage"
	DESTINATION "opt/beegfs/sbin"
	COMPONENT "storage"
)

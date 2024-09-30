include_directories(
	source
)

add_library(
	meta STATIC
	./source/toolkit/StorageTkEx.cpp
	./source/toolkit/BuddyCommTk.cpp
	./source/toolkit/BuddyCommTk.h
	./source/toolkit/XAttrTk.h
	./source/toolkit/XAttrTk.cpp
	./source/toolkit/StorageTkEx.h
	./source/net/message/mon/RequestMetaDataMsgEx.h
	./source/net/message/mon/RequestMetaDataMsgEx.cpp
	./source/net/message/control/AckMsgEx.h
	./source/net/message/control/SetChannelDirectMsgEx.cpp
	./source/net/message/control/AckMsgEx.cpp
	./source/net/message/control/SetChannelDirectMsgEx.h
	./source/net/message/NetMessageFactory.h
	./source/net/message/session/opening/OpenFileMsgEx.cpp
	./source/net/message/session/opening/CloseFileMsgEx.h
	./source/net/message/session/opening/OpenFileMsgEx.h
	./source/net/message/session/opening/CloseFileMsgEx.cpp
	./source/net/message/session/BumpFileVersionMsgEx.h
	./source/net/message/session/GetFileVersionMsgEx.h
	./source/net/message/session/locking/FLockEntryMsgEx.h
	./source/net/message/session/locking/FLockRangeMsgEx.h
	./source/net/message/session/locking/FLockRangeMsgEx.cpp
	./source/net/message/session/locking/FLockAppendMsgEx.h
	./source/net/message/session/locking/FLockEntryMsgEx.cpp
	./source/net/message/session/locking/FLockAppendMsgEx.cpp
	./source/net/message/session/GetFileVersionMsgEx.cpp
	./source/net/message/session/BumpFileVersionMsgEx.cpp
	./source/net/message/session/AckNotifyMsgEx.h
	./source/net/message/NetMessageFactory.cpp
	./source/net/message/nodes/SetTargetConsistencyStatesMsgEx.cpp
	./source/net/message/nodes/GetNodesMsgEx.h
	./source/net/message/nodes/GetNodeCapacityPoolsMsgEx.cpp
	./source/net/message/nodes/RefreshCapacityPoolsMsgEx.cpp
	./source/net/message/nodes/GetTargetMappingsMsgEx.h
	./source/net/message/nodes/GetClientStatsMsgEx.h
	./source/net/message/nodes/MapTargetsMsgEx.cpp
	./source/net/message/nodes/RefreshTargetStatesMsgEx.cpp
	./source/net/message/nodes/storagepools/RefreshStoragePoolsMsgEx.h
	./source/net/message/nodes/storagepools/RefreshStoragePoolsMsgEx.cpp
	./source/net/message/nodes/HeartbeatMsgEx.cpp
	./source/net/message/nodes/HeartbeatRequestMsgEx.h
	./source/net/message/nodes/GetClientStatsMsgEx.cpp
	./source/net/message/nodes/GetTargetMappingsMsgEx.cpp
	./source/net/message/nodes/PublishCapacitiesMsgEx.h
	./source/net/message/nodes/GenericDebugMsgEx.cpp
	./source/net/message/nodes/GetNodesMsgEx.cpp
	./source/net/message/nodes/SetMirrorBuddyGroupMsgEx.h
	./source/net/message/nodes/PublishCapacitiesMsgEx.cpp
	./source/net/message/nodes/RemoveNodeMsgEx.cpp
	./source/net/message/nodes/SetTargetConsistencyStatesMsgEx.h
	./source/net/message/nodes/HeartbeatRequestMsgEx.cpp
	./source/net/message/nodes/MapTargetsMsgEx.h
	./source/net/message/nodes/HeartbeatMsgEx.h
	./source/net/message/nodes/GenericDebugMsgEx.h
	./source/net/message/nodes/RemoveNodeMsgEx.h
	./source/net/message/nodes/GetNodeCapacityPoolsMsgEx.h
	./source/net/message/nodes/RefreshTargetStatesMsgEx.h
	./source/net/message/nodes/SetMirrorBuddyGroupMsgEx.cpp
	./source/net/message/nodes/RefreshCapacityPoolsMsgEx.h
	./source/net/message/MirroredMessage.h
	./source/net/message/storage/moving/RenameV2MsgEx.cpp
	./source/net/message/storage/moving/RenameV2MsgEx.h
	./source/net/message/storage/moving/MovingFileInsertMsgEx.cpp
	./source/net/message/storage/moving/MovingDirInsertMsgEx.h
	./source/net/message/storage/moving/MovingFileInsertMsgEx.h
	./source/net/message/storage/moving/MovingDirInsertMsgEx.cpp
	./source/net/message/storage/GetHighResStatsMsgEx.h
	./source/net/message/storage/creating/MkFileWithPatternMsgEx.cpp
	./source/net/message/storage/creating/MkLocalDirMsgEx.cpp
	./source/net/message/storage/creating/MkFileWithPatternMsgEx.h
	./source/net/message/storage/creating/RmDirEntryMsgEx.h
	./source/net/message/storage/creating/UnlinkFileMsgEx.h
	./source/net/message/storage/creating/MkFileMsgEx.cpp
	./source/net/message/storage/creating/RmDirMsgEx.cpp
	./source/net/message/storage/creating/MkLocalDirMsgEx.h
	./source/net/message/storage/creating/MkFileMsgEx.h
	./source/net/message/storage/creating/MkDirMsgEx.cpp
	./source/net/message/storage/creating/UnlinkFileMsgEx.cpp
	./source/net/message/storage/creating/MkDirMsgEx.h
	./source/net/message/storage/creating/HardlinkMsgEx.h
	./source/net/message/storage/creating/HardlinkMsgEx.cpp
	./source/net/message/storage/creating/RmLocalDirMsgEx.cpp
	./source/net/message/storage/creating/RmDirEntryMsgEx.cpp
	./source/net/message/storage/creating/RmLocalDirMsgEx.h
	./source/net/message/storage/creating/RmDirMsgEx.h
	./source/net/message/storage/creating/MoveFileInodeMsgEx.cpp
	./source/net/message/storage/creating/UnlinkLocalFileInodeMsgEx.cpp
	./source/net/message/storage/TruncFileMsgEx.h
	./source/net/message/storage/mirroring/ResyncRawInodesMsgEx.cpp
	./source/net/message/storage/mirroring/StorageResyncStartedMsgEx.cpp
	./source/net/message/storage/mirroring/StorageResyncStartedMsgEx.h
	./source/net/message/storage/mirroring/SetMetadataMirroringMsgEx.h
	./source/net/message/storage/mirroring/ResyncSessionStoreMsgEx.h
	./source/net/message/storage/mirroring/ResyncSessionStoreMsgEx.cpp
	./source/net/message/storage/mirroring/GetMetaResyncStatsMsgEx.h
	./source/net/message/storage/mirroring/SetMetadataMirroringMsgEx.cpp
	./source/net/message/storage/mirroring/GetMetaResyncStatsMsgEx.cpp
	./source/net/message/storage/mirroring/ResyncRawInodesMsgEx.h
	./source/net/message/storage/attribs/RemoveXAttrMsgEx.cpp
	./source/net/message/storage/attribs/UpdateDirParentMsgEx.h
	./source/net/message/storage/attribs/RefreshEntryInfoMsgEx.h
	./source/net/message/storage/attribs/StatMsgEx.h
	./source/net/message/storage/attribs/RemoveXAttrMsgEx.h
	./source/net/message/storage/attribs/ListXAttrMsgEx.h
	./source/net/message/storage/attribs/GetEntryInfoMsgEx.h
	./source/net/message/storage/attribs/SetAttrMsgEx.h
	./source/net/message/storage/attribs/SetXAttrMsgEx.h
	./source/net/message/storage/attribs/SetDirPatternMsgEx.h
	./source/net/message/storage/attribs/ListXAttrMsgEx.cpp
	./source/net/message/storage/attribs/SetDirPatternMsgEx.cpp
	./source/net/message/storage/attribs/GetXAttrMsgEx.cpp
	./source/net/message/storage/attribs/StatMsgEx.cpp
	./source/net/message/storage/attribs/RefreshEntryInfoMsg.cpp
	./source/net/message/storage/attribs/GetEntryInfoMsgEx.cpp
	./source/net/message/storage/attribs/GetXAttrMsgEx.h
	./source/net/message/storage/attribs/UpdateDirParentMsgEx.cpp
	./source/net/message/storage/attribs/SetAttrMsgEx.cpp
	./source/net/message/storage/attribs/SetXAttrMsgEx.cpp
	./source/net/message/storage/TruncFileMsgEx.cpp
	./source/net/message/storage/GetHighResStatsMsgEx.cpp
	./source/net/message/storage/lookup/FindOwnerMsgEx.cpp
	./source/net/message/storage/lookup/FindLinkOwnerMsgEx.cpp
	./source/net/message/storage/lookup/FindOwnerMsgEx.h
	./source/net/message/storage/lookup/LookupIntentMsgEx.h
	./source/net/message/storage/lookup/FindLinkOwnerMsgEx.h
	./source/net/message/storage/lookup/LookupIntentMsgEx.cpp
	./source/net/message/storage/StatStoragePathMsgEx.h
	./source/net/message/storage/StatStoragePathMsgEx.cpp
	./source/net/message/storage/listing/ListDirFromOffsetMsgEx.h
	./source/net/message/storage/listing/ListDirFromOffsetMsgEx.cpp
	./source/net/message/storage/quota/SetExceededQuotaMsgEx.cpp
	./source/net/message/storage/quota/SetExceededQuotaMsgEx.h
	./source/net/message/fsck/UpdateFileAttribsMsgEx.cpp
	./source/net/message/fsck/RemoveInodesMsgEx.h
	./source/net/message/fsck/AdjustChunkPermissionsMsgEx.h
	./source/net/message/fsck/FixInodeOwnersMsgEx.cpp
	./source/net/message/fsck/AdjustChunkPermissionsMsgEx.cpp
	./source/net/message/fsck/FsckSetEventLoggingMsgEx.h
	./source/net/message/fsck/UpdateDirAttribsMsgEx.cpp
	./source/net/message/fsck/FixInodeOwnersMsgEx.h
	./source/net/message/fsck/UpdateFileAttribsMsgEx.h
	./source/net/message/fsck/UpdateDirAttribsMsgEx.h
	./source/net/message/fsck/FixInodeOwnersInDentryMsgEx.cpp
	./source/net/message/fsck/RetrieveDirEntriesMsgEx.cpp
	./source/net/message/fsck/DeleteDirEntriesMsgEx.h
	./source/net/message/fsck/RetrieveFsIDsMsgEx.h
	./source/net/message/fsck/CreateEmptyContDirsMsgEx.h
	./source/net/message/fsck/RetrieveInodesMsgEx.cpp
	./source/net/message/fsck/LinkToLostAndFoundMsgEx.h
	./source/net/message/fsck/RetrieveInodesMsgEx.h
	./source/net/message/fsck/RecreateDentriesMsgEx.cpp
	./source/net/message/fsck/CreateDefDirInodesMsgEx.h
	./source/net/message/fsck/LinkToLostAndFoundMsgEx.cpp
	./source/net/message/fsck/RecreateFsIDsMsgEx.cpp
	./source/net/message/fsck/FixInodeOwnersInDentryMsgEx.h
	./source/net/message/fsck/FsckSetEventLoggingMsgEx.cpp
	./source/net/message/fsck/RetrieveDirEntriesMsgEx.h
	./source/net/message/fsck/DeleteDirEntriesMsgEx.cpp
	./source/net/message/fsck/CreateDefDirInodesMsgEx.cpp
	./source/net/message/fsck/RecreateDentriesMsgEx.h
	./source/net/message/fsck/RecreateFsIDsMsgEx.h
	./source/net/message/fsck/RemoveInodesMsgEx.cpp
	./source/net/message/fsck/RetrieveFsIDsMsgEx.cpp
	./source/net/message/fsck/CreateEmptyContDirsMsgEx.cpp
	./source/net/message/fsck/CheckAndRepairDupInodeMsgEx.cpp
	./source/net/msghelpers/MsgHelperMkFile.h
	./source/net/msghelpers/MsgHelperTrunc.cpp
	./source/net/msghelpers/MsgHelperXAttr.cpp
	./source/net/msghelpers/MsgHelperStat.h
	./source/net/msghelpers/MsgHelperLocking.h
	./source/net/msghelpers/MsgHelperUnlink.h
	./source/net/msghelpers/MsgHelperXAttr.h
	./source/net/msghelpers/MsgHelperOpen.cpp
	./source/net/msghelpers/MsgHelperClose.cpp
	./source/net/msghelpers/MsgHelperClose.h
	./source/net/msghelpers/MsgHelperTrunc.h
	./source/net/msghelpers/MsgHelperUnlink.cpp
	./source/net/msghelpers/MsgHelperMkFile.cpp
	./source/net/msghelpers/MsgHelperStat.cpp
	./source/net/msghelpers/MsgHelperOpen.h
	./source/net/msghelpers/MsgHelperLocking.cpp
	./source/components/FileEventLogger.h
	./source/components/DisposalGarbageCollector.h
	./source/components/DatagramListener.h
	./source/components/InternodeSyncer.h
	./source/components/ModificationEventFlusher.cpp
	./source/components/ModificationEventFlusher.h
	./source/components/InternodeSyncer.cpp
	./source/components/DatagramListener.cpp
	./source/components/worker/GetChunkFileAttribsWork.cpp
	./source/components/worker/SetChunkFileAttribsWork.h
	./source/components/worker/SetChunkFileAttribsWork.cpp
	./source/components/worker/UnlinkChunkFileWork.h
	./source/components/worker/BarrierWork.h
	./source/components/worker/CloseChunkFileWork.h
	./source/components/worker/GetChunkFileAttribsWork.h
	./source/components/worker/UnlinkChunkFileWork.cpp
	./source/components/worker/TruncChunkFileWork.cpp
	./source/components/worker/CloseChunkFileWork.cpp
	./source/components/worker/LockEntryNotificationWork.h
	./source/components/worker/LockEntryNotificationWork.cpp
	./source/components/worker/LockRangeNotificationWork.h
	./source/components/worker/LockRangeNotificationWork.cpp
	./source/components/worker/TruncChunkFileWork.h
	./source/components/FileEventLogger.cpp
	./source/components/DisposalGarbageCollector.cpp
	./source/components/buddyresyncer/BuddyResyncer.cpp
	./source/components/buddyresyncer/BuddyResyncJob.h
	./source/components/buddyresyncer/BuddyResyncerBulkSyncSlave.cpp
	./source/components/buddyresyncer/SessionStoreResyncer.cpp
	./source/components/buddyresyncer/BuddyResyncerGatherSlave.cpp
	./source/components/buddyresyncer/BuddyResyncerBulkSyncSlave.h
	./source/components/buddyresyncer/SyncCandidate.h
	./source/components/buddyresyncer/BuddyResyncerGatherSlave.h
	./source/components/buddyresyncer/SessionStoreResyncer.h
	./source/components/buddyresyncer/BuddyResyncJob.cpp
	./source/components/buddyresyncer/SyncSlaveBase.cpp
	./source/components/buddyresyncer/SyncSlaveBase.h
	./source/components/buddyresyncer/BuddyResyncerModSyncSlave.h
	./source/components/buddyresyncer/BuddyResyncerModSyncSlave.cpp
	./source/components/buddyresyncer/BuddyResyncer.h
	./source/session/LockingNotifier.cpp
	./source/session/EntryLock.h
	./source/session/EntryLockStore.cpp
	./source/session/EntryLockStore.h
	./source/session/SessionFile.h
	./source/session/LockingNotifier.h
	./source/session/Session.h
	./source/session/SessionStore.cpp
	./source/session/SessionFileStore.h
	./source/session/SessionFile.cpp
	./source/session/Session.cpp
	./source/session/MirrorMessageResponseState.h
	./source/session/SessionStore.h
	./source/session/SessionFileStore.cpp
	./source/session/MirrorMessageResponseState.cpp
	./source/program/Program.h
	./source/program/Program.cpp
	./source/program/Main.cpp
	./source/app/App.h
	./source/app/App.cpp
	./source/app/config/Config.h
	./source/app/config/Config.cpp
	./source/nodes/MetaNodeOpStats.h
	./source/storage/DirInode.h
	./source/storage/IncompleteInode.cpp
	./source/storage/MetadataEx.h
	./source/storage/Locking.h
	./source/storage/DirEntryStore.cpp
	./source/storage/DentryStoreData.h
	./source/storage/FileInodeStoreData.h
	./source/storage/InodeFileStore.cpp
	./source/storage/PosixACL.cpp
	./source/storage/IncompleteInode.h
	./source/storage/Locking.cpp
	./source/storage/MkFileDetails.h
	./source/storage/MetaStore.cpp
	./source/storage/InodeFileStore.h
	./source/storage/FileInode.cpp
	./source/storage/DiskMetaData.cpp
	./source/storage/FileInode.h
	./source/storage/InodeDirStore.cpp
	./source/storage/DirEntry.cpp
	./source/storage/SyncedDiskAccessPath.h
	./source/storage/DirEntryStore.h
	./source/storage/MetaStore.h
	./source/storage/MetaStoreRename.cpp
	./source/storage/NodeOfflineWait.h
	./source/storage/InodeDirStore.h
	./source/storage/DirInode.cpp
	./source/storage/DiskMetaData.h
	./source/storage/DirEntry.h
	./source/storage/MetaFileHandle.h
	./source/storage/FileInodeStoreData.cpp
	./source/storage/PosixACL.h
)

target_link_libraries(
	meta
	beegfs-common
	dl
	pthread
	blkid
)

add_executable(
	beegfs-meta
	source/program/Main.cpp
)

target_link_libraries(
	beegfs-meta
	meta
)

if(NOT BEEGFS_SKIP_TESTS)
	add_executable(
		test-meta
		./tests/TestConfig.h
		./tests/TestSerialization.h
		./tests/TestSerialization.cpp
		./tests/TestConfig.cpp
		./tests/TestBuddyMirroring.cpp
	)

	target_link_libraries(
		test-meta
		meta
		gtest_main
	)

	# required for a test
	file(
		COPY ${CMAKE_CURRENT_SOURCE_DIR}/build/dist/etc/beegfs-meta.conf
		DESTINATION dist/etc/
	)

	add_test(
		NAME test-meta
		COMMAND test-meta --compiler
	)
endif()

install(
	TARGETS beegfs-meta
	DESTINATION "usr/sbin"
	COMPONENT "meta"
)

install(
	PROGRAMS "build/dist/sbin/beegfs-setup-meta"
	DESTINATION "usr/sbin"
	COMPONENT "meta"
)

install(
	FILES "build/dist/usr/lib/systemd/system/beegfs-meta.service" "build/dist/usr/lib/systemd/system/beegfs-meta@.service"
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/systemd/system"
	COMPONENT "meta"
)

install(
	FILES "build/dist/etc/beegfs-meta.conf"
	DESTINATION "etc/beegfs"
	COMPONENT "meta"
)

install(
	PROGRAMS "build/beegfs-meta.sh"
	RENAME "beegfs-meta"
	DESTINATION "opt/beegfs/sbin"
	COMPONENT "meta"
)

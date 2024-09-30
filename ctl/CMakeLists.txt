include_directories(
	source
	${CMAKE_SOURCE_DIR}/client_devel/include
	${CMAKE_SOURCE_DIR}/client_module/include
)

add_library(
	ctl STATIC
	./source/toolkit/IoctlTk.h
	./source/toolkit/IoctlTk.cpp
	./source/toolkit/XAttrTk.h
	./source/toolkit/XAttrTk.cpp
	./source/net/message/control/AckMsgEx.h
	./source/net/message/control/AckMsgEx.cpp
	./source/net/message/NetMessageFactory.h
	./source/net/message/NetMessageFactory.cpp
	./source/net/message/nodes/HeartbeatMsgEx.cpp
	./source/net/message/nodes/HeartbeatRequestMsgEx.h
	./source/net/message/nodes/HeartbeatRequestMsgEx.cpp
	./source/net/message/nodes/HeartbeatMsgEx.h
	./source/components/DatagramListener.h
	./source/components/DatagramListener.cpp
	./source/modes/ModeIOTest.h
	./source/modes/ModeListTargets.h
	./source/modes/ModeHelp.cpp
	./source/modes/storagepools/ModeListStoragePools.h
	./source/modes/storagepools/ModeAddStoragePool.h
	./source/modes/storagepools/ModeAddStoragePool.cpp
	./source/modes/storagepools/ModeRemoveStoragePool.h
	./source/modes/storagepools/ModeRemoveStoragePool.cpp
	./source/modes/storagepools/ModeModifyStoragePool.h
	./source/modes/storagepools/ModeListStoragePools.cpp
	./source/modes/storagepools/ModeModifyStoragePool.cpp
	./source/modes/ModeMapTarget.cpp
	./source/modes/Mode.h
	./source/modes/ModeIoctl.cpp
	./source/modes/Common.h
	./source/modes/modehelpers/ModeInterruptedException.h
	./source/modes/modehelpers/ModeHelperGetNodes.cpp
	./source/modes/modehelpers/ModeHelperGetNodes.h
	./source/modes/modehelpers/ModeHelper.h
	./source/modes/modehelpers/ModeHelper.cpp
	./source/modes/ModeSetQuota.h
	./source/modes/ModeIOStat.h
	./source/modes/ModeHashDir.h
	./source/modes/ModeRefreshEntryInfo.cpp
	./source/modes/ModeCreateFile.h
	./source/modes/ModeSetPattern.cpp
	./source/modes/ModeRemoveTarget.cpp
	./source/modes/ModeGetQuotaInfo.cpp
	./source/modes/migrate/ModeMigrate.h
	./source/modes/migrate/MigrateFile.h
	./source/modes/migrate/MigrateFile.cpp
	./source/modes/migrate/ModeMigrate.cpp
	./source/modes/migrate/ModeFind.h
	./source/modes/migrate/ModeMigrateException.h
	./source/modes/ModeDisposeUnusedFiles.cpp
	./source/modes/ModeIOStat.cpp
	./source/modes/Mode.cpp
	./source/modes/ModeRefreshEntryInfo.h
	./source/modes/ModeCreateFile.cpp
	./source/modes/ModeRemoveNode.cpp
	./source/modes/ModeGetNodes.cpp
	./source/modes/Common.cpp
	./source/modes/ModeRemoveTarget.h
	./source/modes/ModeStorageBench.h
	./source/modes/mirroring/ModeStartResync.h
	./source/modes/mirroring/ModeSetState.cpp
	./source/modes/mirroring/ModeRemoveBuddyGroup.h
	./source/modes/mirroring/ModeSetState.h
	./source/modes/mirroring/ModeAddMirrorBuddyGroup.h
	./source/modes/mirroring/ModeSetMetadataMirroring.cpp
	./source/modes/mirroring/ModeListMirrorBuddyGroups.cpp
	./source/modes/mirroring/ModeResyncStats.cpp
	./source/modes/mirroring/ModeListMirrorBuddyGroups.h
	./source/modes/mirroring/ModeStartResync.cpp
	./source/modes/mirroring/ModeRemoveBuddyGroup.cpp
	./source/modes/mirroring/ModeSetMetadataMirroring.h
	./source/modes/mirroring/ModeResyncStats.h
	./source/modes/mirroring/ModeAddMirrorBuddyGroup.cpp
	./source/modes/modeclientstats/ModeClientStats.h
	./source/modes/modeclientstats/ModeClientStats.cpp
	./source/modes/ModeStorageBench.cpp
	./source/modes/ModeRemoveDirEntry.h
	./source/modes/ModeHelp.h
	./source/modes/ModeIOTest.cpp
	./source/modes/ModeMapTarget.h
	./source/modes/ModeHashDir.cpp
	./source/modes/ModeRemoveNode.h
	./source/modes/ModeGetNodes.h
	./source/modes/ModeIoctl.h
	./source/modes/ModeGetQuotaInfo.h
	./source/modes/ModeSetPattern.h
	./source/modes/ModeGetEntryInfo.cpp
	./source/modes/ModeDisposeUnusedFiles.h
	./source/modes/ModeGetEntryInfo.h
	./source/modes/ModeListTargets.cpp
	./source/modes/ModeCreateDir.h
	./source/modes/ModeGenericDebug.h
	./source/modes/ModeRemoveDirEntry.cpp
	./source/modes/ModeSetQuota.cpp
	./source/modes/ModeCreateDir.cpp
	./source/modes/ModeGenericDebug.cpp
	./source/modes/ModeMoveFileInode.cpp
	./source/program/Program.h
	./source/program/Program.cpp
	./source/program/Main.cpp
	./source/app/App.h
	./source/app/App.cpp
	./source/app/config/Config.h
	./source/app/config/Config.cpp
)

target_link_libraries(
	ctl
	beegfs-common
	dl
	pthread
)
	
add_executable(
	beegfs-ctl
	source/program/Main.cpp
)

target_link_libraries(
	beegfs-ctl
	ctl
)

if(NOT BEEGFS_SKIP_TESTS)
	add_executable(
		test-ctl
		./tests/TestXAttrTk.cpp
		./tests/syscall.h
		./tests/testing/syscall_override.h
		./tests/syscall.cpp
	)

	target_link_libraries(
		test-ctl
		ctl
		gtest_main
	)

	add_test(
		NAME test-ctl
		COMMAND test-ctl --compiler
	)
endif()

install(
	TARGETS beegfs-ctl
	DESTINATION "usr/bin"
	COMPONENT "utils"
)

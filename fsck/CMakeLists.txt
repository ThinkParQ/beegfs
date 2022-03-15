include_directories(
	source
)

add_library(
	fsck STATIC
	./source/toolkit/DatabaseTk.cpp
	./source/toolkit/FsckDefinitions.h
	./source/toolkit/FsckException.h
	./source/toolkit/FsckDefinitions.cpp
	./source/toolkit/FsckTkEx.cpp
	./source/toolkit/DatabaseTk.h
	./source/toolkit/FsckTkEx.h
	./source/database/VectorSource.h
	./source/database/DirInode.h
	./source/database/FsckDBTable.cpp
	./source/database/Select.h
	./source/database/EntryID.h
	./source/database/Union.h
	./source/database/SetFragmentCursor.h
	./source/database/FsID.h
	./source/database/Set.h
	./source/database/FsckDB.cpp
	./source/database/SetFragment.h
	./source/database/Group.h
	./source/database/FsckDB.h
	./source/database/FsckDBChecks.cpp
	./source/database/Table.h
	./source/database/Cursor.h
	./source/database/UsedTarget.h
	./source/database/FsckDBException.cpp
	./source/database/FileInode.h
	./source/database/ModificationEvent.h
	./source/database/LeftJoinEq.h
	./source/database/Filter.h
	./source/database/FsckDBException.h
	./source/database/DirEntry.h
	./source/database/Chunk.h
	./source/database/FsckDBTable.h
	./source/database/ContDir.h
	./source/database/StripeTargets.h
	./source/database/Buffer.h
	./source/database/Distinct.h
	./source/database/DiskList.h
	./source/net/message/NetMessageFactory.h
	./source/net/message/NetMessageFactory.cpp
	./source/net/message/nodes/HeartbeatMsgEx.cpp
	./source/net/message/nodes/HeartbeatMsgEx.h
	./source/net/message/testing/DummyMsgEx.h
	./source/net/message/testing/DummyMsgEx.cpp
	./source/net/message/fsck/FsckModificationEventMsgEx.cpp
	./source/net/message/fsck/FsckModificationEventMsgEx.h
	./source/net/msghelpers/MsgHelperRepair.h
	./source/net/msghelpers/MsgHelperRepair.cpp
	./source/components/DatagramListener.h
	./source/components/ModificationEventHandler.h
	./source/components/InternodeSyncer.h
	./source/components/InternodeSyncer.cpp
	./source/components/DataFetcher.cpp
	./source/components/ModificationEventHandler.cpp
	./source/components/DatagramListener.cpp
	./source/components/worker/RetrieveDirEntriesWork.h
	./source/components/worker/RetrieveInodesWork.h
	./source/components/worker/AdjustChunkPermissionsWork.h
	./source/components/worker/AdjustChunkPermissionsWork.cpp
	./source/components/worker/RetrieveFsIDsWork.cpp
	./source/components/worker/RetrieveChunksWork.cpp
	./source/components/worker/RetrieveChunksWork.h
	./source/components/worker/RetrieveInodesWork.cpp
	./source/components/worker/RetrieveFsIDsWork.h
	./source/components/worker/RetrieveDirEntriesWork.cpp
	./source/components/DataFetcher.h
	./source/modes/ModeHelp.cpp
	./source/modes/Mode.h
	./source/modes/ModeCheckFS.cpp
	./source/modes/Mode.cpp
	./source/modes/ModeEnableQuota.cpp
	./source/modes/ModeHelp.h
	./source/modes/ModeEnableQuota.h
	./source/modes/ModeCheckFS.h
	./source/program/Program.h
	./source/program/Program.cpp
	./source/program/Main.cpp
	./source/app/App.h
	./source/app/App.cpp
	./source/app/config/Config.h
	./source/app/config/Config.cpp
)

target_link_libraries(
	fsck
	beegfs-common
	dl
	pthread
)
	
add_executable(
	fsck.beegfs
	source/program/Main.cpp
)

target_link_libraries(
	fsck.beegfs
	fsck
)

if(NOT BEEGFS_SKIP_TESTS)
	# This is a dirty workaround used for a hardcoded path in
	# "fsck/tests/TestConfig.cpp". Once the "defaultConfigFile" test does
	# not steal the config file from "client_module" anymore, this
	# workaround can be removed.
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTHIS_IS_A_CMAKE_BUILD=1")

	add_executable(
		test-fsck
		./tests/TestFsckTk.cpp
		./tests/TestDatabase.cpp
		./tests/TestConfig.h
		./tests/TestSet.cpp
		./tests/TestCursors.cpp
		./tests/TestTable.cpp
		./tests/FlatTest.h
		./tests/TestSetFragment.cpp
		./tests/TestSerialization.cpp
		./tests/FlatTest.cpp
		./tests/TestTable.h
		./tests/TestConfig.cpp
		./tests/TestDatabase.h
	)

	target_link_libraries(
		test-fsck
		fsck
		gtest_main
	)

	# required for a test
	file(
		COPY ${CMAKE_CURRENT_SOURCE_DIR}/../client_module/build/dist/etc/beegfs-client.conf
		DESTINATION dist/etc/
	)

	add_test(
		NAME test-fsck
		COMMAND test-fsck --compiler
	)
endif()

install(
	TARGETS fsck.beegfs
	DESTINATION "sbin"
	COMPONENT "utils"
)

include_directories(
	source
)

add_library(
	helperd STATIC
	./source/app/config/Config.h
	./source/app/config/Config.cpp
	./source/app/App.h
	./source/app/App.cpp
	./source/program/Program.cpp
	./source/program/Program.h
	./source/program/Main.cpp
	./source/net/message/NetMessageFactory.h
	./source/net/message/helperd/GetHostByNameMsgEx.cpp
	./source/net/message/helperd/GetHostByNameMsgEx.h
	./source/net/message/helperd/LogMsgEx.cpp
	./source/net/message/helperd/LogMsgEx.h
	./source/net/message/control/SetChannelDirectMsgEx.cpp
	./source/net/message/control/SetChannelDirectMsgEx.h
	./source/net/message/NetMessageFactory.cpp
)

target_link_libraries(
	helperd
	beegfs-common
	pthread
	dl
)
	
add_executable(
	beegfs-helperd
	source/program/Main.cpp
)

target_link_libraries(
	beegfs-helperd
	helperd
)

if(NOT BEEGFS_SKIP_TESTS)
	add_executable(
		test-helperd
		./tests/TestConfig.cpp
		./tests/TestConfig.h
	)

	target_link_libraries(
		test-helperd
		helperd
		gtest_main
	)

	add_test(
		NAME test-helperd
		COMMAND test-helperd --compiler
	)
endif()

install(
	TARGETS beegfs-helperd
	DESTINATION "usr/sbin"
	COMPONENT "helperd"
)

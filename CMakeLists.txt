PROJECT(workspace)

CMAKE_MINIMUM_REQUIRED(VERSION 2.6)


SET(CMAKE_CXX_FLAGS "-g -Wall")

OPTION(LUACALLOUTS "Enable LUA Callouts" FALSE)
IF (LUACALLOUTS)
	ADD_DEFINITIONS(-DLUACALLOUTS)
	SET(LUALIB "lua5.1")
	find_package(Lua51 REQUIRED)
	INCLUDE_DIRECTORIES(${LUA_INCLUDE_DIR} ${workspace_SOURCE_DIR}/yaml-cpp-0.5.0/include)
ELSE (LUACALLOUTS)
	INCLUDE_DIRECTORIES(${workspace_SOURCE_DIR}/yaml-cpp-0.5.0/include)
ENDIF (LUACALLOUTS)

FIND_PACKAGE(Boost COMPONENTS system filesystem regex program_options REQUIRED)

ADD_SUBDIRECTORY(${workspace_SOURCE_DIR}/yaml-cpp-0.5.0)


SET (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${workspace_BINARY_DIR}/bin)
ADD_EXECUTABLE(ws_allocate ${workspace_SOURCE_DIR}/src/ws_allocate.cpp ${workspace_SOURCE_DIR}/src/ws_util.cpp ${workspace_SOURCE_DIR}/src/ws_util.h)
ADD_EXECUTABLE(ws_release ${workspace_SOURCE_DIR}/src/ws_release.cpp ${workspace_SOURCE_DIR}/src/ws_util.cpp ${workspace_SOURCE_DIR}/src/ws_util.h)

TARGET_LINK_LIBRARIES( ws_allocate ${Boost_LIBRARIES} ${LUALIB} cap yaml-cpp)
TARGET_LINK_LIBRARIES( ws_release ${Boost_LIBRARIES} cap yaml-cpp)

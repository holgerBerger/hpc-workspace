PROJECT(workspace)

CMAKE_MINIMUM_REQUIRED(VERSION 2.6)


SET(CMAKE_CXX_FLAGS "-g -Wall")

OPTION(LUACALLOUTS "Enable LUA Callouts" FALSE)
IF (LUACALLOUTS)
	ADD_DEFINITIONS(-DLUACALLOUTS)
	SET(LUALIB "lua5.1")
ENDIF (LUACALLOUTS)

FIND_PACKAGE(Boost COMPONENTS system filesystem regex program_options REQUIRED)

ADD_SUBDIRECTORY(${workspace_SOURCE_DIR}/yaml-cpp-0.5.0)

INCLUDE_DIRECTORIES(${workspace_SOURCE_DIR}/yaml-cpp-0.5.0/include)

SET (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${workspace_BINARY_DIR}/bin)
ADD_EXECUTABLE(ws_allocate ${workspace_SOURCE_DIR}/src/ws_allocate.cpp ${workspace_SOURCE_DIR}/src/ws_util.cpp)
ADD_EXECUTABLE(ws_release ${workspace_SOURCE_DIR}/src/ws_release.cpp ${workspace_SOURCE_DIR}/src/ws_util.cpp)

TARGET_LINK_LIBRARIES( ws_allocate ${Boost_LIBRARIES} ${LUALIB} cap yaml-cpp)
TARGET_LINK_LIBRARIES( ws_release ${Boost_LIBRARIES} cap yaml-cpp)

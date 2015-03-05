PROJECT(workspace)

CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

#workaround for old Redhat 6 cmake
set(Boost_NO_BOOST_CMAKE ON)

SET(CMAKE_CXX_FLAGS "-g -Wall")

OPTION(SETUID "use setuid instead of capabilites" TRUE)
IF (SETUID)
	ADD_DEFINITIONS(-DSETUID)
	SET(CAP "")
ELSE (SETUID)
    SET(CAP "cap")
ENDIF (SETUID)

OPTION(LUACALLOUTS "Enable LUA Callouts" FALSE)
IF (LUACALLOUTS)
	ADD_DEFINITIONS(-DLUACALLOUTS)
	SET(LUALIB "lua5.1")
	find_package(Lua51 REQUIRED)
	INCLUDE_DIRECTORIES(${LUA_INCLUDE_DIR} ${workspace_SOURCE_DIR}/yaml-cpp-0.5.0/include)
ELSE (LUACALLOUTS)
	INCLUDE_DIRECTORIES(${workspace_SOURCE_DIR}/yaml-cpp-0.5.0/include)
ENDIF (LUACALLOUTS)


set(Boost_USE_STATIC_LIBS ON) 
set(Boost_USE_MULTITHREADED OFF)  
set(Boost_USE_STATIC_RUNTIME ON) 
FIND_PACKAGE(Boost COMPONENTS system filesystem regex program_options REQUIRED)

ADD_SUBDIRECTORY(${workspace_SOURCE_DIR}/yaml-cpp-0.5.0)


SET (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${workspace_BINARY_DIR}/bin)
ADD_EXECUTABLE(ws_allocate ${workspace_SOURCE_DIR}/src/ws_allocate.cpp 
                ${workspace_SOURCE_DIR}/src/ws_util.cpp 
                ${workspace_SOURCE_DIR}/src/ws_util.h)

ADD_EXECUTABLE(ws_release ${workspace_SOURCE_DIR}/src/ws_release.cpp 
                ${workspace_SOURCE_DIR}/src/ws_util.cpp 
                ${workspace_SOURCE_DIR}/src/ws_util.h)

ADD_EXECUTABLE(ws_allocate_n ${workspace_SOURCE_DIR}/src/ws_allocate_n.cpp 
							 ${workspace_SOURCE_DIR}/src/ws.cpp 
							 ${workspace_SOURCE_DIR}/src/ws.h
							 ${workspace_SOURCE_DIR}/src/ws_util.cpp 
							 ${workspace_SOURCE_DIR}/src/ws_util.h)

ADD_EXECUTABLE(ws_release_n ${workspace_SOURCE_DIR}/src/ws_release_n.cpp 
							 ${workspace_SOURCE_DIR}/src/ws.cpp 
							 ${workspace_SOURCE_DIR}/src/ws.h
							 ${workspace_SOURCE_DIR}/src/ws_util.cpp 
							 ${workspace_SOURCE_DIR}/src/ws_util.h)

TARGET_LINK_LIBRARIES( ws_allocate ${Boost_LIBRARIES} ${LUALIB} ${CAP} yaml-cpp)
TARGET_LINK_LIBRARIES( ws_release ${Boost_LIBRARIES} ${CAP} yaml-cpp)

TARGET_LINK_LIBRARIES( ws_allocate_n ${Boost_LIBRARIES} ${LUALIB} ${CAP} yaml-cpp)
TARGET_LINK_LIBRARIES( ws_release_n ${Boost_LIBRARIES} ${CAP} yaml-cpp)

PROJECT(workspace)

CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

#workaround for old Redhat 6 cmake
set(Boost_NO_BOOST_CMAKE ON)

SET(CMAKE_CXX_FLAGS "-Wall")

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
	#INCLUDE_DIRECTORIES(${workspace_SOURCE_DIR}/yaml-cpp-0.5.0/include)
	INCLUDE_DIRECTORIES(${workspace_SOURCE_DIR}/yaml-cpp-release-0.5.2/include)
ENDIF (LUACALLOUTS)


set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED OFF)  
set(Boost_USE_STATIC_RUNTIME OFF) 
FIND_PACKAGE(Boost COMPONENTS system filesystem regex program_options REQUIRED)

# ADD_SUBDIRECTORY(${workspace_SOURCE_DIR}/yaml-cpp-0.5.0)
ADD_SUBDIRECTORY(${workspace_SOURCE_DIR}/yaml-cpp-release-0.5.2)


SET (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${workspace_BINARY_DIR}/bin)

ADD_EXECUTABLE(ws_allocate ${workspace_SOURCE_DIR}/src/ws_allocate.cpp 
							 ${workspace_SOURCE_DIR}/src/ws.cpp 
							 ${workspace_SOURCE_DIR}/src/ws.h
							 ${workspace_SOURCE_DIR}/src/wsdb.cpp 
							 ${workspace_SOURCE_DIR}/src/wsdb.h)

ADD_EXECUTABLE(ws_release ${workspace_SOURCE_DIR}/src/ws_release.cpp 
							 ${workspace_SOURCE_DIR}/src/ws.cpp 
							 ${workspace_SOURCE_DIR}/src/ws.h
							 ${workspace_SOURCE_DIR}/src/wsdb.cpp 
							 ${workspace_SOURCE_DIR}/src/wsdb.h)

ADD_EXECUTABLE(ws_restore ${workspace_SOURCE_DIR}/src/ws_restore.cpp 
							 ${workspace_SOURCE_DIR}/src/ruh.cpp 
							 ${workspace_SOURCE_DIR}/src/ruh.h
							 ${workspace_SOURCE_DIR}/src/ws.cpp 
							 ${workspace_SOURCE_DIR}/src/ws.h
							 ${workspace_SOURCE_DIR}/src/wsdb.cpp 
							 ${workspace_SOURCE_DIR}/src/wsdb.h)

TARGET_LINK_LIBRARIES( ws_allocate ${Boost_LIBRARIES} ${LUALIB} ${CAP} yaml-cpp)
TARGET_LINK_LIBRARIES( ws_release ${Boost_LIBRARIES} ${LUALIB} ${CAP} yaml-cpp)
TARGET_LINK_LIBRARIES( ws_restore ${Boost_LIBRARIES} ${LUALIB} ${CAP} yaml-cpp termcap)


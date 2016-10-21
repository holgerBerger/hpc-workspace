PROJECT(workspace)

CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

#workaround for old Redhat 6 cmake
set(Boost_NO_BOOST_CMAKE ON)

SET(CMAKE_CXX_FLAGS "-Wall -g")

OPTION(STATIC "static linking" FALSE)
IF (STATIC)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static")
    set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++ -static")
    SET(EXTRA_STATIC_LIBS "pthread")
#    set(Boost_USE_STATIC_LIBS ON)
#    set(Boost_USE_STATIC_RUNTIME ON) 
ELSE (STATIC)
    set(Boost_USE_STATIC_LIBS OFF)
    set(Boost_USE_STATIC_RUNTIME OFF) 
ENDIF (STATIC)

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
	INCLUDE_DIRECTORIES(${LUA_INCLUDE_DIR})
    LINK_DIRECTORIES(${LUA_LIBRARY_DIRS})
ENDIF (LUACALLOUTS)

set(Boost_USE_MULTITHREADED OFF)  
FIND_PACKAGE(Boost COMPONENTS system filesystem regex program_options REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})


# find terminfo for "are you human" checker
FIND_LIBRARY(TERMINFO NAMES libtinfo.so libtinfo.a)
IF (TERMINFO)
    MESSAGE("-- Found terminfo")
    INCLUDE_DIRECTORIES(${TERMINFO_INCLUDE_DIRS})
    SET(LIBS ${LIBS} ${TERMINFO_LIBRARIES})
    SET(TLIB "tinfo")
ELSE (TERMINFO)
    MESSAGE("-- No terminfo, trying curses")
    #find curses for terminfo for "are you human" checker
    SET(CURSES_NEED_NCURSES TRUE)
    find_package(Curses REQUIRED)
    include_directories(${CURSES_INCLUDE_DIR})
    link_directories(${CURSES_LIBRARY_DIR})
    SET(TLIB ${CURSES_LIBRARIES})
ENDIF (TERMINFO)


# find termcap for "are you human" checker
#FIND_LIBRARY(TERMCAP NAMES libtermcap.so libtermcap.a)
#IF (TERMCAP)
    #MESSAGE("-- Found termcap")
    #INCLUDE_DIRECTORIES(${TERMCAP_INCLUDE_DIRS})
    #SET(LIBS ${LIBS} ${TERMCAP_LIBRARIES})
#ELSE (TERMCAP)
    #MESSAGE(FATAL_ERROR "-- No termcap, please get a termcap library!")
#ENDIF (TERMCAP)


FIND_LIBRARY(YAML libyaml-cpp.so)
IF (YAML)
    MESSAGE("-- Found system YAML")
ELSE (YAML)
    MESSAGE("-- No system YAML, building own YAML")
    ADD_SUBDIRECTORY(${workspace_SOURCE_DIR}/yaml-cpp-release-0.5.3)
    INCLUDE_DIRECTORIES(${workspace_SOURCE_DIR}/yaml-cpp-release-0.5.3/include)
ENDIF (YAML)

#get_directory_property(LINKER_VAR LINK_DIRECTORIES)
#message(STATUS "LINKER_VAR: ${LINKER_VAR}")


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

TARGET_LINK_LIBRARIES( ws_allocate ${Boost_LIBRARIES} ${LUALIB} ${CAP} yaml-cpp ${EXTRA_STATIC_LIBS})
TARGET_LINK_LIBRARIES( ws_release ${Boost_LIBRARIES} ${LUALIB} ${CAP} yaml-cpp ${EXTRA_STATIC_LIBS})
TARGET_LINK_LIBRARIES( ws_restore ${Boost_LIBRARIES} ${LUALIB} ${CAP} yaml-cpp ${TLIB} ${EXTRA_STATIC_LIBS})


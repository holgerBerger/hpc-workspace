CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

PROJECT(workspace)
# set to version number of the latest release
add_definitions("-DWS_VERSION=\"1.3.0\"")

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

OPTION(CHECK_ALL_GROUPS "search all groups for ACLs, not just primary" FALSE)
IF (CHECK_ALL_GROUPS)
    ADD_DEFINITIONS("-DCHECK_ALL_GROUPS")
ENDIF (CHECK_ALL_GROUPS)

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

get_directory_property(LINKER_VAR LINK_DIRECTORIES)
message(STATUS "LINKER_VAR: ${LINKER_VAR}")

IF (IS_DIRECTORY "${PROJECT_SOURCE_DIR}/.git")
    add_definitions(-DIS_GIT_REPOSITORY)
    # git commit hash macro
    execute_process(
       COMMAND git log -1 --format=%h
       WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
       OUTPUT_VARIABLE GIT_COMMIT_HASH
       OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    add_definitions("-DGIT_COMMIT_HASH=\"${GIT_COMMIT_HASH}\"")
ENDIF ()

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

TARGET_LINK_LIBRARIES( ws_allocate "-L ${LINKER_VAR}" ${Boost_LIBRARIES} ${LUALIB} ${CAP} yaml-cpp ${EXTRA_STATIC_LIBS})
TARGET_LINK_LIBRARIES( ws_release "-L ${LINKER_VAR}" ${Boost_LIBRARIES} ${LUALIB} ${CAP} yaml-cpp ${EXTRA_STATIC_LIBS})
TARGET_LINK_LIBRARIES( ws_restore "-L ${LINKER_VAR}" ${Boost_LIBRARIES} ${LUALIB} ${CAP} yaml-cpp ${TLIB} ${EXTRA_STATIC_LIBS})


# Get install target
set(PROGRAM_PERMISSIONS_DEFAULT
    OWNER_WRITE OWNER_READ OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE)
install (FILES bin/ws_extend bin/ws_find bin/ws_list bin/ws_register bin/ws_send_ical DESTINATION bin PERMISSIONS ${PROGRAM_PERMISSIONS_DEFAULT})
install(TARGETS
      ws_allocate ws_release ws_restore
      DESTINATION bin
      PERMISSIONS ${PROGRAM_PERMISSIONS_DEFAULT} SETUID)
install (FILES sbin/ws_expirer sbin/ws_restore sbin/ws_validate_config DESTINATION sbin PERMISSIONS ${PROGRAM_PERMISSIONS_DEFAULT})

# Install man pages
INSTALL(FILES man/ws_allocate.1 man/ws_find.1 man/ws_register.1
              man/ws_restore.1 man/ws_extend.1 man/ws_list.1 man/ws_release.1
              man/ws_send_ical.1             DESTINATION share/man/man1)

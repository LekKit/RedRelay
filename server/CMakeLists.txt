cmake_minimum_required(VERSION 3.0.2)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to Release as none was specified.")
  set(CMAKE_BUILD_TYPE "Release" CACHE
      STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

set(CMAKE_CXX_FLAGS "-std=c++11" CACHE STRING "Flags used by the CXX compiler during all build types." FORCE)

macro(set_option var default type docstring)
    if(NOT DEFINED ${var})
        set(${var} ${default})
    endif()
    set(${var} ${${var}} CACHE ${type} ${docstring} FORCE)
endmacro()

set_option(REDRELAY_EPOLL TRUE BOOL "Build RedRelay Server with epoll/kqueue/IOCP support")
set_option(REDRELAY_MULTITHREAD FALSE BOOL "Build RedRelay Server with UDP multi-threading")

project(RedRelayServer VERSION 9)

find_package(SFML 2.5 COMPONENTS network)
if (NOT SFML_FOUND)
	message(STATUS "SFML not found. Proceeding to build a minimal version from ../deps")
	add_definitions(-DSFML_STATIC)
	include_directories(../deps)
    list (APPEND REDRELAY_SOURCES ../deps/SFML/Err.cpp ../deps/SFML/Time.cpp ../deps/SFML/Sleep.cpp ../deps/SFML/Clock.cpp ../deps/SFML/Lock.cpp ../deps/SFML/Mutex.cpp
		../deps/SFML/IpAddress.cpp ../deps/SFML/Socket.cpp ../deps/SFML/SocketSelector.cpp ../deps/SFML/TcpListener.cpp ../deps/SFML/TcpSocket.cpp ../deps/SFML/UdpSocket.cpp)
	if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
		list (APPEND REDRELAY_SOURCES ../deps/SFML/Win32ClockImpl.cpp ../deps/SFML/Win32MutexImpl.cpp ../deps/SFML/Win32SleepImpl.cpp ../deps/SFML/Win32SocketImpl.cpp)
	else()
		list (APPEND REDRELAY_SOURCES ../deps/SFML/UnixClockImpl.cpp ../deps/SFML/UnixMutexImpl.cpp ../deps/SFML/UnixSleepImpl.cpp ../deps/SFML/UnixSocketImpl.cpp)
	endif()
endif()

if (REDRELAY_EPOLL)
	add_definitions(-DREDRELAY_EPOLL)
	if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
		message(STATUS "Extended polling on Windows requires libwepoll, added to build list")
		include_directories(../deps)
		list (APPEND REDRELAY_SOURCES ../deps/wepoll.c)
	endif()
	list (APPEND REDRELAY_SOURCES EpollSelector.cpp)
endif()

if (REDRELAY_MULTITHREAD)
	message(STATUS "Warning: multi-threading in RedRelay wasn't extensively tested, use at your own risc")
	add_definitions(-DREDRELAY_MULTITHREAD)
endif()

add_library(redrelay-server STATIC ${REDRELAY_SOURCES} RedRelayServer.cpp Channel.cpp RelayPacket.cpp)

add_executable(RedRelayServer Main.cpp)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	list (APPEND REDRELAY_LIBS winmm ws2_32)
	if(${CMAKE_BUILD_TYPE} STREQUAL "Release")
		set (LINKERFLAGS "${LINKERFLAGS} -static")
	endif()
endif()

if(${CMAKE_BUILD_TYPE} STREQUAL "Release")
	set (LINKERFLAGS "${LINKERFLAGS} -s")
	if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows" OR ${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
		set (LINKERFLAGS "${LINKERFLAGS} -flto")
	endif()
endif()

set(CMAKE_EXE_LINKER_FLAGS_RELEASE ${LINKERFLAGS} CACHE STRING "Flags used by the linker during RELEASE builds." FORCE)

target_link_libraries(RedRelayServer PUBLIC redrelay-server ${REDRELAY_LIBS})

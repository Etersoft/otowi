cmake_minimum_required(VERSION 3.0.0)

project(otowi)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
	message(STATUS "Enabling ccache")
	set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
	set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif()


find_package(PkgConfig REQUIRED)

set(CMAKE_C_FLAGS "-g -Wno-attributes -D__WINESRC__ -D_NTSYSTEM_")
#pkg_check_modules(GLIB2 glib-2.0)

#add_subdirectory(wineguts)

file(GLOB FLAT_SOURCE_FILES port/codepages/*.c port/*.c ntdll/*.c kernel32/*.c port.otowi/*.c)

set(SOURCES
#src/APIConsole.cpp
#src/APIFiles.cpp
#port.otowi/file.c
#port.otowi/ntdll.c
#port/debug.c
#kernel32/file.c
#kernel32/thread.c
#ntdll/error.c
#ntdll/heap.c
#ntdll/rtl.c
#ntdll/rtlstr.c
#src/APIFSNotify.cpp
#src/APIKeyboard.cpp
#src/APIMemory.cpp
#src/APIOther.cpp
#src/APIPrintFormat.cpp
#src/APIRegistry.cpp
#src/APIStringCodepages.cpp
#src/APIStringMap.cpp
#src/APISynch.cpp
#src/APITime.cpp
#src/ConsoleBuffer.cpp
#src/ConsoleInput.cpp
#src/ConsoleOutput.cpp
#src/WinPortHandle.cpp
#src/WinPortSynch.cpp
#src/PathHelpers.cpp
)

add_library (otowi ${FLAT_SOURCE_FILES} ${SOURCES})

#target_include_directories(WinPort PRIVATE ${GLIB2_INCLUDE_DIRS})

target_include_directories(otowi PRIVATE ../include)
#target_include_directories(WinPort PRIVATE src/UI)
#target_include_directories(WinPort PRIVATE src)

#target_link_libraries(WinPort wineguts utils)

#target_link_libraries(WinPort ${GLIB2_LIBRARIES})


cmake_minimum_required(VERSION 3.0.0)

project(otowi)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
	message(STATUS "Enabling ccache")
	set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
	set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif()

#set (BUILD_SHARED_LIBS True)

find_package(PkgConfig REQUIRED)

set(CMAKE_C_FLAGS "-g -Wno-attributes -D__WINESRC__ -D_NTSYSTEM_ -D_KERNEL32_ -D_USER32_ -D_ADVAPI32_")
set(LDFLAGS "-fvisibility=default")
#pkg_check_modules(GLIB2 glib-2.0)

#add_subdirectory(wineguts)

file(GLOB FLAT_SOURCE_FILES port/codepages/*.c port/*.c ntdll/*.c ntdll.otowi/*.c kernel32/*.c kernel32.otowi/*.c user32/*.c )

add_library (otowi SHARED ${FLAT_SOURCE_FILES})
add_library (otowi_static STATIC ${FLAT_SOURCE_FILES})

target_include_directories(otowi PRIVATE ../include)
target_include_directories(otowi_static PRIVATE ../include)

target_link_libraries(otowi -ldl)


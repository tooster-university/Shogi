cmake_minimum_required(VERSION 3.8)
project(shogi)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_FLAGS "${CMAKE_CSS_FLAGS} -Wall -Wextra -Werror")

set(SOURCE_FILES
        src/App.c src/App.h
        src/ResourceManager.c src/ResourceManager.h
        src/Model.c src/Model.h
        src/Logger.c src/Logger.h
        src/Utils.h
        )

add_executable(shogi ${SOURCE_FILES})

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

include_directories(${GTK3_INCLUDE_DIRS})
link_directories(${GTK3_LIBRARY_DIRS})

add_definitions(${GTK3_CFLAGS_OTHER})

target_link_libraries(shogi ${GTK3_LIBRARIES})

# copy resources to build folder
file(COPY ${CMAKE_SOURCE_DIR}/resources DESTINATION ${CMAKE_BINARY_DIR})


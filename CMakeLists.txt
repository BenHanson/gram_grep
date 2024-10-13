cmake_minimum_required(VERSION 3.25)

project(gram_grep
    DESCRIPTION "gram_grep: grep for the 21st century"
    HOMEPAGE_URL "http://benhanson.net/gram_grep.html"
    LANGUAGES CXX
)

if(ENABLE_ASAN)
    if(WIN32)
        string(REPLACE " /RTC1" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
        string(REPLACE "/RTC1 " "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
        string(REPLACE " /RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
        string(REPLACE "/RTC1 " "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
        string(APPEND CMAKE_CXX_FLAGS_DEBUG " /fsanitize=address")
    else()
        string(APPEND CMAKE_CXX_FLAGS_DEBUG " -fsanitize=address")
    endif()
endif()

message(STATUS "CMAKE_CXX_FLAGS_DEBUG: ${CMAKE_CXX_FLAGS_DEBUG}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(target_name gram_grep)

set(SOURCES
args.cpp
main.cpp
parser.cpp
search.cpp
types.cpp
)

set(HEADERS
args.hpp
colours.hpp
gg_error.hpp
parser.hpp
search.hpp
types.hpp
)

if(WIN32)
    # Set the gram_grep project as the default in the solution file
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT gram_grep)
endif()

include_directories(${target_name} PRIVATE "../lexertl17/include")
include_directories(${target_name} PRIVATE "../parsertl17/include")
include_directories(${target_name} PRIVATE "../wildcardtl/include")

add_executable(${target_name} ${SOURCES} ${HEADERS})

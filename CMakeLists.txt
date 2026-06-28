cmake_minimum_required(VERSION 4.1)
project(elpris)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)

include_directories(include) 

add_executable(elpris
    src/main.cpp
    src/fetch.cpp
    src/parse.cpp
)

target_link_libraries(elpris CURL::libcurl)
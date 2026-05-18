cmake_minimum_required(VERSION 3.10)
project(tierscaped LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(tierscaped
    main.cpp
    config.cpp
    sanity.cpp
    region.cpp
    sampler.cpp
    classifier.cpp
    migrator.cpp
    util.cpp
)

target_link_libraries(tierscaped PRIVATE numa pthread)

install(TARGETS tierscaped DESTINATION bin)

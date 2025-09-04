# This file is just an orchestration
cmake_minimum_required(VERSION 3.18)

option(CMAKE_EXPORT_COMPILE_COMMANDS "Export compile command" ON)

project(skd_ilp_server VERSION 1.0 LANGUAGES CXX)
message(STATUS "${PROJECT_NAME} version: ${PROJECT_VERSION}")


find_package(ortools CONFIG REQUIRED)
message(STATUS "Using OR-Tools: ${ortools_DIR}")


# enable full optimization and disable debug mode
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function")



file(GLOB_RECURSE SOURCE_FILES
"${CMAKE_CURRENT_SOURCE_DIR}/model/*.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/utils/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/server.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/skd_ilp.cpp"
)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})


# add_executable(${PROJECT_NAME} server.cpp skd_ilp.cpp utils/ model/)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

target_link_libraries(${PROJECT_NAME} 
	PRIVATE 
	ortools::ortools
	)


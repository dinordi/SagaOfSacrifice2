cmake_minimum_required(VERSION 3.10)

project(SOS)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

# Zet standaard build type op Debug als er nog geen build type is gekozen
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Voeg debug flags toe voor Debug build (geen optimalisatie, debug symbols)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")

# Find Boost libraries
find_package(Boost REQUIRED COMPONENTS system thread chrono)
message(STATUS "Boost include dirs found: ${Boost_INCLUDE_DIRS}") 
message(STATUS "Boost libraries found: ${Boost_LIBRARIES}")

# Add pthread
find_package(Threads REQUIRED)

# Find the PNG package
find_package(PNG REQUIRED)

# Find SDL2 packages
find_package(SDL2 REQUIRED)
find_package(SDL2_mixer REQUIRED)

# Collect all source files
file(GLOB_RECURSE SOS_LIB_SOURCES "${CMAKE_CURRENT_LIST_DIR}/../SOS/src/*.cpp")
file(GLOB_RECURSE CLIENT_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp")

# Create the executable target with sources
add_executable(SOS ${SOS_LIB_SOURCES} ${CLIENT_SOURCES} "${CMAKE_CURRENT_LIST_DIR}/main.cpp")

# Add include directories
target_include_directories(SOS PRIVATE
    ${PROJECT_SOURCE_DIR}/../SOS/include
    ${PROJECT_SOURCE_DIR}/../SOS/include/logging
    ${PROJECT_SOURCE_DIR}/../SOS/include/interfaces
    ${PROJECT_SOURCE_DIR}/../SOS/include/collision
    ${PROJECT_SOURCE_DIR}/../SOS/include/objects
    ${PROJECT_SOURCE_DIR}/../SOS/include/utils
    ${PROJECT_SOURCE_DIR}/../SOS/include/petalinux
    ${PROJECT_SOURCE_DIR}/../SOS/include/network
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/../SOS/src
    ${CMAKE_CURRENT_LIST_DIR}/../SOS/src/interfaces
    ${CMAKE_CURRENT_LIST_DIR}/../SOS/src/collision
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${Boost_INCLUDE_DIRS}
    ${PNG_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(SOS PRIVATE
    ${PNG_LIBRARIES} 
    ${SDL2_LIBRARIES} 
    ${Boost_LIBRARIES}
    ${SDL2_MIXER_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

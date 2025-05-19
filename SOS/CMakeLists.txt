# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.10)

# Set the project name
project(SOS)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set CMAKE_MODULE_PATH to find custom Find*.cmake modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

# Find the PNG package
find_package(PNG REQUIRED)
include_directories(${PNG_INCLUDE_DIRS})

# Find SDL2 packages
find_package(SDL2 REQUIRED)
find_package(SDL2_mixer REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

# Find Boost libraries
find_package(Boost COMPONENTS system REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

# Add include directories
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/include/interfaces)
include_directories(${PROJECT_SOURCE_DIR}/include/fpga)
include_directories(${PROJECT_SOURCE_DIR}/include/objects)
include_directories(${PROJECT_SOURCE_DIR}/include/logging)
include_directories(${PROJECT_SOURCE_DIR}/include/network)
include_directories(${PROJECT_SOURCE_DIR}/include/level)
include_directories(${PROJECT_SOURCE_DIR}/include/level_manager)
include_directories(${PROJECT_SOURCE_DIR}/src)

# Echo the include directories for debugging
message(STATUS "Include directories: ${PROJECT_SOURCE_DIR}/include")
message(STATUS "Include directories: ${PROJECT_SOURCE_DIR}/src")

# Add source files
set(SOS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Object.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/objects/platform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/objects/entity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/objects/player.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/objects/enemy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/network/AsioNetworkClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/network/MultiplayerManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/TimeUtils.cpp
    # Add more source files as needed
)

# Add executable
add_executable(SOS ${SOS_SOURCES})

# Link libraries (after add_executable)
target_link_libraries(SOS 
    ${PNG_LIBRARIES} 
    ${SDL2_LIBRARIES} 
    SDL2_mixer 
    ${Boost_LIBRARIES}
)

# Add installation targets
install(TARGETS SOS DESTINATION bin)

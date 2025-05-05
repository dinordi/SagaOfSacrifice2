# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.10)

# Set the project name
project(SOS)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)


set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

# Find Boost libraries
find_package(Boost REQUIRED COMPONENTS system thread chrono)
include_directories(${Boost_INCLUDE_DIRS})

# Add pthread
find_package(Threads REQUIRED)

# Find the PNG package
find_package(PNG REQUIRED)
include_directories(${PNG_INCLUDE_DIRS})

# Find SDL2 packages
find_package(SDL2 REQUIRED)
find_package(SDL2_mixer REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})


# Add include directories
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/include/interfaces)
include_directories(${PROJECT_SOURCE_DIR}/include/petalinux)
include_directories(${PROJECT_SOURCE_DIR}/include/objects)
include_directories(${PROJECT_SOURCE_DIR}/include/logging)
include_directories(${PROJECT_SOURCE_DIR}/include/network)
include_directories(${PROJECT_SOURCE_DIR}/src)

# Echo the include directories
message("Include directories: ${PROJECT_SOURCE_DIR}/include")
message("Include directories: ${PROJECT_SOURCE_DIR}/src")

# Get all source files
file(GLOB_RECURSE SOS_SOURCES 
    "src/*.cpp"
    "src/*/*.cpp"
    "src/*/*/*.cpp"
    "main.cpp"
)

# Add executable
add_executable(SOS ${SOS_SOURCES})

# Link libraries (moved after add_executable)
target_link_libraries(SOS 
    ${PNG_LIBRARIES} 
    ${SDL2_LIBRARIES} 
    ${Boost_LIBRARIES}
    ${SDL2_MIXER_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)
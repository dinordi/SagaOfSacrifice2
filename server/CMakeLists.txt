cmake_minimum_required(VERSION 3.10)

project(SagaOfSacrificeServer)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Find Boost libraries
find_package(Boost COMPONENTS system thread REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

# Add include directories
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/src)

# Add source files
file(GLOB_RECURSE SOURCES
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
)

# Add executable
add_executable(SagaServer ${PROJECT_SOURCE_DIR}/main.cpp ${SOURCES})

# Link libraries
target_link_libraries(SagaServer ${Boost_LIBRARIES})
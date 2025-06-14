cmake_minimum_required(VERSION 3.10)

project(SagaOfSacrificeServer)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(MSVC)
    # Boost configuration - use consistent path separators and naming
    set(BOOST_ROOT "C:/local/boost_1_87_0" CACHE PATH "Boost root directory")
    set(Boost_NO_SYSTEM_PATHS ON)
    set(Boost_NO_BOOST_CMAKE ON)
    set(Boost_USE_STATIC_LIBS ON)
    set(Boost_USE_MULTITHREADED ON)
    set(Boost_USE_DEBUG_LIBS OFF)
    set(Boost_USE_RELEASE_LIBS ON)
    set(Boost_DEBUG ON)
    set(Boost_DETAILED_FAILURE_MSG ON)
endif()

# The Boost install may not have "include" subfolder - check directory structure
set(BOOST_INCLUDEDIR "${BOOST_ROOT}" CACHE PATH "Boost include directory")
set(BOOST_LIBRARYDIR "${BOOST_ROOT}/lib64-msvc-14.3" CACHE PATH "Boost library directory")

# Find Boost libraries
find_package(Boost COMPONENTS system thread chrono REQUIRED)

# Print debug info
message(STATUS "Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
message(STATUS "Boost_LIBRARY_DIRS: ${Boost_LIBRARY_DIRS}")
message(STATUS "Boost_LIBRARIES: ${Boost_LIBRARIES}")

# If docker build
if(DEFINED ENV{DOCKER_BUILD})
    message(STATUS "Building in Docker")
    # Add include directories
    include_directories(${Boost_INCLUDE_DIRS})
    include_directories(${PROJECT_SOURCE_DIR}/SOS/include)
    include_directories(${PROJECT_SOURCE_DIR}/SOS/include/network)
    include_directories(${PROJECT_SOURCE_DIR}/SOS/include/objects)
    include_directories(${PROJECT_SOURCE_DIR}/SOS/include/interfaces)
    include_directories(${PROJECT_SOURCE_DIR}/SOS/include/logging)
else()
    # Add include directories
    include_directories(${Boost_INCLUDE_DIRS})
    include_directories(${PROJECT_SOURCE_DIR}/../SOS/include)
    include_directories(${PROJECT_SOURCE_DIR}/../SOS/include/network)
    include_directories(${PROJECT_SOURCE_DIR}/../SOS/include/objects)
    include_directories(${PROJECT_SOURCE_DIR}/../SOS/include/interfaces)
    include_directories(${PROJECT_SOURCE_DIR}/../SOS/include/logging)
    # include_directories(${PROJECT_SOURCE_DIR}/src)
endif()


# Add source files
file(GLOB_RECURSE SOURCES
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
)

if(DEFINED ENV{DOCKER_BUILD})
    # Add source files for Docker build
    file(GLOB_RECURSE SOS_SOURCES
        "${PROJECT_SOURCE_DIR}/SOS/src/*.cpp"
    )
else()
    # Add source files for non-Docker build
    file(GLOB_RECURSE SOS_SOURCES
        "${PROJECT_SOURCE_DIR}/../SOS/src/*.cpp"
    )
endif()

# Add executable
add_executable(SagaServer ${PROJECT_SOURCE_DIR}/main.cpp ${SOURCES} ${SOS_SOURCES})

# Link libraries
target_link_libraries(SagaServer PRIVATE ${Boost_LIBRARIES})
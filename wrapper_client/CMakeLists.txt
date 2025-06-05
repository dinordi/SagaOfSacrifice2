cmake_minimum_required(VERSION 3.16)


set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

# Conditional policies for newer CMake versions
if(POLICY CMP0177)
    cmake_policy(SET CMP0177 NEW)
endif()

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()
# set the output directory for built objects.
# This makes sure that the dynamic library goes into the build directory automatically.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

# prevent installing to system directories. 
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}" CACHE INTERNAL "")

if(APPLE)
    set(CMAKE_C_COMPILER /usr/bin/clang)
    set(CMAKE_CXX_COMPILER /usr/bin/clang++)
endif()

# Declare the project
project(SagaOfSacrifice2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # Disable compiler-specific extensions

if (APPLE OR EMSCRIPTEN)
    set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")    # Disable shared builds on platforms where it does not make sense to use them
endif()

if(MSVC)
    if(NOT CMAKE_GENERATOR STREQUAL "Ninja")
        add_definitions(/MP)				# parallelize each target, unless Ninja is the generator
    endif()
endif()

# Set the name of the executable
set(EXECUTABLE_NAME ${PROJECT_NAME})

# Create an executable or a shared library based on the platform and add our sources to it
if (ANDROID)
    # The SDL java code is hardcoded to load libmain.so on android, so we need to change EXECUTABLE_NAME
    set(EXECUTABLE_NAME main)
    add_library(${EXECUTABLE_NAME} SHARED)
else()
    add_executable(${EXECUTABLE_NAME})
endif()

if(MSVC)
    # Boost configuration - use consistent path separators and naming
    set(BOOST_ROOT "C:/local/boost_1_87_0" CACHE PATH "Boost root directory")
    set(Boost_NO_SYSTEM_PATHS ON)
    set(Boost_NO_BOOST_CMAKE ON)
    set(Boost_USE_STATIC_LIBS ON)
    set(Boost_USE_MULTITHREADED ON)
    set(Boost_USE_DEBUG_LIBS ON)
    set(Boost_USE_RELEASE_LIBS ON)
    set(Boost_DEBUG ON)
    set(Boost_DETAILED_FAILURE_MSG ON)

    # The Boost install may not have "include" subfolder - check directory structure
    set(BOOST_INCLUDEDIR "${BOOST_ROOT}" CACHE PATH "Boost include directory")
    set(BOOST_LIBRARYDIR "${BOOST_ROOT}/lib64-msvc-14.3" CACHE PATH "Boost library directory")
endif()

# Find Boost libraries
# Hint for Homebrew on Apple Silicon if find_package fails automatically
find_package(Boost COMPONENTS system thread chrono REQUIRED) # Added thread component for server/client
message(STATUS "Boost include dirs found: ${Boost_INCLUDE_DIRS}") # Check if path is found
message(STATUS "Boost libraries found: ${Boost_LIBRARIES}")     # Check libraries


include_directories(${PROJECT_SOURCE_DIR}/../SOS/include)
include_directories(${PROJECT_SOURCE_DIR}/../SOS/include/logging)
include_directories(${PROJECT_SOURCE_DIR}/../SOS/include/interfaces)
include_directories(${PROJECT_SOURCE_DIR}/../SOS/include/collision)
include_directories(${PROJECT_SOURCE_DIR}/../SOS/include/objects)
include_directories(${PROJECT_SOURCE_DIR}/../SOS/include/utils)
file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_LIST_DIR}/../SOS/src/*.cpp")

target_sources(${EXECUTABLE_NAME} PRIVATE ${SOURCES})

# Add your sources to the target
file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp")
target_sources(${EXECUTABLE_NAME} PRIVATE ${SOURCES})

target_include_directories(${EXECUTABLE_NAME} PRIVATE 
    ${CMAKE_CURRENT_LIST_DIR}/../SOS/src
    ${CMAKE_CURRENT_LIST_DIR}/../SOS/src/interfaces
    ${CMAKE_CURRENT_LIST_DIR}/../SOS/src/collision
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/src/game_overrides
    ${Boost_INCLUDE_DIRS}
)
# What is iosLaunchScreen.storyboard? This file describes what Apple's mobile platforms
# should show the user while the application is starting up. If you don't include one,
# then you get placed in a compatibility mode that does not allow HighDPI.
# This file is referenced inside Info.plist.in, where it is marked as the launch screen file.
# It is also ignored on non-Apple platforms. 

# To get an app icon on Apple platforms, add it to your executable.
# Afterward, the image file in Info.plist.in.
if(APPLE)
    target_sources("${EXECUTABLE_NAME}" PRIVATE "src/logo.png")
endif()

# Set C++ version
target_compile_features(${EXECUTABLE_NAME} PUBLIC cxx_std_20)

# on Web targets, we need CMake to generate a HTML webpage. 
if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html" CACHE INTERNAL "")
endif()

# Configure SDL by calling its CMake file.
# we use EXCLUDE_FROM_ALL so that its install targets and configs don't
# pollute upwards into our configuration.
add_subdirectory(SDL EXCLUDE_FROM_ALL)

# If you don't want SDL_ttf, then remove this section.
set(SDLTTF_VENDORED ON) # tell SDL_ttf to build its own dependencies
add_subdirectory(SDL_ttf EXCLUDE_FROM_ALL)	

# SDL_mixer (used for playing audio)
set(SDLMIXER_MIDI_NATIVE OFF)     # disable formats we don't use to make the build faster and smaller. Also some of these don't work on all platforms so you'll need to do some experimentation.
set(SDLMIXER_GME OFF)
set(SDLMIXER_WAVPACK OFF)     
set(SDLMIXER_MOD OFF)
set(SDLMIXER_OPUS OFF)
set(SDLMIXER_VENDORED ON)   # tell SDL_mixer to build its own dependencies
add_subdirectory(SDL_mixer EXCLUDE_FROM_ALL)

# SDL_image (used for loading various image formats)
set(SDLIMAGE_VENDORED ON)
set(SDLIMAGE_AVIF OFF)	# disable formats we don't use to make the build faster and smaller.
set(SDLIMAGE_BMP OFF)
set(SDLIMAGE_JPEG OFF)
set(SDLIMAGE_WEBP OFF)
add_subdirectory(SDL_image EXCLUDE_FROM_ALL)

# Link SDL to our executable. This also makes its include directory available to us. 
target_link_libraries(${EXECUTABLE_NAME} PUBLIC 
    SDL3_ttf::SDL3_ttf      # remove if you are not using SDL_ttf
    SDL3_mixer::SDL3_mixer  # remove if you are not using SDL_mixer
    SDL3_image::SDL3_image	# remove if you are not using SDL_image
    SDL3::SDL3              # If using satelite libraries, SDL must be the last item in the list. 
    Boost::system
    Boost::thread
    Boost::chrono
)

if (APPLE)
    find_library(IO_LIB ImageIO)
    find_library(CS_LIB CoreServices)
    target_link_libraries(${EXECUTABLE_NAME} PUBLIC ${IO_LIB} ${CS_LIB})
endif()

target_compile_definitions(${EXECUTABLE_NAME} PUBLIC SDL_MAIN_USE_CALLBACKS)

# Add include directories
target_include_directories(${EXECUTABLE_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../SOS/src)


# Dealing with assets
# We have some non-code resources that our application needs in order to work. How we deal with those differs per platform.
if (APPLE)
    # on Apple targets, the application bundle has a "resources" subfolder where we can place our assets.
    # SDL_GetBasePath() gives us easy access to that location.
    set(input_root "${CMAKE_CURRENT_LIST_DIR}/src")
    macro(add_resource FILE)
        file(RELATIVE_PATH relpath "${input_root}" "${FILE}")
        get_filename_component(relpath "${relpath}" DIRECTORY)
        target_sources(${EXECUTABLE_NAME} PRIVATE ${FILE})
        set_property(SOURCE ${FILE} PROPERTY MACOSX_PACKAGE_LOCATION "Resources/${relpath}")
    endmacro()
    add_resource("${CMAKE_CURRENT_LIST_DIR}/src/Inter-VariableFont.ttf")
elseif(EMSCRIPTEN)
    # on the web, we have to put the files inside of the webassembly
    # somewhat unintuitively, this is done via a linker argument.
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE 
        "--preload-file \"${CMAKE_CURRENT_LIST_DIR}/src/Inter-VariableFont.ttf@/\""
    )
else()
    if (ANDROID)
        file(MAKE_DIRECTORY "${MOBILE_ASSETS_DIR}")    # we need to create the project Assets dir otherwise CMake won't copy our assets.
    endif()

    macro(copy_helper filename)
        if (ANDROID)
            # MOBILE_ASSETS_DIR is set in the gradle file via the cmake command line and points to the Android Studio Assets folder. 
            # when we copy assets there, the Android build pipeline knows to add them to the apk. 
            set(outname "${MOBILE_ASSETS_DIR}/${filename}")    
        else()
            # for platforms that do not have a good packaging format, all we can do is copy the assets to the process working directory.
            set(outname "${CMAKE_BINARY_DIR}/$<CONFIGURATION>/${filename}")
        endif()
        add_custom_command(POST_BUILD
            TARGET "${EXECUTABLE_NAME}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_LIST_DIR}/src/${filename}" "${outname}"
            DEPENDS "${CMAKE_CURRENT_LIST_DIR}/src/${filename}"
        )
    endmacro()

    # Copy individual files
    copy_helper("Inter-VariableFont.ttf")
    # copy_helper("the_entertainer.ogg")
    # copy_helper("gs_tiger.svg")

    # Copy SOS/assets/ to windows Debug folder
    # First ensure the base SOS/assets directory exists
    add_custom_command(TARGET "${EXECUTABLE_NAME}" PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/$<CONFIGURATION>/SOS/assets"
        COMMENT "Creating SOS/assets directory"
    )
    
    set(OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    if(MSVC)
        file(GLOB_RECURSE ASSETS "${CMAKE_CURRENT_LIST_DIR}/../SOS/assets/*")
        foreach(ASSET ${ASSETS})
            # Get relative path from source assets dir to maintain folder structure
            file(RELATIVE_PATH ASSET_REL_PATH "${CMAKE_CURRENT_LIST_DIR}/../SOS/assets" "${ASSET}")
            get_filename_component(ASSET_DIR "${ASSET_REL_PATH}" DIRECTORY)
            
            if (ANDROID)
                # on Android, we need to copy the assets to the mobile assets directory.
                set(outname "${MOBILE_ASSETS_DIR}/${ASSET_REL_PATH}")
                # Create directory structure
                if(NOT "${ASSET_DIR}" STREQUAL "")
                    add_custom_command(TARGET "${EXECUTABLE_NAME}" PRE_BUILD
                        COMMAND ${CMAKE_COMMAND} -E make_directory "${MOBILE_ASSETS_DIR}/${ASSET_DIR}"
                        COMMENT "Creating directory ${MOBILE_ASSETS_DIR}/${ASSET_DIR}"
                    )
                endif()
            else()
                # for platforms that do not have a good packaging format, all we can do is copy the assets to the process working directory.
                set(outname "${OUTPUT_DIR}/SOS/assets/${ASSET_REL_PATH}")
                
                # Create directory structure if needed
                if(NOT "${ASSET_DIR}" STREQUAL "")
                    add_custom_command(TARGET "${EXECUTABLE_NAME}" PRE_BUILD
                        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/$<CONFIGURATION>/SOS/assets/${ASSET_DIR}"
                        COMMENT "Creating directory SOS/assets/${ASSET_DIR}"
                    )
                endif()
            endif()
            
            # Copy the file with echo
            add_custom_command(POST_BUILD
                TARGET "${EXECUTABLE_NAME}"
                COMMAND ${CMAKE_COMMAND} -E echo "Copying ${ASSET} to ${outname}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ASSET}" "${outname}"
                DEPENDS "${ASSET}"
            )
        endforeach()
    endif()
endif()

# set some extra configs for each platform
set_target_properties(${EXECUTABLE_NAME} PROPERTIES 
    # On macOS, make a proper .app bundle instead of a bare executable
    MACOSX_BUNDLE TRUE
    # Set the Info.plist file for Apple Mobile platforms. Without this file, your app
    # will not launch. 
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/src/Info.plist.in"

    # in Xcode, create a Scheme in the schemes dropdown for the app.
    XCODE_GENERATE_SCHEME TRUE
    # Identification for Xcode
    XCODE_ATTRIBUTE_BUNDLE_IDENTIFIER "com.ravbug.sdl3-sample"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.ravbug.sdl3-sample"
    XCODE_ATTRIBUTE_CURRENTYEAR "${CURRENTYEAR}"
)

# on Visual Studio, set our app as the default project
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT "${EXECUTABLE_NAME}")

# On macOS Platforms, ensure that the bundle is valid for distribution by calling fixup_bundle.
# note that fixup_bundle does not work on iOS, so you will want to use static libraries 
# or manually copy dylibs and set rpaths
if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    # tell Install about the target, otherwise fixup won't know about the transitive dependencies
    install(TARGETS ${EXECUTABLE_NAME}
        BUNDLE DESTINATION ./install COMPONENT Runtime
           RUNTIME DESTINATION ./install/bin COMPONENT Runtime
    )
    
    set(DEP_DIR "${CMAKE_BINARY_DIR}")  # where to look for dependencies when fixing up
    INSTALL(CODE 
        "include(BundleUtilities)
        fixup_bundle(\"$<TARGET_BUNDLE_DIR:${EXECUTABLE_NAME}>\" \"\" \"${DEP_DIR}\")
        " 
    )
    set(CPACK_GENERATOR "DragNDrop")
    include(CPack)
endif()
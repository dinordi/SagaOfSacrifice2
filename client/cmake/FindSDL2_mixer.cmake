# FindSDL2_mixer.cmake
#
# Custom module to find SDL2_mixer

# Look for SDL2_mixer using pkg-config first
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_SDL2_MIXER QUIET SDL2_mixer)
endif()

# Find the SDL2_mixer include directory
find_path(SDL2_MIXER_INCLUDE_DIR
  NAMES SDL_mixer.h
  PATH_SUFFIXES SDL2 include/SDL2 include
  HINTS
    ${PC_SDL2_MIXER_INCLUDEDIR}
    ${PC_SDL2_MIXER_INCLUDE_DIRS}
    $ENV{SDL2_MIXER_DIR}
    ${SDL2_DIR}
  )

# Find the SDL2_mixer library
find_library(SDL2_MIXER_LIBRARY
  NAMES SDL2_mixer
  PATH_SUFFIXES lib64 lib
  HINTS
    ${PC_SDL2_MIXER_LIBDIR}
    ${PC_SDL2_MIXER_LIBRARY_DIRS}
    $ENV{SDL2_MIXER_DIR}
    ${SDL2_DIR}
  )

# Set version from pkg-config if available
if(PC_SDL2_MIXER_VERSION)
  set(SDL2_MIXER_VERSION_STRING ${PC_SDL2_MIXER_VERSION})
endif()

# Standard handling of the package arguments
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SDL2_mixer
  REQUIRED_VARS SDL2_MIXER_LIBRARY SDL2_MIXER_INCLUDE_DIR
  VERSION_VAR SDL2_MIXER_VERSION_STRING)

# Create imported target
if(SDL2_MIXER_FOUND AND NOT TARGET SDL2::Mixer)
  add_library(SDL2::Mixer UNKNOWN IMPORTED)
  set_target_properties(SDL2::Mixer PROPERTIES
    IMPORTED_LOCATION "${SDL2_MIXER_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${SDL2_MIXER_INCLUDE_DIR}")
endif()

# Set output variables
if(SDL2_MIXER_FOUND)
  set(SDL2_MIXER_LIBRARIES ${SDL2_MIXER_LIBRARY})
  set(SDL2_MIXER_INCLUDE_DIRS ${SDL2_MIXER_INCLUDE_DIR})
endif()

mark_as_advanced(SDL2_MIXER_INCLUDE_DIR SDL2_MIXER_LIBRARY)
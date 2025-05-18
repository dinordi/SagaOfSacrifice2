# Install script for directory: D:/github/SagaOfSacrifice2/wrapper_client/SDL_ttf/external/harfbuzz

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "D:/github/SagaOfSacrifice2/wrapper_client/build")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/harfbuzz" TYPE FILE FILES
    "D:/github/SagaOfSacrifice2/wrapper_client/SDL_ttf/external/harfbuzz/src/hb-aat-layout.h"
    "D:/github/SagaOfSacrifice2/wrapper_client/SDL_ttf/external/harfbuzz/src/hb-aat.h"
    "D:/github/SagaOfSacrifice2/wrapper_client/SDL_ttf/external/harfbuzz/src/hb-blob.h"
    "D:/github/SagaOfSacrifice2/wrapper_client/SDL_ttf/external/harfbuzz/src/hb-buffer.h"
    "D:/github/SagaOfSacrifice2/wrapper_client/SDL_ttf/external/harfbuzz/src/hb-common.h"
    "D:/github/SagaOfSacrifice2/wrapper_client/SDL_ttf/external/harfbuzz/src/hb-cplusplus.hh"
    "D:/github/SagaOfSacrifice2/wrapper_client/SDL_ttf/external/harfbuzz/src/hb-deprecated.h"
    "D:/github/SagaOfSacrifice2/wrapper_client/SDL_ttf/external/harfbuzz/src/hb-draw.h"
    "D:/github/SagaOfSacrifice2/wrapper_client/SDL_ttf/external/harfbuzz/src/hb-face.h"
    "D:/github/SagaOfSacrifice2/wrapper_client/SDL_ttf/external/harfbuzz/src/hb-font.h"
    "D:/github/SagaOfSacrifice2/wrapper_client/SDL_ttf/external/harfbuzz/src/hb-map.h"
    "D:/github/SagaOfSacrifice2/wrapper_client/SDL_ttf/external/harfbuzz/src/hb-ot-color.h"
    "D:/github/SagaOfSacrifice2/wrapper_client/SDL_ttf/external/harfbuzz/src/hb-ot-deprecated.h"
    "D:/github/SagaOfSacrifice2/wrapper_client/SDL_ttf/external/harfbuzz/src/hb-ot-font.h"
    "D:/github/SagaOfSacrifice2/wrapper_client/SDL_ttf/external/harfbuzz/src/hb-ot-layout.h"
    "D:/github/SagaOfSacrifice2/wrapper_client/SDL_ttf/external/harfbuzz/src/hb-ot-math.h"
    "D:/github/SagaOfSacrifice2/wrapper_client/SDL_ttf/external/harfbuzz/src/hb-ot-meta.h"
    "D:/github/SagaOfSacrifice2/wrapper_client/SDL_ttf/external/harfbuzz/src/hb-ot-metrics.h"
    "D:/github/SagaOfSacrifice2/wrapper_client/SDL_ttf/external/harfbuzz/src/hb-ot-name.h"
    "D:/github/SagaOfSacrifice2/wrapper_client/SDL_ttf/external/harfbuzz/src/hb-ot-shape.h"
    "D:/github/SagaOfSacrifice2/wrapper_client/SDL_ttf/external/harfbuzz/src/hb-ot-var.h"
    "D:/github/SagaOfSacrifice2/wrapper_client/SDL_ttf/external/harfbuzz/src/hb-ot.h"
    "D:/github/SagaOfSacrifice2/wrapper_client/SDL_ttf/external/harfbuzz/src/hb-paint.h"
    "D:/github/SagaOfSacrifice2/wrapper_client/SDL_ttf/external/harfbuzz/src/hb-set.h"
    "D:/github/SagaOfSacrifice2/wrapper_client/SDL_ttf/external/harfbuzz/src/hb-shape-plan.h"
    "D:/github/SagaOfSacrifice2/wrapper_client/SDL_ttf/external/harfbuzz/src/hb-shape.h"
    "D:/github/SagaOfSacrifice2/wrapper_client/SDL_ttf/external/harfbuzz/src/hb-style.h"
    "D:/github/SagaOfSacrifice2/wrapper_client/SDL_ttf/external/harfbuzz/src/hb-unicode.h"
    "D:/github/SagaOfSacrifice2/wrapper_client/SDL_ttf/external/harfbuzz/src/hb-version.h"
    "D:/github/SagaOfSacrifice2/wrapper_client/SDL_ttf/external/harfbuzz/src/hb.h"
    "D:/github/SagaOfSacrifice2/wrapper_client/SDL_ttf/external/harfbuzz/src/hb-ft.h"
    "D:/github/SagaOfSacrifice2/wrapper_client/SDL_ttf/external/harfbuzz/src/hb-gdi.h"
    "D:/github/SagaOfSacrifice2/wrapper_client/SDL_ttf/external/harfbuzz/src/hb-uniscribe.h"
    )
endif()


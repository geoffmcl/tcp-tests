# Locate SIMGEAR
# This module defines
# SG_LIBRARIES
# SG_INCLUDE_DIR, where to find the headers
#
# $SIMGEAR_DIR is an environment variable that would
# correspond to the ./configure --prefix=$SIMGEAR_DIR
# used in building SimGear.
#
# Created by Geoff McLane
# This was influenced by FG FindSimGear.cmake by James Turner.
# who in turn was influenced by the FindOpenAL.cmake module.

#=============================================================================
# Copyright 2005-2009 Kitware, Inc.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distributed this file outside of CMake, substitute the full
#  License text for the above reference.)

# Per my request, CMake should search for frameworks first in
# the following order:
# ~/Library/Frameworks/OpenAL.framework/Headers
# /Library/Frameworks/OpenAL.framework/Headers
# /System/Library/Frameworks/OpenAL.framework/Headers
#
# On OS X, this will prefer the Framework version (if found) over others.
# People will have to manually change the cache values of 
# OPENAL_LIBRARY to override this selection or set the CMake environment
# CMAKE_INCLUDE_PATH to modify the search paths.

include(SelectLibraryConfigurations)

if(UNIX AND APPLE)
message(STATUS "*** SimGear checking FRAMEWORK" )
set(save_FIND_FRAMEWORK ${CMAKE_FIND_FRAMEWORK})
set(CMAKE_FIND_FRAMEWORK ONLY)
FIND_PATH(SG_INCLUDE_DIR simgear/version.h
  PATH_SUFFIXES include/simgear include 
  PATHS
  ~/Library/Frameworks
  /Library/Frameworks
)
set(CMAKE_FIND_FRAMEWORK ${save_FIND_FRAMEWORK})
endif(UNIX AND APPLE)

if(NOT SG_INCLUDE_DIR)
    FIND_PATH(SG_INCLUDE_DIR simgear/version.h
      PATH_SUFFIXES include 
      HINTS $ENV{SIMGEAR_DIR}
      PATHS
      /usr/local
      /opt/local
      /usr
    )
endif()

message(STATUS "*** SimGear INCLUDE path ${SG_INCLUDE_DIR}" )

# check for dynamic framework on Mac ()
if(UNIX AND APPLE)
message(STATUS "*** SimGear checking FRAMEWORK" )
set(save_FIND_FRAMEWORK ${CMAKE_FIND_FRAMEWORK})
set(CMAKE_FIND_FRAMEWORK ONLY)
FIND_LIBRARY(SG_LIBRARIES
  NAMES sgbucket
  HINTS
  $ENV{SIMGEAR_DIR}
  PATHS
  ~/Library/Frameworks
  /Library/Frameworks
)
set(CMAKE_FIND_FRAMEWORK ${save_FIND_FRAMEWORK})
endif(UNIX AND APPLE)

macro(find_static_component comp libs)
    set(compLib "${comp}")

    string(TOUPPER "SG_${comp}" compLibBase)
    set( compLibName ${compLibBase}_LIBRARY )

    FIND_LIBRARY(${compLibName}_DEBUG
      NAMES ${compLib}d
      HINTS $ENV{SIMGEAR_DIR}
      PATH_SUFFIXES lib64 lib libs64 libs libs/Win32 libs/Win64
      PATHS
      /usr/local
      /usr
      /opt
    )
    FIND_LIBRARY(${compLibName}_RELEASE
      NAMES ${compLib}
      HINTS $ENV{SIMGEAR_DIR}
      PATH_SUFFIXES lib64 lib libs64 libs libs/Win32 libs/Win64
      PATHS
      /usr/local
      /usr
      /opt
    )
    select_library_configurations( ${compLibBase} )

    set(componentLibRelease ${${compLibName}_RELEASE})
    message(STATUS "*** Simgear ${compLibName}_RELEASE ${componentLibRelease}")
    set(componentLibDebug ${${compLibName}_DEBUG})
    message(STATUS "*** Simgear ${compLibName}_DEBUG ${componentLibDebug}")
    if (NOT ${compLibName}_DEBUG)
        if (NOT ${compLibName}_RELEASE)
            message(STATUS "*** Found SG ${componentLib}")
            list(APPEND ${libs} ${componentLibRelease})
        endif()
    else()
        list(APPEND ${libs} optimized ${componentLibRelease} debug ${componentLibDebug})
    endif()
endmacro()

##if(${SG_LIBRARIES} STREQUAL "SG_LIBRARIES-NOTFOUND")
##if(NOT UNIX AND APPLE)
    set(SG_LIBRARIES "") # clear value
    set(outDeps ${SG_FIND_COMPONENTS})
    message(STATUS "*** SG finding ${outDeps}")
    # look for traditional static libraries
    foreach(component ${outDeps})
        find_static_component(${component} SG_LIBRARIES)
    endforeach()
##endif()

include(FindPackageHandleStandardArgs)
message(STATUS "*** Find PACKAGE passed SG_LIBRARIES ${SG_LIBRARIES}, SG_INCLUDE_DIR ${SG_INCLUDE_DIR}")
FIND_PACKAGE_HANDLE_STANDARD_ARGS(SG DEFAULT_MSG SG_LIBRARIES SG_INCLUDE_DIR)

# eof - FindSG.cmake

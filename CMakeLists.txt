# 20151231 - re-named tcp-tests - all SG links removed!
# 20141118 - 20131015 - 20120705 - fgio root CMakeLists.txt 
cmake_minimum_required( VERSION 2.8.7 )

project(tcp-tests)

### option(USE_SIMGEAR "Include simgear in the compile and link process" OFF)
### option(ADD_METAR2  "Include metar2 in the compile"                   OFF)
### if(USE_SIMGEAR)
###    # We have some custom .cmake scripts not in the official distribution.
###    set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/CMakeModules;${CMAKE_MODULE_PATH}")
### endif()

if(CMAKE_COMPILER_IS_GNUCXX)
    set( WARNING_FLAGS -Wall )
endif(CMAKE_COMPILER_IS_GNUCXX)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang") 
   set( WARNING_FLAGS "-Wall -Wno-overloaded-virtual" )
endif() 

if (WIN32 AND MSVC)
    # turn off various warnings
    set(WARNING_FLAGS "${WARNING_FLAGS} /wd4996")
    # foreach(warning 4244 4251 4267 4275 4290 4786 4305)
    #    set(WARNING_FLAGS "${WARNING_FLAGS} /wd${warning}")
    #endforeach()

    set( EXTRA_FLAGS "-DNOMINMAX -D_USE_MATH_DEFINES -D_CRT_SECURE_NO_WARNINGS -D_SCL_SECURE_NO_WARNINGS -D__CRT_NONSTDC_NO_WARNINGS" )
    # if (${MSVC_VERSION} EQUAL 1600)
    #    list(APPEND EXTRA_LD_FLAGS "/FORCE:MULTIPLE" )
    # endif (${MSVC_VERSION} EQUAL 1600)
    ###set( NOMINMAX 1 )
    ### add_definitions( -DUL_MSVC -DOPENALSDK -DHAVE_CONFIG_H -DFREEGLUT_LIB_PRAGMAS -DUL_WIN32 -D_CRT_SECURE_NO_DEPRECATE -DFREEGLUT_STATIC -DNO_TRACKER_PORT )
else()
    ### list(APPEND EXTRA_LD_FLAGS -L.)
    set(WARNING_FLAGS "${WARNING_FLAGS} -Wno-unused-function")
    set(WARNING_FLAGS "${WARNING_FLAGS} -Wno-unused-variable")
endif()

# deal with config.h
configure_file( ${CMAKE_SOURCE_DIR}/config.h.in ${CMAKE_BINARY_DIR}/config.h )
include_directories( ${CMAKE_BINARY_DIR} )
add_definitions( -DHAVE_CONFIG_H )

include_directories( src )

if(USE_SIMGEAR)
    message(FATAL_ERROR "*** USE_SIMGEAR is ON! This is now too OLD to be relevant!!")
    find_package(SG REQUIRED sgdebug sgmisc)
    message(STATUS "*** SimGear SG_INCLUDE_DIR=${SG_INCLUDE_DIR}")
    message(STATUS "*** SimGear SG_LIBRARIES=${SG_LIBRARIES}")
    include_directories( SYSTEM "${SG_INCLUDE_DIR}" )
    ###list(APPEND add_LIBS "${SG_LIBRARY}")
    add_definitions( -DNO_OPENSCENEGRAPH_INTERFACE )
endif()

if (WIN32)
    list(APPEND add_LIBS ws2_32.lib)
endif ()

set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WARNING_FLAGS} ${EXTRA_FLAGS} -D_REENTRANT" )
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WARNING_FLAGS} ${EXTRA_FLAGS} -D_REENTRANT" )
set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${EXTRA_LD_FLAGS}" )

# to distinguish between debug and release lib
set( CMAKE_DEBUG_POSTFIX "d" )

macro(ADD_EXE name srcs libs)
   add_executable( ${name} ${srcs} )
   target_link_libraries( ${name} ${libs} )
   if (MSVC)
        set_target_properties( ${name} PROPERTIES DEBUG_POSTFIX d )
   endif()
   ### NO INSTALL NEEDED
   ###install( TARGETS ${name} DESTINATION bin )
endmacro()

set(simpserv_SRCS src/simp_server.cxx src/simp_common.hxx)

if (WIN32)
    if (ADD_METAR2)
        ADD_EXE( metar2 src/metar/metar_main.cxx "${add_LIBS}" )
    endif ()
    ADD_EXE( recvfrom src/recvfrom.cxx "${add_LIBS}" )
    ADD_EXE( sendto src/sendto.cxx "${add_LIBS}" )
endif()

# common unix/windows
ADD_EXE( simp_server "${simpserv_SRCS}" "${add_LIBS}" )
ADD_EXE( simp_client src/simp_client.cxx "${add_LIBS}" )
ADD_EXE( udpserver src/udpserver.cxx "${add_LIBS}" )
ADD_EXE( udpclient src/udpclient.cxx "${add_LIBS}" )
ADD_EXE( udpserver2 src/udpserver2.cxx "${add_LIBS}" )
ADD_EXE( udpclient2 src/udpclient2.cxx "${add_LIBS}" )
ADD_EXE( unix_server src/unix_server.cxx "${add_LIBS}" )
ADD_EXE( unix_client src/unix_client.cxx "${add_LIBS}" )

# ADD_EXE( httpget src/httpget.cxx "${add_LIBS}" )

ADD_EXE( nbserver src/nbserver.cxx "${add_LIBS}" )
ADD_EXE( nbclient src/nbclient.cxx "${add_LIBS}" )

# no install of these local only test/experimental applications

# 20151231 - copied from edbrowse project
set(name ebsocket)
set(dir src)
set(${name}_SRCS ${dir}/${name}.c ${dir}/${name}.h)
#ADD_EXE( ebsocket "${ebsocket_SRCS}" "${add_LIBS}" )
add_executable( ${name} ${${name}_SRCS} )
target_link_libraries( ${name} ${add_LIBS} )
if (MSVC)
    set_target_properties( ${name} PROPERTIES DEBUG_POSTFIX d )
endif()
set_target_properties(${name} PROPERTIES COMPILE_FLAGS "-DTEST_CONNECT")
# no install of these local only test/experimental applications

# 20160107 - Add WSAEnum... if WIN32
if (WIN32)
    set(name WSAenum)
    set(dir src)
    set(${name}_SRCS ${dir}/${name}.cxx ${dir}/${name}.hxx)
    add_executable( ${name} ${${name}_SRCS} )
    target_link_libraries( ${name} ${add_LIBS} )
    if (MSVC)
        set_target_properties( ${name} PROPERTIES DEBUG_POSTFIX d )
    endif()
    # no install of these local only test/experimental applications
endif ()

# 20160729 udp-send and udp-recv
set(name udp-send)
set(dir src)
set(${name}_SRCS ${dir}/${name}.cxx)
#ADD_EXE( ${name} "${ebsocket_SRCS}" "${add_LIBS}" )
add_executable( ${name} ${${name}_SRCS} )
target_link_libraries( ${name} ${add_LIBS} )
if (MSVC)
    set_target_properties( ${name} PROPERTIES DEBUG_POSTFIX d )
endif()
# no install of these local only test/experimental applications

set(name udp-recv)
set(dir src)
set(${name}_SRCS ${dir}/${name}.cxx)
#ADD_EXE( ${name} "${${name}_SRCS}" "${add_LIBS}" )
add_executable( ${name} ${${name}_SRCS} )
target_link_libraries( ${name} ${add_LIBS} )
if (MSVC)
    set_target_properties( ${name} PROPERTIES DEBUG_POSTFIX d )
endif()
# no install of these local only test/experimental applications

if (MSVC)
    # a UNICODE apps
    set(name udp-wsend)
    set(dir src)
    set(${name}_SRCS ${dir}/${name}.cxx)
    #ADD_EXE( ${name} "${${name}_SRCS}" "${add_LIBS}" )
    add_executable( ${name} ${${name}_SRCS} )
    target_link_libraries( ${name} ${add_LIBS} )
    if (MSVC)
        set_target_properties( ${name} PROPERTIES DEBUG_POSTFIX d )
    endif()
    # no install of these local only test/experimental applications
    
    set(name udp-wrecv)
    set(dir src)
    set(${name}_SRCS ${dir}/${name}.cxx)
    #ADD_EXE( ${name} "${${name}_SRCS}" "${add_LIBS}" )
    add_executable( ${name} ${${name}_SRCS} )
    target_link_libraries( ${name} ${add_LIBS} )
    if (MSVC)
        set_target_properties( ${name} PROPERTIES DEBUG_POSTFIX d )
    endif()
    # no install of these local only test/experimental applications

    # simple app - get local ip
    set(name getlocalip)
    set(dir src)
    set(${name}_SRCS ${dir}/${name}.cpp)
    #ADD_EXE( ${name} "${${name}_SRCS}" "${add_LIBS}" )
    add_executable( ${name} ${${name}_SRCS} )
    target_link_libraries( ${name} ${add_LIBS} )
    if (MSVC)
        set_target_properties( ${name} PROPERTIES DEBUG_POSTFIX d )
    endif()
    # no install of these local only test/experimental applications
    
endif ()

# eof

# generated by Buildyard, do not edit.

include(System)
list(APPEND FIND_PACKAGES_DEFINES ${SYSTEM})

find_package(hwloc 1.3)
find_package(DNSSD )
find_package(LibJpegTurbo 1.2.1)
find_package(OpenMP )
find_package(Boost 1.41.0 REQUIRED regex serialization)

if(EXISTS ${CMAKE_SOURCE_DIR}/CMake/FindPackagesPost.cmake)
  include(${CMAKE_SOURCE_DIR}/CMake/FindPackagesPost.cmake)
endif()

if(hwloc_FOUND)
  set(hwloc_name hwloc)
endif()
if(HWLOC_FOUND)
  set(hwloc_name HWLOC)
endif()
if(hwloc_name)
  list(APPEND FIND_PACKAGES_DEFINES LUNCHBOX_USE_HWLOC)
  set(FIND_PACKAGES_FOUND "${FIND_PACKAGES_FOUND} hwloc")
  link_directories(${${hwloc_name}_LIBRARY_DIRS})
  if(NOT "${${hwloc_name}_INCLUDE_DIRS}" MATCHES "-NOTFOUND")
    include_directories(${${hwloc_name}_INCLUDE_DIRS})
  endif()
endif()

if(DNSSD_FOUND)
  set(DNSSD_name DNSSD)
endif()
if(DNSSD_FOUND)
  set(DNSSD_name DNSSD)
endif()
if(DNSSD_name)
  list(APPEND FIND_PACKAGES_DEFINES LUNCHBOX_USE_DNSSD)
  set(FIND_PACKAGES_FOUND "${FIND_PACKAGES_FOUND} DNSSD")
  link_directories(${${DNSSD_name}_LIBRARY_DIRS})
  if(NOT "${${DNSSD_name}_INCLUDE_DIRS}" MATCHES "-NOTFOUND")
    include_directories(${${DNSSD_name}_INCLUDE_DIRS})
  endif()
endif()

if(LibJpegTurbo_FOUND)
  set(LibJpegTurbo_name LibJpegTurbo)
endif()
if(LIBJPEGTURBO_FOUND)
  set(LibJpegTurbo_name LIBJPEGTURBO)
endif()
if(LibJpegTurbo_name)
  list(APPEND FIND_PACKAGES_DEFINES LUNCHBOX_USE_LIBJPEGTURBO)
  set(FIND_PACKAGES_FOUND "${FIND_PACKAGES_FOUND} LibJpegTurbo")
  link_directories(${${LibJpegTurbo_name}_LIBRARY_DIRS})
  if(NOT "${${LibJpegTurbo_name}_INCLUDE_DIRS}" MATCHES "-NOTFOUND")
    include_directories(${${LibJpegTurbo_name}_INCLUDE_DIRS})
  endif()
endif()

if(OpenMP_FOUND)
  set(OpenMP_name OpenMP)
endif()
if(OPENMP_FOUND)
  set(OpenMP_name OPENMP)
endif()
if(OpenMP_name)
  list(APPEND FIND_PACKAGES_DEFINES LUNCHBOX_USE_OPENMP)
  set(FIND_PACKAGES_FOUND "${FIND_PACKAGES_FOUND} OpenMP")
  link_directories(${${OpenMP_name}_LIBRARY_DIRS})
  if(NOT "${${OpenMP_name}_INCLUDE_DIRS}" MATCHES "-NOTFOUND")
    include_directories(${${OpenMP_name}_INCLUDE_DIRS})
  endif()
endif()

if(Boost_FOUND)
  set(Boost_name Boost)
endif()
if(BOOST_FOUND)
  set(Boost_name BOOST)
endif()
if(Boost_name)
  list(APPEND FIND_PACKAGES_DEFINES LUNCHBOX_USE_BOOST)
  set(FIND_PACKAGES_FOUND "${FIND_PACKAGES_FOUND} Boost")
  link_directories(${${Boost_name}_LIBRARY_DIRS})
  if(NOT "${${Boost_name}_INCLUDE_DIRS}" MATCHES "-NOTFOUND")
    include_directories(SYSTEM ${${Boost_name}_INCLUDE_DIRS})
  endif()
endif()

set(LUNCHBOX_BUILD_DEBS autoconf;automake;cmake;git;git-svn;libavahi-compat-libdnssd-dev;libboost-regex-dev;libboost-serialization-dev;libhwloc-dev;libjpeg-turbo8-dev;libturbojpeg;pkg-config;subversion)

set(LUNCHBOX_DEPENDS hwloc;DNSSD;LibJpegTurbo;OpenMP;Boost)

# Write defines.h and options.cmake
if(NOT PROJECT_INCLUDE_NAME)
  set(PROJECT_INCLUDE_NAME ${CMAKE_PROJECT_NAME})
endif()
if(NOT OPTIONS_CMAKE)
  set(OPTIONS_CMAKE ${CMAKE_BINARY_DIR}/options.cmake)
endif()
set(DEFINES_FILE "${CMAKE_BINARY_DIR}/include/${PROJECT_INCLUDE_NAME}/defines${SYSTEM}.h")
set(DEFINES_FILE_IN ${DEFINES_FILE}.in)
file(WRITE ${DEFINES_FILE_IN}
  "// generated by CMake/FindPackages.cmake, do not edit.\n\n"
  "#ifndef ${CMAKE_PROJECT_NAME}_DEFINES_${SYSTEM}_H\n"
  "#define ${CMAKE_PROJECT_NAME}_DEFINES_${SYSTEM}_H\n\n")
file(WRITE ${OPTIONS_CMAKE} "# Optional modules enabled during build\n")
foreach(DEF ${FIND_PACKAGES_DEFINES})
  add_definitions(-D${DEF})
  file(APPEND ${DEFINES_FILE_IN}
  "#ifndef ${DEF}\n"
  "#  define ${DEF}\n"
  "#endif\n")
if(NOT DEF STREQUAL SYSTEM)
  file(APPEND ${OPTIONS_CMAKE} "set(${DEF} ON)\n")
endif()
endforeach()
file(APPEND ${DEFINES_FILE_IN}
  "\n#endif\n")

include(UpdateFile)
update_file(${DEFINES_FILE_IN} ${DEFINES_FILE})
if(Boost_FOUND) # another WAR for broken boost stuff...
  set(Boost_VERSION ${Boost_MAJOR_VERSION}.${Boost_MINOR_VERSION}.${Boost_SUBMINOR_VERSION})
endif()
if(CUDA_FOUND)
  string(REPLACE "-std=c++11" "" CUDA_HOST_FLAGS "${CUDA_HOST_FLAGS}")
  string(REPLACE "-std=c++0x" "" CUDA_HOST_FLAGS "${CUDA_HOST_FLAGS}")
endif()
if(FIND_PACKAGES_FOUND)
  if(MSVC)
    message(STATUS "Configured with ${FIND_PACKAGES_FOUND}")
  else()
    message(STATUS "Configured with ${CMAKE_BUILD_TYPE}${FIND_PACKAGES_FOUND}")
  endif()
endif()

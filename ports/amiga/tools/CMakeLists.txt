# capture: vAmiga's core and a program that records frames (see capture.cpp).
# Built by setup.sh, with vAmiga's sources in .build/src/vAmiga.
cmake_minimum_required(VERSION 3.16)
project(capture CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(VAMIGA ${CMAKE_CURRENT_SOURCE_DIR}/.build/src/vAmiga/Core)
add_subdirectory(${VAMIGA} vamiga EXCLUDE_FROM_ALL)
add_executable(capture capture.cpp ${VAMIGA}/vadebug.cpp)
target_link_libraries(capture PRIVATE VACore)

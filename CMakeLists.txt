cmake_minimum_required (VERSION 3.0)
project (ovuf)
set(NAME "ovuf")

find_path(CMAKE_FOLDER NAMES cmake/emp-tool-config.cmake)
include(${CMAKE_FOLDER}/cmake/emp-base.cmake)

find_package(emp-ot REQUIRED)
include_directories(${EMP-OT_INCLUDE_DIRS})
set(RELIC ON)
find_package(RELIC REQUIRED)
include_directories(${RELIC_INCLUDE_DIRS})
# Installation
install(FILES cmake/emp-sh2pc-config.cmake DESTINATION cmake/)
install(DIRECTORY emp-sh2pc DESTINATION include/)


ENABLE_TESTING()
ADD_SUBDIRECTORY(test)

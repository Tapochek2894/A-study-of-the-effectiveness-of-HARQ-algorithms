cmake_minimum_required(VERSION 3.16)

project(harq LANGUAGES CXX)

option(ENABLE_AFF3CT "Enable AFF3CT-based convolutional codec backend" OFF)
option(USE_SYSTEM_AFF3CT "Search for system-installed AFF3CT when ENABLE_AFF3CT=ON" ON)
option(FETCH_AFF3CT "Auto-fetch AFF3CT sources when not found in system/source dir" OFF)
set(AFF3CT_ROOT "" CACHE PATH "Optional root prefix for system AFF3CT lookup")
set(AFF3CT_SOURCE_DIR "" CACHE PATH "Optional local AFF3CT source directory (with CMakeLists.txt)")
set(AFF3CT_GIT_REPOSITORY "https://github.com/aff3ct/aff3ct.git" CACHE STRING "AFF3CT Git repository used when FETCH_AFF3CT=ON")
set(AFF3CT_GIT_TAG "master" CACHE STRING "AFF3CT Git tag/branch used when FETCH_AFF3CT=ON")
option(AFF3CT_FETCH_SUBMODULES "Initialize/update AFF3CT git submodules when using source/fetch mode" ON)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/Project.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/Tests.cmake)

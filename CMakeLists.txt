cmake_minimum_required(VERSION 3.16)

project(harq LANGUAGES CXX)

option(ENABLE_AFF3CT "Enable AFF3CT-based convolutional codec backend" OFF)
set(AFF3CT_GIT_REPOSITORY "https://github.com/aff3ct/aff3ct.git" CACHE STRING "AFF3CT Git repository used when ENABLE_AFF3CT=ON")
set(AFF3CT_GIT_TAG "master" CACHE STRING "AFF3CT Git tag/branch used when ENABLE_AFF3CT=ON")

include(${CMAKE_CURRENT_LIST_DIR}/cmake/Project.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/Tests.cmake)

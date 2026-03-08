cmake_minimum_required(VERSION 3.16)

project(harq LANGUAGES CXX)

option(ENABLE_AFF3CT "Enable AFF3CT-based convolutional codec backend" OFF)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/Project.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/Tests.cmake)

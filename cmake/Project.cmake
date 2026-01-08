set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(harq STATIC
    ${CMAKE_CURRENT_LIST_DIR}/../src/bpsk.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../src/bpsk_passband.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../src/chase_algorithm.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../src/utils.cpp
)

target_include_directories(harq PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/../include
)

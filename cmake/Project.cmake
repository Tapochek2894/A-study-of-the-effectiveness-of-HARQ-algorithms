set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE HARQ_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_LIST_DIR}/../src/*.cpp
)

add_library(harq STATIC ${HARQ_SOURCES})

target_include_directories(harq PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/../include
)

set(HARQ_AFF3CT_ENABLED 0)
if(ENABLE_AFF3CT)
    find_path(AFF3CT_INCLUDE_DIR aff3ct.hpp)
    find_library(AFF3CT_LIBRARY NAMES aff3ct)

    if(AFF3CT_INCLUDE_DIR AND AFF3CT_LIBRARY)
        target_include_directories(harq PUBLIC ${AFF3CT_INCLUDE_DIR})
        target_link_libraries(harq PUBLIC ${AFF3CT_LIBRARY})
        set(HARQ_AFF3CT_ENABLED 1)
        message(STATUS "AFF3CT enabled: ${AFF3CT_LIBRARY}")
    else()
        message(WARNING "ENABLE_AFF3CT=ON but AFF3CT not found. Convolutional backend will be disabled.")
    endif()
endif()

target_compile_definitions(harq PUBLIC HARQ_ENABLE_AFF3CT=${HARQ_AFF3CT_ENABLED})

add_executable(bpsk_passband_cloud
    ${CMAKE_CURRENT_LIST_DIR}/../tools/bpsk_passband_cloud.cpp
)

target_link_libraries(bpsk_passband_cloud PRIVATE harq)

add_executable(bpsk_awgn_sim
    ${CMAKE_CURRENT_LIST_DIR}/../tools/bpsk_awgn_sim.cpp
)

target_link_libraries(bpsk_awgn_sim PRIVATE harq)

add_executable(ber_bler_sim
    ${CMAKE_CURRENT_LIST_DIR}/../tools/ber_bler_sim.cpp
)

target_link_libraries(ber_bler_sim PRIVATE harq)

add_executable(crc_ber_bler_sim
    ${CMAKE_CURRENT_LIST_DIR}/../tools/crc_ber_bler_sim.cpp
)

target_link_libraries(crc_ber_bler_sim PRIVATE harq)
add_executable(chase_ham_sim
    ${CMAKE_CURRENT_LIST_DIR}/../tools/chase_ham_sim.cpp
)

target_link_libraries(chase_ham_sim PRIVATE harq)

add_executable(chase_comb_ham
    ${CMAKE_CURRENT_LIST_DIR}/../tools/chase_comb_ham.cpp
)

target_link_libraries(chase_comb_ham PRIVATE harq)

add_executable(qpsk_bpsk_sim
    ${CMAKE_CURRENT_LIST_DIR}/../tools/qpsk_bpsk.cpp
)

target_link_libraries(qpsk_bpsk_sim PRIVATE harq)

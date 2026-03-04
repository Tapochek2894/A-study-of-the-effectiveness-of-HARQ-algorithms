set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB HARQ_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_LIST_DIR}/../src/*.cpp
)

add_library(harq STATIC ${HARQ_SOURCES})

target_include_directories(harq PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/../include
)

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

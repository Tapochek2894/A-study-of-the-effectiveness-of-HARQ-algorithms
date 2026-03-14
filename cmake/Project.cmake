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
set(HARQ_AFF3CT_LINK_TARGET "")
set(HARQ_AFF3CT_INCLUDE_DIR "")

if(ENABLE_AFF3CT)
    find_package(Git QUIET)
    if(NOT Git_FOUND)
        message(FATAL_ERROR "ENABLE_AFF3CT=ON requires git to auto-download AFF3CT.")
    endif()

    set(_aff3ct_fetch_dir "${CMAKE_BINARY_DIR}/_deps/aff3ct-src")
    if(NOT EXISTS "${_aff3ct_fetch_dir}/CMakeLists.txt")
        message(STATUS "Fetching AFF3CT from ${AFF3CT_GIT_REPOSITORY} (${AFF3CT_GIT_TAG})...")
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" clone --depth 1 --branch "${AFF3CT_GIT_TAG}" --recurse-submodules "${AFF3CT_GIT_REPOSITORY}" "${_aff3ct_fetch_dir}"
            RESULT_VARIABLE _aff3ct_clone_rc
            OUTPUT_QUIET
            ERROR_VARIABLE _aff3ct_clone_err
        )
        if(NOT _aff3ct_clone_rc EQUAL 0)
            message(FATAL_ERROR "Failed to fetch AFF3CT: ${_aff3ct_clone_err}")
        endif()
    endif()

    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${_aff3ct_fetch_dir}" submodule update --init --recursive
        RESULT_VARIABLE _aff3ct_submodule_fetch_rc
        OUTPUT_QUIET
        ERROR_VARIABLE _aff3ct_submodule_fetch_err
    )
    if(NOT _aff3ct_submodule_fetch_rc EQUAL 0)
        message(FATAL_ERROR "AFF3CT submodule init failed: ${_aff3ct_submodule_fetch_err}")
    endif()

    if(NOT EXISTS "${_aff3ct_fetch_dir}/lib/streampu/CMakeLists.txt")
        message(FATAL_ERROR "Fetched AFF3CT tree is missing required submodules (expected lib/streampu).")
    endif()

    # Force AFF3CT library mode when used as a subproject.
    set(AFF3CT_COMPILE_EXE OFF CACHE BOOL "" FORCE)
    set(AFF3CT_COMPILE_STATIC_LIB ON CACHE BOOL "" FORCE)
    set(AFF3CT_COMPILE_SHARED_LIB OFF CACHE BOOL "" FORCE)

    add_subdirectory("${_aff3ct_fetch_dir}" "${CMAKE_BINARY_DIR}/_deps/aff3ct-fetch-build" EXCLUDE_FROM_ALL)
    if(TARGET aff3ct::aff3ct)
        set(HARQ_AFF3CT_LINK_TARGET aff3ct::aff3ct)
        set(HARQ_AFF3CT_ENABLED 1)
    elseif(TARGET aff3ct-static)
        set(HARQ_AFF3CT_LINK_TARGET aff3ct-static)
        set(HARQ_AFF3CT_ENABLED 1)
    elseif(TARGET aff3ct)
        set(HARQ_AFF3CT_LINK_TARGET aff3ct)
        set(HARQ_AFF3CT_ENABLED 1)
    elseif(TARGET aff3ct-static-lib)
        set(HARQ_AFF3CT_LINK_TARGET aff3ct-static-lib)
        set(HARQ_AFF3CT_ENABLED 1)
    elseif(TARGET aff3ct-shared-lib)
        set(HARQ_AFF3CT_LINK_TARGET aff3ct-shared-lib)
        set(HARQ_AFF3CT_ENABLED 1)
    else()
        message(FATAL_ERROR "Fetched AFF3CT source, but no known target found (aff3ct::aff3ct/aff3ct-static/aff3ct/aff3ct-static-lib/aff3ct-shared-lib).")
    endif()

    if(EXISTS "${_aff3ct_fetch_dir}/include")
        set(HARQ_AFF3CT_INCLUDE_DIR "${_aff3ct_fetch_dir}/include")
        target_include_directories(harq PUBLIC "${HARQ_AFF3CT_INCLUDE_DIR}")
    endif()

    target_link_libraries(harq PUBLIC "${HARQ_AFF3CT_LINK_TARGET}")
    message(STATUS "AFF3CT enabled via fetched target: ${HARQ_AFF3CT_LINK_TARGET}")
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

add_executable(qpsk_awgn_sim
    ${CMAKE_CURRENT_LIST_DIR}/../tools/qpsk_awgn_sim.cpp
)

target_link_libraries(qpsk_awgn_sim PRIVATE harq)
add_executable(arq_chase_sim
    ${CMAKE_CURRENT_LIST_DIR}/../tools/arq_chase_sim.cpp
)

target_link_libraries(arq_chase_sim PRIVATE harq)

add_executable(fec_arq_harq_sim
    ${CMAKE_CURRENT_LIST_DIR}/../tools/fec_arq_harq_sim.cpp
)

target_link_libraries(fec_arq_harq_sim PRIVATE harq)

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
set(HARQ_AFF3CT_LIBRARY "")

if(ENABLE_AFF3CT)
    find_package(Git QUIET)
    # 1) Try system-installed AFF3CT.
    if(USE_SYSTEM_AFF3CT)
        set(_aff3ct_roots "")
        if(DEFINED AFF3CT_ROOT AND NOT AFF3CT_ROOT STREQUAL "")
            list(APPEND _aff3ct_roots "${AFF3CT_ROOT}")
        endif()
        if(DEFINED ENV{AFF3CT_ROOT} AND NOT "$ENV{AFF3CT_ROOT}" STREQUAL "")
            list(APPEND _aff3ct_roots "$ENV{AFF3CT_ROOT}")
        endif()

        find_path(AFF3CT_INCLUDE_DIR
            NAMES aff3ct.hpp
            HINTS ${_aff3ct_roots}
            PATH_SUFFIXES include
        )
        find_library(AFF3CT_LIBRARY
            NAMES aff3ct
            HINTS ${_aff3ct_roots}
            PATH_SUFFIXES lib lib64
        )

        if(AFF3CT_INCLUDE_DIR AND AFF3CT_LIBRARY)
            set(HARQ_AFF3CT_ENABLED 1)
            set(HARQ_AFF3CT_INCLUDE_DIR "${AFF3CT_INCLUDE_DIR}")
            message(STATUS "AFF3CT found in system: ${AFF3CT_LIBRARY}")
        else()
            message(STATUS "System AFF3CT not found.")
        endif()
    endif()

    # 2) Try local AFF3CT source tree (vendored or manually provided).
    if(HARQ_AFF3CT_ENABLED EQUAL 0 AND DEFINED AFF3CT_SOURCE_DIR AND NOT AFF3CT_SOURCE_DIR STREQUAL "")
        if(EXISTS "${AFF3CT_SOURCE_DIR}/CMakeLists.txt")
            if(AFF3CT_FETCH_SUBMODULES)
                if(Git_FOUND AND EXISTS "${AFF3CT_SOURCE_DIR}/.git")
                    execute_process(
                        COMMAND "${GIT_EXECUTABLE}" -C "${AFF3CT_SOURCE_DIR}" submodule update --init --recursive
                        RESULT_VARIABLE _aff3ct_submodule_rc
                        OUTPUT_QUIET
                        ERROR_VARIABLE _aff3ct_submodule_err
                    )
                    if(NOT _aff3ct_submodule_rc EQUAL 0)
                        message(WARNING "AFF3CT_SOURCE_DIR submodule init failed: ${_aff3ct_submodule_err}")
                    endif()
                elseif(NOT Git_FOUND)
                    message(WARNING "AFF3CT_FETCH_SUBMODULES=ON but git was not found.")
                endif()
            endif()

            if(EXISTS "${AFF3CT_SOURCE_DIR}/lib/streampu/CMakeLists.txt")
                add_subdirectory("${AFF3CT_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/_deps/aff3ct-build" EXCLUDE_FROM_ALL)
                if(TARGET aff3ct::aff3ct)
                    set(HARQ_AFF3CT_LINK_TARGET aff3ct::aff3ct)
                    set(HARQ_AFF3CT_ENABLED 1)
                elseif(TARGET aff3ct-static)
                    set(HARQ_AFF3CT_LINK_TARGET aff3ct-static)
                    set(HARQ_AFF3CT_ENABLED 1)
                elseif(TARGET aff3ct)
                    set(HARQ_AFF3CT_LINK_TARGET aff3ct)
                    set(HARQ_AFF3CT_ENABLED 1)
                else()
                    message(WARNING "AFF3CT source added, but no known target found (aff3ct::aff3ct/aff3ct-static/aff3ct).")
                endif()
            else()
                message(WARNING "AFF3CT_SOURCE_DIR is missing required submodules (expected lib/streampu).")
            endif()

            if(HARQ_AFF3CT_ENABLED EQUAL 1 AND EXISTS "${AFF3CT_SOURCE_DIR}/include")
                set(HARQ_AFF3CT_INCLUDE_DIR "${AFF3CT_SOURCE_DIR}/include")
            endif()
        else()
            message(WARNING "AFF3CT_SOURCE_DIR is set but CMakeLists.txt not found: ${AFF3CT_SOURCE_DIR}")
        endif()
    endif()

    # 3) Optional auto-fetch AFF3CT sources.
    if(HARQ_AFF3CT_ENABLED EQUAL 0 AND FETCH_AFF3CT)
        set(_aff3ct_fetch_dir "${CMAKE_BINARY_DIR}/_deps/aff3ct-src")

        if(NOT EXISTS "${_aff3ct_fetch_dir}/CMakeLists.txt")
            if(NOT Git_FOUND)
                message(WARNING "FETCH_AFF3CT=ON but git was not found. Cannot fetch AFF3CT.")
            else()
                message(STATUS "Fetching AFF3CT from ${AFF3CT_GIT_REPOSITORY} (${AFF3CT_GIT_TAG})...")
                execute_process(
                    COMMAND "${GIT_EXECUTABLE}" clone --depth 1 --branch "${AFF3CT_GIT_TAG}" --recurse-submodules "${AFF3CT_GIT_REPOSITORY}" "${_aff3ct_fetch_dir}"
                    RESULT_VARIABLE _aff3ct_clone_rc
                    OUTPUT_QUIET
                    ERROR_VARIABLE _aff3ct_clone_err
                )
                if(NOT _aff3ct_clone_rc EQUAL 0)
                    message(WARNING "Failed to fetch AFF3CT: ${_aff3ct_clone_err}")
                endif()
            endif()
        endif()

        if(EXISTS "${_aff3ct_fetch_dir}/CMakeLists.txt")
            if(AFF3CT_FETCH_SUBMODULES)
                if(Git_FOUND)
                    execute_process(
                        COMMAND "${GIT_EXECUTABLE}" -C "${_aff3ct_fetch_dir}" submodule update --init --recursive
                        RESULT_VARIABLE _aff3ct_submodule_fetch_rc
                        OUTPUT_QUIET
                        ERROR_VARIABLE _aff3ct_submodule_fetch_err
                    )
                    if(NOT _aff3ct_submodule_fetch_rc EQUAL 0)
                        message(WARNING "AFF3CT fetched tree submodule init failed: ${_aff3ct_submodule_fetch_err}")
                    endif()
                else()
                    message(WARNING "AFF3CT_FETCH_SUBMODULES=ON but git was not found.")
                endif()
            endif()

            if(EXISTS "${_aff3ct_fetch_dir}/lib/streampu/CMakeLists.txt")
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
                else()
                    message(WARNING "Fetched AFF3CT source, but no known target found (aff3ct::aff3ct/aff3ct-static/aff3ct).")
                endif()
            else()
                message(WARNING "Fetched AFF3CT tree is missing required submodules (expected lib/streampu).")
            endif()

            if(HARQ_AFF3CT_ENABLED EQUAL 1 AND EXISTS "${_aff3ct_fetch_dir}/include")
                set(HARQ_AFF3CT_INCLUDE_DIR "${_aff3ct_fetch_dir}/include")
            endif()
        endif()
    endif()

    if(HARQ_AFF3CT_ENABLED EQUAL 1)
        if(NOT HARQ_AFF3CT_INCLUDE_DIR STREQUAL "")
            target_include_directories(harq PUBLIC "${HARQ_AFF3CT_INCLUDE_DIR}")
        endif()

        if(NOT HARQ_AFF3CT_LINK_TARGET STREQUAL "")
            target_link_libraries(harq PUBLIC "${HARQ_AFF3CT_LINK_TARGET}")
            message(STATUS "AFF3CT enabled via target: ${HARQ_AFF3CT_LINK_TARGET}")
        elseif(AFF3CT_LIBRARY)
            target_link_libraries(harq PUBLIC "${AFF3CT_LIBRARY}")
            message(STATUS "AFF3CT enabled via system library: ${AFF3CT_LIBRARY}")
        else()
            message(WARNING "AFF3CT resolved without link target/library. Disabling backend.")
            set(HARQ_AFF3CT_ENABLED 0)
        endif()
    else()
        message(WARNING "ENABLE_AFF3CT=ON but AFF3CT was not resolved. Convolutional backend will be disabled.")
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

add_executable(qpsk_awgn_sim
    ${CMAKE_CURRENT_LIST_DIR}/../tools/qpsk_awgn_sim.cpp
)

target_link_libraries(qpsk_awgn_sim PRIVATE harq)

include(CTest)
if(BUILD_TESTING)
    include(FetchContent)

    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    FetchContent_MakeAvailable(googletest)

    add_executable(bpsk_tests
        ${CMAKE_CURRENT_LIST_DIR}/../tests/bpsk_test.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../tests/bpsk_passband_test.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../tests/chase_algorithm_test.cpp
    )

    target_link_libraries(bpsk_tests
        PRIVATE
            harq
            GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(bpsk_tests)
endif()

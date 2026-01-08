include(CTest)
if(BUILD_TESTING)
    include(FetchContent)

    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    FetchContent_MakeAvailable(googletest)

    file(GLOB HARQ_TEST_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_LIST_DIR}/../tests/*_test.cpp
    )

    add_executable(bpsk_tests ${HARQ_TEST_SOURCES})

    target_link_libraries(bpsk_tests
        PRIVATE
            harq
            GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(bpsk_tests)
endif()

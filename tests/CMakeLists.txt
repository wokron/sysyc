add_executable(tests
    main.cpp
    test_test.cpp
)
include_directories(PRIVATE .)

add_test(NAME TestAll COMMAND tests)

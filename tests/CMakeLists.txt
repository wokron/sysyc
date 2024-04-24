add_executable(tests
    main.cpp
    test_test.cpp
    test_ast.cpp
)
target_include_directories(tests PRIVATE .)
target_link_libraries(tests libsysyc)

add_test(NAME TestAll COMMAND tests)

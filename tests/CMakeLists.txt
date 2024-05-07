aux_source_directory(. TEST_SOURCES)
add_executable(tests
    ${TEST_SOURCES}
)
target_include_directories(tests PRIVATE .)
target_link_libraries(tests libsysyc)

add_test(NAME TestAll COMMAND tests)

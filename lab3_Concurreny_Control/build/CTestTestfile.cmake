# CMake generated Testfile for 
# Source directory: /workspaces/buzzdb
# Build directory: /workspaces/buzzdb/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(buffer_manager_test "/workspaces/buzzdb/build/test/buffer_manager_test" "--gtest_color=yes" "--gtest_output=xml:/workspaces/buzzdb/build/test/unit_buffer_manager_test.xml")
set_tests_properties(buffer_manager_test PROPERTIES  _BACKTRACE_TRIPLES "/workspaces/buzzdb/test/CMakeLists.txt;57;add_test;/workspaces/buzzdb/test/CMakeLists.txt;0;;/workspaces/buzzdb/CMakeLists.txt;84;include;/workspaces/buzzdb/CMakeLists.txt;0;")
add_test(buffer_manager_test_valgrind "/usr/bin/valgrind" "--error-exitcode=1" "--leak-check=full" "--soname-synonyms=somalloc=*jemalloc*" "--trace-children=yes" "--track-origins=yes" "--suppressions=/workspaces/buzzdb/script/valgrind.supp" "/workspaces/buzzdb/build/test/buffer_manager_test_valgrind" "--gtest_color=yes" "--gtest_output=xml:/workspaces/buzzdb/build/test/unit_buffer_manager_test_valgrind.xml")
set_tests_properties(buffer_manager_test_valgrind PROPERTIES  _BACKTRACE_TRIPLES "/workspaces/buzzdb/test/CMakeLists.txt;64;add_test;/workspaces/buzzdb/test/CMakeLists.txt;0;;/workspaces/buzzdb/CMakeLists.txt;84;include;/workspaces/buzzdb/CMakeLists.txt;0;")

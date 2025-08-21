# CMake generated Testfile for 
# Source directory: /Users/chc/Desktop/db/lab4_Query_Optimizer
# Build directory: /Users/chc/Desktop/db/lab4_Query_Optimizer/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(optimizer_test "/Users/chc/Desktop/db/lab4_Query_Optimizer/build/test/optimizer_test" "--gtest_color=yes" "--gtest_output=xml:/Users/chc/Desktop/db/lab4_Query_Optimizer/build/test/unit_optimizer_test.xml")
set_tests_properties(optimizer_test PROPERTIES  _BACKTRACE_TRIPLES "/Users/chc/Desktop/db/lab4_Query_Optimizer/test/CMakeLists.txt;73;add_test;/Users/chc/Desktop/db/lab4_Query_Optimizer/test/CMakeLists.txt;0;;/Users/chc/Desktop/db/lab4_Query_Optimizer/CMakeLists.txt;81;include;/Users/chc/Desktop/db/lab4_Query_Optimizer/CMakeLists.txt;0;")
add_test(optimizer_test_valgrind "VALGRIND_BIN-NOTFOUND" "--error-exitcode=1" "--leak-check=full" "--soname-synonyms=somalloc=*jemalloc*" "--trace-children=yes" "--track-origins=yes" "--suppressions=/Users/chc/Desktop/db/lab4_Query_Optimizer/script/valgrind.supp" "/Users/chc/Desktop/db/lab4_Query_Optimizer/build/test/optimizer_test_valgrind" "--gtest_color=yes" "--gtest_output=xml:/Users/chc/Desktop/db/lab4_Query_Optimizer/build/test/unit_optimizer_test_valgrind.xml")
set_tests_properties(optimizer_test_valgrind PROPERTIES  _BACKTRACE_TRIPLES "/Users/chc/Desktop/db/lab4_Query_Optimizer/test/CMakeLists.txt;80;add_test;/Users/chc/Desktop/db/lab4_Query_Optimizer/test/CMakeLists.txt;0;;/Users/chc/Desktop/db/lab4_Query_Optimizer/CMakeLists.txt;81;include;/Users/chc/Desktop/db/lab4_Query_Optimizer/CMakeLists.txt;0;")

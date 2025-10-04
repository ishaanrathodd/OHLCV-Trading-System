# CMake generated Testfile for 
# Source directory: /Users/ishaanrathod/Code/Demo/tests
# Build directory: /Users/ishaanrathod/Code/Demo/build/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[TickDataTests]=] "/Users/ishaanrathod/Code/Demo/build/tests/test_tick_data")
set_tests_properties([=[TickDataTests]=] PROPERTIES  _BACKTRACE_TRIPLES "/Users/ishaanrathod/Code/Demo/tests/CMakeLists.txt;17;add_test;/Users/ishaanrathod/Code/Demo/tests/CMakeLists.txt;0;")
add_test([=[PerformanceTests]=] "/Users/ishaanrathod/Code/Demo/build/tests/perf_harness" "--validate-slo")
set_tests_properties([=[PerformanceTests]=] PROPERTIES  _BACKTRACE_TRIPLES "/Users/ishaanrathod/Code/Demo/tests/CMakeLists.txt;36;add_test;/Users/ishaanrathod/Code/Demo/tests/CMakeLists.txt;0;")

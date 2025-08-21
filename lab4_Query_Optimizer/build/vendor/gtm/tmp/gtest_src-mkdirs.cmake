# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/Users/chc/Desktop/db/lab4_Query_Optimizer/build/vendor/gtm/src/googletest/googletest")
  file(MAKE_DIRECTORY "/Users/chc/Desktop/db/lab4_Query_Optimizer/build/vendor/gtm/src/googletest/googletest")
endif()
file(MAKE_DIRECTORY
  "/Users/chc/Desktop/db/lab4_Query_Optimizer/build/vendor/gtm/src/gtest_src-build"
  "/Users/chc/Desktop/db/lab4_Query_Optimizer/build/vendor/gtm/gtest"
  "/Users/chc/Desktop/db/lab4_Query_Optimizer/build/vendor/gtm/tmp"
  "/Users/chc/Desktop/db/lab4_Query_Optimizer/build/vendor/gtm/src/gtest_src-stamp"
  "/Users/chc/Desktop/db/lab4_Query_Optimizer/build/vendor/gtm/src"
  "/Users/chc/Desktop/db/lab4_Query_Optimizer/build/vendor/gtm/src/gtest_src-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/chc/Desktop/db/lab4_Query_Optimizer/build/vendor/gtm/src/gtest_src-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/chc/Desktop/db/lab4_Query_Optimizer/build/vendor/gtm/src/gtest_src-stamp${cfgdir}") # cfgdir has leading slash
endif()

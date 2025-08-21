# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/Users/chc/Desktop/db/lab4_Query_Optimizer/build/vendor/rapidjson/src/rapidjson_src")
  file(MAKE_DIRECTORY "/Users/chc/Desktop/db/lab4_Query_Optimizer/build/vendor/rapidjson/src/rapidjson_src")
endif()
file(MAKE_DIRECTORY
  "/Users/chc/Desktop/db/lab4_Query_Optimizer/build/vendor/rapidjson/src/rapidjson_src-build"
  "/Users/chc/Desktop/db/lab4_Query_Optimizer/build/vendor/rapidjson"
  "/Users/chc/Desktop/db/lab4_Query_Optimizer/build/vendor/rapidjson/tmp"
  "/Users/chc/Desktop/db/lab4_Query_Optimizer/build/vendor/rapidjson/src/rapidjson_src-stamp"
  "/Users/chc/Desktop/db/lab4_Query_Optimizer/build/vendor/rapidjson/src"
  "/Users/chc/Desktop/db/lab4_Query_Optimizer/build/vendor/rapidjson/src/rapidjson_src-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/chc/Desktop/db/lab4_Query_Optimizer/build/vendor/rapidjson/src/rapidjson_src-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/chc/Desktop/db/lab4_Query_Optimizer/build/vendor/rapidjson/src/rapidjson_src-stamp${cfgdir}") # cfgdir has leading slash
endif()

# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/Users/chc/Library/Mobile Documents/com~apple~CloudDocs/Master/2025 Spring/ADSI/assignment/lab1/buzzdb/build/vendor/gflags/src/gflags_src")
  file(MAKE_DIRECTORY "/Users/chc/Library/Mobile Documents/com~apple~CloudDocs/Master/2025 Spring/ADSI/assignment/lab1/buzzdb/build/vendor/gflags/src/gflags_src")
endif()
file(MAKE_DIRECTORY
  "/Users/chc/Library/Mobile Documents/com~apple~CloudDocs/Master/2025 Spring/ADSI/assignment/lab1/buzzdb/build/vendor/gflags/src/gflags_src-build"
  "/Users/chc/Library/Mobile Documents/com~apple~CloudDocs/Master/2025 Spring/ADSI/assignment/lab1/buzzdb/build/vendor/gflags"
  "/Users/chc/Library/Mobile Documents/com~apple~CloudDocs/Master/2025 Spring/ADSI/assignment/lab1/buzzdb/build/vendor/gflags/tmp"
  "/Users/chc/Library/Mobile Documents/com~apple~CloudDocs/Master/2025 Spring/ADSI/assignment/lab1/buzzdb/build/vendor/gflags/src/gflags_src-stamp"
  "/Users/chc/Library/Mobile Documents/com~apple~CloudDocs/Master/2025 Spring/ADSI/assignment/lab1/buzzdb/build/vendor/gflags/src"
  "/Users/chc/Library/Mobile Documents/com~apple~CloudDocs/Master/2025 Spring/ADSI/assignment/lab1/buzzdb/build/vendor/gflags/src/gflags_src-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/chc/Library/Mobile Documents/com~apple~CloudDocs/Master/2025 Spring/ADSI/assignment/lab1/buzzdb/build/vendor/gflags/src/gflags_src-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/chc/Library/Mobile Documents/com~apple~CloudDocs/Master/2025 Spring/ADSI/assignment/lab1/buzzdb/build/vendor/gflags/src/gflags_src-stamp${cfgdir}") # cfgdir has leading slash
endif()

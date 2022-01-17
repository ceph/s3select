cmake_minimum_required(VERSION 3.0)

project(s3select)

find_package(Arrow QUIET)

if(Arrow_FOUND)
  message( "arrow is installed")
  add_definitions(-D_ARROW_EXIST)
endif()

set(CMAKE_CXX_FLAGS "-std=gnu++17 -ggdb -Wnon-virtual-dtor -Wreorder -Wunused-variable -Wtype-limits -Wsign-compare -Wmaybe-uninitialized -O3")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED)
find_package(GTest REQUIRED)

if(DEFINED ENV{NANO_SEC})
  add_definitions(-DBOOST_DATE_TIME_POSIX_TIME_STD_CONFIG)
endif()


set(REGEX_EN)
if(REGEX_EN MATCHES "HS")
  find_path(HYPERSCAN_INCLUDE_DIR NAMES hs/hs.h)
  find_library(HYPERSCAN_LIB1 NAMES hs)
  find_library(HYPERSCAN_LIB2 NAMES hs_runtime)
  set(HYPERSCAN_LIB $(HYPERSCAN_LIB1) $(HYPERSCAN_LIB2))
  if(HYPERSCAN_INCLUDE_DIR AND HYPERSCAN_LIB)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lhs")
    add_definitions(-DREGEX_HS)
    message(STATUS "Hyperscan is set as regex engine")
  else()
    message(WARNING "-- Hyperscan not found --")
  endif()
elseif(REGEX_EN MATCHES "RE2")
  find_path(RE2_INCLUDE_DIR NAMES re2/re2.h)
  find_library(RE2_LIB NAMES re2)
  if(RE2_INCLUDE_DIR AND RE2_LIB)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lre2")
    add_definitions(-DREGEX_RE2)
    message(STATUS "re2 is set as regex engine")
  else()
    message(WARNING "-- re2 not found --")
  endif()
endif()

enable_testing() 

add_subdirectory(example)
add_subdirectory(test)



cmake_minimum_required(VERSION 3.0)

project(s3select)

find_package(Arrow QUIET)

if(Arrow_FOUND)
  message( "arrow is installed")
  add_definitions(-D_ARROW_EXIST)
endif()

set(CMAKE_CXX_FLAGS "-std=gnu++17 -ggdb -Wnon-virtual-dtor -Wreorder -Wunused-variable -Wtype-limits -Wsign-compare -Wmaybe-uninitialized -lprofiler -O3")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED)
find_package(GTest REQUIRED)

if(DEFINED ENV{NANO_SEC})
  add_definitions(-DBOOST_DATE_TIME_POSIX_TIME_STD_CONFIG)
endif()

enable_testing() 

add_subdirectory(example)
add_subdirectory(test)



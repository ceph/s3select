cmake_minimum_required(VERSION 3.0)

project(s3select)

set(CMAKE_CXX_FLAGS "-std=gnu++17 -ggdb -Wnon-virtual-dtor -Wreorder -Wunused-variable -Wtype-limits -Wsign-compare -Wmaybe-uninitialized")
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

add_test(NAME run_my_test 
       COMMAND sh -c "../example/run_test.bash")


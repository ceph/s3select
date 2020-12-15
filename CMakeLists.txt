cmake_minimum_required(VERSION 3.0)

project(s3select)

set(CMAKE_CXX_FLAGS "-std=gnu++17 -ggdb -Wnon-virtual-dtor -Wreorder -Wno-unused-variable")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED)
find_package(GTest REQUIRED)

enable_testing() 

add_subdirectory(example)
add_subdirectory(test)

add_test(NAME run_my_test 
       COMMAND sh -c "../example/run_test.bash")


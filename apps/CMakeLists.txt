# version 3.11 or later of CMake or needed later for installing GoogleTest
# so let's require it now.
cmake_minimum_required(VERSION 3.5)

project(main)

add_executable(main main.cpp)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
# We need hello.h and the hello library
# target_link_libraries(main PRIVATE hello)
target_link_libraries(main PRIVATE Threads::Threads)

# Tell C++ compiler to use C++20 features. We don't actually use any of them.
target_compile_features(main PUBLIC cxx_std_20)

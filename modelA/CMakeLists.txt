# version 3.11 or later of CMake or needed later for installing GoogleTest
# so let's require it now.
cmake_minimum_required(VERSION 3.5)

project(modelA)

set(BUILD_SHARED_LIBS ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS True)

add_library(modelA src/modelA.cpp)

# PUBLIC needed to make both hello.h and hello library available elsewhere in project
target_include_directories(${PROJECT_NAME}
    PUBLIC ${PROJECT_SOURCE_DIR}/include)

# Tell compiler to use C++20 features. The code doesn't actually use any of them.
target_compile_features(modelA PUBLIC cxx_std_20)

add_custom_command(TARGET modelA POST_BUILD 
  COMMAND "${CMAKE_COMMAND}" -E copy 
     "$<TARGET_FILE:modelA>"
     "../apps/$<CONFIGURATION>/$<TARGET_FILE_NAME:modelA>" 
  COMMENT "Copying to output directory")

  add_custom_command(TARGET modelA POST_BUILD 
  COMMAND "${CMAKE_COMMAND}" -E copy 
     "$<TARGET_FILE:modelA>"
     "../tests/$<CONFIGURATION>/$<TARGET_FILE_NAME:modelA>" 
  COMMENT "Copying to tests directory")
# Tests of ml_compare's figures (src/ml/Compare): predictions read, scores, the paired test.
#   cmake -S tests/ml -B build-tests-ml -G Ninja -DCMAKE_PREFIX_PATH=$HOME/Qt/6.12.0/macos && cmake --build build-tests-ml
#   ./build-tests-ml/ml_tests [filter]
cmake_minimum_required(VERSION 3.21)
project(MlTests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
find_package(Qt6 REQUIRED COMPONENTS Core)
set(DW_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
add_executable(ml_tests ${CMAKE_CURRENT_SOURCE_DIR}/compare_tests.cpp ${DW_ROOT}/src/ml/Compare.cpp ${DW_ROOT}/src/ml/Compare.h)
target_include_directories(ml_tests PRIVATE ${DW_ROOT}/src ${DW_ROOT}/tests/charts)
target_link_libraries(ml_tests PRIVATE Qt6::Core)

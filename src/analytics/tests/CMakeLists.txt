# Unit tests of the analytics engine (pure functions). Not part of the app build (the app globs *.cpp only):
#   cmake -S src/analytics/tests -B build-analytics-tests -G Ninja -DCMAKE_PREFIX_PATH=$HOME/Qt/6.12.0/macos
#   cmake --build build-analytics-tests && build-analytics-tests/analytics_tests
cmake_minimum_required(VERSION 3.21)
project(analytics_tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 REQUIRED COMPONENTS Core)
set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/../..)
add_executable(analytics_tests analytics_tests.cc
  ${SRC}/analytics/Indicators.cpp ${SRC}/analytics/Liquidity.cpp ${SRC}/analytics/Technical.cpp
  ${SRC}/market/Instruments.cpp)
set_source_files_properties(analytics_tests.cc PROPERTIES LANGUAGE CXX)
target_include_directories(analytics_tests PRIVATE ${SRC})
target_link_libraries(analytics_tests PRIVATE Qt6::Core)

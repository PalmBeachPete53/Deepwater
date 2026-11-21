# Tests of the trial registry (src/trials): append-only storage, the figures written per try, the deflated Sharpe read
# back from every try on the same data.
#   cmake -S tests/trials -B build-tests-trials -G Ninja -DCMAKE_PREFIX_PATH=$HOME/Qt/6.12.0/macos && cmake --build build-tests-trials
#   ./build-tests-trials/trials_tests [filter]
cmake_minimum_required(VERSION 3.21)
project(TrialsTests LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
find_package(Qt6 REQUIRED COMPONENTS Core Sql)
set(DW_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
# charts::quant (moments, normal CDF, expected maximum Sharpe), built as the Charts tests build it
add_compile_options($<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-ffp-contract=off>)
add_subdirectory(${DW_ROOT}/third_party/quickjs-ng ${CMAKE_CURRENT_BINARY_DIR}/quickjs-ng)
file(GLOB CHARTS_CORE CONFIGURE_DEPENDS ${DW_ROOT}/src/charts/core/*.cpp ${DW_ROOT}/src/charts/core/*.h)
add_executable(trials_tests ${CHARTS_CORE} ${CMAKE_CURRENT_SOURCE_DIR}/trials_tests.cpp
  ${DW_ROOT}/src/trials/Registry.cpp ${DW_ROOT}/src/trials/Registry.h ${DW_ROOT}/src/trials/Stats.cpp ${DW_ROOT}/src/trials/Stats.h
  ${DW_ROOT}/src/trials/AgentEval.cpp ${DW_ROOT}/src/trials/AgentEval.h ${CMAKE_CURRENT_SOURCE_DIR}/agenteval_tests.cpp
  ${DW_ROOT}/src/ai/Blind.cpp ${DW_ROOT}/src/ai/Blind.h ${CMAKE_CURRENT_SOURCE_DIR}/blind_tests.cpp
  ${DW_ROOT}/src/core/Paths.cpp)
target_include_directories(trials_tests PRIVATE ${DW_ROOT}/src ${DW_ROOT}/tests/charts)
target_link_libraries(trials_tests PRIVATE Qt6::Core Qt6::Sql quickjs)

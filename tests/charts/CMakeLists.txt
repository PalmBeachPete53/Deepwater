# Tests of the Charts port (src/charts), separate from the app so they build in seconds:
#   cmake -S tests/charts -B build-tests -G Ninja -DCMAKE_PREFIX_PATH=$HOME/Qt/6.12.0/macos && cmake --build build-tests
#   ./build-tests/charts_core_tests [filter]      ./build-tests/charts_data_tests [filter]
# Every tests/charts/core/*.cpp goes into charts_core_tests (pure computing, QtCore + QuickJS-ng for custom indicators);
# every tests/charts/data/*.cpp into charts_data_tests (services: src/charts/data + src/net + src/core, offline fixtures).
cmake_minimum_required(VERSION 3.21)
project(ChartsTests LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
find_package(Qt6 REQUIRED COMPONENTS Core Network Concurrent Gui)
set(DW_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
# Same floating-point results as the JS (V8 never fuses a*b+c into one multiply-add; clang does on arm64).
add_compile_options($<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-ffp-contract=off>)
# QuickJS-ng, the custom indicators' JavaScript engine (static C library `quickjs`, same build as the app's).
add_subdirectory(${DW_ROOT}/third_party/quickjs-ng ${CMAKE_CURRENT_BINARY_DIR}/quickjs-ng)

file(GLOB CORE_SRC CONFIGURE_DEPENDS ${DW_ROOT}/src/charts/core/*.cpp ${DW_ROOT}/src/charts/core/*.h)
file(GLOB CORE_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/core/*.cpp)
add_executable(charts_core_tests ${CORE_SRC} ${CORE_TESTS})
target_include_directories(charts_core_tests PRIVATE ${DW_ROOT}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(charts_core_tests PRIVATE Qt6::Core quickjs)
# PineTS (src/charts/core/Pine), embedded as in the app
qt_add_resources(charts_core_tests charts_pinets PREFIX "/pinets" BASE ${DW_ROOT}/third_party/pinets FILES ${DW_ROOT}/third_party/pinets/pinets.js)
target_compile_definitions(charts_core_tests PRIVATE CHARTS_FIXTURES="${DW_ROOT}/quant-terminal/test/fixtures" CHARTS_QT_ROOT="${DW_ROOT}/quant-terminal")

# The PineTS engine checked against TA-Lib (tests/charts/talib): built only where TA-Lib is installed (brew install
# ta-lib); a reference for the tests, never part of the app.
find_path(TALIB_INCLUDE ta-lib/ta_libc.h HINTS /opt/homebrew/include /usr/local/include)
find_library(TALIB_LIB NAMES ta-lib ta_lib HINTS /opt/homebrew/lib /usr/local/lib)
if(TALIB_INCLUDE AND TALIB_LIB)
  add_executable(charts_talib_tests ${CORE_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/talib/pine_talib_tests.cpp ${CMAKE_CURRENT_SOURCE_DIR}/core/main.cpp)
  target_include_directories(charts_talib_tests PRIVATE ${DW_ROOT}/src ${CMAKE_CURRENT_SOURCE_DIR} ${TALIB_INCLUDE})
  target_link_libraries(charts_talib_tests PRIVATE Qt6::Core quickjs ${TALIB_LIB})
  qt_add_resources(charts_talib_tests charts_talib_pinets PREFIX "/pinets" BASE ${DW_ROOT}/third_party/pinets FILES ${DW_ROOT}/third_party/pinets/pinets.js)
  target_compile_definitions(charts_talib_tests PRIVATE CHARTS_FIXTURES="${DW_ROOT}/quant-terminal/test/fixtures" CHARTS_QT_ROOT="${DW_ROOT}/quant-terminal")
else()
  message(STATUS "TA-Lib not found: charts_talib_tests not built")
endif()
file(GLOB DATA_SRC CONFIGURE_DEPENDS ${DW_ROOT}/src/charts/data/*.cpp ${DW_ROOT}/src/charts/data/*.h)
file(GLOB DATA_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/data/*.cpp)
if(DATA_SRC)
  find_package(Qt6 REQUIRED COMPONENTS WebSockets Qml HttpServer)   # LiveFeed (Binance kline streams); Qml: imports_tests' own QJSEngine; HttpServer: ApiServer
  add_executable(charts_data_tests ${CORE_SRC} ${DATA_SRC} ${DATA_TESTS}
    ${DW_ROOT}/src/net/Http.cpp ${DW_ROOT}/src/net/Http.h ${DW_ROOT}/src/net/Sources.cpp ${DW_ROOT}/src/net/Sources.h
    ${DW_ROOT}/src/core/Settings.cpp ${DW_ROOT}/src/core/Paths.cpp)
  target_include_directories(charts_data_tests PRIVATE ${DW_ROOT}/src ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(charts_data_tests PRIVATE Qt6::Core Qt6::Qml Qt6::Network Qt6::Concurrent Qt6::Gui Qt6::WebSockets Qt6::HttpServer quickjs)
  qt_add_resources(charts_data_tests charts_data_pinets PREFIX "/pinets" BASE ${DW_ROOT}/third_party/pinets FILES ${DW_ROOT}/third_party/pinets/pinets.js)
  target_compile_definitions(charts_data_tests PRIVATE CHARTS_FIXTURES="${DW_ROOT}/quant-terminal/test/fixtures" CHARTS_QT_ROOT="${DW_ROOT}/quant-terminal")
  # Live check of the order-flow trade stream against Binance (network; not run by the test suite):
  #   DW_PROFILE=<scratch> ./build-tests/charts_trades_live BINANCE:BTCUSDT.P 120
  add_executable(charts_trades_live ${CMAKE_CURRENT_SOURCE_DIR}/tools/trades_live.cpp ${CORE_SRC} ${DATA_SRC}
    ${DW_ROOT}/src/net/Http.cpp ${DW_ROOT}/src/net/Http.h ${DW_ROOT}/src/net/Sources.cpp ${DW_ROOT}/src/net/Sources.h
    ${DW_ROOT}/src/core/Settings.cpp ${DW_ROOT}/src/core/Paths.cpp)
  target_include_directories(charts_trades_live PRIVATE ${DW_ROOT}/src)
  target_link_libraries(charts_trades_live PRIVATE Qt6::Core Qt6::Qml Qt6::Network Qt6::Concurrent Qt6::Gui Qt6::WebSockets Qt6::HttpServer quickjs)
  qt_add_resources(charts_trades_live charts_live_pinets PREFIX "/pinets" BASE ${DW_ROOT}/third_party/pinets FILES ${DW_ROOT}/third_party/pinets/pinets.js)
  # Live check of the order-book stream (network; not run by the test suite):
  #   DW_PROFILE=<scratch> ./build-tests/charts_depth_live BINANCE:BTCUSDT 90
  add_executable(charts_depth_live ${CMAKE_CURRENT_SOURCE_DIR}/tools/depth_live.cpp ${CORE_SRC} ${DATA_SRC}
    ${DW_ROOT}/src/net/Http.cpp ${DW_ROOT}/src/net/Http.h ${DW_ROOT}/src/net/Sources.cpp ${DW_ROOT}/src/net/Sources.h
    ${DW_ROOT}/src/core/Settings.cpp ${DW_ROOT}/src/core/Paths.cpp)
  target_include_directories(charts_depth_live PRIVATE ${DW_ROOT}/src)
  target_link_libraries(charts_depth_live PRIVATE Qt6::Core Qt6::Qml Qt6::Network Qt6::Concurrent Qt6::Gui Qt6::WebSockets Qt6::HttpServer quickjs)
  qt_add_resources(charts_depth_live charts_depth_pinets PREFIX "/pinets" BASE ${DW_ROOT}/third_party/pinets FILES ${DW_ROOT}/third_party/pinets/pinets.js)
  # Live check of the futures statistics for the liquidation heatmap (network; not run by the test suite):
  #   DW_PROFILE=<scratch> ./build-tests/charts_futures_live BTCUSDT 120
  add_executable(charts_futures_live ${CMAKE_CURRENT_SOURCE_DIR}/tools/futures_live.cpp ${CORE_SRC} ${DATA_SRC}
    ${DW_ROOT}/src/net/Http.cpp ${DW_ROOT}/src/net/Http.h ${DW_ROOT}/src/net/Sources.cpp ${DW_ROOT}/src/net/Sources.h
    ${DW_ROOT}/src/core/Settings.cpp ${DW_ROOT}/src/core/Paths.cpp)
  target_include_directories(charts_futures_live PRIVATE ${DW_ROOT}/src)
  target_link_libraries(charts_futures_live PRIVATE Qt6::Core Qt6::Qml Qt6::Network Qt6::Concurrent Qt6::Gui Qt6::WebSockets Qt6::HttpServer quickjs)
  qt_add_resources(charts_futures_live charts_futures_pinets PREFIX "/pinets" BASE ${DW_ROOT}/third_party/pinets FILES ${DW_ROOT}/third_party/pinets/pinets.js)
  # Calibration of the liquidation heatmap on Binance's archive (offline; files from tools/binance_archive.py):
  #   ./build-tests/charts_liq_history <dir> cm BTCUSD_PERP
  add_executable(charts_liq_history ${CMAKE_CURRENT_SOURCE_DIR}/tools/liq_history.cpp ${CORE_SRC})
  target_include_directories(charts_liq_history PRIVATE ${DW_ROOT}/src)
  target_link_libraries(charts_liq_history PRIVATE Qt6::Core Qt6::Concurrent quickjs)
  qt_add_resources(charts_liq_history charts_liqh_pinets PREFIX "/pinets" BASE ${DW_ROOT}/third_party/pinets FILES ${DW_ROOT}/third_party/pinets/pinets.js)
  # Hours of made-up heatmap records for performance checks (writes only into DW_PROFILE)
  add_executable(charts_heatmap_synth ${CMAKE_CURRENT_SOURCE_DIR}/tools/heatmap_synth.cpp ${CORE_SRC} ${DW_ROOT}/src/core/Paths.cpp ${DW_ROOT}/src/core/Settings.cpp)
  target_include_directories(charts_heatmap_synth PRIVATE ${DW_ROOT}/src)
  target_link_libraries(charts_heatmap_synth PRIVATE Qt6::Core quickjs)
  qt_add_resources(charts_heatmap_synth charts_synth_pinets PREFIX "/pinets" BASE ${DW_ROOT}/third_party/pinets FILES ${DW_ROOT}/third_party/pinets/pinets.js)
endif()

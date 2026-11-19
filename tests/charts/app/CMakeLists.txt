# Stand-alone Charts tab (the page only, without the rest of Deepwater), so UI work builds in seconds and in its own
# build dir while other agents work:
#   cmake -S tests/charts/app -B build-charts-<you> -G Ninja -DCMAKE_PREFIX_PATH=$HOME/Qt/6.12.0/macos
#   cmake --build build-charts-<you>
#   DW_PROFILE=<scratch dir> ./build-charts-<you>/charts_app [--size 1600x1000] [--screenshot out.png --delay 6000]
#       [--offline]   (QT_OFFLINE-like: providers read quant-terminal/test/fixtures, no network)
#       [--eval "<step>"]  see main.cpp (UI steps for screenshots: open a tab, a dialog, …)
# Compiles src/charts/**, src/pages/charts/** and the few app files they need (theme, kit, settings, paths, net).
cmake_minimum_required(VERSION 3.21)
project(ChartsApp LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Concurrent Svg WebSockets)
find_package(Qt6 QUIET COMPONENTS HttpServer)
set(DW_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:$<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-ffp-contract=off>>)

file(GLOB_RECURSE CHARTS_SRC CONFIGURE_DEPENDS ${DW_ROOT}/src/charts/*.cpp ${DW_ROOT}/src/charts/*.h
  ${DW_ROOT}/src/pages/charts/*.cpp ${DW_ROOT}/src/pages/charts/*.h)
set(APP_SRC
  ${DW_ROOT}/src/ui/Theme.cpp ${DW_ROOT}/src/ui/Kit.cpp ${DW_ROOT}/src/ui/Format.cpp ${DW_ROOT}/src/ui/Chart.cpp
  ${DW_ROOT}/src/core/Settings.cpp ${DW_ROOT}/src/core/Paths.cpp ${DW_ROOT}/src/core/I18n.cpp
  ${DW_ROOT}/src/net/Http.cpp ${DW_ROOT}/src/net/Sources.cpp ${DW_ROOT}/src/net/Http.h ${DW_ROOT}/src/net/Sources.h
  ${DW_ROOT}/src/app/Pages.cpp)
add_executable(charts_app main.cpp ${CHARTS_SRC} ${APP_SRC})
target_include_directories(charts_app PRIVATE ${DW_ROOT}/src)
target_link_libraries(charts_app PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Concurrent Qt6::Svg Qt6::WebSockets)
if(TARGET Qt6::HttpServer)
  target_link_libraries(charts_app PRIVATE Qt6::HttpServer)
endif()
# QuickJS-ng (custom indicators): the same static library target as the app.
add_subdirectory(${DW_ROOT}/third_party/quickjs-ng ${CMAKE_BINARY_DIR}/quickjs-ng)
target_link_libraries(charts_app PRIVATE quickjs)
target_compile_definitions(charts_app PRIVATE CHARTS_FIXTURES="${DW_ROOT}/quant-terminal/test/fixtures" CHARTS_QT_ROOT="${DW_ROOT}/quant-terminal")
file(GLOB_RECURSE DW_FONTS RELATIVE ${DW_ROOT}/resources ${DW_ROOT}/resources/fonts/*)
list(FILTER DW_FONTS EXCLUDE REGEX "\\.md$|SOURCES")
set(DW_FONTS_ABS "")
foreach(f ${DW_FONTS})
  list(APPEND DW_FONTS_ABS ${DW_ROOT}/resources/${f})
endforeach()
qt_add_resources(charts_app charts_fonts PREFIX "/" BASE ${DW_ROOT}/resources FILES ${DW_FONTS_ABS})
file(GLOB_RECURSE CHARTS_RES RELATIVE ${DW_ROOT}/resources CONFIGURE_DEPENDS ${DW_ROOT}/resources/charts/*)
if(CHARTS_RES)
  set(CHARTS_RES_ABS "")
  foreach(f ${CHARTS_RES})
    list(APPEND CHARTS_RES_ABS ${DW_ROOT}/resources/${f})
  endforeach()
  qt_add_resources(charts_app charts_res PREFIX "/" BASE ${DW_ROOT}/resources FILES ${CHARTS_RES_ABS})
endif()

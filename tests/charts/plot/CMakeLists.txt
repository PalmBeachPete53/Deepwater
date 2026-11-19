# Demo / visual test of the Charts tab's chart engine (src/pages/charts/plot), separate from the app:
#   cmake -S tests/charts/plot -B build-tests-plot -G Ninja -DCMAKE_PREFIX_PATH=$HOME/Qt/6.12.0/macos && cmake --build build-tests-plot
#   ./build-tests-plot/plot_demo docs/charts/shots/plot          # PNGs (offscreen)
#   QT_SCALE_FACTOR=2 ./build-tests-plot/plot_demo docs/charts/shots/plot --suffix @2x
#   ./build-tests-plot/plot_demo /tmp --perf                     # 16 charts x 5000 bars, fps while scrolling
cmake_minimum_required(VERSION 3.21)
project(PlotDemo LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
set(DW_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../..)

file(GLOB PLOT_SRC CONFIGURE_DEPENDS ${DW_ROOT}/src/pages/charts/plot/*.cpp ${DW_ROOT}/src/pages/charts/plot/*.h)
add_executable(plot_demo plot_demo.cpp ${PLOT_SRC}
  ${DW_ROOT}/src/ui/Theme.cpp ${DW_ROOT}/src/ui/Theme.h ${DW_ROOT}/src/core/Settings.cpp ${DW_ROOT}/src/core/Settings.h)
target_include_directories(plot_demo PRIVATE ${DW_ROOT}/src)
target_link_libraries(plot_demo PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
target_compile_definitions(plot_demo PRIVATE CHARTS_FIXTURES="${DW_ROOT}/quant-terminal/test/fixtures")
# the two fonts the chart uses, at the paths theme::install() loads
qt_add_resources(plot_demo plot_fonts PREFIX "/" BASE ${DW_ROOT}/resources
  FILES ${DW_ROOT}/resources/fonts/JetBrainsMono.ttf ${DW_ROOT}/resources/fonts/Inter.ttf)

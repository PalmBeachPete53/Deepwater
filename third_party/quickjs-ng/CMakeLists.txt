# QuickJS-ng as the static C library `quickjs` (see SOURCES.md). Written for Deepwater, not upstream's build file:
# used by the app (CMakeLists.txt) and by the Charts tests (tests/charts/CMakeLists.txt) with add_subdirectory.
add_library(quickjs STATIC quickjs.c libregexp.c libunicode.c dtoa.c)
set_target_properties(quickjs PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS ON AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
target_include_directories(quickjs SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(quickjs PRIVATE _GNU_SOURCE QUICKJS_NG_BUILD)
if(WIN32)
  target_compile_definitions(quickjs PRIVATE WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0601)
endif()
# Warnings off (vendored code). Upstream's flags otherwise: unsigned char; no fused multiply-add, as in V8.
if(MSVC)
  target_compile_options(quickjs PRIVATE /w /experimental:c11atomics)
else()
  target_compile_options(quickjs PRIVATE -w -funsigned-char -ffp-contract=off)
  target_link_libraries(quickjs PUBLIC m)
endif()

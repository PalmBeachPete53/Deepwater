# miniz 3.0.2 (Rich Geldreich, MIT, see LICENSE), taken unchanged from
# https://github.com/richgel999/miniz/releases/download/3.0.2/miniz-3.0.2.zip (amalgamated miniz.c / miniz.h).
# Deepwater uses only its streaming inflater (tinfl) to read Binance's public archives (zip of CSV) as they download,
# without unpacking them in memory: Data › Technical data, footprint history (src/technicaldata/FootprintArchive.cpp).
# No archive, stdio or time code, and no zlib-compatible names (they would clash with the zlib Qt links).
cmake_minimum_required(VERSION 3.21)
project(miniz LANGUAGES C)
add_library(miniz STATIC miniz.c)
target_include_directories(miniz PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(miniz PUBLIC MINIZ_NO_STDIO MINIZ_NO_TIME MINIZ_NO_ARCHIVE_APIS MINIZ_NO_ARCHIVE_WRITING_APIS
                                        MINIZ_NO_ZLIB_COMPATIBLE_NAMES)
set_target_properties(miniz PROPERTIES POSITION_INDEPENDENT_CODE ON)

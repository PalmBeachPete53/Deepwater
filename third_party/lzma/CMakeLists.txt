# LZMA decoder of the LZMA SDK 24.09 (Igor Pavlov, public domain: https://www.7-zip.org/sdk.html), taken as is from
# https://github.com/ip7z/7zip/tree/24.09/C: LzmaDec.c/.h, 7zTypes.h, Precomp.h, Compiler.h, 7zWindows.h. Used to read
# Dukascopy's .bi5 candle files (LZMA "alone" format) in Data › Technical data (src/technicaldata/Dukascopy.cpp).
cmake_minimum_required(VERSION 3.21)
project(lzmadec LANGUAGES C)
add_library(lzmadec STATIC LzmaDec.c)
target_include_directories(lzmadec PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(lzmadec PROPERTIES POSITION_INDEPENDENT_CODE ON)

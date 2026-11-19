cmake_minimum_required(VERSION 3.21)
project(Deepwater VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# MSVC, for every target (llama.cpp included): sources are UTF-8, and __cplusplus must give the real standard
# (llama-chat.cpp picks its u8 literal form from it; MSVC reports 199711 without /Zc:__cplusplus).
if(MSVC)
  add_compile_options(/utf-8 $<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Svg Concurrent Xml Sql OpenGL OpenGLWidgets LinguistTools WebSockets HttpServer)   # HttpServer: Charts local API (src/charts/data/ApiServer)
enable_language(C)
if(APPLE)
  enable_language(OBJC OBJCXX)   # ggml's Metal backend has .m files
endif()

# Every .cpp under src/ is part of the app: new pages and services need no edit here.
file(GLOB_RECURSE DW_SOURCES CONFIGURE_DEPENDS src/*.cpp src/*.h)
if(APPLE)
  file(GLOB_RECURSE DW_OBJC CONFIGURE_DEPENDS src/*.mm)
  list(APPEND DW_SOURCES ${DW_OBJC})
  # PDFKit and Vision objects are allocated with alloc/init and never released by hand: without ARC each PDF text
  # extraction, print and OCR leaked its document / request. NativeTv.mm keeps its objects in void* members with
  # manual retain/release (correct as written) and stays without ARC; a new .mm file should be added here.
  set_source_files_properties(src/banks/Pdf.mm src/macro/Ocr.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
endif()

# JavaScript engine of the Charts tab's custom indicators (src/charts/core/CustomJs): QuickJS-ng vendored in
# third_party/quickjs-ng (library sources only, MIT, see its LICENSE and SOURCES.md): the static C library `quickjs`,
# built from its own folder, outside the src/ glob.
add_subdirectory(third_party/quickjs-ng)
# third_party/lzma: the LZMA SDK's decoder (public domain), for Dukascopy's .bi5 files (Data › Technical data, forex)
add_subdirectory(third_party/lzma)

qt_add_executable(Deepwater MACOSX_BUNDLE ${DW_SOURCES})
# Charts tab (port of quant-terminal): same floating-point results as the JS, so no fused multiply-add there.
# src/technicaldata computes the same indicators into Data › Technical data: its tests build it with -ffp-contract=off
# (tests/technicaldata/CMakeLists.txt), so the app does too, or the shipped values could differ from the tested ones.
file(GLOB_RECURSE DW_CHARTS_SOURCES CONFIGURE_DEPENDS src/charts/*.cpp src/pages/charts/*.cpp src/technicaldata/*.cpp)
if(NOT MSVC)
  set_source_files_properties(${DW_CHARTS_SOURCES} PROPERTIES COMPILE_OPTIONS "-ffp-contract=off")
endif()
target_include_directories(Deepwater PRIVATE src)
# MSVC: sources are UTF-8 (labels hold "→", "·", "€"…), some translation units exceed the default section count, and Qt
# wants the real __cplusplus. On Windows the app is a GUI program (no console window behind it).
target_compile_options(Deepwater PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8 /bigobj /Zc:__cplusplus /permissive->)
target_compile_definitions(Deepwater PRIVATE $<$<CXX_COMPILER_ID:MSVC>:NOMINMAX _USE_MATH_DEFINES>)
if(WIN32)
  set_target_properties(Deepwater PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
# Hardening (audit 2026-11-18, B1; user's choice 2026-11-19): out-of-range accesses in the standard containers
# (std::vector, std::span, std::string, std::optional…) and in Qt's (QList, QString…) stop the app at the faulty line
# instead of reading or writing memory at random — a clean crash report instead of silent corruption or a crash
# elsewhere later. libc++ "fast" mode: the cheap checks only. QT_FORCE_ASSERTS: Qt's own Q_ASSERT bounds checks in its
# inline container code, otherwise compiled out of every non-Debug build (QT_NO_DEBUG). The cost was measured on the
# heaviest Charts frames (docs/AUDIT-2026-11-18-FIX-PROMPT.md, B1). -DDW_HARDENING=OFF builds without, for comparison.
option(DW_HARDENING "Bounds checks in the standard library and Qt containers" ON)
if(DW_HARDENING)
  target_compile_definitions(Deepwater PRIVATE
    $<$<CXX_COMPILER_ID:Clang,AppleClang>:_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST>
    $<$<CXX_COMPILER_ID:MSVC>:_MSVC_STL_HARDENING=1>
    QT_FORCE_ASSERTS)
endif()
# Compiler warnings on Deepwater's own code (third_party/ builds in its own targets without them). Kept at zero: a new
# one is seen. missing-field-initializers is off: `Row{a, b}` leaving the other fields at their defaults is the house
# style (263 of the 303 warnings when this was turned on, none a bug).
target_compile_options(Deepwater PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,AppleClang,Clang,GNU>:-Wall -Wextra -Wno-missing-field-initializers>
                                         $<$<COMPILE_LANG_AND_ID:OBJCXX,AppleClang,Clang>:-Wall -Wextra -Wno-missing-field-initializers>
                                         $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/W4>)
target_link_libraries(Deepwater PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Svg Qt6::Concurrent Qt6::Xml Qt6::Sql Qt6::OpenGL Qt6::OpenGLWidgets Qt6::WebSockets Qt6::HttpServer quickjs lzmadec)
if(APPLE)
  target_link_libraries(Deepwater PRIVATE "-framework Foundation" "-framework AppKit" "-framework Vision" "-framework PDFKit" "-framework CoreGraphics" "-framework WebKit")
endif()

# Every file under resources/ is embedded at :/<path relative to resources>.
file(GLOB_RECURSE DW_RESOURCES CONFIGURE_DEPENDS RELATIVE ${CMAKE_SOURCE_DIR}/resources resources/*)
list(FILTER DW_RESOURCES EXCLUDE REGEX "(^|/)\\.DS_Store$|(^|/)SOURCES[^/]*$|\\.md$|(^|/)__pycache__/|\\.pyc$")
set(DW_RESOURCES_ABS "")
foreach(r ${DW_RESOURCES})
  list(APPEND DW_RESOURCES_ABS ${CMAKE_SOURCE_DIR}/resources/${r})
endforeach()
qt_add_resources(Deepwater dw_assets PREFIX "/" BASE resources FILES ${DW_RESOURCES_ABS})
# Pine Script runtime of the Charts tab (src/charts/core/Pine): PineTS vendored in third_party/pinets (AGPL-3.0, see
# its LICENSE and SOURCES.md; Deepwater is AGPL-3.0 because it ships it), embedded at :/pinets/pinets.js.
qt_add_resources(Deepwater dw_pinets PREFIX "/pinets" BASE third_party/pinets FILES third_party/pinets/pinets.js)

set_target_properties(Deepwater PROPERTIES
  MACOSX_BUNDLE_GUI_IDENTIFIER com.deepwater-terminal.Deepwater
  MACOSX_BUNDLE_BUNDLE_NAME Deepwater
  MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})

# On-device EN→FR translation helper (Apple Translation framework), built next to the executable.
if(APPLE)
  find_program(DW_SWIFTC swiftc)
  if(DW_SWIFTC)
    set(DW_TRANSLATE_SRC ${CMAKE_SOURCE_DIR}/src/news/translate/dw-translate.swift)
    set(DW_TRANSLATE_BIN ${CMAKE_BINARY_DIR}/Deepwater.app/Contents/MacOS/dw-translate)
    add_custom_command(OUTPUT ${DW_TRANSLATE_BIN}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/Deepwater.app/Contents/MacOS
      COMMAND ${DW_SWIFTC} -parse-as-library -O ${DW_TRANSLATE_SRC} -o ${DW_TRANSLATE_BIN}
      DEPENDS ${DW_TRANSLATE_SRC} COMMENT "Building dw-translate")
    add_custom_target(dw_translate ALL DEPENDS ${DW_TRANSLATE_BIN})
    add_dependencies(Deepwater dw_translate)
  endif()
endif()

# Local models (AI › Agents): llama.cpp vendored in third_party/llama.cpp (library parts only, MIT) and the helper
# deepwater-llm (helpers/llm), one process per lit environment, started by src/ai/Engine inside a sandbox.
set(LLAMA_BUILD_COMMON OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_APP OFF CACHE BOOL "" FORCE)
set(LLAMA_OPENSSL OFF CACHE BOOL "" FORCE)
set(LLAMA_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
set(GGML_NATIVE OFF CACHE BOOL "" FORCE)            # the app runs on other Macs than the one that built it
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
if(APPLE)
  set(GGML_METAL ON CACHE BOOL "" FORCE)
  set(GGML_METAL_EMBED_LIBRARY ON CACHE BOOL "" FORCE)
endif()
add_subdirectory(third_party/llama.cpp EXCLUDE_FROM_ALL)
add_executable(deepwater-llm helpers/llm/main.cpp)
target_link_libraries(deepwater-llm PRIVATE llama nlohmann)
if(APPLE)
  set_target_properties(deepwater-llm PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Deepwater.app/Contents/MacOS)
endif()
add_dependencies(Deepwater deepwater-llm)

# French translations: one Qt Linguist file per package (translations/<package>_fr.ts), compiled to .qm and embedded
# at :/i18n. Update a package's file with: lupdate <its source folders> -ts translations/<package>_fr.ts -no-obsolete
file(GLOB DW_TS_FILES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/translations/*_fr.ts)
if(DW_TS_FILES)
  set_source_files_properties(${DW_TS_FILES} PROPERTIES OUTPUT_LOCATION ${CMAKE_BINARY_DIR}/i18n)
  qt_add_lrelease(TS_FILES ${DW_TS_FILES} QM_FILES_OUTPUT_VARIABLE DW_QM_FILES)
  qt_add_resources(Deepwater dw_i18n PREFIX "/i18n" BASE ${CMAKE_BINARY_DIR}/i18n FILES ${DW_QM_FILES})
endif()

# ---- Windows package: the app, the model runner, Qt's DLLs and plugins (windeployqt), the VC++ runtime beside them, and
# an NSIS installer (cpack -G NSIS, or the `dist` target). Everything at the top of the install folder: Qt's deploy
# script would otherwise put the DLLs in bin/ apart from the executable.
if(WIN32)
  target_compile_options(deepwater-llm PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
  set(CMAKE_INSTALL_BINDIR ".")
  include(GNUInstallDirs)
  install(TARGETS Deepwater deepwater-llm RUNTIME DESTINATION .)
  qt_generate_deploy_app_script(TARGET Deepwater OUTPUT_SCRIPT DW_DEPLOY_SCRIPT NO_TRANSLATIONS)
  install(SCRIPT ${DW_DEPLOY_SCRIPT})
  set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
  set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
  include(InstallRequiredSystemLibraries)

  set(CPACK_GENERATOR "NSIS;ZIP")
  set(CPACK_PACKAGE_NAME "Deepwater")
  set(CPACK_PACKAGE_VENDOR "Deepwater")
  set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Deepwater market terminal")
  set(CPACK_PACKAGE_INSTALL_DIRECTORY "Deepwater")
  set(CPACK_PACKAGE_EXECUTABLES "Deepwater;Deepwater")         # Start menu shortcut
  set(CPACK_CREATE_DESKTOP_LINKS "Deepwater")
  set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
  set(CPACK_NSIS_DISPLAY_NAME "Deepwater")
  set(CPACK_NSIS_PACKAGE_NAME "Deepwater")
  set(CPACK_NSIS_INSTALLED_ICON_NAME "Deepwater.exe")
  set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
  set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")
  set(CPACK_NSIS_MUI_FINISHPAGE_RUN "Deepwater.exe")
  set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_BINARY_DIR}/dist")
  include(CPack)
  add_custom_target(dist COMMAND ${CMAKE_CPACK_COMMAND} --config ${CMAKE_BINARY_DIR}/CPackConfig.cmake
                    WORKING_DIRECTORY ${CMAKE_BINARY_DIR} DEPENDS Deepwater deepwater-llm VERBATIM)
endif()

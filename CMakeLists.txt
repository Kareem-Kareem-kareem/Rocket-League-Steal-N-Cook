cmake_minimum_required(VERSION 3.18)
project(StealAndCook VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Single-config generators (Ninja) need build type at configure time
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ── Paths ─────────────────────────────────────────────────────
# Point BAKKESMODSDK_PATH to your local BakkesMod SDK clone.
# e.g.  cmake -DBAKKESMODSDK_PATH=C:/BakkesModSDK ..
set(BAKKESMODSDK_PATH "${CMAKE_SOURCE_DIR}/../BakkesModSDK"
    CACHE PATH "Path to BakkesModSDK root")

if(NOT EXISTS "${BAKKESMODSDK_PATH}/include/bakkesmod/plugin/bakkesmodplugin.h")
    message(FATAL_ERROR
        "BakkesMod SDK not found at ${BAKKESMODSDK_PATH}. "
        "Clone https://github.com/bakkesmodorg/BakkesModSDK next to this repo "
        "or set -DBAKKESMODSDK_PATH=<your path>.")
endif()

# ── Plugin DLL target ─────────────────────────────────────────
add_library(StealAndCook SHARED
    src/StealAndCook.cpp
    src/pch.h
)

target_include_directories(StealAndCook PRIVATE
    "${BAKKESMODSDK_PATH}/include"
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(StealAndCook PRIVATE
    "${BAKKESMODSDK_PATH}/lib/pluginsdk.lib"
)

target_compile_definitions(StealAndCook PRIVATE
    _WINDLL
    NOMINMAX
)

target_precompile_headers(StealAndCook PRIVATE src/pch.h)

set_target_properties(StealAndCook PROPERTIES
    OUTPUT_NAME "StealAndCook"
    SUFFIX ".dll"
)

# ── Install to BakkesMod plugins folder ───────────────────────
set(BM_PLUGINS_DIR "$ENV{APPDATA}/bakkesmod/bakkesmod/plugins"
    CACHE PATH "BakkesMod plugins directory")

install(TARGETS StealAndCook
    RUNTIME DESTINATION "${BM_PLUGINS_DIR}"
    LIBRARY DESTINATION "${BM_PLUGINS_DIR}")

install(FILES cfg/StealAndCook.set
    DESTINATION "${BM_PLUGINS_DIR}/settings")

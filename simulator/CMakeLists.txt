cmake_minimum_required(VERSION 3.20)

project(companion_simulator LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(DEFINED ENV{LVGL_SOURCE_DIR} AND NOT "$ENV{LVGL_SOURCE_DIR}" STREQUAL "")
    set(_lvgl_default "$ENV{LVGL_SOURCE_DIR}")
else()
    set(_lvgl_default "${CMAKE_CURRENT_SOURCE_DIR}/../managed_components/lvgl__lvgl")
endif()
set(LVGL_SOURCE_DIR "${_lvgl_default}" CACHE PATH "Path to the LVGL source tree")

if(NOT EXISTS "${LVGL_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "LVGL source not found at ${LVGL_SOURCE_DIR}. "
        "Run the pinned device configure once or set LVGL_SOURCE_DIR."
    )
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)

add_subdirectory("${LVGL_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/lvgl")
target_link_libraries(lvgl PUBLIC PkgConfig::SDL2)

add_executable(companion-simulator
    "main.c"
    "../main/product_ui/product_ui.c"
)

target_include_directories(companion-simulator PRIVATE
    "../main/product_ui"
)

target_compile_options(companion-simulator PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

target_link_libraries(companion-simulator PRIVATE
    lvgl::lvgl
)

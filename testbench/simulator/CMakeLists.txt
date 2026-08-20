cmake_minimum_required(VERSION 3.20)

project(touch_ui_simulator LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(LVGL_SOURCE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../managed_components/lvgl__lvgl"
    CACHE PATH
    "Path to the LVGL source managed by ESP-IDF"
)

if(NOT EXISTS "${LVGL_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "LVGL source not found at ${LVGL_SOURCE_DIR}. "
        "Run an ESP-IDF build first so the Component Manager downloads it."
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

add_executable(touch-ui-simulator MACOSX_BUNDLE
    "main.c"
    "../components/app_ui/app_ui.c"
)

set_target_properties(touch-ui-simulator PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "Touch UI Simulator"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.huntsyea.touch-ui-simulator"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
)

target_include_directories(touch-ui-simulator PRIVATE
    "../components/app_ui/include"
)

target_compile_options(touch-ui-simulator PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

target_link_libraries(touch-ui-simulator PRIVATE
    lvgl::lvgl
)

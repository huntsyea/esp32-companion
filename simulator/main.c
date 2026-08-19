#include "product_ui.h"

#include "lvgl.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 240
#define WINDOW_ZOOM 2.0F

static bool is_smoke_test(int argc, char** argv) {
    return argc == 2 && strcmp(argv[1], "--smoke-test") == 0;
}

int main(int argc, char** argv) {
    const bool smoke_test = is_smoke_test(argc, argv);
    if (argc > 1 && !smoke_test) {
        fprintf(stderr, "Usage: %s [--smoke-test]\n", argv[0]);
        return 2;
    }

    lv_init();

    lv_display_t* display = lv_sdl_window_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    if (display == NULL) {
        fputs("Failed to create the SDL display.\n", stderr);
        return 1;
    }

    lv_sdl_window_set_title(display, "ESP32 Companion Simulator");
    lv_sdl_window_set_zoom(display, WINDOW_ZOOM);

    lv_indev_t* mouse = lv_sdl_mouse_create();
    if (mouse == NULL) {
        fputs("Failed to create the SDL pointer input.\n", stderr);
        return 1;
    }
    lv_indev_set_display(mouse, display);

    if (product_ui_create(NULL) != NULL) {
        fputs("Product UI accepted an invalid parent.\n", stderr);
        return 1;
    }

    lv_obj_t* product_ui = product_ui_create(lv_screen_active());
    if (product_ui == NULL) {
        fputs("Failed to create the Product UI.\n", stderr);
        return 1;
    }

    if (smoke_test) {
        for (int iteration = 0; iteration < 10; ++iteration) {
            lv_timer_handler();
            lv_delay_ms(5);
        }
        return 0;
    }

    while (true) {
        lv_timer_handler();
        lv_delay_ms(5);
    }
}

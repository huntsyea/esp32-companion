#include "product_ui.h"

#include "lvgl.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 240
#define WINDOW_ZOOM 2.0F

static bool is_smoke_test(int argc, char** argv) {
    return argc == 2 && strcmp(argv[1], "--smoke-test") == 0;
}

static const char* get_reference_path(int argc, char** argv) {
    if (argc == 3 && strcmp(argv[1], "--render-reference") == 0) {
        return argv[2];
    }
    return NULL;
}

static bool is_interaction_transparent(lv_obj_t* object) {
    if (lv_obj_has_flag(object, LV_OBJ_FLAG_CLICKABLE) ||
        lv_obj_has_flag(object, LV_OBJ_FLAG_SCROLLABLE)) {
        return false;
    }

    const uint32_t child_count = lv_obj_get_child_count(object);
    for (uint32_t index = 0; index < child_count; ++index) {
        if (!is_interaction_transparent(lv_obj_get_child(object, (int32_t)index))) {
            return false;
        }
    }
    return true;
}

static int write_reference_image(lv_obj_t* product_ui, const char* path) {
    lv_draw_buf_t* snapshot = lv_snapshot_create_draw_buf(product_ui, LV_COLOR_FORMAT_RGB888);
    if (snapshot != NULL && lv_snapshot_take_to_draw_buf(product_ui, LV_COLOR_FORMAT_RGB888,
                                                         snapshot) != LV_RESULT_OK) {
        fputs("LVGL rejected the Product UI snapshot buffer.\n", stderr);
        lv_draw_buf_destroy(snapshot);
        snapshot = NULL;
    }
    if (snapshot == NULL || snapshot->header.w != DISPLAY_WIDTH ||
        snapshot->header.h != DISPLAY_HEIGHT) {
        if (snapshot == NULL) {
            fputs("Failed to capture the Product UI.\n", stderr);
        } else {
            fprintf(stderr, "Expected a 240 by 240 Product UI, got %u by %u.\n",
                    (unsigned)snapshot->header.w, (unsigned)snapshot->header.h);
        }
        if (snapshot != NULL) {
            lv_draw_buf_destroy(snapshot);
        }
        return 1;
    }

    FILE* output = fopen(path, "wb");
    if (output == NULL) {
        fprintf(stderr, "Failed to open reference image: %s\n", path);
        lv_draw_buf_destroy(snapshot);
        return 1;
    }

    fprintf(output, "P6\n%d %d\n255\n", DISPLAY_WIDTH, DISPLAY_HEIGHT);
    for (uint32_t y = 0; y < snapshot->header.h; ++y) {
        const uint8_t* row = snapshot->data + (y * snapshot->header.stride);
        for (uint32_t x = 0; x < snapshot->header.w; ++x) {
            const uint8_t* blue_green_red = row + (x * 3U);
            fputc(blue_green_red[2], output);
            fputc(blue_green_red[1], output);
            fputc(blue_green_red[0], output);
        }
    }

    const bool stream_failed = ferror(output) != 0;
    const bool close_failed = fclose(output) != 0;
    lv_draw_buf_destroy(snapshot);
    if (stream_failed || close_failed) {
        fprintf(stderr, "Failed to write reference image: %s\n", path);
        return 1;
    }
    return 0;
}

int main(int argc, char** argv) {
    const bool smoke_test = is_smoke_test(argc, argv);
    const char* reference_path = get_reference_path(argc, argv);
    if (argc > 1 && !smoke_test && reference_path == NULL) {
        fprintf(stderr, "Usage: %s [--smoke-test | --render-reference PATH]\n", argv[0]);
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
    if (!is_interaction_transparent(product_ui)) {
        fputs("Product UI blocks pointer input.\n", stderr);
        return 1;
    }

    if (smoke_test || reference_path != NULL) {
        for (int iteration = 0; iteration < 10; ++iteration) {
            lv_timer_handler();
            lv_delay_ms(5);
        }
        if (reference_path != NULL) {
            return write_reference_image(product_ui, reference_path);
        }
        return 0;
    }

    while (true) {
        lv_timer_handler();
        lv_delay_ms(5);
    }
}

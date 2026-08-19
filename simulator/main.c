#include "clock_screen.h"
#include "companion_time.h"
#include "product_ui.h"

#include "lvgl.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 240
#define WINDOW_ZOOM 2.0F

typedef struct {
    product_ui_t* product_ui;
    uint16_t envelope;
} simulator_controls_t;

static void print_usage(const char* program) {
    fprintf(stderr,
            "Usage: %s [--smoke-test | --render-reference PATH | "
            "--render-state STATE TIME_MS ENVELOPE PATH | --trace-events EVENTS | "
            "--render-clock EPOCH PATH | --render-clock-unsynchronized PATH | "
            "--inspect-clock EPOCH | --inspect-clock-unsynchronized | "
            "--trace-clock-updates FIRST SAME_MINUTE NEXT_MINUTE | "
            "--inspect-clock-zone ZONE EPOCH]\n",
            program);
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

static int write_reference_image(lv_obj_t* product_ui_root, const char* path) {
    lv_draw_buf_t* snapshot = lv_snapshot_create_draw_buf(product_ui_root, LV_COLOR_FORMAT_RGB888);
    if (snapshot != NULL && lv_snapshot_take_to_draw_buf(product_ui_root, LV_COLOR_FORMAT_RGB888,
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

static bool parse_unsigned(const char* text, uint32_t maximum, uint32_t* value) {
    if (text == NULL || *text == '\0' || *text == '-') {
        return false;
    }
    errno = 0;
    char* end = NULL;
    const unsigned long parsed = strtoul(text, &end, 10);
    if (errno != 0 || *end != '\0' || parsed > maximum) {
        return false;
    }
    *value = (uint32_t)parsed;
    return true;
}

static bool parse_epoch(const char* text, int64_t* epoch) {
    if (text == NULL || *text == '\0' || *text == '-') {
        return false;
    }
    errno = 0;
    char* end = NULL;
    const unsigned long long parsed = strtoull(text, &end, 10);
    if (errno != 0 || *end != '\0' || parsed > INT64_MAX) {
        return false;
    }
    *epoch = (int64_t)parsed;
    return true;
}

static bool update_clock(clock_screen_t* clock_screen, int64_t epoch, bool synchronized) {
    companion_local_time_t local_time;
    if (!companion_time_from_epoch(epoch, synchronized, &local_time)) {
        return false;
    }
    clock_screen_update(clock_screen, &local_time);
    return true;
}

static int inspect_clock(clock_screen_t* clock_screen, int64_t epoch, bool synchronized) {
    if (!update_clock(clock_screen, epoch, synchronized)) {
        fputs("Failed to convert the injected clock epoch.\n", stderr);
        return 1;
    }
    const clock_screen_model_t* model = clock_screen_get_model(clock_screen);
    if (model == NULL) {
        fputs("Clock model is unavailable.\n", stderr);
        return 1;
    }
    if (!model->synchronized) {
        puts("sync=no status=TIME_NOT_SYNCED");
        return 0;
    }
    printf("sync=yes time=%s period=%s weekday=%s date=%s dst=%u\n", model->time, model->period,
           model->weekday, model->date, model->is_dst ? 1U : 0U);
    return 0;
}

static int trace_clock_updates(clock_screen_t* clock_screen, const int64_t epochs[3]) {
    bool changed[3];
    for (size_t index = 0; index < 3; ++index) {
        companion_local_time_t local_time;
        if (!companion_time_from_epoch(epochs[index], true, &local_time)) {
            fputs("Failed to convert a clock update epoch.\n", stderr);
            return 1;
        }
        changed[index] = clock_screen_update(clock_screen, &local_time);
    }
    printf("first=%s same_minute=%s next_minute=%s\n", changed[0] ? "changed" : "unchanged",
           changed[1] ? "changed" : "unchanged", changed[2] ? "changed" : "unchanged");
    return 0;
}

static bool dispatch_state_path(product_ui_t* ui, const char* state_name) {
    if (strcmp(state_name, "offline") == 0) {
        return true;
    }
    product_ui_dispatch(ui, PRODUCT_UI_EVENT_CONNECTED);
    if (strcmp(state_name, "idle") == 0) {
        return true;
    }
    product_ui_dispatch(ui, PRODUCT_UI_EVENT_LISTEN_STARTED);
    if (strcmp(state_name, "listening") == 0) {
        return true;
    }
    product_ui_dispatch(ui, PRODUCT_UI_EVENT_RECORDING_ENDED);
    if (strcmp(state_name, "thinking") == 0) {
        return true;
    }
    if (strcmp(state_name, "error") == 0) {
        product_ui_dispatch(ui, PRODUCT_UI_EVENT_TEMPORARY_ERROR);
        return true;
    }
    product_ui_dispatch(ui, PRODUCT_UI_EVENT_TTS_STARTED);
    return strcmp(state_name, "speaking") == 0;
}

static bool parse_event(const char* name, product_ui_event_t* event) {
    static const struct {
        const char* name;
        product_ui_event_t event;
    } events[] = {
        {"connected", PRODUCT_UI_EVENT_CONNECTED},
        {"listen", PRODUCT_UI_EVENT_LISTEN_STARTED},
        {"recording_end", PRODUCT_UI_EVENT_RECORDING_ENDED},
        {"tts_start", PRODUCT_UI_EVENT_TTS_STARTED},
        {"tts_stop", PRODUCT_UI_EVENT_TTS_STOPPED},
        {"cancel", PRODUCT_UI_EVENT_CANCELLED},
        {"disconnected", PRODUCT_UI_EVENT_DISCONNECTED},
        {"error", PRODUCT_UI_EVENT_TEMPORARY_ERROR},
        {"recover", PRODUCT_UI_EVENT_ERROR_RECOVERED},
    };
    for (size_t index = 0; index < sizeof(events) / sizeof(events[0]); ++index) {
        if (strcmp(name, events[index].name) == 0) {
            *event = events[index].event;
            return true;
        }
    }
    return false;
}

static void print_trace_line(const char* action, const char* result, product_ui_t* ui) {
    printf("action=%s result=%s state=%s envelope=%u\n", action, result,
           product_ui_state_name(product_ui_get_state(ui)),
           (unsigned)product_ui_get_smoothed_envelope(ui));
}

static int trace_events(product_ui_t* ui, const char* event_list) {
    char* events = malloc(strlen(event_list) + 1U);
    if (events == NULL) {
        fputs("Failed to allocate the event trace.\n", stderr);
        return 1;
    }
    strcpy(events, event_list);
    print_trace_line("initial", "initial", ui);

    char* save_pointer = NULL;
    for (char* token = strtok_r(events, ",", &save_pointer); token != NULL;
         token = strtok_r(NULL, ",", &save_pointer)) {
        uint32_t value = 0;
        if (strncmp(token, "level=", 6) == 0) {
            if (!parse_unsigned(token + 6, 1000, &value)) {
                fprintf(stderr, "Invalid playback level: %s\n", token + 6);
                free(events);
                return 2;
            }
            product_ui_set_playback_envelope(ui, (uint16_t)value);
            print_trace_line(token, "applied", ui);
            continue;
        }
        if (strncmp(token, "time=", 5) == 0) {
            if (!parse_unsigned(token + 5, UINT32_MAX, &value)) {
                fprintf(stderr, "Invalid animation time: %s\n", token + 5);
                free(events);
                return 2;
            }
            product_ui_set_animation_time(ui, value);
            print_trace_line(token, "applied", ui);
            continue;
        }

        product_ui_event_t event;
        if (!parse_event(token, &event)) {
            fprintf(stderr, "Unknown Product UI event: %s\n", token);
            free(events);
            return 2;
        }
        const bool changed = product_ui_dispatch(ui, event);
        print_trace_line(token, changed ? "changed" : "ignored", ui);
    }
    free(events);
    return 0;
}

static void apply_key(simulator_controls_t* controls, uint32_t key) {
    switch (key) {
        case 'i':
            if (product_ui_get_state(controls->product_ui) == PRODUCT_UI_STATE_OFFLINE) {
                product_ui_dispatch(controls->product_ui, PRODUCT_UI_EVENT_CONNECTED);
            } else {
                product_ui_dispatch(controls->product_ui, PRODUCT_UI_EVENT_CANCELLED);
            }
            break;
        case 'l':
            product_ui_dispatch(controls->product_ui, PRODUCT_UI_EVENT_LISTEN_STARTED);
            break;
        case 't':
            product_ui_dispatch(controls->product_ui, PRODUCT_UI_EVENT_RECORDING_ENDED);
            break;
        case 's':
            product_ui_dispatch(controls->product_ui, PRODUCT_UI_EVENT_TTS_STARTED);
            break;
        case 'x':
            product_ui_dispatch(controls->product_ui, PRODUCT_UI_EVENT_TTS_STOPPED);
            break;
        case 'o':
            product_ui_dispatch(controls->product_ui, PRODUCT_UI_EVENT_DISCONNECTED);
            break;
        case 'e':
            product_ui_dispatch(controls->product_ui, PRODUCT_UI_EVENT_TEMPORARY_ERROR);
            break;
        case 'r':
            product_ui_dispatch(controls->product_ui, PRODUCT_UI_EVENT_ERROR_RECOVERED);
            break;
        case '+':
            controls->envelope = controls->envelope >= 900 ? 1000 : controls->envelope + 100;
            product_ui_set_playback_envelope(controls->product_ui, controls->envelope);
            break;
        case '-':
            controls->envelope = controls->envelope <= 100 ? 0 : controls->envelope - 100;
            product_ui_set_playback_envelope(controls->product_ui, controls->envelope);
            break;
        default:
            return;
    }
    printf("state=%s envelope_target=%u\n",
           product_ui_state_name(product_ui_get_state(controls->product_ui)),
           (unsigned)controls->envelope);
}

static void on_key(lv_event_t* event) {
    simulator_controls_t* controls = lv_event_get_user_data(event);
    apply_key(controls, lv_event_get_key(event));
}

int main(int argc, char** argv) {
    const bool smoke_test = argc == 2 && strcmp(argv[1], "--smoke-test") == 0;
    const bool legacy_reference = argc == 3 && strcmp(argv[1], "--render-reference") == 0;
    const bool render_state = argc == 6 && strcmp(argv[1], "--render-state") == 0;
    const bool event_trace = argc == 3 && strcmp(argv[1], "--trace-events") == 0;
    const bool render_clock = argc == 4 && strcmp(argv[1], "--render-clock") == 0;
    const bool render_clock_unsynchronized =
        argc == 3 && strcmp(argv[1], "--render-clock-unsynchronized") == 0;
    const bool inspect_clock_epoch = argc == 3 && strcmp(argv[1], "--inspect-clock") == 0;
    const bool inspect_clock_unsynchronized =
        argc == 2 && strcmp(argv[1], "--inspect-clock-unsynchronized") == 0;
    const bool trace_clock = argc == 5 && strcmp(argv[1], "--trace-clock-updates") == 0;
    const bool inspect_clock_zone = argc == 4 && strcmp(argv[1], "--inspect-clock-zone") == 0;
    const bool interactive = argc == 1;
    if (!interactive && !smoke_test && !legacy_reference && !render_state && !event_trace &&
        !render_clock && !render_clock_unsynchronized && !inspect_clock_epoch &&
        !inspect_clock_unsynchronized && !trace_clock && !inspect_clock_zone) {
        print_usage(argv[0]);
        return 2;
    }

    uint32_t reference_time = 0;
    uint32_t reference_envelope = 0;
    if (render_state && (!parse_unsigned(argv[3], UINT32_MAX, &reference_time) ||
                         !parse_unsigned(argv[4], 1000, &reference_envelope))) {
        fputs("TIME_MS must be a nonnegative 32-bit integer and ENVELOPE must be 0..1000.\n",
              stderr);
        return 2;
    }

    int64_t clock_epoch = 0;
    if ((render_clock || inspect_clock_epoch) && !parse_epoch(argv[2], &clock_epoch)) {
        fputs("EPOCH must be a nonnegative signed 64-bit integer.\n", stderr);
        return 2;
    }
    if (inspect_clock_zone && !parse_epoch(argv[3], &clock_epoch)) {
        fputs("EPOCH must be a nonnegative signed 64-bit integer.\n", stderr);
        return 2;
    }
    int64_t clock_update_epochs[3] = {0};
    if (trace_clock && (!parse_epoch(argv[2], &clock_update_epochs[0]) ||
                        !parse_epoch(argv[3], &clock_update_epochs[1]) ||
                        !parse_epoch(argv[4], &clock_update_epochs[2]))) {
        fputs("Clock update epochs must be nonnegative signed 64-bit integers.\n", stderr);
        return 2;
    }

    if (!companion_time_configure(COMPANION_TIMEZONE_NAME)) {
        fputs("Failed to configure the simulator time zone.\n", stderr);
        return 1;
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

    product_ui_t* product_ui = product_ui_create(lv_screen_active());
    if (product_ui == NULL || product_ui_get_root(product_ui) == NULL) {
        fputs("Failed to create the Product UI.\n", stderr);
        return 1;
    }

    clock_screen_t* clock_screen = clock_screen_create(lv_screen_active());
    if (clock_screen == NULL || clock_screen_get_root(clock_screen) == NULL) {
        fputs("Failed to create the Clock Screen.\n", stderr);
        return 1;
    }
    if (clock_screen_create(NULL) != NULL ||
        !is_interaction_transparent(clock_screen_get_root(clock_screen))) {
        fputs("Clock Screen accepted invalid input or blocks pointer input.\n", stderr);
        return 1;
    }
    lv_obj_add_flag(clock_screen_get_root(clock_screen), LV_OBJ_FLAG_HIDDEN);
    if (!is_interaction_transparent(product_ui_get_root(product_ui))) {
        fputs("Product UI blocks pointer input.\n", stderr);
        return 1;
    }

    if (event_trace) {
        return trace_events(product_ui, argv[2]);
    }

    if (inspect_clock_epoch || inspect_clock_unsynchronized) {
        return inspect_clock(clock_screen, clock_epoch, inspect_clock_epoch);
    }
    if (inspect_clock_zone) {
        const bool fallback = !companion_time_configure(argv[2]);
        if (fallback && !companion_time_configure(COMPANION_TIMEZONE_NAME)) {
            fputs("Failed to configure the fallback time zone.\n", stderr);
            return 1;
        }
        printf("requested=%s active=%s fallback=%s ", argv[2], COMPANION_TIMEZONE_NAME,
               fallback ? "yes" : "no");
        return inspect_clock(clock_screen, clock_epoch, true);
    }
    if (trace_clock) {
        return trace_clock_updates(clock_screen, clock_update_epochs);
    }

    for (int iteration = 0; iteration < 10; ++iteration) {
        lv_timer_handler();
        lv_delay_ms(5);
    }

    if (legacy_reference) {
        product_ui_dispatch(product_ui, PRODUCT_UI_EVENT_CONNECTED);
        product_ui_set_animation_time(product_ui, 0);
        return write_reference_image(product_ui_get_root(product_ui), argv[2]);
    }

    if (render_state) {
        if (!dispatch_state_path(product_ui, argv[2])) {
            fprintf(stderr, "Unknown Product UI state: %s\n", argv[2]);
            return 2;
        }
        product_ui_set_playback_envelope(product_ui, (uint16_t)reference_envelope);
        product_ui_set_animation_time(product_ui, 0);
        product_ui_set_animation_time(product_ui, reference_time);
        return write_reference_image(product_ui_get_root(product_ui), argv[5]);
    }

    if (render_clock || render_clock_unsynchronized) {
        lv_obj_add_flag(product_ui_get_root(product_ui), LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(clock_screen_get_root(clock_screen), LV_OBJ_FLAG_HIDDEN);
        if (!update_clock(clock_screen, clock_epoch, render_clock)) {
            fputs("Failed to render the injected clock epoch.\n", stderr);
            return 1;
        }
        return write_reference_image(clock_screen_get_root(clock_screen),
                                     render_clock ? argv[3] : argv[2]);
    }

    if (smoke_test) {
        return 0;
    }

    simulator_controls_t controls = {
        .product_ui = product_ui,
        .envelope = 0,
    };
    lv_indev_t* keyboard = lv_sdl_keyboard_create();
    lv_group_t* keyboard_group = lv_group_create();
    if (keyboard == NULL || keyboard_group == NULL) {
        fputs("Failed to create simulator keyboard controls.\n", stderr);
        return 1;
    }
    lv_group_add_obj(keyboard_group, product_ui_get_root(product_ui));
    lv_indev_set_group(keyboard, keyboard_group);
    lv_obj_add_event_cb(product_ui_get_root(product_ui), on_key, LV_EVENT_KEY, &controls);

    puts(
        "Controls: i connect/cancel, l listen, t think, s speak, x stop, "
        "o offline, e error, r recover, +/- playback level");
    while (true) {
        lv_timer_handler();
        lv_delay_ms(5);
    }
}

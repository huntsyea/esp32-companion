#include "clock_screen.h"

#include <stdio.h>
#include <string.h>

#define CLOCK_BACKGROUND_COLOR 0x061116
#define CLOCK_PRIMARY_COLOR 0x95E1FF
#define CLOCK_SECONDARY_COLOR 0xB8CAD2
#define CLOCK_MUTED_COLOR 0x74838A
#define CLOCK_ERROR_COLOR 0xF19A9A

#define DIGIT_WIDTH 34
#define DIGIT_HEIGHT 64
#define SEGMENT_THICKNESS 6

enum {
    SEGMENT_TOP,
    SEGMENT_UPPER_RIGHT,
    SEGMENT_LOWER_RIGHT,
    SEGMENT_BOTTOM,
    SEGMENT_LOWER_LEFT,
    SEGMENT_UPPER_LEFT,
    SEGMENT_MIDDLE,
    SEGMENT_COUNT,
};

struct clock_screen {
    lv_obj_t* root;
    lv_obj_t* synchronized_group;
    lv_obj_t* unsynchronized_group;
    lv_obj_t* digits[4][SEGMENT_COUNT];
    lv_obj_t* colon[2];
    lv_obj_t* period_label;
    lv_obj_t* weekday_label;
    lv_obj_t* date_label;
    clock_screen_model_t model;
    bool has_model;
};

static const uint8_t DIGIT_SEGMENTS[10] = {
    0x3F,  // 0: top, upper/lower right, bottom, lower/upper left
    0x06,  // 1
    0x5B,  // 2
    0x4F,  // 3
    0x66,  // 4
    0x6D,  // 5
    0x7D,  // 6
    0x07,  // 7
    0x7F,  // 8
    0x6F,  // 9
};

static const char* const WEEKDAYS[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char* const MONTHS[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                     "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

static void make_event_transparent(lv_obj_t* object) {
    lv_obj_add_flag(object, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
}

static void apply_surface_style(lv_obj_t* object, uint32_t color, int32_t radius) {
    lv_obj_set_style_bg_color(object, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(object, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(object, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(object, radius, LV_PART_MAIN);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(object, LV_SCROLLBAR_MODE_OFF);
    make_event_transparent(object);
}

static lv_obj_t* create_shape(lv_obj_t* parent, int32_t x, int32_t y, int32_t width, int32_t height,
                              uint32_t color, int32_t radius) {
    lv_obj_t* shape = lv_obj_create(parent);
    lv_obj_set_pos(shape, x, y);
    lv_obj_set_size(shape, width, height);
    apply_surface_style(shape, color, radius);
    return shape;
}

static lv_obj_t* create_label(lv_obj_t* parent, int32_t y, uint32_t color) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_set_width(label, LV_PCT(100));
    lv_obj_set_y(label, y);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
    make_event_transparent(label);
    return label;
}

static void create_digit(clock_screen_t* screen, size_t index, int32_t x, int32_t y) {
    lv_obj_t** segments = screen->digits[index];
    segments[SEGMENT_TOP] =
        create_shape(screen->synchronized_group, x + SEGMENT_THICKNESS, y, DIGIT_WIDTH - 12,
                     SEGMENT_THICKNESS, CLOCK_PRIMARY_COLOR, LV_RADIUS_CIRCLE);
    segments[SEGMENT_UPPER_RIGHT] = create_shape(
        screen->synchronized_group, x + DIGIT_WIDTH - SEGMENT_THICKNESS, y + SEGMENT_THICKNESS,
        SEGMENT_THICKNESS, (DIGIT_HEIGHT / 2) - 6, CLOCK_PRIMARY_COLOR, LV_RADIUS_CIRCLE);
    segments[SEGMENT_LOWER_RIGHT] = create_shape(
        screen->synchronized_group, x + DIGIT_WIDTH - SEGMENT_THICKNESS, y + (DIGIT_HEIGHT / 2),
        SEGMENT_THICKNESS, (DIGIT_HEIGHT / 2) - 6, CLOCK_PRIMARY_COLOR, LV_RADIUS_CIRCLE);
    segments[SEGMENT_BOTTOM] = create_shape(
        screen->synchronized_group, x + SEGMENT_THICKNESS, y + DIGIT_HEIGHT - SEGMENT_THICKNESS,
        DIGIT_WIDTH - 12, SEGMENT_THICKNESS, CLOCK_PRIMARY_COLOR, LV_RADIUS_CIRCLE);
    segments[SEGMENT_LOWER_LEFT] =
        create_shape(screen->synchronized_group, x, y + (DIGIT_HEIGHT / 2), SEGMENT_THICKNESS,
                     (DIGIT_HEIGHT / 2) - 6, CLOCK_PRIMARY_COLOR, LV_RADIUS_CIRCLE);
    segments[SEGMENT_UPPER_LEFT] =
        create_shape(screen->synchronized_group, x, y + SEGMENT_THICKNESS, SEGMENT_THICKNESS,
                     (DIGIT_HEIGHT / 2) - 6, CLOCK_PRIMARY_COLOR, LV_RADIUS_CIRCLE);
    segments[SEGMENT_MIDDLE] =
        create_shape(screen->synchronized_group, x + SEGMENT_THICKNESS,
                     y + (DIGIT_HEIGHT / 2) - (SEGMENT_THICKNESS / 2), DIGIT_WIDTH - 12,
                     SEGMENT_THICKNESS, CLOCK_PRIMARY_COLOR, LV_RADIUS_CIRCLE);
}

static void set_visible(lv_obj_t* object, bool visible) {
    if (visible) {
        lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    }
}

static void set_digit(clock_screen_t* screen, size_t index, uint8_t digit) {
    const uint8_t mask = DIGIT_SEGMENTS[digit];
    for (size_t segment = 0; segment < SEGMENT_COUNT; ++segment) {
        set_visible(screen->digits[index][segment], (mask & (1U << segment)) != 0);
    }
}

static bool models_equal(const clock_screen_model_t* left, const clock_screen_model_t* right) {
    return left->synchronized == right->synchronized && left->is_dst == right->is_dst &&
           strcmp(left->time, right->time) == 0 && strcmp(left->period, right->period) == 0 &&
           strcmp(left->weekday, right->weekday) == 0 && strcmp(left->date, right->date) == 0;
}

static void on_root_deleted(lv_event_t* event) { lv_free(lv_event_get_user_data(event)); }

bool clock_screen_format(const companion_local_time_t* local_time, clock_screen_model_t* model) {
    if (local_time == NULL || model == NULL) {
        return false;
    }

    *model = (clock_screen_model_t){.synchronized = local_time->synchronized};
    if (!local_time->synchronized) {
        return true;
    }
    if (local_time->year < 1970 || local_time->month < 1 || local_time->month > 12 ||
        local_time->day < 1 || local_time->day > 31 || local_time->weekday > 6 ||
        local_time->hour > 23 || local_time->minute > 59) {
        model->synchronized = false;
        return false;
    }

    const unsigned hour_12 = local_time->hour % 12U == 0 ? 12U : local_time->hour % 12U;
    snprintf(model->time, sizeof(model->time), "%u:%02u", hour_12, local_time->minute);
    snprintf(model->period, sizeof(model->period), "%s", local_time->hour < 12 ? "AM" : "PM");
    snprintf(model->weekday, sizeof(model->weekday), "%s", WEEKDAYS[local_time->weekday]);
    snprintf(model->date, sizeof(model->date), "%s-%u-%d", MONTHS[local_time->month - 1],
             local_time->day, local_time->year);
    model->is_dst = local_time->is_dst;
    return true;
}

clock_screen_t* clock_screen_create(lv_obj_t* parent) {
    if (parent == NULL) {
        return NULL;
    }

    clock_screen_t* screen = lv_malloc_zeroed(sizeof(*screen));
    if (screen == NULL) {
        return NULL;
    }

    screen->root = lv_obj_create(parent);
    lv_obj_set_size(screen->root, LV_PCT(100), LV_PCT(100));
    lv_obj_align(screen->root, LV_ALIGN_CENTER, 0, 0);
    apply_surface_style(screen->root, CLOCK_BACKGROUND_COLOR, 0);

    screen->synchronized_group = lv_obj_create(screen->root);
    lv_obj_set_size(screen->synchronized_group, LV_PCT(100), LV_PCT(100));
    apply_surface_style(screen->synchronized_group, CLOCK_BACKGROUND_COLOR, 0);
    screen->unsynchronized_group = lv_obj_create(screen->root);
    lv_obj_set_size(screen->unsynchronized_group, LV_PCT(100), LV_PCT(100));
    apply_surface_style(screen->unsynchronized_group, CLOCK_BACKGROUND_COLOR, 0);

    screen->weekday_label = create_label(screen->synchronized_group, 25, CLOCK_SECONDARY_COLOR);
    screen->date_label = create_label(screen->synchronized_group, 47, CLOCK_MUTED_COLOR);
    create_digit(screen, 0, 18, 82);
    create_digit(screen, 1, 58, 82);
    create_digit(screen, 2, 119, 82);
    create_digit(screen, 3, 159, 82);
    screen->colon[0] = create_shape(screen->synchronized_group, 106, 100, 6, 6, CLOCK_PRIMARY_COLOR,
                                    LV_RADIUS_CIRCLE);
    screen->colon[1] = create_shape(screen->synchronized_group, 106, 124, 6, 6, CLOCK_PRIMARY_COLOR,
                                    LV_RADIUS_CIRCLE);
    screen->period_label = create_label(screen->synchronized_group, 166, CLOCK_PRIMARY_COLOR);

    lv_obj_t* zone_label = create_label(screen->synchronized_group, 204, CLOCK_MUTED_COLOR);
    lv_label_set_text(zone_label, "LOCAL TIME");
    lv_obj_t* unsynchronized_title =
        create_label(screen->unsynchronized_group, 91, CLOCK_ERROR_COLOR);
    lv_label_set_text(unsynchronized_title, "TIME NOT SYNCED");
    lv_obj_t* unsynchronized_detail =
        create_label(screen->unsynchronized_group, 121, CLOCK_MUTED_COLOR);
    lv_label_set_text(unsynchronized_detail, "Connect to network");

    lv_obj_add_event_cb(screen->root, on_root_deleted, LV_EVENT_DELETE, screen);
    const companion_local_time_t initial_time = {.synchronized = false};
    clock_screen_update(screen, &initial_time);
    return screen;
}

lv_obj_t* clock_screen_get_root(const clock_screen_t* screen) {
    return screen == NULL ? NULL : screen->root;
}

bool clock_screen_update(clock_screen_t* screen, const companion_local_time_t* local_time) {
    if (screen == NULL || local_time == NULL) {
        return false;
    }

    clock_screen_model_t model;
    clock_screen_format(local_time, &model);
    if (screen->has_model && models_equal(&screen->model, &model)) {
        return false;
    }

    screen->model = model;
    screen->has_model = true;
    set_visible(screen->synchronized_group, model.synchronized);
    set_visible(screen->unsynchronized_group, !model.synchronized);
    if (!model.synchronized) {
        return true;
    }

    const uint8_t hour_tens = model.time[1] == ':' ? 0 : (uint8_t)(model.time[0] - '0');
    const size_t separator = model.time[1] == ':' ? 1U : 2U;
    const uint8_t hour_ones = (uint8_t)(model.time[separator - 1] - '0');
    const uint8_t minute_tens = (uint8_t)(model.time[separator + 1] - '0');
    const uint8_t minute_ones = (uint8_t)(model.time[separator + 2] - '0');
    if (model.time[1] == ':') {
        for (size_t segment = 0; segment < SEGMENT_COUNT; ++segment) {
            set_visible(screen->digits[0][segment], false);
        }
    } else {
        set_digit(screen, 0, hour_tens);
    }
    set_digit(screen, 1, hour_ones);
    set_digit(screen, 2, minute_tens);
    set_digit(screen, 3, minute_ones);
    lv_label_set_text(screen->period_label, model.period);
    lv_label_set_text(screen->weekday_label, model.weekday);
    lv_label_set_text(screen->date_label, model.date);
    return true;
}

const clock_screen_model_t* clock_screen_get_model(const clock_screen_t* screen) {
    return screen == NULL || !screen->has_model ? NULL : &screen->model;
}

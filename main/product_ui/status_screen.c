#include "status_screen.h"
#include "view_helpers.h"

#include <stdio.h>
#include <string.h>

#define STATUS_BACKGROUND_COLOR 0x061116
#define STATUS_PRIMARY_COLOR 0x95E1FF
#define STATUS_SECONDARY_COLOR 0xB8CAD2
#define STATUS_MUTED_COLOR 0x74838A
#define STATUS_OK_COLOR 0x7BC67E
#define STATUS_ERROR_COLOR 0xF19A9A

struct status_screen {
    lv_obj_t* root;
    lv_obj_t* wifi_label;
    lv_obj_t* hermes_label;
    lv_obj_t* wifi_state;
    lv_obj_t* hermes_state;
    bool wifi_connected;
    bool hermes_connected;
    bool has_status;
};

static lv_obj_t* create_label(lv_obj_t* parent, int32_t x, int32_t y, uint32_t color) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, LV_PCT(100));
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
    product_ui_view_make_event_transparent(label);
    return label;
}

static lv_obj_t* create_state_label(lv_obj_t* parent, int32_t x, int32_t y) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, LV_PCT(100));
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
    product_ui_view_make_event_transparent(label);
    return label;
}

static void on_root_deleted(lv_event_t* event) { lv_free(lv_event_get_user_data(event)); }

status_screen_t* status_screen_create(lv_obj_t* parent) {
    if (parent == NULL) {
        return NULL;
    }

    status_screen_t* screen = lv_malloc_zeroed(sizeof(*screen));
    if (screen == NULL) {
        return NULL;
    }

    screen->root = lv_obj_create(parent);
    lv_obj_set_size(screen->root, LV_PCT(100), LV_PCT(100));
    lv_obj_align(screen->root, LV_ALIGN_CENTER, 0, 0);
    product_ui_view_apply_surface_style(screen->root, STATUS_BACKGROUND_COLOR, 0);

    lv_obj_t* title = create_label(screen->root, 0, 30, STATUS_PRIMARY_COLOR);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_text(title, "CONNECTION");

    screen->wifi_label = create_label(screen->root, 20, 90, STATUS_SECONDARY_COLOR);
    lv_label_set_text(screen->wifi_label, "WiFi");
    screen->wifi_state = create_state_label(screen->root, 20, 90);

    screen->hermes_label = create_label(screen->root, 20, 130, STATUS_SECONDARY_COLOR);
    lv_label_set_text(screen->hermes_label, "Hermes");
    screen->hermes_state = create_state_label(screen->root, 20, 130);

    lv_obj_add_event_cb(screen->root, on_root_deleted, LV_EVENT_DELETE, screen);
    return screen;
}

lv_obj_t* status_screen_get_root(const status_screen_t* screen) {
    return screen == NULL ? NULL : screen->root;
}

void status_screen_set_connection_status(status_screen_t* screen, bool wifi_connected,
                                         bool hermes_connected) {
    if (screen == NULL) {
        return;
    }
    screen->wifi_connected = wifi_connected;
    screen->hermes_connected = hermes_connected;
    screen->has_status = true;

    lv_obj_set_style_text_color(screen->wifi_state,
                                lv_color_hex(wifi_connected ? STATUS_OK_COLOR : STATUS_ERROR_COLOR),
                                LV_PART_MAIN);
    lv_label_set_text(screen->wifi_state, wifi_connected ? "Connected" : "Offline");

    lv_obj_set_style_text_color(
        screen->hermes_state,
        lv_color_hex(hermes_connected ? STATUS_OK_COLOR : STATUS_ERROR_COLOR), LV_PART_MAIN);
    lv_label_set_text(screen->hermes_state, hermes_connected ? "Connected" : "Offline");
}

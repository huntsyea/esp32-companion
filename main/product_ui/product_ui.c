#include "product_ui.h"

#define PRODUCT_UI_BACKGROUND_COLOR 0xEDF5F9
#define PRODUCT_UI_TEXT_COLOR 0x061116

static void make_event_transparent(lv_obj_t* object) {
    lv_obj_add_flag(object, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_CLICKABLE);
}

static lv_obj_t* create_label(lv_obj_t* parent, const char* text, lv_align_t alignment, int x,
                              int y) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(PRODUCT_UI_TEXT_COLOR), LV_PART_MAIN);
    lv_obj_align(label, alignment, x, y);
    make_event_transparent(label);
    return label;
}

static lv_obj_t* create_color_swatch(lv_obj_t* parent, uint32_t color, int x) {
    lv_obj_t* swatch = lv_obj_create(parent);
    lv_obj_set_size(swatch, 40, 40);
    lv_obj_align(swatch, LV_ALIGN_CENTER, x, -4);
    lv_obj_set_style_bg_color(swatch, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(swatch, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(swatch, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(swatch, lv_color_hex(PRODUCT_UI_TEXT_COLOR), LV_PART_MAIN);
    lv_obj_set_style_radius(swatch, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_all(swatch, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(swatch, LV_SCROLLBAR_MODE_OFF);
    make_event_transparent(swatch);
    return swatch;
}

static void update_touch_position(lv_event_t* event) {
    lv_obj_t* touch_label = lv_event_get_user_data(event);
    lv_indev_t* input = lv_indev_active();
    if (input == NULL) {
        lv_label_set_text(touch_label, "Touch input pending");
        return;
    }

    lv_point_t point;
    lv_indev_get_point(input, &point);
    lv_label_set_text_fmt(touch_label, "Touch %d, %d", (int)point.x, (int)point.y);
}

lv_obj_t* product_ui_create(lv_obj_t* parent) {
    if (parent == NULL) {
        return NULL;
    }

    lv_obj_t* root = lv_obj_create(parent);
    lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));
    lv_obj_align(root, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(root, lv_color_hex(PRODUCT_UI_BACKGROUND_COLOR), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(root, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(root, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(root, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    create_label(root, "^ TOP ^", LV_ALIGN_TOP_MID, 0, 12);
    create_label(root, "Companion baseline", LV_ALIGN_TOP_MID, 0, 38);

    create_color_swatch(root, 0xFF0000, -69);
    create_color_swatch(root, 0x00FF00, -23);
    create_color_swatch(root, 0x0000FF, 23);
    create_color_swatch(root, 0xFFFFFF, 69);

    create_label(root, "RGB + white", LV_ALIGN_CENTER, 0, 31);
    create_label(root, "Touch the display", LV_ALIGN_BOTTOM_MID, 0, -43);
    lv_obj_t* touch_label = create_label(root, "Touch input pending", LV_ALIGN_BOTTOM_MID, 0, -18);

    lv_obj_add_event_cb(root, update_touch_position, LV_EVENT_PRESSING, touch_label);
    return root;
}

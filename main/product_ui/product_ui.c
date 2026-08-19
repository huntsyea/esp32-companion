#include "product_ui.h"

#include "product_identity.h"

static void make_event_transparent(lv_obj_t* object) {
    lv_obj_add_flag(object, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
}

static void apply_shape_style(lv_obj_t* object, uint32_t color, int32_t radius) {
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
    apply_shape_style(shape, color, radius);
    return shape;
}

static void create_eye(lv_obj_t* parent, int32_t x, int32_t y, int32_t width, int32_t height) {
    lv_obj_t* eye = create_shape(parent, x, y, width, height, PRODUCT_IDENTITY_EYE_COLOR,
                                 PRODUCT_IDENTITY_EYE_RADIUS);
    lv_obj_set_style_transform_pivot_x(eye, LV_PCT(50), LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(eye, LV_PCT(50), LV_PART_MAIN);
    lv_obj_set_style_transform_rotation(eye, PRODUCT_IDENTITY_EYE_ROTATION, LV_PART_MAIN);
}

lv_obj_t* product_ui_create(lv_obj_t* parent) {
    if (parent == NULL) {
        return NULL;
    }

    lv_obj_t* root = lv_obj_create(parent);
    lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));
    lv_obj_align(root, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(root, lv_color_hex(PRODUCT_IDENTITY_BACKGROUND_COLOR), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(root, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(root, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(root, LV_SCROLLBAR_MODE_OFF);
    make_event_transparent(root);

    create_shape(root, PRODUCT_IDENTITY_BODY_X, PRODUCT_IDENTITY_BODY_Y,
                 PRODUCT_IDENTITY_BODY_WIDTH, PRODUCT_IDENTITY_BODY_HEIGHT,
                 PRODUCT_IDENTITY_BODY_COLOR, LV_RADIUS_CIRCLE);
    create_eye(root, PRODUCT_IDENTITY_LEFT_EYE_X, PRODUCT_IDENTITY_LEFT_EYE_Y,
               PRODUCT_IDENTITY_LEFT_EYE_WIDTH, PRODUCT_IDENTITY_LEFT_EYE_HEIGHT);
    create_eye(root, PRODUCT_IDENTITY_RIGHT_EYE_X, PRODUCT_IDENTITY_RIGHT_EYE_Y,
               PRODUCT_IDENTITY_RIGHT_EYE_WIDTH, PRODUCT_IDENTITY_RIGHT_EYE_HEIGHT);

    return root;
}

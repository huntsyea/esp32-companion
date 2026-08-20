#include "app_ui.h"
#include "board.h"

#include "esp_log.h"
#include "esp_lvgl_port.h"

static const char *TAG = "app";

void app_main(void)
{
    ESP_ERROR_CHECK(board_init());

    if (lvgl_port_lock(0)) {
        app_ui_create();
        lvgl_port_unlock();
    }

    ESP_ERROR_CHECK(board_backlight_set(80));
    ESP_LOGI(TAG, "ui ready");
}

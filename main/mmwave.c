#include "esp_log.h"
#include "esp_system.h"
#include "driver/gpio.h"

#include "esp_rd-03d.h"

radar_sensor_t radar;

void sensor_task(void *pvParameters) {
    // Initialize the radar sensor
    esp_err_t ret = radar_sensor_init(&radar, UART_NUM_1, 39, 38);
    if (ret != ESP_OK) {
        switch (ret)
        {
        case ESP_OK:
            ESP_LOGI("RD-03D", "Initialization successful");
            break;
        case ESP_ERR_INVALID_ARG:
            ESP_LOGE("RD-03D", "Invalid arguments provided");
            break;
        default:
            ESP_LOGE("RD-03D", "Initialization failed: %s", esp_err_to_name(ret));
            break;
        }
        vTaskDelete(NULL);
    }

    // Start UART communication
    ret = radar_sensor_begin(&radar, 256000);
    if (ret != ESP_OK)
    {
        ESP_LOGE("RD-03D", "Failed to start radar sensor");
        vTaskDelete(NULL);
    }

    // Configure for security application (longer retention)
    radar_sensor_set_retention_times(&radar, 10000, 500); // 10s detection, 0.5s absence

    ESP_LOGI("RD-03D", "Sensor is active, starting main loop.");
    // Main loop
    while (1)
    {
        if (radar_sensor_update(&radar))
        {
            radar_target_t target = radar_sensor_get_target(&radar);

            if (target.detected)
            {
                ESP_LOGI("RD-03D", "Target detected at (%.1f, %.1f) mm, distance: %.1f mm",target.x, target.y, target.distance);
                ESP_LOGI("RD-03D", "Position: %s", target.position_description);
                ESP_LOGI("RD-03D", "Angle: %.1f degrees, Distance: %.1f mm, Speed: %.1f mm/s", target.angle, target.distance, target.speed );
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

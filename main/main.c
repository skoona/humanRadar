/*
 * LVGL 9+, ILI9488, and Touch FT6436
 * skoona@gmail.com
 * 12/29/2025
 * LVGL Template
 * file: main.c
 */

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <dirent.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "wifi_network.h"

#define SKN_LVGL_PRIORITY 4
#define SKN_LVGL_STACK_SZ 9216 // 8192
#define BEEP_DURATION_MS 500

#define BUZZER_GPIO   20
#define LEDC_TIMER     LEDC_TIMER_0
#define LEDC_MODE      LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL   LEDC_CHANNEL_0
#define LEDC_DUTY_RES  LEDC_TIMER_13_BIT // 8191
#define LEDC_DUTY      (4095)			 // 50% duty cycle (4095 out of 8191)
#define LEDC_FREQUENCY (600)			 // Hz tone

const char *TAG = "rgb_display";


extern void vDisplayServiceTask(void *pvParameters);
extern esp_err_t startWiFiService(void);
void sensor_task(void *pvParameters);

esp_err_t fileList() {
	/* Get file name in storage */
	struct dirent *p_dirent = NULL;
	struct stat st;
	ESP_LOGI(TAG, "fileList(): Proceeding...");

	DIR *p_dir_stream = opendir("/spiffs");
	if (p_dir_stream == NULL) {
		ESP_LOGE(TAG, "Failed to open mount: %s", "/spiffs");
		return ESP_FAIL;
	}

	char files[256] = {"/spiffs/"};

	/* Scan files in storage */
	while (true) {
		p_dirent = readdir(p_dir_stream);
		if (NULL != p_dirent) {
			strcpy(files, "/spiffs/");
			strcat(files, p_dirent->d_name);
			if (stat(files, &st) == 0) {
				ESP_LOGI(TAG, "Filename: [%d] %s", st.st_size,
						 p_dirent->d_name);
			} else {
				ESP_LOGI(TAG, "Filename: %s", p_dirent->d_name);
			}
		} else {
			closedir(p_dir_stream);
			break;
		}
	}
	return ESP_OK;
}
esp_err_t skn_spiffs_mount(void) {
	esp_vfs_spiffs_conf_t conf = {
		.base_path = "/spiffs",
		.partition_label = "storage",
		.max_files = 12,
		.format_if_mount_failed = false,
	};

	esp_err_t ret_val = esp_vfs_spiffs_register(&conf);

	ESP_ERROR_CHECK(ret_val);

	size_t total = 0, used = 0;
	ret_val = esp_spiffs_info(conf.partition_label, &total, &used);
	if (ret_val != ESP_OK) {
		ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)",
				 esp_err_to_name(ret_val));
	} else {
		ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
	}

	return ret_val;
}
esp_err_t skn_spiffs_unmount(void) {
	return esp_vfs_spiffs_unregister("storage");
}
esp_err_t skn_beep_init() {
	ESP_LOGI(TAG, "skn_beep_init(): Initializing");
	// Configure LEDC Timer
	ledc_timer_config_t timer_conf = {
		.speed_mode = LEDC_MODE,
		.timer_num = LEDC_TIMER,
		.duty_resolution = LEDC_DUTY_RES,
		.freq_hz = LEDC_FREQUENCY,
		.clk_cfg = LEDC_AUTO_CLK,
	};
	ledc_timer_config(&timer_conf);

	// Configure LEDC Channel
	ledc_channel_config_t channel_conf = {
		.gpio_num = BUZZER_GPIO,
		.speed_mode = LEDC_MODE,
		.channel = LEDC_CHANNEL,
		.intr_type = LEDC_INTR_DISABLE,
		.timer_sel = LEDC_TIMER,
		.duty = 0, // Start with buzzer off
	};
	return ledc_channel_config(&channel_conf);
}
void skn_beep(uint32_t duration_ms) {
	// Set the duty cycle and update the channel to start the sound
	ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY);
	ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);

	// Wait for the beep duration
	vTaskDelay(pdMS_TO_TICKS(duration_ms));

	// Stop the sound (set duty to 0)
	ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0);
	ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}
void logMemoryStats(char *message) {
	char buffer[1024] = {0};

	vTaskList(buffer);

	ESP_LOGI(TAG, "[APP] %s...", message);
	ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
	ESP_LOGI(TAG, "Internal free heap size: %ld bytes", esp_get_free_internal_heap_size());
    ESP_LOGI(TAG, "PSRAM    free heap size: %ld bytes", esp_get_free_heap_size() - esp_get_free_internal_heap_size());
    ESP_LOGI(TAG, "Total    free heap size: %ld bytes", esp_get_free_heap_size());
	ESP_LOGI(TAG, "Task List:\n%s", buffer);
}

void app_main(void) {
	vTaskDelay(pdMS_TO_TICKS(1000));

	logMemoryStats("Startup Begining...");

	esp_log_level_set("*", ESP_LOG_INFO);
	esp_log_level_set("transport", ESP_LOG_VERBOSE);

	ESP_ERROR_CHECK(esp_event_loop_create_default());

	ESP_ERROR_CHECK(skn_wifi_service());
	ESP_ERROR_CHECK(skn_spiffs_mount());
	ESP_ERROR_CHECK(skn_beep_init());
	
	xTaskCreatePinnedToCore(vDisplayServiceTask, "SKN Display", SKN_LVGL_STACK_SZ, NULL, (SKN_LVGL_PRIORITY), NULL, 0);
	xTaskCreatePinnedToCore(sensor_task, "RD-03D Sensor", 4096, NULL, 8, NULL, tskNO_AFFINITY );

	skn_beep(BEEP_DURATION_MS);
	logMemoryStats("Startup Complete...");
}

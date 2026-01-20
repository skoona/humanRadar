/*
 * rgb_panel.c
 */

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_lcd_ili9488.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_touch_ft6x36.h"
#include "esp_log.h"
#include "esp_lv_decoder.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "jpeg_decoder.h"
#include "lvgl.h"
#include <stdio.h>
#include "radar_panel.h"

extern char *TAG; //  = "Display";

#define BEEP_DURATION_MS 500

#define SKN_BUFFER_BASE       (CONFIG_LCD_V_RES * CONFIG_LCD_BUFFER_SIZE_FACTOR)
#define SKN_DRAW_BUFF_SZ      (SKN_BUFFER_BASE * sizeof(lv_color_t))
#define SKN_TRANSFER_BUFF_SZ  (CONFIG_LCD_V_RES * (CONFIG_LCD_BUFFER_SIZE_FACTOR * 2) * sizeof(uint16_t))


static esp_lcd_touch_handle_t touch_panel = NULL;
static esp_lcd_panel_handle_t lcd_panel = NULL;
static lv_indev_t *lvgl_touch_indev = NULL;
static lv_display_t *display; // contains callback functions

const uint32_t panel_Hres = CONFIG_LCD_H_RES;
const uint32_t panel_Vres = CONFIG_LCD_V_RES;
extern void ui_skoona_page(lv_obj_t *scr);
extern void logMemoryStats(char *message);
extern esp_err_t skn_beep_init();
extern esp_err_t skn_beep();
extern esp_err_t fileList();

void skn_touch_event_handler(lv_event_t *e) {
	lv_point_t p;
	lv_indev_get_point(e->user_data, &p);
	int32_t screen_width = lv_obj_get_width(lv_scr_act());
	int32_t screen_height = lv_obj_get_height(lv_scr_act());

	printf("-->Event: X=%3.0ld\tY=%3.0ld\tScrnX=%3.0ld\tScrnY=%3.0ld\n", p.x,
		   p.y, screen_width, screen_height);
	if (p.y > (screen_height / 2)) {
		printf("Task List: y=%ld\n", p.y);
		logMemoryStats("Active Task List");
		fileList();
	}
}

static bool skn_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io,
										esp_lcd_panel_io_event_data_t *edata,
										void *user_ctx) {
	lv_display_t *disp_driver = (lv_display_t *)user_ctx;
	lv_disp_flush_ready(disp_driver);
	return false;
}
static void skn_lvgl_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *color_map) {
	esp_lcd_panel_handle_t panel_handle =
		(esp_lcd_panel_handle_t)display->user_data;

	int offsetx1 = area->x1;
	int offsetx2 = area->x2;
	int offsety1 = area->y1;
	int offsety2 = area->y2;
	esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1,
							  offsety2 + 1, color_map);
	lv_display_flush_ready(display);
}
static uint32_t skn_tick_cb(void) {
	return (uint32_t)esp_timer_get_time() / 1000ULL;
}

void skn_lvgl_touch_cb(lv_indev_t *drv, lv_indev_data_t *data) {
	uint8_t touchpad_cnt = 0;
	esp_lcd_touch_point_data_t touch_data;
	esp_lcd_touch_handle_t tp = (esp_lcd_touch_handle_t)drv->user_data;

	esp_lcd_touch_read_data(tp);

	esp_err_t err = esp_lcd_touch_get_data(tp, &touch_data, &touchpad_cnt, 1);
	if (err == ESP_OK && touchpad_cnt > 0) {
		data->point.x = touch_data.x;
		data->point.y = touch_data.y;
		data->state = LV_INDEV_STATE_PRESSED;
		ESP_LOGD("Touch", "-->Touch: X=%3.0ld,\tY=%3.0ld\n", data->point.x, data->point.y);
	} else {
		data->state = LV_INDEV_STATE_RELEASED;
	}
}

esp_err_t skn_touch_init() {
	ESP_LOGI("Touch", "Initialize I2C");

	const i2c_config_t i2c_conf = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = CONFIG_TOUCH_I2C_SDA_GPIO,
		.scl_io_num = CONFIG_TOUCH_I2C_SCK_GPIO,
		.sda_pullup_en = GPIO_PULLUP_ENABLE,
		.scl_pullup_en = GPIO_PULLUP_ENABLE,
		.master.clk_speed = CONFIG_I2C_MASTER_FREQUENCY,
	};
	// Initialize I2C
	ESP_ERROR_CHECK(i2c_param_config(CONFIG_I2C_MASTER_PORT_NUM, &i2c_conf));
	ESP_ERROR_CHECK(
		i2c_driver_install(CONFIG_I2C_MASTER_PORT_NUM, i2c_conf.mode, 0, 0, 0));

	esp_lcd_panel_io_i2c_config_t tp_io_config =
		ESP_LCD_TOUCH_IO_I2C_FT6x36_CONFIG();

	ESP_LOGI("Touch", "Initialize touch IO (I2C)");
	esp_lcd_panel_io_handle_t tp_io_handle = NULL;
	ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(
		(esp_lcd_i2c_bus_handle_t)CONFIG_I2C_MASTER_PORT_NUM, &tp_io_config,
		&tp_io_handle));

	esp_lcd_touch_config_t tp_cfg = {
		.x_max = panel_Hres,
		.y_max = panel_Vres,
		.rst_gpio_num = -1,
		.int_gpio_num = -1,
		.levels =
			{
				.reset = 0,
				.interrupt = 0,
			},
		.flags =
			{
				.swap_xy = 1, // rotated
				.mirror_x = 1,
				.mirror_y = 0,
			},
	};

	/* Initialize touch */
	ESP_LOGI("Touch", "Initialize touch controller FT6x36");
	esp_err_t ret =
		esp_lcd_touch_new_i2c_ft6x36(tp_io_handle, &tp_cfg, &touch_panel);

	/* Above Config has V/H swapped and flags set
		esp_lcd_touch_set_swap_xy(tp,true);
		esp_lcd_touch_set_mirror_x(tp, true);
		esp_lcd_touch_set_mirror_y(tp, false);
	*/

	/*Create an input device for touch handling*/

	lvgl_touch_indev = lv_indev_create();
	lv_indev_set_type(lvgl_touch_indev, LV_INDEV_TYPE_POINTER);
	lv_indev_set_read_cb(lvgl_touch_indev, skn_lvgl_touch_cb);
	lv_indev_set_user_data(lvgl_touch_indev, touch_panel);
	lv_indev_add_event_cb(lvgl_touch_indev, skn_touch_event_handler,
						  LV_EVENT_CLICKED, lvgl_touch_indev);

	return ret;
}

esp_err_t skn_lcd_init() {
	ESP_LOGI(TAG, "Initialize Intel 8080 bus");
	esp_lcd_i80_bus_handle_t i80_bus = NULL;
	esp_lcd_i80_bus_config_t bus_config = {
		.clk_src = LCD_CLK_SRC_DEFAULT,
		.dc_gpio_num = CONFIG_LCD_DC_GPIO,
		.wr_gpio_num = CONFIG_LCD_WR_GPIO,
		.data_gpio_nums =
			{
				CONFIG_LCD_D00_GPIO,
				CONFIG_LCD_D01_GPIO,
				CONFIG_LCD_D02_GPIO,
				CONFIG_LCD_D03_GPIO,
				CONFIG_LCD_D04_GPIO,
				CONFIG_LCD_D05_GPIO,
				CONFIG_LCD_D06_GPIO,
				CONFIG_LCD_D07_GPIO,
				CONFIG_LCD_D08_GPIO,
				CONFIG_LCD_D09_GPIO,
				CONFIG_LCD_D10_GPIO,
				CONFIG_LCD_D11_GPIO,
				CONFIG_LCD_D12_GPIO,
				CONFIG_LCD_D13_GPIO,
				CONFIG_LCD_D14_GPIO,
				CONFIG_LCD_D15_GPIO,
			},
		.bus_width = CONFIG_LCD_BUS_WIDTH,
		.max_transfer_bytes = SKN_TRANSFER_BUFF_SZ,
		.psram_trans_align = 0,
		.sram_trans_align = 0,
	};
	ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus));

	esp_lcd_panel_io_handle_t io_handle = NULL;
	esp_lcd_panel_io_i80_config_t io_config = {
		.cs_gpio_num = CONFIG_LCD_CS_GPIO,
		.pclk_hz = CONFIG_LCD_PIXEL_CLOCK_HZ,
		.trans_queue_depth = 10,
		.dc_levels =
			{
				.dc_idle_level = 0,
				.dc_cmd_level = 0,
				.dc_dummy_level = 0,
				.dc_data_level = 1,
			},
		.on_color_trans_done = skn_notify_lvgl_flush_ready,
		.user_ctx = &display,
		.lcd_cmd_bits = CONFIG_LCD_CMD_BITS,
		.lcd_param_bits = CONFIG_LCD_PARAM_BITS,
		.flags =
			{
				.swap_color_bytes = false, // !CONFIG_LV_COLOR_16_SWAP,
				.cs_active_high = false,
				.reverse_color_bits = false,
			},
	};
	ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &io_handle));

	ESP_LOGI(TAG, "Install LCD driver of ili9488");

	esp_lcd_panel_dev_config_t panel_config = {
		.reset_gpio_num = CONFIG_LCD_RST_GPIO,
		.color_space = ESP_LCD_COLOR_SPACE_BGR,
		.bits_per_pixel = CONFIG_LCD_BUS_WIDTH,
	};
	esp_err_t ret = esp_lcd_new_panel_ili9488(
		io_handle, &panel_config, SKN_DRAW_BUFF_SZ, &lcd_panel);

	esp_lcd_panel_reset(lcd_panel);
	esp_lcd_panel_init(lcd_panel);
	esp_lcd_panel_invert_color(lcd_panel, false);
	esp_lcd_panel_set_gap(lcd_panel, 0, 0);

	/*
	 * Rotate LCD display
	 * - following lv_display_create has x/y/ reversed
	 */
	esp_lcd_panel_swap_xy(lcd_panel, true);
	esp_lcd_panel_mirror(lcd_panel, true, false);

	return ret;
}

esp_err_t skn_lvgl_init() {
	ESP_LOGI(TAG, "Initialize LVGL library");

	lv_init();
	lv_tick_set_cb(skn_tick_cb);

	ESP_LOGI(TAG, "Register display driver to LVGL");
	display = lv_display_create(CONFIG_LCD_V_RES, CONFIG_LCD_H_RES);

	// initialize LVGL draw buffers
	printf("[BUFFERS]--> Color Sz: %d\tlv_color_t: %d\tDraw buffer: %'.0u\tTransfer "
		   "buffer: %'.0u\tBuffer base: %.0u\n",
		   lv_color_format_get_size(lv_display_get_color_format(display)),
		   sizeof(lv_color_t), SKN_DRAW_BUFF_SZ, SKN_TRANSFER_BUFF_SZ, SKN_BUFFER_BASE);

	uint8_t *buf1 =
		heap_caps_malloc(SKN_DRAW_BUFF_SZ, MALLOC_CAP_DMA | MALLOC_CAP_32BIT);
	assert(buf1);
	uint8_t *buf2 =
		heap_caps_malloc(SKN_DRAW_BUFF_SZ, MALLOC_CAP_DMA | MALLOC_CAP_32BIT);
	assert(buf2);
	lv_display_set_buffers(display, buf1, buf2, SKN_BUFFER_BASE, LV_DISPLAY_RENDER_MODE_PARTIAL);

	lv_display_set_flush_cb(display, skn_lvgl_flush_cb);
	lv_display_set_user_data(display, lcd_panel);

	esp_lv_decoder_handle_t decoder_handle = NULL;
	esp_lv_decoder_init(&decoder_handle); // Initialize this after lvgl starts

	return ESP_OK;
}

void vDisplayServiceTask(void *pvParameters) {

	ESP_LOGI(TAG, "Configure and Turn off LCD backlight");
	gpio_config_t bk_gpio_config = {.mode = GPIO_MODE_OUTPUT,
									.pin_bit_mask =
										1ULL << CONFIG_LCD_BACK_LIGHT_GPIO};
	ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
	gpio_set_level(CONFIG_LCD_BACK_LIGHT_GPIO, !CONFIG_LCD_BACK_LIGHT_ON_LEVEL);

	ESP_ERROR_CHECK(skn_lcd_init());
	ESP_ERROR_CHECK(skn_lvgl_init());
	ESP_ERROR_CHECK(skn_touch_init());

	ESP_LOGI(TAG, "Turn on LCD backlight");
	gpio_set_level(CONFIG_LCD_BACK_LIGHT_GPIO, CONFIG_LCD_BACK_LIGHT_ON_LEVEL);

	lv_lock();
		lv_obj_t *scr = lv_obj_create(NULL);
		lv_screen_load(scr);

		// Draw radar screen
		lv_obj_t *radar = lv_radar_screen_create(scr, 480, 320);
		int16_t center_x = 240, center_y = 320, radius = 310;
		lv_radar_sweep_t *sweep = lv_radar_sweep_create(radar, center_x, center_y, radius, 4000, true);
		
		// Add person markers
		lv_radar_marker_t markers[2] = {
			{.distance = 2.5f, .angle = 45, .icon = NULL},   // 2.5 meters at 45 degrees
			{.distance = 4.5f, .angle = 120, .icon = NULL}   // 4.5 meters at 120 degrees
		};
		lv_radar_add_markers(radar, center_x, center_y, radius, 4, markers, 2);
		
	lv_unlock();
		
	while (1)
	{
		lv_lock();
		lv_timer_periodic_handler();
		lv_unlock();
	}
	
	lv_radar_sweep_delete(sweep);
}

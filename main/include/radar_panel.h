// radar_panel.h
#pragma once

/**
 * @brief Structure to hold radar sweep state
 */
typedef struct
{
    lv_obj_t *parent;
    int16_t center_x;
    int16_t center_y;
    int16_t radius;
    uint16_t current_angle;
    lv_obj_t *sweep_line;
    lv_obj_t *shadow_lines[5]; // Array for trailing shadow lines
} lv_radar_sweep_t;

/**
 * @brief Structure to hold radar marker information
 */
typedef struct
{
    lv_obj_t *icon;
    float distance;  // Distance in meters
    uint16_t angle;  // Angle in degrees (0-180)
} lv_radar_marker_t;

lv_obj_t *lv_radar_screen_create(lv_obj_t *parent, int16_t width, int16_t height);
lv_radar_sweep_t *lv_radar_sweep_create(lv_obj_t *parent, uint32_t duration_ms, bool loop);
void lv_radar_sweep_delete(lv_radar_sweep_t *sweep);
void lv_radar_add_markers(lv_obj_t *parent,  uint8_t band_count, lv_radar_marker_t *markers, uint8_t marker_count);
void lv_radar_update_markers(uint8_t band_count, lv_radar_marker_t *markers, uint8_t marker_count);
void lv_radar_remove_markers(lv_radar_marker_t *markers, uint8_t marker_count);
void lv_radar_panel_init(int16_t xRes, int16_t yRes);
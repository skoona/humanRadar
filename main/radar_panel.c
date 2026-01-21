#include "lvgl.h"
#include <stdio.h>
#include <math.h>
#include "radar_panel.h"

static int16_t center_x, center_y, radius;
    /**
     * @brief Draw a semi-circle radar grid with 4 horizontal arches and 9 vertical lines
     *
     * The radar screen consists of:
     * - A semi-circle arc (180 to 360 degrees - top half)
     * - 4 horizontal semi-circular arches representing 2 meter divisions
     * - 9 vertical radial lines representing 20-degree increments (180°, 200°, 220°, ..., 360°)
     *
     * @param parent The parent LVGL object to draw on
     * @param center_x The X coordinate of the radar center
     * @param center_y The Y coordinate of the radar center
     * @param radius The radius of the semi-circle in pixels
     * @param band_count Number of horizontal semi-circular arches (typically 4)
     * @param line_count Number of vertical radial lines (typically 9)
     */
void lv_radar_screen_draw(lv_obj_t *parent, uint8_t band_count, uint8_t line_count) {
    // Create style for radar lines
    static lv_style_t style_line;
    lv_style_init(&style_line);
    lv_style_set_line_width(&style_line, 2);
    lv_style_set_line_color(&style_line, lv_color_hex(0x4080FF));  // Light blue
    lv_style_set_line_rounded(&style_line, false);

    int16_t band_radius = radius / band_count;
    

    // Draw 7 vertical radial lines (180°, 200°, 220°, ..., 360°)
    static lv_point_precise_t points[10][2];
    for (uint8_t i = 0; i < line_count; i++) {
        
        // Calculate angle in degrees (180 to 360)
        float angle_deg = (180.0f / (line_count - 1)) * i;

        // Convert angle to radians (LVGL uses standard mathematical angle system)
        // Note: LVGL arc angles: 0° is right, 90° is down, 180° is left, 270° is up
        // For radar: 180° is left, 270° is up, 360° is right (top half sweep)
        float angle_rad = (angle_deg * M_PI) / 180.0f;

        // Calculate end point of the line on the semi-circle
        int16_t end_x = center_x + (int16_t)(radius * cosf(angle_rad));
        int16_t end_y = center_y - (int16_t)(radius * sinf(angle_rad));

        // Create a line object for each radial line
        lv_obj_t *line = lv_line_create(parent);
                
        // Define line points (from center to radius)
        points[i][0].x = center_x;
        points[i][0].y = center_y;
        points[i][1].x = end_x;
        points[i][1].y = end_y;
        lv_line_set_points(line, points[i], 2);
        lv_obj_add_style(line, &style_line, 0);
    }

    // Draw horizontal reference arcs at band boundaries
    static lv_style_t style_band_arc;
    lv_style_init(&style_band_arc);
    lv_style_set_arc_width(&style_band_arc, 1);
    lv_style_set_arc_color(&style_band_arc, lv_color_hex(0x4080FF)); // , lv_color_hex(0x2060CC));  // Darker blue

    for (uint8_t band = 1; band <= band_count; band++) {
        int16_t current_arc_radius = band_radius * band;

        // Draw semi-circular arc at each band level
        lv_obj_t *h_arc = lv_arc_create(parent);
        lv_obj_set_size(h_arc, current_arc_radius * 2, current_arc_radius * 2);
        lv_obj_set_pos(h_arc, center_x - current_arc_radius, center_y - current_arc_radius);
        lv_arc_set_range(h_arc, 180, 360);  // Semi-circle: 180 to 360 degrees (top half)
        lv_arc_set_bg_angles(h_arc, 180, 360);
        lv_arc_set_value(h_arc, 180);
        lv_obj_remove_style(h_arc, NULL, LV_PART_KNOB);  // Remove the knob
        lv_obj_add_style(h_arc, &style_band_arc, 0);

        // Remove fill
        lv_obj_set_style_bg_opa(h_arc, LV_OPA_TRANSP, 0);
    }
}

/**
 * @brief Update the radar sweep line with trailing shadow effect
 * 
 * @param sweep Pointer to the radar sweep structure
 * @param angle The current sweep angle (0-180 degrees)
 */
void lv_radar_sweep_update(lv_radar_sweep_t *sweep, uint16_t angle) {
    if (angle > 180) angle = 180;
    
    sweep->current_angle = angle;
    
    // Convert angle to radians
    float angle_rad = (angle * M_PI) / 180.0f;
    
    // Calculate sweep line end point
    int16_t end_x = sweep->center_x + (int16_t)(sweep->radius * cosf(angle_rad));
    int16_t end_y = sweep->center_y - (int16_t)(sweep->radius * sinf(angle_rad));
    
    // Update or create main sweep line
    if (sweep->sweep_line == NULL) {
        sweep->sweep_line = lv_line_create(sweep->parent);
        static lv_style_t style_sweep;
        lv_style_init(&style_sweep);
        lv_style_set_line_width(&style_sweep, 3);
        lv_style_set_line_color(&style_sweep, lv_color_hex(0x00FF00));  // Bright green
        lv_style_set_line_rounded(&style_sweep, true);
        lv_obj_add_style(sweep->sweep_line, &style_sweep, 0);
    }
    
    static lv_point_precise_t sweep_points[2];
    sweep_points[0].x = sweep->center_x;
    sweep_points[0].y = sweep->center_y;
    sweep_points[1].x = end_x;
    sweep_points[1].y = end_y;
    lv_line_set_points(sweep->sweep_line, sweep_points, 2);
    
    // Update trailing shadow lines (5 shadow trails with decreasing opacity)
    int16_t shadow_count = 5;
    for (int16_t i = 0; i < shadow_count; i++) {
        float shadow_angle = angle - ((i + 1) * 8);  // 8-degree increments for shadow
        if (shadow_angle < 0) shadow_angle = 0;
        
        float shadow_rad = (shadow_angle * M_PI) / 180.0f;
        int16_t shadow_end_x = sweep->center_x + (int16_t)(sweep->radius * cosf(shadow_rad));
        int16_t shadow_end_y = sweep->center_y - (int16_t)(sweep->radius * sinf(shadow_rad));
        
        // Create shadow line if it doesn't exist
        if (sweep->shadow_lines[i] == NULL) {
            sweep->shadow_lines[i] = lv_line_create(sweep->parent);
            static lv_style_t style_shadow;
            lv_style_init(&style_shadow);
            lv_style_set_line_width(&style_shadow, 2);
            lv_style_set_line_color(&style_shadow, lv_color_hex(0x00AA00));  // Darker green
            lv_style_set_line_rounded(&style_shadow, true);
            lv_obj_add_style(sweep->shadow_lines[i], &style_shadow, 0);
        }
        
        // Set opacity based on shadow distance (fade effect)
        uint8_t opacity = 255 - (i * 50);  // Decreasing opacity
        lv_obj_set_style_line_opa(sweep->shadow_lines[i], opacity, 0);

        static lv_point_precise_t shadow_points[2];
        shadow_points[0].x = sweep->center_x;
        shadow_points[0].y = sweep->center_y;
        shadow_points[1].x = shadow_end_x;
        shadow_points[1].y = shadow_end_y;
        lv_line_set_points(sweep->shadow_lines[i], shadow_points, 2);
    }
}

/**
 * @brief Animation callback for radar sweep
 *
 * @param var Pointer to the animated variable
 * @param value The current animated value (0-180 for angle)
 */
static void lv_radar_sweep_anim_cb(void *var, int32_t value) {
    lv_radar_sweep_t *sweep = (lv_radar_sweep_t *)var;
    lv_radar_sweep_update(sweep, (uint16_t)value);
}

/**
 * @brief Create a radar sweep object with trailing shadow
 * 
 * @param parent The parent LVGL object (should be the radar container)
 * @param center_x The X coordinate of the radar center
 * @param center_y The Y coordinate of the radar center
 * @param radius The radius of the semi-circle
 * @param duration_ms Duration of one complete sweep in milliseconds
 * @param loop Whether to loop the animation
 * @return Pointer to the created sweep structure (must be freed by user)
 */
lv_radar_sweep_t *lv_radar_sweep_create(lv_obj_t *parent,  uint32_t duration_ms, bool loop) {
    // Allocate sweep structure
    lv_radar_sweep_t *sweep = (lv_radar_sweep_t *)malloc(sizeof(lv_radar_sweep_t));
    if (sweep == NULL) return NULL;
    
    sweep->parent = parent;
    sweep->center_x = center_x;
    sweep->center_y = center_y;
    sweep->radius = radius;
    sweep->current_angle = 0;
    sweep->sweep_line = NULL;
    for (int i = 0; i < 5; i++) {
        sweep->shadow_lines[i] = NULL;
    }
    
    // Initialize the sweep line at 0 degrees
    lv_radar_sweep_update(sweep, 0);
    
    // Create animation for the sweep
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_exec_cb(&anim, lv_radar_sweep_anim_cb);
    lv_anim_set_var(&anim, sweep);
    lv_anim_set_values(&anim, 0, 180);
    lv_anim_set_duration(&anim, duration_ms);
    lv_anim_set_playback_duration(&anim, duration_ms);  // Enable back-and-forth motion
    lv_anim_set_repeat_delay(&anim, 500);  // 500ms delay before next sweep

    if (loop) {
        lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
    }

    lv_anim_start(&anim);
    
    return sweep;
}

/**
 * @brief Delete a radar sweep object
 * 
 * @param sweep Pointer to the sweep structure to delete
 */
void lv_radar_sweep_delete(lv_radar_sweep_t *sweep) {
    if (sweep == NULL) return;
    
    if (sweep->sweep_line != NULL) {
        lv_obj_del(sweep->sweep_line);
    }
    
    for (int i = 0; i < 5; i++) {
        if (sweep->shadow_lines[i] != NULL) {
            lv_obj_del(sweep->shadow_lines[i]);
        }
    }
    
    free(sweep);
}

/**
 * @brief Add person markers to the radar screen
 * 
 * @param parent The parent LVGL object (radar container)
 * @param center_x The X coordinate of the radar center
 * @param center_y The Y coordinate of the radar center
 * @param radius The radius of the semi-circle in pixels
 * @param band_count Number of horizontal semi-circular arches (typically 4)
 * @param markers Array of marker structures
 * @param marker_count Number of markers to place
 */
void lv_radar_add_markers(lv_obj_t *parent, uint8_t band_count, lv_radar_marker_t *markers, uint8_t marker_count) {
    // Calculate meters per pixel
    float total_meters = band_count * 2.0f;  // 4 arches * 2 meters each = 8 meters
    float meters_per_pixel = total_meters / radius;
    
    // Create style for person icons
    static lv_style_t style_marker;
    lv_style_init(&style_marker);
    lv_style_set_text_color(&style_marker, lv_color_hex(0xFFFF00));  // Yellow
    lv_style_set_text_font(&style_marker, &lv_font_montserrat_16);
    
    for (uint8_t i = 0; i < marker_count; i++) {
        lv_radar_marker_t *marker = &markers[i];
        
        // Convert distance to pixels
        float pixel_distance = marker->distance / meters_per_pixel;
        
        // Convert angle to radians
        float angle_rad = (marker->angle * M_PI) / 180.0f;
        
        // Calculate position
        int16_t marker_x = center_x + (int16_t)(pixel_distance * cosf(angle_rad));
        int16_t marker_y = center_y - (int16_t)(pixel_distance * sinf(angle_rad));
        
        // Create person icon (using Unicode person symbol)
        lv_obj_t *icon = lv_label_create(parent);
        lv_label_set_text(icon, "👤");  // Person emoji
        lv_obj_add_style(icon, &style_marker, 0);
        
        // Position the icon (center it on the calculated position)
        lv_obj_set_pos(icon, marker_x - 8, marker_y - 8);  // 16x16 icon, so offset by half
        
        // Store reference to icon in marker structure
        marker->icon = icon;
    }
}

/**
 * @brief Update marker positions (useful if radar parameters change)
 * 
 * @param center_x The X coordinate of the radar center
 * @param center_y The Y coordinate of the radar center
 * @param radius The radius of the semi-circle in pixels
 * @param band_count Number of horizontal semi-circular arches
 * @param markers Array of marker structures
 * @param marker_count Number of markers
 */
void lv_radar_update_markers(uint8_t band_count, lv_radar_marker_t *markers, uint8_t marker_count)
{
    // Calculate meters per pixel
    float total_meters = band_count * 2.0f;
    float meters_per_pixel = total_meters / radius;
    
    for (uint8_t i = 0; i < marker_count; i++) {
        lv_radar_marker_t *marker = &markers[i];
        
        // Convert distance to pixels
        float pixel_distance = marker->distance / meters_per_pixel;
        
        // Convert angle to radians
        float angle_rad = (marker->angle * M_PI) / 180.0f;
        
        // Calculate position
        int16_t marker_x = center_x + (int16_t)(pixel_distance * cosf(angle_rad));
        int16_t marker_y = center_y - (int16_t)(pixel_distance * sinf(angle_rad));
        
        // Update position
        lv_obj_set_pos(marker->icon, marker_x - 8, marker_y - 8);
    }
}

/**
 * @brief Remove all radar markers
 * 
 * @param markers Array of marker structures
 * @param marker_count Number of markers
 */
void lv_radar_remove_markers(lv_radar_marker_t *markers, uint8_t marker_count)
{
    for (uint8_t i = 0; i < marker_count; i++) {
        if (markers[i].icon != NULL) {
            lv_obj_del(markers[i].icon);
            markers[i].icon = NULL;
        }
    }
}

/**
 * @brief Create a complete radar screen widget
 * 
 * @param parent The parent LVGL object
 * @param width Width of the radar screen
 * @param height Height of the radar screen
 * @return Pointer to the created container object
 */
lv_obj_t *lv_radar_screen_create(lv_obj_t *parent, int16_t width, int16_t height)
{
    // Create a container for the radar
    lv_obj_t *radar_cont = lv_obj_create(parent);
    lv_obj_set_size(radar_cont, width, height);
    lv_obj_set_style_bg_color(radar_cont, lv_color_black(), 0);
    lv_obj_set_style_border_width(radar_cont, 0, 0);
    lv_obj_set_style_pad_all(radar_cont, 0, 0);

    // Calculate center and radius
    center_x = width / 2;
    center_y = height;  // Center at bottom for upward-facing semi-circle
    radius = height - 10;  // Leave some padding

    // Draw the radar grid
    lv_radar_screen_draw(radar_cont, 4, 9);

    return radar_cont;
}

void lv_radar_panel_init( int16_t xRes, int16_t yRes) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_screen_load(scr);
    lv_obj_clean(scr);

    // Draw radar screen
    lv_obj_t *radar = lv_radar_screen_create(scr, xRes, yRes);

    lv_radar_sweep_create(radar, 4000, true);

    // Add person markers
    lv_radar_marker_t markers[2] = {
        {.distance = 2.5f, .angle = 45, .icon = NULL}, // 2.5 meters at 45 degrees
        {.distance = 4.5f, .angle = 120, .icon = NULL} // 4.5 meters at 120 degrees
    };
    lv_radar_add_markers(radar, 4, markers, 2);
}
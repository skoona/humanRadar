#include "lvgl.h"
#include <math.h>

/**
 * @brief Draw a semi-circle radar grid with 4 horizontal bands and 9 vertical lines
 * 
 * The radar screen consists of:
 * - A semi-circle arc (180 degrees)
 * - 4 horizontal bands representing 2 meter divisions
 * - 9 vertical radial lines representing 20-degree increments (0°, 20°, 40°, ..., 180°)
 * 
 * @param parent The parent LVGL object to draw on
 * @param center_x The X coordinate of the radar center
 * @param center_y The Y coordinate of the radar center
 * @param radius The radius of the semi-circle in pixels
 * @param band_count Number of horizontal bands (typically 4)
 * @param line_count Number of vertical radial lines (typically 9)
 */
void lv_radar_screen_draw(lv_obj_t *parent, int16_t center_x, int16_t center_y, 
                          int16_t radius, uint8_t band_count, uint8_t line_count)
{
    // Create style for radar lines
    static lv_style_t style_line;
    lv_style_init(&style_line);
    lv_style_set_line_width(&style_line, 2);
    lv_style_set_line_color(&style_line, lv_color_hex(0x4080FF));  // Light blue
    lv_style_set_line_rounded(&style_line, false);

    // Create style for outer arc
    static lv_style_t style_arc;
    lv_style_init(&style_arc);
    lv_style_set_arc_width(&style_arc, 2);
    lv_style_set_arc_color(&style_arc, lv_color_hex(0x4080FF));  // Light blue

    // Draw semi-circle arc using multiple arcs for each band
    int16_t band_radius = radius / band_count;
    
    for (uint8_t band = 1; band <= band_count; band++) {
        int16_t current_radius = band_radius * band;
        
        // Create an arc object for this band
        lv_obj_t *arc = lv_arc_create(parent);
        lv_obj_set_size(arc, current_radius * 2, current_radius * 2);
        lv_obj_set_pos(arc, center_x - current_radius, center_y - current_radius);
        lv_arc_set_range(arc, 0, 180);  // Semi-circle: 0 to 180 degrees
        lv_arc_set_bg_angles(arc, 0, 180);
        lv_arc_set_value(arc, 180);
        lv_obj_remove_style(arc, NULL, LV_PART_KNOB);  // Remove the knob
        lv_obj_add_style(arc, &style_arc, 0);
        
        // Remove fill
        lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, 0);
    }

    // Draw 9 vertical radial lines (0°, 20°, 40°, ..., 180°)
    for (uint8_t i = 0; i < line_count; i++) {
        // Calculate angle in degrees (0 to 180)
        float angle_deg = (180.0f / (line_count - 1)) * i;
        
        // Convert angle to radians (LVGL uses standard mathematical angle system)
        // Note: LVGL arc angles: 0° is right, 90° is top, 180° is left
        // For radar: 0° is right, 90° is up, 180° is left (same system)
        float angle_rad = (angle_deg * M_PI) / 180.0f;
        
        // Calculate end point of the line on the semi-circle
        int16_t end_x = center_x + (int16_t)(radius * cosf(angle_rad));
        int16_t end_y = center_y - (int16_t)(radius * sinf(angle_rad));
        
        // Create a line object for each radial line
        lv_obj_t *line = lv_line_create(parent);
        
        // Define line points (from center to radius)
        static lv_point_t points[2];
        points[0].x = center_x;
        points[0].y = center_y;
        points[1].x = end_x;
        points[1].y = end_y;
        
        lv_line_set_points(line, points, 2);
        lv_obj_add_style(line, &style_line, 0);
    }

    // Draw horizontal reference lines at band boundaries
    static lv_style_t style_band_line;
    lv_style_init(&style_band_line);
    lv_style_set_line_width(&style_band_line, 1);
    lv_style_set_line_color(&style_band_line, lv_color_hex(0x2060CC));  // Darker blue
    lv_style_set_line_rounded(&style_band_line, false);

    for (uint8_t band = 1; band < band_count; band++) {
        int16_t y_pos = center_y - (band_radius * band);
        
        // Draw horizontal line at each band level
        lv_obj_t *h_line = lv_line_create(parent);
        
        static lv_point_t h_points[2];
        h_points[0].x = center_x - (band_radius * band);
        h_points[0].y = y_pos;
        h_points[1].x = center_x + (band_radius * band);
        h_points[1].y = y_pos;
        
        lv_line_set_points(h_line, h_points, 2);
        lv_obj_add_style(h_line, &style_band_line, 0);
    }
}

/**
 * @brief Structure to hold radar sweep state
 */
typedef struct {
    lv_obj_t *parent;
    int16_t center_x;
    int16_t center_y;
    int16_t radius;
    uint16_t current_angle;
    lv_obj_t *sweep_line;
    lv_obj_t *shadow_lines[5];  // Array for trailing shadow lines
} lv_radar_sweep_t;

/**
 * @brief Update the radar sweep line with trailing shadow effect
 * 
 * @param sweep Pointer to the radar sweep structure
 * @param angle The current sweep angle (0-180 degrees)
 */
void lv_radar_sweep_update(lv_radar_sweep_t *sweep, uint16_t angle)
{
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
        lv_style_t style_sweep;
        lv_style_init(&style_sweep);
        lv_style_set_line_width(&style_sweep, 3);
        lv_style_set_line_color(&style_sweep, lv_color_hex(0x00FF00));  // Bright green
        lv_style_set_line_rounded(&style_sweep, true);
        lv_obj_add_style(sweep->sweep_line, &style_sweep, 0);
    }
    
    static lv_point_t sweep_points[2];
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
            lv_style_t style_shadow;
            lv_style_init(&style_shadow);
            lv_style_set_line_width(&style_shadow, 2);
            lv_style_set_line_color(&style_shadow, lv_color_hex(0x00AA00));  // Darker green
            lv_style_set_line_rounded(&style_shadow, true);
            lv_obj_add_style(sweep->shadow_lines[i], &style_shadow, 0);
        }
        
        // Set opacity based on shadow distance (fade effect)
        uint8_t opacity = 255 - (i * 50);  // Decreasing opacity
        lv_obj_set_style_line_opa(sweep->shadow_lines[i], opacity, 0);
        
        static lv_point_t shadow_points[2];
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
 * @param anim Pointer to the animation object
 * @param value The animated value (0-180 for angle)
 */
static void lv_radar_sweep_anim_cb(lv_anim_t *anim)
{
    lv_radar_sweep_t *sweep = (lv_radar_sweep_t *)anim->user_data;
    lv_radar_sweep_update(sweep, (uint16_t)anim->var->value);
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
lv_radar_sweep_t *lv_radar_sweep_create(lv_obj_t *parent, int16_t center_x, 
                                        int16_t center_y, int16_t radius,
                                        uint32_t duration_ms, bool loop)
{
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
    lv_anim_set_var(&anim, &sweep->current_angle);
    lv_anim_set_values(&anim, 0, 180);
    lv_anim_set_time(&anim, duration_ms);
    lv_anim_set_playback_time(&anim, 0);
    lv_anim_set_repeat_delay(&anim, 500);  // 500ms delay before next sweep
    
    if (loop) {
        lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
    }
    
    lv_anim_set_exec_cb(&anim, lv_radar_sweep_anim_cb);
    lv_anim_set_user_data(&anim, sweep);
    lv_anim_start(&anim);
    
    return sweep;
}

/**
 * @brief Delete a radar sweep object
 * 
 * @param sweep Pointer to the sweep structure to delete
 */
void lv_radar_sweep_delete(lv_radar_sweep_t *sweep)
{
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
    int16_t center_x = width / 2;
    int16_t center_y = height;  // Center at bottom for upward-facing semi-circle
    int16_t radius = height - 10;  // Leave some padding

    // Draw the radar grid
    lv_radar_screen_draw(radar_cont, center_x, center_y, radius, 4, 9);

    return radar_cont;
}
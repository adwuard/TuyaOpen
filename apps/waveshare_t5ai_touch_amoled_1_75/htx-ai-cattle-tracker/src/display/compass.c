/**
 * Simple 480x480 compass-like gauge using LVGL canvas and draw APIs.
 * Not a full replica; just outer ring ticks, cardinal labels, and a red north marker.
 */

#include "lvgl/lvgl.h"
#include "tal_api.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void draw_compass(lv_obj_t *canvas, float heading)
{
    /* Use fixed size since canvas size may not be updated immediately after creation */
    const int32_t w = 466;
    const int32_t h = 466;
    const int32_t cx = w / 2;
    const int32_t cy = h / 2;
    const int32_t outer_r = 194; /* (466 * 200 / 480) ≈ 194 */
    const int32_t tick_long = 14;
    const int32_t tick_mid = 8;
    const int32_t tick_short = 4;

    // PR_DEBUG("draw_compass: canvas=466x466 (fixed), cx=%d, cy=%d, outer_r=%d, heading=%.1f", 
            //  cx, cy, outer_r, heading);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    /* Draw outer circle (full arc) */
    lv_draw_arc_dsc_t arc_dsc;
    lv_draw_arc_dsc_init(&arc_dsc);
    arc_dsc.color = lv_color_white();
    arc_dsc.width = 2;
    arc_dsc.center.x = cx;
    arc_dsc.center.y = cy;
    arc_dsc.radius = outer_r;
    arc_dsc.start_angle = 0;
    arc_dsc.end_angle = 360;
    lv_draw_arc(&layer, &arc_dsc);

    /* Ticks: draw every 10 degrees for balanced detail and performance */
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = lv_color_hex(0xB0B0B0);
    line_dsc.width = 2;
    for(int deg = 0; deg < 360; deg += 10){  /* Draw every 10 degrees (36 ticks) */
        int len = tick_short;
        if(deg % 90 == 0) len = tick_long;      /* Long ticks for cardinal directions */
        else if(deg % 30 == 0) len = tick_mid;  /* Medium ticks for 30° intervals */

        /* 0° at top, clockwise, with heading offset */
        float a = (float)(90 - deg + heading) * (float)M_PI / 180.0f;
        float s = sinf(a);
        float c = cosf(a);
        lv_draw_line_dsc_t ld = line_dsc; /* copy */
        ld.p1.x = (lv_coord_t)(cx + c * (outer_r - len));
        ld.p1.y = (lv_coord_t)(cy - s * (outer_r - len));
        ld.p2.x = (lv_coord_t)(cx + c * (outer_r + 2));
        ld.p2.y = (lv_coord_t)(cy - s * (outer_r + 2));
        lv_draw_line(&layer, &ld);
    }

    /* Degree labels every 30 degrees (0, 30, 60, 90, 120, 150, 180, 210, 240, 270, 300, 330) */
    lv_draw_label_dsc_t lbl_dsc;
    lv_draw_label_dsc_init(&lbl_dsc);
    lbl_dsc.color = lv_color_hex(0xC8C8C8);  /* Unified color for all labels */
    lbl_dsc.align = LV_TEXT_ALIGN_CENTER;

    /* Static text buffers for each degree label */
    static const char* degree_texts[] = {"N", "30", "60", "E", "120", "150", "S", "210", "240", "W", "300", "330"};
    int text_idx = 0;

    for(int deg = 0; deg < 360; deg += 30){
        /* Use larger font for cardinal directions (N/E/S/W) for emphasis */
        if (deg % 90 == 0) {
            lbl_dsc.font = &lv_font_montserrat_16;  /* Larger bold font for N/E/S/W */
        } else {
            lbl_dsc.font = &lv_font_montserrat_14;  /* Smaller font for degree numbers */
        }
        
        /* 0° at top, clockwise, with heading offset */
        float a = (float)(90 - deg + heading) * (float)M_PI / 180.0f;
        float s = sinf(a);
        float c = cosf(a);
        int r = outer_r - 26;
        lv_area_t area;
        int x = (int)(cx + c * r);
        int y = (int)(cy - s * r);
        /* place label centered; give wider box to avoid wrapping */
        area.x1 = x - 20; area.y1 = y - 10; area.x2 = x + 20; area.y2 = y + 10;
        lbl_dsc.text = degree_texts[text_idx++];
        lv_draw_label(&layer, &lbl_dsc, &area);
    }


    /* Red north marker: simple triangular outline using 3 lines */
    lv_draw_line_dsc_t red;
    lv_draw_line_dsc_init(&red);
    red.color = lv_color_hex(0xFF3B30);
    red.width = 3;

    /* Red solid triangle should always point to "N" (North) position */
    /* Use same calculation as N label position */
    float north_angle = (90.0f - 0.0f + heading) * (float)M_PI / 180.0f;
    float s = sinf(north_angle);
    float c = cosf(north_angle);

    /* Calculate triangle vertices directly in global coordinates */
    /* Triangle points along the radial direction */
    int tri_r_tip = outer_r + 20;   /* tip of triangle (furthest out) */
    int tri_r_base = outer_r + 4;   /* base of triangle */
    int tri_width = 8;               /* half width of triangle base */

    /* Tip point (pointing outward along radius) */
    int16_t xt = (int16_t)(cx + c * tri_r_tip);
    int16_t yt = (int16_t)(cy - s * tri_r_tip);

    /* Base left point (perpendicular to radius) */
    int16_t xl = (int16_t)(cx + c * tri_r_base + s * tri_width);
    int16_t yl = (int16_t)(cy - s * tri_r_base + c * tri_width);

    /* Base right point (perpendicular to radius) */
    int16_t xr = (int16_t)(cx + c * tri_r_base - s * tri_width);
    int16_t yr = (int16_t)(cy - s * tri_r_base - c * tri_width);

    /* Draw filled triangle */
    lv_draw_triangle_dsc_t tri_dsc;
    lv_draw_triangle_dsc_init(&tri_dsc);
    tri_dsc.bg_color = lv_color_hex(0xFF3B30);
    tri_dsc.bg_opa = LV_OPA_COVER;

    tri_dsc.p[0].x = xt; tri_dsc.p[0].y = yt;
    tri_dsc.p[1].x = xl; tri_dsc.p[1].y = yl;
    tri_dsc.p[2].x = xr; tri_dsc.p[2].y = yr;

    lv_draw_triangle(&layer, &tri_dsc);

    lv_canvas_finish_layer(canvas, &layer);
}

/* Global objects for reentrant compass */
static lv_obj_t *compass_scr = NULL;
static lv_obj_t *compass_canvas = NULL;
static uint8_t *compass_canvas_buf = NULL;

void compass_main(float heading)
{
    /* Create screen and canvas only once */
    if (compass_scr == NULL) {
        compass_scr = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(compass_scr, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(compass_scr, LV_OPA_COVER, 0);

        /* Allocate canvas buffer from PSRAM only if not already allocated */
        if (compass_canvas_buf == NULL) {
            size_t buf_size = LV_CANVAS_BUF_SIZE(480, 480, 16, LV_DRAW_BUF_STRIDE_ALIGN);
            compass_canvas_buf = (uint8_t *)tal_psram_malloc(buf_size);
            if (compass_canvas_buf == NULL) {
                PR_ERR("Failed to allocate compass canvas buffer from PSRAM");
                return;
            }
        }

        /* Canvas covering full screen in RGB565 for less memory */
        compass_canvas = lv_canvas_create(compass_scr);
        lv_canvas_set_buffer(compass_canvas, compass_canvas_buf, 480, 480, LV_COLOR_FORMAT_RGB565);
        lv_canvas_fill_bg(compass_canvas, lv_color_black(), LV_OPA_COVER);
        lv_obj_align(compass_canvas, LV_ALIGN_CENTER, 0, 0);

        /* Load screen only once */
        lv_scr_load(compass_scr);
    }

    /* Clear canvas before redrawing to avoid overlapping */
    lv_canvas_fill_bg(compass_canvas, lv_color_black(), LV_OPA_COVER);

    /* Redraw compass with new heading */
    draw_compass(compass_canvas, heading);
}

/* New function to create compass canvas as a child object instead of full screen */
lv_obj_t* compass_create_canvas(lv_obj_t *parent)
{
    static lv_obj_t *canvas = NULL;
    static uint8_t *canvas_buf = NULL;
    
    if (canvas == NULL) {
        /* Allocate canvas buffer from PSRAM only if not already allocated */
        if (canvas_buf == NULL) {
            size_t buf_size = LV_CANVAS_BUF_SIZE(466, 466, 16, LV_DRAW_BUF_STRIDE_ALIGN);
            canvas_buf = (uint8_t *)tal_psram_malloc(buf_size);
            if (canvas_buf == NULL) {
                PR_ERR("Failed to allocate compass canvas buffer from PSRAM");
                return NULL;
            }
            PR_DEBUG("Compass canvas buffer allocated: %zu bytes from PSRAM", buf_size);
        }
        
        /* Create canvas as child of parent */
        canvas = lv_canvas_create(parent);
        /* Use RGB565 instead of ARGB8888 to save memory (half the size) */
        lv_canvas_set_buffer(canvas, canvas_buf, 466, 466, LV_COLOR_FORMAT_RGB565);
        lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_COVER);
        lv_obj_set_size(canvas, 466, 466);
        lv_obj_center(canvas);
        
        /* Performance optimizations - same as original image */
        lv_obj_clear_flag(canvas, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(canvas, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_opa(canvas, LV_OPA_COVER, 0);
        lv_obj_set_style_blend_mode(canvas, LV_BLEND_MODE_NORMAL, 0);
        lv_obj_set_style_radius(canvas, 0, 0);
        lv_obj_set_style_border_width(canvas, 0, 0);
        lv_obj_set_style_outline_width(canvas, 0, 0);
        lv_obj_set_style_shadow_width(canvas, 0, 0);
        
        /* Set pivot point to canvas center for rotation (466/2 = 233) */
        lv_obj_set_style_transform_pivot_x(canvas, 233, 0);
        lv_obj_set_style_transform_pivot_y(canvas, 233, 0);
        
        /* Draw initial compass at 0 degrees (fixed orientation) */
        draw_compass(canvas, 0);
        PR_DEBUG("Compass canvas created successfully (466x466)");
    }
    
    return canvas;
}

/* Function to update compass canvas with new heading */
void compass_update_canvas(lv_obj_t *canvas, float heading)
{
    if (canvas == NULL) return;
    
    /* Clear canvas before redrawing */
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_COVER);
    
    /* Redraw compass with new heading */
    draw_compass(canvas, heading);
}

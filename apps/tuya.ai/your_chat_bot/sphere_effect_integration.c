/**
 * @file sphere_effect_integration.c
 * @brief Sphere effect integration for chat bot - maps AI audio states to visual sphere animations
 * @version 0.1
 * @date 2025-01-XX
 */

#include "sphere_effect_integration.h"
#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_memory.h"
#include "tkl_mutex.h"
#include "board_com_api.h"
#include "tdl_audio_manage.h"
#include "tuya_ringbuf.h"
#include "ai_audio_input.h"
#include "ai_audio_player.h"

#if defined(ENABLE_SPI) && (ENABLE_SPI) && defined(ENABLE_LEDS_PIXEL) && (ENABLE_LEDS_PIXEL)
#include "tdl_pixel_dev_manage.h"
#include "tdl_pixel_color_manage.h"
#endif

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/***********************************************************
************************macro define************************
***********************************************************/

#if defined(ENABLE_SPI) && (ENABLE_SPI) && defined(ENABLE_LEDS_PIXEL) && (ENABLE_LEDS_PIXEL)
#define LED_PIXELS_TOTAL_NUM 1024 + 3
#define COLOR_RESOLUTION     1000u
#define BRIGHTNESS           0.1f
#define MATRIX_WIDTH         32
#define MATRIX_HEIGHT        32
#endif

// 3D space configuration
#define SPACE_SIZE      32
#define SPHERE_RADIUS   16.0f
#define SPHERE_CENTER_X 16.0f
#define SPHERE_CENTER_Y 16.0f
#define SPHERE_CENTER_Z 16.0f

// Audio configuration
#define SAMPLE_RATE        16000
#define CHANNELS           1
#define BYTES_PER_SAMPLE   2 // 16-bit PCM
#define FRAME_SIZE_MS      10
#define FRAME_SIZE_BYTES   (SAMPLE_RATE * CHANNELS * BYTES_PER_SAMPLE * FRAME_SIZE_MS / 1000) // 320 bytes
#define AUDIO_RINGBUF_SIZE (FRAME_SIZE_BYTES * 32)

#ifndef AUDIO_CODEC_NAME
#define AUDIO_CODEC_NAME "audio"
#endif

// Audio power calculation
#define AUDIO_BUFFER_SIZE   160
#define POWER_NORMALIZATION 50000.0f

/***********************************************************
***********************variable define**********************
***********************************************************/

#if defined(ENABLE_SPI) && (ENABLE_SPI) && defined(ENABLE_LEDS_PIXEL) && (ENABLE_LEDS_PIXEL)
static PIXEL_HANDLE_T g_pixels_handle = NULL;
#endif

// Audio processing - separate buffers for input and output
static TUYA_RINGBUFF_T g_audio_input_ringbuf = NULL;
static TUYA_RINGBUFF_T g_audio_output_ringbuf = NULL;
static MUTEX_HANDLE g_audio_input_rb_mutex = NULL;
static MUTEX_HANDLE g_audio_output_rb_mutex = NULL;
static MUTEX_HANDLE g_audio_power_mutex = NULL;

static int16_t g_audio_input_buffer[AUDIO_BUFFER_SIZE];
static int16_t g_audio_output_buffer[AUDIO_BUFFER_SIZE];
static float g_audio_input_power = 0.0f;  // Audio power from input (mic)
static float g_audio_output_power = 0.0f; // Audio power from output (speaker)

// Double buffering for smooth rendering
#define NUM_BUFFERS 2
static struct {
    uint8_t r[MATRIX_WIDTH][MATRIX_HEIGHT];
    uint8_t g[MATRIX_WIDTH][MATRIX_HEIGHT];
    uint8_t b[MATRIX_WIDTH][MATRIX_HEIGHT];
    bool ready;
} g_render_buffers[NUM_BUFFERS];

static int g_front_buffer = 0;
static int g_back_buffer = 1;
static MUTEX_HANDLE g_buffer_mutex = NULL;

// Voice interaction state machine (mapped from AI audio states)
typedef enum {
    VOICE_STATE_IDLE = 0,
    VOICE_STATE_START,
    VOICE_STATE_PROCESSING,
    VOICE_STATE_RESPONDING,
    VOICE_STATE_TRANSITION_TO_IDLE
} voice_state_t;

static voice_state_t g_voice_state = VOICE_STATE_IDLE;
static MUTEX_HANDLE g_voice_state_mutex = NULL;
static uint64_t g_state_transition_start_time = 0;
static float g_idle_circle_radius = 2.5f;
static float g_target_animation_radius = SPHERE_RADIUS;
static float g_current_radius = 2.5f;
static float g_running_ring_angle = 0.0f;
static float g_running_ring_angle2 = 0.0f;
static float g_start_state_peak_radius = 2.5f;

// Audio-reactive effects
static float g_audio_power_smoothed = 0.0f;
static float g_sphere_breath = 0.0f;
static float g_hue_shift = 0.0f;
static float g_random_hue_offset = 0.0f;
static uint32_t g_last_random_update = 0;

// Brightness decay/hold for smooth animation
static float g_effective_audio_power = 0.0f;  // The power used for rendering (with decay)
static float g_peak_audio_power = 0.0f;       // Peak power to hold
static uint64_t g_last_audio_update_time = 0; // Last time audio power was updated
#define AUDIO_POWER_HOLD_TIME_MS 100          // Hold peak power for this duration
#define AUDIO_POWER_DECAY_RATE   0.95f        // Decay rate per frame (0.95 = 5% decay)

// Audio output handle for processing state
static TDL_AUDIO_HANDLE_T g_audio_output_handle = NULL;

/***********************************************************
********************function declaration********************
***********************************************************/

#if defined(ENABLE_SPI) && (ENABLE_SPI) && defined(ENABLE_LEDS_PIXEL) && (ENABLE_LEDS_PIXEL)
static uint32_t __matrix_coord_to_led_index(uint32_t x, uint32_t y);
static OPERATE_RET pixel_led_init(void);
static void render_sphere_3d(int buffer_idx);
static void display_sphere_fast(void);
static float calculate_sphere_hue(float x, float y, float z, float audio_power);
static void hsv_to_rgb(float hue, float saturation, float value, uint32_t *r, uint32_t *g, uint32_t *b);
static void update_audio_reactive_effects(float audio_power);
static void render_voxel_gradient_core(int buffer_idx, float radius, float audio_power, bool mic_responsive,
                                       float white_fade_factor);
static void render_idle_state(int buffer_idx);
static void render_start_state(int buffer_idx);
static void render_processing_state(int buffer_idx);
static void render_responding_state(int buffer_idx);
static void render_transition_to_idle_state(int buffer_idx);
static void render_running_ring(int buffer_idx, float angle1, float angle2);
static void transition_to_state(voice_state_t new_state);
#endif

// Audio processing functions
static void process_audio_input_power(uint8_t *audio_data, uint32_t data_len);
static void process_audio_output_power(uint8_t *audio_data, uint32_t data_len);
static void sphere_rendering_task(void *args);
static void sphere_display_task(void *args);

// Callback functions (forward declarations)
static void sphere_audio_input_cb(uint8_t *data, uint32_t len, void *user_data);
static void sphere_audio_output_cb(int16_t *pcm_data, uint32_t samples, uint8_t channels, void *user_data);

/***********************************************************
***********************function implementation**************
***********************************************************/

#if defined(ENABLE_SPI) && (ENABLE_SPI) && defined(ENABLE_LEDS_PIXEL) && (ENABLE_LEDS_PIXEL)
/**
 * @brief Convert matrix coordinates to LED index
 */
static uint32_t __matrix_coord_to_led_index(uint32_t x, uint32_t y)
{
    if (x >= 32 || y >= 32)
        return LED_PIXELS_TOTAL_NUM;

    uint32_t led_index;
    if (y % 2 == 0) {
        led_index = y * 32 + x;
    } else {
        led_index = (y + 1) * 32 - 1 - x;
    }
    return led_index;
}

/**
 * @brief Initialize pixel LED driver
 */
static OPERATE_RET pixel_led_init(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(PIXEL_DEVICE_NAME)
    tal_system_sleep(100);

    rt = tdl_pixel_dev_find(PIXEL_DEVICE_NAME, &g_pixels_handle);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to find pixel device '%s': %d", PIXEL_DEVICE_NAME, rt);
        return rt;
    }

    if (g_pixels_handle == NULL) {
        PR_ERR("Pixel device handle is NULL after find");
        return OPRT_COM_ERROR;
    }

    PIXEL_DEV_CONFIG_T pixels_cfg = {
        .pixel_num = LED_PIXELS_TOTAL_NUM,
        .pixel_resolution = COLOR_RESOLUTION,
    };
    rt = tdl_pixel_dev_open(g_pixels_handle, &pixels_cfg);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to open pixel device: %d", rt);
        g_pixels_handle = NULL;
        return rt;
    }

    PR_NOTICE("Pixel LED initialized: %d pixels", LED_PIXELS_TOTAL_NUM);
#else
    PR_ERR("PIXEL_DEVICE_NAME not defined");
    return OPRT_INVALID_PARM;
#endif

    return rt;
}

/**
 * @brief Calculate hue for a point on the sphere surface
 */
static float calculate_sphere_hue(float x, float y, float z, float audio_power)
{
    float dx = x - SPHERE_CENTER_X;
    float dy = y - SPHERE_CENTER_Y;
    float dz = z - SPHERE_CENTER_Z;

    float azimuth = atan2f(dx, dz);
    if (azimuth < 0.0f) {
        azimuth += 2.0f * M_PI;
    }

    float dist_xz = sqrtf(dx * dx + dz * dz);
    float elevation = atan2f(dy, dist_xz);
    float elevation_normalized = (elevation + M_PI / 2.0f) / M_PI;

    float base_hue = (azimuth / (2.0f * M_PI)) * 360.0f;
    float hue = base_hue + (elevation_normalized - 0.5f) * 180.0f;

    hue += g_hue_shift * 0.5f;

    while (hue >= 360.0f)
        hue -= 360.0f;
    while (hue < 0.0f)
        hue += 360.0f;

    return hue;
}

/**
 * @brief Convert HSV to RGB
 */
static void hsv_to_rgb(float hue, float saturation, float value, uint32_t *r, uint32_t *g, uint32_t *b)
{
    while (hue < 0.0f)
        hue += 360.0f;
    while (hue >= 360.0f)
        hue -= 360.0f;

    float h = hue / 60.0f;
    float c = value * saturation;
    float x = c * (1.0f - fabsf(fmodf(h, 2.0f) - 1.0f));
    float m = value - c;

    float rf, gf, bf;
    if (h < 1.0f) {
        rf = c;
        gf = x;
        bf = 0;
    } else if (h < 2.0f) {
        rf = x;
        gf = c;
        bf = 0;
    } else if (h < 3.0f) {
        rf = 0;
        gf = c;
        bf = x;
    } else if (h < 4.0f) {
        rf = 0;
        gf = x;
        bf = c;
    } else if (h < 5.0f) {
        rf = x;
        gf = 0;
        bf = c;
    } else {
        rf = c;
        gf = 0;
        bf = x;
    }

    *r = (uint32_t)((rf + m) * 255.0f);
    *g = (uint32_t)((gf + m) * 255.0f);
    *b = (uint32_t)((bf + m) * 255.0f);
}

/**
 * @brief Update audio-reactive effects with brightness decay/hold
 */
static void update_audio_reactive_effects(float audio_power)
{
    uint64_t current_time_ms = tal_time_get_posix_ms();

    // Smooth the incoming audio power
    float alpha = 0.1f;
    g_audio_power_smoothed = alpha * audio_power + (1.0f - alpha) * g_audio_power_smoothed;

    // Brightness decay/hold mechanism
    if (g_audio_power_smoothed > g_peak_audio_power) {
        // New peak detected - update peak and hold time
        g_peak_audio_power = g_audio_power_smoothed;
        g_last_audio_update_time = current_time_ms;
        g_effective_audio_power = g_peak_audio_power;
    } else {
        // Check if we should still hold the peak
        uint64_t time_since_peak = current_time_ms - g_last_audio_update_time;
        if (time_since_peak < AUDIO_POWER_HOLD_TIME_MS) {
            // Hold the peak power
            g_effective_audio_power = g_peak_audio_power;
        } else {
            // Decay the effective power
            g_effective_audio_power *= AUDIO_POWER_DECAY_RATE;
            // Update peak to current if it's lower (for smooth decay)
            if (g_audio_power_smoothed < g_peak_audio_power) {
                g_peak_audio_power = g_audio_power_smoothed;
            }
            // Clamp to current smoothed power (don't go below current level)
            if (g_effective_audio_power < g_audio_power_smoothed) {
                g_effective_audio_power = g_audio_power_smoothed;
            }
        }
    }

    // Use effective power for breath animation
    static float breath_target = 0.0f;
    breath_target = g_effective_audio_power * 0.4f;
    float breath_alpha = 0.05f;
    g_sphere_breath = breath_alpha * breath_target + (1.0f - breath_alpha) * g_sphere_breath;

    g_hue_shift += 0.8f + g_effective_audio_power * 3.0f;
    if (g_hue_shift >= 360.0f) {
        g_hue_shift -= 360.0f;
    }

    uint32_t current_time = tal_time_get_posix();
    if (g_last_random_update == 0 || (current_time - g_last_random_update) > (5 + (current_time % 3))) {
        g_random_hue_offset = ((float)((current_time * 11) % 1000) / 1000.0f) * 360.0f;
        g_last_random_update = current_time;
    }
}

/**
 * @brief Transition to a new voice state
 */
static void transition_to_state(voice_state_t new_state)
{
    if (g_voice_state_mutex != NULL) {
        if (tal_mutex_lock(g_voice_state_mutex) == OPRT_OK) {
            g_voice_state = new_state;
            g_state_transition_start_time = tal_time_get_posix_ms();
            tal_mutex_unlock(g_voice_state_mutex);
        }
    }
}

/**
 * @brief Render voxel gradient core
 */
static void render_voxel_gradient_core(int buffer_idx, float radius, float audio_power, bool mic_responsive,
                                       float white_fade_factor)
{
    float base_radius = radius;
    if (mic_responsive) {
        base_radius = radius * (1.0f + g_sphere_breath * 0.5f);
    } else {
        base_radius = radius * (1.0f + g_sphere_breath);
    }
    float current_radius = base_radius;

    static float effect2_rotation_z = 0.0f;
    static uint32_t effect2_last_time = 0;
    uint32_t current_time = tal_time_get_posix();
    if (effect2_last_time > 0) {
        float elapsed_sec = (float)(current_time - effect2_last_time);
        if (elapsed_sec > 0.1f) {
            elapsed_sec = 0.1f;
        }
        float rotation_speed_z = 0.5f;
        effect2_rotation_z += rotation_speed_z * elapsed_sec;
        while (effect2_rotation_z >= 2.0f * M_PI)
            effect2_rotation_z -= 2.0f * M_PI;
        while (effect2_rotation_z < 0.0f)
            effect2_rotation_z += 2.0f * M_PI;
    }
    effect2_last_time = current_time;

    float cos_z = cosf(effect2_rotation_z);
    float sin_z = sinf(effect2_rotation_z);

    float radius_sq = current_radius * current_radius;

    static float temp_brightness[MATRIX_WIDTH][MATRIX_HEIGHT];
    static float temp_hue[MATRIX_WIDTH][MATRIX_HEIGHT];
    memset(temp_brightness, 0, sizeof(temp_brightness));
    memset(temp_hue, 0, sizeof(temp_hue));

    for (int z = 0; z < SPACE_SIZE; z++) {
        float z_world = (float)z;
        float dz = z_world - SPHERE_CENTER_Z;

        for (int y = 0; y < MATRIX_HEIGHT; y++) {
            for (int x = 0; x < MATRIX_WIDTH; x++) {
                float x_world = (float)x;
                float y_world = (float)y;
                float dx = x_world - SPHERE_CENTER_X;
                float dy = y_world - SPHERE_CENTER_Y;

                float rx = dx * cos_z - dy * sin_z;
                float ry = dx * sin_z + dy * cos_z;
                float rz = dz;

                float dist_sq = rx * rx + ry * ry + rz * rz;
                if (dist_sq <= radius_sq) {
                    float nz = rz / current_radius;
                    float brightness = fmaxf(0.0f, nz * 0.5f + 0.5f);

                    // Use effective audio power (with decay/hold) for smooth animation
                    float audio_intensity_boost = 0.4f + g_effective_audio_power * 1.6f;
                    brightness *= audio_intensity_boost;

                    float hue = calculate_sphere_hue(rx + SPHERE_CENTER_X, ry + SPHERE_CENTER_Y, rz + SPHERE_CENTER_Z,
                                                     g_effective_audio_power);
                    hue += g_hue_shift * 0.5f;
                    hue += g_random_hue_offset * 0.2f;

                    while (hue >= 360.0f)
                        hue -= 360.0f;
                    while (hue < 0.0f)
                        hue += 360.0f;

                    temp_brightness[x][y] += brightness;
                    if (temp_brightness[x][y] > 0.001f) {
                        float weight = brightness / (temp_brightness[x][y] + 0.001f);
                        temp_hue[x][y] = temp_hue[x][y] * (1.0f - weight) + hue * weight;
                    }
                }
            }
        }
    }

    float max_brightness = 0.0f;
    for (int y = 0; y < MATRIX_HEIGHT; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            if (temp_brightness[x][y] > max_brightness) {
                max_brightness = temp_brightness[x][y];
            }
        }
    }

    if (max_brightness > 0.001f) {
        for (int y = 0; y < MATRIX_HEIGHT; y++) {
            for (int x = 0; x < MATRIX_WIDTH; x++) {
                float brightness = temp_brightness[x][y] / max_brightness;
                if (brightness > 1.0f)
                    brightness = 1.0f;
                float hue = temp_hue[x][y];
                while (hue >= 360.0f)
                    hue -= 360.0f;
                while (hue < 0.0f)
                    hue += 360.0f;

                uint32_t r, g, b;
                float base_saturation = 0.6f + audio_power * 0.4f;
                float white_factor = (brightness > 0.7f) ? (brightness - 0.7f) * 0.5f : 0.0f;
                float saturation = base_saturation * (1.0f - white_factor);
                if (saturation < 0.2f)
                    saturation = 0.2f;

                float final_brightness;
                if (mic_responsive) {
                    final_brightness = brightness * (0.5f + audio_power * 0.5f);
                } else {
                    final_brightness = brightness * 0.7f;
                }

                if (white_fade_factor > 0.001f) {
                    hsv_to_rgb(hue, saturation, final_brightness, &r, &g, &b);
                    float white_r = 255 * final_brightness;
                    float white_g = 255 * final_brightness;
                    float white_b = 255 * final_brightness;
                    g_render_buffers[buffer_idx].r[x][y] =
                        (uint8_t)(r * (1.0f - white_fade_factor) + white_r * white_fade_factor);
                    g_render_buffers[buffer_idx].g[x][y] =
                        (uint8_t)(g * (1.0f - white_fade_factor) + white_g * white_fade_factor);
                    g_render_buffers[buffer_idx].b[x][y] =
                        (uint8_t)(b * (1.0f - white_fade_factor) + white_b * white_fade_factor);
                } else {
                    hsv_to_rgb(hue, saturation, final_brightness, &r, &g, &b);
                    g_render_buffers[buffer_idx].r[x][y] = (uint8_t)r;
                    g_render_buffers[buffer_idx].g[x][y] = (uint8_t)g;
                    g_render_buffers[buffer_idx].b[x][y] = (uint8_t)b;
                }
            }
        }
    } else {
        memset(g_render_buffers[buffer_idx].r, 0, sizeof(g_render_buffers[buffer_idx].r));
        memset(g_render_buffers[buffer_idx].g, 0, sizeof(g_render_buffers[buffer_idx].g));
        memset(g_render_buffers[buffer_idx].b, 0, sizeof(g_render_buffers[buffer_idx].b));
    }

    g_render_buffers[buffer_idx].ready = true;
}

/**
 * @brief Render PROCESSING state: voxel gradient + running ring (uses OUTPUT audio power)
 * Note: During UPLOAD state, audio output may not be available yet, so we use minimal/no audio power
 * The rings provide visual feedback that processing is happening
 */
static void render_processing_state(int buffer_idx)
{
    // Get audio OUTPUT power (cloud return/response) for processing state
    // During UPLOAD, output may not be available, so we use a minimal value
    float audio_power = ai_audio_player_get_power();

    // Render voxel gradient with output audio power (or minimal if not available)
    render_voxel_gradient_core(buffer_idx, SPHERE_RADIUS, audio_power, false, 0.0f);

    // Add two white running rings (always visible during processing)
    float ring_speed = 0.3f;
    g_running_ring_angle += ring_speed;
    if (g_running_ring_angle >= 2.0f * M_PI) {
        g_running_ring_angle -= 2.0f * M_PI;
    }

    g_running_ring_angle2 -= ring_speed * 0.8f;
    if (g_running_ring_angle2 < 0.0f) {
        g_running_ring_angle2 += 2.0f * M_PI;
    }

    render_running_ring(buffer_idx, g_running_ring_angle, g_running_ring_angle2);
}

/**
 * @brief Render RESPONDING state: voxel gradient without ring, mic responsive (uses INPUT audio power)
 */
static void render_responding_state(int buffer_idx)
{
    // Get audio OUTPUT power (speaker) for responding state - AI is speaking!
    float audio_power = ai_audio_player_get_power();

    // Render voxel gradient (speaker responsive, no white fade, no ring)
    render_voxel_gradient_core(buffer_idx, SPHERE_RADIUS, audio_power, false, 0.0f);
}

/**
 * @brief Render TRANSITION_TO_IDLE state
 */
static void render_transition_to_idle_state(int buffer_idx)
{
    float audio_power = ai_audio_input_get_power();

    uint64_t state_time_ms = tal_time_get_posix_ms();
    uint64_t transition_time_ms = 0;
    if (g_state_transition_start_time > 0) {
        transition_time_ms = state_time_ms - g_state_transition_start_time;
    }

    float target_radius = SPHERE_RADIUS;
    float transition_duration = 500.0f;
    float white_fade_factor = 0.0f;
    float current_radius = target_radius;

    if (transition_time_ms < transition_duration) {
        float t = (float)transition_time_ms / transition_duration;
        t = t * t;
        current_radius = target_radius - (target_radius - g_idle_circle_radius) * t;
        white_fade_factor = 1.0f - t;
    } else {
        current_radius = g_idle_circle_radius;
        white_fade_factor = 1.0f;
    }

    g_current_radius = current_radius;
    render_voxel_gradient_core(buffer_idx, current_radius, audio_power, false, white_fade_factor);
}

/**
 * @brief Render idle state: breathing white circle at center
 */
static void render_idle_state(int buffer_idx)
{
    memset(g_render_buffers[buffer_idx].r, 0, sizeof(g_render_buffers[buffer_idx].r));
    memset(g_render_buffers[buffer_idx].g, 0, sizeof(g_render_buffers[buffer_idx].g));
    memset(g_render_buffers[buffer_idx].b, 0, sizeof(g_render_buffers[buffer_idx].b));

    float center_x = SPHERE_CENTER_X;
    float center_y = SPHERE_CENTER_Y;

    uint32_t current_time = tal_time_get_posix();
    float pulse = 0.5f + 0.5f * sinf((float)current_time * 0.001f);

    float min_radius = 2.0f;
    float max_radius = 3.0f;
    float radius = min_radius + (max_radius - min_radius) * pulse;
    float radius_sq = radius * radius;

    float brightness = 0.6f + 0.4f * pulse;

    for (int y = 0; y < MATRIX_HEIGHT; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            float dx = (float)x - center_x;
            float dy = (float)y - center_y;
            float dist_sq = dx * dx + dy * dy;

            if (dist_sq <= radius_sq) {
                float dist = sqrtf(dist_sq);
                float normalized_dist = dist / radius;
                float edge_falloff = 1.0f - normalized_dist * 0.5f;
                if (edge_falloff < 0.0f)
                    edge_falloff = 0.0f;

                float final_brightness = brightness * edge_falloff;
                g_render_buffers[buffer_idx].r[x][y] = (uint8_t)(255 * final_brightness);
                g_render_buffers[buffer_idx].g[x][y] = (uint8_t)(255 * final_brightness);
                g_render_buffers[buffer_idx].b[x][y] = (uint8_t)(255 * final_brightness);
            }
        }
    }

    g_render_buffers[buffer_idx].ready = true;
}

/**
 * @brief Render START state: red circle that grows with audio RMS (uses INPUT audio power)
 */
static void render_start_state(int buffer_idx)
{
    // Get audio INPUT power (mic) for start state
    float audio_power = ai_audio_input_get_power();

    memset(g_render_buffers[buffer_idx].r, 0, sizeof(g_render_buffers[buffer_idx].r));
    memset(g_render_buffers[buffer_idx].g, 0, sizeof(g_render_buffers[buffer_idx].g));
    memset(g_render_buffers[buffer_idx].b, 0, sizeof(g_render_buffers[buffer_idx].b));

    float center_x = SPHERE_CENTER_X;
    float center_y = SPHERE_CENTER_Y;

    float min_radius = 2.5f;
    float max_radius = 16.0f;

    float responsive_power = 0.0f;
    if (audio_power > 0.001f) {
        responsive_power = sqrtf(audio_power);
        if (audio_power < 0.3f) {
            responsive_power = responsive_power * 2.3f;
            if (responsive_power > 0.7f)
                responsive_power = 0.7f;
        } else {
            float compressed = (audio_power - 0.3f) / 0.7f;
            responsive_power = 0.7f + compressed * 0.3f;
        }
        if (responsive_power > 1.0f)
            responsive_power = 1.0f;
        if (responsive_power < 0.0f)
            responsive_power = 0.0f;
    }

    float current_radius = min_radius + (max_radius - min_radius) * responsive_power;

    if (current_radius > g_start_state_peak_radius) {
        g_start_state_peak_radius = current_radius;
    }

    if (g_start_state_peak_radius > min_radius) {
        g_start_state_peak_radius -= 0.3f;
        if (g_start_state_peak_radius < min_radius) {
            g_start_state_peak_radius = min_radius;
        }
    }

    float display_radius = g_start_state_peak_radius;
    float radius_sq = display_radius * display_radius;
    float brightness = 0.7f + responsive_power * 0.3f;

    for (int y = 0; y < MATRIX_HEIGHT; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            float dx = (float)x - center_x;
            float dy = (float)y - center_y;
            float dist_sq = dx * dx + dy * dy;

            if (dist_sq <= radius_sq) {
                float dist = sqrtf(dist_sq);
                float normalized_dist = dist / display_radius;
                float edge_falloff = 1.0f - normalized_dist * 0.3f;
                if (edge_falloff < 0.0f)
                    edge_falloff = 0.0f;

                float final_brightness = brightness * edge_falloff;
                g_render_buffers[buffer_idx].r[x][y] = (uint8_t)(255 * final_brightness);
                g_render_buffers[buffer_idx].g[x][y] = 0;
                g_render_buffers[buffer_idx].b[x][y] = 0;
            }
        }
    }

    g_current_radius = current_radius;
    g_render_buffers[buffer_idx].ready = true;
}

/**
 * @brief Render running ring
 */
static void render_running_ring(int buffer_idx, float angle1, float angle2)
{
    float center_x = SPHERE_CENTER_X;
    float center_y = SPHERE_CENTER_Y;
    float ring_radius = 15.5f;

    for (int y = 0; y < MATRIX_HEIGHT; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            float dx = (float)x - center_x;
            float dy = (float)y - center_y;
            float dist = sqrtf(dx * dx + dy * dy);

            float brightness = 0.0f;

            if (fabsf(dist - ring_radius) < 0.7f) {
                float pixel_angle = atan2f(dy, dx);
                if (pixel_angle < 0.0f)
                    pixel_angle += 2.0f * M_PI;

                float angle_diff1 = fabsf(pixel_angle - angle1);
                if (angle_diff1 > M_PI)
                    angle_diff1 = 2.0f * M_PI - angle_diff1;

                if (angle_diff1 < 0.6f) {
                    float ring_brightness = 1.0f - (angle_diff1 / 0.6f);
                    if (ring_brightness > brightness) {
                        brightness = ring_brightness;
                    }
                }

                float angle_diff2 = fabsf(pixel_angle - angle2);
                if (angle_diff2 > M_PI)
                    angle_diff2 = 2.0f * M_PI - angle_diff2;

                if (angle_diff2 < 0.6f) {
                    float ring_brightness = 1.0f - (angle_diff2 / 0.6f);
                    if (ring_brightness > brightness) {
                        brightness = ring_brightness;
                    }
                }
            }

            if (brightness > 0.0f) {
                uint8_t existing_r = g_render_buffers[buffer_idx].r[x][y];
                uint8_t existing_g = g_render_buffers[buffer_idx].g[x][y];
                uint8_t existing_b = g_render_buffers[buffer_idx].b[x][y];

                uint8_t ring_brightness = (uint8_t)(255 * brightness);
                g_render_buffers[buffer_idx].r[x][y] =
                    (existing_r + ring_brightness > 255) ? 255 : (existing_r + ring_brightness);
                g_render_buffers[buffer_idx].g[x][y] =
                    (existing_g + ring_brightness > 255) ? 255 : (existing_g + ring_brightness);
                g_render_buffers[buffer_idx].b[x][y] =
                    (existing_b + ring_brightness > 255) ? 255 : (existing_b + ring_brightness);
            }
        }
    }
}

/**
 * @brief Render 3D sphere - routes to appropriate engine based on state
 */
static void render_sphere_3d(int buffer_idx)
{
    voice_state_t current_state;
    if (g_voice_state_mutex != NULL) {
        if (tal_mutex_lock(g_voice_state_mutex) == OPRT_OK) {
            current_state = g_voice_state;
            tal_mutex_unlock(g_voice_state_mutex);
        } else {
            current_state = VOICE_STATE_IDLE;
        }
    } else {
        current_state = VOICE_STATE_IDLE;
    }

    switch (current_state) {
    case VOICE_STATE_IDLE:
        render_idle_state(buffer_idx);
        break;
    case VOICE_STATE_START:
        render_start_state(buffer_idx);
        break;
    case VOICE_STATE_PROCESSING:
        render_processing_state(buffer_idx);
        break;
    case VOICE_STATE_RESPONDING:
        render_responding_state(buffer_idx);
        break;
    case VOICE_STATE_TRANSITION_TO_IDLE:
        render_transition_to_idle_state(buffer_idx);
        break;
    default:
        render_idle_state(buffer_idx);
        break;
    }
}

/**
 * @brief Fast display update
 */
static void display_sphere_fast(void)
{
    if (g_pixels_handle == NULL) {
        return;
    }

    if (g_buffer_mutex != NULL && tal_mutex_lock(g_buffer_mutex) == OPRT_OK) {
        if (g_render_buffers[g_back_buffer].ready) {
            int temp = g_front_buffer;
            g_front_buffer = g_back_buffer;
            g_back_buffer = temp;
            g_render_buffers[g_back_buffer].ready = false;
        }
        tal_mutex_unlock(g_buffer_mutex);
    }

    PIXEL_COLOR_T off_color = {0};
    tdl_pixel_set_single_color(g_pixels_handle, 0, LED_PIXELS_TOTAL_NUM, &off_color);

    for (int y = 0; y < MATRIX_HEIGHT; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            uint8_t r = g_render_buffers[g_front_buffer].r[x][y];
            uint8_t g_val = g_render_buffers[g_front_buffer].g[x][y];
            uint8_t b = g_render_buffers[g_front_buffer].b[x][y];

            PIXEL_COLOR_T pixel_color = {.red = (uint32_t)(g_val * COLOR_RESOLUTION * BRIGHTNESS / 255),
                                         .green = (uint32_t)(r * COLOR_RESOLUTION * BRIGHTNESS / 255),
                                         .blue = (uint32_t)(b * COLOR_RESOLUTION * BRIGHTNESS / 255),
                                         .warm = 0,
                                         .cold = 0};

            uint32_t led_index = __matrix_coord_to_led_index((uint32_t)x, (uint32_t)y);
            if (led_index < LED_PIXELS_TOTAL_NUM) {
                tdl_pixel_set_single_color(g_pixels_handle, led_index, 1, &pixel_color);
            }
        }
    }

    tdl_pixel_dev_refresh(g_pixels_handle);
}

/**
 * @brief Sphere rendering task
 */
static void sphere_rendering_task(void *args)
{
    PR_NOTICE("Sphere rendering task started");

    uint32_t g_last_update_time = tal_time_get_posix();

    while (1) {
        uint32_t current_time = tal_time_get_posix();
        if (g_last_update_time > 0) {
            float elapsed_sec = (float)(current_time - g_last_update_time);
            if (elapsed_sec > 0.1f) {
                elapsed_sec = 0.1f;
            }
            if (elapsed_sec < 0.0f) {
                elapsed_sec = 0.0f;
            }

            // Get audio power for reactive effects
            float audio_power = 0.0f;
            voice_state_t current_state = VOICE_STATE_IDLE;
            if (g_voice_state_mutex != NULL) {
                if (tal_mutex_lock(g_voice_state_mutex) == OPRT_OK) {
                    current_state = g_voice_state;
                    tal_mutex_unlock(g_voice_state_mutex);
                }
            }

            // Use appropriate audio power based on state
            // Read power from AI audio components
            if (current_state == VOICE_STATE_PROCESSING) {
                audio_power = ai_audio_player_get_power(); // Processing uses output
            } else {
                audio_power = ai_audio_input_get_power(); // Others use input
            }

            // Also update internal power values for backward compatibility
            if (g_audio_power_mutex != NULL) {
                if (tal_mutex_lock(g_audio_power_mutex) == OPRT_OK) {
                    if (current_state == VOICE_STATE_PROCESSING) {
                        g_audio_output_power = audio_power;
                    } else {
                        g_audio_input_power = audio_power;
                    }
                    tal_mutex_unlock(g_audio_power_mutex);
                }
            }

            update_audio_reactive_effects(audio_power);
        }
        g_last_update_time = current_time;

        if (!g_render_buffers[g_back_buffer].ready) {
            render_sphere_3d(g_back_buffer);
        }

        tal_system_sleep(1);
    }
}

/**
 * @brief Sphere display task
 */
static void sphere_display_task(void *args)
{
    PR_NOTICE("Sphere display task started");

    while (1) {
#if defined(ENABLE_SPI) && (ENABLE_SPI) && defined(ENABLE_LEDS_PIXEL) && (ENABLE_LEDS_PIXEL)
        uint32_t frame_start = tal_time_get_posix();

        display_sphere_fast();

        uint32_t frame_time = tal_time_get_posix() - frame_start;
        uint32_t target_frame_time = 20; // ~50 FPS

        if (frame_time < target_frame_time) {
            tal_system_sleep(target_frame_time - frame_time);
        } else {
            tal_system_sleep(1);
        }
#else
        tal_system_sleep(20);
#endif
    }
}
#endif

/**
 * @brief Callback for audio input data from ai_audio_input
 */
static void sphere_audio_input_cb(uint8_t *data, uint32_t len, void *user_data)
{
    if (data == NULL || len == 0) {
        return;
    }
    process_audio_input_power(data, len);
}

/**
 * @brief Callback for audio output data from ai_audio_player
 */
static void sphere_audio_output_cb(int16_t *pcm_data, uint32_t samples, uint8_t channels, void *user_data)
{
    if (pcm_data == NULL || samples == 0) {
        return;
    }
    // Convert samples to bytes (2 bytes per sample for int16_t)
    uint32_t data_len = samples * BYTES_PER_SAMPLE;
    process_audio_output_power((uint8_t *)pcm_data, data_len);
}

/**
 * @brief Process audio input data and calculate power
 */
static void process_audio_input_power(uint8_t *audio_data, uint32_t data_len)
{
    uint32_t num_samples = data_len / BYTES_PER_SAMPLE;
    if (num_samples > AUDIO_BUFFER_SIZE) {
        num_samples = AUDIO_BUFFER_SIZE;
    }

    int16_t *pcm_samples = (int16_t *)audio_data;

    if (num_samples >= AUDIO_BUFFER_SIZE) {
        memcpy(g_audio_input_buffer, pcm_samples, AUDIO_BUFFER_SIZE * sizeof(int16_t));
    } else {
        memmove(g_audio_input_buffer, &g_audio_input_buffer[num_samples],
                (AUDIO_BUFFER_SIZE - num_samples) * sizeof(int16_t));
        memcpy(&g_audio_input_buffer[AUDIO_BUFFER_SIZE - num_samples], pcm_samples, num_samples * sizeof(int16_t));
    }

    float sum_squares = 0.0f;
    int valid_samples = 0;

    for (int i = 0; i < AUDIO_BUFFER_SIZE; i++) {
        float sample = (float)g_audio_input_buffer[i];
        sum_squares += sample * sample;
        valid_samples++;
    }

    float rms = 0.0f;
    if (valid_samples > 0) {
        rms = sqrtf(sum_squares / (float)valid_samples);
    }

    float normalized_power = rms / POWER_NORMALIZATION;
    if (normalized_power > 1.0f)
        normalized_power = 1.0f;
    if (normalized_power < 0.0f)
        normalized_power = 0.0f;

    normalized_power = log10f(1.0f + normalized_power * 9.0f);

    if (g_audio_power_mutex != NULL) {
        if (tal_mutex_lock(g_audio_power_mutex) == OPRT_OK) {
            g_audio_input_power = normalized_power;
            tal_mutex_unlock(g_audio_power_mutex);
        }
    }
}

/**
 * @brief Process audio output data and calculate power
 */
static void process_audio_output_power(uint8_t *audio_data, uint32_t data_len)
{
    uint32_t num_samples = data_len / BYTES_PER_SAMPLE;
    if (num_samples > AUDIO_BUFFER_SIZE) {
        num_samples = AUDIO_BUFFER_SIZE;
    }

    int16_t *pcm_samples = (int16_t *)audio_data;

    if (num_samples >= AUDIO_BUFFER_SIZE) {
        memcpy(g_audio_output_buffer, pcm_samples, AUDIO_BUFFER_SIZE * sizeof(int16_t));
    } else {
        memmove(g_audio_output_buffer, &g_audio_output_buffer[num_samples],
                (AUDIO_BUFFER_SIZE - num_samples) * sizeof(int16_t));
        memcpy(&g_audio_output_buffer[AUDIO_BUFFER_SIZE - num_samples], pcm_samples, num_samples * sizeof(int16_t));
    }

    float sum_squares = 0.0f;
    int valid_samples = 0;

    for (int i = 0; i < AUDIO_BUFFER_SIZE; i++) {
        float sample = (float)g_audio_output_buffer[i];
        sum_squares += sample * sample;
        valid_samples++;
    }

    float rms = 0.0f;
    if (valid_samples > 0) {
        rms = sqrtf(sum_squares / (float)valid_samples);
    }

    float normalized_power = rms / POWER_NORMALIZATION;
    if (normalized_power > 1.0f)
        normalized_power = 1.0f;
    if (normalized_power < 0.0f)
        normalized_power = 0.0f;

    normalized_power = log10f(1.0f + normalized_power * 9.0f);

    if (g_audio_power_mutex != NULL) {
        if (tal_mutex_lock(g_audio_power_mutex) == OPRT_OK) {
            g_audio_output_power = normalized_power;
            tal_mutex_unlock(g_audio_power_mutex);
        }
    }
}

/**
 * @brief Bridge audio output PCM data for RMS calculation
 * @deprecated This function is no longer called directly from ai_audio_player.
 *            Use ai_audio_player_register_output_cb() instead.
 *            Kept for backward compatibility.
 */
void sphere_effect_integration_bridge_audio_output(int16_t *pcm_data, uint32_t samples)
{
    // This function is deprecated - callbacks are used instead
    // But we can still process if called directly
    if (pcm_data == NULL || samples == 0) {
        return;
    }
    uint32_t data_len = samples * BYTES_PER_SAMPLE;
    process_audio_output_power((uint8_t *)pcm_data, data_len);
}

/**
 * @brief Map AI audio state to sphere effect state
 */
static voice_state_t map_ai_state_to_sphere_state(AI_AUDIO_STATE_E ai_state)
{
    switch (ai_state) {
    case AI_AUDIO_STATE_STANDBY:
        // When player finishes, transition smoothly to idle
        return VOICE_STATE_TRANSITION_TO_IDLE;
    case AI_AUDIO_STATE_LISTEN:
        return VOICE_STATE_START;
    case AI_AUDIO_STATE_UPLOAD:
        return VOICE_STATE_PROCESSING;
    case AI_AUDIO_STATE_AI_SPEAK:
        return VOICE_STATE_RESPONDING;
    default:
        return VOICE_STATE_TRANSITION_TO_IDLE;
    }
}

/**
 * @brief Initialize sphere effect integration
 */
OPERATE_RET sphere_effect_integration_init(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(ENABLE_SPI) && (ENABLE_SPI) && defined(ENABLE_LEDS_PIXEL) && (ENABLE_LEDS_PIXEL)
    rt = pixel_led_init();
    if (OPRT_OK != rt) {
        PR_ERR("Pixel LED initialization failed: %d", rt);
        return rt;
    }
    PR_NOTICE("Pixel LED initialized");
#endif

    // Audio input is owned by ai_audio_input.c, no ring buffer needed here
    g_audio_input_ringbuf = NULL;
    g_audio_input_rb_mutex = NULL;

    // Audio output is owned by ai_audio_player.c, no ring buffer needed here
    g_audio_output_ringbuf = NULL;
    g_audio_output_rb_mutex = NULL;

    rt = tal_mutex_create_init(&g_audio_power_mutex);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to create audio power mutex: %d", rt);
        return rt;
    }

    rt = tal_mutex_create_init(&g_buffer_mutex);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to create buffer mutex: %d", rt);
        return rt;
    }

    rt = tal_mutex_create_init(&g_voice_state_mutex);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to create voice state mutex: %d", rt);
        return rt;
    }

    // Initialize double buffers
    for (int i = 0; i < NUM_BUFFERS; i++) {
        memset(g_render_buffers[i].r, 0, sizeof(g_render_buffers[i].r));
        memset(g_render_buffers[i].g, 0, sizeof(g_render_buffers[i].g));
        memset(g_render_buffers[i].b, 0, sizeof(g_render_buffers[i].b));
        g_render_buffers[i].ready = false;
    }
    g_front_buffer = 0;
    g_back_buffer = 1;

    // Initialize audio power
    g_audio_input_power = 0.0f;
    g_audio_output_power = 0.0f;
    g_effective_audio_power = 0.0f;
    g_peak_audio_power = 0.0f;
    g_last_audio_update_time = 0;
    memset(g_audio_input_buffer, 0, sizeof(g_audio_input_buffer));
    memset(g_audio_output_buffer, 0, sizeof(g_audio_output_buffer));

    // Initialize voice state
    g_voice_state = VOICE_STATE_IDLE;
    g_state_transition_start_time = 0;
    g_idle_circle_radius = 2.5f;
    g_target_animation_radius = SPHERE_RADIUS;
    g_current_radius = 2.5f;
    g_running_ring_angle = 0.0f;
    g_running_ring_angle2 = 0.0f;
    g_start_state_peak_radius = 2.5f;

    // Initialize audio-reactive effects
    g_audio_power_smoothed = 0.0f;
    g_sphere_breath = 0.0f;
    g_hue_shift = 0.0f;
    g_random_hue_offset = 0.0f;
    g_last_random_update = 0;

    // Note: Audio output capture is handled differently
    // The audio player already uses the audio device, so we can't directly open it for output capture
    // Instead, we'll try to hook into the player's output stream if possible
    // For now, output power will be 0 during UPLOAD and will be populated during AI_SPEAK
    // if we can capture the output stream. The processing state will show rings regardless.
    // TODO: Hook into audio player output stream for better output audio analysis
    g_audio_output_handle = NULL; // Not used directly - would need player integration

    // Register callbacks with AI audio components to receive audio data
    OPERATE_RET cb_rt = ai_audio_input_register_data_cb(sphere_audio_input_cb, NULL);
    if (OPRT_OK != cb_rt) {
        PR_ERR("Failed to register audio input callback: %d", cb_rt);
    } else {
        PR_NOTICE("Registered audio input callback");
    }

    cb_rt = ai_audio_player_register_output_cb(sphere_audio_output_cb, NULL);
    if (OPRT_OK != cb_rt) {
        PR_ERR("Failed to register audio output callback: %d", cb_rt);
    } else {
        PR_NOTICE("Registered audio output callback");
    }

#if defined(ENABLE_SPI) && (ENABLE_SPI) && defined(ENABLE_LEDS_PIXEL) && (ENABLE_LEDS_PIXEL)
    // Start sphere rendering task
    THREAD_CFG_T render_thrd_param = {.stackDepth = 4096, .priority = THREAD_PRIO_3, .thrdname = "sphere_render"};
    THREAD_HANDLE render_thrd = NULL;
    rt = tal_thread_create_and_start(&render_thrd, NULL, NULL, sphere_rendering_task, NULL, &render_thrd_param);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to start sphere rendering thread: %d", rt);
        return rt;
    }
    PR_NOTICE("Sphere rendering thread started");

    // Start sphere display task
    THREAD_CFG_T display_thrd_param = {.stackDepth = 4096, .priority = THREAD_PRIO_1, .thrdname = "sphere_display"};
    THREAD_HANDLE display_thrd = NULL;
    rt = tal_thread_create_and_start(&display_thrd, NULL, NULL, sphere_display_task, NULL, &display_thrd_param);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to start sphere display thread: %d", rt);
        return rt;
    }
    PR_NOTICE("Sphere display thread started");
#endif

    PR_NOTICE("Sphere effect integration initialized");
    return OPRT_OK;
}

/**
 * @brief Update sphere effect state based on AI audio state
 */
void sphere_effect_integration_update_state(AI_AUDIO_STATE_E state)
{
    voice_state_t sphere_state = map_ai_state_to_sphere_state(state);
    transition_to_state(sphere_state);
    PR_DEBUG("Sphere effect state updated: AI state %d -> Sphere state %d", state, sphere_state);
}

/**
 * @brief Process audio input data for animation
 * @deprecated This function is no longer called directly from ai_audio_input.
 *            Use ai_audio_input_register_data_cb() instead.
 *            Kept for backward compatibility.
 */
void sphere_effect_integration_process_audio_input(uint8_t *data, uint32_t len)
{
    // This function is deprecated - callbacks are used instead
    // But we can still process if called directly
    if (data != NULL && len > 0) {
        process_audio_input_power(data, len);
    }
}

/**
 * @brief Process audio output data for animation
 */
void sphere_effect_integration_process_audio_output(uint8_t *data, uint32_t len)
{
    // This function can be called directly with audio output data
    if (data != NULL && len > 0) {
        process_audio_output_power(data, len);
    }
}

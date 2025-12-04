/**
 * @file sphere_effect_integration.h
 * @brief Sphere effect integration for chat bot - maps AI audio states to visual sphere animations
 * @version 0.1
 * @date 2025-01-XX
 */

#ifndef __SPHERE_EFFECT_INTEGRATION_H__
#define __SPHERE_EFFECT_INTEGRATION_H__

#include "tuya_cloud_types.h"
#include "ai_audio.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Initialize sphere effect integration
 * @return OPERATE_RET - OPRT_OK on success
 */
OPERATE_RET sphere_effect_integration_init(void);

/**
 * @brief Update sphere effect state based on AI audio state
 * @param state - AI audio state
 */
void sphere_effect_integration_update_state(AI_AUDIO_STATE_E state);

/**
 * @brief Process audio input data for animation (for LISTEN and AI_SPEAK states)
 * @param data - Audio PCM data
 * @param len - Data length in bytes
 */
void sphere_effect_integration_process_audio_input(uint8_t *data, uint32_t len);

/**
 * @brief Process audio output data for animation (for UPLOAD/PROCESSING state)
 * @param data - Audio PCM data
 * @param len - Data length in bytes
 */
void sphere_effect_integration_process_audio_output(uint8_t *data, uint32_t len);

/**
 * @brief Bridge audio output PCM data for RMS calculation
 * This function should be called from ai_audio_player when it writes PCM data via tdl_audio_play()
 *
 * @param pcm_data Pointer to PCM audio data (int16_t samples)
 * @param samples Number of samples (not bytes)
 */
void sphere_effect_integration_bridge_audio_output(int16_t *pcm_data, uint32_t samples);

#ifdef __cplusplus
}
#endif

#endif /* __SPHERE_EFFECT_INTEGRATION_H__ */

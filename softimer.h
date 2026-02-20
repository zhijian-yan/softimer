// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Zhijian Yan

#ifndef __SOFTIMER_H
#define __SOFTIMER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

#define STIM_MALLOC(size) malloc(size)
#define STIM_FREE(ptr) free(ptr)
#define STIM_ENTER_CRITICAL ((void *)0)
#define STIM_EXIT_CRITICAL ((void *)0)
#define STIM_CMD_ARR_SIZE (10)
#define STIM_MAX_TICKS (((uint32_t)(-1)) >> 1)

/**
 * @brief Opaque handle type for timer instances
 */
typedef stim_t *stim_handle_t;

/**
 * @brief Timer expiration callback function type
 *
 * @param timer Handle of the expired timer
 * @param user_data User-defined data pointer
 */
typedef void (*stim_cb_t)(stim_handle_t timer, void *user_data);

/**
 * @brief Increment the system tick counter
 * @note This function should be called periodically to advance the timer base
 */
void stim_systick_inc(void);

/**
 * @brief Create a new timer instance
 * @note The period parameter must be in the range [1, STIM_MAX_TICKS]
 *       If the period is outside this range, the function returns NULL
 *
 * @param period_ticks Timer period in system ticks
 * @param cb Callback function to be executed upon timer expiration
 * @param user_data User-defined data pointer passed to the callback function
 * @return
 * Timer handle on success, NULL on failure
 */
stim_handle_t stim_create(uint32_t period_ticks, stim_cb_t cb, void *user_data);

/**
 * @brief Delete a timer instance and release its resources
 * @note This operation can only be performed when the timer is stopped
 *
 * @param timer Handle of the timer to be deleted
 */
void stim_delete(stim_handle_t timer);

/**
 * @brief Start the specified timer
 * @note The timer will be inserted into the active timer list sorted by
 * remaining time
 *
 * @param timer Handle of the timer to start
 */
int stim_start(stim_handle_t timer);

/**
 * @brief Stop the specified timer
 * @note This function removes the timer from the active timer list
 *
 * @param timer Handle of the timer to stop
 */
int stim_stop(stim_handle_t timer);

/**
 * @brief Process timer expiration events
 * @note This function should be called periodically to handle expired timers
 */
void stim_handler(void);

/**
 * @brief Register or update the callback function for a timer
 *
 * @param timer Handle of the target timer
 * @param cb New callback function to be registered
 * @param user_data User data to be passed to the callback function
 */
void stim_register_callback(stim_handle_t timer, stim_cb_t cb, void *user_data);

/**
 * @brief Set the period for the specified timer
 * @note The period parameter must be in the range [1, STIM_MAX_TICKS]
 *       If the period is outside this range, the function returns -1
 *
 * @param timer Handle of the target timer
 * @param period_ticks New timer period in system ticks
 */
void stim_set_period_ticks(stim_handle_t timer, uint32_t period_ticks);

/**
 * @brief Set timer count value
 *
 * @param timer Handle of the target timer
 * @param count New count value
 */
void stim_set_count(stim_handle_t timer, uint32_t count);

/**
 * @brief Get current timer count value
 *
 * @param timer Handle of the target timer
 * @return
 * Current count value of timer, always 0 for invalid handle
 */
uint32_t stim_get_count(stim_handle_t timer);

#ifdef __cplusplus
}
#endif

#endif

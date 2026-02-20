// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Zhijian Yan

#ifndef __SOFTIMER_H
#define __SOFTIMER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define STIM_CMD_ARR_SIZE (16) // must be power of 2
#define STIM_MAX_TICKS (((uint32_t)(-1)) >> 1)

typedef uint32_t stim_irq_state_t;

static inline stim_irq_state_t stim_enter_critical(void) {
    //
    return 0;
}

static inline void stim_exit_critical(stim_irq_state_t irq_state) {
    //
    (void)irq_state;
}

/**
 * @brief Opaque handle type for timer instances
 */
typedef struct stim *stim_handle_t;

/**
 * @brief Timer expiration callback function type
 *
 * @param timer Handle of the expired timer
 * @param user_data User-defined data pointer
 */
typedef void (*stim_cb_t)(stim_handle_t timer, void *user_data);

typedef struct stim_node {
    struct stim_node *next;
    struct stim_node *prev;
} stim_node_t;

typedef struct stim {
    stim_cb_t cb;
    void *user_data;
    uint32_t expiry_ticks;
    uint32_t period_ticks;
    uint32_t count;
    enum {
        STIM_DISABLE = 0,
        STIM_ENABLE = 1,
    } state;
    stim_node_t node;
} stim_t;

/**
 * @brief Increment the system tick counter
 * @note This function should be called periodically to advance the timer base
 */
void stim_systick_inc(void);

int stim_init(stim_t *timer, uint32_t period_ticks, stim_cb_t cb,
              void *user_data);

/**
 * @brief Start the specified timer
 * @note The timer will be inserted into the active timer list sorted by
 * remaining time
 *
 * @param timer Handle of the timer to start
 */
int stim_start(stim_t *timer);

/**
 * @brief Stop the specified timer
 * @note This function removes the timer from the active timer list
 *
 * @param timer Handle of the timer to stop
 */
int stim_stop(stim_t *timer);

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
void stim_register_callback(stim_t *timer, stim_cb_t cb, void *user_data);

/**
 * @brief Set the period for the specified timer
 * @note The period parameter must be in the range [1, STIM_MAX_TICKS]
 *       If the period is outside this range, the function returns -1
 *
 * @param timer Handle of the target timer
 * @param period_ticks New timer period in system ticks
 */
void stim_set_period_ticks(stim_t *timer, uint32_t period_ticks);

/**
 * @brief Set timer count value
 *
 * @param timer Handle of the target timer
 * @param count New count value
 */
void stim_set_count(stim_t *timer, uint32_t count);

/**
 * @brief Get current timer count value
 *
 * @param timer Handle of the target timer
 * @return
 * Current count value of timer, always 0 for invalid handle
 */
uint32_t stim_get_count(stim_t *timer);

#ifdef __cplusplus
}
#endif

#endif

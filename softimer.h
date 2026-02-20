// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Zhijian Yan

#ifndef __SOFTIMER_H
#define __SOFTIMER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @def STIM_CMD_ARR_SIZE
 * @brief Size of internal command queue (must be power of 2)
 *
 * This defines the capacity of the internal start/stop command ring buffer.
 * Increasing this value allows more concurrent start/stop requests before
 * overflow occurs.
 *
 * Must be a power of 2.
 */
#define STIM_CMD_ARR_SIZE (16)

/**
 * @def STIM_MAX_TICKS
 * @brief Maximum supported tick interval
 *
 * Timers must use period values in range:
 *      [1, STIM_MAX_TICKS]
 *
 * The value is chosen to ensure safe signed wrap-around comparison.
 */
#define STIM_MAX_TICKS (((uint32_t)(-1)) >> 1)

/**
 * @brief Platform-defined interrupt state type
 *
 * This type represents the CPU interrupt state that must be saved and
 * restored when entering/exiting critical sections.
 *
 * On bare-metal systems this may contain PRIMASK or similar register.
 * On non-RTOS systems it may simply be unused.
 */
typedef uint32_t stim_irq_state_t;

/**
 * @brief Enter critical section
 *
 * Default implementation:
 *   - Does nothing
 *   - Suitable for single-threaded / non-interrupt environments
 *
 * ⚠ IMPORTANT:
 *   If this timer is used in:
 *      - ISR context
 *      - Multi-threaded environment
 *      - RTOS environment
 *
 *   You MUST provide your own implementation that:
 *      - Disables interrupts or acquires a lock
 *      - Returns previous interrupt/lock state
 *
 *   Example (Cortex-M):
 *      uint32_t primask = __get_PRIMASK();
 *      __disable_irq();
 *      return primask;
 *
 * @return Previous interrupt state (0 by default)
 */
static inline stim_irq_state_t stim_enter_critical(void) {
    /* TODO: Implement platform-specific interrupt disable here if needed */
    return 0;
}

/**
 * @brief Exit critical section
 *
 * Default implementation:
 *   - Does nothing
 *
 * ⚠ IMPORTANT:
 *   Must restore the state returned by stim_enter_critical()
 *
 *   Example (Cortex-M):
 *      if (!irq_state)
 *          __enable_irq();
 *
 * @param irq_state State returned by stim_enter_critical()
 */
static inline void stim_exit_critical(stim_irq_state_t irq_state) {
    /* TODO: Restore interrupt state here if using real critical section */
    (void)irq_state;
}

/**
 * @brief Opaque handle type for timer instances
 *
 * Users should treat this as a handle.
 */
typedef struct stim *stim_handle_t;

/**
 * @brief Timer expiration callback function type
 *
 * This function is called when a timer expires.
 *
 * @note The callback is executed in the context of stim_handler().
 *       It MUST NOT block for long durations.
 *
 * @param timer     Handle of the expired timer
 * @param user_data User-defined data pointer
 */
typedef void (*stim_cb_t)(stim_handle_t timer, void *user_data);

/**
 * @brief Internal intrusive list node
 *
 * Used to link active timers in a doubly-linked sorted list.
 */
typedef struct stim_node {
    struct stim_node *next;
    struct stim_node *prev;
} stim_node_t;

/**
 * @brief Timer object structure
 *
 * Users must allocate this structure (static or dynamic).
 *
 * A timer must be initialized using stim_init() before use.
 *
 * Internal fields MUST NOT be modified directly by user code.
 */
typedef struct stim {
    stim_cb_t cb;          /**< Expiration callback function */
    void *user_data;       /**< User-provided data pointer */
    uint32_t expiry_ticks; /**< Absolute expiration time (internal use) */
    uint32_t period_ticks; /**< Timer period in system ticks */
    uint32_t count;        /**< Number of times this timer has expired */
    /**
     * @brief Timer state
     *
     * STIM_DISABLE  - timer not active
     * STIM_ENABLE   - timer active
     */
    enum {
        STIM_DISABLE = 0,
        STIM_ENABLE = 1,
    } state;
    stim_node_t node; /**< Intrusive list node (internal use) */
} stim_t;

/**
 * @brief Increment the system tick counter
 *
 * This function advances the internal time base.
 *
 * @note Typically called from:
 *       - SysTick interrupt
 *       - Hardware timer ISR
 *
 * This function must be called periodically.
 */
void stim_systick_inc(void);

/**
 * @brief Initialize a timer instance
 *
 * This function must be called before using the timer.
 *
 * @param timer        Pointer to timer object
 * @param period_ticks Period in system ticks (1 ~ STIM_MAX_TICKS)
 * @param cb           Expiration callback
 * @param user_data    User-defined pointer passed to callback
 *
 * @return 0 on success
 * @return -1 on invalid parameter
 */
int stim_init(stim_t *timer, uint32_t period_ticks, stim_cb_t cb,
              void *user_data);

/**
 * @brief Start the specified timer
 *
 * The timer will be scheduled and inserted into the active list.
 *
 * @note This function is thread/ISR safe.
 * @note If command queue is full, returns -1.
 *
 * @param timer Timer to start
 *
 * @return 0 on success
 * @return -1 on error
 */
int stim_start(stim_t *timer);

/**
 * @brief Stop the specified timer
 *
 * Removes the timer from the active list.
 *
 * @note This function is thread/ISR safe.
 * @note If command queue is full, returns -1.
 *
 * @param timer Timer to stop
 *
 * @return 0 on success
 * @return -1 on error
 */
int stim_stop(stim_t *timer);

/**
 * @brief Process timer events
 *
 * This function:
 *  - Handles pending start/stop commands
 *  - Checks expired timers
 *  - Executes expiration callbacks
 *
 * @warning Must NOT be called concurrently.
 * @note Should be called periodically from main loop or timer task.
 */
void stim_handler(void);

/**
 * @brief Register or update timer callback
 *
 * Can be used to change callback function at runtime.
 *
 * @param timer Target timer
 * @param cb New callback
 * @param user_data User data pointer
 */
void stim_register_callback(stim_t *timer, stim_cb_t cb, void *user_data);

/**
 * @brief Set timer period
 *
 * @note Does NOT automatically restart the timer.
 * @note If timer is running, new period takes effect after next expiration.
 *
 * @param timer Target timer
 * @param period_ticks New period
 */
void stim_set_period_ticks(stim_t *timer, uint32_t period_ticks);

/**
 * @brief Set timer expiration count value
 *
 * @param timer Target timer
 * @param count New count value
 */
void stim_set_count(stim_t *timer, uint32_t count);

/**
 * @brief Get timer expiration count
 *
 * @param timer Target timer
 * @return Number of expirations since initialization
 */
uint32_t stim_get_count(stim_t *timer);

#ifdef __cplusplus
}
#endif

#endif

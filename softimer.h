// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Zhijian Yan

#ifndef __SOFTIMER_H
#define __SOFTIMER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

static inline int stim_lock(void) {
    /* Disable interrupts if needed */
    return 0;
}

static inline void stim_unlock(int stim_lock_state) {
    /* Restore interrupt state */
    (void)stim_lock_state;
}

#define STIM_ATOMIC_TICKS
#define STIM_QUEUE_SIZE (16)
#if (STIM_QUEUE_SIZE > 256)
#error "STIM_QUEUE_SIZE must be <= 256"
#endif
#if (STIM_QUEUE_SIZE & (STIM_QUEUE_SIZE - 1)) != 0
#error "STIM_QUEUE_SIZE must be power of 2"
#endif
#define STIM_MAX_TICKS (((uint32_t)(-1)) >> 1)
#define STIM_EINVAL 22
#define STIM_EAGAIN 11

typedef struct stim stim_t;

typedef void (*stim_cb_t)(stim_t *timer, void *user_data);

typedef enum {
    STIM_STATE_STOPPED = 0,
    STIM_STATE_RUNNING,
} stim_state_t;

typedef enum {
    STIM_CB_MODE_DEFERRED,
    STIM_CB_MODE_IMMEDIATE,
} stim_cb_mode_t;

typedef struct stim_node {
    struct stim_node *next;
    struct stim_node *prev;
} stim_node_t;

struct stim {
    stim_node_t node;
    stim_cb_t cb;
    void *user_data;
    stim_cb_mode_t cb_mode;
    stim_state_t state;
    uint32_t expire_ticks;
    uint32_t period_ticks;
    volatile uint32_t count;
};

void stim_tick_inc(void);
int stim_init(stim_t *timer, uint32_t period_ticks, stim_cb_mode_t cb_mode,
              stim_cb_t cb, void *user_data);
int stim_start(stim_t *timer);
int stim_stop(stim_t *timer);
int stim_poll(void);
void stim_dispatch(void);
int stim_set_count(stim_t *timer, uint32_t count);
int stim_get_count(const stim_t *timer, uint32_t *count);

#ifdef __cplusplus
}
#endif

#endif

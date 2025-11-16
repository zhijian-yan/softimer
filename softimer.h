// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Zhijian Yan

#ifndef __SOFTIMER_H
#define __SOFTIMER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

#define stim_malloc(size) malloc(size)
#define stim_free(ptr) free(ptr)
#define STIM_MAX_TICKS (((uint32_t) - 1) >> 1)

typedef struct stim *stim_handle_t;
typedef void (*stim_cb_t)(stim_handle_t timer, void *user_data);

inline void stim_systick_inc(void);
stim_handle_t stim_create(uint32_t period_ticks, stim_cb_t cb, void *user_data);
void stim_delete(stim_handle_t *ptimer);
void stim_start(stim_handle_t timer);
void stim_stop(stim_handle_t timer);
void stim_handler(void);
int stim_register_callback(stim_handle_t timer, stim_cb_t cb, void *user_data);
int stim_set_period(stim_handle_t timer, uint32_t period_ticks);
int stim_set_period_sync(stim_handle_t timer, uint32_t period_ticks);

#ifdef __cplusplus
}
#endif

#endif

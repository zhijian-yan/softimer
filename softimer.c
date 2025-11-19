// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Zhijian Yan

#include "softimer.h"

static struct stim {
  stim_cb_t cb;
  void *user_data;
  uint32_t expiry_ticks;
  uint32_t period_ticks : 31;
  uint32_t enabled : 1;
  stim_handle_t prev;
  stim_handle_t next;
} *head;

volatile static uint32_t stim_systicks;
volatile static uint32_t flag_critical;

inline void stim_systick_inc(void) { stim_systicks++; }

stim_handle_t stim_create(uint32_t period_ticks, stim_cb_t cb,
                          void *user_data) {
  stim_handle_t timer;
  if (period_ticks > STIM_MAX_TICKS || period_ticks == 0)
    return NULL;
  timer = (stim_handle_t)stim_malloc(sizeof(struct stim));
  if (!timer)
    return NULL;
  timer->period_ticks = period_ticks;
  timer->expiry_ticks = period_ticks;
  timer->user_data = user_data;
  timer->cb = cb;
  timer->enabled = 0;
  timer->prev = NULL;
  timer->next = NULL;
  return timer;
}

void stim_delete(stim_handle_t *ptimer) {
  stim_handle_t timer = *ptimer;
  if (!ptimer || !timer)
    return;
  if (timer->enabled)
    return;
  *ptimer = NULL;
  stim_free(timer);
}

static void stim_list_add(stim_handle_t timer) {
  stim_handle_t temp = head;
  uint32_t curr_ticks = stim_systicks;
  flag_critical = 1;
  while (temp) {
    if ((int32_t)(timer->expiry_ticks - curr_ticks) <
        (int32_t)(temp->expiry_ticks - curr_ticks)) {
      break;
    }
    temp = temp->next;
  }
  if (temp) {
    timer->prev = temp->prev;
    timer->next = temp;
    if (temp == head)
      head = timer;
    else
      temp->prev->next = timer;
    temp->prev = timer;
  } else {
    if (head == NULL) {
      head = timer;
      head->prev = timer;
    } else {
      timer->prev = head->prev;
      head->prev->next = timer;
      head->prev = timer;
    }
    timer->next = NULL;
  }
  flag_critical = 0;
}

static void stim_list_del(stim_handle_t timer) {
  flag_critical = 1;
  if (timer == head) {
    if (timer->next) {
      timer->next->prev = head->prev;
      head = timer->next;
    } else
      head = NULL;
  } else {
    timer->prev->next = timer->next;
    if (timer->next)
      timer->next->prev = timer->prev;
    else
      head->prev = timer->prev;
  }
  timer->prev = NULL;
  timer->next = NULL;
  flag_critical = 0;
}

void stim_start(stim_handle_t timer) {
  if (!timer || timer->enabled)
    return;
  timer->enabled = 1;
  timer->expiry_ticks += stim_systicks;
  stim_list_add(timer);
}

void stim_stop(stim_handle_t timer) {
  if (!timer || !timer->enabled)
    return;
  stim_list_del(timer);
  timer->expiry_ticks = timer->period_ticks;
  timer->enabled = 0;
}

static int stim_handle_timer(stim_handle_t timer, uint32_t curr_ticks) {
  if ((int32_t)(timer->expiry_ticks - curr_ticks) <= 0) {
    timer->expiry_ticks = curr_ticks + timer->period_ticks;
    if (timer->cb)
      timer->cb(timer, timer->user_data);
    return 1;
  }
  return 0;
}

void stim_handler(void) {
  stim_handle_t temp = head;
  uint32_t curr_ticks = stim_systicks;
  if (flag_critical)
    return;
  while (temp && stim_handle_timer(temp, curr_ticks)) {
    if (temp == head && temp->enabled) {
      stim_list_del(temp);
      stim_list_add(temp);
    }
    temp = head;
  }
}

int stim_register_callback(stim_handle_t timer, stim_cb_t cb, void *user_data) {
  if (timer && !timer->enabled) {
    timer->user_data = user_data;
    timer->cb = cb;
    return 0;
  }
  return -1;
}

int stim_set_period(stim_handle_t timer, uint32_t period_ticks) {
  if (timer && !timer->enabled) {
    if (period_ticks > STIM_MAX_TICKS)
      period_ticks = STIM_MAX_TICKS;
    timer->period_ticks = period_ticks;
    return 0;
  }
  return -1;
}

int stim_set_period_reset(stim_handle_t timer, uint32_t period_ticks) {
  if (timer && !timer->enabled) {
    if (period_ticks > STIM_MAX_TICKS)
      period_ticks = STIM_MAX_TICKS;
    timer->period_ticks = period_ticks;
    timer->expiry_ticks = period_ticks;
    return 0;
  }
  return -1;
}

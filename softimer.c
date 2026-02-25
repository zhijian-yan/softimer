// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Zhijian Yan

#include "softimer.h"
#include <stddef.h>

#if (STIM_CMD_ARR_SIZE & (STIM_CMD_ARR_SIZE - 1)) != 0
#error "STIM_CMD_ARR_SIZE must be power of 2"
#endif

#define container_of(ptr, type, member)                                        \
    ((type *)((char *)(ptr) - offsetof(type, member)))

typedef enum {
    STIM_CMD_NONE = 0,
    STIM_CMD_START,
    STIM_CMD_STOP,
} stim_cmd_type_t;

typedef struct {
    stim_t *stim;
    stim_cmd_type_t type;
} stim_cmd_t;

static stim_node_t head = {
    .next = &head,
    .prev = &head,
};
static stim_cmd_t stim_cmd_arr[STIM_CMD_ARR_SIZE];
static stim_tick_atomic_t stim_systicks = 0;
static uint32_t stim_cmd_head = 0;
static uint32_t stim_cmd_tail = 0;

void stim_systick_inc(void) {
#if defined(STIM_USE_C11_ATOMIC)
    atomic_fetch_add_explicit(&stim_systicks, 1, memory_order_relaxed);
#elif defined(STIM_USE_CRITICAL)
    stim_irq_state_t irq_state = stim_enter_critical();
    ++stim_systicks;
    stim_exit_critical(irq_state);
#else
    ++stim_systicks;
#endif
}

static uint32_t stim_get_systicks(void) {
#if defined(STIM_USE_C11_ATOMIC)
    return atomic_load_explicit(&stim_systicks, memory_order_relaxed);
#elif defined(STIM_USE_CRITICAL)
    uint32_t now;
    stim_irq_state_t irq_state = stim_enter_critical();
    now = stim_systicks;
    stim_exit_critical(irq_state);
    return now;
#else
    return stim_systicks;
#endif
}

int stim_init(stim_t *timer, uint32_t period_ticks, stim_cb_t cb,
              void *user_data) {
    if (!timer || period_ticks > STIM_MAX_TICKS || period_ticks == 0)
        return -1;
    timer->period_ticks = period_ticks;
    timer->expiry_ticks = 0;
    timer->user_data = user_data;
    timer->cb = cb;
    timer->count = 0;
    timer->state = STIM_DISABLE;
    timer->node.next = &timer->node;
    timer->node.prev = &timer->node;
    return 0;
}

static void stim_list_add(stim_t *timer, uint32_t now) {
    stim_t *entry;
    stim_node_t *pos;
    stim_node_t *node = &timer->node;
    if (node->next != node)
        return;
    for (pos = head.next; pos != &head; pos = pos->next) {
        entry = container_of(pos, stim_t, node);
        if ((int32_t)(timer->expiry_ticks - now) <
            (int32_t)(entry->expiry_ticks - now)) {
            break;
        }
    }
    node->next = pos;
    node->prev = pos->prev;
    pos->prev->next = node;
    pos->prev = node;
}

static void stim_list_del(stim_t *timer) {
    stim_node_t *node = &timer->node;
    if (node->next == node)
        return;
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->next = node;
    node->prev = node;
}

static int stim_cmd_push(stim_t *timer, stim_cmd_type_t type) {
    uint32_t next_head;
    next_head = (stim_cmd_head + 1) & (STIM_CMD_ARR_SIZE - 1);
    if (next_head == stim_cmd_tail) {
        return -1;
    }
    stim_cmd_arr[stim_cmd_head].stim = timer;
    stim_cmd_arr[stim_cmd_head].type = type;
    stim_cmd_head = next_head;
    return 0;
}

int stim_start(stim_t *timer) {
    stim_irq_state_t irq_state;
    int ret = 0;
    if (!timer)
        return -1;
    irq_state = stim_enter_critical();
    if (timer->state == STIM_DISABLE) {
        ret = stim_cmd_push(timer, STIM_CMD_START);
        if (!ret)
            timer->state = STIM_ENABLE;
    }
    stim_exit_critical(irq_state);
    return ret;
}

int stim_stop(stim_t *timer) {
    stim_irq_state_t irq_state;
    int ret = 0;
    if (!timer)
        return -1;
    irq_state = stim_enter_critical();
    if (timer->state == STIM_ENABLE) {
        ret = stim_cmd_push(timer, STIM_CMD_STOP);
        if (!ret)
            timer->state = STIM_DISABLE;
    }
    stim_exit_critical(irq_state);
    return ret;
}

static void stim_cmd_handler(void) {
    stim_cmd_t cmd;
    uint32_t now;
    stim_irq_state_t irq_state;
    while (1) {
        irq_state = stim_enter_critical();
        if (stim_cmd_head == stim_cmd_tail) {
            stim_exit_critical(irq_state);
            return;
        }
        cmd = stim_cmd_arr[stim_cmd_tail];
        stim_cmd_tail = (stim_cmd_tail + 1) & (STIM_CMD_ARR_SIZE - 1);
        stim_exit_critical(irq_state);
        switch (cmd.type) {
        case STIM_CMD_START:
            now = stim_get_systicks();
            cmd.stim->expiry_ticks = cmd.stim->period_ticks + now;
            stim_list_add(cmd.stim, now);
            break;
        case STIM_CMD_STOP:
            stim_list_del(cmd.stim);
            break;
        default:
            break;
        }
    }
}

void stim_handler(void) {
    stim_t *expired;
    uint32_t now;
    stim_cmd_handler();
    now = stim_get_systicks();
    while (head.next != &head) {
        expired = container_of(head.next, stim_t, node);
        if ((int32_t)(expired->expiry_ticks - now) <= 0) {
            stim_list_del(expired);
            ++expired->count;
            if (expired->cb)
                expired->cb(expired, expired->user_data);
            if (expired->state == STIM_ENABLE) {
                expired->expiry_ticks += expired->period_ticks;
                stim_list_add(expired, now);
            }
        } else
            break;
    }
}

void stim_register_callback(stim_t *timer, stim_cb_t cb, void *user_data) {
    if (!timer)
        return;
    timer->user_data = user_data;
    timer->cb = cb;
}

void stim_set_period_ticks(stim_t *timer, uint32_t period_ticks) {
    if (!timer)
        return;
    if (period_ticks > STIM_MAX_TICKS || period_ticks == 0)
        return;
    timer->period_ticks = period_ticks;
}

void stim_set_count(stim_t *timer, uint32_t count) {
    if (!timer)
        return;
    timer->count = count;
}

uint32_t stim_get_count(stim_t *timer) {
    if (!timer)
        return 0;
    return timer->count;
}

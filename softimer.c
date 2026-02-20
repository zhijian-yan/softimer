// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Zhijian Yan

#include "softimer.h"
#include <string.h>

#if (STIM_CMD_ARR_SIZE & (STIM_CMD_ARR_SIZE - 1)) != 0
#error "STIM_CMD_ARR_SIZE must be power of two"
#endif

#define container_of(ptr, type, member)                                        \
    ((type *)((char *)(ptr) - offsetof(type, member)))

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

typedef struct {
    stim_t *stim;
    enum {
        STIM_CMD_NONE = 0,
        STIM_CMD_START,
        STIM_CMD_STOP,
    } type;
} stim_cmd_t;

static stim_node_t head = {
    .next = &head,
    .prev = &head,
};
static stim_cmd_t stim_cmd_arr[STIM_CMD_ARR_SIZE];
volatile static uint32_t stim_systicks = 0;
volatile static uint32_t stim_cmd_head = 0;
volatile static uint32_t stim_cmd_tail = 0;

void stim_systick_inc(void) { ++stim_systicks; }

static inline uint32_t stim_get_systicks(void) {
    uint32_t now;
    STIM_ENTER_CRITICAL;
    now = stim_systicks;
    STIM_EXIT_CRITICAL;
    return now;
}

stim_handle_t stim_create(uint32_t period_ticks, stim_cb_t cb,
                          void *user_data) {
    stim_t *stim = NULL;
    if (period_ticks > STIM_MAX_TICKS || period_ticks == 0)
        return NULL;
    stim = (stim_t *)STIM_MALLOC(sizeof(stim_t));
    if (!stim)
        return NULL;
    memset(stim, 0, sizeof(stim_t));
    stim->period_ticks = period_ticks;
    stim->expiry_ticks = 0;
    stim->user_data = user_data;
    stim->cb = cb;
    stim->count = 0;
    stim->state = STIM_DISABLE;
    stim->node.next = &stim->node;
    stim->node.prev = &stim->node;
    return stim;
}

void stim_delete(stim_handle_t timer) {
    stim_t *stim = timer;
    if (!stim || stim->state == STIM_ENABLE)
        return;
    STIM_FREE(stim);
}

static void stim_list_add(stim_handle_t timer, uint32_t now) {
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

static void stim_list_del(stim_handle_t timer) {
    stim_node_t *node = &timer->node;
    if (node->next == node)
        return;
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->next = node;
    node->prev = node;
}

static int stim_cmd_push(stim_t *stim, uint32_t type) {
    uint32_t next_head;
    next_head = (stim_cmd_head + 1) & (STIM_CMD_ARR_SIZE - 1);
    if (next_head == stim_cmd_tail) {
        return -1;
    }
    stim_cmd_arr[stim_cmd_head].stim = stim;
    stim_cmd_arr[stim_cmd_head].type = type;
    stim_cmd_head = next_head;
    return 0;
}

int stim_start(stim_handle_t timer) {
    stim_t *stim = timer;
    int ret = 0;
    if (!stim)
        return -1;
    STIM_ENTER_CRITICAL;
    if (stim->state == STIM_DISABLE) {
        ret = stim_cmd_push(stim, STIM_CMD_START);
        if (!ret)
            stim->state = STIM_ENABLE;
    }
    STIM_EXIT_CRITICAL;
    return ret;
}

int stim_stop(stim_handle_t timer) {
    stim_t *stim = timer;
    int ret = 0;
    if (!stim)
        return -1;
    STIM_ENTER_CRITICAL;
    if (stim->state == STIM_ENABLE) {
        ret = stim_cmd_push(stim, STIM_CMD_STOP);
        if (!ret)
            stim->state = STIM_DISABLE;
    }
    STIM_EXIT_CRITICAL;
    return ret;
}

static void stim_cmd_handler(void) {
    stim_cmd_t cmd;
    uint32_t now;
    while (1) {
        STIM_ENTER_CRITICAL;
        if (stim_cmd_head == stim_cmd_tail) {
            STIM_EXIT_CRITICAL;
            return;
        }
        cmd = stim_cmd_arr[stim_cmd_tail];
        stim_cmd_tail = (stim_cmd_tail + 1) & (STIM_CMD_ARR_SIZE - 1);
        STIM_EXIT_CRITICAL;
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
            return;
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

void stim_register_callback(stim_handle_t timer, stim_cb_t cb,
                            void *user_data) {
    stim_t *stim = timer;
    if (!stim)
        return;
    stim->user_data = user_data;
    stim->cb = cb;
}

void stim_set_period_ticks(stim_handle_t timer, uint32_t period_ticks) {
    stim_t *stim = timer;
    if (!stim)
        return;
    if (period_ticks > STIM_MAX_TICKS || period_ticks == 0)
        return;
    stim->period_ticks = period_ticks;
}

void stim_set_count(stim_handle_t timer, uint32_t count) {
    stim_t *stim = timer;
    if (!stim)
        return;
    stim->count = count;
}

uint32_t stim_get_count(stim_handle_t timer) {
    stim_t *stim = timer;
    if (!stim)
        return 0;
    return stim->count;
}

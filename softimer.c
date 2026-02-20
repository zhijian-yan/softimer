// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Zhijian Yan

#include "softimer.h"
#include <string.h>

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
    struct stim *prev;
    struct stim *next;
} stim_t;

typedef struct {
    stim_t *stim;
    enum {
        STIM_CMD_NONE = 0,
        STIM_CMD_START,
        STIM_CMD_STOP,
    } type;
} stim_cmd_t;

static stim_t *head = NULL;
static stim_cmd_t stim_cmd_arr[STIM_CMD_ARR_SIZE];
volatile static uint32_t stim_systicks = 0;
volatile static uint32_t stim_cmd_head = 0;
volatile static uint32_t stim_cmd_tail = 0;

void stim_systick_inc(void) { ++stim_systicks; }

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
    stim->prev = NULL;
    stim->next = NULL;
    return stim;
}

void stim_delete(stim_handle_t timer) {
    stim_t *stim = timer;
    if (!stim || stim->state == STIM_ENABLE)
        return;
    STIM_FREE(stim);
}

static void stim_list_add(stim_handle_t timer) {
    stim_t *stim = timer;
    stim_t *temp = head;
    uint32_t curr_ticks = stim_systicks;
    while (temp) {
        if ((int32_t)(stim->expiry_ticks - curr_ticks) <
            (int32_t)(temp->expiry_ticks - curr_ticks)) {
            break;
        }
        temp = temp->next;
    }
    if (temp) {
        stim->prev = temp->prev;
        stim->next = temp;
        if (temp == head)
            head = stim;
        else
            temp->prev->next = stim;
        temp->prev = stim;
    } else {
        if (head == NULL) {
            head = stim;
            head->prev = stim;
        } else {
            stim->prev = head->prev;
            head->prev->next = stim;
            head->prev = stim;
        }
        stim->next = NULL;
    }
}

static void stim_list_del(stim_handle_t timer) {
    stim_t *stim = timer;
    if (stim == head) {
        if (stim->next) {
            stim->next->prev = head->prev;
            head = stim->next;
        } else
            head = NULL;
    } else {
        stim->prev->next = stim->next;
        if (stim->next)
            stim->next->prev = stim->prev;
        else
            head->prev = stim->prev;
    }
    stim->prev = NULL;
    stim->next = NULL;
}

static int stim_cmd_push(stim_t *stim, uint32_t type) {
    uint32_t next_head;
    next_head = (stim_cmd_head + 1) % STIM_CMD_ARR_SIZE;
    if (next_head == stim_cmd_tail) {
        return -1;
    }
    stim_cmd_arr[stim_cmd_head].stim = stim;
    stim_cmd_arr[stim_cmd_head].type = type;
    stim_cmd_head = next_head;
    return 0;
}

void stim_start(stim_handle_t timer) {
    stim_t *stim = timer;
    if (!stim)
        return;
    STIM_ENTER_CRITICAL;
    stim_cmd_push(stim, STIM_CMD_START);
    STIM_EXIT_CRITICAL;
}

void stim_stop(stim_handle_t timer) {
    stim_t *stim = timer;
    if (!stim)
        return;
    STIM_ENTER_CRITICAL;
    stim_cmd_push(stim, STIM_CMD_STOP);
    STIM_EXIT_CRITICAL;
}

static void stim_cmd_handler(void) {
    stim_cmd_t cmd;
    while (1) {
        STIM_ENTER_CRITICAL;
        if (stim_cmd_head == stim_cmd_tail) {
            STIM_EXIT_CRITICAL;
            return;
        }
        cmd = stim_cmd_arr[stim_cmd_tail];
        stim_cmd_tail = (stim_cmd_tail + 1) % STIM_CMD_ARR_SIZE;
        STIM_EXIT_CRITICAL;
        switch (cmd.type) {
        case STIM_CMD_START:
            if (cmd.stim->state == STIM_DISABLE) {
                cmd.stim->state = STIM_ENABLE;
                cmd.stim->expiry_ticks = cmd.stim->period_ticks + stim_systicks;
                stim_list_add(cmd.stim);
            }
            break;
        case STIM_CMD_STOP:
            if (cmd.stim->state == STIM_ENABLE) {
                stim_list_del(cmd.stim);
                cmd.stim->state = STIM_DISABLE;
            }
            break;
        default:
            return;
        }
    }
}

static int stim_timer_handler(stim_handle_t timer, uint32_t curr_ticks) {
    stim_t *stim = timer;
    if ((int32_t)(stim->expiry_ticks - curr_ticks) <= 0) {
        ++stim->count;
        if (stim->cb)
            stim->cb(stim, stim->user_data);
        stim->expiry_ticks += stim->period_ticks;
        return 1;
    }
    return 0;
}

void stim_handler(void) {
    uint32_t curr_ticks;
    stim_t *expired;
    stim_cmd_handler();
    while (head) {
        curr_ticks = stim_systicks;
        if (!stim_timer_handler(head, curr_ticks))
            break;
        expired = head;
        stim_list_del(expired);
        if (expired->state == STIM_ENABLE)
            stim_list_add(expired);
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

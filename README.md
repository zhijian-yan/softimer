# Softimer

A lightweight software timer library for embedded systems.

Softimer provides a simple and efficient tick-based software timer implementation designed for bare-metal and RTOS environments.

Features:

- Tick-based software timer
- Sorted linked-list scheduling
- Deferred callback execution
- Immediate callback execution
- Asynchronous start/stop operations
- Event counting support
- Overflow-safe tick comparison
- No dynamic memory allocation
- Platform-independent locking abstraction

---

# Design

Softimer separates timer scheduling from callback execution.

```text
                SysTick ISR
                     |
                     |
             stim_tick_inc()
                     |
                     |
                Main loop
                     |
          +----------+----------+
          |                     |
          |                     |
     stim_poll()         stim_dispatch()
          |                     |
          |                     |
          |               Execute callback
          |
          +--> Process commands
          |
          +--> Check expiration
          |
          +--> Update timer state
          |
          +--> Generate timer events
                     |
                     |
         +-----------+-----------+
         |                       |
         |                       |
Immediate callback       Deferred queue
```

This architecture keeps ISR execution lightweight while allowing both low-latency callbacks and deferred callbacks.

---

# Callback Modes

Softimer supports two callback modes.

## STIM_MODE_DEFERRED

The callback is queued and later executed in:

```c
stim_dispatch();
```

Advantages:

- Safe for long operations
- Safe for blocking APIs
- Safe for printf()
- Safe for malloc()
- Suitable for application logic

Limitations:

- Callback timing depends on main loop execution speed

Example:

```c
stim_init(
    &timer,
    100,
    STIM_MODE_DEFERRED,
    timer_callback,
    NULL
);
```

---

## STIM_MODE_IMMEDIATE

The callback executes immediately inside:

```c
stim_poll();
```

Advantages:

- Minimal latency
- Event callbacks are not delayed
- Suitable for time-critical operations

Limitations:

- Must not execute long blocking operations

Avoid:

```c
void callback(stim_t *timer, void *arg)
{
    printf("Hello\n");

    delay_ms(100);

    malloc(128);
}
```

Recommended:

```c
void callback(stim_t *timer, void *arg)
{
    gpio_toggle();
}
```

---

# Usage

## Step 1: Create timer object

```c
stim_t led_timer;
```

---

## Step 2: Initialize timer

```c
stim_init(
    &led_timer,
    100,
    STIM_MODE_DEFERRED,
    led_callback,
    NULL
);
```

Parameters:

| Parameter | Description |
|------------|-------------|
| timer | Timer object |
| period_ticks | Timer period |
| mode | Callback mode |
| cb | Callback function |
| user_data | User private data |

---

## Step 3: Start timer

```c
stim_start(&led_timer);
```

---

## Step 4: Increment tick

Call periodically from a hardware timer interrupt:

```c
void SysTick_Handler(void)
{
    stim_tick_inc();
}
```

---

## Step 5: Poll timer system

Call periodically from main loop:

```c
while (1)
{
    stim_poll();

    stim_dispatch();
}
```

---

# Complete Example

```c
#include "softimer.h"

static stim_t led_timer;

static void led_callback(
    stim_t *timer,
    void *arg
)
{
    led_toggle();
}

void SysTick_Handler(void)
{
    stim_tick_inc();
}

int main(void)
{
    hardware_init();

    stim_init(
        &led_timer,
        100,
        STIM_MODE_DEFERRED,
        led_callback,
        NULL
    );

    stim_start(&led_timer);

    while (1)
    {
        stim_poll();

        stim_dispatch();
    }
}
```

---

# API Reference

## stim_init

Initialize a timer object.

```c
int stim_init(
    stim_t *timer,
    uint32_t period_ticks,
    stim_mode_t mode,
    stim_cb_t cb,
    void *user_data
);
```

Returns:

```c
0
```

Success

```c
-STIM_EINVAL
```

Invalid argument

---

## stim_start

Start timer asynchronously.

```c
int stim_start(
    stim_t *timer
);
```

Returns:

```c
0
```

Success

```c
-STIM_EAGAIN
```

Queue full

```c
-STIM_EINVAL
```

Invalid argument

---

## stim_stop

Stop timer asynchronously.

```c
int stim_stop(
    stim_t *timer
);
```

Returns:

```c
0
```

Success

```c
-STIM_EAGAIN
```

Queue full

```c
-STIM_EINVAL
```

Invalid argument

---

## stim_tick_inc

Increment internal tick counter.

Usually called from a hardware timer ISR.

```c
void stim_tick_inc(void);
```

---

## stim_get_ticks

Get current tick value.

```c
uint32_t stim_get_ticks(void);
```

---

## stim_poll

Check timer expiration and generate timer events.

Should be called periodically.

```c
int stim_poll(void);
```

Returns:

```c
0
```

Success

```c
-STIM_EAGAIN
```

Event queue full

---

## stim_dispatch

Execute deferred callbacks.

```c
void stim_dispatch(void);
```

---

## stim_get_count

Get timer trigger count.

```c
int stim_get_count(
    const stim_t *timer,
    uint32_t *count
);
```

---

## stim_set_count

Set timer trigger count.

```c
int stim_set_count(
    stim_t *timer,
    uint32_t count
);
```

---

# Notes

## Start/Stop operations are asynchronous

Calling:

```c
stim_start(&timer);

stim_stop(&timer);
```

does not immediately change timer state.

Actual state changes happen during:

```c
stim_poll();
```

because commands are processed through an internal queue.

---

## Queue size must be power of two

Current implementation uses:

```c
next = (index + 1) &
       (STIM_QUEUE_SIZE - 1);
```

Therefore valid values are:

```c
#define STIM_QUEUE_SIZE 2
#define STIM_QUEUE_SIZE 4
#define STIM_QUEUE_SIZE 8
#define STIM_QUEUE_SIZE 16
#define STIM_QUEUE_SIZE 32
```

Invalid examples:

```c
#define STIM_QUEUE_SIZE 3
#define STIM_QUEUE_SIZE 5
#define STIM_QUEUE_SIZE 10
```

---

## Tick overflow support

Softimer supports tick overflow handling through signed subtraction:

```c
if ((int32_t)(expire_ticks - now) <= 0)
```

This allows timer scheduling to continue correctly even after tick wrap-around.

---

## Callback restrictions

For:

```c
STIM_MODE_IMMEDIATE
```

callbacks execute inside:

```c
stim_poll()
```

Avoid recursive calls:

```c
void callback(
    stim_t *timer,
    void *arg
)
{
    stim_poll();
}
```

because recursive execution may occur.

---

# License

MIT License

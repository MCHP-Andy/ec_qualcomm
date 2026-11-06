/*
 * @Author: andy.chang 
 * @Date: 2026-04-18 18:01:20 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-21 16:16:55
 */

#pragma once

#include <stdio.h>
#include <stdint.h>

#define SYS_INIT_EARLY (0)
#define SYS_INIT_NORMAL (5)
#define SYS_INIT_LATE (10)

#define APP_STACK_MIN (512)
#define APP_STACK_NML (1024)
#define APP_STACK_MAX (2048)

#define APP_PRIO_H (0)
#define APP_PRIO_M (5)
#define APP_PRIO_L (10)

enum {
    SYS_EVT_PWR_STA_CHG = 0,

    
    SYS_EVT_MAX,
    LOCAL_EVT_START = SYS_EVT_MAX,
};

#define SYS_PWR_STA_CHG BIT(SYS_EVT_PWR_STA_CHG)

#define SYS_EVT_MASK (BIT(SYS_EVT_MAX)-1)

#include <zephyr/kernel.h>

struct sys_event_subscriber {
    struct k_event *pevent;
    const char *name;
};

#define SYS_EVENT_SUBSCRIBE(mname, ename)                                      \
    STRUCT_SECTION_ITERABLE(sys_event_subscriber,                              \
                            _##mname##_##ename##_sys_event_subscriber) = {     \
        .pevent = &ename,                                                      \
        .name = #mname,                                                        \
    }

#define SYS_EVENT_SUBMIT(evt)                                                  \
    do {                                                                       \
        uint32_t event = evt & SYS_EVT_MASK;                                   \
                                                                               \
        k_sched_lock();                                                        \
                                                                               \
        STRUCT_SECTION_FOREACH(sys_event_subscriber, p) {                      \
            if (p->pevent) {                                                   \
                k_event_post(p->pevent, event);                                \
            }                                                                  \
        }                                                                      \
                                                                               \
        k_sched_unlock();                                                      \
    } while (0)

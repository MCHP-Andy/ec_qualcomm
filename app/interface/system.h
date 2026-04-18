/*
 * @Author: andy.chang 
 * @Date: 2026-04-18 18:01:20 
 * @Last Modified by: andy.chang
 * @Last Modified time: 2026-04-18 18:15:10
 */

#pragma once

#define SYS_INIT_EARLY (0)
#define SYS_INIT_NORMAL (5)
#define SYS_INIT_LATE (10)

#define APP_STACK_MIN (512)
#define APP_STACK_NML (1024)
#define APP_STACK_MAX (2048)

#define APP_PRIO_H (0)
#define APP_PRIO_M (5)
#define APP_PRIO_L (10)

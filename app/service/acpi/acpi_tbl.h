/*
 * @Author: andy.chang 
 * @Date: 2026-04-21 21:46:34 
 * @Last Modified by:   andy.chang 
 * @Last Modified time: 2026-04-21 21:46:34 
 */

#pragma once

#define ACPI_CHECK_IN(c, cl, min)                                              \
    do {                                                                       \
        if ((c) == NULL || (cl) < (min)) {                                     \
            LOG_ERR("Invalid input: ptr=%p, len=%u (min=%u)", (void *)(c),     \
                    (unsigned int)(cl), (unsigned int)(min));                  \
            return -EINVAL;                                                    \
        }                                                                      \
    } while (0)

#define ACPI_CHECK_OUT(r, rl, min)                                             \
    do {                                                                       \
        if ((r) == NULL || (rl) < (min)) {                                     \
            LOG_ERR("Invalid output: ptr=%p, len=%u (min=%u)", (void *)(r),    \
                    (unsigned int)(rl), (unsigned int)(min));                  \
            return -EINVAL;                                                    \
        }                                                                      \
    } while (0)

#ifndef HUB_LOG_H
#define HUB_LOG_H

#include <stdarg.h>

typedef enum {
    HUB_LOG_ERROR = 0,
    HUB_LOG_WARN,
    HUB_LOG_INFO,
    HUB_LOG_DEBUG
} hub_log_level_t;

void hub_log_set_level(hub_log_level_t level);
void hub_log(hub_log_level_t level, const char *fmt, ...);
void hub_vlog(hub_log_level_t level, const char *fmt, va_list ap);

#define HUB_LOGE(...) hub_log(HUB_LOG_ERROR, __VA_ARGS__)
#define HUB_LOGW(...) hub_log(HUB_LOG_WARN, __VA_ARGS__)
#define HUB_LOGI(...) hub_log(HUB_LOG_INFO, __VA_ARGS__)
#define HUB_LOGD(...) hub_log(HUB_LOG_DEBUG, __VA_ARGS__)

#endif /* HUB_LOG_H */


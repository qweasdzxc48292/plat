#include "hub_log.h"

#include <stdio.h>
#include <time.h>

static hub_log_level_t g_level = HUB_LOG_INFO;

static const char *level_text(hub_log_level_t level)
{
    switch (level) {
    case HUB_LOG_ERROR:
        return "E";
    case HUB_LOG_WARN:
        return "W";
    case HUB_LOG_INFO:
        return "I";
    case HUB_LOG_DEBUG:
        return "D";
    default:
        return "?";
    }
}

void hub_log_set_level(hub_log_level_t level)
{
    g_level = level;
}

void hub_vlog(hub_log_level_t level, const char *fmt, va_list ap)
{
    time_t now;
    struct tm tm_info;
    char time_buf[32];

    if (level > g_level) {
        return;
    }

    now = time(NULL);
    localtime_r(&now, &tm_info);
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &tm_info);

    fprintf(stderr, "[%s][%s] ", time_buf, level_text(level));
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
}

void hub_log(hub_log_level_t level, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    hub_vlog(level, fmt, ap);
    va_end(ap);
}


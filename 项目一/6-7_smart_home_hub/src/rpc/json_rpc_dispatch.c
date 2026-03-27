#include "json_rpc_dispatch.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hub_log.h"

static const char *skip_ws(const char *s)
{
    while (*s && isspace((unsigned char)*s)) {
        ++s;
    }
    return s;
}

static int json_find_key(const char *json, const char *key, const char **out_val)
{
    char pattern[64];
    const char *p;

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    p = strstr(json, pattern);
    if (!p) {
        return -1;
    }

    p = strchr(p + strlen(pattern), ':');
    if (!p) {
        return -1;
    }

    *out_val = skip_ws(p + 1);
    return 0;
}

static int json_get_string(const char *json, const char *key, char *out, size_t out_sz)
{
    const char *p;
    size_t len = 0;

    if (json_find_key(json, key, &p) != 0 || *p != '"') {
        return -1;
    }
    ++p;

    while (p[len] && p[len] != '"' && len + 1 < out_sz) {
        out[len] = p[len];
        ++len;
    }
    if (p[len] != '"') {
        return -1;
    }
    out[len] = '\0';
    return 0;
}

static int json_get_id_raw(const char *json, char *out, size_t out_sz)
{
    const char *p;
    size_t len = 0;

    if (json_find_key(json, "id", &p) != 0) {
        snprintf(out, out_sz, "null");
        return 0;
    }

    while (p[len] && p[len] != ',' && p[len] != '}' && len + 1 < out_sz) {
        out[len] = p[len];
        ++len;
    }
    while (len > 0 && isspace((unsigned char)out[len - 1])) {
        --len;
    }
    out[len] = '\0';

    if (len == 0) {
        snprintf(out, out_sz, "null");
    }
    return 0;
}

static int json_get_int_key(const char *json, const char *key, int *value)
{
    const char *p;
    char *endp;
    long v;

    if (json_find_key(json, key, &p) != 0) {
        return -1;
    }

    v = strtol(p, &endp, 10);
    if (p == endp) {
        return -1;
    }

    *value = (int)v;
    return 0;
}

static int json_get_first_array_int(const char *json, const char *key, int *value)
{
    const char *p;
    char *endp;
    long v;

    if (json_find_key(json, key, &p) != 0) {
        return -1;
    }

    p = strchr(p, '[');
    if (!p) {
        return -1;
    }
    p = skip_ws(p + 1);
    v = strtol(p, &endp, 10);
    if (p == endp) {
        return -1;
    }

    *value = (int)v;
    return 0;
}

static int json_get_first_array_string(const char *json, const char *key, char *out, size_t out_sz)
{
    const char *p;
    size_t len = 0;

    if (json_find_key(json, key, &p) != 0) {
        return -1;
    }

    p = strchr(p, '[');
    if (!p) {
        return -1;
    }
    p = skip_ws(p + 1);
    if (*p != '"') {
        return -1;
    }
    ++p;

    while (p[len] && p[len] != '"' && len + 1 < out_sz) {
        out[len] = p[len];
        ++len;
    }
    if (p[len] != '"') {
        return -1;
    }
    out[len] = '\0';
    return 0;
}

static int make_error(char *resp, size_t resp_size, const char *id_raw, int code, const char *msg)
{
    return snprintf(resp, resp_size,
                    "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":%d,\"message\":\"%s\"},\"id\":%s}",
                    code, msg, id_raw);
}

int hub_json_rpc_dispatch(hub_rpc_context_t *ctx, const char *request, char *response, size_t response_size)
{
    char method[64];
    char id_raw[64];
    int led_on;
    int humi;
    int temp;
    char media_file[256];

    if (!ctx || !ctx->hal || !request || !response || response_size == 0) {
        return -1;
    }

    if (json_get_string(request, "method", method, sizeof(method)) != 0) {
        snprintf(response, response_size,
                 "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32600,\"message\":\"Invalid Request\"},\"id\":null}");
        return 0;
    }
    json_get_id_raw(request, id_raw, sizeof(id_raw));

    if (strcmp(method, "led_control") == 0) {
        if (json_get_first_array_int(request, "params", &led_on) != 0) {
            make_error(response, response_size, id_raw, -32602, "Invalid params");
            return 0;
        }
        if (hub_led_set(ctx->hal, led_on) != 0) {
            make_error(response, response_size, id_raw, -32001, "LED control failed");
            return 0;
        }
        snprintf(response, response_size, "{\"jsonrpc\":\"2.0\",\"result\":0,\"id\":%s}", id_raw);
        return 0;
    }

    if (strcmp(method, "control") == 0) {
        if (json_get_int_key(request, "LED1", &led_on) != 0) {
            make_error(response, response_size, id_raw, -32602, "Missing LED1");
            return 0;
        }
        if (hub_led_set(ctx->hal, led_on) != 0) {
            make_error(response, response_size, id_raw, -32001, "LED control failed");
            return 0;
        }
        snprintf(response, response_size,
                 "{\"jsonrpc\":\"2.0\",\"result\":{\"LED1\":%d},\"id\":%s}", led_on, id_raw);
        return 0;
    }

    if (strcmp(method, "dht11_read") == 0) {
        if (hub_dht11_read(ctx->hal, &humi, &temp) != 0) {
            make_error(response, response_size, id_raw, -32002, "Sensor read failed");
            return 0;
        }
        snprintf(response, response_size,
                 "{\"jsonrpc\":\"2.0\",\"result\":[%d,%d],\"id\":%s}", humi, temp, id_raw);
        return 0;
    }

    if (strcmp(method, "media_play") == 0) {
        if (!ctx->media) {
            make_error(response, response_size, id_raw, -32003, "Media service unavailable");
            return 0;
        }
        media_file[0] = '\0';
        if (json_get_string(request, "file", media_file, sizeof(media_file)) != 0) {
            (void)json_get_first_array_string(request, "params", media_file, sizeof(media_file));
        }
        if (hub_media_player_play(ctx->media, media_file[0] ? media_file : NULL) != 0) {
            make_error(response, response_size, id_raw, -32003, "Media play failed");
            return 0;
        }
        snprintf(response, response_size, "{\"jsonrpc\":\"2.0\",\"result\":0,\"id\":%s}", id_raw);
        return 0;
    }

    if (strcmp(method, "media_stop") == 0) {
        if (!ctx->media) {
            make_error(response, response_size, id_raw, -32003, "Media service unavailable");
            return 0;
        }
        if (hub_media_player_stop(ctx->media) != 0) {
            make_error(response, response_size, id_raw, -32003, "Media stop failed");
            return 0;
        }
        snprintf(response, response_size, "{\"jsonrpc\":\"2.0\",\"result\":0,\"id\":%s}", id_raw);
        return 0;
    }

    HUB_LOGW("unknown rpc method: %s", method);
    make_error(response, response_size, id_raw, -32601, "Method not found");
    return 0;
}

#include <stdio.h>

#include "plugin_manager.h"

static int g_tick_count = 0;

static int sample_on_load(void *context)
{
    (void)context;
    g_tick_count = 0;
    fprintf(stdout, "[sample_backend_plugin] loaded\n");
    return 0;
}

static int sample_on_tick(void *context)
{
    (void)context;
    g_tick_count++;
    if ((g_tick_count % 8) == 0) {
        fprintf(stdout, "[sample_backend_plugin] tick=%d\n", g_tick_count);
    }
    return 0;
}

static void sample_on_unload(void *context)
{
    (void)context;
    fprintf(stdout, "[sample_backend_plugin] unloaded\n");
}

const edt_backend_plugin_api_t *edt_backend_plugin_get_api(void)
{
    static const edt_backend_plugin_api_t api = {
        .name = "sample_backend_plugin",
        .on_load = sample_on_load,
        .on_tick = sample_on_tick,
        .on_unload = sample_on_unload,
    };
    return &api;
}

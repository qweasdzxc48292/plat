#ifndef EDT_PLUGIN_MANAGER_H
#define EDT_PLUGIN_MANAGER_H

#include <stddef.h>

typedef struct {
    const char *name;
    int (*on_load)(void *context);
    int (*on_tick)(void *context);
    void (*on_unload)(void *context);
} edt_backend_plugin_api_t;

typedef const edt_backend_plugin_api_t *(*edt_backend_plugin_get_api_fn)(void);

typedef struct {
    char path[256];
    char name[64];
    void *dl_handle;
    const edt_backend_plugin_api_t *api;
} edt_plugin_slot_t;

typedef struct {
    edt_plugin_slot_t *slots;
    size_t slot_count;
    void *context;
} edt_plugin_manager_t;

int edt_plugin_manager_init(edt_plugin_manager_t *manager, size_t max_plugins, void *context);
int edt_plugin_manager_load_dir(edt_plugin_manager_t *manager, const char *plugin_dir);
int edt_plugin_manager_tick(edt_plugin_manager_t *manager);
void edt_plugin_manager_unload_all(edt_plugin_manager_t *manager);
void edt_plugin_manager_destroy(edt_plugin_manager_t *manager);

#endif

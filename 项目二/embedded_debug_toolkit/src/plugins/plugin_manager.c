#include "plugin_manager.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__linux__)
#include <dirent.h>
#include <dlfcn.h>
#endif

int edt_plugin_manager_init(edt_plugin_manager_t *manager, size_t max_plugins, void *context)
{
    if (!manager || max_plugins == 0U) {
        return -EINVAL;
    }
    memset(manager, 0, sizeof(*manager));
    manager->slots = (edt_plugin_slot_t *)calloc(max_plugins, sizeof(edt_plugin_slot_t));
    if (!manager->slots) {
        return -ENOMEM;
    }
    manager->slot_count = max_plugins;
    manager->context = context;
    return 0;
}

static int edt_plugin_manager_add(edt_plugin_manager_t *manager, const char *path)
{
#if !defined(__linux__)
    (void)manager;
    (void)path;
    return -ENOSYS;
#else
    size_t i;
    void *handle;
    edt_backend_plugin_get_api_fn get_api;
    const edt_backend_plugin_api_t *api;

    if (!manager || !path) {
        return -EINVAL;
    }

    for (i = 0U; i < manager->slot_count; i++) {
        if (!manager->slots[i].dl_handle) {
            break;
        }
    }
    if (i >= manager->slot_count) {
        return -ENOSPC;
    }

    handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        return -EINVAL;
    }

    get_api = (edt_backend_plugin_get_api_fn)dlsym(handle, "edt_backend_plugin_get_api");
    if (!get_api) {
        dlclose(handle);
        return -EINVAL;
    }
    api = get_api();
    if (!api || !api->name) {
        dlclose(handle);
        return -EINVAL;
    }
    if (api->on_load && api->on_load(manager->context) != 0) {
        dlclose(handle);
        return -EINVAL;
    }

    manager->slots[i].dl_handle = handle;
    manager->slots[i].api = api;
    (void)snprintf(manager->slots[i].path, sizeof(manager->slots[i].path), "%s", path);
    (void)snprintf(manager->slots[i].name, sizeof(manager->slots[i].name), "%s", api->name);
    return 0;
#endif
}

int edt_plugin_manager_load_dir(edt_plugin_manager_t *manager, const char *plugin_dir)
{
#if !defined(__linux__)
    (void)manager;
    (void)plugin_dir;
    return -ENOSYS;
#else
    DIR *dir;
    struct dirent *entry;

    if (!manager || !plugin_dir) {
        return -EINVAL;
    }

    dir = opendir(plugin_dir);
    if (!dir) {
        return -errno;
    }

    while ((entry = readdir(dir)) != NULL) {
        char full_path[512];
        size_t n = strlen(entry->d_name);
        if (n < 4U) {
            continue;
        }
        if (strcmp(entry->d_name + n - 3U, ".so") != 0) {
            continue;
        }
        (void)snprintf(full_path, sizeof(full_path), "%s/%s", plugin_dir, entry->d_name);
        (void)edt_plugin_manager_add(manager, full_path);
    }
    (void)closedir(dir);
    return 0;
#endif
}

int edt_plugin_manager_tick(edt_plugin_manager_t *manager)
{
    size_t i;
    if (!manager) {
        return -EINVAL;
    }
    for (i = 0U; i < manager->slot_count; i++) {
        if (manager->slots[i].api && manager->slots[i].api->on_tick) {
            (void)manager->slots[i].api->on_tick(manager->context);
        }
    }
    return 0;
}

void edt_plugin_manager_unload_all(edt_plugin_manager_t *manager)
{
    if (!manager || !manager->slots) {
        return;
    }
    for (size_t i = 0U; i < manager->slot_count; i++) {
        if (manager->slots[i].api && manager->slots[i].api->on_unload) {
            manager->slots[i].api->on_unload(manager->context);
        }
#if defined(__linux__)
        if (manager->slots[i].dl_handle) {
            dlclose(manager->slots[i].dl_handle);
        }
#endif
        memset(&manager->slots[i], 0, sizeof(manager->slots[i]));
    }
}

void edt_plugin_manager_destroy(edt_plugin_manager_t *manager)
{
    if (!manager) {
        return;
    }
    edt_plugin_manager_unload_all(manager);
    free(manager->slots);
    manager->slots = NULL;
    manager->slot_count = 0U;
}

#ifndef JSON_RPC_DISPATCH_H
#define JSON_RPC_DISPATCH_H

#include <stddef.h>

#include "device_hal.h"
#include "media_player.h"

typedef struct hub_rpc_context {
    hub_device_hal_t *hal;
    hub_media_player_t *media;
} hub_rpc_context_t;

int hub_json_rpc_dispatch(hub_rpc_context_t *ctx, const char *request, char *response, size_t response_size);

#endif /* JSON_RPC_DISPATCH_H */

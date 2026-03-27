#include <stdio.h>
#include <string.h>

#include "device_hal.h"
#include "hub_config.h"
#include "hub_log.h"

static void usage(const char *prog)
{
    printf("Usage:\n");
    printf("  %s [config.json] led on|off|get\n", prog);
    printf("  %s [config.json] dht\n", prog);
}

int main(int argc, char **argv)
{
    const char *cfg_path = "config/hub_config.json";
    const char *cmd = NULL;
    hub_config_t cfg;
    hub_device_hal_t hal;
    int led = 0;
    int humi = 0;
    int temp = 0;

    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    if (strstr(argv[1], ".json")) {
        cfg_path = argv[1];
        if (argc < 3) {
            usage(argv[0]);
            return 1;
        }
        cmd = argv[2];
    } else {
        cmd = argv[1];
    }

    hub_log_set_level(HUB_LOG_INFO);
    if (hub_config_load(cfg_path, &cfg) != 0) {
        printf("load config failed: %s\n", cfg_path);
        return 1;
    }
    if (hub_device_hal_init(&hal, &cfg) != 0) {
        printf("hal init failed\n");
        return 1;
    }

    if (strcmp(cmd, "led") == 0) {
        int arg_index = strstr(argv[1], ".json") ? 3 : 2;
        const char *arg = (argc > arg_index) ? argv[arg_index] : NULL;
        if (!arg) {
            usage(argv[0]);
            hub_device_hal_deinit(&hal);
            return 1;
        }
        if (strcmp(arg, "on") == 0) {
            (void)hub_led_set(&hal, 1);
            printf("LED set on\n");
        } else if (strcmp(arg, "off") == 0) {
            (void)hub_led_set(&hal, 0);
            printf("LED set off\n");
        } else if (strcmp(arg, "get") == 0) {
            if (hub_led_get(&hal, &led) == 0) {
                printf("LED=%d\n", led);
            } else {
                printf("LED get failed\n");
            }
        } else {
            usage(argv[0]);
        }
    } else if (strcmp(cmd, "dht") == 0) {
        if (hub_dht11_read(&hal, &humi, &temp) == 0) {
            printf("DHT11 temp=%d humi=%d\n", temp, humi);
        } else {
            printf("DHT11 read failed\n");
        }
    } else {
        usage(argv[0]);
    }

    hub_device_hal_deinit(&hal);
    return 0;
}

#include "mqtt_service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifdef HUB_USE_PAHO
#include "MQTTClient.h"
#endif

#include "hub_log.h"

typedef struct hub_mqtt_impl {
#ifdef HUB_USE_PAHO
    MQTTClient client;
    int client_created;
    int connected;
#endif
    int reconnect_backoff;
    uint64_t last_report_ms;
} hub_mqtt_impl_t;

static int clamp_qos(int qos, int fallback)
{
    if (qos < 0 || qos > 2) {
        return fallback;
    }
    return qos;
}

static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static void sleep_ms(unsigned int ms)
{
    usleep((useconds_t)ms * 1000U);
}

int hub_mqtt_service_publish(hub_mqtt_service_t *svc, const char *topic, const char *payload, int qos)
{
    if (!svc || !topic || !payload) {
        return -1;
    }

#ifdef HUB_USE_PAHO
    {
        hub_mqtt_impl_t *impl = (hub_mqtt_impl_t *)svc->impl;
        MQTTClient_message msg = MQTTClient_message_initializer;
        MQTTClient_deliveryToken token;
        int rc;

        if (!impl || !impl->connected) {
            return -1;
        }

        if (qos < 0) {
            qos = 0;
        } else if (qos > 2) {
            qos = 2;
        }

        msg.payload = (void *)payload;
        msg.payloadlen = (int)strlen(payload);
        msg.qos = qos;
        msg.retained = 0;

        rc = MQTTClient_publishMessage(impl->client, topic, &msg, &token);
        if (rc != MQTTCLIENT_SUCCESS) {
            impl->connected = 0;
            HUB_LOGW("MQTT publish failed rc=%d", rc);
            return -1;
        }
        (void)MQTTClient_waitForCompletion(impl->client, token, 3000L);
    }
#else
    (void)qos;
    HUB_LOGI("[MOCK MQTT] publish topic=%s payload=%s", topic, payload);
#endif
    return 0;
}

static void publish_sensor_report(hub_mqtt_service_t *svc)
{
    int humi = 0;
    int temp = 0;
    char payload[256];

    if (svc->hal) {
        (void)hub_dht11_read(svc->hal, &humi, &temp);
    }

    snprintf(payload, sizeof(payload),
             "{\"method\":\"report\",\"params\":{\"temp_value\":%d,\"humi_value\":%d},\"timestamp\":%llu}",
             temp, humi, (unsigned long long)time(NULL));

    (void)hub_mqtt_service_publish(svc, svc->config.mqtt_topic_up, payload,
                                   clamp_qos(svc->config.mqtt_qos_report, 0));
}

#ifdef HUB_USE_PAHO
static void paho_delivered(void *context, MQTTClient_deliveryToken dt)
{
    (void)context;
    (void)dt;
}

static void paho_connlost(void *context, char *cause)
{
    hub_mqtt_service_t *svc = (hub_mqtt_service_t *)context;
    hub_mqtt_impl_t *impl = (hub_mqtt_impl_t *)svc->impl;
    if (impl) {
        impl->connected = 0;
    }
    HUB_LOGW("MQTT connection lost: %s", cause ? cause : "unknown");
}

static int paho_msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    hub_mqtt_service_t *svc = (hub_mqtt_service_t *)context;
    char *payload = NULL;
    char response[512] = {0};
    int response_len = 0;

    (void)topicLen;
    if (!svc || !message) {
        return 1;
    }

    payload = (char *)calloc(1, (size_t)message->payloadlen + 1);
    if (payload) {
        memcpy(payload, message->payload, (size_t)message->payloadlen);
    }

    if (payload && svc->on_message) {
        response_len = svc->on_message(topicName ? topicName : "", payload,
                                       response, sizeof(response), svc->cb_user);
        if (response_len >= 0 && response[0] != '\0') {
            (void)hub_mqtt_service_publish(svc, svc->config.mqtt_topic_resp, response,
                                           clamp_qos(svc->config.mqtt_qos_response, 1));
        }
    }

    free(payload);
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

static int paho_prepare_client(hub_mqtt_service_t *svc)
{
    hub_mqtt_impl_t *impl = (hub_mqtt_impl_t *)svc->impl;
    char address[256];
    int rc;

    if (!impl) {
        return -1;
    }
    if (impl->client_created) {
        return 0;
    }

    snprintf(address, sizeof(address), "tcp://%s:%d", svc->config.mqtt_host, svc->config.mqtt_port);
    rc = MQTTClient_create(&impl->client, address, svc->config.mqtt_client_id,
                           MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (rc != MQTTCLIENT_SUCCESS) {
        HUB_LOGE("MQTTClient_create failed rc=%d", rc);
        return -1;
    }

    rc = MQTTClient_setCallbacks(impl->client, svc, paho_connlost, paho_msgarrvd, paho_delivered);
    if (rc != MQTTCLIENT_SUCCESS) {
        HUB_LOGE("MQTTClient_setCallbacks failed rc=%d", rc);
        MQTTClient_destroy(&impl->client);
        return -1;
    }

    impl->client_created = 1;
    return 0;
}

static int paho_connect_once(hub_mqtt_service_t *svc)
{
    hub_mqtt_impl_t *impl = (hub_mqtt_impl_t *)svc->impl;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;

    if (paho_prepare_client(svc) != 0) {
        return -1;
    }

    conn_opts.keepAliveInterval = svc->config.mqtt_keepalive_sec;
    conn_opts.cleansession = 1;
    if (svc->config.mqtt_username[0]) {
        conn_opts.username = svc->config.mqtt_username;
    }
    if (svc->config.mqtt_password[0]) {
        conn_opts.password = svc->config.mqtt_password;
    }

    rc = MQTTClient_connect(impl->client, &conn_opts);
    if (rc != MQTTCLIENT_SUCCESS) {
        HUB_LOGW("MQTT connect failed rc=%d", rc);
        return -1;
    }

    rc = MQTTClient_subscribe(impl->client, svc->config.mqtt_topic_down,
                              clamp_qos(svc->config.mqtt_qos_control, 1));
    if (rc != MQTTCLIENT_SUCCESS) {
        HUB_LOGW("MQTT subscribe failed rc=%d", rc);
        (void)MQTTClient_disconnect(impl->client, 1000);
        return -1;
    }

    impl->connected = 1;
    HUB_LOGI("MQTT connected to %s:%d", svc->config.mqtt_host, svc->config.mqtt_port);
    return 0;
}
#endif

static void *mqtt_thread_main(void *arg)
{
    hub_mqtt_service_t *svc = (hub_mqtt_service_t *)arg;
    hub_mqtt_impl_t *impl = (hub_mqtt_impl_t *)svc->impl;
    int report_ms;

    report_ms = svc->config.report_interval_sec > 0 ? svc->config.report_interval_sec * 1000 : 5000;
    impl->last_report_ms = now_ms();

    while (svc->running) {
#ifdef HUB_USE_PAHO
        if (!impl->connected) {
            if (paho_connect_once(svc) != 0) {
                int delay = impl->reconnect_backoff > 0 ? impl->reconnect_backoff : 1;
                if (delay > 60) {
                    delay = 60;
                }
                sleep((unsigned int)delay);
                impl->reconnect_backoff = delay < 32 ? delay * 2 : 32;
                continue;
            }
            impl->reconnect_backoff = svc->config.mqtt_reconnect_sec > 0 ? svc->config.mqtt_reconnect_sec : 1;
        } else {
            MQTTClient_yield();
            if (!MQTTClient_isConnected(impl->client)) {
                impl->connected = 0;
                continue;
            }
        }
#endif

        if ((int)(now_ms() - impl->last_report_ms) >= report_ms) {
            publish_sensor_report(svc);
            impl->last_report_ms = now_ms();
        }
        sleep_ms(100);
    }

#ifdef HUB_USE_PAHO
    if (impl->client_created) {
        if (impl->connected) {
            (void)MQTTClient_disconnect(impl->client, 1000);
            impl->connected = 0;
        }
        MQTTClient_destroy(&impl->client);
        impl->client_created = 0;
    }
#endif
    return NULL;
}

int hub_mqtt_service_start(hub_mqtt_service_t *svc, const hub_config_t *cfg, hub_device_hal_t *hal,
                           hub_mqtt_message_cb_t cb, void *cb_user)
{
    hub_mqtt_impl_t *impl;

    if (!svc || !cfg) {
        return -1;
    }

    memset(svc, 0, sizeof(*svc));
    svc->config = *cfg;
    svc->hal = hal;
    svc->on_message = cb;
    svc->cb_user = cb_user;

    impl = (hub_mqtt_impl_t *)calloc(1, sizeof(*impl));
    if (!impl) {
        return -1;
    }
    impl->reconnect_backoff = cfg->mqtt_reconnect_sec > 0 ? cfg->mqtt_reconnect_sec : 1;
    svc->impl = impl;

    svc->running = 1;
    if (pthread_create(&svc->thread, NULL, mqtt_thread_main, svc) != 0) {
        svc->running = 0;
        free(impl);
        svc->impl = NULL;
        return -1;
    }

    HUB_LOGI("mqtt service started");
    return 0;
}

void hub_mqtt_service_stop(hub_mqtt_service_t *svc)
{
    if (!svc || !svc->impl) {
        return;
    }

    svc->running = 0;
    if (svc->thread) {
        pthread_join(svc->thread, NULL);
        svc->thread = 0;
    }

    free(svc->impl);
    svc->impl = NULL;
    HUB_LOGI("mqtt service stopped");
}

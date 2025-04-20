#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/wifi_credentials.h>
#include "zephyr/kernel.h"
#include "zephyr/net/net_if.h"
#include "zephyr/net/wifi.h"
#include "zephyr/sys/time_units.h"
LOG_MODULE_REGISTER(rw612_hello, LOG_LEVEL_INF);
#define event_mask (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)
static struct wifi_connect_req_params wifi_connect_params;
static struct net_mgmt_event_callback wifi_mgmt_cb;
static bool connected = false;
static K_SEM_DEFINE(run_app, 0, 1);

void wifi_mgmt_event_handler(struct net_mgmt_event_callback* cb,
                             uint32_t mgmt_event,
                             struct net_if* iface) {
    if ((mgmt_event & event_mask) != mgmt_event) {
        return;
    }
    if (mgmt_event == NET_EVENT_L4_CONNECTED) {
        LOG_INF("Connected to Wi-Fi");
        connected = true;
        k_sem_give(&run_app);
        return;
    }
    if (mgmt_event == NET_EVENT_L4_DISCONNECTED) {
        if (connected == false) {
            LOG_INF("waiting for connection");
        } else {
            LOG_INF("Disconnected from Wi-Fi");
            connected = false;
        }
        k_sem_reset(&run_app);
        return;
    }
};
#if CONFIG_WIFI_CREDENTIALS_STATIC

int wifi_args_to_params(struct wifi_connect_req_params* params) {
    if (params == NULL) {
        return -1;
    }
    // Set the SSID and password
    params->ssid        = CONFIG_WIFI_CREDENTIALS_STATIC_SSID;
    params->ssid_length = strlen(CONFIG_WIFI_CREDENTIALS_STATIC_SSID);

    params->psk        = CONFIG_WIFI_CREDENTIALS_STATIC_PASSWORD;
    params->psk_length = strlen(CONFIG_WIFI_CREDENTIALS_STATIC_PASSWORD);

    params->channel  = WIFI_CHANNEL_ANY;
    params->security = WIFI_SECURITY_TYPE_PSK;
    params->mfp      = WIFI_MFP_OPTIONAL;
    params->band     = WIFI_FREQ_BAND_UNKNOWN;
    params->timeout  = SYS_FOREVER_MS;
    memset(params->bssid, 0, sizeof(params->bssid));
    return 0;
}

#endif
int main(void) {
    printf("Hello World! %s\n", CONFIG_BOARD_TARGET);
    net_mgmt_init_event_callback(&wifi_mgmt_cb, wifi_mgmt_event_handler, event_mask);
    net_mgmt_add_event_callback(&wifi_mgmt_cb);
    struct net_if* iface = net_if_get_default();
    if (iface == NULL) {
        LOG_ERR("Failed to get default network interface");
        return -1;
    }
#if CONFIG_WIFI_CREDENTIALS_STATIC

    wifi_args_to_params(&wifi_connect_params);

    int err = net_mgmt(NET_REQUEST_WIFI_CONNECT,
                       iface,
                       &wifi_connect_params,
                       sizeof(struct wifi_connect_req_params));
    if (err) {
        LOG_ERR("Failed to connect to Wi-Fi: %d", err);
        return err;
    }
#endif
    int rc = net_mgmt(NET_REQUEST_WIFI_CONNECT_STORED, iface, NULL, 0);

    if (rc) {
        LOG_ERR("An error occurred when trying to auto-connect to a network. err: %d", rc);
    }
    k_sem_take(&run_app, K_FOREVER);
    LOG_INF("run_app started");
    return 0;
}
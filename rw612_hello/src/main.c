#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/wifi_credentials.h>
#include "arpa/inet.h"
#include "netdb.h"
#include "zephyr/kernel.h"
#include "zephyr/net/net_if.h"
#include "zephyr/net/net_ip.h"
#include "zephyr/net/wifi.h"
#include "zephyr/sys/time_units.h"

#ifndef __ZEPHYR__

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#else

#include <zephyr/net/socket.h>

#endif

/************************for tcp echo server********************** */
#define BIND_PORT 4242

/************************for wifi connection********************** */
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

    /*************************address convention*********************** */
    struct sockaddr_in sa;   // IPv4
    struct sockaddr_in6 sa6; // IPv6
    char ip4_str[INET_ADDRSTRLEN];
    char ip6_str[INET6_ADDRSTRLEN];

    inet_pton(AF_INET, "10.12.110.57", &(sa.sin_addr));             // IPv4
    inet_pton(AF_INET6, "2001:db8:63b3:1::3490", &(sa6.sin6_addr)); // IPv6
    inet_ntop(AF_INET, &(sa.sin_addr), ip4_str, INET_ADDRSTRLEN);
    inet_ntop(AF_INET6, &(sa6.sin6_addr), ip6_str, INET6_ADDRSTRLEN);
    LOG_INF("IPv4 address: %s", ip4_str);
    LOG_INF("IPv6 address: %s", ip6_str);

    /*************************DNA resolve example*********************** */
    {
#define SERVER_HOSTNAME "www.baidu.com"
#define SERVER_PORT     "https"
        int ret;
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_family   = AF_INET; // AF_INET or AF_INET6
        /**servname is bassicly the port number or type of protocal in string format */
        ret = getaddrinfo(SERVER_HOSTNAME, NULL, &hints, &res);
        if (ret != 0) {
            LOG_ERR("getaddrinfo failed: %d", ret);
            return ret;
        }
        struct addrinfo* p = res;
        int dns_rse_inx    = 0;
        while (p != NULL) {
            LOG_INF("dns_rse_inx: %d", dns_rse_inx++);
            char ip4str[INET_ADDRSTRLEN]  = {0};
            char ip6str[INET6_ADDRSTRLEN] = {0};
            if (p->ai_family == AF_INET) {
                struct sockaddr_in* addr4 = (struct sockaddr_in*)p->ai_addr;
                addr4->sin_family         = AF_INET;
                inet_ntop(AF_INET, &(addr4->sin_addr), ip4str, INET_ADDRSTRLEN);
                LOG_INF("IPv4 address: %s", ip4str);
            }
            if (p->ai_family == AF_INET6) {
                struct sockaddr_in6* addr6 = (struct sockaddr_in6*)p->ai_addr;
                inet_ntop(AF_INET6, &(addr6->sin6_addr), ip6str, INET6_ADDRSTRLEN);
                LOG_INF("IPv6 address: %s", ip6str);
            }
            p = p->ai_next;
        }
    }
    return 0;
}
/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/modem/chat.h>
#include <zephyr/modem/pipelink.h>

LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL);

#define EG25G_MODEM_NODE    DT_ALIAS(modem)
#define EG25G_PIPELINK_NAME _CONCAT(user_pipe_, 0)

const struct device* modem = DEVICE_DT_GET(DT_ALIAS(modem));

MODEM_PIPELINK_DT_DECLARE(EG25G_MODEM_NODE, EG25G_PIPELINK_NAME);
static struct modem_pipelink* eg25g_pipelink = MODEM_PIPELINK_DT_GET(EG25G_MODEM_NODE,
                                                                     EG25G_PIPELINK_NAME);
uint8_t eg25g_chat_r_buf[128];
uint8_t* eg25g_chat_arg_buf[32];

static struct modem_chat eg25g_chat;
static struct modem_chat_config eg25g_chat_config = {
    .receive_buf      = eg25g_chat_r_buf,
    .receive_buf_size = sizeof(eg25g_chat_r_buf),
    .delimiter        = "\r",
    .delimiter_size   = 1,
    .filter           = "\n",
    .filter_size      = 1,
    .argv             = eg25g_chat_arg_buf,
    .argv_size        = ARRAY_SIZE(eg25g_chat_arg_buf),
};

static void modem_cellular_chat_callback_handler(struct modem_chat* chat,
                                                 enum modem_chat_script_result result,
                                                 void* user_data);
static void modem_cellular_chat_connect_callback_handler(struct modem_chat* chat,
                                                         enum modem_chat_script_result result,
                                                         void* user_data);

MODEM_CHAT_MATCH_DEFINE(ok_match, "OK", "", NULL);
MODEM_CHAT_MATCH_DEFINE(connect_match, "CONNECT", "", NULL);
MODEM_CHAT_MATCHES_DEFINE(abort_matches, MODEM_CHAT_MATCH("ERROR", "", NULL));
MODEM_CHAT_MATCHES_DEFINE(alow_matches,
                          MODEM_CHAT_MATCH("OK", "", NULL),
                          MODEM_CHAT_MATCH("ERROR", "", NULL));

MODEM_CHAT_SCRIPT_CMDS_DEFINE(eg25g_init_chat_script_cmds,
                              MODEM_CHAT_SCRIPT_CMD_RESP("AT&F", ok_match),
                              MODEM_CHAT_SCRIPT_CMD_RESP("ATE0", ok_match),
                              //   MODEM_CHAT_SCRIPT_CMD_RESP("AT+CFUN=4", ok_match),
                              MODEM_CHAT_SCRIPT_CMD_RESP("AT+CMEE=1", ok_match),
                              MODEM_CHAT_SCRIPT_CMD_RESP("AT+CREG=1", ok_match),
                              MODEM_CHAT_SCRIPT_CMD_RESP("AT+CGREG=1", ok_match),
                              MODEM_CHAT_SCRIPT_CMD_RESP("AT+CEREG=1", ok_match),
                              MODEM_CHAT_SCRIPT_CMD_RESP("AT+CREG?", ok_match),
                              MODEM_CHAT_SCRIPT_CMD_RESP("AT+CEREG?", ok_match),
                              MODEM_CHAT_SCRIPT_CMD_RESP("AT+CGREG?", ok_match),
                              MODEM_CHAT_SCRIPT_CMD_RESP("AT+CSQ", ok_match),
                              MODEM_CHAT_SCRIPT_CMD_RESP("AT+CPIN?", ok_match));
//   MODEM_CHAT_SCRIPT_CMD_RESP("AT+CGSN", imei_match),
//   MODEM_CHAT_SCRIPT_CMD_RESP("", ok_match),
//   MODEM_CHAT_SCRIPT_CMD_RESP("AT+CGMM", cgmm_match),
//   MODEM_CHAT_SCRIPT_CMD_RESP("", ok_match),
//   MODEM_CHAT_SCRIPT_CMD_RESP("AT+CGMI", cgmi_match),备忘AT+QIOPEN=1,0,"TCP","220.180.239.212",8009,0,2
//   MODEM_CHAT_SCRIPT_CMD_RESP("", ok_match),
//   MODEM_CHAT_SCRIPT_CMD_RESP("AT+CGMR", cgmr_match),
//   MODEM_CHAT_SCRIPT_CMD_RESP("", ok_match),
//   MODEM_CHAT_SCRIPT_CMD_RESP("AT+CIMI", cimi_match),
//   MODEM_CHAT_SCRIPT_CMD_RESP("", ok_match),
//   MODEM_CHAT_SCRIPT_CMD_RESP_NONE("AT+CMUX=0,0,5,127,10,3,30,10,2",
//                                   100));

MODEM_CHAT_SCRIPT_DEFINE(eg25g_init_chat_script,
                         eg25g_init_chat_script_cmds,
                         abort_matches,
                         modem_cellular_chat_callback_handler,
                         20);

MODEM_CHAT_SCRIPT_CMDS_DEFINE(
    eg25g_connect_chat_script_cmds,
    // MODEM_CHAT_SCRIPT_CMD_RESP("AT+CFUN=1", ok_match),
    // MODEM_CHAT_SCRIPT_CMD_RESP("AT+CREG?", ok_match),
    // MODEM_CHAT_SCRIPT_CMD_RESP("AT+CGDCONT=1,\"IP\",\"cmiot\"", ok_match),
    // MODEM_CHAT_SCRIPT_CMD_RESP_MULT("AT+CGACT=1,1", alow_matches),
    // MODEM_CHAT_SCRIPT_CMD_RESP("AT+CSQ", ok_match),
    MODEM_CHAT_SCRIPT_CMD_RESP("AT+QICSGP=1,1,\"cmiot\",\"\",\"\",0", ok_match),
    MODEM_CHAT_SCRIPT_CMD_RESP("AT+QIDEACT=1", ok_match),
    MODEM_CHAT_SCRIPT_CMD_RESP("AT+QIACT=1", ok_match),
    MODEM_CHAT_SCRIPT_CMD_RESP("AT+QIOPEN=1,0,\"TCP\",\"112.125.89.8\",46650,0,2", connect_match));

MODEM_CHAT_SCRIPT_DEFINE(eg25g_connect_chat_script,
                         eg25g_connect_chat_script_cmds,
                         abort_matches,
                         modem_cellular_chat_connect_callback_handler,
                         20);

MODEM_CHAT_SCRIPT_CMDS_DEFINE(eg25g_periodic_chat_script_cmds,
                              MODEM_CHAT_SCRIPT_CMD_RESP("AT+CREG?", ok_match),
                              MODEM_CHAT_SCRIPT_CMD_RESP("AT+CEREG?", ok_match),
                              MODEM_CHAT_SCRIPT_CMD_RESP("AT+CGREG?", ok_match),
                              MODEM_CHAT_SCRIPT_CMD_RESP("AT+CSQ", ok_match));

MODEM_CHAT_SCRIPT_DEFINE(eg25g_periodic_chat_script,
                         eg25g_periodic_chat_script_cmds,
                         abort_matches,
                         modem_cellular_chat_callback_handler,
                         20);
static struct k_work eg25g_pipe_open_work;

// static void eg25g_pipe_open_handler(struct k_work* work);

int main(void) {
    int ret;
    if (!device_is_ready(modem)) {
        printk("Modem device not ready\n");
        return -1;
    }
    printk("Powering on modem\n");
    k_msleep(150);
    // pm_device_action_run(modem, PM_DEVICE_ACTION_TURN_ON);
    // pm_device_action_run(modem, PM_DEVICE_ACTION_SUSPEND);
    // k_msleep(3);
    pm_device_action_run(modem, PM_DEVICE_ACTION_RESUME);
    k_msleep(10);
    // k_work_init(&eg25g_pipe_open_work, eg25g_pipe_open_handler);
    modem_chat_init(&eg25g_chat, &eg25g_chat_config);
    // modem_pipelink_attach(eg25g_pipelink, modem_pipelink_callback callback, );
    ret = modem_pipe_open(modem_pipelink_get_pipe(eg25g_pipelink), K_SECONDS(10));
    if (ret < 0) {
        printk("Failed to open modem pipe\n");
        return ret;
    }
    ret = modem_chat_attach(&eg25g_chat, modem_pipelink_get_pipe(eg25g_pipelink));
    if (ret < 0) {
        printk("Failed to attach modem chat\n");
        modem_pipe_close(modem_pipelink_get_pipe(eg25g_pipelink), K_SECONDS(10));
        return ret;
    }
    ret = modem_chat_run_script_async(&eg25g_chat, &eg25g_init_chat_script);
    if (ret < 0) {
        LOG_ERR("Failed to initialize");
        modem_pipe_close(modem_pipelink_get_pipe(eg25g_pipelink), K_SECONDS(10));
        return ret;
    }
    k_msleep(12000);
    ret = modem_chat_run_script_async(&eg25g_chat, &eg25g_connect_chat_script);
    if (ret < 0) {
        LOG_ERR("Failed to initialize");
        modem_pipe_close(modem_pipelink_get_pipe(eg25g_pipelink), K_SECONDS(10));
        return ret;
    }
    // modem_chat_script_abort(&eg25g_chat);
    // modem_chat_release(&eg25g_chat);
    while (1) {
        k_msleep(10000);
        // modem_chat_run_script_async(&eg25g_chat, &quectel_eg25_g_periodic_chat_script);
        modem_pipe_transmit(modem_pipelink_get_pipe(eg25g_pipelink),
                            "fdsgaferta\r",
                            sizeof("fdsgaferta\r"));
    }

    return 0;
}

static void eg25g_pipe_open_handler(struct k_work* work) {
    ARG_UNUSED(work);
}

static void modem_cellular_chat_callback_handler(struct modem_chat* chat,
                                                 enum modem_chat_script_result result,
                                                 void* user_data) {
    // struct modem_cellular_data* data = (struct modem_cellular_data*)user_data;

    if (result == MODEM_CHAT_SCRIPT_RESULT_SUCCESS) {
        LOG_INF(".....chat script success");
        // modem_cellular_delegate_event(data, MODEM_CELLULAR_EVENT_SCRIPT_SUCCESS);
    } else {
        LOG_INF(".....chat script failed");
        // modem_cellular_delegate_event(data, MODEM_CELLULAR_EVENT_SCRIPT_FAILED);
    }
}

static void modem_cellular_chat_connect_callback_handler(struct modem_chat* chat,
                                                         enum modem_chat_script_result result,
                                                         void* user_data) {
    // struct modem_cellular_data* data = (struct modem_cellular_data*)user_data;

    if (result == MODEM_CHAT_SCRIPT_RESULT_SUCCESS) {
        LOG_INF(".....chat script success");
        modem_chat_script_abort(&eg25g_chat);
        modem_chat_release(&eg25g_chat);
        // modem_cellular_delegate_event(data, MODEM_CELLULAR_EVENT_SCRIPT_SUCCESS);
    } else {
        LOG_INF(".....chat script failed");
        // modem_cellular_delegate_event(data, MODEM_CELLULAR_EVENT_SCRIPT_FAILED);
    }
}
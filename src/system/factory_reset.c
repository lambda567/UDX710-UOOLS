/**
 * @file factory_reset.c
 * @brief 恢复出厂设置实现
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "mongoose.h"
#include "factory_reset.h"
#include "exec_utils.h"
#include "traffic.h"

#define VNSTAT_DB "/var/lib/vnstat/vnstat.db"
#define MAIN_DB "/home/root/6677/6677.db"
#define HOSTAPD_2G_CONF "/mnt/data/hostapd_2g.conf"
#define HOSTAPD_5G_CONF "/mnt/data/hostapd_5g.conf"

/* GET /api/factory-reset - 恢复出厂设置 */
void handle_factory_reset(struct mg_connection *c, struct mg_http_message *hm) {
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    char output[256];

    /* 1. 删除流量统计数据库并重新初始化 */
    run_command(output, sizeof(output), "rm", "-f", VNSTAT_DB, NULL);
    init_traffic();

    /* 2. 删除主数据库 */
    run_command(output, sizeof(output), "rm", "-f", MAIN_DB, NULL);

    /* 3. 删除WiFi配置文件 */
    run_command(output, sizeof(output), "rm", "-f", HOSTAPD_2G_CONF, NULL);
    run_command(output, sizeof(output), "rm", "-f", HOSTAPD_5G_CONF, NULL);

    /* 先返回响应 */
    mg_http_reply(c, 200,
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n",
        "{\"success\":true,\"msg\":\"Factory reset complete, rebooting...\"}");

    /* 延迟1秒后重启 */
    sleep(1);
    run_command(output, sizeof(output), "/sbin/reboot", NULL);
}

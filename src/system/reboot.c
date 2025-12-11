/**
 * @file reboot.c
 * @brief 定时重启实现 (Go: handlers/reboot.go)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include "mongoose.h"
#include "reboot.h"
#include "exec_utils.h"

#define CRON_FILE "/var/spool/cron/crontabs/root"

/* 读取第一个重启任务 */
static int read_first_reboot_job(char *job, size_t size) {
    FILE *f = fopen(CRON_FILE, "r");
    if (!f) return -1;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "/sbin/reboot")) {
            strncpy(job, line, size - 1);
            job[size - 1] = '\0';
            /* 去除换行符 */
            size_t len = strlen(job);
            if (len > 0 && job[len - 1] == '\n') job[len - 1] = '\0';
            fclose(f);
            return 0;
        }
    }
    fclose(f);
    job[0] = '\0';
    return 0;
}

/* GET /api/get/first-reboot - 获取定时重启配置 */
void handle_get_first_reboot(struct mg_connection *c, struct mg_http_message *hm) {
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    char job[256] = {0};
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm);

    int found = (read_first_reboot_job(job, sizeof(job)) == 0 && strlen(job) > 0);

    char json[512];
    if (found) {
        snprintf(json, sizeof(json),
            "{\"success\":true,\"job\":\"%s\",\"time\":\"%s\"}", job, time_str);
    } else {
        snprintf(json, sizeof(json),
            "{\"success\":false,\"job\":\"\",\"time\":\"%s\"}", time_str);
    }

    mg_http_reply(c, 200,
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n",
        "%s", json);
}

/* GET /api/set/reboot - 设置定时重启 */
void handle_set_reboot(struct mg_connection *c, struct mg_http_message *hm) {
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    char day[32] = {0}, hour[8] = {0}, minute[8] = {0};
    struct mg_str query = hm->query;

    if (query.len > 0) {
        char *q = strndup(query.buf, query.len);
        char *p = strstr(q, "day=");
        if (p) sscanf(p, "day=%31[^&]", day);
        p = strstr(q, "hour=");
        if (p) sscanf(p, "hour=%7[^&]", hour);
        p = strstr(q, "minute=");
        if (p) sscanf(p, "minute=%7[^&]", minute);
        free(q);
    }

    if (strlen(day) == 0 || strlen(hour) == 0 || strlen(minute) == 0) {
        mg_http_reply(c, 400,
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n",
            "{\"success\":false,\"msg\":\"Missing parameters\"}");
        return;
    }

    /* 确保目录存在 */
    mkdir("/var/spool/cron", 0755);
    mkdir("/var/spool/cron/crontabs", 0755);

    /* 删除现有 reboot 任务 */
    char output[256];
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "sed -i '/reboot/d' %s 2>/dev/null || true", CRON_FILE);
    run_command(output, sizeof(output), "sh", "-c", cmd, NULL);

    /* 添加新任务 */
    snprintf(cmd, sizeof(cmd), "echo '%s %s * * %s /sbin/reboot' >> %s", minute, hour, day, CRON_FILE);
    if (run_command(output, sizeof(output), "sh", "-c", cmd, NULL) != 0) {
        mg_http_reply(c, 500,
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n",
            "{\"success\":false,\"msg\":\"Failed to add job\"}");
        return;
    }

    mg_http_reply(c, 200,
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n",
        "{\"success\":true,\"msg\":\"Reboot job added\"}");
}

/* GET /api/claen/cron - 清除定时任务 */
void handle_clear_cron(struct mg_connection *c, struct mg_http_message *hm) {
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    char output[256];
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "sed -i '/reboot/d' %s 2>/dev/null || true", CRON_FILE);
    run_command(output, sizeof(output), "sh", "-c", cmd, NULL);

    mg_http_reply(c, 200,
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n",
        "{\"success\":true,\"msg\":\"Clean Reboot\"}");
}

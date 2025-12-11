/**
 * @file handlers.c
 * @brief HTTP API handlers implementation (Go: handlers)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "mongoose.h"
#include "handlers.h"
#include "dbus_core.h"
#include "sysinfo.h"
#include "exec_utils.h"
#include "airplane.h"
#include "modem.h"
#include "led.h"

/* 检查请求方法 */
static int check_method(struct mg_connection *c, struct mg_http_message *hm, const char *method) {
    /* 处理 OPTIONS 预检请求 */
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return 0;
    }
    
    size_t method_len = strlen(method);
    if (hm->method.len != method_len || memcmp(hm->method.buf, method, method_len) != 0) {
        send_error_response(c, 405, "Method not allowed");
        return 0;
    }
    return 1;
}


/* GET /api/info - 获取系统信息 */
void handle_info(struct mg_connection *c, struct mg_http_message *hm) {
    if (!check_method(c, hm, "GET")) return;

    SystemInfo info;
    char json[4096];

    get_system_info(&info);

    snprintf(json, sizeof(json),
        "{"
        "\"hostname\":\"%s\","
        "\"sysname\":\"%s\","
        "\"release\":\"%s\","
        "\"version\":\"%s\","
        "\"machine\":\"%s\","
        "\"total_ram\":%lu,"
        "\"free_ram\":%lu,"
        "\"cached_ram\":%lu,"
        "\"cpu_usage\":%.2f,"
        "\"uptime\":%.2f,"
        "\"bridge_status\":\"%s\","
        "\"sim_slot\":\"%s\","
        "\"signal_strength\":\"%s\","
        "\"thermal_temp\":%.2f,"
        "\"power_status\":\"%s\","
        "\"battery_health\":\"%s\","
        "\"battery_capacity\":%u,"
        "\"ssid\":\"%s\","
        "\"passwd\":\"%s\","
        "\"select_network_mode\":\"%s\","
        "\"is_activated\":%d,"
        "\"serial\":\"%s\","
        "\"network_mode\":\"%s\","
        "\"airplane_mode\":%s,"
        "\"imei\":\"%s\","
        "\"iccid\":\"%s\","
        "\"imsi\":\"%s\","
        "\"carrier\":\"%s\","
        "\"network_type\":\"%s\","
        "\"network_band\":\"%s\","
        "\"qci\":%d,"
        "\"downlink_rate\":%d,"
        "\"uplink_rate\":%d"
        "}",
        info.hostname, info.sysname, info.release, info.version, info.machine,
        info.total_ram, info.free_ram, info.cached_ram, info.cpu_usage, info.uptime,
        info.bridge_status, info.sim_slot, info.signal_strength, info.thermal_temp,
        info.power_status, info.battery_health, info.battery_capacity,
        info.ssid, info.passwd, info.select_network_mode, info.is_activated,
        info.serial, info.network_mode, info.airplane_mode ? "true" : "false",
        info.imei, info.iccid, info.imsi, info.carrier,
        info.network_type, info.network_band, info.qci, info.downlink_rate, info.uplink_rate
    );

    mg_http_reply(c, 200, 
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n", 
        "%s", json);
}

/* JSON 字符串转义 - 处理特殊字符 */
static void json_escape_string(const char *src, char *dst, size_t dst_size) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j < dst_size - 2; i++) {
        char c = src[i];
        switch (c) {
            case '"':  if (j + 2 < dst_size) { dst[j++] = '\\'; dst[j++] = '"'; } break;
            case '\\': if (j + 2 < dst_size) { dst[j++] = '\\'; dst[j++] = '\\'; } break;
            case '\n': if (j + 2 < dst_size) { dst[j++] = '\\'; dst[j++] = 'n'; } break;
            case '\r': if (j + 2 < dst_size) { dst[j++] = '\\'; dst[j++] = 'r'; } break;
            case '\t': if (j + 2 < dst_size) { dst[j++] = '\\'; dst[j++] = 't'; } break;
            default:
                if ((unsigned char)c >= 0x20) {
                    dst[j++] = c;
                }
                break;
        }
    }
    dst[j] = '\0';
}

/* POST /api/at - 执行 AT 命令 */
void handle_execute_at(struct mg_connection *c, struct mg_http_message *hm) {
    if (!check_method(c, hm, "POST")) return;

    char cmd[256] = {0};
    char *result = NULL;
    char response[4096];

    /* 解析 JSON 请求体: {"command": "AT..."} */
    struct mg_str json = hm->body;
    
    /* 简单 JSON 解析 - 提取 command 字段 */
    char *cmd_start = strstr(json.buf, "\"command\"");
    if (cmd_start) {
        cmd_start = strchr(cmd_start, ':');
        if (cmd_start) {
            cmd_start = strchr(cmd_start, '"');
            if (cmd_start) {
                cmd_start++;
                char *cmd_end = strchr(cmd_start, '"');
                if (cmd_end) {
                    size_t len = cmd_end - cmd_start;
                    if (len < sizeof(cmd)) {
                        memcpy(cmd, cmd_start, len);
                    }
                }
            }
        }
    }

    if (strlen(cmd) == 0) {
        snprintf(response, sizeof(response), 
            "{\"Code\":1,\"Error\":\"命令不能为空\",\"Data\":null}");
        mg_http_reply(c, 200, 
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n", 
            "%s", response);
        return;
    }

    /* 自动添加 AT 前缀 */
    if (strncasecmp(cmd, "AT", 2) != 0) {
        char tmp[256];
        snprintf(tmp, sizeof(tmp), "AT%s", cmd);
        strncpy(cmd, tmp, sizeof(cmd) - 1);
    }

    printf("执行 AT 命令: %s\n", cmd);

    /* 执行 AT 命令 */
    if (execute_at(cmd, &result) == 0) {
        printf("AT 命令执行成功: %s\n", result);
        /* 转义 JSON 特殊字符 */
        char escaped[2048];
        json_escape_string(result ? result : "", escaped, sizeof(escaped));
        snprintf(response, sizeof(response),
            "{\"Code\":0,\"Error\":\"\",\"Data\":\"%s\"}", escaped);
        g_free(result);
    } else {
        printf("AT 命令执行失败: %s\n", dbus_get_last_error());
        char escaped_err[512];
        json_escape_string(dbus_get_last_error(), escaped_err, sizeof(escaped_err));
        snprintf(response, sizeof(response),
            "{\"Code\":1,\"Error\":\"%s\",\"Data\":null}", escaped_err);
    }

    mg_http_reply(c, 200, 
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n", 
        "%s", response);
}


/* 简单 JSON 字符串提取 */
static int extract_json_string(const char *json, const char *key, char *value, size_t size) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    char *p = strstr(json, pattern);
    if (!p) return -1;
    p = strchr(p + strlen(pattern), ':');
    if (!p) return -1;
    p = strchr(p, '"');
    if (!p) return -1;
    p++;
    char *end = strchr(p, '"');
    if (!end) return -1;
    size_t len = end - p;
    if (len >= size) len = size - 1;
    memcpy(value, p, len);
    value[len] = '\0';
    return 0;
}

/* POST /api/set_network - 设置网络模式 */
void handle_set_network(struct mg_connection *c, struct mg_http_message *hm) {
    if (!check_method(c, hm, "POST")) return;

    char mode[32] = {0};
    char slot[16] = {0};

    /* 解析 JSON: {"mode": "...", "slot": "..."} */
    extract_json_string(hm->body.buf, "mode", mode, sizeof(mode));
    extract_json_string(hm->body.buf, "slot", slot, sizeof(slot));

    if (strlen(mode) == 0) {
        mg_http_reply(c, 400, 
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n", 
            "{\"error\":\"Mode parameter is required\"}");
        return;
    }

    if (!is_valid_network_mode(mode)) {
        mg_http_reply(c, 400, 
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n", 
            "{\"error\":\"Invalid mode value\"}");
        return;
    }

    if (strlen(slot) > 0 && !is_valid_slot(slot)) {
        mg_http_reply(c, 400, 
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n", 
            "{\"error\":\"Invalid slot value. Must be 'slot1' or 'slot2'\"}");
        return;
    }

    if (set_network_mode_for_slot(mode, strlen(slot) > 0 ? slot : NULL) == 0) {
        mg_http_reply(c, 200, 
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n", 
            "{\"status\":\"success\",\"message\":\"Network mode updated successfully\"}");
    } else {
        mg_http_reply(c, 200, 
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n", 
            "{\"status\":\"error\",\"message\":\"Failed to update network mode\"}");
    }
}

/* POST /api/switch - 切换 SIM 卡槽 */
void handle_switch(struct mg_connection *c, struct mg_http_message *hm) {
    if (!check_method(c, hm, "POST")) return;

    char slot[16] = {0};

    /* 解析 JSON: {"slot": "slot1"} */
    extract_json_string(hm->body.buf, "slot", slot, sizeof(slot));

    if (strlen(slot) == 0) {
        mg_http_reply(c, 400, 
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n", 
            "{\"error\":\"Slot parameter is required\"}");
        return;
    }

    if (!is_valid_slot(slot)) {
        mg_http_reply(c, 400, 
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n", 
            "{\"error\":\"Invalid slot value. Must be 'slot1' or 'slot2'\"}");
        return;
    }

    if (switch_slot(slot) == 0) {
        char response[128];
        snprintf(response, sizeof(response), 
            "{\"status\":\"success\",\"message\":\"Slot switched to %s successfully\"}", slot);
        mg_http_reply(c, 200, 
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n", 
            "%s", response);
    } else {
        char response[128];
        snprintf(response, sizeof(response), 
            "{\"status\":\"error\",\"message\":\"Failed to switch slot to %s\"}", slot);
        mg_http_reply(c, 200, 
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n", 
            "%s", response);
    }
}

/* POST /api/airplane_mode - 飞行模式控制 */
void handle_airplane_mode(struct mg_connection *c, struct mg_http_message *hm) {
    if (!check_method(c, hm, "POST")) return;

    /* 解析 JSON: {"enabled": true/false} */
    int enabled = -1;
    struct mg_str json = hm->body;

    if (strstr(json.buf, "\"enabled\":true") || strstr(json.buf, "\"enabled\": true")) {
        enabled = 1;
    } else if (strstr(json.buf, "\"enabled\":false") || strstr(json.buf, "\"enabled\": false")) {
        enabled = 0;
    }

    if (enabled == -1) {
        mg_http_reply(c, 400, 
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n", 
            "{\"error\":\"Invalid request body\"}");
        return;
    }

    if (set_airplane_mode(enabled) == 0) {
        mg_http_reply(c, 200, 
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n", 
            "{\"status\":\"success\",\"message\":\"Airplane mode updated successfully\"}");
    } else {
        mg_http_reply(c, 500, 
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n", 
            "{\"error\":\"Failed to set airplane mode: AT command failed\"}");
    }
}

/* POST /api/device_control - 设备控制 */
void handle_device_control(struct mg_connection *c, struct mg_http_message *hm) {
    if (!check_method(c, hm, "POST")) return;

    char action[32] = {0};

    /* 解析 JSON: {"action": "reboot"} 或 {"action": "poweroff"} */
    extract_json_string(hm->body.buf, "action", action, sizeof(action));

    if (strlen(action) == 0) {
        mg_http_reply(c, 400, 
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n", 
            "{\"error\":\"Action parameter is required\"}");
        return;
    }

    if (strcmp(action, "reboot") == 0) {
        mg_http_reply(c, 200, 
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n", 
            "{\"status\":\"success\",\"message\":\"Reboot command sent\"}");
        device_reboot();
    } else if (strcmp(action, "poweroff") == 0) {
        mg_http_reply(c, 200, 
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n", 
            "{\"status\":\"success\",\"message\":\"Poweroff command sent\"}");
        device_poweroff();
    } else {
        mg_http_reply(c, 400, 
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n", 
            "{\"error\":\"Invalid action. Must be 'reboot' or 'poweroff'\"}");
    }
}

/* POST /api/clear_cache - 清除缓存 */
void handle_clear_cache(struct mg_connection *c, struct mg_http_message *hm) {
    if (!check_method(c, hm, "POST")) return;

    if (clear_cache() == 0) {
        mg_http_reply(c, 200, 
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n", 
            "{\"status\":\"success\",\"message\":\"Cache cleared successfully\"}");
    } else {
        mg_http_reply(c, 500, 
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n", 
            "{\"error\":\"Failed to clear cache\"}");
    }
}


/* 解析 AT 命令返回的小区数据 (Go: parseCellToVec) */
/* 返回解析后的行数，data 是二维数组 [行][列] */
int parse_cell_to_vec(const char *input, char data[64][16][32]) {
    char cleaned[4096];
    strncpy(cleaned, input, sizeof(cleaned) - 1);
    cleaned[sizeof(cleaned) - 1] = '\0';

    /* 去除 OK 和换行符 */
    char *ok_pos = strstr(cleaned, "OK");
    if (ok_pos) *ok_pos = '\0';
    
    /* 替换 \r\n 为空 */
    char *p = cleaned;
    char *dst = cleaned;
    while (*p) {
        if (*p != '\r' && *p != '\n') {
            *dst++ = *p;
        }
        p++;
    }
    *dst = '\0';

    int row = 0;
    int col = 0;
    char current_part[4096] = {0};
    int part_len = 0;
    char prev_char = 0;

    p = cleaned;
    while (*p && row < 64) {
        char c = *p;
        
        if (c == '-') {
            if (prev_char == ',') {
                /* 规则2: ,- 作为负数处理 */
                current_part[part_len++] = c;
            } else if (*(p + 1) == '-') {
                /* 规则3: -- 分割换行并保留第二个 - */
                if (part_len > 0) {
                    current_part[part_len] = '\0';
                    /* 按逗号分割 */
                    col = 0;
                    char *token = strtok(current_part, ",");
                    while (token && col < 16) {
                        while (*token == ' ') token++;
                        strncpy(data[row][col], token, 31);
                        data[row][col][31] = '\0';
                        col++;
                        token = strtok(NULL, ",");
                    }
                    row++;
                    part_len = 0;
                }
                current_part[part_len++] = '-';
                p++; /* 跳过下一个 - */
            } else {
                /* 规则1: 单独 - 换行 */
                if (part_len > 0) {
                    current_part[part_len] = '\0';
                    col = 0;
                    char *token = strtok(current_part, ",");
                    while (token && col < 16) {
                        while (*token == ' ') token++;
                        strncpy(data[row][col], token, 31);
                        data[row][col][31] = '\0';
                        col++;
                        token = strtok(NULL, ",");
                    }
                    row++;
                    part_len = 0;
                }
            }
        } else {
            current_part[part_len++] = c;
        }
        prev_char = c;
        p++;
    }

    /* 处理最后剩余部分 */
    if (part_len > 0 && row < 64) {
        current_part[part_len] = '\0';
        col = 0;
        char *token = strtok(current_part, ",");
        while (token && col < 16) {
            while (*token == ' ') token++;
            strncpy(data[row][col], token, 31);
            data[row][col][31] = '\0';
            col++;
            token = strtok(NULL, ",");
        }
        row++;
    }

    return row;
}

/**
 * 判断当前网络是否为 5G
 * 通过 D-Bus 查询 oFono NetworkMonitor 获取网络类型
 * @return 1=5G, 0=4G/其他
 */
static int is_5g_network(void) {
    char output[2048];
    
    /* 使用 dbus-send 获取网络信息 (与 Go 版本一致) */
    if (run_command(output, sizeof(output), "dbus-send", "--system", "--dest=org.ofono", 
                    "--print-reply", "/ril_0", "org.ofono.NetworkMonitor.GetServingCellInformation", NULL) != 0) {
        printf("D-Bus 查询网络类型失败，默认使用 4G\n");
        return 0;
    }

    /* 判断网络类型 - 检查是否包含 "nr" */
    if (strstr(output, "\"nr\"")) {
        return 1; /* 5G */
    }
    
    return 0; /* 4G 或其他 */
}

/* GET /api/current_band - 获取当前连接频段 */
void handle_get_current_band(struct mg_connection *c, struct mg_http_message *hm) {
    if (!check_method(c, hm, "GET")) return;

    char net_type[32] = "N/A";
    char band[32] = "N/A";
    int arfcn = 0, pci = 0;
    double rsrp = 0, rsrq = 0, sinr = 0;
    char response[512];

    /* 通过 D-Bus 判断网络类型 (与 Go 版本一致) */
    int is_5g = is_5g_network();
    char *result = NULL;

    if (is_5g) {
        /* 5G 网络: AT+SPENGMD=0,14,1 */
        if (execute_at("AT+SPENGMD=0,14,1", &result) == 0 && result && strlen(result) > 100) {
            char data[64][16][32] = {{{0}}};
            int rows = parse_cell_to_vec(result, data);
            
            if (rows > 15) {
                strcpy(net_type, "5G NR");
                if (strlen(data[0][0]) > 0) {
                    snprintf(band, sizeof(band), "N%s", data[0][0]);
                }
                if (strlen(data[1][0]) > 0) {
                    arfcn = atoi(data[1][0]);
                }
                if (strlen(data[2][0]) > 0) {
                    pci = atoi(data[2][0]);
                }
                if (strlen(data[3][0]) > 0) {
                    rsrp = atof(data[3][0]) / 100.0;
                }
                if (strlen(data[4][0]) > 0) {
                    rsrq = atof(data[4][0]) / 100.0;
                }
                if (strlen(data[15][0]) > 0) {
                    sinr = atof(data[15][0]) / 100.0;
                }
                printf("当前连接5G频段: Band=%s, ARFCN=%d, PCI=%d, RSRP=%.2f, RSRQ=%.2f, SINR=%.2f\n",
                       band, arfcn, pci, rsrp, rsrq, sinr);
            }
        }
        if (result) { g_free(result); result = NULL; }
    } else {
        /* 4G 网络: AT+SPENGMD=0,6,0 */
        if (execute_at("AT+SPENGMD=0,6,0", &result) == 0 && result && strlen(result) > 100) {
            char data[64][16][32] = {{{0}}};
            int rows = parse_cell_to_vec(result, data);
            
            if (rows > 33) {
                strcpy(net_type, "4G LTE");
                if (strlen(data[0][0]) > 0) {
                    snprintf(band, sizeof(band), "B%s", data[0][0]);
                }
                if (strlen(data[1][0]) > 0) {
                    arfcn = atoi(data[1][0]);
                }
                if (strlen(data[2][0]) > 0) {
                    pci = atoi(data[2][0]);
                }
                if (strlen(data[3][0]) > 0) {
                    rsrp = atof(data[3][0]) / 100.0;
                }
                if (strlen(data[4][0]) > 0) {
                    rsrq = atof(data[4][0]) / 100.0;
                }
                if (strlen(data[33][0]) > 0) {
                    sinr = atof(data[33][0]) / 100.0;
                }
                printf("当前连接4G频段: Band=%s, ARFCN=%d, PCI=%d, RSRP=%.2f, RSRQ=%.2f, SINR=%.2f\n",
                       band, arfcn, pci, rsrp, rsrq, sinr);
            }
        }
        if (result) { g_free(result); result = NULL; }
    }

    snprintf(response, sizeof(response),
        "{\"Code\":0,\"Error\":\"\",\"Data\":{"
        "\"network_type\":\"%s\","
        "\"band\":\"%s\","
        "\"arfcn\":%d,"
        "\"pci\":%d,"
        "\"rsrp\":%.2f,"
        "\"rsrq\":%.2f,"
        "\"sinr\":%.2f"
        "}}",
        net_type, band, arfcn, pci, rsrp, rsrq, sinr);

    mg_http_reply(c, 200,
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n",
        "%s", response);
}


/* ==================== 短信 API ==================== */
#include "sms.h"

/* GET /api/sms - 获取短信列表 */
void handle_sms_list(struct mg_connection *c, struct mg_http_message *hm) {
    /* 处理 OPTIONS 预检请求 */
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    SmsMessage messages[100];
    int count = sms_get_list(messages, 100);
    
    if (count < 0) {
        mg_http_reply(c, 500, 
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n",
            "{\"error\":\"获取短信列表失败\"}");
        return;
    }

    /* 构建JSON数组 */
    char json[65536];
    int offset = 0;
    offset += snprintf(json + offset, sizeof(json) - offset, "[");
    
    for (int i = 0; i < count; i++) {
        char escaped_content[2048];
        json_escape_string(messages[i].content, escaped_content, sizeof(escaped_content));
        
        char time_str[32];
        struct tm *tm_info = localtime(&messages[i].timestamp);
        strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%S", tm_info);
        
        offset += snprintf(json + offset, sizeof(json) - offset,
            "%s{\"id\":%d,\"sender\":\"%s\",\"content\":\"%s\",\"timestamp\":\"%s\",\"read\":%s}",
            i > 0 ? "," : "",
            messages[i].id, messages[i].sender, escaped_content, time_str,
            messages[i].is_read ? "true" : "false");
    }
    
    offset += snprintf(json + offset, sizeof(json) - offset, "]");

    mg_http_reply(c, 200, 
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n",
        "%s", json);
}

/* POST /api/sms/send - 发送短信 */
void handle_sms_send(struct mg_connection *c, struct mg_http_message *hm) {
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    char recipient[64] = {0};
    char content[1024] = {0};
    
    /* 解析JSON */
    struct mg_str json = hm->body;
    char *p;
    
    /* 提取 recipient */
    p = strstr(json.buf, "\"recipient\"");
    if (p) {
        p = strchr(p, ':');
        if (p) {
            p = strchr(p, '"');
            if (p) {
                p++;
                char *end = strchr(p, '"');
                if (end) {
                    size_t len = end - p;
                    if (len < sizeof(recipient)) memcpy(recipient, p, len);
                }
            }
        }
    }
    
    /* 提取 content */
    p = strstr(json.buf, "\"content\"");
    if (p) {
        p = strchr(p, ':');
        if (p) {
            p = strchr(p, '"');
            if (p) {
                p++;
                char *end = strchr(p, '"');
                if (end) {
                    size_t len = end - p;
                    if (len < sizeof(content)) memcpy(content, p, len);
                }
            }
        }
    }

    if (strlen(recipient) == 0 || strlen(content) == 0) {
        mg_http_reply(c, 400, 
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n",
            "{\"error\":\"收件人和内容不能为空\"}");
        return;
    }

    char result_path[256] = {0};
    if (sms_send(recipient, content, result_path, sizeof(result_path)) == 0) {
        mg_http_reply(c, 200, 
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n",
            "{\"status\":\"success\",\"message\":\"短信发送成功\",\"path\":\"%s\"}", result_path);
    } else {
        mg_http_reply(c, 500, 
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n",
            "{\"error\":\"短信发送失败\"}");
    }
}

/* DELETE /api/sms/:id - 删除短信 */
void handle_sms_delete(struct mg_connection *c, struct mg_http_message *hm) {
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    /* 从URL提取ID: /api/sms/123 */
    int id = 0;
    const char *uri = hm->uri.buf;
    const char *id_start = strstr(uri, "/api/sms/");
    if (id_start) {
        id_start += 9;  /* 跳过 "/api/sms/" */
        id = atoi(id_start);
    }

    if (id <= 0) {
        mg_http_reply(c, 400, 
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n",
            "{\"error\":\"无效的短信ID\"}");
        return;
    }

    if (sms_delete(id) == 0) {
        mg_http_reply(c, 200, 
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n",
            "{\"status\":\"success\",\"message\":\"短信已删除\"}");
    } else {
        mg_http_reply(c, 500, 
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n",
            "{\"error\":\"删除短信失败\"}");
    }
}

/* GET /api/sms/webhook - 获取Webhook配置 */
void handle_sms_webhook_get(struct mg_connection *c, struct mg_http_message *hm) {
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    WebhookConfig config;
    if (sms_get_webhook_config(&config) != 0) {
        mg_http_reply(c, 500, 
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n",
            "{\"error\":\"获取配置失败\"}");
        return;
    }

    char escaped_body[4096];
    char escaped_headers[1024];
    json_escape_string(config.body, escaped_body, sizeof(escaped_body));
    json_escape_string(config.headers, escaped_headers, sizeof(escaped_headers));

    char json[8192];
    snprintf(json, sizeof(json),
        "{\"enabled\":%s,\"platform\":\"%s\",\"url\":\"%s\",\"body\":\"%s\",\"headers\":\"%s\"}",
        config.enabled ? "true" : "false", config.platform, config.url, escaped_body, escaped_headers);

    mg_http_reply(c, 200, 
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n",
        "%s", json);
}

/* 智能解析JSON字符串值 - 正确处理转义字符 */
static int parse_json_string_field(const char *json, const char *key, char *out, size_t out_size) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    
    const char *p = strstr(json, pattern);
    if (!p) return -1;
    
    p += strlen(pattern);
    while (*p == ' ' || *p == ':') p++;
    if (*p != '"') return -1;
    p++;  /* 跳过开始引号 */
    
    size_t i = 0;
    while (*p && i < out_size - 1) {
        if (*p == '\\' && *(p + 1)) {
            p++;
            switch (*p) {
                case 'n': out[i++] = '\n'; break;
                case 'r': out[i++] = '\r'; break;
                case 't': out[i++] = '\t'; break;
                case '"': out[i++] = '"'; break;
                case '\\': out[i++] = '\\'; break;
                case '/': out[i++] = '/'; break;
                case 'u':
                    /* 跳过Unicode \uXXXX */
                    if (*(p+1) && *(p+2) && *(p+3) && *(p+4)) {
                        p += 4;
                        out[i++] = '?';
                    }
                    break;
                default: out[i++] = *p; break;
            }
            p++;
        } else if (*p == '"') {
            break;  /* 未转义的引号表示字符串结束 */
        } else {
            out[i++] = *p++;
        }
    }
    out[i] = '\0';
    return 0;
}

/* POST /api/sms/webhook - 保存Webhook配置 */
void handle_sms_webhook_save(struct mg_connection *c, struct mg_http_message *hm) {
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    WebhookConfig config = {0};
    
    /* 复制JSON到临时缓冲区确保以null结尾 */
    char json_buf[8192];
    size_t json_len = hm->body.len < sizeof(json_buf) - 1 ? hm->body.len : sizeof(json_buf) - 1;
    memcpy(json_buf, hm->body.buf, json_len);
    json_buf[json_len] = '\0';
    
    /* 解析 enabled */
    if (strstr(json_buf, "\"enabled\":true") || strstr(json_buf, "\"enabled\": true")) {
        config.enabled = 1;
    }
    
    /* 使用智能解析函数解析字符串字段 */
    parse_json_string_field(json_buf, "platform", config.platform, sizeof(config.platform));
    parse_json_string_field(json_buf, "url", config.url, sizeof(config.url));
    parse_json_string_field(json_buf, "body", config.body, sizeof(config.body));
    parse_json_string_field(json_buf, "headers", config.headers, sizeof(config.headers));

    if (sms_save_webhook_config(&config) == 0) {
        mg_http_reply(c, 200, 
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n",
            "{\"status\":\"success\",\"message\":\"配置已保存\"}");
    } else {
        mg_http_reply(c, 500, 
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n",
            "{\"error\":\"保存配置失败\"}");
    }
}

/* POST /api/sms/webhook/test - 测试Webhook */
void handle_sms_webhook_test(struct mg_connection *c, struct mg_http_message *hm) {
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: POST, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    if (sms_test_webhook() == 0) {
        mg_http_reply(c, 200, 
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n",
            "{\"status\":\"success\",\"message\":\"测试通知已发送\"}");
    } else {
        mg_http_reply(c, 500, 
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n",
            "{\"error\":\"Webhook未启用或URL为空\"}");
    }
}

/* GET /api/sms/sent - 获取发送记录列表 */
void handle_sms_sent_list(struct mg_connection *c, struct mg_http_message *hm) {
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    SentSmsMessage messages[150];
    int count = sms_get_sent_list(messages, 150);
    
    if (count < 0) {
        mg_http_reply(c, 500, 
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n",
            "{\"error\":\"获取发送记录失败\"}");
        return;
    }

    char json[65536];
    int offset = snprintf(json, sizeof(json), "[");
    
    for (int i = 0; i < count; i++) {
        char escaped_content[2048];
        size_t j = 0;
        for (size_t k = 0; messages[i].content[k] && j < sizeof(escaped_content) - 2; k++) {
            char ch = messages[i].content[k];
            if (ch == '"' || ch == '\\') {
                escaped_content[j++] = '\\';
            } else if (ch == '\n') {
                escaped_content[j++] = '\\';
                ch = 'n';
            } else if (ch == '\r') {
                escaped_content[j++] = '\\';
                ch = 'r';
            }
            escaped_content[j++] = ch;
        }
        escaped_content[j] = '\0';
        
        offset += snprintf(json + offset, sizeof(json) - offset,
            "%s{\"id\":%d,\"recipient\":\"%s\",\"content\":\"%s\",\"timestamp\":%ld,\"status\":\"%s\"}",
            i > 0 ? "," : "",
            messages[i].id, messages[i].recipient, escaped_content,
            (long)messages[i].timestamp, messages[i].status);
    }
    
    snprintf(json + offset, sizeof(json) - offset, "]");
    
    mg_http_reply(c, 200, 
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n",
        "%s", json);
}

/* GET /api/sms/config - 获取短信配置 */
void handle_sms_config_get(struct mg_connection *c, struct mg_http_message *hm) {
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    int max_count = sms_get_max_count();
    int max_sent_count = sms_get_max_sent_count();
    
    mg_http_reply(c, 200, 
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n",
        "{\"max_count\":%d,\"max_sent_count\":%d}", max_count, max_sent_count);
}

/* POST /api/sms/config - 保存短信配置 */
void handle_sms_config_save(struct mg_connection *c, struct mg_http_message *hm) {
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    struct mg_str body = hm->body;
    double val = 0;
    int max_count = sms_get_max_count();
    int max_sent_count = sms_get_max_sent_count();
    
    if (mg_json_get_num(body, "$.max_count", &val)) {
        max_count = (int)val;
    }
    if (mg_json_get_num(body, "$.max_sent_count", &val)) {
        max_sent_count = (int)val;
    }
    
    if (max_count < 10 || max_count > 150) {
        mg_http_reply(c, 400, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
            "{\"error\":\"收件箱最大存储数量必须在10-150之间\"}");
        return;
    }
    if (max_sent_count < 1 || max_sent_count > 50) {
        mg_http_reply(c, 400, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
            "{\"error\":\"发件箱最大存储数量必须在1-50之间\"}");
        return;
    }

    sms_set_max_count(max_count);
    sms_set_max_sent_count(max_sent_count);
    
    mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
        "{\"status\":\"success\",\"max_count\":%d,\"max_sent_count\":%d}", max_count, max_sent_count);
}

/* DELETE /api/sms/sent/:id - 删除发送记录 */
void handle_sms_sent_delete(struct mg_connection *c, struct mg_http_message *hm) {
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: DELETE, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    int id = 0;
    const char *uri = hm->uri.buf;
    const char *id_start = strstr(uri, "/api/sms/sent/");
    if (id_start) {
        id_start += 14;
        id = atoi(id_start);
    }

    if (id <= 0) {
        mg_http_reply(c, 400, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
            "{\"error\":\"无效的ID\"}");
        return;
    }

    if (sms_delete_sent(id) == 0) {
        mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
            "{\"status\":\"success\"}");
    } else {
        mg_http_reply(c, 500, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
            "{\"error\":\"删除失败\"}");
    }
}

/* GET /api/sms/fix - 获取短信接收修复开关状态 */
void handle_sms_fix_get(struct mg_connection *c, struct mg_http_message *hm) {
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    int enabled = sms_get_fix_enabled();
    
    mg_http_reply(c, 200, 
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n",
        "{\"enabled\":%s}", enabled ? "true" : "false");
}

/* POST /api/sms/fix - 设置短信接收修复开关 */
void handle_sms_fix_set(struct mg_connection *c, struct mg_http_message *hm) {
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    struct mg_str body = hm->body;
    int enabled = 0;
    
    /* 解析 enabled 字段 */
    if (strstr(body.buf, "\"enabled\":true") || strstr(body.buf, "\"enabled\": true")) {
        enabled = 1;
    }
    
    if (sms_set_fix_enabled(enabled) == 0) {
        mg_http_reply(c, 200, 
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n",
            "{\"status\":\"success\",\"enabled\":%s,\"message\":\"%s\"}", 
            enabled ? "true" : "false",
            enabled ? "短信接收修复已开启" : "短信接收修复已关闭");
    } else {
        mg_http_reply(c, 500, 
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n",
            "{\"error\":\"设置失败，AT命令执行错误\"}");
    }
}

/* ==================== WiFi API ==================== */
#include "wifi.h"

/* GET /api/wifi/status - 获取WiFi状态 */
void handle_wifi_status(struct mg_connection *c, struct mg_http_message *hm) {
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    WifiConfig config;
    if (wifi_get_status(&config) != 0) {
        mg_http_reply(c, 500, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
            "{\"error\":\"获取WiFi状态失败\"}");
        return;
    }

    char json[1024];
    snprintf(json, sizeof(json),
        "{\"enabled\":%s,\"band\":\"%s\",\"ssid\":\"%s\",\"password\":\"%s\",\"channel\":%d,"
        "\"encryption\":\"%s\",\"hidden\":%s,\"max_clients\":%d}",
        config.enabled ? "true" : "false", config.band, config.ssid, config.password, config.channel,
        config.encryption, config.hidden ? "true" : "false", config.max_clients);

    mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
        "%s", json);
}

/* POST /api/wifi/config - 设置WiFi配置 */
void handle_wifi_config(struct mg_connection *c, struct mg_http_message *hm) {
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    struct mg_str body = hm->body;
    char ssid[64] = {0}, password[64] = {0}, band[16] = {0};
    int channel = 0, hidden = -1, max_clients = 0;

    /* 解析JSON字段 */
    extract_json_string(body.buf, "ssid", ssid, sizeof(ssid));
    extract_json_string(body.buf, "password", password, sizeof(password));
    extract_json_string(body.buf, "band", band, sizeof(band));
    
    double val;
    if (mg_json_get_num(body, "$.channel", &val)) channel = (int)val;
    if (mg_json_get_num(body, "$.max_clients", &val)) max_clients = (int)val;
    
    if (strstr(body.buf, "\"hidden\":true")) hidden = 1;
    else if (strstr(body.buf, "\"hidden\":false")) hidden = 0;

    /* 应用配置 */
    int changed = 0;
    if (strlen(ssid) > 0 && wifi_set_ssid(ssid) == 0) changed++;
    if (strlen(password) >= 8 && wifi_set_password(password) == 0) changed++;
    if (strlen(band) > 0 && wifi_set_band(band) == 0) changed++;
    if (channel > 0 && wifi_set_channel(channel) == 0) changed++;
    if (hidden >= 0 && wifi_set_hidden(hidden) == 0) changed++;
    if (max_clients > 0 && wifi_set_max_clients(max_clients) == 0) changed++;

    mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
        "{\"status\":\"success\",\"changes\":%d}", changed);
}

/* POST /api/wifi/enable - 启用WiFi */
void handle_wifi_enable(struct mg_connection *c, struct mg_http_message *hm) {
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: POST, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    char band[16] = {0};
    extract_json_string(hm->body.buf, "band", band, sizeof(band));

    if (wifi_enable(strlen(band) > 0 ? band : NULL) == 0) {
        mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
            "{\"status\":\"success\",\"message\":\"WiFi已启用\"}");
    } else {
        mg_http_reply(c, 500, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
            "{\"error\":\"启用WiFi失败\"}");
    }
}

/* POST /api/wifi/disable - 禁用WiFi */
void handle_wifi_disable(struct mg_connection *c, struct mg_http_message *hm) {
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: POST, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    if (wifi_disable() == 0) {
        mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
            "{\"status\":\"success\",\"message\":\"WiFi已禁用\"}");
    } else {
        mg_http_reply(c, 500, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
            "{\"error\":\"禁用WiFi失败\"}");
    }
}

/* POST /api/wifi/band - 切换WiFi频段 */
void handle_wifi_band(struct mg_connection *c, struct mg_http_message *hm) {
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: POST, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    char band[16] = {0};
    extract_json_string(hm->body.buf, "band", band, sizeof(band));

    if (strlen(band) == 0) {
        mg_http_reply(c, 400, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
            "{\"error\":\"频段参数不能为空\"}");
        return;
    }

    if (wifi_set_band(band) == 0) {
        mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
            "{\"status\":\"success\",\"band\":\"%s\"}", band);
    } else {
        mg_http_reply(c, 500, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
            "{\"error\":\"切换频段失败\"}");
    }
}


/* ==================== WiFi 客户端管理 API ==================== */

/* GET /api/wifi/clients - 获取已连接客户端列表 */
void handle_wifi_clients(struct mg_connection *c, struct mg_http_message *hm) {
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    WifiClient clients[64];
    int count = wifi_get_clients(clients, 64);
    
    if (count < 0) {
        mg_http_reply(c, 500, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
            "{\"error\":\"获取客户端列表失败\"}");
        return;
    }

    char json[16384];
    int offset = snprintf(json, sizeof(json), "[");
    
    for (int i = 0; i < count; i++) {
        unsigned long total = clients[i].rx_bytes + clients[i].tx_bytes;
        offset += snprintf(json + offset, sizeof(json) - offset,
            "%s{\"mac\":\"%s\",\"rx_bytes\":%lu,\"tx_bytes\":%lu,\"total\":%lu,\"signal\":%d,\"connected_time\":%d}",
            i > 0 ? "," : "",
            clients[i].mac, clients[i].rx_bytes, clients[i].tx_bytes, total,
            clients[i].signal, clients[i].connected_time);
    }
    
    snprintf(json + offset, sizeof(json) - offset, "]");
    
    mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
        "%s", json);
}

/* /api/wifi/blacklist - 黑名单管理 */
void handle_wifi_blacklist(struct mg_connection *c, struct mg_http_message *hm) {
    /* OPTIONS 预检 */
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    /* GET - 获取黑名单列表 */
    if (hm->method.len == 3 && memcmp(hm->method.buf, "GET", 3) == 0) {
        char macs[128][18];
        int count = wifi_blacklist_list(macs, 128);
        
        if (count < 0) {
            mg_http_reply(c, 500, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
                "{\"error\":\"获取黑名单失败\"}");
            return;
        }

        char json[4096];
        int offset = snprintf(json, sizeof(json), "[");
        for (int i = 0; i < count; i++) {
            offset += snprintf(json + offset, sizeof(json) - offset,
                "%s\"%s\"", i > 0 ? "," : "", macs[i]);
        }
        snprintf(json + offset, sizeof(json) - offset, "]");
        
        mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
            "%s", json);
        return;
    }

    /* POST - 添加到黑名单 */
    if (hm->method.len == 4 && memcmp(hm->method.buf, "POST", 4) == 0) {
        char mac[32] = {0};
        extract_json_string(hm->body.buf, "mac", mac, sizeof(mac));
        
        if (strlen(mac) < 17) {
            mg_http_reply(c, 400, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
                "{\"error\":\"MAC地址无效\"}");
            return;
        }

        if (wifi_blacklist_add(mac) == 0) {
            mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
                "{\"status\":\"success\",\"message\":\"已添加到黑名单并踢出\"}");
        } else {
            mg_http_reply(c, 500, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
                "{\"error\":\"添加黑名单失败\"}");
        }
        return;
    }

    /* DELETE - 删除或清空黑名单 */
    if (hm->method.len == 6 && memcmp(hm->method.buf, "DELETE", 6) == 0) {
        /* 检查URL是否包含MAC地址: /api/wifi/blacklist/xx:xx:xx:xx:xx:xx */
        const char *mac_start = strstr(hm->uri.buf, "/api/wifi/blacklist/");
        if (mac_start && strlen(mac_start) > 20) {
            mac_start += 20;  /* 跳过 "/api/wifi/blacklist/" */
            char mac[32] = {0};
            size_t len = 0;
            while (mac_start[len] && mac_start[len] != '?' && mac_start[len] != ' ' && len < 17) {
                mac[len] = mac_start[len];
                len++;
            }
            mac[len] = '\0';
            
            if (len >= 17) {
                if (wifi_blacklist_del(mac) == 0) {
                    mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
                        "{\"status\":\"success\",\"message\":\"已从黑名单移除\"}");
                } else {
                    mg_http_reply(c, 500, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
                        "{\"error\":\"移除黑名单失败\"}");
                }
                return;
            }
        }
        
        /* 无MAC参数则清空黑名单 */
        if (wifi_blacklist_clear() == 0) {
            mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
                "{\"status\":\"success\",\"message\":\"黑名单已清空\"}");
        } else {
            mg_http_reply(c, 500, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
                "{\"error\":\"清空黑名单失败\"}");
        }
        return;
    }

    mg_http_reply(c, 405, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
        "{\"error\":\"Method not allowed\"}");
}

/* /api/wifi/whitelist - 白名单管理 */
void handle_wifi_whitelist(struct mg_connection *c, struct mg_http_message *hm) {
    /* OPTIONS 预检 */
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    /* GET - 获取白名单列表 */
    if (hm->method.len == 3 && memcmp(hm->method.buf, "GET", 3) == 0) {
        char macs[128][18];
        int count = wifi_whitelist_list(macs, 128);
        
        if (count < 0) {
            mg_http_reply(c, 500, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
                "{\"error\":\"获取白名单失败\"}");
            return;
        }

        char json[4096];
        int offset = snprintf(json, sizeof(json), "[");
        for (int i = 0; i < count; i++) {
            offset += snprintf(json + offset, sizeof(json) - offset,
                "%s\"%s\"", i > 0 ? "," : "", macs[i]);
        }
        snprintf(json + offset, sizeof(json) - offset, "]");
        
        mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
            "%s", json);
        return;
    }

    /* POST - 添加到白名单 */
    if (hm->method.len == 4 && memcmp(hm->method.buf, "POST", 4) == 0) {
        char mac[32] = {0};
        extract_json_string(hm->body.buf, "mac", mac, sizeof(mac));
        
        if (strlen(mac) < 17) {
            mg_http_reply(c, 400, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
                "{\"error\":\"MAC地址无效\"}");
            return;
        }

        if (wifi_whitelist_add(mac) == 0) {
            mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
                "{\"status\":\"success\",\"message\":\"已添加到白名单\"}");
        } else {
            mg_http_reply(c, 500, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
                "{\"error\":\"添加白名单失败\"}");
        }
        return;
    }

    /* DELETE - 删除或清空白名单 */
    if (hm->method.len == 6 && memcmp(hm->method.buf, "DELETE", 6) == 0) {
        /* 检查URL是否包含MAC地址 */
        const char *mac_start = strstr(hm->uri.buf, "/api/wifi/whitelist/");
        if (mac_start && strlen(mac_start) > 20) {
            mac_start += 20;
            char mac[32] = {0};
            size_t len = 0;
            while (mac_start[len] && mac_start[len] != '?' && mac_start[len] != ' ' && len < 17) {
                mac[len] = mac_start[len];
                len++;
            }
            mac[len] = '\0';
            
            if (len >= 17) {
                if (wifi_whitelist_del(mac) == 0) {
                    mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
                        "{\"status\":\"success\",\"message\":\"已从白名单移除\"}");
                } else {
                    mg_http_reply(c, 500, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
                        "{\"error\":\"移除白名单失败\"}");
                }
                return;
            }
        }
        
        /* 无MAC参数则清空白名单 */
        if (wifi_whitelist_clear() == 0) {
            mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
                "{\"status\":\"success\",\"message\":\"白名单已清空\"}");
        } else {
            mg_http_reply(c, 500, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
                "{\"error\":\"清空白名单失败\"}");
        }
        return;
    }

    mg_http_reply(c, 405, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
        "{\"error\":\"Method not allowed\"}");
}

/* ==================== OTA更新 API ==================== */
#include "update.h"

/* GET /api/update/version - 获取当前版本 */
void handle_update_version(struct mg_connection *c, struct mg_http_message *hm) {
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
        "{\"version\":\"%s\"}", update_get_version());
}

/* POST /api/update/upload - 上传更新包 */
void handle_update_upload(struct mg_connection *c, struct mg_http_message *hm) {
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: POST, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    /* 解析multipart表单获取文件 */
    struct mg_http_part part;
    size_t ofs = 0;
    
    while ((ofs = mg_http_next_multipart(hm->body, ofs, &part)) > 0) {
        if (part.filename.len > 0) {
            /* 清理旧文件 */
            update_cleanup();
            
            /* 保存上传的文件 */
            FILE *fp = fopen(UPDATE_ZIP_PATH, "wb");
            if (!fp) {
                mg_http_reply(c, 500, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
                    "{\"error\":\"无法创建文件\"}");
                return;
            }
            
            fwrite(part.body.buf, 1, part.body.len, fp);
            fclose(fp);
            
            printf("更新包上传成功: %lu bytes\n", (unsigned long)part.body.len);
            mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
                "{\"status\":\"success\",\"message\":\"上传成功\",\"size\":%lu}", (unsigned long)part.body.len);
            return;
        }
    }
    
    mg_http_reply(c, 400, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
        "{\"error\":\"未找到上传文件\"}");
}

/* POST /api/update/download - 从URL下载更新包 */
void handle_update_download(struct mg_connection *c, struct mg_http_message *hm) {
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: POST, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    char url[512] = {0};
    extract_json_string(hm->body.buf, "url", url, sizeof(url));
    
    if (strlen(url) == 0) {
        mg_http_reply(c, 400, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
            "{\"error\":\"URL参数不能为空\"}");
        return;
    }

    if (update_download(url) == 0) {
        mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
            "{\"status\":\"success\",\"message\":\"下载成功\"}");
    } else {
        mg_http_reply(c, 500, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
            "{\"error\":\"下载失败\"}");
    }
}

/* POST /api/update/extract - 解压更新包 */
void handle_update_extract(struct mg_connection *c, struct mg_http_message *hm) {
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: POST, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    if (update_extract() == 0) {
        mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
            "{\"status\":\"success\",\"message\":\"解压成功\"}");
    } else {
        mg_http_reply(c, 500, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
            "{\"error\":\"解压失败\"}");
    }
}

/* POST /api/update/install - 执行安装并重启 */
void handle_update_install(struct mg_connection *c, struct mg_http_message *hm) {
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: POST, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    char output[2048] = {0};
    
    if (update_install(output, sizeof(output)) == 0) {
        /* 先返回响应 */
        char escaped[1024];
        json_escape_string(output, escaped, sizeof(escaped));
        mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
            "{\"status\":\"success\",\"message\":\"安装成功，正在重启...\",\"output\":\"%s\"}", escaped);
        /* 刷新连接 */
        c->is_draining = 1;
        /* 延迟2秒后重启 */
        sleep(2);
        device_reboot();
    } else {
        char escaped[1024];
        json_escape_string(output, escaped, sizeof(escaped));
        mg_http_reply(c, 500, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
            "{\"error\":\"安装失败\",\"output\":\"%s\"}", escaped);
    }
}

/* GET /api/update/check - 检查远程版本 */
void handle_update_check(struct mg_connection *c, struct mg_http_message *hm) {
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    /* 使用编译时嵌入的默认URL */
    const char *check_url = UPDATE_CHECK_URL;

    update_info_t info;
    if (update_check_version(check_url, &info) == 0) {
        const char *current = update_get_version();
        int has_update = strcmp(info.version, current) > 0 ? 1 : 0;
        
        char escaped_changelog[2048];
        json_escape_string(info.changelog, escaped_changelog, sizeof(escaped_changelog));
        
        mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
            "{\"current_version\":\"%s\",\"latest_version\":\"%s\",\"has_update\":%s,"
            "\"url\":\"%s\",\"changelog\":\"%s\",\"size\":%lu,\"required\":%s}",
            current, info.version, has_update ? "true" : "false",
            info.url, escaped_changelog, (unsigned long)info.size, info.required ? "true" : "false");
    } else {
        mg_http_reply(c, 500, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
            "{\"error\":\"检查版本失败\"}");
    }
}

/* 已删除 /api/update/config - 版本检查URL已嵌入程序 */

/* GET /api/get/time - 获取系统时间 */
void handle_get_system_time(struct mg_connection *c, struct mg_http_message *hm) {
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    
    char datetime[64];
    strftime(datetime, sizeof(datetime), "%Y-%m-%d %H:%M:%S", tm_info);
    
    char date[16], time_str[16];
    strftime(date, sizeof(date), "%Y-%m-%d", tm_info);
    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);
    
    mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
        "{\"Code\":0,\"Data\":{\"datetime\":\"%s\",\"date\":\"%s\",\"time\":\"%s\",\"timestamp\":%ld}}",
        datetime, date, time_str, (long)now);
}

/* POST /api/set/time - NTP同步系统时间 */
void handle_set_system_time(struct mg_connection *c, struct mg_http_message *hm) {
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    char output[512];
    
    /* NTP 服务器列表，依次尝试 */
    const char *ntp_servers[] = {
        "ntp.aliyun.com",
        "pool.ntp.org",
        "time.windows.com",
        NULL
    };
    
    int success = 0;
    const char *used_server = NULL;
    
    for (int i = 0; ntp_servers[i] != NULL; i++) {
        if (run_command(output, sizeof(output), "ntpdate", ntp_servers[i], NULL) == 0) {
            success = 1;
            used_server = ntp_servers[i];
            break;
        }
    }
    
    if (success) {
        /* 同步到硬件时钟 */
        run_command(output, sizeof(output), "hwclock", "-w", NULL);
        mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
            "{\"Code\":0,\"Data\":\"NTP同步成功\",\"server\":\"%s\"}", used_server);
    } else {
        mg_http_reply(c, 500, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
            "{\"Code\":1,\"Error\":\"所有NTP服务器同步失败\"}");
    }
}

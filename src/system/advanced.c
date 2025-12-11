/**
 * @file advanced.c
 * @brief 高级网络功能实现 (Go: handlers/advanced.go)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>
#include "mongoose.h"
#include "advanced.h"
#include "dbus_core.h"
#include "exec_utils.h"

/* 频段映射结构 */
typedef struct {
    const char *name;
    const char *mode;  /* "4G" or "5G" */
    const char *type;  /* "TDD" or "FDD" */
    int value;
} BandMapping;

/* 频段映射表 */
static const BandMapping band_map[] = {
    {"TDD_34", "4G", "TDD", 2},
    {"TDD_38", "4G", "TDD", 32},
    {"TDD_39", "4G", "TDD", 64},
    {"TDD_40", "4G", "TDD", 128},
    {"TDD_41", "4G", "TDD", 256},
    {"FDD_01", "4G", "FDD", 1},
    {"FDD_03", "4G", "FDD", 4},
    {"FDD_05", "4G", "FDD", 16},
    {"FDD_08", "4G", "FDD", 128},
    {"N01", "5G", "FDD", 1},
    {"N08", "5G", "FDD", 128},
    {"N28", "5G", "FDD", 512},
    {"N41", "5G", "TDD", 16},
    {"N77", "5G", "TDD", 128},
    {"N78", "5G", "TDD", 256},
    {"N79", "5G", "TDD", 512},
    {NULL, NULL, NULL, 0}
};

/* 解析频段锁定状态 */
static void parse_bands_info(const char *output4G, const char *output5G, int *bands) {
    /* 初始化所有频段为未锁定 */
    memset(bands, 0, 16 * sizeof(int));

    /* 解析4G频段: +SPLBAND: 0,tdd,0,fdd,0 */
    if (output4G && strlen(output4G) > 0) {
        int tdd = 0, fdd = 0;
        char *p = strstr(output4G, "+SPLBAND:");
        if (p) {
            sscanf(p, "+SPLBAND: 0,%d,0,%d,0", &tdd, &fdd);
            /* 4G TDD */
            if (tdd & 2) bands[0] = 1;    /* TDD_34 */
            if (tdd & 32) bands[1] = 1;   /* TDD_38 */
            if (tdd & 64) bands[2] = 1;   /* TDD_39 */
            if (tdd & 128) bands[3] = 1;  /* TDD_40 */
            if (tdd & 256) bands[4] = 1;  /* TDD_41 */
            /* 4G FDD */
            if (fdd & 1) bands[5] = 1;    /* FDD_01 */
            if (fdd & 4) bands[6] = 1;    /* FDD_03 */
            if (fdd & 16) bands[7] = 1;   /* FDD_05 */
            if (fdd & 128) bands[8] = 1;  /* FDD_08 */
        }
    }

    /* 解析5G频段: +SPLBAND: fdd,0,tdd,0 */
    if (output5G && strlen(output5G) > 0) {
        int fdd = 0, tdd = 0;
        char *p = strstr(output5G, "+SPLBAND:");
        if (p) {
            sscanf(p, "+SPLBAND: %d,0,%d,0", &fdd, &tdd);
            /* 5G FDD */
            if (fdd & 1) bands[9] = 1;     /* N01 */
            if (fdd & 128) bands[10] = 1;  /* N08 */
            if (fdd & 512) bands[11] = 1;  /* N28 */
            /* 5G TDD */
            if (tdd & 16) bands[12] = 1;   /* N41 */
            if (tdd & 128) bands[13] = 1;  /* N77 */
            if (tdd & 256) bands[14] = 1;  /* N78 */
            if (tdd & 512) bands[15] = 1;  /* N79 */
        }
    }
}

/* GET /api/bands - 获取频段状态 */
void handle_get_bands(struct mg_connection *c, struct mg_http_message *hm) {
    /* OPTIONS 预检 */
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    char *result4G = NULL, *result5G = NULL;
    int bands[16] = {0};

    printf("开始获取频段锁定状态...\n");

    /* 查询4G频段 */
    if (execute_at("AT+SPLBAND=0", &result4G) == 0) {
        printf("4G频段查询结果: %s\n", result4G);
    }

    /* 查询5G频段 */
    if (execute_at("AT+SPLBAND=3", &result5G) == 0) {
        printf("5G频段查询结果: %s\n", result5G);
    }

    parse_bands_info(result4G, result5G, bands);

    if (result4G) g_free(result4G);
    if (result5G) g_free(result5G);

    /* 构建 JSON 响应 */
    char json[2048];
    snprintf(json, sizeof(json),
        "{"
        "\"4G_TDD\":["
            "{\"name\":\"TDD_34\",\"label\":\"B34\",\"locked\":%s},"
            "{\"name\":\"TDD_38\",\"label\":\"B38\",\"locked\":%s},"
            "{\"name\":\"TDD_39\",\"label\":\"B39\",\"locked\":%s},"
            "{\"name\":\"TDD_40\",\"label\":\"B40\",\"locked\":%s},"
            "{\"name\":\"TDD_41\",\"label\":\"B41\",\"locked\":%s}"
        "],"
        "\"4G_FDD\":["
            "{\"name\":\"FDD_01\",\"label\":\"B1\",\"locked\":%s},"
            "{\"name\":\"FDD_03\",\"label\":\"B3\",\"locked\":%s},"
            "{\"name\":\"FDD_05\",\"label\":\"B5\",\"locked\":%s},"
            "{\"name\":\"FDD_08\",\"label\":\"B8\",\"locked\":%s}"
        "],"
        "\"5G\":["
            "{\"name\":\"N01\",\"label\":\"N1\",\"locked\":%s},"
            "{\"name\":\"N08\",\"label\":\"N8\",\"locked\":%s},"
            "{\"name\":\"N28\",\"label\":\"N28\",\"locked\":%s},"
            "{\"name\":\"N41\",\"label\":\"N41\",\"locked\":%s},"
            "{\"name\":\"N77\",\"label\":\"N77\",\"locked\":%s},"
            "{\"name\":\"N78\",\"label\":\"N78\",\"locked\":%s},"
            "{\"name\":\"N79\",\"label\":\"N79\",\"locked\":%s}"
        "]"
        "}",
        bands[0] ? "true" : "false", bands[1] ? "true" : "false",
        bands[2] ? "true" : "false", bands[3] ? "true" : "false",
        bands[4] ? "true" : "false", bands[5] ? "true" : "false",
        bands[6] ? "true" : "false", bands[7] ? "true" : "false",
        bands[8] ? "true" : "false", bands[9] ? "true" : "false",
        bands[10] ? "true" : "false", bands[11] ? "true" : "false",
        bands[12] ? "true" : "false", bands[13] ? "true" : "false",
        bands[14] ? "true" : "false", bands[15] ? "true" : "false"
    );

    mg_http_reply(c, 200,
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n",
        "%s", json);
}


/* 查找频段映射 */
static const BandMapping *find_band(const char *name) {
    for (int i = 0; band_map[i].name; i++) {
        if (strcmp(band_map[i].name, name) == 0) {
            return &band_map[i];
        }
    }
    return NULL;
}

/* 从 JSON 数组中提取频段名称 */
static int parse_bands_array(const char *json, char bands[][32], int max_bands) {
    int count = 0;
    const char *p = strstr(json, "\"bands\"");
    if (!p) return 0;
    
    p = strchr(p, '[');
    if (!p) return 0;
    p++;

    while (*p && count < max_bands) {
        /* 跳过空白 */
        while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t' || *p == ',') p++;
        if (*p == ']') break;
        if (*p == '"') {
            p++;
            char *end = strchr(p, '"');
            if (end) {
                size_t len = end - p;
                if (len < 32) {
                    memcpy(bands[count], p, len);
                    bands[count][len] = '\0';
                    count++;
                }
                p = end + 1;
            }
        } else {
            p++;
        }
    }
    return count;
}

/* POST /api/lock_bands - 锁定频段 */
void handle_lock_bands(struct mg_connection *c, struct mg_http_message *hm) {
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    char bands[32][32] = {{0}};
    int band_count = parse_bands_array(hm->body.buf, bands, 32);

    printf("收到锁频请求，要锁定的频段数量: %d\n", band_count);

    /* 计算位掩码 */
    int tdd4G = 0, fdd4G = 0, fdd5G = 0, tdd5G = 0;
    for (int i = 0; i < band_count; i++) {
        const BandMapping *bm = find_band(bands[i]);
        if (bm) {
            printf("处理频段 %s: 模式=%s, 类型=%s, 值=%d\n", bm->name, bm->mode, bm->type, bm->value);
            if (strcmp(bm->mode, "4G") == 0 && strcmp(bm->type, "TDD") == 0) {
                tdd4G |= bm->value;
            } else if (strcmp(bm->mode, "4G") == 0 && strcmp(bm->type, "FDD") == 0) {
                fdd4G |= bm->value;
            } else if (strcmp(bm->mode, "5G") == 0 && strcmp(bm->type, "TDD") == 0) {
                tdd5G |= bm->value;
            } else if (strcmp(bm->mode, "5G") == 0 && strcmp(bm->type, "FDD") == 0) {
                fdd5G |= bm->value;
            }
        }
    }

    printf("计算结果: 4G TDD=%d, 4G FDD=%d, 5G FDD=%d, 5G TDD=%d\n", tdd4G, fdd4G, fdd5G, tdd5G);

    char *result = NULL;
    char cmd[128];

    /* 执行命令序列 */
    /* 1. 关闭设备 */
    if (execute_at("AT+SFUN=5", &result) != 0) {
        mg_http_reply(c, 500, "Access-Control-Allow-Origin: *\r\n", "{\"error\":\"关闭设备失败\"}");
        if (result) g_free(result);
        return;
    }
    if (result) { g_free(result); result = NULL; }
    usleep(300000);

    /* 2. 解锁5G频段 */
    execute_at("AT+SPLBAND=2,0,0,0,0", &result);
    if (result) { g_free(result); result = NULL; }
    usleep(300000);

    /* 3. 锁定4G频段 */
    if (tdd4G != 0 || fdd4G != 0) {
        snprintf(cmd, sizeof(cmd), "AT+SPLBAND=1,0,%d,0,%d,0", tdd4G, fdd4G);
        execute_at(cmd, &result);
        if (result) { g_free(result); result = NULL; }
        usleep(300000);
    }

    /* 4. 锁定5G频段 */
    if (fdd5G != 0 || tdd5G != 0) {
        snprintf(cmd, sizeof(cmd), "AT+SPLBAND=2,%d,0,%d,0", fdd5G, tdd5G);
        execute_at(cmd, &result);
        if (result) { g_free(result); result = NULL; }
        usleep(300000);
    }

    /* 5. 开启设备 */
    execute_at("AT+SFUN=4", &result);
    if (result) { g_free(result); result = NULL; }
    usleep(300000);

    /* 6. 激活网络 */
    execute_at("AT+CGACT=0,1", &result);
    if (result) g_free(result);

    printf("频段锁定成功\n");
    mg_http_reply(c, 200,
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n",
        "{\"success\":true,\"message\":\"频段锁定成功\"}");
}


/* POST /api/unlock_bands - 解锁所有频段 */
void handle_unlock_bands(struct mg_connection *c, struct mg_http_message *hm) {
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    printf("开始解锁所有频段...\n");
    char *result = NULL;

    /* 1. 关闭设备 */
    if (execute_at("AT+SFUN=5", &result) != 0) {
        mg_http_reply(c, 500, "Access-Control-Allow-Origin: *\r\n", "{\"error\":\"关闭设备失败\"}");
        if (result) g_free(result);
        return;
    }
    if (result) { g_free(result); result = NULL; }
    usleep(300000);

    /* 2. 解锁4G频段 */
    execute_at("AT+SPLBAND=1,0,0,0,0,0", &result);
    if (result) { g_free(result); result = NULL; }
    usleep(300000);

    /* 3. 解锁5G频段 */
    execute_at("AT+SPLBAND=2,0,0,0,0", &result);
    if (result) { g_free(result); result = NULL; }
    usleep(300000);

    /* 4. 开启设备 */
    execute_at("AT+SFUN=4", &result);
    if (result) { g_free(result); result = NULL; }
    usleep(300000);

    /* 5. 激活网络 */
    execute_at("AT+CGACT=0,1", &result);
    if (result) g_free(result);

    printf("频段解锁成功\n");
    mg_http_reply(c, 200,
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n",
        "{\"success\":true,\"message\":\"频段解锁成功\"}");
}

/* 解析小区数据 (复用 handlers.c 中的函数) */
extern int parse_cell_to_vec(const char *input, char data[64][16][32]);

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

/* GET /api/cells - 获取小区信息 */
void handle_get_cells(struct mg_connection *c, struct mg_http_message *hm) {
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    printf("开始获取小区信息...\n");
    char *result = NULL;
    
    /* 通过 D-Bus 判断网络类型 (与 Go 版本一致) */
    int is_5g = is_5g_network();
    printf("检测到%s网络\n", is_5g ? "5G" : "4G");

    char json[8192] = "{\"Code\":0,\"Error\":\"\",\"Data\":[";
    int json_len = strlen(json);
    int cell_count = 0;

    if (is_5g) {
        /* 5G 主小区 */
        if (execute_at("AT+SPENGMD=0,14,1", &result) == 0 && result) {
            char data[64][16][32] = {{{0}}};
            int rows = parse_cell_to_vec(result, data);
            if (rows > 15) {
                char cell_json[512];
                snprintf(cell_json, sizeof(cell_json),
                    "{\"rat\":\"5G\",\"band\":\"N%s\",\"arfcn\":%d,\"pci\":%d,"
                    "\"rsrp\":%.2f,\"rsrq\":%.2f,\"sinr\":%.2f,\"isServing\":true}",
                    data[0][0], atoi(data[1][0]), atoi(data[2][0]),
                    atof(data[3][0]) / 100.0, atof(data[4][0]) / 100.0,
                    atof(data[15][0]) / 100.0);
                json_len += snprintf(json + json_len, sizeof(json) - json_len, "%s", cell_json);
                cell_count++;
            }
            g_free(result);
            result = NULL;
        }

        /* 5G 邻小区 */
        if (execute_at("AT+SPENGMD=0,14,2", &result) == 0 && result) {
            char data[64][16][32] = {{{0}}};
            int rows = parse_cell_to_vec(result, data);
            if (rows > 5) {
                int col_count = 0;
                for (int i = 0; i < 16 && data[0][i][0]; i++) col_count++;
                for (int i = 0; i < col_count; i++) {
                    int arfcn = atoi(data[1][i]);
                    int pci = atoi(data[2][i]);
                    if (arfcn == 0 || pci == 0) continue;
                    char cell_json[512];
                    snprintf(cell_json, sizeof(cell_json),
                        "%s{\"rat\":\"5G\",\"band\":\"N%s\",\"arfcn\":%d,\"pci\":%d,"
                        "\"rsrp\":%.2f,\"rsrq\":%.2f,\"sinr\":%.2f,\"isServing\":false}",
                        cell_count > 0 ? "," : "",
                        data[0][i], arfcn, pci,
                        atof(data[3][i]) / 100.0, atof(data[4][i]) / 100.0,
                        atof(data[5][i]) / 100.0);
                    json_len += snprintf(json + json_len, sizeof(json) - json_len, "%s", cell_json);
                    cell_count++;
                }
            }
            g_free(result);
        }
    } else {
        /* 4G 主小区 */
        if (execute_at("AT+SPENGMD=0,6,0", &result) == 0 && result) {
            char data[64][16][32] = {{{0}}};
            int rows = parse_cell_to_vec(result, data);
            if (rows > 33) {
                char cell_json[512];
                snprintf(cell_json, sizeof(cell_json),
                    "{\"rat\":\"4G\",\"band\":\"B%s\",\"arfcn\":%d,\"pci\":%d,"
                    "\"rsrp\":%.2f,\"rsrq\":%.2f,\"sinr\":%.2f,\"isServing\":true}",
                    data[0][0], atoi(data[1][0]), atoi(data[2][0]),
                    atof(data[3][0]) / 100.0, atof(data[4][0]) / 100.0,
                    atof(data[33][0]) / 100.0);
                json_len += snprintf(json + json_len, sizeof(json) - json_len, "%s", cell_json);
                cell_count++;
            }
            g_free(result);
            result = NULL;
        }

        /* 4G 邻小区 */
        if (execute_at("AT+SPENGMD=0,6,6", &result) == 0 && result) {
            char data[64][16][32] = {{{0}}};
            int rows = parse_cell_to_vec(result, data);
            for (int i = 0; i < rows; i++) {
                int arfcn = atoi(data[i][0]);
                int pci = atoi(data[i][1]);
                if (arfcn == 0 || pci == 0) continue;
                char cell_json[512];
                const char *band = (strlen(data[i][12]) > 0) ? data[i][12] : "0";
                snprintf(cell_json, sizeof(cell_json),
                    "%s{\"rat\":\"4G\",\"band\":\"B%s\",\"arfcn\":%d,\"pci\":%d,"
                    "\"rsrp\":%.2f,\"rsrq\":%.2f,\"sinr\":%.2f,\"isServing\":false}",
                    cell_count > 0 ? "," : "",
                    band, arfcn, pci,
                    atof(data[i][2]) / 100.0, atof(data[i][3]) / 100.0,
                    atof(data[i][6]) / 100.0);
                json_len += snprintf(json + json_len, sizeof(json) - json_len, "%s", cell_json);
                cell_count++;
            }
            g_free(result);
        }
    }

    snprintf(json + json_len, sizeof(json) - json_len, "]}");
    printf("小区信息获取完成，共 %d 个小区\n", cell_count);

    mg_http_reply(c, 200,
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n",
        "%s", json);
}


/* POST /api/lock_cell - 锁定小区 */
void handle_lock_cell(struct mg_connection *c, struct mg_http_message *hm) {
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    char technology[32] = {0}, arfcn[32] = {0}, pci[32] = {0};

    /* 解析 JSON */
    char *p = strstr(hm->body.buf, "\"technology\"");
    if (p) {
        p = strchr(p, ':'); if (p) p = strchr(p, '"'); if (p) { p++;
        char *end = strchr(p, '"');
        if (end) { size_t len = end - p; if (len < 32) { memcpy(technology, p, len); } }
    }}

    p = strstr(hm->body.buf, "\"arfcn\"");
    if (p) {
        p = strchr(p, ':'); if (p) p = strchr(p, '"'); if (p) { p++;
        char *end = strchr(p, '"');
        if (end) { size_t len = end - p; if (len < 32) { memcpy(arfcn, p, len); } }
    }}

    p = strstr(hm->body.buf, "\"pci\"");
    if (p) {
        p = strchr(p, ':'); if (p) p = strchr(p, '"'); if (p) { p++;
        char *end = strchr(p, '"');
        if (end) { size_t len = end - p; if (len < 32) { memcpy(pci, p, len); } }
    }}

    printf("收到锁小区请求: Technology=%s, ARFCN=%s, PCI=%s\n", technology, arfcn, pci);

    /* 确定 band 参数 */
    const char *band = "12"; /* 4G */
    if (strstr(technology, "5G") || strstr(technology, "NR") ||
        strstr(technology, "5g") || strstr(technology, "nr")) {
        band = "16"; /* 5G */
    }

    char *result = NULL;
    char cmd[128];

    /* 1. 关闭射频 */
    execute_at("AT+SFUN=5", &result);
    if (result) { g_free(result); result = NULL; }
    usleep(300000);

    /* 2. 解锁4G */
    execute_at("AT+SPFORCEFRQ=12,0", &result);
    if (result) { g_free(result); result = NULL; }
    usleep(300000);

    /* 3. 解锁5G */
    execute_at("AT+SPFORCEFRQ=16,0", &result);
    if (result) { g_free(result); result = NULL; }
    usleep(300000);

    /* 4. 锁定小区 */
    snprintf(cmd, sizeof(cmd), "AT+SPFORCEFRQ=%s,2,%s,%s", band, arfcn, pci);
    execute_at(cmd, &result);
    if (result) { g_free(result); result = NULL; }
    usleep(300000);

    /* 5. 打开射频 */
    execute_at("AT+SFUN=4", &result);
    if (result) { g_free(result); result = NULL; }
    usleep(300000);

    /* 6. 激活网络 */
    execute_at("AT+CGACT=0,1", &result);
    if (result) g_free(result);

    printf("小区锁定成功\n");
    mg_http_reply(c, 200,
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n",
        "{\"Code\":0,\"Error\":\"\",\"Data\":{\"success\":true,\"message\":\"小区锁定成功\"}}");
}

/* POST /api/unlock_cell - 解锁小区 */
void handle_unlock_cell(struct mg_connection *c, struct mg_http_message *hm) {
    if (hm->method.len == 7 && memcmp(hm->method.buf, "OPTIONS", 7) == 0) {
        mg_http_reply(c, 200, "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n", "");
        return;
    }

    printf("开始解锁小区...\n");
    char *result = NULL;

    /* 1. 关闭射频 */
    execute_at("AT+SFUN=5", &result);
    if (result) { g_free(result); result = NULL; }
    usleep(300000);

    /* 2. 解锁4G */
    execute_at("AT+SPFORCEFRQ=12,0", &result);
    if (result) { g_free(result); result = NULL; }
    usleep(300000);

    /* 3. 解锁5G */
    execute_at("AT+SPFORCEFRQ=16,0", &result);
    if (result) { g_free(result); result = NULL; }
    usleep(300000);

    /* 4. 打开射频 */
    execute_at("AT+SFUN=4", &result);
    if (result) { g_free(result); result = NULL; }
    usleep(300000);

    /* 5. 激活网络 */
    execute_at("AT+CGACT=0,1", &result);
    if (result) g_free(result);

    printf("小区解锁成功\n");
    mg_http_reply(c, 200,
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n",
        "{\"Code\":0,\"Error\":\"\",\"Data\":{\"success\":true,\"message\":\"小区解锁成功\"}}");
}

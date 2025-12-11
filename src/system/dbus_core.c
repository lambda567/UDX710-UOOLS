/**
 * @file dbus_core.c
 * @brief D-Bus 核心连接管理实现 (对应 Go: system/dbus.go)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <gio/gio.h>
#include "dbus_core.h"
#include "sysinfo.h"

/* 常量定义 */
#define OFONO_SERVICE       "org.ofono"
#define OFONO_MODEM_IFACE   "org.ofono.Modem"
#define DEFAULT_MODEM_PATH  "/ril_0"
#define AT_COMMAND_TIMEOUT  8000  /* 8秒超时 (毫秒) */
#define MAX_RETRIES         1

/* 全局变量 */
static GDBusConnection *g_dbus_conn = NULL;
static GDBusProxy *g_modem_proxy = NULL;
static pthread_mutex_t g_at_mutex = PTHREAD_MUTEX_INITIALIZER;
static char g_last_error[512] = {0};
static char g_modem_path[64] = DEFAULT_MODEM_PATH;

/* 设置错误信息 */
static void set_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_last_error, sizeof(g_last_error), fmt, args);
    va_end(args);
}

const char *dbus_get_last_error(void) {
    return g_last_error;
}

int is_dbus_initialized(void) {
    return (g_dbus_conn != NULL && g_modem_proxy != NULL) ? 1 : 0;
}


int init_dbus(void) {
    GError *error = NULL;

    if (g_dbus_conn != NULL) {
        return 0;  /* 已初始化 */
    }

    /* 动态获取当前卡槽路径 */
    char slot[16], ril_path[32];
    if (get_current_slot(slot, ril_path) == 0 && strcmp(ril_path, "unknown") != 0) {
        strncpy(g_modem_path, ril_path, sizeof(g_modem_path) - 1);
        g_modem_path[sizeof(g_modem_path) - 1] = '\0';
        printf("D-Bus 使用卡槽: %s (%s)\n", slot, g_modem_path);
    } else {
        printf("D-Bus 使用默认卡槽: %s\n", g_modem_path);
    }

    /* 获取系统 D-Bus 连接 */
    g_dbus_conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (!g_dbus_conn) {
        set_error("连接系统 D-Bus 失败: %s", error ? error->message : "unknown");
        if (error) g_error_free(error);
        return -1;
    }

    /* 创建 oFono Modem 代理对象 */
    g_modem_proxy = g_dbus_proxy_new_sync(
        g_dbus_conn,
        G_DBUS_PROXY_FLAGS_NONE,
        NULL,                       /* GDBusInterfaceInfo */
        OFONO_SERVICE,
        g_modem_path,
        OFONO_MODEM_IFACE,
        NULL,                       /* GCancellable */
        &error
    );

    if (!g_modem_proxy) {
        set_error("创建 oFono Modem 代理失败: %s", error ? error->message : "unknown");
        if (error) g_error_free(error);
        g_object_unref(g_dbus_conn);
        g_dbus_conn = NULL;
        return -1;
    }

    printf("D-Bus 连接和 oFono Modem 对象初始化成功 (路径: %s)\n", g_modem_path);
    return 0;
}

void close_dbus(void) {
    if (g_modem_proxy) {
        g_object_unref(g_modem_proxy);
        g_modem_proxy = NULL;
    }
    if (g_dbus_conn) {
        g_object_unref(g_dbus_conn);
        g_dbus_conn = NULL;
    }
    printf("D-Bus 连接已关闭\n");
}


/* 验证 AT 命令格式 */
static int validate_at_command(const char *cmd) {
    if (!cmd || strlen(cmd) < 2) return 0;
    /* 检查是否以 AT 开头 (不区分大小写) */
    if ((cmd[0] == 'A' || cmd[0] == 'a') && (cmd[1] == 'T' || cmd[1] == 't')) {
        return 1;
    }
    return 0;
}

int execute_at(const char *command, char **result) {
    GError *error = NULL;
    GVariant *ret = NULL;
    int rc = -1;
    int retry;

    if (!command || !result) {
        set_error("无效的参数");
        return -1;
    }
    *result = NULL;

    /* 去除首尾空白 */
    while (*command == ' ' || *command == '\t') command++;

    /* 验证 AT 命令格式 */
    if (!validate_at_command(command)) {
        set_error("无效的 AT 命令格式: %s", command);
        return -1;
    }

    /* 检查 D-Bus 是否已初始化 */
    if (!is_dbus_initialized()) {
        printf("D-Bus 未初始化，尝试初始化...\n");
        if (init_dbus() != 0) {
            return -1;
        }
    }

    /* 获取互斥锁，确保串行执行 */
    pthread_mutex_lock(&g_at_mutex);

    printf("准备发送 AT 命令: %s\n", command);

    /* 重试逻辑 */
    for (retry = 0; retry <= MAX_RETRIES; retry++) {
        error = NULL;

        /* 调用 oFono 的 SendAtcmd 方法 */
        ret = g_dbus_proxy_call_sync(
            g_modem_proxy,
            "SendAtcmd",
            g_variant_new("(s)", command),
            G_DBUS_CALL_FLAGS_NONE,
            AT_COMMAND_TIMEOUT,
            NULL,
            &error
        );

        if (!ret) {
            printf("调用 SendAtcmd 失败 (尝试 %d/%d) (%s): %s\n",
                   retry + 1, MAX_RETRIES + 1, command,
                   error ? error->message : "unknown");

            /* 检测连接关闭错误 */
            if (error && strstr(error->message, "connection closed")) {
                printf("检测到连接关闭，尝试重新初始化 D-Bus...\n");
                g_error_free(error);
                close_dbus();
                if (init_dbus() != 0) {
                    set_error("重新初始化 D-Bus 失败");
                    break;
                }
                continue;
            }

            /* 检测操作进行中错误 */
            if (error && strstr(error->message, "Operation already in progress")) {
                printf("检测到 'Operation already in progress'，500ms 后重试...\n");
                g_error_free(error);
                g_usleep(500000);  /* 500ms */
                continue;
            }

            set_error("调用 SendAtcmd 失败: %s", error ? error->message : "unknown");
            if (error) g_error_free(error);
            break;
        }

        /* 提取结果字符串 */
        const gchar *res_str = NULL;
        g_variant_get(ret, "(s)", &res_str);

        if (res_str) {
            /* 去除首尾空白 */
            *result = g_strdup(res_str);
            g_strstrip(*result);
            printf("AT 命令 (%s) 响应: %s\n", command, *result);
            rc = 0;
        } else {
            set_error("空响应");
        }

        g_variant_unref(ret);
        break;
    }

    pthread_mutex_unlock(&g_at_mutex);
    return rc;
}

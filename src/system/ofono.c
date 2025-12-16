/**
 * @file ofono.c
 * @brief ofono D-Bus 接口实现
 */

#include <stdio.h>
#include <string.h>
#include "ofono.h"

/* 全局 D-Bus 连接 */
static GDBusConnection *g_dbus_conn = NULL;

/* 检查 D-Bus 连接是否有效 */
static int is_connection_valid(void) {
    if (!g_dbus_conn) {
        return 0;
    }
    if (g_dbus_connection_is_closed(g_dbus_conn)) {
        g_object_unref(g_dbus_conn);
        g_dbus_conn = NULL;
        return 0;
    }
    return 1;
}

/* 确保 D-Bus 连接有效，如果无效则重新连接 */
static int ensure_connection(void) {
    GError *error = NULL;

    if (is_connection_valid()) {
        return 1;
    }

    g_dbus_conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (!g_dbus_conn) {
        if (error) g_error_free(error);
        return 0;
    }
    return 1;
}

int ofono_init(void) {
    GError *error = NULL;

    if (g_dbus_conn != NULL && is_connection_valid()) {
        return 1;
    }

    g_dbus_conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (!g_dbus_conn) {
        if (error) g_error_free(error);
        return 0;
    }
    return 1;
}

int ofono_is_initialized(void) {
    return is_connection_valid();
}

void ofono_deinit(void) {
    if (g_dbus_conn) {
        g_object_unref(g_dbus_conn);
        g_dbus_conn = NULL;
    }
}

int ofono_network_get_mode_sync(const char* modem_path, char* buffer, int size, int timeout_ms) {
    GError *error = NULL;
    GVariant *result = NULL;
    GDBusProxy *proxy = NULL;
    int ret = -1;

    if (!modem_path || !buffer || size <= 0) {
        return -1;
    }

    if (!ensure_connection()) {
        return -1;
    }

    proxy = g_dbus_proxy_new_sync(
        g_dbus_conn, G_DBUS_PROXY_FLAGS_NONE, NULL,
        OFONO_SERVICE, modem_path, OFONO_RADIO_SETTINGS,
        NULL, &error
    );

    if (!proxy) {
        if (error) g_error_free(error);
        return -1;
    }

    result = g_dbus_proxy_call_sync(
        proxy, "GetProperties", NULL,
        G_DBUS_CALL_FLAGS_NONE, timeout_ms, NULL, &error
    );

    if (!result) {
        if (error) g_error_free(error);
        g_object_unref(proxy);
        return -1;
    }

    GVariant *props = g_variant_get_child_value(result, 0);
    if (props) {
        GVariantIter iter;
        const gchar *key;
        GVariant *value;

        g_variant_iter_init(&iter, props);
        while (g_variant_iter_next(&iter, "{&sv}", &key, &value)) {
            if (g_strcmp0(key, "TechnologyPreference") == 0) {
                const gchar *mode = g_variant_get_string(value, NULL);
                if (mode) {
                    strncpy(buffer, mode, size - 1);
                    buffer[size - 1] = '\0';
                    ret = 0;
                }
                g_variant_unref(value);
                break;
            }
            g_variant_unref(value);
        }
        g_variant_unref(props);
    }

    g_variant_unref(result);
    g_object_unref(proxy);
    return ret;
}

char* ofono_get_datacard(void) {
    GError *error = NULL;
    GVariant *result = NULL;
    char *datacard_path = NULL;

    if (!g_dbus_conn) {
        return NULL;
    }

    result = g_dbus_connection_call_sync(
        g_dbus_conn, OFONO_SERVICE, "/", "org.ofono.Manager",
        "GetDataCard", NULL, G_VARIANT_TYPE("(o)"),
        G_DBUS_CALL_FLAGS_NONE, 5000, NULL, &error
    );

    if (!result) {
        if (error) g_error_free(error);
        return NULL;
    }

    const gchar *path = NULL;
    g_variant_get(result, "(&o)", &path);
    if (path && strlen(path) > 0) {
        datacard_path = g_strdup(path);
    }

    g_variant_unref(result);
    return datacard_path;
}


/* 网络模式映射表 - 索引对应 ofono TechnologyPreference */
static const char* network_modes[] = {
    "WCDMA preferred",           /* 0 */
    "GSM only",                  /* 1 */
    "WCDMA only",                /* 2 */
    "GSM/WCDMA auto",            /* 3 */
    "LTE/GSM/WCDMA auto",        /* 4 */
    "LTE only",                  /* 5 */
    "LTE/WCDMA auto",            /* 6 */
    "NR 5G/LTE/GSM/WCDMA auto",  /* 7 */
    "NR 5G only",                /* 8 */
    "NR 5G/LTE auto",            /* 9 */
    "NSA only",                  /* 10 */
    NULL
};

const char* ofono_get_mode_name(int mode) {
    if (mode >= 0 && mode < 11) {
        return network_modes[mode];
    }
    return NULL;
}

int ofono_get_mode_count(void) {
    return 11;
}

int ofono_network_set_mode_sync(const char* modem_path, int mode, int timeout_ms) {
    GError *error = NULL;
    GVariant *result = NULL;
    GDBusProxy *proxy = NULL;
    const char *mode_str;

    if (!modem_path || !ensure_connection()) {
        return -1;
    }

    mode_str = ofono_get_mode_name(mode);
    if (!mode_str) {
        return -2;
    }

    proxy = g_dbus_proxy_new_sync(
        g_dbus_conn, G_DBUS_PROXY_FLAGS_NONE, NULL,
        OFONO_SERVICE, modem_path, OFONO_RADIO_SETTINGS,
        NULL, &error
    );

    if (!proxy) {
        if (error) g_error_free(error);
        return -3;
    }

    result = g_dbus_proxy_call_sync(
        proxy, "SetProperty",
        g_variant_new("(sv)", "TechnologyPreference", g_variant_new_string(mode_str)),
        G_DBUS_CALL_FLAGS_NONE, timeout_ms, NULL, &error
    );

    if (!result) {
        if (error) g_error_free(error);
        g_object_unref(proxy);
        return -4;
    }

    g_variant_unref(result);
    g_object_unref(proxy);
    return 0;
}

int ofono_modem_set_online(const char* modem_path, int online, int timeout_ms) {
    GError *error = NULL;
    GVariant *result = NULL;
    GDBusProxy *proxy = NULL;

    if (!modem_path || !ensure_connection()) {
        return -1;
    }

    proxy = g_dbus_proxy_new_sync(
        g_dbus_conn, G_DBUS_PROXY_FLAGS_NONE, NULL,
        OFONO_SERVICE, modem_path, "org.ofono.Modem",
        NULL, &error
    );

    if (!proxy) {
        if (error) g_error_free(error);
        return -2;
    }

    result = g_dbus_proxy_call_sync(
        proxy, "SetProperty",
        g_variant_new("(sv)", "Online", g_variant_new_boolean(online ? TRUE : FALSE)),
        G_DBUS_CALL_FLAGS_NONE, timeout_ms, NULL, &error
    );

    if (!result) {
        if (error) g_error_free(error);
        g_object_unref(proxy);
        return -3;
    }

    g_variant_unref(result);
    g_object_unref(proxy);
    return 0;
}


int ofono_set_datacard(const char* modem_path) {
    GError *error = NULL;
    GVariant *result = NULL;

    if (!modem_path || !ensure_connection()) {
        return 0;
    }

    result = g_dbus_connection_call_sync(
        g_dbus_conn, OFONO_SERVICE, "/", "org.ofono.Manager",
        "SetDataCard", g_variant_new("(o)", modem_path),
        NULL, G_DBUS_CALL_FLAGS_NONE, 5000, NULL, &error
    );

    if (!result) {
        if (error) g_error_free(error);
        return 0;
    }

    g_variant_unref(result);
    return 1;
}

int ofono_network_get_signal_strength(const char* modem_path, int* strength, int* dbm, int timeout_ms) {
    GError *error = NULL;
    GVariant *result = NULL;
    GDBusProxy *proxy = NULL;
    int ret = -1;

    if (!modem_path || !ensure_connection()) {
        return -1;
    }

    proxy = g_dbus_proxy_new_sync(
        g_dbus_conn, G_DBUS_PROXY_FLAGS_NONE, NULL,
        OFONO_SERVICE, modem_path, "org.ofono.NetworkRegistration",
        NULL, &error
    );

    if (!proxy) {
        if (error) g_error_free(error);
        return -2;
    }

    result = g_dbus_proxy_call_sync(
        proxy, "GetProperties", NULL,
        G_DBUS_CALL_FLAGS_NONE, timeout_ms, NULL, &error
    );

    if (!result) {
        if (error) g_error_free(error);
        g_object_unref(proxy);
        return -3;
    }

    GVariant *props = g_variant_get_child_value(result, 0);
    if (props) {
        GVariantIter iter;
        const gchar *key;
        GVariant *value;

        g_variant_iter_init(&iter, props);
        while (g_variant_iter_next(&iter, "{&sv}", &key, &value)) {
            if (g_strcmp0(key, "Strength") == 0 && strength) {
                *strength = g_variant_get_byte(value);
                ret = 0;
            }
            g_variant_unref(value);
        }
        g_variant_unref(props);
    }

    if (ret == 0 && dbm && strength) {
        *dbm = 113 - 2 * (*strength);
    }

    g_variant_unref(result);
    g_object_unref(proxy);
    return ret;
}

/* ==================== 数据连接和漫游 API ==================== */

#define OFONO_CONNECTION_CONTEXT  "org.ofono.ConnectionContext"
#define OFONO_CONNECTION_MANAGER  "org.ofono.ConnectionManager"
#define OFONO_NETWORK_REGISTRATION "org.ofono.NetworkRegistration"
#define DEFAULT_CONTEXT_PATH      "/ril_0/context2"
#define DEFAULT_MODEM_PATH        "/ril_0"

/**
 * 动态查找第一个有效的 internet 类型 context 路径
 * 遍历所有 context，优先返回配置了 APN 的 internet 类型 context
 * 
 * @param path_buf 输出缓冲区，存储找到的 context 路径
 * @param buf_size 缓冲区大小
 * @return 0 成功，-1 失败
 */
static int find_internet_context_path(char *path_buf, size_t buf_size) {
    GError *error = NULL;
    GVariant *result = NULL;
    GDBusProxy *proxy = NULL;
    int found = 0;
    char first_internet_path[256] = {0};

    if (!path_buf || buf_size == 0 || !ensure_connection()) {
        return -1;
    }

    /* 创建 ConnectionManager 代理 */
    proxy = g_dbus_proxy_new_sync(
        g_dbus_conn, G_DBUS_PROXY_FLAGS_NONE, NULL,
        OFONO_SERVICE, DEFAULT_MODEM_PATH, OFONO_CONNECTION_MANAGER,
        NULL, &error
    );

    if (!proxy) {
        if (error) g_error_free(error);
        /* 回退到默认路径 */
        strncpy(path_buf, DEFAULT_CONTEXT_PATH, buf_size - 1);
        return 0;
    }

    /* 调用 GetContexts 获取所有 context */
    result = g_dbus_proxy_call_sync(
        proxy, "GetContexts", NULL,
        G_DBUS_CALL_FLAGS_NONE, OFONO_TIMEOUT_MS, NULL, &error
    );

    if (!result) {
        if (error) g_error_free(error);
        g_object_unref(proxy);
        strncpy(path_buf, DEFAULT_CONTEXT_PATH, buf_size - 1);
        return 0;
    }

    /* 解析返回的 a(oa{sv}) 数组 */
    GVariant *array = g_variant_get_child_value(result, 0);
    GVariantIter iter;
    GVariant *child;

    g_variant_iter_init(&iter, array);
    while ((child = g_variant_iter_next_value(&iter)) != NULL) {
        const gchar *path = NULL;
        GVariant *props = NULL;

        g_variant_get(child, "(&o@a{sv})", &path, &props);

        if (props && path) {
            /* 获取 Type 属性 */
            GVariant *type_var = g_variant_lookup_value(props, "Type", G_VARIANT_TYPE_STRING);
            const gchar *context_type = type_var ? g_variant_get_string(type_var, NULL) : "";

            if (g_strcmp0(context_type, "internet") == 0) {
                /* 获取 AccessPointName 属性 */
                GVariant *apn_var = g_variant_lookup_value(props, "AccessPointName", G_VARIANT_TYPE_STRING);
                const gchar *apn = apn_var ? g_variant_get_string(apn_var, NULL) : "";

                /* 记录第一个 internet context */
                if (first_internet_path[0] == '\0') {
                    strncpy(first_internet_path, path, sizeof(first_internet_path) - 1);
                }

                /* 优先返回配置了 APN 的 context */
                if (apn && apn[0] != '\0') {
                    strncpy(path_buf, path, buf_size - 1);
                    found = 1;
                    if (apn_var) g_variant_unref(apn_var);
                    if (type_var) g_variant_unref(type_var);
                    g_variant_unref(props);
                    g_variant_unref(child);
                    break;
                }

                if (apn_var) g_variant_unref(apn_var);
            }

            if (type_var) g_variant_unref(type_var);
            g_variant_unref(props);
        }
        g_variant_unref(child);
    }

    g_variant_unref(array);
    g_variant_unref(result);
    g_object_unref(proxy);

    /* 如果没找到配置了 APN 的，使用第一个 internet context */
    if (!found) {
        if (first_internet_path[0] != '\0') {
            strncpy(path_buf, first_internet_path, buf_size - 1);
        } else {
            strncpy(path_buf, DEFAULT_CONTEXT_PATH, buf_size - 1);
        }
    }

    return 0;
}

int ofono_get_data_status(int *active) {
    GError *error = NULL;
    GVariant *result = NULL;
    GDBusProxy *proxy = NULL;
    int ret = -1;
    char context_path[256] = {0};

    if (!active || !ensure_connection()) {
        return -1;
    }

    /* 动态获取 internet context 路径 */
    if (find_internet_context_path(context_path, sizeof(context_path)) != 0) {
        return -1;
    }

    proxy = g_dbus_proxy_new_sync(
        g_dbus_conn, G_DBUS_PROXY_FLAGS_NONE, NULL,
        OFONO_SERVICE, context_path, OFONO_CONNECTION_CONTEXT,
        NULL, &error
    );

    if (!proxy) {
        if (error) g_error_free(error);
        return -2;
    }

    result = g_dbus_proxy_call_sync(
        proxy, "GetProperties", NULL,
        G_DBUS_CALL_FLAGS_NONE, OFONO_TIMEOUT_MS, NULL, &error
    );

    if (!result) {
        if (error) g_error_free(error);
        g_object_unref(proxy);
        return -3;
    }

    GVariant *props = g_variant_get_child_value(result, 0);
    if (props) {
        GVariantIter iter;
        const gchar *key;
        GVariant *value;

        g_variant_iter_init(&iter, props);
        while (g_variant_iter_next(&iter, "{&sv}", &key, &value)) {
            if (g_strcmp0(key, "Active") == 0) {
                *active = g_variant_get_boolean(value) ? 1 : 0;
                ret = 0;
            }
            g_variant_unref(value);
        }
        g_variant_unref(props);
    }

    g_variant_unref(result);
    g_object_unref(proxy);
    return ret;
}

int ofono_set_data_status(int active) {
    GError *error = NULL;
    GVariant *result = NULL;
    GDBusProxy *proxy = NULL;
    char context_path[256] = {0};

    if (!ensure_connection()) {
        return -1;
    }

    /* 动态获取 internet context 路径 */
    if (find_internet_context_path(context_path, sizeof(context_path)) != 0) {
        return -1;
    }

    proxy = g_dbus_proxy_new_sync(
        g_dbus_conn, G_DBUS_PROXY_FLAGS_NONE, NULL,
        OFONO_SERVICE, context_path, OFONO_CONNECTION_CONTEXT,
        NULL, &error
    );

    if (!proxy) {
        if (error) g_error_free(error);
        return -2;
    }

    result = g_dbus_proxy_call_sync(
        proxy, "SetProperty",
        g_variant_new("(sv)", "Active", g_variant_new_boolean(active ? TRUE : FALSE)),
        G_DBUS_CALL_FLAGS_NONE, OFONO_TIMEOUT_MS, NULL, &error
    );

    if (!result) {
        if (error) g_error_free(error);
        g_object_unref(proxy);
        return -3;
    }

    g_variant_unref(result);
    g_object_unref(proxy);
    return 0;
}

int ofono_get_roaming_status(int *roaming_allowed, int *is_roaming) {
    GError *error = NULL;
    GVariant *result = NULL;
    GDBusProxy *proxy = NULL;
    int ret = -1;

    if (!roaming_allowed || !is_roaming || !ensure_connection()) {
        return -1;
    }

    *roaming_allowed = 0;
    *is_roaming = 0;

    /* 1. 获取 ConnectionManager 的 RoamingAllowed 属性 */
    proxy = g_dbus_proxy_new_sync(
        g_dbus_conn, G_DBUS_PROXY_FLAGS_NONE, NULL,
        OFONO_SERVICE, DEFAULT_MODEM_PATH, OFONO_CONNECTION_MANAGER,
        NULL, &error
    );

    if (!proxy) {
        if (error) g_error_free(error);
        return -2;
    }

    result = g_dbus_proxy_call_sync(
        proxy, "GetProperties", NULL,
        G_DBUS_CALL_FLAGS_NONE, OFONO_TIMEOUT_MS, NULL, &error
    );

    if (result) {
        GVariant *props = g_variant_get_child_value(result, 0);
        if (props) {
            GVariantIter iter;
            const gchar *key;
            GVariant *value;

            g_variant_iter_init(&iter, props);
            while (g_variant_iter_next(&iter, "{&sv}", &key, &value)) {
                if (g_strcmp0(key, "RoamingAllowed") == 0) {
                    *roaming_allowed = g_variant_get_boolean(value) ? 1 : 0;
                    ret = 0;
                }
                g_variant_unref(value);
            }
            g_variant_unref(props);
        }
        g_variant_unref(result);
    } else {
        if (error) { g_error_free(error); error = NULL; }
    }

    g_object_unref(proxy);

    /* 2. 获取 NetworkRegistration 的 Status 属性判断是否漫游中 */
    proxy = g_dbus_proxy_new_sync(
        g_dbus_conn, G_DBUS_PROXY_FLAGS_NONE, NULL,
        OFONO_SERVICE, DEFAULT_MODEM_PATH, OFONO_NETWORK_REGISTRATION,
        NULL, &error
    );

    if (!proxy) {
        if (error) g_error_free(error);
        return ret;  /* 返回已获取的 roaming_allowed */
    }

    result = g_dbus_proxy_call_sync(
        proxy, "GetProperties", NULL,
        G_DBUS_CALL_FLAGS_NONE, OFONO_TIMEOUT_MS, NULL, &error
    );

    if (result) {
        GVariant *props = g_variant_get_child_value(result, 0);
        if (props) {
            GVariantIter iter;
            const gchar *key;
            GVariant *value;

            g_variant_iter_init(&iter, props);
            while (g_variant_iter_next(&iter, "{&sv}", &key, &value)) {
                if (g_strcmp0(key, "Status") == 0) {
                    const gchar *status = g_variant_get_string(value, NULL);
                    if (status && g_strcmp0(status, "roaming") == 0) {
                        *is_roaming = 1;
                    }
                }
                g_variant_unref(value);
            }
            g_variant_unref(props);
        }
        g_variant_unref(result);
    } else {
        if (error) g_error_free(error);
    }

    g_object_unref(proxy);
    return ret;
}

int ofono_set_roaming_allowed(int allowed) {
    GError *error = NULL;
    GVariant *result = NULL;
    GDBusProxy *proxy = NULL;

    if (!ensure_connection()) {
        return -1;
    }

    proxy = g_dbus_proxy_new_sync(
        g_dbus_conn, G_DBUS_PROXY_FLAGS_NONE, NULL,
        OFONO_SERVICE, DEFAULT_MODEM_PATH, OFONO_CONNECTION_MANAGER,
        NULL, &error
    );

    if (!proxy) {
        if (error) g_error_free(error);
        return -2;
    }

    result = g_dbus_proxy_call_sync(
        proxy, "SetProperty",
        g_variant_new("(sv)", "RoamingAllowed", g_variant_new_boolean(allowed ? TRUE : FALSE)),
        G_DBUS_CALL_FLAGS_NONE, OFONO_TIMEOUT_MS, NULL, &error
    );

    if (!result) {
        if (error) g_error_free(error);
        g_object_unref(proxy);
        return -3;
    }

    g_variant_unref(result);
    g_object_unref(proxy);
    return 0;
}


/* ==================== APN 管理 API ==================== */

int ofono_get_all_apn_contexts(ApnContext *contexts, int max_count) {
    GError *error = NULL;
    GVariant *result = NULL;
    GDBusProxy *proxy = NULL;
    int count = 0;

    if (!contexts || max_count <= 0 || !ensure_connection()) {
        return -1;
    }

    /* 创建 ConnectionManager 代理 */
    proxy = g_dbus_proxy_new_sync(
        g_dbus_conn, G_DBUS_PROXY_FLAGS_NONE, NULL,
        OFONO_SERVICE, DEFAULT_MODEM_PATH, OFONO_CONNECTION_MANAGER,
        NULL, &error
    );

    if (!proxy) {
        if (error) g_error_free(error);
        return -2;
    }

    /* 调用 GetContexts */
    result = g_dbus_proxy_call_sync(
        proxy, "GetContexts", NULL,
        G_DBUS_CALL_FLAGS_NONE, OFONO_TIMEOUT_MS, NULL, &error
    );

    if (!result) {
        if (error) g_error_free(error);
        g_object_unref(proxy);
        return -3;
    }

    /* 解析返回的 a(oa{sv}) 数组 */
    GVariant *array = g_variant_get_child_value(result, 0);
    GVariantIter iter;
    GVariant *child;

    g_variant_iter_init(&iter, array);
    while ((child = g_variant_iter_next_value(&iter)) != NULL && count < max_count) {
        const gchar *path = NULL;
        GVariant *props = NULL;

        g_variant_get(child, "(&o@a{sv})", &path, &props);

        if (props) {
            /* 获取 Type 属性 */
            GVariant *type_var = g_variant_lookup_value(props, "Type", G_VARIANT_TYPE_STRING);
            const gchar *context_type = type_var ? g_variant_get_string(type_var, NULL) : "";

            /* 只处理 internet 类型 */
            if (g_strcmp0(context_type, "internet") == 0) {
                ApnContext *ctx = &contexts[count];
                memset(ctx, 0, sizeof(ApnContext));

                /* 复制路径 */
                strncpy(ctx->path, path, APN_STRING_SIZE - 1);
                strncpy(ctx->context_type, context_type, sizeof(ctx->context_type) - 1);

                /* 获取各属性 */
                GVariant *v;
                
                v = g_variant_lookup_value(props, "Name", G_VARIANT_TYPE_STRING);
                if (v) { strncpy(ctx->name, g_variant_get_string(v, NULL), APN_STRING_SIZE - 1); g_variant_unref(v); }
                else { strcpy(ctx->name, "Internet"); }

                v = g_variant_lookup_value(props, "Active", G_VARIANT_TYPE_BOOLEAN);
                if (v) { ctx->active = g_variant_get_boolean(v) ? 1 : 0; g_variant_unref(v); }

                v = g_variant_lookup_value(props, "AccessPointName", G_VARIANT_TYPE_STRING);
                if (v) { strncpy(ctx->apn, g_variant_get_string(v, NULL), APN_STRING_SIZE - 1); g_variant_unref(v); }

                v = g_variant_lookup_value(props, "Protocol", G_VARIANT_TYPE_STRING);
                if (v) { strncpy(ctx->protocol, g_variant_get_string(v, NULL), sizeof(ctx->protocol) - 1); g_variant_unref(v); }
                else { strcpy(ctx->protocol, "ip"); }

                v = g_variant_lookup_value(props, "Username", G_VARIANT_TYPE_STRING);
                if (v) { strncpy(ctx->username, g_variant_get_string(v, NULL), APN_STRING_SIZE - 1); g_variant_unref(v); }

                v = g_variant_lookup_value(props, "Password", G_VARIANT_TYPE_STRING);
                if (v) { strncpy(ctx->password, g_variant_get_string(v, NULL), APN_STRING_SIZE - 1); g_variant_unref(v); }

                v = g_variant_lookup_value(props, "AuthenticationMethod", G_VARIANT_TYPE_STRING);
                if (v) { strncpy(ctx->auth_method, g_variant_get_string(v, NULL), sizeof(ctx->auth_method) - 1); g_variant_unref(v); }
                else { strcpy(ctx->auth_method, "chap"); }

                count++;
            }

            if (type_var) g_variant_unref(type_var);
            g_variant_unref(props);
        }
        g_variant_unref(child);
    }

    g_variant_unref(array);
    g_variant_unref(result);
    g_object_unref(proxy);

    return count;
}

int ofono_set_apn_property(const char *context_path, const char *property, const char *value) {
    GError *error = NULL;
    GVariant *result = NULL;
    GDBusProxy *proxy = NULL;

    if (!context_path || !property || !value || !ensure_connection()) {
        return -1;
    }

    proxy = g_dbus_proxy_new_sync(
        g_dbus_conn, G_DBUS_PROXY_FLAGS_NONE, NULL,
        OFONO_SERVICE, context_path, OFONO_CONNECTION_CONTEXT,
        NULL, &error
    );

    if (!proxy) {
        if (error) g_error_free(error);
        return -2;
    }

    result = g_dbus_proxy_call_sync(
        proxy, "SetProperty",
        g_variant_new("(sv)", property, g_variant_new_string(value)),
        G_DBUS_CALL_FLAGS_NONE, OFONO_TIMEOUT_MS, NULL, &error
    );

    if (!result) {
        if (error) g_error_free(error);
        g_object_unref(proxy);
        return -3;
    }

    g_variant_unref(result);
    g_object_unref(proxy);
    return 0;
}

int ofono_set_apn_properties(const char *context_path, 
                             const char *apn,
                             const char *protocol,
                             const char *username,
                             const char *password,
                             const char *auth_method) {
    GError *error = NULL;
    GVariant *result = NULL;
    GDBusProxy *proxy = NULL;
    int was_active = 0;

    if (!context_path || !ensure_connection()) {
        return -1;
    }

    /* 1. 检查 context 是否激活 */
    proxy = g_dbus_proxy_new_sync(
        g_dbus_conn, G_DBUS_PROXY_FLAGS_NONE, NULL,
        OFONO_SERVICE, context_path, OFONO_CONNECTION_CONTEXT,
        NULL, &error
    );

    if (!proxy) {
        if (error) g_error_free(error);
        return -2;
    }

    result = g_dbus_proxy_call_sync(
        proxy, "GetProperties", NULL,
        G_DBUS_CALL_FLAGS_NONE, OFONO_TIMEOUT_MS, NULL, &error
    );

    if (result) {
        GVariant *props = g_variant_get_child_value(result, 0);
        GVariant *active_var = g_variant_lookup_value(props, "Active", G_VARIANT_TYPE_BOOLEAN);
        if (active_var) {
            was_active = g_variant_get_boolean(active_var) ? 1 : 0;
            g_variant_unref(active_var);
        }
        g_variant_unref(props);
        g_variant_unref(result);
    } else {
        if (error) { g_error_free(error); error = NULL; }
    }

    g_object_unref(proxy);

    /* 2. 如果激活中，先关闭 */
    if (was_active) {
        proxy = g_dbus_proxy_new_sync(
            g_dbus_conn, G_DBUS_PROXY_FLAGS_NONE, NULL,
            OFONO_SERVICE, context_path, OFONO_CONNECTION_CONTEXT,
            NULL, &error
        );
        if (proxy) {
            result = g_dbus_proxy_call_sync(
                proxy, "SetProperty",
                g_variant_new("(sv)", "Active", g_variant_new_boolean(FALSE)),
                G_DBUS_CALL_FLAGS_NONE, OFONO_TIMEOUT_MS, NULL, &error
            );
            if (result) g_variant_unref(result);
            if (error) { g_error_free(error); error = NULL; }
            g_object_unref(proxy);
        }
        /* 等待状态稳定 */
        g_usleep(500000); /* 500ms */
    }

    /* 3. 设置各属性 */
    if (apn) {
        ofono_set_apn_property(context_path, "AccessPointName", apn);
    }
    if (protocol) {
        ofono_set_apn_property(context_path, "Protocol", protocol);
    }
    if (username) {
        ofono_set_apn_property(context_path, "Username", username);
    }
    if (password) {
        ofono_set_apn_property(context_path, "Password", password);
    }
    if (auth_method) {
        ofono_set_apn_property(context_path, "AuthenticationMethod", auth_method);
    }

    /* 4. 如果之前是激活状态，重新激活 */
    if (was_active) {
        g_usleep(500000); /* 500ms */
        proxy = g_dbus_proxy_new_sync(
            g_dbus_conn, G_DBUS_PROXY_FLAGS_NONE, NULL,
            OFONO_SERVICE, context_path, OFONO_CONNECTION_CONTEXT,
            NULL, &error
        );
        if (proxy) {
            result = g_dbus_proxy_call_sync(
                proxy, "SetProperty",
                g_variant_new("(sv)", "Active", g_variant_new_boolean(TRUE)),
                G_DBUS_CALL_FLAGS_NONE, OFONO_TIMEOUT_MS, NULL, &error
            );
            if (result) g_variant_unref(result);
            if (error) g_error_free(error);
            g_object_unref(proxy);
        }
    }

    return 0;
}

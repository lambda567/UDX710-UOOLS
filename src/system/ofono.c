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

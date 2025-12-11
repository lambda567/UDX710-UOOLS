/**
 * @file dbus_core.h
 * @brief D-Bus 核心连接管理 (对应 Go: system/dbus.go)
 */

#ifndef DBUS_CORE_H
#define DBUS_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 D-Bus 连接并获取 oFono Modem 对象
 * @return 0 成功, -1 失败
 */
int init_dbus(void);

/**
 * @brief 关闭 D-Bus 连接
 */
void close_dbus(void);

/**
 * @brief 检查 D-Bus 是否已初始化
 * @return 1 已初始化, 0 未初始化
 */
int is_dbus_initialized(void);

/**
 * @brief 执行 AT 命令 (带重试和超时)
 * @param command AT 命令字符串
 * @param result 返回结果指针 (调用者需用 g_free 释放)
 * @return 0 成功, -1 失败
 */
int execute_at(const char *command, char **result);

/**
 * @brief 获取最后一次错误信息
 * @return 错误信息字符串
 */
const char *dbus_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* DBUS_CORE_H */

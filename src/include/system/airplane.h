/**
 * @file airplane.h
 * @brief Airplane mode and SIM info (Go: system/airplane.go)
 */

#ifndef AIRPLANE_H
#define AIRPLANE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 通过 D-Bus 发送 AT 命令 (简单版本)
 * @param cmd AT 命令
 * @param result 返回结果 (调用者需 g_free)
 * @return 0 成功, -1 失败
 */
int send_at(const char *cmd, char **result);

/**
 * @brief 获取飞行模式状态
 * @return 1 飞行模式开启, 0 正常模式, -1 失败
 */
int get_airplane_mode(void);

/**
 * @brief 设置飞行模式
 * @param enabled 1 开启飞行模式, 0 关闭
 * @return 0 成功, -1 失败
 */
int set_airplane_mode(int enabled);

/**
 * @brief 获取 ICCID
 * @param iccid 输出缓冲区
 * @param size 缓冲区大小
 * @return 0 成功, -1 失败
 */
int get_iccid(char *iccid, size_t size);

/**
 * @brief 获取 IMEI
 * @param imei 输出缓冲区
 * @param size 缓冲区大小
 * @return 0 成功, -1 失败
 */
int get_imei(char *imei, size_t size);

/**
 * @brief 获取 IMSI
 * @param imsi 输出缓冲区
 * @param size 缓冲区大小
 * @return 0 成功, -1 失败
 */
int get_imsi(char *imsi, size_t size);

/**
 * @brief 根据 IMSI 获取运营商名称
 * @param imsi IMSI 字符串
 * @return 运营商名称
 */
const char *get_carrier_from_imsi(const char *imsi);

#ifdef __cplusplus
}
#endif

#endif /* AIRPLANE_H */

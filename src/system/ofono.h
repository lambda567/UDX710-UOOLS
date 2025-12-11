/**
 * @file ofono.h
 * @brief ofono D-Bus 接口封装
 */

#ifndef OFONO_H
#define OFONO_H

#include <gio/gio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ofono D-Bus 常量 */
#define OFONO_SERVICE           "org.ofono"
#define OFONO_RADIO_SETTINGS    "org.ofono.RadioSettings"
#define OFONO_TIMEOUT_MS        10000

/**
 * 初始化 D-Bus 连接
 * @return 成功返回非0，失败返回0
 */
int ofono_init(void);

/**
 * 检查 D-Bus 连接是否已初始化
 * @return 已初始化返回1，否则返回0
 */
int ofono_is_initialized(void);

/**
 * 关闭 D-Bus 连接
 */
void ofono_deinit(void);

/**
 * 获取网络模式
 * @param modem_path modem 路径，如 "/ril_0"
 * @param buffer 输出缓冲区
 * @param size 缓冲区大小
 * @param timeout_ms 超时时间(毫秒)
 * @return 成功返回0，失败返回错误码
 */
int ofono_network_get_mode_sync(const char* modem_path, char* buffer, int size, int timeout_ms);

/**
 * 获取数据卡路径
 * @return 数据卡路径字符串，需要调用 g_free 释放，失败返回 NULL
 */
char* ofono_get_datacard(void);

/**
 * 设置网络模式
 * @param modem_path modem 路径
 * @param mode 网络模式索引 (0-10)
 * @param timeout_ms 超时时间
 * @return 成功返回0，失败返回错误码
 */
int ofono_network_set_mode_sync(const char* modem_path, int mode, int timeout_ms);

/**
 * 获取网络模式名称
 * @param mode 模式索引
 * @return 模式名称字符串
 */
const char* ofono_get_mode_name(int mode);

/**
 * 获取支持的网络模式数量
 */
int ofono_get_mode_count(void);

/**
 * 设置 modem 在线状态
 * @param modem_path modem 路径
 * @param online 1=在线, 0=离线
 * @param timeout_ms 超时时间
 * @return 成功返回0，失败返回错误码
 */
int ofono_modem_set_online(const char* modem_path, int online, int timeout_ms);

/**
 * 设置数据卡
 * @param modem_path modem 路径
 * @return 成功返回1，失败返回0
 */
int ofono_set_datacard(const char* modem_path);

/**
 * 获取信号强度
 * @param modem_path modem 路径
 * @param strength 输出信号强度百分比 (0-100)
 * @param dbm 输出信号强度 dBm 值
 * @param timeout_ms 超时时间
 * @return 成功返回0，失败返回错误码
 */
int ofono_network_get_signal_strength(const char* modem_path, int* strength, int* dbm, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* OFONO_H */

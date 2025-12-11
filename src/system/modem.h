 /**
 * @file modem.h
 * @brief Modem control (Go: system/modem.go)
 */

#ifndef MODEM_H
#define MODEM_H

#ifdef __cplusplus
extern "C" {
#endif

/* 网络模式定义 - 对应 ofono RadioSettings TechnologyPreference 索引 */
#define MODE_LTE_ONLY       5   /* "LTE only" */
#define MODE_NR_5G_ONLY     8   /* "NR 5G only" */
#define MODE_NR_5G_LTE_AUTO 9   /* "NR 5G/LTE auto" */
#define MODE_NSA_ONLY       10  /* "NSA only" */

/**
 * @brief 检查网络模式是否有效
 * @param mode 模式字符串 (lte_only, nr_5g_only, nr_5g_lte_auto, nsa_only)
 * @return 1 有效, 0 无效
 */
int is_valid_network_mode(const char *mode);

/**
 * @brief 检查卡槽是否有效
 * @param slot 卡槽字符串 (slot1, slot2)
 * @return 1 有效, 0 无效
 */
int is_valid_slot(const char *slot);

/**
 * @brief 获取网络模式代码
 * @param mode 模式字符串
 * @return 模式代码, -1 无效
 */
int get_network_mode_code(const char *mode);

/**
 * @brief 设置网络模式 (使用当前激活卡槽)
 * @param mode 模式字符串
 * @return 0 成功, -1 失败
 */
int set_network_mode(const char *mode);

/**
 * @brief 设置指定卡槽的网络模式
 * @param mode 模式字符串
 * @param slot 卡槽字符串 (可为空，使用当前卡槽)
 * @return 0 成功, -1 失败
 */
int set_network_mode_for_slot(const char *mode, const char *slot);

/**
 * @brief 切换 SIM 卡槽
 * @param slot 目标卡槽 (slot1 或 slot2)
 * @return 0 成功, -1 失败
 */
int switch_slot(const char *slot);

#ifdef __cplusplus
}
#endif

#endif /* MODEM_H */

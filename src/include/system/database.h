/**
 * @file database.h
 * @brief 数据库操作模块 - SQLite3 统一接口
 * 
 * 提供数据库初始化、SQL执行、配置管理等功能
 */

#ifndef DATABASE_H
#define DATABASE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * 数据库初始化与管理
 *============================================================================*/

/**
 * 初始化数据库
 * @param path 数据库文件路径，NULL则使用默认路径
 * @return 0成功, -1失败
 */
int db_init(const char *path);

/**
 * 关闭数据库模块
 */
void db_deinit(void);

/**
 * 获取数据库路径
 * @return 数据库文件路径
 */
const char *db_get_path(void);

/*============================================================================
 * SQL 执行接口
 *============================================================================*/

/**
 * 执行SQL命令（无锁）
 * @param sql SQL语句
 * @return 0成功, -1失败
 */
int db_execute(const char *sql);

/**
 * 执行SQL命令（带互斥锁，线程安全）
 * @param sql SQL语句
 * @return 0成功, -1失败
 */
int db_execute_safe(const char *sql);

/**
 * 查询整数结果
 * @param sql SQL查询语句
 * @param default_val 查询失败时的默认值
 * @return 查询结果或默认值
 */
int db_query_int(const char *sql, int default_val);

/**
 * 查询字符串结果
 * @param sql SQL查询语句
 * @param buf 输出缓冲区
 * @param size 缓冲区大小
 * @return 0成功, -1失败
 */
int db_query_string(const char *sql, char *buf, size_t size);

/**
 * 查询多行结果（带分隔符）
 * @param sql SQL查询语句
 * @param separator 字段分隔符（如 "|"）
 * @param buf 输出缓冲区
 * @param size 缓冲区大小
 * @return 0成功, -1失败
 */
int db_query_rows(const char *sql, const char *separator, char *buf, size_t size);

/*============================================================================
 * 字符串处理
 *============================================================================*/

/**
 * SQL字符串转义（转义单引号和特殊字符）
 * @param src 源字符串
 * @param dst 目标缓冲区
 * @param size 目标缓冲区大小
 */
void db_escape_string(const char *src, char *dst, size_t size);

/**
 * SQL字符串反转义
 * @param str 要反转义的字符串（原地修改）
 */
void db_unescape_string(char *str);

/*============================================================================
 * 配置管理接口
 *============================================================================*/

/**
 * 获取配置值（字符串）
 * @param key 配置键名
 * @param value 输出值缓冲区
 * @param value_size 缓冲区大小
 * @return 0成功, -1失败
 */
int config_get(const char *key, char *value, size_t value_size);

/**
 * 设置配置值（字符串）
 * @param key 配置键名
 * @param value 配置值
 * @return 0成功, -1失败
 */
int config_set(const char *key, const char *value);

/**
 * 获取配置值（整数）
 * @param key 配置键名
 * @param default_val 默认值
 * @return 配置值或默认值
 */
int config_get_int(const char *key, int default_val);

/**
 * 设置配置值（整数）
 * @param key 配置键名
 * @param value 配置值
 * @return 0成功, -1失败
 */
int config_set_int(const char *key, int value);

/**
 * 获取配置值（长整数）
 * @param key 配置键名
 * @param default_val 默认值
 * @return 配置值或默认值
 */
long long config_get_ll(const char *key, long long default_val);

/**
 * 设置配置值（长整数）
 * @param key 配置键名
 * @param value 配置值
 * @return 0成功, -1失败
 */
int config_set_ll(const char *key, long long value);

#ifdef __cplusplus
}
#endif

#endif /* DATABASE_H */

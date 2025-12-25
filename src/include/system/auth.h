/**
 * @file auth.h
 * @brief 后台认证模块
 */

#ifndef AUTH_H
#define AUTH_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Token长度 */
#define AUTH_TOKEN_SIZE 65  /* 64 hex chars + null */

/* 默认密码 */
#define AUTH_DEFAULT_PASSWORD "admin"

/* Token有效期（秒） - 24小时 */
#define AUTH_TOKEN_EXPIRE_SECONDS (24 * 60 * 60)

/* 最大同时登录Token数量 */
#define AUTH_MAX_TOKENS 5

/**
 * 初始化认证模块
 * 如果数据库中没有密码，则设置默认密码
 * @return 0成功，-1失败
 */
int auth_init(void);

/**
 * 用户登录
 * @param password 用户输入的密码
 * @param token 输出token缓冲区（至少65字节）
 * @param token_size 缓冲区大小
 * @return 0成功，-1密码错误，-2系统错误
 */
int auth_login(const char *password, char *token, size_t token_size);

/**
 * 验证Token
 * @param token 要验证的token
 * @return 0有效，-1无效或过期
 */
int auth_verify_token(const char *token);

/**
 * 修改密码
 * @param old_password 旧密码
 * @param new_password 新密码
 * @return 0成功，-1旧密码错误，-2系统错误
 */
int auth_change_password(const char *old_password, const char *new_password);

/**
 * 用户登出
 * @param token 要注销的token
 * @return 0成功，-1失败
 */
int auth_logout(const char *token);

/**
 * 获取登录状态
 * @param logged_in 输出是否已登录（1=已登录，0=未登录）
 * @return 0成功，-1失败
 */
int auth_get_status(int *logged_in);

/**
 * 检查是否需要认证（首次使用检查）
 * @return 1需要认证，0不需要（未设置密码）
 */
int auth_is_required(void);

#ifdef __cplusplus
}
#endif

#endif /* AUTH_H */

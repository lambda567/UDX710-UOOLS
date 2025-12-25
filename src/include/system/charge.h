/**
 * @file charge.h
 * @brief 充电控制头文件
 */

#ifndef CHARGE_H
#define CHARGE_H

#include "mongoose.h"

#ifdef __cplusplus
extern "C" {
#endif

void init_charge(void);
void handle_charge_config(struct mg_connection *c, struct mg_http_message *hm);
void handle_charge_on(struct mg_connection *c, struct mg_http_message *hm);
void handle_charge_off(struct mg_connection *c, struct mg_http_message *hm);

/**
 * @brief 获取电池状态
 * @param capacity 输出电池电量百分比 (0-100)
 * @param charging 输出是否正在充电 (1=充电中, 0=未充电)
 */
void charge_get_battery_status(int *capacity, int *charging);

/**
 * @brief 注册电池状态变化回调
 * @param callback 回调函数，参数为 (capacity, charging)
 */
typedef void (*battery_change_callback_t)(int capacity, int charging);
void charge_register_callback(battery_change_callback_t callback);

#ifdef __cplusplus
}
#endif

#endif /* CHARGE_H */

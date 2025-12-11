/**
 * @file advanced.h
 * @brief 高级网络功能头文件 (Go: handlers/advanced.go)
 */

#ifndef ADVANCED_H
#define ADVANCED_H

#include "mongoose.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 频段管理 */
void handle_get_bands(struct mg_connection *c, struct mg_http_message *hm);
void handle_lock_bands(struct mg_connection *c, struct mg_http_message *hm);
void handle_unlock_bands(struct mg_connection *c, struct mg_http_message *hm);

/* 小区管理 */
void handle_get_cells(struct mg_connection *c, struct mg_http_message *hm);
void handle_lock_cell(struct mg_connection *c, struct mg_http_message *hm);
void handle_unlock_cell(struct mg_connection *c, struct mg_http_message *hm);

#ifdef __cplusplus
}
#endif

#endif /* ADVANCED_H */

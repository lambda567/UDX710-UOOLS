/**
 * @file reboot.h
 * @brief 定时重启头文件
 */

#ifndef REBOOT_H
#define REBOOT_H

#include "mongoose.h"

#ifdef __cplusplus
extern "C" {
#endif

void handle_get_first_reboot(struct mg_connection *c, struct mg_http_message *hm);
void handle_set_reboot(struct mg_connection *c, struct mg_http_message *hm);
void handle_clear_cron(struct mg_connection *c, struct mg_http_message *hm);

#ifdef __cplusplus
}
#endif

#endif /* REBOOT_H */

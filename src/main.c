/**
 * @file main.c
 * @brief 服务器主程序入口 (对应 Go: main.go)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "handlers/http_server.h"
#include "system/ofono.h"
#include "system/led.h"
#include "system/power_key.h"

int main(int argc, char *argv[]) {
    const char *port = "80";

    /* 解析命令行参数 */
    if (argc > 1) {
        port = argv[1];
    }

    printf("=== ofono-server (C version) ===\n");

    /* 同步系统时间 */
    system("ntpdate ntp.aliyun.com > /dev/null 2>&1 &");

    /* 初始化 ofono D-Bus 连接 */
    if (!ofono_init()) {
        fprintf(stderr, "警告: ofono D-Bus 连接失败，部分功能可能不可用\n");
    }

    /* 初始化 LED 模块 */
    led_init();

    /* 初始化电源键监听 */
    if (power_key_init() != 0) {
        fprintf(stderr, "警告: 电源键监听初始化失败\n");
    }

    /* 启动 HTTP 服务器 */
    if (http_server_start(port) != 0) {
        fprintf(stderr, "服务器启动失败\n");
        ofono_deinit();
        return 1;
    }

    /* 运行事件循环 */
    http_server_run();

    /* 清理 */
    http_server_stop();
    power_key_deinit();
    led_deinit();
    ofono_deinit();

    return 0;
}

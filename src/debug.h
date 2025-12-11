/**
 * @file debug.h
 * @brief 调试输出控制
 * 
 * 编译时添加 -DDISABLE_PRINTF 禁用所有printf输出
 * 移除该选项可恢复printf输出
 */

#ifndef DEBUG_H
#define DEBUG_H

#ifdef DISABLE_PRINTF
    /* 禁用printf - 使用内联函数避免宏展开问题 */
    #include <stdio.h>
    #ifdef printf
        #undef printf
    #endif
    #define printf(fmt, ...) do { (void)(fmt); } while(0)
#endif

#endif /* DEBUG_H */

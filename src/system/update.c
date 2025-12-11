/**
 * @file update.c
 * @brief OTA更新系统实现
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "update.h"
#include "exec_utils.h"

/* 获取当前版本 */
const char* update_get_version(void) {
    return FIRMWARE_VERSION;
}

/* 从URL下载更新包 */
int update_download(const char *url) {
    char output[1024];
    
    if (!url || strlen(url) == 0) {
        return -1;
    }
    
    /* 清理旧文件 */
    update_cleanup();
    
    /* 优先使用curl（更常见），失败再用wget */
    int ret = run_command(output, sizeof(output), "curl", "-k", "-s", "-L", "-o", UPDATE_ZIP_PATH, url, NULL);
    if (ret != 0) {
        ret = run_command(output, sizeof(output), "wget", "--no-check-certificate", "-q", "-O", UPDATE_ZIP_PATH, url, NULL);
        if (ret != 0) {
            return -1;
        }
    }
    
    /* 检查文件是否存在 */
    struct stat st;
    if (stat(UPDATE_ZIP_PATH, &st) != 0 || st.st_size == 0) {
        return -1;
    }
    
    return 0;
}

/* 解压更新包 */
int update_extract(void) {
    char output[2048];
    struct stat st;

    /* 检查ZIP文件是否存在 */
    if (stat(UPDATE_ZIP_PATH, &st) != 0) {
        return -1;
    }
    
    /* 创建解压目录 */
    run_command(output, sizeof(output), "rm", "-rf", UPDATE_EXTRACT_DIR, NULL);
    run_command(output, sizeof(output), "mkdir", "-p", UPDATE_EXTRACT_DIR, NULL);
    
    /* 解压ZIP - 优先使用unzip，失败则尝试busybox unzip */
    int ret = run_command(output, sizeof(output), "unzip", "-o", UPDATE_ZIP_PATH, "-d", UPDATE_EXTRACT_DIR, NULL);
    if (ret != 0) {
        ret = run_command(output, sizeof(output), "busybox", "unzip", "-o", UPDATE_ZIP_PATH, "-d", UPDATE_EXTRACT_DIR, NULL);
        if (ret != 0) {
            return -1;
        }
    }
    
    return 0;
}


/* 执行安装脚本 */
int update_install(char *output, size_t size) {
    struct stat st;
    
    /* 检查安装脚本是否存在 */
    if (stat(UPDATE_INSTALL_SCRIPT, &st) != 0) {
        snprintf(output, size, "安装脚本不存在");
        return -1;
    }
    
    /* 添加执行权限 */
    run_command(output, size, "chmod", "+x", UPDATE_INSTALL_SCRIPT, NULL);
    
    /* 执行安装脚本 */
    if (run_command(output, size, "sh", UPDATE_INSTALL_SCRIPT, NULL) != 0) {
        return -1;
    }
    
    return 0;
}

/* 清理更新临时文件 */
void update_cleanup(void) {
    char output[256];
    run_command(output, sizeof(output), "rm", "-rf", UPDATE_ZIP_PATH, NULL);
    run_command(output, sizeof(output), "rm", "-rf", UPDATE_EXTRACT_DIR, NULL);
}

/* 检查远程版本 - 简单实现，解析JSON响应 */
int update_check_version(const char *check_url, update_info_t *info) {
    char output[4096];
    
    if (!check_url || !info) {
        return -1;
    }
    
    memset(info, 0, sizeof(update_info_t));
    
    /* 优先使用curl获取版本信息，失败再用wget */
    int ret = run_command(output, sizeof(output), "curl", "-k", "-s", "-L", check_url, NULL);
    if (ret != 0) {
        ret = run_command(output, sizeof(output), "wget", "--no-check-certificate", "-q", "-O", "-", check_url, NULL);
        if (ret != 0) {
            return -1;
        }
    }
    
    /* 简单JSON解析 - 提取version字段 */
    char *p = strstr(output, "\"version\"");
    if (p) {
        p = strchr(p, ':');
        if (p) {
            p = strchr(p, '"');
            if (p) {
                p++;
                char *end = strchr(p, '"');
                if (end) {
                    size_t len = end - p;
                    if (len < sizeof(info->version)) {
                        memcpy(info->version, p, len);
                    }
                }
            }
        }
    }
    
    /* 提取url字段 */
    p = strstr(output, "\"url\"");
    if (p) {
        p = strchr(p, ':');
        if (p) {
            p = strchr(p, '"');
            if (p) {
                p++;
                char *end = strchr(p, '"');
                if (end) {
                    size_t len = end - p;
                    if (len < sizeof(info->url)) {
                        memcpy(info->url, p, len);
                    }
                }
            }
        }
    }
    
    /* 提取changelog字段 */
    p = strstr(output, "\"changelog\"");
    if (p) {
        p = strchr(p, ':');
        if (p) {
            p = strchr(p, '"');
            if (p) {
                p++;
                char *end = strchr(p, '"');
                if (end) {
                    size_t len = end - p;
                    if (len < sizeof(info->changelog)) {
                        memcpy(info->changelog, p, len);
                    }
                }
            }
        }
    }
    
    /* 提取size字段 */
    p = strstr(output, "\"size\"");
    if (p) {
        p = strchr(p, ':');
        if (p) {
            while (*p && (*p == ':' || *p == ' ')) p++;
            info->size = (size_t)atol(p);
        }
    }
    
    /* 提取required字段 */
    p = strstr(output, "\"required\"");
    if (p) {
        if (strstr(p, "true")) {
            info->required = 1;
        }
    }
    
    if (strlen(info->version) == 0) {
        return -1;
    }
    
    return 0;
}


/* 获取嵌入的版本检查URL */
const char* update_get_embedded_url(void) {
    return UPDATE_CHECK_URL;
}

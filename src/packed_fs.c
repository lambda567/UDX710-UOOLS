/**
 * @file packed_fs.c
 * @brief Static file service - serve files from dist directory
 */

#include <string.h>
#include "mongoose.h"

/* Static file directory */
#define STATIC_DIR "./dist"

/* Static file service options */
static struct mg_http_serve_opts s_opts = {
    .root_dir = STATIC_DIR,
    .ssi_pattern = NULL,
    .extra_headers = "Cache-Control: max-age=3600\r\n"
                     "Access-Control-Allow-Origin: *\r\n"
};

/**
 * @brief Serve static files
 * @param c Mongoose connection
 * @param hm HTTP message
 * @return 1 success, 0 not found
 */
int serve_packed_file(struct mg_connection *c, struct mg_http_message *hm) {
    char path[512] = {0};
    size_t len = hm->uri.len < sizeof(path) - 1 ? hm->uri.len : sizeof(path) - 1;
    memcpy(path, hm->uri.buf, len);

    /* Root path or SPA routes - serve index.html */
    if (strcmp(path, "/") == 0 || 
        (strstr(path, ".") == NULL && strncmp(path, "/api/", 5) != 0)) {
        mg_http_serve_file(c, hm, STATIC_DIR "/index.html", &s_opts);
        return 1;
    }

    /* Serve static files from dist directory */
    mg_http_serve_dir(c, hm, &s_opts);
    return 1;
}

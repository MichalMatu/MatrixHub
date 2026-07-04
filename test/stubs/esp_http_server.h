#pragma once

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#ifdef __cplusplus
#include <cstddef>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_TIMEOUT 0x107
#define ESP_ERR_NOT_FOUND 0x109
#define ESP_ERR_HTTPD_RESULT_TRUNC 0x7001
#define HTTPD_RESP_USE_STRLEN -1

typedef int esp_err_t;
typedef void* httpd_handle_t;

typedef enum http_method {
    HTTP_GET = 0,
    HTTP_POST = 1,
    HTTP_PUT = 2,
    HTTP_DELETE = 3,
    HTTP_PATCH = 4,
    HTTP_OPTIONS = 5
} httpd_method_t;

typedef enum {
    HTTPD_WS_TYPE_TEXT = 0x1,
    HTTPD_WS_TYPE_BINARY = 0x2
} httpd_ws_type_t;

typedef struct httpd_ws_frame {
    bool final;
    bool fragmented;
    httpd_ws_type_t type;
    uint8_t* payload;
    size_t len;
} httpd_ws_frame_t;

typedef struct httpd_req {
    httpd_method_t method;
    httpd_handle_t handle;
    void* user_ctx;
    int sockfd;
    const char* uri;
#ifdef __cplusplus
    httpd_req()
        : method(HTTP_GET), handle(nullptr), user_ctx(nullptr), sockfd(-1), uri(nullptr) {}

    httpd_req(std::nullptr_t)
        : method(HTTP_GET), handle(nullptr), user_ctx(nullptr), sockfd(-1), uri(nullptr) {}
#endif
} httpd_req_t;

typedef esp_err_t (*httpd_uri_func_t)(httpd_req_t *r);

typedef struct httpd_uri {
    const char* uri;
    httpd_method_t method;
    httpd_uri_func_t handler;
    void* user_ctx;
    bool is_websocket;
} httpd_uri_t;

typedef struct httpd_config {
    uint16_t server_port;
    uint16_t ctrl_port;
    uint32_t task_priority;
    int max_open_sockets;
    int max_uri_handlers;
    uint32_t backlog_conn;
    uint32_t max_req_hdr_len;
    bool lru_purge_enable;
    int recv_wait_timeout;
    int send_wait_timeout;
    uint32_t stack_size;
    int core_id;
#ifdef __cplusplus
    httpd_config()
        : server_port(0),
          ctrl_port(0),
          task_priority(0),
          max_open_sockets(0),
          max_uri_handlers(0),
          backlog_conn(0),
          max_req_hdr_len(0),
          lru_purge_enable(false),
          recv_wait_timeout(0),
          send_wait_timeout(0),
          stack_size(0),
          core_id(0) {}
#endif
} httpd_config_t;

// Mock functions signatures
esp_err_t httpd_resp_set_type(httpd_req_t *r, const char *type);
esp_err_t httpd_resp_set_status(httpd_req_t *r, const char *status);
esp_err_t httpd_resp_set_hdr(httpd_req_t *r, const char *field, const char *value);
esp_err_t httpd_resp_send(httpd_req_t *r, const char *buf, ssize_t len);
esp_err_t httpd_resp_send_chunk(httpd_req_t *r, const char *buf, ssize_t len);
int httpd_req_to_sockfd(httpd_req_t *r);
esp_err_t httpd_register_uri_handler(httpd_handle_t handle, const httpd_uri_t *uri_handler);
size_t httpd_req_get_hdr_value_len(httpd_req_t *req, const char *field);
esp_err_t httpd_req_get_hdr_value_str(httpd_req_t *req, const char *field, char *val, size_t val_size);
size_t httpd_req_get_url_query_len(httpd_req_t *req);
esp_err_t httpd_req_get_url_query_str(httpd_req_t *req, char *buf, size_t len);
esp_err_t httpd_req_get_cookie_val(httpd_req_t *req, const char *cookie_name, char *val, size_t *val_size);
esp_err_t httpd_query_key_value(const char *qry, const char *key, char *val, size_t val_size);
esp_err_t httpd_ws_recv_frame(httpd_req_t *req, httpd_ws_frame_t *pkt, size_t max_len);
esp_err_t httpd_ws_send_data(httpd_handle_t handle, int sockfd, httpd_ws_frame_t *pkt);
void httpd_sess_trigger_close(httpd_handle_t handle, int sockfd);
void httpd_sess_update_lru_counter(httpd_handle_t handle, int sockfd);

#ifdef __cplusplus
}
#endif

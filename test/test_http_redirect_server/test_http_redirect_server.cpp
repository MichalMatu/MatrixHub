#ifdef UNIT_TEST

#include <unity.h>

#include <cstring>
#include <map>
#include <string>

#include <WiFi.h>

#define private public
#include "../../src/system/network/HttpRedirectServer.cpp"
#undef private

WiFiClass WiFi;

namespace LOG {

void Logging::log(esp_log_level_t level, const char* tag, const char* format, ...) {
    (void)level;
    (void)tag;
    (void)format;
}

void Logging::logSection(const char* title) {
    (void)title;
}

void Logging::logStackHwm(const char* taskName, uint32_t stackSize) {
    (void)taskName;
    (void)stackSize;
}

}  // namespace LOG

namespace {

std::map<std::string, std::string> g_headers;
std::string g_status;
std::string g_location;
std::string g_body;

void resetHttpFakes() {
    g_headers.clear();
    g_status.clear();
    g_location.clear();
    g_body.clear();
}

}  // namespace

extern "C" {

size_t httpd_req_get_hdr_value_len(httpd_req_t* req, const char* field) {
    (void)req;
    const auto it = g_headers.find(field ? field : "");
    return it == g_headers.end() ? 0 : it->second.size();
}

esp_err_t httpd_req_get_hdr_value_str(httpd_req_t* req, const char* field, char* val, size_t val_size) {
    (void)req;
    const auto it = g_headers.find(field ? field : "");
    if (it == g_headers.end() || !val || val_size == 0 || it->second.size() + 1 > val_size) {
        return ESP_FAIL;
    }
    std::memcpy(val, it->second.c_str(), it->second.size() + 1);
    return ESP_OK;
}

esp_err_t httpd_resp_set_status(httpd_req_t* req, const char* status) {
    (void)req;
    g_status = status ? status : "";
    return ESP_OK;
}

esp_err_t httpd_resp_set_type(httpd_req_t* req, const char* type) {
    (void)req;
    (void)type;
    return ESP_OK;
}

esp_err_t httpd_resp_set_hdr(httpd_req_t* req, const char* field, const char* value) {
    (void)req;
    if (field && std::strcmp(field, "Location") == 0) {
        g_location = value ? value : "";
    }
    return ESP_OK;
}

esp_err_t httpd_resp_send(httpd_req_t* req, const char* buf, ssize_t len) {
    (void)req;
    if (!buf) {
        g_body.clear();
    } else if (len < 0) {
        g_body = buf;
    } else {
        g_body.assign(buf, static_cast<size_t>(len));
    }
    return ESP_OK;
}

int httpd_req_to_sockfd(httpd_req_t* req) {
    return req ? req->sockfd : -1;
}

}  // extern "C"

void setUp(void) {
    TEST_STUBS::WIFI::reset();
    resetHttpFakes();
}

void tearDown(void) {}

void test_build_redirect_strips_port_and_preserves_path_query() {
    char out[128] = {0};

    auto status = SYSTEM::NETWORK::buildHttpsRedirectLocation(
        "matrixhub.local:80", "/system/power?tab=config", nullptr, out, sizeof(out));

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SYSTEM::NETWORK::RedirectLocationStatus::Ok),
                            static_cast<uint8_t>(status));
    TEST_ASSERT_EQUAL_STRING("https://matrixhub.local/system/power?tab=config", out);
}

void test_build_redirect_strips_bracketed_ipv6_port() {
    char out[128] = {0};

    auto status = SYSTEM::NETWORK::buildHttpsRedirectLocation("[fe80::1]:80", "/", nullptr, out, sizeof(out));

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SYSTEM::NETWORK::RedirectLocationStatus::Ok),
                            static_cast<uint8_t>(status));
    TEST_ASSERT_EQUAL_STRING("https://[fe80::1]/", out);
}

void test_build_redirect_uses_fallback_host_when_host_header_missing() {
    char out[128] = {0};

    auto status =
        SYSTEM::NETWORK::buildHttpsRedirectLocation("", "/", "matrixhub.local", out, sizeof(out));

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SYSTEM::NETWORK::RedirectLocationStatus::Ok),
                            static_cast<uint8_t>(status));
    TEST_ASSERT_EQUAL_STRING("https://matrixhub.local/", out);
}

void test_build_redirect_rejects_too_small_output_buffer() {
    char out[12] = {0};

    auto status =
        SYSTEM::NETWORK::buildHttpsRedirectLocation("matrixhub.local", "/", nullptr, out, sizeof(out));

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SYSTEM::NETWORK::RedirectLocationStatus::BufferTooSmall),
                            static_cast<uint8_t>(status));
}

void test_server_uses_small_httpd_config() {
    SYSTEM::NETWORK::HttpRedirectServer redirectServer;
    httpd_config_t base{};
    base.task_priority = 7;
    base.core_id = 1;

    TEST_ASSERT_TRUE(redirectServer.begin(base));
    TEST_ASSERT_TRUE(redirectServer.isStarted());
    TEST_ASSERT_EQUAL_UINT32(7, redirectServer._server.config.task_priority);
    TEST_ASSERT_EQUAL_INT(NET::HTTP::REDIRECT_MAX_OPEN_SOCKETS,
                          redirectServer._server.config.max_open_sockets);
    TEST_ASSERT_EQUAL_INT(NET::HTTP::REDIRECT_MAX_URI_HANDLERS,
                          redirectServer._server.config.max_uri_handlers);
    TEST_ASSERT_EQUAL_UINT32(NET::HTTP::REDIRECT_SERVER_STACK_SIZE_BYTES,
                             redirectServer._server.config.stack_size);
    TEST_ASSERT_EQUAL_UINT16(NET::HTTP::REDIRECT_CTRL_PORT, redirectServer._server.config.ctrl_port);
}

void test_redirect_handler_returns_301_location() {
    SYSTEM::NETWORK::HttpRedirectServer redirectServer;
    httpd_req_t req{};
    req.uri = "/logs";
    PsychicRequest request;
    request._req = &req;
    g_headers["Host"] = "192.168.0.18:80";

    TEST_ASSERT_EQUAL_INT(ESP_OK, SYSTEM::NETWORK::HttpRedirectServer::handleRedirect(&request));

    TEST_ASSERT_EQUAL_STRING("301 Moved Permanently", g_status.c_str());
    TEST_ASSERT_EQUAL_STRING("https://192.168.0.18/logs", g_location.c_str());
    TEST_ASSERT_EQUAL_STRING("Redirecting to HTTPS", g_body.c_str());
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_build_redirect_strips_port_and_preserves_path_query);
    RUN_TEST(test_build_redirect_strips_bracketed_ipv6_port);
    RUN_TEST(test_build_redirect_uses_fallback_host_when_host_header_missing);
    RUN_TEST(test_build_redirect_rejects_too_small_output_buffer);
    RUN_TEST(test_server_uses_small_httpd_config);
    RUN_TEST(test_redirect_handler_returns_301_location);
    return UNITY_END();
}

#endif

#include <unity.h>

#include <ArduinoJson.h>
#include <string>

std::string g_responseBuffer;
std::string g_responseType;
std::string g_responseHeaderField;
std::string g_responseHeaderValue;

extern "C" {
#include "esp_http_server.h"

esp_err_t httpd_resp_set_type(httpd_req_t* r, const char* type) {
    (void)r;
    g_responseType = (type ? type : "");
    return ESP_OK;
}

esp_err_t httpd_resp_set_hdr(httpd_req_t* r, const char* field, const char* value) {
    (void)r;
    g_responseHeaderField = (field ? field : "");
    g_responseHeaderValue = (value ? value : "");
    return ESP_OK;
}

esp_err_t httpd_resp_send_chunk(httpd_req_t* r, const char* buf, ssize_t len) {
    (void)r;
    if (len == 0 || buf == nullptr) {
        return ESP_OK;
    }
    g_responseBuffer.append(buf, static_cast<size_t>(len));
    return ESP_OK;
}
}

#include "../../src/system/utils/json/JsonResponseWriter.cpp"
#include "../../src/api/diagnostics/DiagnosticsSnapshots.cpp"
#include "../../src/api/diagnostics/DiagnosticsJsonWriter.cpp"

extern "C" {
size_t heap_caps_get_total_size(uint32_t caps) {
    return (caps == MALLOC_CAP_SPIRAM) ? 1024U : 4096U;
}

size_t heap_caps_get_free_size(uint32_t caps) {
    return (caps == MALLOC_CAP_SPIRAM) ? 512U : 2048U;
}

size_t heap_caps_get_minimum_free_size(uint32_t caps) {
    return (caps == MALLOC_CAP_SPIRAM) ? 256U : 1024U;
}

size_t heap_caps_get_largest_free_block(uint32_t caps) {
    return (caps == MALLOC_CAP_SPIRAM) ? 128U : 1024U;
}

uint32_t esp_get_minimum_free_heap_size() {
    return 1024U;
}

uint32_t esp_get_free_heap_size() {
    return 2048U;
}
}

void setUp(void) {
    g_responseBuffer.clear();
    g_responseType.clear();
    g_responseHeaderField.clear();
    g_responseHeaderValue.clear();
    SYSTEM::LOCK_DIAGNOSTICS::reset();
    TEST_ASSERT_TRUE(Utils::JsonResponseWriter::begin());
}

void tearDown(void) {}

void test_fragmentation_percent_uses_largest_block_ratio() {
    TEST_ASSERT_EQUAL_UINT8(0, API::calculateFragmentationPercent(0, 0));
    TEST_ASSERT_EQUAL_UINT8(0, API::calculateFragmentationPercent(1000, 1000));
    TEST_ASSERT_EQUAL_UINT8(75, API::calculateFragmentationPercent(1000, 250));
}

void test_heap_writer_serializes_regions_and_fragmentation() {
    API::DiagnosticsHeapSnapshot snapshot = API::buildDiagnosticsHeapSnapshot();
    httpd_req_t req{};
    req.uri = "/api/diagnostics/heap";
    Utils::JsonResponseWriter writer(&req);

    TEST_ASSERT_TRUE(writer.beginResponse());
    API::DiagnosticsJsonWriter::writeHeap(writer, snapshot);
    TEST_ASSERT_TRUE(writer.finish());

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, g_responseBuffer);
    TEST_ASSERT_FALSE(err);
    TEST_ASSERT_EQUAL_STRING("diagnostics.heap.v1", doc["schema"].as<const char*>());
    TEST_ASSERT_EQUAL(2048U, doc["regions"]["internal"]["free"].as<unsigned int>());
    TEST_ASSERT_EQUAL(50U, doc["regions"]["internal"]["fragmentationPercent"].as<unsigned int>());
    TEST_ASSERT_EQUAL(75U, doc["regions"]["psram"]["fragmentationPercent"].as<unsigned int>());
}

void test_features_writer_marks_unmeasured_runtime_explicitly() {
    API::DiagnosticsFeaturesSnapshot snapshot{};
    snapshot.configRead = true;
    API::addDiagnosticsFeature(snapshot, {
        "udp_push",
        true,
        true,
        true,
        false,
        false,
        "configured",
    });

    httpd_req_t req{};
    req.uri = "/api/diagnostics/features";
    Utils::JsonResponseWriter writer(&req);

    TEST_ASSERT_TRUE(writer.beginResponse());
    API::DiagnosticsJsonWriter::writeFeatures(writer, snapshot);
    TEST_ASSERT_TRUE(writer.finish());

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, g_responseBuffer);
    TEST_ASSERT_FALSE(err);
    TEST_ASSERT_TRUE(doc["configRead"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("udp_push", doc["features"][0]["key"].as<const char*>());
    TEST_ASSERT_FALSE(doc["features"][0]["runtimeMeasured"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("configured", doc["features"][0]["detail"].as<const char*>());
}

void test_endpoints_writer_lists_all_diagnostics_routes() {
    httpd_req_t req{};
    req.uri = "/api/diagnostics/endpoints";
    Utils::JsonResponseWriter writer(&req);

    TEST_ASSERT_TRUE(writer.beginResponse());
    API::DiagnosticsJsonWriter::writeEndpoints(writer);
    TEST_ASSERT_TRUE(writer.finish());

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, g_responseBuffer);
    TEST_ASSERT_FALSE(err);
    TEST_ASSERT_TRUE(doc["metrics"]["httpTransportCounters"].as<bool>());
    TEST_ASSERT_FALSE(doc["metrics"]["latencyBuckets"].as<bool>());
    TEST_ASSERT_EQUAL(API::kDiagnosticsEndpointCount, doc["diagnostics"].size());
    TEST_ASSERT_EQUAL_STRING("/api/diagnostics/summary", doc["diagnostics"][0]["path"].as<const char*>());
}

void test_summary_writer_exposes_websocket_runtime_counters() {
    API::DiagnosticsSummarySnapshot snapshot{};
    snapshot.schema = "diagnostics.v1";
    snapshot.http.activeClients = 1;
    snapshot.http.openCount = 3;
    snapshot.http.closeCount = 2;
    snapshot.http.wsActiveClients = 2;
    snapshot.http.wsPeakClients = 4;
    snapshot.http.wsOpenCount = 6;
    snapshot.http.wsCloseCount = 4;
    snapshot.http.lastWsOpenMs = 100;
    snapshot.http.lastWsCloseMs = 200;

    httpd_req_t req{};
    req.uri = "/api/diagnostics/summary";
    Utils::JsonResponseWriter writer(&req);

    TEST_ASSERT_TRUE(writer.beginResponse());
    API::DiagnosticsJsonWriter::writeSummary(writer, snapshot);
    TEST_ASSERT_TRUE(writer.finish());

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, g_responseBuffer);
    TEST_ASSERT_FALSE(err);
    TEST_ASSERT_EQUAL(2U, doc["http"]["wsActiveClients"].as<unsigned int>());
    TEST_ASSERT_EQUAL(4U, doc["http"]["wsPeakClients"].as<unsigned int>());
    TEST_ASSERT_EQUAL(6U, doc["http"]["wsOpens"].as<unsigned int>());
    TEST_ASSERT_EQUAL(4U, doc["http"]["wsCloses"].as<unsigned int>());
    TEST_ASSERT_EQUAL(100U, doc["http"]["lastWsOpenMs"].as<unsigned int>());
    TEST_ASSERT_EQUAL(200U, doc["http"]["lastWsCloseMs"].as<unsigned int>());
}

void test_mutex_writer_exposes_runtime_lock_counters() {
    SYSTEM::LOCK_DIAGNOSTICS::recordAcquire(false, pdMS_TO_TICKS(25), pdMS_TO_TICKS(100), true);
    SYSTEM::LOCK_DIAGNOSTICS::recordAcquire(true, pdMS_TO_TICKS(5), 0, false);

    httpd_req_t req{};
    req.uri = "/api/diagnostics/mutexes";
    Utils::JsonResponseWriter writer(&req);

    TEST_ASSERT_TRUE(writer.beginResponse());
    API::DiagnosticsJsonWriter::writeMutexes(writer);
    TEST_ASSERT_TRUE(writer.finish());

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, g_responseBuffer);
    TEST_ASSERT_FALSE(err);
    TEST_ASSERT_EQUAL_STRING("diagnostics.mutexes.v1", doc["schema"].as<const char*>());
    TEST_ASSERT_TRUE(doc["instrumented"].as<bool>());
    TEST_ASSERT_TRUE(doc["coverage"]["timeoutCounters"].as<bool>());
    TEST_ASSERT_TRUE(doc["coverage"]["slowAcquireCounters"].as<bool>());
    TEST_ASSERT_FALSE(doc["coverage"]["holdTimeBuckets"].as<bool>());
    TEST_ASSERT_EQUAL(1U, doc["runtime"]["standard"]["attempts"].as<unsigned int>());
    TEST_ASSERT_EQUAL(1U, doc["runtime"]["standard"]["successes"].as<unsigned int>());
    TEST_ASSERT_EQUAL(1U, doc["runtime"]["standard"]["slowAcquires"].as<unsigned int>());
    TEST_ASSERT_EQUAL(1U, doc["runtime"]["recursive"]["attempts"].as<unsigned int>());
    TEST_ASSERT_EQUAL(1U, doc["runtime"]["recursive"]["timeouts"].as<unsigned int>());
    TEST_ASSERT_EQUAL_STRING("global", doc["criticalLocks"][0]["counterScope"].as<const char*>());
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_fragmentation_percent_uses_largest_block_ratio);
    RUN_TEST(test_heap_writer_serializes_regions_and_fragmentation);
    RUN_TEST(test_features_writer_marks_unmeasured_runtime_explicitly);
    RUN_TEST(test_endpoints_writer_lists_all_diagnostics_routes);
    RUN_TEST(test_summary_writer_exposes_websocket_runtime_counters);
    RUN_TEST(test_mutex_writer_exposes_runtime_lock_counters);
    return UNITY_END();
}

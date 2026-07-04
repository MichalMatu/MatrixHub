#include <unity.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

#include <Arduino.h>

#include "../../src/api/common/WebSocketBroadcaster.h"

#define private public
#include "../../src/api/common/ChannelSubscriptions.cpp"
#undef private

namespace LOG {
Settings Logging::_settings;
void Logging::log(esp_log_level_t level, const char* tag, const char* fmt, ...) {
    (void)level;
    (void)tag;
    (void)fmt;
}
void Logging::logSection(const char* title) { (void)title; }
void Logging::logStackHwm(const char* taskName, uint32_t stackSize) {
    (void)taskName;
    (void)stackSize;
}
const char* Logging::levelToString(esp_log_level_t level) {
    (void)level;
    return "info";
}
esp_log_level_t Logging::stringToLevel(std::string_view name, esp_log_level_t fallback) {
    (void)name;
    return fallback;
}
} // namespace LOG

namespace TEST_STUBS::WS {
extern std::vector<int> asyncBroadcastFds;
extern std::vector<uint8_t> asyncBroadcastPayload;
extern httpd_ws_type_t asyncBroadcastType;
extern std::vector<int> syncBroadcastFds;
extern std::vector<std::vector<uint8_t>> syncBroadcastPayloads;
} // namespace TEST_STUBS::WS

namespace API {
void WebSocketBroadcaster::broadcast(int* fds, size_t count, uint8_t* data, size_t len, httpd_ws_type_t type) {
    TEST_STUBS::WS::asyncBroadcastFds.assign(fds, fds + count);
    TEST_STUBS::WS::asyncBroadcastPayload.assign(data, data + len);
    TEST_STUBS::WS::asyncBroadcastType = type;
}
} // namespace API

namespace TEST_STUBS::WS {
inline std::vector<int> asyncBroadcastFds;
inline std::vector<uint8_t> asyncBroadcastPayload;
inline httpd_ws_type_t asyncBroadcastType = HTTPD_WS_TYPE_TEXT;
inline std::vector<int> syncBroadcastFds;
inline std::vector<std::vector<uint8_t>> syncBroadcastPayloads;

inline void reset() {
    asyncBroadcastFds.clear();
    asyncBroadcastPayload.clear();
    asyncBroadcastType = HTTPD_WS_TYPE_TEXT;
    syncBroadcastFds.clear();
    syncBroadcastPayloads.clear();
}
} // namespace TEST_STUBS::WS

extern "C" esp_err_t httpd_ws_send_data(httpd_handle_t handle, int sockfd, httpd_ws_frame_t* pkt) {
    (void)handle;
    TEST_STUBS::WS::syncBroadcastFds.push_back(sockfd);
    TEST_STUBS::WS::syncBroadcastPayloads.emplace_back(
        pkt && pkt->payload ? pkt->payload : nullptr,
        pkt && pkt->payload ? pkt->payload + pkt->len : nullptr
    );
    return ESP_OK;
}

namespace {

using Channel = API::ChannelSubscriptions::Channel;
using SubscriptionModel = std::map<int, uint16_t>;

constexpr std::array<Channel, 11> kAllChannels = {
    Channel::SHELLY,
    Channel::SENSING,
    Channel::ALARMS,
    Channel::POWER,
    Channel::BLE,
    Channel::TELEGRAM,
    Channel::NOTIF_STATS,
    Channel::TELEMETRY,
    Channel::MACROS,
    Channel::SYSTEM_STATUS,
    Channel::AIRMOUSE,
};

constexpr std::array<Channel, 5> kModelChannels = {
    Channel::BLE,
    Channel::TELEMETRY,
    Channel::TELEGRAM,
    Channel::MACROS,
    Channel::SYSTEM_STATUS,
};

size_t modelCountForChannel(const SubscriptionModel& model, Channel channel) {
    const uint16_t mask = static_cast<uint16_t>(channel);
    return static_cast<size_t>(std::count_if(model.begin(), model.end(), [mask](const auto& entry) {
        return (entry.second & mask) != 0;
    }));
}

void modelSubscribe(SubscriptionModel& model, int fd, Channel channel) {
    model[fd] |= static_cast<uint16_t>(channel);
}

void modelUnsubscribe(SubscriptionModel& model, int fd, Channel channel) {
    auto it = model.find(fd);
    if (it == model.end()) {
        return;
    }

    it->second &= static_cast<uint16_t>(~static_cast<uint16_t>(channel));
    if (it->second == 0) {
        model.erase(it);
    }
}

void modelUnsubscribeAll(SubscriptionModel& model, int fd) {
    model.erase(fd);
}

void assertMatchesModel(API::ChannelSubscriptions& subscriptions, const SubscriptionModel& model) {
    for (Channel channel : kAllChannels) {
        const size_t expectedCount = modelCountForChannel(model, channel);
        TEST_ASSERT_EQUAL(expectedCount, subscriptions.getSubscriberCount(channel));
        TEST_ASSERT_EQUAL(expectedCount > 0, subscriptions.hasSubscribers(channel));
    }

    TEST_ASSERT_EQUAL(static_cast<int>(model.size()), static_cast<int>(subscriptions._subscriptions.size()));
}

} // namespace

void setUp(void) {
    TEST_STUBS::WS::reset();
}
void tearDown(void) {}

void test_handle_message_subscribes_known_channel() {
    API::ChannelSubscriptions subscriptions;
    API::ChannelSubscriptions::Channel channel = API::ChannelSubscriptions::NONE;
    bool isSubscribe = false;
    bool changed = false;

    const char* msg = "{\"subscribe\":\"telemetry\"}";
    TEST_ASSERT_TRUE(subscriptions.handleMessage(11, msg, strlen(msg), &channel, &isSubscribe, &changed));
    TEST_ASSERT_TRUE(isSubscribe);
    TEST_ASSERT_TRUE(changed);
    TEST_ASSERT_EQUAL(API::ChannelSubscriptions::TELEMETRY, channel);
    TEST_ASSERT_TRUE(subscriptions.hasSubscribers(API::ChannelSubscriptions::TELEMETRY));
    TEST_ASSERT_EQUAL(static_cast<size_t>(1), subscriptions.getSubscriberCount(API::ChannelSubscriptions::TELEMETRY));
}

void test_handle_message_treats_duplicate_subscribe_as_idempotent() {
    API::ChannelSubscriptions subscriptions;
    API::ChannelSubscriptions::Channel channel = API::ChannelSubscriptions::NONE;
    bool isSubscribe = false;
    bool changed = false;
    const char* msg = "{\"subscribe\":\"ble\"}";

    TEST_ASSERT_TRUE(subscriptions.handleMessage(22, msg, strlen(msg), &channel, &isSubscribe, &changed));
    TEST_ASSERT_TRUE(isSubscribe);
    TEST_ASSERT_TRUE(changed);
    TEST_ASSERT_TRUE(subscriptions.handleMessage(22, msg, strlen(msg), &channel, &isSubscribe, &changed));
    TEST_ASSERT_TRUE(isSubscribe);
    TEST_ASSERT_FALSE(changed);
    TEST_ASSERT_TRUE(subscriptions.hasSubscribers(API::ChannelSubscriptions::BLE));
    TEST_ASSERT_EQUAL(static_cast<size_t>(1), subscriptions.getSubscriberCount(API::ChannelSubscriptions::BLE));
}

void test_subscribe_two_clients_same_channel_counts_both_and_releases_cleanly() {
    API::ChannelSubscriptions subscriptions;

    TEST_ASSERT_TRUE(subscriptions.subscribe(70, API::ChannelSubscriptions::BLE));
    TEST_ASSERT_TRUE(subscriptions.subscribe(71, API::ChannelSubscriptions::BLE));
    TEST_ASSERT_EQUAL(static_cast<size_t>(2), subscriptions.getSubscriberCount(API::ChannelSubscriptions::BLE));

    TEST_ASSERT_TRUE(subscriptions.unsubscribe(70, API::ChannelSubscriptions::BLE));
    TEST_ASSERT_EQUAL(static_cast<size_t>(1), subscriptions.getSubscriberCount(API::ChannelSubscriptions::BLE));

    TEST_ASSERT_TRUE(subscriptions.unsubscribe(71, API::ChannelSubscriptions::BLE));
    TEST_ASSERT_FALSE(subscriptions.hasSubscribers(API::ChannelSubscriptions::BLE));
    TEST_ASSERT_EQUAL(static_cast<size_t>(0), subscriptions.getSubscriberCount(API::ChannelSubscriptions::BLE));
}

void test_handle_message_unsubscribes_only_requested_channel() {
    API::ChannelSubscriptions subscriptions;

    TEST_ASSERT_TRUE(subscriptions.handleMessage(33, "{\"subscribe\":\"ble\"}", strlen("{\"subscribe\":\"ble\"}")));
    TEST_ASSERT_TRUE(subscriptions.handleMessage(33, "{\"subscribe\":\"telemetry\"}", strlen("{\"subscribe\":\"telemetry\"}")));
    TEST_ASSERT_TRUE(subscriptions.handleMessage(33, "{\"unsubscribe\":\"ble\"}", strlen("{\"unsubscribe\":\"ble\"}")));

    TEST_ASSERT_FALSE(subscriptions.hasSubscribers(API::ChannelSubscriptions::BLE));
    TEST_ASSERT_TRUE(subscriptions.hasSubscribers(API::ChannelSubscriptions::TELEMETRY));
}

void test_handle_message_treats_duplicate_unsubscribe_as_idempotent() {
    API::ChannelSubscriptions subscriptions;
    API::ChannelSubscriptions::Channel channel = API::ChannelSubscriptions::NONE;
    bool isSubscribe = true;
    bool changed = false;

    TEST_ASSERT_TRUE(subscriptions.handleMessage(
        34,
        "{\"subscribe\":\"telegram\"}",
        strlen("{\"subscribe\":\"telegram\"}")));

    TEST_ASSERT_TRUE(subscriptions.handleMessage(
        34,
        "{\"unsubscribe\":\"telegram\"}",
        strlen("{\"unsubscribe\":\"telegram\"}"),
        &channel,
        &isSubscribe,
        &changed));
    TEST_ASSERT_FALSE(isSubscribe);
    TEST_ASSERT_TRUE(changed);

    TEST_ASSERT_TRUE(subscriptions.handleMessage(
        34,
        "{\"unsubscribe\":\"telegram\"}",
        strlen("{\"unsubscribe\":\"telegram\"}"),
        &channel,
        &isSubscribe,
        &changed));
    TEST_ASSERT_FALSE(isSubscribe);
    TEST_ASSERT_FALSE(changed);
}

void test_unsubscribe_all_removes_client_from_every_channel_and_keeps_other_clients() {
    API::ChannelSubscriptions subscriptions;

    TEST_ASSERT_TRUE(subscriptions.subscribe(80, API::ChannelSubscriptions::BLE));
    TEST_ASSERT_TRUE(subscriptions.subscribe(80, API::ChannelSubscriptions::TELEMETRY));
    TEST_ASSERT_TRUE(subscriptions.subscribe(80, API::ChannelSubscriptions::TELEGRAM));
    TEST_ASSERT_TRUE(subscriptions.subscribe(81, API::ChannelSubscriptions::BLE));

    subscriptions.unsubscribeAll(80);

    TEST_ASSERT_FALSE(subscriptions.hasSubscribers(API::ChannelSubscriptions::TELEMETRY));
    TEST_ASSERT_FALSE(subscriptions.hasSubscribers(API::ChannelSubscriptions::TELEGRAM));
    TEST_ASSERT_TRUE(subscriptions.hasSubscribers(API::ChannelSubscriptions::BLE));
    TEST_ASSERT_EQUAL(static_cast<size_t>(1), subscriptions.getSubscriberCount(API::ChannelSubscriptions::BLE));
    TEST_ASSERT_TRUE(subscriptions._subscriptions.find(80) == subscriptions._subscriptions.end());
}

void test_handle_message_accepts_channel_aliases() {
    API::ChannelSubscriptions subscriptions;
    API::ChannelSubscriptions::Channel channel = API::ChannelSubscriptions::NONE;
    bool isSubscribe = false;
    bool changed = false;

    TEST_ASSERT_TRUE(subscriptions.handleMessage(
        44,
        "{\"subscribe\":\"system\"}",
        strlen("{\"subscribe\":\"system\"}"),
        &channel,
        &isSubscribe,
        &changed));

    TEST_ASSERT_TRUE(isSubscribe);
    TEST_ASSERT_TRUE(changed);
    TEST_ASSERT_EQUAL(API::ChannelSubscriptions::SYSTEM_STATUS, channel);
    TEST_ASSERT_TRUE(subscriptions.hasSubscribers(API::ChannelSubscriptions::SYSTEM_STATUS));
}

void test_handle_message_rejects_invalid_json() {
    API::ChannelSubscriptions subscriptions;
    TEST_ASSERT_TRUE(subscriptions.subscribe(90, API::ChannelSubscriptions::BLE));

    TEST_ASSERT_FALSE(subscriptions.handleMessage(55, "{invalid", strlen("{invalid")));
    TEST_ASSERT_TRUE(subscriptions.hasSubscribers(API::ChannelSubscriptions::BLE));
    TEST_ASSERT_EQUAL(static_cast<size_t>(1), subscriptions.getSubscriberCount(API::ChannelSubscriptions::BLE));
}

void test_handle_message_rejects_unknown_channel() {
    API::ChannelSubscriptions subscriptions;
    TEST_ASSERT_TRUE(subscriptions.subscribe(91, API::ChannelSubscriptions::TELEMETRY));

    TEST_ASSERT_FALSE(subscriptions.handleMessage(
        66,
        "{\"subscribe\":\"unknown\"}",
        strlen("{\"subscribe\":\"unknown\"}")));
    TEST_ASSERT_TRUE(subscriptions.hasSubscribers(API::ChannelSubscriptions::TELEMETRY));
    TEST_ASSERT_EQUAL(static_cast<size_t>(1), subscriptions.getSubscriberCount(API::ChannelSubscriptions::TELEMETRY));
}

void test_handle_message_with_both_subscribe_and_unsubscribe_prefers_subscribe_path() {
    API::ChannelSubscriptions subscriptions;
    API::ChannelSubscriptions::Channel channel = API::ChannelSubscriptions::NONE;
    bool isSubscribe = false;
    bool changed = false;

    const char* msg = "{\"subscribe\":\"ble\",\"unsubscribe\":\"telemetry\"}";
    TEST_ASSERT_TRUE(subscriptions.handleMessage(92, msg, strlen(msg), &channel, &isSubscribe, &changed));

    TEST_ASSERT_TRUE(isSubscribe);
    TEST_ASSERT_TRUE(changed);
    TEST_ASSERT_EQUAL(API::ChannelSubscriptions::BLE, channel);
    TEST_ASSERT_TRUE(subscriptions.hasSubscribers(API::ChannelSubscriptions::BLE));
    TEST_ASSERT_FALSE(subscriptions.hasSubscribers(API::ChannelSubscriptions::TELEMETRY));
}

void test_broadcast_sends_sync_payload_only_to_subscribed_clients() {
    API::ChannelSubscriptions subscriptions;
    const uint8_t payload[] = {0x54, 0x12, 0x34};

    TEST_ASSERT_TRUE(subscriptions.subscribe(100, API::ChannelSubscriptions::BLE));
    TEST_ASSERT_TRUE(subscriptions.subscribe(101, API::ChannelSubscriptions::TELEMETRY));
    TEST_ASSERT_TRUE(subscriptions.subscribe(102, API::ChannelSubscriptions::BLE));

    subscriptions.broadcast(nullptr, reinterpret_cast<httpd_handle_t>(0x1), API::ChannelSubscriptions::BLE, payload, sizeof(payload));

    std::vector<int> expectedFds = {100, 102};
    TEST_ASSERT_EQUAL(static_cast<int>(expectedFds.size()), static_cast<int>(TEST_STUBS::WS::syncBroadcastFds.size()));
    TEST_ASSERT_EQUAL_INT_ARRAY(expectedFds.data(), TEST_STUBS::WS::syncBroadcastFds.data(), static_cast<int>(expectedFds.size()));
    TEST_ASSERT_EQUAL(2, static_cast<int>(TEST_STUBS::WS::syncBroadcastPayloads.size()));
    for (const auto& sentPayload : TEST_STUBS::WS::syncBroadcastPayloads) {
        TEST_ASSERT_EQUAL(sizeof(payload), sentPayload.size());
        TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, sentPayload.data(), static_cast<unsigned int>(sizeof(payload)));
    }
}

void test_broadcast_is_noop_when_channel_has_no_subscribers() {
    API::ChannelSubscriptions subscriptions;
    const uint8_t payload[] = {0x01, 0x02};

    TEST_ASSERT_TRUE(subscriptions.subscribe(110, API::ChannelSubscriptions::TELEMETRY));
    subscriptions.broadcast(nullptr, reinterpret_cast<httpd_handle_t>(0x1), API::ChannelSubscriptions::BLE, payload, sizeof(payload));

    TEST_ASSERT_TRUE(TEST_STUBS::WS::syncBroadcastFds.empty());
    TEST_ASSERT_TRUE(TEST_STUBS::WS::syncBroadcastPayloads.empty());
}

void test_cached_counts_match_model_across_mixed_operation_sequence() {
    API::ChannelSubscriptions subscriptions;
    SubscriptionModel model;
    uint32_t state = 0xC6A4A793u;

    for (int step = 0; step < 250; ++step) {
        state = state * 1664525u + 1013904223u;
        const int operation = static_cast<int>(state % 4u);
        const int fd = 200 + static_cast<int>((state >> 3) % 6u);
        const Channel channel = kModelChannels[(state >> 8) % kModelChannels.size()];
        const uint16_t mask = static_cast<uint16_t>(channel);
        const auto modelIt = model.find(fd);
        const bool wasSubscribed = modelIt != model.end() && ((modelIt->second & mask) != 0);

        switch (operation) {
            case 0: {
                TEST_ASSERT_EQUAL(!wasSubscribed, subscriptions.subscribe(fd, channel));
                modelSubscribe(model, fd, channel);
                break;
            }
            case 1: {
                TEST_ASSERT_EQUAL(wasSubscribed, subscriptions.unsubscribe(fd, channel));
                modelUnsubscribe(model, fd, channel);
                break;
            }
            case 2: {
                subscriptions.unsubscribeAll(fd);
                modelUnsubscribeAll(model, fd);
                break;
            }
            default: {
                char message[64];
                const bool subscribeMessage = (state & 0x10000u) == 0;
                snprintf(
                    message,
                    sizeof(message),
                    subscribeMessage ? "{\"subscribe\":\"%s\"}" : "{\"unsubscribe\":\"%s\"}",
                    channel == Channel::BLE
                        ? "ble"
                        : channel == Channel::TELEMETRY
                              ? "telemetry"
                              : channel == Channel::TELEGRAM
                                    ? "telegram"
                                    : channel == Channel::MACROS ? "macros" : "system_status");

                bool changed = false;
                TEST_ASSERT_TRUE(subscriptions.handleMessage(fd, message, strlen(message), nullptr, nullptr, &changed));

                if (subscribeMessage) {
                    TEST_ASSERT_EQUAL(!wasSubscribed, changed);
                    modelSubscribe(model, fd, channel);
                } else {
                    TEST_ASSERT_EQUAL(wasSubscribed, changed);
                    modelUnsubscribe(model, fd, channel);
                }
                break;
            }
        }

        assertMatchesModel(subscriptions, model);
    }
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_handle_message_subscribes_known_channel);
    RUN_TEST(test_handle_message_treats_duplicate_subscribe_as_idempotent);
    RUN_TEST(test_subscribe_two_clients_same_channel_counts_both_and_releases_cleanly);
    RUN_TEST(test_handle_message_unsubscribes_only_requested_channel);
    RUN_TEST(test_handle_message_treats_duplicate_unsubscribe_as_idempotent);
    RUN_TEST(test_unsubscribe_all_removes_client_from_every_channel_and_keeps_other_clients);
    RUN_TEST(test_handle_message_accepts_channel_aliases);
    RUN_TEST(test_handle_message_rejects_invalid_json);
    RUN_TEST(test_handle_message_rejects_unknown_channel);
    RUN_TEST(test_handle_message_with_both_subscribe_and_unsubscribe_prefers_subscribe_path);
    RUN_TEST(test_broadcast_sends_sync_payload_only_to_subscribed_clients);
    RUN_TEST(test_broadcast_is_noop_when_channel_has_no_subscribers);
    RUN_TEST(test_cached_counts_match_model_across_mixed_operation_sequence);
    return UNITY_END();
}

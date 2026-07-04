#include "UsbTerminalApiService.h"

#include <PsychicJson.h>
#include <cstdio>
#include <cstring>
#include <esp_heap_caps.h>

#include "../../config/App.h"
#include "../../config/json/ConfigKeys.h"
#include "../../config/json/UsbTerminalConfigJson.h"
#include "../../system/logging/Logging.h"
#include "../../system/utils/json/JsonResponseWriter.h"
#include "../../usb_terminal/UsbTerminalService.h"
#include "../../usb_terminal/UsbTerminalSettingsService.h"
#include "core/config/ConfigManager.h"
#include <utils/ResponseUtils.h>

namespace API {

namespace {

constexpr size_t USB_TERMINAL_WS_DOC_SIZE = 512;
constexpr const char* kUsbTerminalConfigPath = "/api/usbterminal/config";
// Typical terminal frames are short text payloads; fixed WS slots avoid
// per-message heap allocations without changing the transport protocol.
constexpr size_t kUsbTerminalWsPayloadSlotSize = 3072;

} // namespace

UsbTerminalApiService::UsbTerminalApiService(PsychicHttpServer* server,
                                             SecurityManager* securityManager,
                                             POWER::PowerManager* powerManager,
                                             USB_TERMINAL::UsbTerminalService* terminalService,
                                             USB_TERMINAL::UsbTerminalSettingsService* settings)
    : BaseApiService(server, securityManager, powerManager, "api/usbterminal"),
      _terminalService(terminalService),
      _settings(settings),
      _wsAuthenticator(securityManager, AuthenticationPredicates::IS_ADMIN),
      _wsEndpoint(server, CONFIG::Keys::kWsUsbTerminal, CONFIG::Keys::kUsbTerminalBroadcastName, &_wsAuthenticator) {
    if (_settings) {
        _configEndpoint = std::make_unique<HttpEndpoint<RTC::UsbTerminalData>>(
            USB_TERMINAL::UsbTerminalSettingsService::readState,
            USB_TERMINAL::UsbTerminalSettingsService::updateState,
            _settings,
            _server,
            kUsbTerminalConfigPath,
            _securityManager,
            AuthenticationPredicates::IS_ADMIN,
            [this](PsychicRequest* request, JsonObject& jsonObject) {
                return validateConfigUpdate(request, jsonObject);
            },
            [this]() {
                if (_powerManager) {
                    _powerManager->notifyActivity(_activityTag);
                }
            });
    }

    // USB terminal stays an admin-only dedicated websocket, but client tracking
    // is now owned by the shared runtime/broadcaster. This service should only
    // care about terminal semantics: session snapshot on open, commands in, and
    // targeted output back to the owning fd.
    _wsEndpoint.setFrameHandler([this](httpd_req_t* req, int fd) {
        return handleFrame(req, fd);
    });
    _wsEndpoint.setOpenCallback([this](int fd) {
        sendSessionToFd(fd);
    });
    _wsEndpoint.setCleanupCallback([this](int fd) {
        handleClientCleanup(fd);
    });
    _wsEndpoint.setRequestCallback([this]() {
        if (_powerManager) {
            _powerManager->notifyActivity("ws/usbterminal");
        }
    });
}

UsbTerminalApiService::~UsbTerminalApiService() {
    if (_terminalService) {
        _terminalService->setWebOutputCallback(nullptr);
        _terminalService->setSessionChangeCallback(nullptr);
    }
}

void UsbTerminalApiService::begin() {
    if (_configEndpoint) {
        _configEndpoint->begin();
    }

    _wsEndpoint.begin(8, 4096, kUsbTerminalWsPayloadSlotSize);

    if (_terminalService) {
        _terminalService->setWebOutputCallback([this](const USB_TERMINAL::OutputEvent& event) {
            if (event.transport != USB_TERMINAL::SessionTransport::Web) {
                return;
            }

            const int fd = UsbTerminalWireCodec::parseClientId(event.targetId);
            if (fd >= 0) {
                sendOutputToFd(fd, event.phase, event.text ? event.text : "");
            }
        });

        _terminalService->setSessionChangeCallback([this](const USB_TERMINAL::SessionState& state) {
            (void)state;
            broadcastSessionToAll();
        });
    }
}

void UsbTerminalApiService::broadcastSessionToAll() {
    int targets[WEBSOCKET::MAX_BROADCAST_TARGETS];
    // Snapshot active clients from the shared broadcaster instead of maintaining
    // a second client registry locally. If these two ever drift apart, terminal
    // session rebroadcasts become flaky, so we keep a single source of truth.
    const size_t count = _wsEndpoint.broadcaster().snapshotClients(targets, WEBSOCKET::MAX_BROADCAST_TARGETS);

    for (size_t i = 0; i < count; i++) {
        sendSessionToFd(targets[i]);
    }
}

void UsbTerminalApiService::sendSessionToFd(int fd) {
    if (!_terminalService || fd < 0) {
        return;
    }

    char clientId[LIMITS::USB_TERMINAL::MAX_TARGET_ID_LEN] = {0};
    UsbTerminalWireCodec::formatClientId(fd, clientId, sizeof(clientId));
    const USB_TERMINAL::SessionSnapshot snapshot =
        _terminalService->getSessionSnapshot(USB_TERMINAL::SessionTransport::Web, clientId);

    int target = fd;
    static constexpr size_t kSessionFrameCapacity = 160;
    // Targeted direct serialization keeps the common "session changed" path
    // allocation-light and makes it obvious that this message is for one client,
    // not a broadcast to everyone.
    _wsEndpoint.broadcaster().broadcastSerialized(
        &target,
        1,
        kSessionFrameCapacity,
        [&snapshot](uint8_t* buffer, size_t capacity) -> size_t {
            UsbTerminalWireCodec::buildSessionJson(
                snapshot,
                reinterpret_cast<char*>(buffer),
                capacity);
            return strlen(reinterpret_cast<const char*>(buffer));
        },
        HTTPD_WS_TYPE_TEXT);
}

void UsbTerminalApiService::sendAck(int fd, const char* action, const USB_TERMINAL::CommandAck& ack) {
    if (fd < 0) {
        return;
    }

    char prefix[128];
    snprintf(
        prefix,
        sizeof(prefix),
        "{\"type\":\"ack\",\"action\":\"%s\",\"ok\":%s,\"message\":\"",
        action ? action : "unknown",
        ack.ok ? "true" : "false");

    const size_t jsonLen = UsbTerminalWireCodec::measureTextJson(prefix, ack.message, "\"}");
    int target = fd;
    _wsEndpoint.broadcaster().broadcastSerialized(
        &target,
        1,
        jsonLen,
        [prefix, &ack](uint8_t* buffer, size_t capacity) -> size_t {
            return UsbTerminalWireCodec::writeTextJson(
                prefix,
                ack.message,
                "\"}",
                reinterpret_cast<char*>(buffer),
                capacity);
        },
        HTTPD_WS_TYPE_TEXT);
}

void UsbTerminalApiService::sendError(int fd, const char* message) {
    if (fd < 0) {
        return;
    }

    const size_t jsonLen = UsbTerminalWireCodec::measureTextJson(
        "{\"type\":\"error\",\"message\":\"",
        message,
        "\"}");
    int target = fd;
    _wsEndpoint.broadcaster().broadcastSerialized(
        &target,
        1,
        jsonLen,
        [message](uint8_t* buffer, size_t capacity) -> size_t {
            return UsbTerminalWireCodec::writeTextJson(
                "{\"type\":\"error\",\"message\":\"",
                message,
                "\"}",
                reinterpret_cast<char*>(buffer),
                capacity);
        },
        HTTPD_WS_TYPE_TEXT);
}

void UsbTerminalApiService::sendOutputToFd(int fd, USB_TERMINAL::OutputPhase phase, const char* text) {
    if (fd < 0) {
        return;
    }

    char prefix[128];
    snprintf(
        prefix,
        sizeof(prefix),
        "{\"type\":\"output\",\"phase\":\"%s\",\"text\":\"",
        USB_TERMINAL::toPhaseString(phase));

    const size_t jsonLen = UsbTerminalWireCodec::measureTextJson(prefix, text, "\"}");
    int target = fd;
    _wsEndpoint.broadcaster().broadcastSerialized(
        &target,
        1,
        jsonLen,
        [prefix, text](uint8_t* buffer, size_t capacity) -> size_t {
            return UsbTerminalWireCodec::writeTextJson(
                prefix,
                text,
                "\"}",
                reinterpret_cast<char*>(buffer),
                capacity);
        },
        HTTPD_WS_TYPE_TEXT);
}

esp_err_t UsbTerminalApiService::handleFrame(httpd_req_t* req, int fd) {
    httpd_ws_frame_t ws_pkt{};
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        return ret;
    }

    if (ws_pkt.len == 0) {
        return ESP_OK;
    }

    // As on /ws/system, the first recv only peeks the frame metadata. Returning
    // early on an oversized payload would leave the body unread on the socket,
    // which can corrupt parsing of the next frame. USB terminal commands are
    // small control messages, so oversize input is a protocol violation and we
    // close the client instead of buffering arbitrary data.
    if (ws_pkt.len > LIMITS::API::WS_MESSAGE_MAX_SIZE) {
        return rejectOversizeFrame(fd, ws_pkt.len, LIMITS::API::WS_MESSAGE_MAX_SIZE);
    }

    char payload[LIMITS::API::WS_MESSAGE_MAX_SIZE + 1];
    ws_pkt.payload = reinterpret_cast<uint8_t*>(payload);
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) {
        return ret;
    }
    payload[ws_pkt.len] = '\0';

    SYSTEM::SpiRamJsonDocument doc(USB_TERMINAL_WS_DOC_SIZE);
    const DeserializationError err = deserializeJson(doc, payload);
    if (err || doc.overflowed()) {
        sendError(fd, "Invalid terminal request payload.");
        return ESP_OK;
    }

    JsonObject obj = doc.as<JsonObject>();
    const char* type = obj["type"] | "";
    char clientId[LIMITS::USB_TERMINAL::MAX_TARGET_ID_LEN] = {0};
    UsbTerminalWireCodec::formatClientId(fd, clientId, sizeof(clientId));

    if (!_terminalService) {
        sendError(fd, "USB terminal service is unavailable.");
        return ESP_OK;
    }

    if (strcmp(type, "execute") == 0) {
        const char* command = obj["command"] | "";
        USB_TERMINAL::CommandAck ack =
            _terminalService->execute(command, USB_TERMINAL::SessionTransport::Web, clientId);
        sendAck(fd, "execute", ack);
        return ESP_OK;
    }

    if (strcmp(type, "cancel") == 0) {
        const USB_TERMINAL::CommandAck ack =
            _terminalService->execute("cancel", USB_TERMINAL::SessionTransport::Web, clientId);
        sendAck(fd, "cancel", ack);
        return ESP_OK;
    }

    if (strcmp(type, "status") == 0) {
        const USB_TERMINAL::CommandAck ack =
            _terminalService->execute("status", USB_TERMINAL::SessionTransport::Web, clientId);
        sendAck(fd, "status", ack);
        return ESP_OK;
    }

    sendError(fd, "Unsupported terminal request type.");
    return ESP_OK;
}

esp_err_t UsbTerminalApiService::rejectOversizeFrame(int fd,
                                                     size_t frameLen,
                                                     size_t maxLen) {
    LOGW("Closing /ws/usbterminal fd %d after oversize frame (%u > %u bytes)",
         fd,
         static_cast<unsigned>(frameLen),
         static_cast<unsigned>(maxLen));

    // Release runtime/client ownership before forcing the socket closed so the
    // terminal session cannot stay latched to a client whose unread payload
    // will never be drained.
    _wsEndpoint.broadcaster().removeClient(fd, true);
    handleClientCleanup(fd);
    return ESP_OK;
}

void UsbTerminalApiService::cleanupClient(int fd) {
    _wsEndpoint.cleanupClient(fd);
}

void UsbTerminalApiService::handleClientCleanup(int fd) {
    if (_terminalService) {
        // Transport cleanup and terminal ownership cleanup are separate on
        // purpose: the runtime removes the socket, and only then we release the
        // domain-level session for that fd.
        char clientId[LIMITS::USB_TERMINAL::MAX_TARGET_ID_LEN] = {0};
        UsbTerminalWireCodec::formatClientId(fd, clientId, sizeof(clientId));
        _terminalService->handleDisconnect(USB_TERMINAL::SessionTransport::Web, clientId);
    }
}

StateHandlerResult UsbTerminalApiService::validateConfigUpdate(PsychicRequest* request, JsonObject& jsonObject) {
    (void)request;

    RTC::UsbTerminalData currentState{};
    bool loaded = false;
    if (_settings) {
        _settings->read([&](RTC::UsbTerminalData& state) {
            currentState = state;
            loaded = true;
        });
    }

    if (!loaded) {
        return StateHandlerResult::failure("internal/update_failed");
    }

    RTC::UsbTerminalData nextState = currentState;
    CONFIG::JSON::deserializeUsbTerminal(jsonObject, nextState);

    const bool disablingTerminal = currentState.enabled && !nextState.enabled;
    if (disablingTerminal && _terminalService) {
        const USB_TERMINAL::SessionSnapshot snapshot =
            _terminalService->getSessionSnapshot(USB_TERMINAL::SessionTransport::None, nullptr);
        if (snapshot.busy) {
            return StateHandlerResult::failure("usb_terminal/session_active", 409);
        }
    }

    return StateHandlerResult::success();
}

} // namespace API

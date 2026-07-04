/**
 * @file BaseApiService.h
 * @brief Base class for API services with common wrapper patterns
 *
 * Provides DRY utilities for HTTP endpoint registration:
 * - Authentication wrappers (admin, authenticated)
 * - Activity notification integration
 * - Common constructor pattern
 *
 * Usage example:
 * @code
 * class MyApiService : public BaseApiService {
 * public:
 *     MyApiService(PsychicHttpServer* server, SecurityManager* securityManager)
 *         : BaseApiService(server, securityManager, "api/my") {}
 *
 *     void begin() override {
 *         _server->on("/api/my/data", HTTP_GET,
 *             wrapAuth([this](PsychicRequest* r) { return handleData(r); }));
 *
 *         _server->on("/api/my/config", HTTP_POST,
 *             wrapAdmin([this](PsychicRequest* r) { return handleConfig(r); }));
 *     }
 *
 * private:
 *     esp_err_t handleData(PsychicRequest* request);
 *     esp_err_t handleConfig(PsychicRequest* request);
 * };
 * @endcode
 */

#pragma once

#include <PsychicHttpServer.h>
#include <security/SecurityManager.h>
#include <utils/ResponseUtils.h>
#include "../system/power/PowerManager.h"
#include "../system/memory/PsramAllocator.h"

namespace API {

class BaseApiService {
public:
    virtual ~BaseApiService() = default;

    /**
     * Register API endpoints - must be implemented by derived classes
     */
    virtual void begin() = 0;

    /**
     * @brief Called when a client disconnects.
     * Can be overridden by services that manage WebSocket connections.
     * @param fd Client socket file descriptor
     */
    virtual void cleanupClient(int fd) {}

protected:
    /**
     * Construct base API service
     * 
     * @param server HTTP server instance
     * @param securityManager Security manager for auth wrappers
     * @param activityTag Tag for power activity notification (e.g., "api/sensors")
     */
    BaseApiService(PsychicHttpServer* server, SecurityManager* securityManager,
                   POWER::PowerManager* powerManager, const char* activityTag)
        : _server(server)
        , _securityManager(securityManager)
        , _powerManager(powerManager)
        , _activityTag(activityTag) {}

    /**
     * Wrap handler with authentication check (any authenticated user)
     * 
     * Automatically notifies PowerManager of API activity.
     * 
     * @param fn Handler function taking PsychicRequest* and returning esp_err_t
     * @return Wrapped handler suitable for server->on()
     */
    template<typename Fn>
    auto wrapAuth(Fn&& fn) {
        const char* tag = _activityTag;
        POWER::PowerManager* pm = _powerManager;
        return _securityManager->wrapRequest(
            [fn, tag, pm](PsychicRequest* request) -> esp_err_t {
                if (pm) pm->notifyActivity(tag);
                return fn(request);
            },
            AuthenticationPredicates::IS_AUTHENTICATED
        );
    }

    /**
     * Wrap handler with admin authentication check
     * 
     * Automatically notifies PowerManager of API activity.
     * 
     * @param fn Handler function taking PsychicRequest* and returning esp_err_t
     * @return Wrapped handler suitable for server->on()
     */
    template<typename Fn>
    auto wrapAdmin(Fn&& fn) {
        const char* tag = _activityTag;
        POWER::PowerManager* pm = _powerManager;
        return _securityManager->wrapRequest(
            [fn, tag, pm](PsychicRequest* request) -> esp_err_t {
                if (pm) pm->notifyActivity(tag);
                return fn(request);
            },
            AuthenticationPredicates::IS_ADMIN
        );
    }

    /**
     * Wrap handler with custom authentication predicate
     * 
     * @param fn Handler function
     * @param predicate Authentication predicate to use
     * @return Wrapped handler suitable for server->on()
     */
    template<typename Fn>
    auto wrapCustom(Fn&& fn, AuthenticationPredicate predicate) {
        const char* tag = _activityTag;
        POWER::PowerManager* pm = _powerManager;
        return _securityManager->wrapRequest(
            [fn, tag, pm](PsychicRequest* request) -> esp_err_t {
                if (pm) pm->notifyActivity(tag);
                return fn(request);
            },
            predicate
        );
    }

    /**
     * Parse JSON body into a PSRAM-backed document with standardized errors.
     *
     * Returns JSON errors:
     * - 413 input/payload_too_large
     * - 400 input/invalid_json
     */
    template <typename Fn>
    esp_err_t parseJsonBody(PsychicRequest* request, size_t docSize, Fn&& handler, size_t limit) const {
        if (request->contentLength() > limit) {
            return Response::payloadTooLarge(request);
        }

        SYSTEM::SpiRamJsonDocument doc(docSize);
        DeserializationError err = deserializeJson(doc, request->body());
        if (err == DeserializationError::NoMemory || doc.overflowed()) {
            return Response::payloadTooLarge(request);
        }
        if (err) {
            return Response::invalidJson(request, err.c_str());
        }

        return handler(doc);
    }

    PsychicHttpServer* _server;
    SecurityManager* _securityManager;
    POWER::PowerManager* _powerManager;
    const char* _activityTag;
};

}  // namespace API

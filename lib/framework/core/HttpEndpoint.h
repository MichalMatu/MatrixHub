#ifndef HttpEndpoint_h
#define HttpEndpoint_h

#include <functional>

#include <esp_log.h>
#include <PsychicHttp.h>

#include <security/SecurityManager.h>
#include <core/StatefulService.h>
#include <utils/ResponseUtils.h>

#include "../../../src/system/memory/PsramAllocator.h"

#define HTTP_ENDPOINT_ORIGIN_ID "http"

#ifndef SVK_TAG
#define SVK_TAG "HttpEndpoint"
#endif


using namespace std::placeholders; // for `_1` etc

template <class T>
class HttpEndpoint
{
protected:
    using JsonRequestValidator = std::function<StateHandlerResult(PsychicRequest *request, JsonObject &root)>;
    using RequestActivityNotifier = std::function<void()>;

    static constexpr size_t kMaxJsonPayloadBytes = Response::kStateJsonPayloadLimitBytes;

    JsonStateReader<T> _stateReader;
    JsonStateUpdater<T> _stateUpdater;
    IStatefulService<T> *_statefulService;
    SecurityManager *_securityManager;
    AuthenticationPredicate _writeAuthenticationPredicate;
    AuthenticationPredicate _readAuthenticationPredicate;
    JsonRequestValidator _requestValidator;
    RequestActivityNotifier _activityNotifier;
    PsychicHttpServer *_server;
    const char *_servicePath;

public:
    HttpEndpoint(JsonStateReader<T> stateReader,
                 JsonStateUpdater<T> stateUpdater,
                 IStatefulService<T> *statefulService,
                 PsychicHttpServer *server,
                 const char *servicePath,
                 SecurityManager *securityManager,
                 AuthenticationPredicate authenticationPredicate = AuthenticationPredicates::IS_ADMIN,
                 JsonRequestValidator requestValidator = nullptr,
                 RequestActivityNotifier activityNotifier = nullptr) : HttpEndpoint(
                                                                                                             stateReader,
                                                                                                             stateUpdater,
                                                                                                             statefulService,
                                                                                                             server,
                                                                                                             servicePath,
                                                                                                             securityManager,
                                                                                                             authenticationPredicate,
                                                                                                             authenticationPredicate,
                                                                                                             requestValidator,
                                                                                                             activityNotifier)
    {
    }

    HttpEndpoint(JsonStateReader<T> stateReader,
                 JsonStateUpdater<T> stateUpdater,
                 IStatefulService<T> *statefulService,
                 PsychicHttpServer *server,
                 const char *servicePath,
                 SecurityManager *securityManager,
                 AuthenticationPredicate writeAuthenticationPredicate,
                 AuthenticationPredicate readAuthenticationPredicate,
                 JsonRequestValidator requestValidator = nullptr,
                 RequestActivityNotifier activityNotifier = nullptr) : _stateReader(stateReader),
                                                                       _stateUpdater(stateUpdater),
                                                                       _statefulService(statefulService),
                                                                       _securityManager(securityManager),
                                                                       _writeAuthenticationPredicate(writeAuthenticationPredicate),
                                                                       _readAuthenticationPredicate(readAuthenticationPredicate),
                                                                       _requestValidator(requestValidator),
                                                                       _activityNotifier(activityNotifier),
                                                                       _server(server),
                                                                       _servicePath(servicePath)
    {
    }

    // register the web server on() endpoints
    void begin()
    {

// OPTIONS (for CORS preflight)
#ifdef ENABLE_CORS
        _server->on(_servicePath,
                    HTTP_OPTIONS,
                    _securityManager->wrapRequest(
                        [this](PsychicRequest *request)
                        {
                            return request->reply(200);
                        },
                        AuthenticationPredicates::IS_AUTHENTICATED));
#endif

        // GET
        _server->on(_servicePath,
                    HTTP_GET,
                    _securityManager->wrapRequest(
                        [this](PsychicRequest *request)
                        {
                            return handleReadRequest(request);
                        },
                        _readAuthenticationPredicate));
        ESP_LOGV(SVK_TAG, "Registered GET endpoint: %s", _servicePath);

        // POST
        _server->on(_servicePath,
                    HTTP_POST,
                    _securityManager->wrapRequest(
                        [this](PsychicRequest *request)
                        {
                            return handleWriteRequest(request);
                        },
                        _writeAuthenticationPredicate));

        ESP_LOGV(SVK_TAG, "Registered POST endpoint: %s", _servicePath);
    }

    esp_err_t handleReadRequest(PsychicRequest *request)
    {
        notifyActivity();
        return sendCurrentState(request);
    }

    esp_err_t handleWriteRequest(PsychicRequest *request)
    {
        notifyActivity();

        if (request && request->contentLength() > kMaxJsonPayloadBytes)
        {
            return Response::payloadTooLarge(request);
        }

        SYSTEM::SpiRamJsonDocument jsonDocument(kMaxJsonPayloadBytes);
        DeserializationError error = deserializeJson(jsonDocument, request->body());
        if (error == DeserializationError::NoMemory || jsonDocument.overflowed())
        {
            return Response::payloadTooLarge(request);
        }
        if (error || !jsonDocument.is<JsonObject>())
        {
            return Response::invalidJson(request, error ? error.c_str() : nullptr);
        }

        JsonObject jsonObject = jsonDocument.as<JsonObject>();
        if (_requestValidator)
        {
            const StateHandlerResult validationResult = _requestValidator(request, jsonObject);
            if (!validationResult.ok)
            {
                return Response::error(
                    request,
                    validationResult.httpStatus,
                    validationResult.errorCode ? validationResult.errorCode : "internal/update_failed");
            }
        }

        const StateTransactionResult txResult =
            _statefulService->updateAndPropagate(jsonObject, _stateUpdater, HTTP_ENDPOINT_ORIGIN_ID);
        const StateUpdateResult outcome = txResult.outcome;

        if (outcome == StateUpdateResult::ERROR)
        {
            return Response::error(
                request,
                txResult.httpStatus,
                txResult.errorCode ? txResult.errorCode : "internal/update_failed");
        }

        return sendCurrentState(request);
    }

private:
    void notifyActivity() const
    {
        if (_activityNotifier) {
            _activityNotifier();
        }
    }

    esp_err_t sendCurrentState(PsychicRequest *request)
    {
        PsychicJsonResponse response = PsychicJsonResponse(request, false);
        JsonObject jsonObject = response.getRoot();
        const StateHandlerResult readResult = _statefulService->read(jsonObject, _stateReader);
        if (!readResult.ok) {
            return Response::error(
                request,
                readResult.httpStatus,
                readResult.errorCode ? readResult.errorCode : "internal/update_failed");
        }

        return response.send();
    }
};

#endif

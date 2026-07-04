#include <security/RequestAuthorizer.h>

#include <utils/ResponseUtils.h>
#include "../../src/system/errors/ErrorCodes.h"
#include "../../src/system/logging/Logging.h"

#undef LOG_TAG
#define LOG_TAG "Security"

namespace {
constexpr size_t kMaxRequestPayloadBytes = Response::kDefaultJsonPayloadLimitBytes;
constexpr size_t kMaxJsonPayloadBytes = Response::kJsonAuthCallbackPayloadLimitBytes;
}

RequestAuthorizer::RequestAuthorizer(RateLimiter *rateLimiter, JwtAuthenticator jwtAuthenticator)
    : _rateLimiter(rateLimiter),
      _jwtAuthenticator(std::move(jwtAuthenticator))
{
}

void RequestAuthorizer::setJwtAuthenticator(JwtAuthenticator jwtAuthenticator)
{
    _jwtAuthenticator = std::move(jwtAuthenticator);
}

Authentication RequestAuthorizer::authenticateRequest(PsychicRequest *request) const
{
    if (!request || !_jwtAuthenticator)
    {
        return Authentication();
    }

    if (request->hasHeader(AUTHORIZATION_HEADER))
    {
        auto value = request->header(AUTHORIZATION_HEADER);
        if (value.startsWith(AUTHORIZATION_HEADER_PREFIX))
        {
            value = value.substring(AUTHORIZATION_HEADER_PREFIX_LEN);
            return _jwtAuthenticator(value.c_str());
        }
    }

    return Authentication();
}

bool RequestAuthorizer::authorizeRequest(PsychicRequest *request,
                                         AuthenticationPredicate predicate,
                                         bool loadParams) const
{
    if (!request || !request->client())
    {
        return false;
    }

    if (loadParams)
    {
        request->loadParams();
    }

    const IPAddress remoteIp = request->client()->remoteIP();
    if (_rateLimiter && !_rateLimiter->shouldAllow(remoteIp))
    {
        LOGW("Request rate limit exceeded for IP: %s uri=%s",
             remoteIp.toString().c_str(),
             request->uri().c_str());
        Response::error(request, 429, ErrorCodes::Auth::RATE_LIMITED, "Too Many Requests");
        return false;
    }

    Authentication authentication = authenticateRequest(request);
    if (!authentication.authenticated)
    {
        Response::error(request, 401, ErrorCodes::Auth::UNAUTHORIZED);
        return false;
    }

    if (!predicate(authentication))
    {
        Response::error(request, 403, ErrorCodes::Auth::FORBIDDEN);
        return false;
    }

    return true;
}

PsychicRequestFilterFunction RequestAuthorizer::filterRequest(AuthenticationPredicate predicate) const
{
    return [this, predicate](PsychicRequest *request)
    {
        if (request && request->uri().isEmpty() && request->method() == HTTP_DELETE)
        {
            return true;
        }

        return authorizeRequest(request, predicate, true);
    };
}

PsychicHttpRequestCallback RequestAuthorizer::wrapRequest(PsychicHttpRequestCallback onRequest,
                                                          AuthenticationPredicate predicate) const
{
    return [this, onRequest, predicate](PsychicRequest *request)
    {
        if (request && request->contentLength() > kMaxRequestPayloadBytes)
        {
            return Response::payloadTooLarge(request);
        }

        if (!authorizeRequest(request, predicate, true))
        {
            return (request && request->responseSent()) ? ESP_OK : ESP_FAIL;
        }

        return onRequest(request);
    };
}

PsychicJsonRequestCallback RequestAuthorizer::wrapCallback(PsychicJsonRequestCallback onRequest,
                                                           AuthenticationPredicate predicate) const
{
    return [this, onRequest, predicate](PsychicRequest *request, JsonVariant &json)
    {
        if (request && request->contentLength() > kMaxJsonPayloadBytes)
        {
            return Response::payloadTooLarge(request);
        }

        if (!authorizeRequest(request, predicate, true))
        {
            return (request && request->responseSent()) ? ESP_OK : ESP_FAIL;
        }

        return onRequest(request, json);
    };
}

#ifndef RequestAuthorizer_h
#define RequestAuthorizer_h

#include <functional>

#include <security/RateLimiter.h>
#include <security/SecurityManager.h>

class RequestAuthorizer
{
public:
    using JwtAuthenticator = std::function<Authentication(const char *jwt)>;

    RequestAuthorizer(RateLimiter *rateLimiter, JwtAuthenticator jwtAuthenticator = nullptr);

    void setJwtAuthenticator(JwtAuthenticator jwtAuthenticator);

    Authentication authenticateRequest(PsychicRequest *request) const;
    bool authorizeRequest(PsychicRequest *request,
                          AuthenticationPredicate predicate,
                          bool loadParams) const;

    PsychicRequestFilterFunction filterRequest(AuthenticationPredicate predicate) const;
    PsychicHttpRequestCallback wrapRequest(PsychicHttpRequestCallback onRequest,
                                           AuthenticationPredicate predicate) const;
    PsychicJsonRequestCallback wrapCallback(PsychicJsonRequestCallback onRequest,
                                            AuthenticationPredicate predicate) const;

private:
    RateLimiter *_rateLimiter;
    JwtAuthenticator _jwtAuthenticator;
};

#endif // end RequestAuthorizer_h

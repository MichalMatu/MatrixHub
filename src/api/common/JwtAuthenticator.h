#pragma once

#include "IWebSocketAuthenticator.h"
#include <security/SecurityManager.h>

namespace API {

/**
 * @brief Implementation of IWebSocketAuthenticator that validates JWT tokens
 * passed in the access_token cookie.
 */
class JwtAuthenticator : public IWebSocketAuthenticator {
public:
    /**
     * @param securityManager Pointer to the SecurityManager used for JWT validation.
     *                        Must NOT be nullptr.
     */
    explicit JwtAuthenticator(SecurityManager* securityManager,
                              AuthenticationPredicate predicate = AuthenticationPredicates::IS_AUTHENTICATED);
    
    ~JwtAuthenticator() override = default;

    bool authenticate(httpd_req_t *req) override;

private:
    SecurityManager* _securityManager;
    AuthenticationPredicate _predicate;
};

} // namespace API

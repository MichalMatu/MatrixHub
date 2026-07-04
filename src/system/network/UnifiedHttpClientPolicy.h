#pragma once

namespace NETWORK {

// Small policy helper extracted for unit-test coverage. The runtime rule is:
// a reused HTTP connection may stay alive only when the request succeeded AND
// this layer actually consumed the response body. Status-only callers
// intentionally close after 2xx to avoid leaving unread bytes on keep-alive.
constexpr bool shouldCloseConnectionAfterSuccess(bool readSuccess,
                                                 bool reuseEnabled,
                                                 bool callerConsumesBody) {
    return !readSuccess || !reuseEnabled || !callerConsumesBody;
}

}  // namespace NETWORK

#pragma once

#include <Arduino.h>

namespace API::RequestQuery {

// Shared query-flag parser for HTTP endpoints that accept permissive boolean
// values (`1`, `true`, `yes`, non-zero integers). This was centralized after
// Alarms/System API started drifting with duplicated local helpers.
//
// If request parameters such as `details=1` or `includeStatus=yes` ever behave
// differently between endpoints, debug from here first and only then inspect
// the individual handler branches.
inline bool isTruthyParam(const String& value) {
    if (value.length() == 0) {
        return false;
    }

    if (value == "1" || value == "true" || value == "TRUE") {
        return true;
    }

    if (value == "yes" || value == "YES") {
        return true;
    }

    return value.toInt() != 0;
}

}  // namespace API::RequestQuery

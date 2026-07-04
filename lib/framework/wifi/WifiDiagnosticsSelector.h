#pragma once

#include <cstddef>
#include <cstring>

namespace WIFI_DIAGNOSTICS {

constexpr size_t kNoNetworkIndex = static_cast<size_t>(-1);

template <typename NetworkList, typename GetSsid>
size_t selectNetworkIndex(const NetworkList& networks,
                          const char* connectedSsid,
                          bool staConnected,
                          size_t currentNetworkIndex,
                          GetSsid getSsid) {
    if (networks.empty()) {
        return kNoNetworkIndex;
    }

    if (staConnected && connectedSsid && connectedSsid[0] != '\0') {
        for (size_t i = 0; i < networks.size(); i++) {
            const char* candidate = getSsid(networks[i]);
            if (candidate && std::strcmp(candidate, connectedSsid) == 0) {
                return i;
            }
        }
    }

    if (currentNetworkIndex < networks.size()) {
        return currentNetworkIndex;
    }

    return 0;
}

}  // namespace WIFI_DIAGNOSTICS

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <atomic>
#include <NetworkClient.h> // Or appropriate base class for stream
#include <HTTPClient.h>
#include "TlsManager.h"
#include "../runtime/TelegramRuntimeConfigView.h"
#include "TelegramClient.h" // For FetchResult

namespace TELEGRAM {

struct TelegramClientBuffers;

/**
 * @brief Handles Telegram updates long-polling
 * 
 * Extracted from ApiExecutor.
 */
class UpdatesFetcher {
public:
    /**
     * @brief Start a long-poll request for updates
     * 
     * The URL scratch buffer is provided explicitly by TelegramClient so this
     * polling path no longer depends on a translation-unit global buffer.
     *
     * @return Helper method to start getting updates stream.
     *         Returns pointer to NetworkClient stream on success (caller must parse).
     *         Returns nullptr on error or timeout (check result.httpCode).
     */
    static NetworkClient* beginStream(
        int64_t lastUpdateId,
        FetchResult& result,
        int timeoutSec,
        const TelegramRuntimeConfigView& config,
        TelegramClientBuffers& buffers,
        std::atomic<bool>* cancelFlag,
        TlsManager& tlsMgr,
        HTTPClient& http
    );
};

} // namespace TELEGRAM

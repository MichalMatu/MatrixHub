#include "LogTailHandler.h"
#include "../utils/JsonFormatter.h"
#include "../../../system/logging/LogRingBuffer.h"
#include "../../../system/logging/Logging.h"
#include "../../../config/System.h"
#include <utils/ResponseUtils.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "LogTailHandler";

namespace API {
namespace Handlers {

namespace {

constexpr uint16_t kMaxTailResponseLines = 128;

void yieldLogTailBatch() {
    // Keep tail streaming cooperative, but yield once per copied batch rather
    // than once per line. The previous per-line 1 ms delay made 500-line tail
    // requests pay hundreds of milliseconds of artificial latency in the admin UI.
    vTaskDelay(pdMS_TO_TICKS(1));
}

}  // namespace

esp_err_t LogTailHandler::handleTail(PsychicRequest* request) {
    uint16_t lines = 20;
    if (request->hasParam("lines")) {
        lines = static_cast<uint16_t>(request->getParam("lines")->value().toInt());
        if (lines == 0) lines = 1;
        if (lines > LOG::RingBuffer::kFixedCapacity) lines = LOG::RingBuffer::kFixedCapacity;
        if (lines > kMaxTailResponseLines) lines = kMaxTailResponseLines;
    }

    httpd_req_t* req = request->request();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");

    char headerBuf[64];
    int len = snprintf(headerBuf, sizeof(headerBuf), "{\"capacity\":%zu,\"lines\":[", LOG::RingBuffer::kFixedCapacity);
    if (len > 0 && len < (int)sizeof(headerBuf)) {
        httpd_resp_send_chunk(req, headerBuf, len);
    } else {
        httpd_resp_send_chunk(req, "{\"capacity\":0,\"lines\":[", 23);
    }

    // Allocate separate buffers for JSON output and escape work to prevent overlaps
    static constexpr size_t kJsonBufSize = LOG_CFG::LOG_TAIL_JSON_BUF_BYTES;
    static constexpr size_t kEscapeTagSize = LOG_CFG::LOG_TAIL_ESCAPED_TAG_BYTES;
    static constexpr size_t kEscapeMsgSize = LOG_CFG::LOG_TAIL_ESCAPED_MSG_BYTES;
    static constexpr size_t kEscapeBufSize = kEscapeTagSize + kEscapeMsgSize;
    static constexpr size_t kOutChunkSize  = LOG_CFG::LOG_TAIL_OUT_CHUNK_BYTES;

    // Allocate everything as one continuous block in PSRAM to reduce heap fragmentation
    char* masterBuf = (char*)heap_caps_malloc(kJsonBufSize + kEscapeBufSize + kOutChunkSize, MALLOC_CAP_SPIRAM);
    if (!masterBuf) {
        httpd_resp_send_chunk(req, "]}", 2);
        httpd_resp_send_chunk(req, NULL, 0);
        return ESP_ERR_NO_MEM;
    }

    char* jsonBuf    = masterBuf;
    char* escapedTag = masterBuf + kJsonBufSize;
    char* escapedMsg = masterBuf + kJsonBufSize + kEscapeTagSize;
    char* outChunk   = masterBuf + kJsonBufSize + kEscapeBufSize;
    size_t outLen    = 0;

    const uint16_t chunkLines = (lines < LOG_CFG::LOG_TAIL_COPY_CHUNK_LINES)
        ? lines
        : LOG_CFG::LOG_TAIL_COPY_CHUNK_LINES;

    LOG::Line* tailLines = (LOG::Line*)heap_caps_malloc(sizeof(LOG::Line) * chunkLines, MALLOC_CAP_SPIRAM);
    if (!tailLines) {
        heap_caps_free(masterBuf);
        httpd_resp_send_chunk(req, "]}", 2);
        httpd_resp_send_chunk(req, NULL, 0);
        return ESP_ERR_NO_MEM;
    }

    bool first = true;
    uint32_t sinceMs = 0;
    if (request->hasParam("since")) {
        sinceMs = static_cast<uint32_t>(request->getParam("since")->value().toInt());
        if (sinceMs > (uint32_t)millis()) {
            sinceMs = 0;
        }
    }

    size_t offset = 0;
    bool done = false;
    
    while (!done) {
        const size_t copied = LOG::RingBuffer::copyTailRange(tailLines, lines, offset, chunkLines);
        if (copied == 0) {
            break;
        }

        // Optimization: since the buffer returns lines from oldest to newest in the requested range,
        // and we fetch windows starting from oldest offset first,
        // if the *newest* item in this copied window is still too old, we can skip processing the entire window.
        // wait, copyTailRange copies from oldest-to-newest inside the window.
        if (sinceMs > 0 && tailLines[copied - 1].timestampMs <= sinceMs) {
            offset += copied;
            if (!done && copied == chunkLines) {
                yieldLogTailBatch();
            }
            continue;
        }

        for (size_t i = 0; i < copied; i++) {
            const LOG::Line& line = tailLines[i];
            if (sinceMs > 0 && line.timestampMs <= sinceMs) {
                continue;
            }

            if (!first) {
                if (outLen + 1 >= kOutChunkSize) {
                    httpd_resp_send_chunk(req, outChunk, outLen);
                    outLen = 0;
                }
                outChunk[outLen++] = ',';
            }
            first = false;

            const char* lvl = "I";
            switch (line.levelChar) {
                case 'E': lvl = "E"; break;
                case 'W': lvl = "W"; break;
                case 'I': lvl = "I"; break;
                case 'D': lvl = "D"; break;
                case 'V': lvl = "V"; break;
                default: break;
            }

            Utils::escapeJsonString(escapedTag, kEscapeTagSize, line.tag);
            Utils::escapeJsonString(escapedMsg, kEscapeMsgSize, line.message);

            // Ultraszybkie, hardware-friendly składanie JSONa bez powolnego snprintf
            char* ptr = jsonBuf;
            *ptr++ = '{'; *ptr++ = '"'; *ptr++ = 't'; *ptr++ = '"'; *ptr++ = ':';
            ptr += Utils::fast_u32_to_str(ptr, line.timestampMs);

            *ptr++ = ','; *ptr++ = '"'; *ptr++ = 'l'; *ptr++ = '"'; *ptr++ = ':'; *ptr++ = '"';
            *ptr++ = *lvl; 
            *ptr++ = '"';

            *ptr++ = ','; *ptr++ = '"'; *ptr++ = 'g'; *ptr++ = '"'; *ptr++ = ':'; *ptr++ = '"';
            const char* tagPtr = escapedTag;
            while (*tagPtr) *ptr++ = *tagPtr++;
            *ptr++ = '"';

            *ptr++ = ','; *ptr++ = '"'; *ptr++ = 'm'; *ptr++ = '"'; *ptr++ = ':'; *ptr++ = '"';
            const char* msgPtr = escapedMsg;
            while (*msgPtr) *ptr++ = *msgPtr++;
            *ptr++ = '"'; *ptr++ = '}';
            
            int len = ptr - jsonBuf;

            if (len > 0 && len < (int)kJsonBufSize) {
                if (outLen + len >= kOutChunkSize) {
                    if (outLen > 0) {
                        httpd_resp_send_chunk(req, outChunk, outLen);
                        outLen = 0;
                    }
                    if (len >= (int)kOutChunkSize) { // Edge case: individual line exceeds chunk buffer entirely
                        httpd_resp_send_chunk(req, jsonBuf, len);
                        len = 0;
                    }
                }
                if (len > 0) {
                    memcpy(outChunk + outLen, jsonBuf, len);
                    outLen += len;
                }
            }
        }

        offset += copied;
        if (!done && copied == chunkLines) {
            yieldLogTailBatch();
        }
    }

    if (outLen > 0) {
        httpd_resp_send_chunk(req, outChunk, outLen);
    }

    heap_caps_free(tailLines);
    heap_caps_free(masterBuf);

    httpd_resp_send_chunk(req, "]}", 2);
    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

esp_err_t LogTailHandler::handleClear(PsychicRequest* request) {
    LOG::Logging::clearBuffer();

    return Response::success(request, [](JsonVariant& root) {
        root["ok"] = true;
        root["status"] = "cleared";
    });
}

} // namespace Handlers
} // namespace API

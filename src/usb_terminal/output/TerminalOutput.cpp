#include "TerminalOutput.h"
#include <esp_heap_caps.h>
#include <cctype>
#include <cstring>
#include <cstdio>
#include "../../system/logging/Logging.h"

#undef LOG_TAG
#define LOG_TAG "TerminalOut"

namespace USB_TERMINAL {

TerminalOutput::TerminalOutput()
    : _accumulatedOutput(nullptr),
      _outputLen(0),
      _totalBytesCaptured(0),
      _captureStartTime(0),
      _lastFlushTime(0),
      _isCapturing(false) {
    _targetId[0] = '\0';
}

TerminalOutput::~TerminalOutput() {
    if (_accumulatedOutput) {
        heap_caps_free(_accumulatedOutput);
        _accumulatedOutput = nullptr;
    }
}

bool TerminalOutput::begin() {
    if (!_accumulatedOutput) {
        _accumulatedOutput = (char*)heap_caps_malloc(LIMITS::USB_TERMINAL::MAX_OUTPUT_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
        if (!_accumulatedOutput) {
             LOGE("Critical: Failed to allocate _accumulatedOutput in PSRAM!");
             return false;
        }
        _accumulatedOutput[0] = '\0';
    }
    return true;
}

void TerminalOutput::end() {
    _isCapturing = false;
    _outputLen = 0;
    _totalBytesCaptured = 0;
    _captureStartTime = 0;
    _lastFlushTime = 0;
    _targetId[0] = '\0';
    _ansiFilter.reset();
    // Keep the PSRAM buffer for the service lifetime so frequent terminal
    // sessions do not churn heap allocations on every start/stop cycle.
    if (_accumulatedOutput) {
        _accumulatedOutput[0] = '\0';
    }
}

void TerminalOutput::startCapture(const char* targetId) {
    _outputLen = 0;
    _totalBytesCaptured = 0;
    if (_accumulatedOutput) _accumulatedOutput[0] = '\0';
    
    _ansiFilter.reset();

    if (targetId) {
        strlcpy(_targetId, targetId, sizeof(_targetId));
    } else {
        _targetId[0] = '\0';
    }

    _isCapturing = true;
    _captureStartTime = millis();
    _lastFlushTime = millis();
}

void TerminalOutput::trimWhitespace() {
    if (!_accumulatedOutput) return;
    while (_outputLen > 0 && isspace(static_cast<unsigned char>(_accumulatedOutput[_outputLen - 1]))) {
        _outputLen--;
    }
    _accumulatedOutput[_outputLen] = '\0';
}

bool TerminalOutput::flushChunk(char*& outMsgBuf, char* outTargetId, bool clearBuffer) {
    outMsgBuf = nullptr;
    if (_targetId[0] == '\0' || !_accumulatedOutput) return false;

    trimWhitespace();
    if (_outputLen == 0) return false;

    outMsgBuf = (char*)heap_caps_malloc(_outputLen + 1, MALLOC_CAP_SPIRAM);
    if (outMsgBuf) {
        memcpy(outMsgBuf, _accumulatedOutput, _outputLen);
        outMsgBuf[_outputLen] = '\0';
        strlcpy(outTargetId, _targetId, LIMITS::USB_TERMINAL::MAX_TARGET_ID_LEN);
    } else {
        LOGE("OOM: Failed to allocate message dispatch chunk in PSRAM!");
    }
    
    if (clearBuffer) {
        _outputLen = 0;
        _accumulatedOutput[0] = '\0';
    }
    return outMsgBuf != nullptr;
}

FlushKind TerminalOutput::stopCapture(char*& outMsgBuf, char* outTargetId) {
    if (!_isCapturing) return FlushKind::None;
    _isCapturing = false;
    flushChunk(outMsgBuf, outTargetId, true);
    return FlushKind::Final;
}

FlushKind TerminalOutput::appendChar(char c, char*& outMsgBuf, char* outTargetId) {
    outMsgBuf = nullptr;
    if (!_isCapturing) return FlushKind::None;
    
    _totalBytesCaptured++;
    
    if (_totalBytesCaptured >= LIMITS::USB_TERMINAL::MAX_TOTAL_STREAM_BYTES) {
        _isCapturing = false;
        flushChunk(outMsgBuf, outTargetId, true);
        return FlushKind::Final;
    }
    
    if (!_ansiFilter.processChar(c)) {
        return FlushKind::None;
    }

    if (c >= 32 || c == '\n' || c == '\t' || c == '\r') {
        if (_outputLen < LIMITS::USB_TERMINAL::MAX_OUTPUT_BUFFER_SIZE - 1) {
            _accumulatedOutput[_outputLen++] = c;
            _accumulatedOutput[_outputLen] = '\0';
        }
        _captureStartTime = millis(); // Refresh timeout
    }
    
    if (_outputLen >= LIMITS::USB_TERMINAL::PARTIAL_FLUSH_THRESHOLD_BYTES) {
        bool dispatched = flushChunk(outMsgBuf, outTargetId, true);
        _lastFlushTime = millis();
        return dispatched ? FlushKind::Partial : FlushKind::None;
    }
    
    return FlushKind::None;
}

FlushKind TerminalOutput::checkTimeouts(uint32_t idleTimeoutMs, char*& outMsgBuf, char* outTargetId) {
    outMsgBuf = nullptr;
    if (!_isCapturing || !_accumulatedOutput) return FlushKind::None;
    
    uint32_t now = millis();
    
    if (now - _captureStartTime > idleTimeoutMs) {
        _isCapturing = false;
        flushChunk(outMsgBuf, outTargetId, true);
        return FlushKind::Final;
    }

    if (_outputLen > 0 && (now - _lastFlushTime > TIMEOUT::USB_TERMINAL::PERIODIC_FLUSH_INTERVAL_MS)) {
        bool dispatched = flushChunk(outMsgBuf, outTargetId, true);
        _lastFlushTime = now;
        return dispatched ? FlushKind::Partial : FlushKind::None;
    }

    return FlushKind::None;
}

bool TerminalOutput::gatherStatus(char*& outMsgBuf) {
    outMsgBuf = nullptr;
    
    if (_isCapturing && _outputLen > 0 && _accumulatedOutput) {
        size_t trimmedLen = _outputLen;
        while (trimmedLen > 0 && isspace(static_cast<unsigned char>(_accumulatedOutput[trimmedLen - 1]))) {
            trimmedLen--;
        }

        outMsgBuf = (char*)heap_caps_malloc(trimmedLen + 1, MALLOC_CAP_SPIRAM);
        if (outMsgBuf) {
            memcpy(outMsgBuf, _accumulatedOutput, trimmedLen);
            outMsgBuf[trimmedLen] = '\0';
            return true;
        }
    }
    return false;
}


} // namespace USB_TERMINAL

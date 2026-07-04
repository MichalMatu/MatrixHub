#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include "freertos/FreeRTOS.h"
#include "AnsiFilter.h"
#include "../UsbTerminalTypes.h"
#include "../../config/System.h"

namespace USB_TERMINAL {

/**
 * @class TerminalOutput
 * @brief Manages PSRAM-allocated buffer for gathering USB terminal responses.
 */
class TerminalOutput {
public:
    TerminalOutput();
    ~TerminalOutput();

    /**
     * @brief Allocates the long-lived PSRAM buffer. 
     * @return true if successful, false otherwise.
     */
    bool begin();

    /**
     * @brief Release the long-lived PSRAM buffer and reset capture state.
     */
    void end();

    /**
     * @brief Prepares the buffer for a new capture session.
     * @param targetId Session target identifier.
     */
    void startCapture(const char* targetId);

    /**
     * @brief Stops capturing and returns remaining output.
     * @param outMsgBuf Pointer to dynamically allocated PSRAM string buffer pointer. Caller MUST free.
     * @param outTargetId Pointer to a buffer to receive the target ID.
     * @return FlushKind::Final when the capture session has ended.
     */
    FlushKind stopCapture(char*& outMsgBuf, char* outTargetId);

    /**
     * @brief Adds a character to the buffer, checking limits and processing ANSI escapes.
     * @param c Character from serial.
     * @param outMsgBuf Pointer to dynamically allocated PSRAM string buffer pointer. Caller MUST free.
     * @param outTargetId Pointer to a buffer to receive the target ID.
     * @return FlushKind describing whether a partial or final chunk is ready.
     */
    FlushKind appendChar(char c, char*& outMsgBuf, char* outTargetId);

    /**
     * @brief Checks idle timeout and flushes if necessary. 
     * @param idleTimeoutMs Max duration without activity before auto-stop.
     * @param outMsgBuf Pointer to dynamically allocated PSRAM string buffer pointer. Caller MUST free.
     * @param outTargetId Pointer to a buffer to receive the target ID.
     * @return FlushKind describing whether a partial or final chunk is ready.
     */
    FlushKind checkTimeouts(uint32_t idleTimeoutMs, char*& outMsgBuf, char* outTargetId);

    /**
     * @brief Format status strings if user query status.
     * @param outMsgBuf Set to an unallocated pointer. The function allocates it on PSRAM.
     * @return true if it was allocated.
     */
    bool gatherStatus(char*& outMsgBuf);

    bool isCapturing() const { return _isCapturing; }
    
private:
    char _targetId[LIMITS::USB_TERMINAL::MAX_TARGET_ID_LEN];
    char* _accumulatedOutput;
    
    size_t _outputLen;
    size_t _totalBytesCaptured;
    unsigned long _captureStartTime;
    unsigned long _lastFlushTime;
    
    bool _isCapturing;
    AnsiFilter _ansiFilter;
    
    void trimWhitespace();
    bool flushChunk(char*& outMsgBuf, char* outTargetId, bool clearBuffer);
};

} // namespace USB_TERMINAL

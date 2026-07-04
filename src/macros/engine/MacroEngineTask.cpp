#include "MacroEngine.h"

#include <cstring>

#include "../../config/App.h"
#include "../../config/System.h"
#include "../../config/UIColors.h"
#include "../../system/logging/Logging.h"
#include "../../system/utils/ScopeLock.h"

#ifndef NATIVE_BUILD
#include "../../airmouse/AirMouseService.h"
#include "../../keyboard/KeyboardService.h"
#include "../../system/matrix_manager/MatrixManagerService.h"
#include "../../system/rtc/RtcConfig.h"
#endif

#undef LOG_TAG
#define LOG_TAG "MacroEngine"

namespace MACROS {

static size_t sanitizeMatrixText(char* dst, size_t dstSize, const char* src) {
    if (!dst || dstSize == 0) return 0;
    dst[0] = '\0';
    if (!src) return 0;

    const unsigned char* s = reinterpret_cast<const unsigned char*>(src);
    if (s[0] == 0xEF && s[1] == 0xBB && s[2] == 0xBF) {
        s += 3; // Skip UTF-8 BOM if present
    }

    size_t out = 0;
    while (*s && out < (dstSize - 1)) {
        unsigned char c = *s++;
        if (c >= 32 && c <= 126) {
            dst[out++] = static_cast<char>(c);
        } else if (c == '\t' || c == '\r' || c == '\n') {
            dst[out++] = ' ';
        } else {
            dst[out++] = ' ';
        }
    }

    dst[out] = '\0';
    return out;
}

void MacroEngine::taskFunction(void* parameter) {
    MacroEngine* self = static_cast<MacroEngine*>(parameter);

    // Use block to enforce destructor calls (MacroParser) BEFORE task deletion
    {
        self->executeLoop();
    }

    // Hand control back to stop(), which owns task deletion and task-memory cleanup.
    vTaskSuspend(NULL);
}

void MacroEngine::executeLoop() {
    MacroParser parser;
    MacroCommand currentCmd;
    MacroCommand lastCmd;
    uint32_t defaultDelay = 0;

    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (_stopSignal) {
            // If stopSignal is true at top level, it might mean STOP TASK or STOP SCRIPT.
            // But usually startScript resets _stopSignal.
            // If we are here, we were notified.
            // If stop() called us, _stopSignal is true.
            // We should check if we need to run a script.
            // We need to distinguish between "Stop Script" and "Stop Engine".
            // In MacroService implementation, _stopSignal was mostly for Stopping Script?
            // No, _stop() set _stopSignal = true.
            // But startScript set _stopSignal = false.
            // If stopScript() is called, it sets _stopSignal=true.
            // So if we wake up and _stopSignal is true, maybe we should just loop around or exit?
            // Actually, the outer loop `while(true)` is the engine loop.
            // If `stop()` is called, it sets `_stopSignal=true` and notifies.
            // We should provide a way to exit the task completely.
            // MacroService used `break` if `_stopSignal` was set at top of loop.
            // But `startScript` cleared it.
            // So if `stop()` sets it, we break.
            {
                SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(100));
                if (lock.isLocked()) {
                    _state.status = MacroStatus::IDLE;
                }
            }
            notifyState();
            break;
        }

        // Fetch script to run
        PsramString filename;
        {
            SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(500));
            if (lock.isLocked()) {
                filename = _state.currentScript;
            } else {
                continue;
            }
        }

        if (filename.empty()) continue; // Spurious wake?

        String fullPath = "/scripts/" + String(filename.c_str());

        // Read entire script file under fsMutex to prevent FS race conditions.
        // Scripts are small (< 4KB), so this is a brief lock (~1ms).
        PsramVector<uint8_t> scriptContent;
        bool readOk = false;
        {
            SYSTEM::ScopeLock fsLock(_fsMutex ? _fsMutex : nullptr, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
            bool locked = !_fsMutex || fsLock.isLocked();
            if (locked) {
                File f = LittleFS.open(fullPath.c_str(), "r");
                if (f) {
                    size_t sz = f.size();
                    if (sz > 0) {
                        try {
                            scriptContent.resize(sz);
                            f.read(scriptContent.data(), sz);
                            readOk = true;
                        } catch (...) {
                            LOGE("Failed to allocate PSRAM for script (%u bytes)", (unsigned int)sz);
                        }
                    }
                    f.close();
                }
            } else {
                LOGW("FS mutex timeout reading script: %s", filename.c_str());
            }
        }

        if (!readOk || !parser.beginFromContent(scriptContent.data(), scriptContent.size())) {
            // Error handle
            SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(500));
            if (lock.isLocked()) {
                _state.status = MacroStatus::ERROR;
                _state.lastError = parser.getError();
            }
            notifyState();
#ifndef NATIVE_BUILD
            if (_matrixManager) {
                MATRIX_MANAGER::LayerContent errContent;
                errContent.active = true;
                errContent.type = ::CommandType::SHOW_ICON;
                errContent.icon = IconType::ALARM_CRITICAL;
                errContent.durationMs = 2000;
                _matrixManager->setLayer(MATRIX_MANAGER::Layer::SYSTEM_MODAL, errContent);
            }
#endif
            continue;
        }

        LOGI("Running script: %s", filename.c_str());
#ifndef NATIVE_BUILD
        if (_matrixManager) {
            MATRIX_MANAGER::LayerContent runContent;
            runContent.active = true;
            runContent.type = ::CommandType::SHOW_TEXT;
            strlcpy(runContent.text, "RUN   ", sizeof(runContent.text));
            runContent.color = UI::COLOR::MACRO_RUN;
            runContent.durationMs = 3000;
            _matrixManager->setLayer(MATRIX_MANAGER::Layer::SYSTEM_MODAL, runContent);
        }
#endif

        unsigned long lastNotifyTime = 0;
        uint32_t executedCommands = 0;
        const uint32_t scriptStartMs = millis();
        PsramString abortError;

        while (!_stopSignal) {
            if (!parser.next(currentCmd)) break; // EOF

            if (currentCmd.type == CommandType::UNKNOWN) {
                abortError = currentCmd.textData.empty() ? "Invalid macro command" : currentCmd.textData;
                LOGW("Aborting macro due to invalid command at line %u: %s",
                     (unsigned)parser.getLineNumber(),
                     abortError.c_str());
                break;
            }

            if (currentCmd.type != CommandType::REM) {
                executedCommands++;
                if (executionLimitExceeded(scriptStartMs, executedCommands, abortError)) {
                    LOGW("Aborting macro at line %u: %s",
                         (unsigned)parser.getLineNumber(),
                         abortError.c_str());
                    break;
                }
                {
                    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(10));
                    if (lock.isLocked()) {
                        _state.currentLine = parser.getLineNumber();
                    }
                }
                if (millis() - lastNotifyTime >= 100) {
                    notifyState();
                    lastNotifyTime = millis();
                }
            }

            processCommand(currentCmd, lastCmd, defaultDelay);

            if (currentCmd.type == CommandType::REPEAT) {
                for (uint32_t i = 0; i < currentCmd.numericData; i++) {
                    if (_stopSignal) break;
                    executedCommands++;
                    if (executionLimitExceeded(scriptStartMs, executedCommands, abortError)) {
                        LOGW("Aborting macro repeat at line %u: %s",
                             (unsigned)parser.getLineNumber(),
                             abortError.c_str());
                        break;
                    }
                    processCommand(lastCmd, lastCmd, defaultDelay);
                    if (defaultDelay > 0) {
                        performDelay(defaultDelay);
                    }
                    vTaskDelay(pdMS_TO_TICKS(1));
                }
                if (!abortError.empty()) {
                    break;
                }
            } else {
                if (currentCmd.type != CommandType::REM && currentCmd.type != CommandType::DEFAULT_DELAY) {
                    lastCmd = currentCmd;
                    if (defaultDelay > 0) {
                        performDelay(defaultDelay);
                    }
                }
            }
            vTaskDelay(pdMS_TO_TICKS(1));
        }

        parser.end();
        if (_keyboardService) _keyboardService->releaseAll();

        // Log peak stack usage after script execution (when stack was deepest)
        LOG_STACK_SIZE(CONFIG::TASKS::STACK_MACRO);

        /*
           Logic Check: If we stopped due to _stopSignal, was it stopScript() or stop()?
           If stop(), we want to break outer loop.
           If stopScript(), we want to go back to wait.
           How to distinguish?
           MacroService didn't distinguish explicitly, but startScript cleared flag.
           If stop() is called, it sets flag and waits for task deletion.
           If we break inner loop, we hit bottom of outer loop.
           Then we loop back to `ulTaskNotifyTake`.
           If stop() was called, `_stopSignal` is true.
           So next `ulTaskNotifyTake` returns (notify called by stop())?
           Yes, stop() calls notify.
           So we wake up. `if (_stopSignal) break;`. Task ends.
           Correct.

           BUT, if stopScript() was called, `_stopSignal` is true.
           We break inner loop.
           We update status to IDLE.
           We loop back to `ulTaskNotifyTake`.
           Does stopScript() notify? No.
           So `ulTaskNotifyTake` blocks.
           But `_stopSignal` is TRUE.
           If startScript() is called later, it sets `_stopSignal` to FALSE and notifies.
           We wake up. `if (_stopSignal)` is FALSE. We proceed.
           So correct.
        */

        {
            SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(500));
            if (lock.isLocked()) {
                if (!abortError.empty()) {
                    _state.status = MacroStatus::ERROR;
                    _state.lastError = abortError;
                } else {
                    _state.status = (_stopSignal) ? MacroStatus::IDLE : MacroStatus::COMPLETED;
                    if (!_stopSignal) {
                        _state.lastError.clear();
                    }
                }
            }
        }
        notifyState();

        if (!_stopSignal) LOGI("Script finished");
        else LOGI("Script/Engine stopped");
    }
}

void MacroEngine::performDelay(uint32_t ms) {
    uint32_t remaining = ms;
    while (remaining > 0 && !_stopSignal) {
        uint32_t step = (remaining > 100) ? 100 : remaining;
        vTaskDelay(pdMS_TO_TICKS(step));
        remaining -= step;
    }
}

void MacroEngine::processCommand(const MacroCommand& cmd, MacroCommand& lastCmd, uint32_t& defaultDelay) {
    // Copy-paste content from MacroService::processCommand
    switch (cmd.type) {
        case CommandType::DELAY:
            performDelay(cmd.numericData);
            break;
        case CommandType::STRING:
            if (_keyboardService && _keyboardService->isInitialized()) {
                _keyboardService->type(cmd.textData.c_str());
            } else {
                LOGW("Keyboard not initialized - STRING command ignored");
            }
            break;
        case CommandType::STRINGLN:
            if (_keyboardService && _keyboardService->isInitialized()) {
                _keyboardService->typeLn(cmd.textData.c_str());
            } else {
                LOGW("Keyboard not initialized - STRINGLN command ignored");
            }
            break;
        case CommandType::KEY:
            if (_keyboardService) {
                _keyboardService->press(cmd.key);
                vTaskDelay(pdMS_TO_TICKS(20));
                _keyboardService->releaseAll();
            }
            break;
        case CommandType::COMBO:
            {
                // Decode modifier bitmask into actual key codes
                uint8_t keys[5]; // Max: 4 modifiers + 1 key
                size_t count = 0;

                if (cmd.modifiers & MOD_CTRL)  keys[count++] = KEY_LEFT_CTRL;
                if (cmd.modifiers & MOD_SHIFT) keys[count++] = KEY_LEFT_SHIFT;
                if (cmd.modifiers & MOD_ALT)   keys[count++] = KEY_LEFT_ALT;
                if (cmd.modifiers & MOD_GUI)   keys[count++] = KEY_LEFT_GUI;

                if (cmd.key != 0) keys[count++] = cmd.key;

                if (count > 0 && _keyboardService) {
                    _keyboardService->pressCombo(keys, count);
                }
            }
            break;
        case CommandType::DEFAULT_DELAY:
            defaultDelay = cmd.numericData;
            break;
        case CommandType::MOUSE_MOVE:
            {
                int16_t x = (int16_t)(cmd.numericData >> 16);
                int16_t y = (int16_t)(cmd.numericData & 0xFFFF);
                if (_airMouseService) {
                    _airMouseService->move(x, y);
                } else {
                    LOGW("AirMouseService not injected - MOUSE_MOVE command ignored");
                }
            }
            break;
        case CommandType::MOUSE_CLICK:
            if (_airMouseService) {
                _airMouseService->click(cmd.key);
            } else {
                LOGW("AirMouseService not injected - MOUSE_CLICK command ignored");
            }
            break;
        case CommandType::MATRIX_PRINT:
            {
#ifndef NATIVE_BUILD
                uint32_t duration = 0;
                if (_matrixManager) {
                    // Calculate duration for one full scroll using current scroll speed.
                    // Matrix width is 8 pixels. MatrixRenderer adds 4 spaces of padding (2 front, 2 back).
                    // Total pixels to scroll = right edge start (8) + padded text width ((length + 4) * FONT_WIDTH_PX)
                    // Time = Total pixels * scrollSpeedMs
                    uint16_t scrollSpeedMs = UI::MATRIX::SCROLL_INTERVAL_MS;
                    uint16_t cfgSpeed = RTC::getConfig().matrix.menu.scrollSpeed;
                    if (cfgSpeed >= 20 && cfgSpeed <= 120) {
                        scrollSpeedMs = cfgSpeed;
                    }

                    MATRIX_MANAGER::LayerContent printContent;
                    printContent.active = true;
                    printContent.type = ::CommandType::SHOW_TEXT;
                    size_t len = sanitizeMatrixText(printContent.text, sizeof(printContent.text), cmd.textData.c_str());
                    constexpr size_t kTailPadding = 3;
                    for (size_t i = 0; i < kTailPadding && len < (sizeof(printContent.text) - 1); ++i) {
                        printContent.text[len++] = ' ';
                    }
                    printContent.text[len] = '\0';

                    size_t displayLen = len;
                    const uint32_t kPerCharMs = UI::MATRIX::FONT_WIDTH_PX * scrollSpeedMs;
                    const uint32_t kBaseScrollMs = 8 * scrollSpeedMs + 4 * kPerCharMs;
                    duration = kBaseScrollMs + (displayLen * kPerCharMs);

                    printContent.color = UI::COLOR::MACRO_PRINT;
                    printContent.durationMs = duration;
                    _matrixManager->setLayer(MATRIX_MANAGER::Layer::SYSTEM_MODAL, printContent);
                }

                // Yield to MatrixTask to process command before continuing
                vTaskDelay(pdMS_TO_TICKS(50));

                // Use performDelay for waiting while checking stopSignal
                performDelay(duration);
#else
                LOGW("MATRIX_PRINT ignored in native build");
#endif
            }
            break;
        case CommandType::SYSTEM_CONTROL:
            if (_keyboardService) _keyboardService->pressSystem(cmd.key);
            break;
        case CommandType::CONSUMER:
            if (_keyboardService) _keyboardService->pressConsumer(cmd.key);
            break;
        case CommandType::PRESS_KEY:
            if (_keyboardService) {
                _keyboardService->pressKey(cmd.key);
            }
            break;
        case CommandType::RELEASE_KEY:
            if (_keyboardService) {
                _keyboardService->releaseKey(cmd.key);
            }
            break;
        case CommandType::RELEASE_ALL:
            if (_keyboardService) {
                _keyboardService->releaseAll();
            }
            break;
        default:
            break;
    }
}

} // namespace MACROS

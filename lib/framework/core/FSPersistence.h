#ifndef FSPersistence_h
#define FSPersistence_h

/**
 *   ESP32 SvelteKit
 *
 *   A simple, secure and extensible framework for IoT projects for ESP32 platforms
 *   with responsive Sveltekit front-end built with TailwindCSS and DaisyUI.
 *   https://github.com/theelims/ESP32-sveltekit
 *
 *   Copyright (C) 2018 - 2023 rjwats
 *   Copyright (C) 2023 - 2025 theelims
 *
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 **/

#include <core/StatefulService.h>
#include <FS.h>
#include <ArduinoJson.h>
#include "../../../src/core/config/ConfigCommon.h"
#include "../../../src/system/utils/ScopeLock.h"

template <class T>
class FSPersistence
{
public:
    FSPersistence(JsonStateReader<T> stateReader,
                  JsonStateUpdater<T> stateUpdater,
                  IStatefulService<T> *statefulService,
                  FS *fs,
                  const char *filePath) : _stateReader(stateReader),
                                          _stateUpdater(stateUpdater),
                                          _statefulService(statefulService),
                                          _fs(fs),
                                          _filePath(filePath),
                                          _updateHandlerId(0)
    {
        enableUpdateHandler();
    }

    void readFromFS()
    {
        if (!_fs || !_filePath || !_statefulService) {
            applyDefaults();
            return;
        }

        JsonDocument jsonDocument;
        bool fileExists = false;
        bool parsed = false;
        bool readFailed = false;

        {
            SYSTEM::ScopeLock fsLock(g_fsMutex, pdMS_TO_TICKS(500));
            if (g_fsMutex && !fsLock.isLocked()) {
                applyDefaults();
                return;
            }

            File settingsFile = _fs->open(_filePath, "r");
            if (settingsFile)
            {
                fileExists = true;
                DeserializationError error = deserializeJson(jsonDocument, settingsFile);
                parsed = (error == DeserializationError::Ok && jsonDocument.is<JsonObject>());
                settingsFile.close();
            }
            else
            {
                fileExists = _fs->exists(_filePath);
                if (fileExists) {
                    readFailed = true;
                }
            }
        }

        if (readFailed) {
            applyDefaults();
            return;
        }

        if (fileExists && parsed)
        {
            JsonObject jsonObject = jsonDocument.as<JsonObject>();
            const StateUpdateResult updateResult =
                _statefulService->updateWithoutPropagation(jsonObject, _stateUpdater, _filePath);
            if (updateResult != StateUpdateResult::ERROR) {
                return;
            }
        }

        applyDefaults();
        if (!fileExists) {
            writeToFS();
        }
    }

    bool writeToFS()
    {
        if (!_fs || !_filePath || !_statefulService) {
            return false;
        }

        // create and populate a new json object
        JsonDocument jsonDocument;
        JsonObject jsonObject = jsonDocument.to<JsonObject>();
        const StateHandlerResult readResult = _statefulService->read(jsonObject, _stateReader);
        if (!readResult.ok) {
            return false;
        }

        const String filePath(_filePath);
        const String tmpPath = filePath + ".tmp";
        const String bakPath = filePath + ".bak";

        SYSTEM::ScopeLock fsLock(g_fsMutex, pdMS_TO_TICKS(500));
        if (g_fsMutex && !fsLock.isLocked()) {
            return false;
        }

        // make directories if required
        mkdirs();

        File settingsFile = _fs->open(tmpPath.c_str(), "w");
        if (!settingsFile) {
            return false;
        }

        const size_t written = serializeJson(jsonDocument, settingsFile);
        settingsFile.close();
        if (written == 0 || jsonDocument.overflowed()) {
            _fs->remove(tmpPath.c_str());
            return false;
        }

        if (_fs->exists(bakPath.c_str())) {
            _fs->remove(bakPath.c_str());
        }

        if (_fs->exists(filePath.c_str()) && !_fs->rename(filePath.c_str(), bakPath.c_str())) {
            _fs->remove(tmpPath.c_str());
            return false;
        }

        if (!_fs->rename(tmpPath.c_str(), filePath.c_str())) {
            if (_fs->exists(bakPath.c_str())) {
                _fs->rename(bakPath.c_str(), filePath.c_str());
            }
            _fs->remove(tmpPath.c_str());
            return false;
        }

        if (_fs->exists(bakPath.c_str())) {
            _fs->remove(bakPath.c_str());
        }

        return true;
    }

    void disableUpdateHandler()
    {
        if (_updateHandlerId)
        {
            _statefulService->removeUpdateHandler(_updateHandlerId);
            _updateHandlerId = 0;
        }
    }

    void enableUpdateHandler()
    {
        if (!_updateHandlerId)
        {
            _updateHandlerId = _statefulService->addUpdateHandler([&](std::string_view originId) {
                (void)originId;
                return writeToFS()
                    ? StateHandlerResult::success()
                    : StateHandlerResult::failure("config/save_failed");
            });
        }
    }

private:
    JsonStateReader<T> _stateReader;
    JsonStateUpdater<T> _stateUpdater;
    IStatefulService<T> *_statefulService;
    FS *_fs;
    const char *_filePath;
    update_handler_id_t _updateHandlerId;

    // We assume we have a _filePath with format "/directory1/directory2/filename"
    // We create a directory for each missing parent
    void mkdirs()
    {
        if (!_fs || !_filePath) return;

        String path(_filePath);
        int index = 0;
        while ((index = path.indexOf('/', index + 1)) != -1)
        {
            String segment = path.substring(0, index);
            if (segment.length() <= 1) continue; // Skip empty or root "/"

            // Optimization: Just try to create. 
            // Checking exists() first can cause Panic on freshly formatted LittleFS (Core 0 Illegal Instruction)
            // due to lfs_dir_fetchmatch failure on empty structures.
            _fs->mkdir(segment);
        }
    }

protected:
    // We assume the updater supplies sensible defaults if an empty object
    // is supplied, this virtual function allows that to be changed.
    virtual void applyDefaults()
    {
        JsonDocument jsonDocument;
        JsonObject jsonObject = jsonDocument.to<JsonObject>();
        _statefulService->updateWithoutPropagation(jsonObject, _stateUpdater, _filePath);
    }
};

#endif // end FSPersistence

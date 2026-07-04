#pragma once

#include "FS.h"
#include "PsychicRequest.h"
#include "PsychicHttpServer.h"
#include "PsychicJson.h"

class PsychicResponse {
public:
    explicit PsychicResponse(PsychicRequest* request)
        : _request(request) {}

    void setCode(int code) {
        _code = code;
    }

    void setContentType(const char* contentType) {
        _contentType = contentType ? contentType : "";
    }

    void addHeader(const char* field, const char* value) {
        headers.emplace_back(field ? field : "", value ? value : "");
    }

    void setContent(const char* content) {
        _content = content ? content : "";
    }

    void setContent(const uint8_t* content, size_t len) {
        _content.assign(reinterpret_cast<const char*>(content), len);
    }

    esp_err_t send() {
        if (_request) {
            _request->lastStatusCode = _code;
            _request->lastResponseBody = _content;
        }
        return ESP_OK;
    }

    std::vector<std::pair<std::string, std::string>> headers;

protected:
    PsychicRequest* _request = nullptr;
    int _code = 200;
    std::string _contentType;
    std::string _content;
};

class PsychicFileResponse : public PsychicResponse {
public:
    PsychicFileResponse(PsychicRequest* request,
                        fs::FS& fs,
                        const String& path,
                        const String& contentType = String(),
                        bool download = false)
        : PsychicResponse(request) {
        (void)fs;
        (void)path;
        (void)download;
        if (!contentType.empty()) {
            setContentType(contentType.c_str());
        }
    }
};

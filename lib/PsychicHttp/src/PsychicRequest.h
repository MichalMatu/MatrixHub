#ifndef PsychicRequest_h
#define PsychicRequest_h

#include "PsychicCore.h"
#include "PsychicHttpServer.h"
#include "PsychicClient.h"
#include "PsychicWebParameter.h"
#include "PsychicResponse.h"
#include "PsychicContentDisposition.h"

// ESP32-S3 optimized: Fixed-size session storage instead of std::map<String, String>
// This eliminates heap fragmentation from map node allocations + String keys
constexpr size_t MAX_SESSION_KEYS = 8;  // Increased to 8 to safely hold all Digest parameters

struct SessionEntry {
    char key[24];    // Fixed-size key (enough for "realm", "nonce", "opaque")
    char value[47];  // Adjusted to 47 to align struct to 72 bytes (24+47+1)
    bool used;
};

struct SessionData {
    SessionEntry entries[MAX_SESSION_KEYS];
    
    SessionData() {
        for (size_t i = 0; i < MAX_SESSION_KEYS; i++) {
            entries[i].used = false;
            entries[i].key[0] = '\0';
            entries[i].value[0] = '\0';
        }
    }
    
    // Iterator-like interface for compatibility
    SessionEntry* find(const String& key) {
        for (size_t i = 0; i < MAX_SESSION_KEYS; i++) {
            if (entries[i].used && strcmp(entries[i].key, key.c_str()) == 0) {
                return &entries[i];
            }
        }
        return nullptr;
    }
    
    SessionEntry* end() { return nullptr; }
    
    bool insert(const String& key, const String& value) {
        // First check if key exists
        for (size_t i = 0; i < MAX_SESSION_KEYS; i++) {
            if (entries[i].used && strcmp(entries[i].key, key.c_str()) == 0) {
                strncpy(entries[i].value, value.c_str(), sizeof(entries[i].value) - 1);
                entries[i].value[sizeof(entries[i].value) - 1] = '\0';
                return true;
            }
        }
        // Find empty slot
        for (size_t i = 0; i < MAX_SESSION_KEYS; i++) {
            if (!entries[i].used) {
                strncpy(entries[i].key, key.c_str(), sizeof(entries[i].key) - 1);
                entries[i].key[sizeof(entries[i].key) - 1] = '\0';
                strncpy(entries[i].value, value.c_str(), sizeof(entries[i].value) - 1);
                entries[i].value[sizeof(entries[i].value) - 1] = '\0';
                entries[i].used = true;
                return true;
            }
        }
        return false; // No space
    }
};

class PsychicRequest {
  friend PsychicHttpServer;

  protected:
    PsychicHttpServer *_server;
    httpd_req_t *_req;
    SessionData *_session;
    PsychicClient *_client;

    http_method _method;
    String _uri;
    String _query;
    char* _body = nullptr;
    size_t _bodyLen = 0;
    bool _paramsLoaded = false;
    bool _responseSent = false;

    std::list<PsychicWebParameter*> _params;

    void _addParams(const String& params, bool post);
    void _parseGETParams();
    void _parsePOSTParams();

    const String _extractParam(const String& authReq, const String& param, const char delimit);
    const String _getRandomHexString();

  public:
    PsychicRequest(PsychicHttpServer *server, httpd_req_t *req);
    virtual ~PsychicRequest();

    void *_tempObject;

    PsychicHttpServer * server();
    httpd_req_t * request();
    virtual PsychicClient * client();

    bool isMultipart();
    esp_err_t loadBody();

    const String header(const char *name);
    bool hasHeader(const char *name);

    static void freeSession(void *ctx);
    bool hasSessionKey(const String& key);
    const String getSessionKey(const String& key);
    void setSessionKey(const String& key, const String& value);

    bool hasCookie(const char * key);
    const String getCookie(const char * key);

    http_method method();       // returns the HTTP method used as enum value (eg. HTTP_GET)
    const String methodStr();   // returns the HTTP method used as a string (eg. "GET")
    const String path();        // returns the request path (eg /page?foo=bar returns "/page")
    const String& uri();        // returns the full request uri (eg /page?foo=bar)
    const String& query();      // returns the request query data (eg /page?foo=bar returns "foo=bar")
    const String host();        // returns the requested host (request to http://psychic.local/foo will return "psychic.local")
    const String contentType(); // returns the Content-Type header value
    size_t contentLength();     // returns the Content-Length header value
    const char* body() const;       // returns the body of the request
    size_t bodyLength() const;     // returns the length of the body
    const ContentDisposition getContentDisposition();

    const String& queryString() { return query(); }  //compatability function.  same as query()
    const String& url() { return uri(); }            //compatability function.  same as uri()

    void loadParams();
    PsychicWebParameter * addParam(PsychicWebParameter *param);
    PsychicWebParameter * addParam(const String &name, const String &value, bool decode = true, bool post = false);
    bool hasParam(const char *key);
    PsychicWebParameter * getParam(const char *name);
    void markResponseSent() { _responseSent = true; }
    bool responseSent() const { return _responseSent; }

    const String getFilename();

    bool authenticate(const char * username, const char * password);
    esp_err_t requestAuthentication(HTTPAuthMethod mode, const char* realm, const char* authFailMsg);

    esp_err_t redirect(const char *url);
    esp_err_t reply(int code);
    esp_err_t reply(const char *content);
    esp_err_t reply(int code, const char *contentType, const char *content);
};

#endif // PsychicRequest_h

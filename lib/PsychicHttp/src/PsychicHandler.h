#ifndef PsychicHandler_h
#define PsychicHandler_h

#include "PsychicCore.h"
#include "PsychicRequest.h"

class PsychicEndpoint;
class PsychicHttpServer;

/*
* HANDLER :: Can be attached to any endpoint or as a generic request handler.
*/

class PsychicHandler {
  friend PsychicEndpoint;

  protected:
    PsychicRequestFilterFunction _filter;
    PsychicHttpServer *_server;

    // ESP32-C6 optimized: Use const char* instead of String to avoid heap allocations
    // Assumes credentials are passed as string literals or long-lived strings
    const char* _username;
    const char* _password;
    HTTPAuthMethod _method;
    const char* _realm;
    const char* _authFailMsg;

    std::list<PsychicClient*> _clients;

  public:
    PsychicHandler();
    virtual ~PsychicHandler();

    PsychicHandler* setFilter(PsychicRequestFilterFunction fn);
    bool filter(PsychicRequest *request);

    PsychicHandler* setAuthentication(const char *username, const char *password, HTTPAuthMethod method = BASIC_AUTH, const char *realm = "", const char *authFailMsg = "");
    bool needsAuthentication(PsychicRequest *request);
    esp_err_t authenticate(PsychicRequest *request);

    PsychicClient * checkForNewClient(PsychicClient *client);
    void checkForClosedClient(PsychicClient *client);

    virtual void addClient(PsychicClient *client);
    virtual void removeClient(PsychicClient *client);
    virtual PsychicClient * getClient(int socket);
    virtual PsychicClient * getClient(PsychicClient *client);
    virtual void openCallback(PsychicClient *client) {};
    virtual void closeCallback(PsychicClient *client) {};

    bool hasClient(PsychicClient *client);
    int count() { return _clients.size(); };
    
    // Safe iteration over clients
    void applyToAllClients(std::function<void(PsychicClient*)> fn);

    //derived classes must implement these functions
    virtual bool canHandle(PsychicRequest *request) { return true; };
    virtual esp_err_t handleRequest(PsychicRequest *request) = 0;
};

#endif
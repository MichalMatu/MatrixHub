#include "PsychicHandler.h"

PsychicHandler::PsychicHandler() :
  _filter(NULL),
  _server(NULL),
  _username(nullptr),
  _password(nullptr),
  _method(DIGEST_AUTH),
  _realm(""),
  _authFailMsg("")
  {
  }

PsychicHandler::~PsychicHandler() {
  // actual PsychicClient deletion handled by PsychicServer
  // for (PsychicClient *client : _clients)
  //   delete(client);
  _clients.clear();
}

PsychicHandler* PsychicHandler::setFilter(PsychicRequestFilterFunction fn) {
  _filter = fn;
  return this;
}

bool PsychicHandler::filter(PsychicRequest *request){
  return _filter == NULL || _filter(request);
}

PsychicHandler* PsychicHandler::setAuthentication(const char *username, const char *password, HTTPAuthMethod method, const char *realm, const char *authFailMsg) {
  // ESP32-C6 optimized: Store pointers directly instead of copying to String
  // Caller must ensure these strings outlive the handler
  _username = username;
  _password = password;
  _method = method;
  _realm = realm;
  _authFailMsg = authFailMsg;
  return this;
}

bool PsychicHandler::needsAuthentication(PsychicRequest *request) {
  return (_username != nullptr && _password != nullptr && 
          _username[0] != '\0' && _password[0] != '\0') && 
          !request->authenticate(_username, _password);
}

esp_err_t PsychicHandler::authenticate(PsychicRequest *request) {
  return request->requestAuthentication(_method, _realm ? _realm : "", _authFailMsg ? _authFailMsg : "");
}

PsychicClient * PsychicHandler::checkForNewClient(PsychicClient *client)
{
  PsychicClient *c = PsychicHandler::getClient(client);
  if (c == NULL)
  {
    c = client;
    addClient(c);
    c->isNew = true;
  }
  else
    c->isNew = false;

  return c;
}

void PsychicHandler::checkForClosedClient(PsychicClient *client)
{
  if (hasClient(client))
  {
    closeCallback(client);
    removeClient(client);
  }
}

void PsychicHandler::addClient(PsychicClient *client) {
  if (_server && xSemaphoreTake(_server->_clientMutex, portMAX_DELAY)) {
    _clients.push_back(client);
    xSemaphoreGive(_server->_clientMutex);
  }
}

void PsychicHandler::removeClient(PsychicClient *client) {
  if (_server && xSemaphoreTake(_server->_clientMutex, portMAX_DELAY)) {
    _clients.remove(client);
    xSemaphoreGive(_server->_clientMutex);
  }
}

PsychicClient * PsychicHandler::getClient(int socket)
{
  //make sure the server has it too.
  if (!_server || !_server->hasClient(socket))
    return NULL;

  PsychicClient* found = NULL;
  if (xSemaphoreTake(_server->_clientMutex, portMAX_DELAY)) {
    for (PsychicClient *client : _clients) {
      if (client->socket() == socket) {
        found = client;
        break;
      }
    }
    xSemaphoreGive(_server->_clientMutex);
  }

  return found;
}

PsychicClient * PsychicHandler::getClient(PsychicClient *client) {
  return PsychicHandler::getClient(client->socket());
}

bool PsychicHandler::hasClient(PsychicClient *socket) {
  return PsychicHandler::getClient(socket) != NULL;
}

void PsychicHandler::applyToAllClients(std::function<void(PsychicClient*)> fn) {
  if (_server && xSemaphoreTake(_server->_clientMutex, portMAX_DELAY)) {
    for (auto *client : _clients) {
      fn(client);
    }
    xSemaphoreGive(_server->_clientMutex);
  }
}
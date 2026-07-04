#include "PsychicJson.h"

#ifdef ARDUINOJSON_6_COMPATIBILITY
  PsychicJsonResponse::PsychicJsonResponse(PsychicRequest *request, bool isArray, size_t maxJsonBufferSize) :
    PsychicResponse(request),
    _jsonBuffer(maxJsonBufferSize)
  {
    setContentType(JSON_MIMETYPE);
    if (isArray)
      _root = _jsonBuffer.createNestedArray();
    else
      _root = _jsonBuffer.createNestedObject();
  }
#else
  PsychicJsonResponse::PsychicJsonResponse(PsychicRequest *request, bool isArray) : PsychicResponse(request)
  {
    setContentType(JSON_MIMETYPE);
    if (isArray)
      _root = _jsonBuffer.add<JsonArray>();
    else
      _root = _jsonBuffer.add<JsonObject>();
  }
#endif

JsonVariant &PsychicJsonResponse::getRoot() { return _root; }

size_t PsychicJsonResponse::getLength()
{
  return measureJson(_root);
}

esp_err_t PsychicJsonResponse::send()
{
  esp_err_t err = ESP_OK;
  size_t length = getLength();
  // ESP32-C6 optimization: avoid heap malloc/free on every JSON response.
  // Using a fixed stack buffer drastically reduces heap fragmentation under frequent polling.
  // BUT: Stack is small (8KB). Allocating 2KB-4KB on stack is dangerous. Move to Heap/PSRAM.
  
  // Use unique_ptr with custom deleter for PSRAM allocation
  char* raw_ptr = (char*)heap_caps_malloc(JSON_BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!raw_ptr) {
     ESP_LOGE(PH_TAG, "Failed to allocate JSON buffer in PSRAM");
     return ESP_ERR_NO_MEM;
  }
  std::unique_ptr<char, void(*)(void*)> buffer(raw_ptr, heap_caps_free);
  
  const size_t buffer_size = (length < (JSON_BUFFER_SIZE - 1)) ? (length + 1) : JSON_BUFFER_SIZE;

  //send it in one shot or no?
  if (length < JSON_BUFFER_SIZE)
  {
    serializeJson(_root, buffer.get(), buffer_size);

    this->setContent((uint8_t *)buffer.get(), length);
    this->setContentType(JSON_MIMETYPE);

    err = PsychicResponse::send();
  }
  else
  {
    //helper class that acts as a stream to print chunked responses
    ChunkPrinter dest(this, (uint8_t *)buffer.get(), JSON_BUFFER_SIZE);

    //keep our headers
    this->sendHeaders();

    serializeJson(_root, dest);

    //send the last bits
    dest.flush();

    //done with our chunked response too
    err = this->finishChunking();
  }

  return err;
}

#ifdef ARDUINOJSON_6_COMPATIBILITY
  PsychicJsonHandler::PsychicJsonHandler(size_t maxJsonBufferSize) :
    _onRequest(NULL),
    _maxJsonBufferSize(maxJsonBufferSize)
  {};

  PsychicJsonHandler::PsychicJsonHandler(PsychicJsonRequestCallback onRequest, size_t maxJsonBufferSize) :
    _onRequest(onRequest),
    _maxJsonBufferSize(maxJsonBufferSize)
  {}
#else
  PsychicJsonHandler::PsychicJsonHandler() :
    _onRequest(NULL)
  {};

  PsychicJsonHandler::PsychicJsonHandler(PsychicJsonRequestCallback onRequest) :
    _onRequest(onRequest)
  {}
#endif

void PsychicJsonHandler::onRequest(PsychicJsonRequestCallback fn) { _onRequest = fn; }

esp_err_t PsychicJsonHandler::handleRequest(PsychicRequest *request)
{
  //process basic stuff
  PsychicWebHandler::handleRequest(request);

  if (_onRequest)
  {
    #ifdef ARDUINOJSON_6_COMPATIBILITY
      DynamicJsonDocument jsonBuffer(this->_maxJsonBufferSize);
      DeserializationError error = deserializeJson(jsonBuffer, request->body());
      if (error)
        return request->reply(400);

      JsonVariant json = jsonBuffer.as<JsonVariant>();
    #else
      JsonDocument jsonBuffer;
      DeserializationError error = deserializeJson(jsonBuffer, request->body());
      if (error)
        return request->reply(400);

      JsonVariant json = jsonBuffer.as<JsonVariant>();
    #endif

    return _onRequest(request, json);
  }
  else
    return request->reply(500);
}
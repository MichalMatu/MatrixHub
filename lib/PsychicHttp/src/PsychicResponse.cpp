#include "PsychicResponse.h"
#include "PsychicRequest.h"
#include <http_status.h>
#include <cstring>

namespace {

bool hasExplicitHeader(const FixedHeader* headers, size_t count, const char* field)
{
  for (size_t i = 0; i < count; i++) {
    if (headers[i].used && strcmp(headers[i].field, field) == 0) {
      return true;
    }
  }
  return false;
}

bool shouldDisableCachingForPath(const String& path)
{
  return path.startsWith("/api/") || path.startsWith("/rest/");
}

} // namespace

PsychicResponse::PsychicResponse(PsychicRequest *request) :
  _request(request),
  _code(200),
  _status(""),
  _headerCount(0),
  _contentLength(0),
  _body("")
{
  // Initialize all headers as unused
  for (size_t i = 0; i < MAX_RESPONSE_HEADERS; i++) {
    _headers[i].used = false;
    _headers[i].field[0] = '\0';
    _headers[i].value[0] = '\0';
  }
}

PsychicResponse::~PsychicResponse()
{
  // ESP32-S3 optimized: No heap cleanup needed - headers are stack/object allocated
  // Nothing to free - fixed-size arrays are part of the object
}

void PsychicResponse::addHeader(const char *field, const char *value)
{
  // ESP32-S3 optimized: Use fixed-size slots instead of malloc
  if (_headerCount >= MAX_RESPONSE_HEADERS) {
    ESP_LOGW(PH_TAG, "Max headers reached, ignoring: %s", field);
    return;
  }
  
  FixedHeader& header = _headers[_headerCount];
  strncpy(header.field, field, MAX_HEADER_FIELD_LEN - 1);
  header.field[MAX_HEADER_FIELD_LEN - 1] = '\0';
  strncpy(header.value, value, MAX_HEADER_VALUE_LEN - 1);
  header.value[MAX_HEADER_VALUE_LEN - 1] = '\0';
  header.used = true;
  _headerCount++;
}

void PsychicResponse::setCookie(const char *name, const char *value, unsigned long secondsFromNow, const char *extras)
{
  time_t now = time(nullptr);
  
  // Calculate size to reserve and avoid fragmentation
  // name=value; Max-Age=1234567890; \0
  // + extra buffer for safety
  size_t estimatedSize = strlen(name) + strlen(value) + 40; 
  if (extras) estimatedSize += strlen(extras) + 2;

  String output;
  output.reserve(estimatedSize);
  
  output += urlEncode(name);
  output += "=";
  output += urlEncode(value);

  //if current time isn't modern, default to using max age
  if (now < 1700000000) {
    output += "; Max-Age=";
    output += String(secondsFromNow);
  }
  //otherwise, set an expiration date
  else
  {
    time_t expirationTimestamp = now + secondsFromNow;

    // Convert the expiration timestamp to a formatted string for the "expires" attribute
    struct tm* tmInfo = gmtime(&expirationTimestamp);
    char expires[32];
    strftime(expires, sizeof(expires), "%a, %d %b %Y %H:%M:%S GMT", tmInfo);
    output += "; Expires=";
    output += expires;
  }

  //did we get any extras?
  if (extras && extras[0] != '\0') {
    output += "; ";
    output += extras;
  }

  //okay, add it in.
  addHeader("Set-Cookie", output.c_str());
}

void PsychicResponse::setCode(int code)
{
  _code = code;
}

void PsychicResponse::setContentType(const char *contentType)
{
  httpd_resp_set_type(_request->request(), contentType);
}

void PsychicResponse::setContent(const char *content)
{
  _body = content;
  setContentLength(strlen(content));
}

void PsychicResponse::setContent(const uint8_t *content, size_t len)
{
  _body = (char *)content;
  setContentLength(len);
}

const char * PsychicResponse::getContent()
{
  return _body;
}

size_t PsychicResponse::getContentLength()
{
  return _contentLength;
}

esp_err_t PsychicResponse::send()
{
  //esp-idf makes you set the whole status.
  snprintf(_status, sizeof(_status), "%u %s", _code, http_status_reason(_code));
  httpd_resp_set_status(_request->request(), _status);

  //our headers too
  this->sendHeaders();

  //now send it off
  esp_err_t err = httpd_resp_send(_request->request(), getContent(), getContentLength());

  //did something happen?
  if (err != ESP_OK)
    ESP_LOGE(PH_TAG, "Send response failed (%s)", esp_err_to_name(err));
  else
    _request->markResponseSent();

  return err;
}

void PsychicResponse::sendHeaders()
{
  //get our global headers out of the way first
  for (HTTPHeader header : DefaultHeaders::Instance().getHeaders())
    httpd_resp_set_hdr(_request->request(), header.field, header.value);

  if (!hasExplicitHeader(_headers, _headerCount, "Cache-Control") &&
      shouldDisableCachingForPath(_request->path())) {
    httpd_resp_set_hdr(_request->request(), "Cache-Control", "no-store");
  }

  //now do our individual headers (ESP32-S3 optimized: fixed-size array)
  for (size_t i = 0; i < _headerCount; i++) {
    if (_headers[i].used) {
      httpd_resp_set_hdr(this->_request->request(), _headers[i].field, _headers[i].value);
    }
  }
  // No cleanup needed - headers are part of the object, not heap allocated
}

esp_err_t PsychicResponse::sendChunk(uint8_t *chunk, size_t chunksize)
{
  /* Send the buffer contents as HTTP response chunk */
  esp_err_t err = httpd_resp_send_chunk(this->_request->request(), (char *)chunk, chunksize);
  if (err != ESP_OK)
  {
    ESP_LOGE(PH_TAG, "File sending failed (%s)", esp_err_to_name(err));

    /* Abort sending file */
    httpd_resp_sendstr_chunk(this->_request->request(), NULL);

    /* Respond with 500 Internal Server Error */
    httpd_resp_send_err(this->_request->request(), HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
  } else {
    _request->markResponseSent();
  }

  return err;
}

esp_err_t PsychicResponse::finishChunking()
{
  /* Respond with an empty chunk to signal HTTP response completion */
  esp_err_t err = httpd_resp_send_chunk(this->_request->request(), NULL, 0);
  if (err == ESP_OK) {
    _request->markResponseSent();
  }
  return err;
}

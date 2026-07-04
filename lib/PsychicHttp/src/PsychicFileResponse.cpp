#include "PsychicFileResponse.h"
#include "PsychicResponse.h"
#include "PsychicRequest.h"


PsychicFileResponse::PsychicFileResponse(PsychicRequest *request, FS &fs, const String& path, const String& contentType, bool download)
 : PsychicResponse(request) {
  //_code = 200;
  String _path(path);

  if(!download && !fs.exists(_path) && fs.exists(_path+".gz")){
    _path = _path+".gz";
    addHeader("Content-Encoding", "gzip");
  } else if (!download && _path.endsWith(".gz")) {
      addHeader("Content-Encoding", "gzip");
  }

  _content = fs.open(_path, "r");
  _contentLength = _content.size();

  if(contentType == "")
    _setContentType(path);
  else
    setContentType(contentType.c_str());

  int filenameStart = path.lastIndexOf('/') + 1;
  char buf[26+path.length()-filenameStart];
  char* filename = (char*)path.c_str() + filenameStart;

  if(download) {
    // set filename and force download
    snprintf(buf, sizeof (buf), "attachment; filename=\"%s\"", filename);
    addHeader("Content-Disposition", buf);
  } 
  // else {
  //   // set filename and force rendering
  //   snprintf(buf, sizeof (buf), "inline; filename=\"%s\"", filename);
  // }
  // addHeader("Content-Disposition", buf);
}

PsychicFileResponse::PsychicFileResponse(PsychicRequest *request, File content, const String& path, const String& contentType, bool download)
 : PsychicResponse(request) {
  String _path(path);

  if(!download && String(content.name()).endsWith(".gz") && !path.endsWith(".gz")){
    addHeader("Content-Encoding", "gzip");
  }

  _content = content;
  _contentLength = _content.size();

  if(contentType == "")
    _setContentType(path);
  else
    setContentType(contentType.c_str());

  int filenameStart = path.lastIndexOf('/') + 1;
  char buf[26+path.length()-filenameStart];
  char* filename = (char*)path.c_str() + filenameStart;

  if(download) {
    snprintf(buf, sizeof (buf), "attachment; filename=\"%s\"", filename);
  } else {
    snprintf(buf, sizeof (buf), "inline; filename=\"%s\"", filename);
  }
  addHeader("Content-Disposition", buf);
}

PsychicFileResponse::~PsychicFileResponse()
{
  if(_content)
    _content.close();
}

void PsychicFileResponse::_setContentType(const String& path){
  const char *_contentType;
	
  if (path.endsWith(".html")) _contentType = "text/html";
  else if (path.endsWith(".htm")) _contentType = "text/html";
  else if (path.endsWith(".css")) _contentType = "text/css";
  else if (path.endsWith(".json")) _contentType = "application/json";
  else if (path.endsWith(".js")) _contentType = "application/javascript";
  else if (path.endsWith(".png")) _contentType = "image/png";
  else if (path.endsWith(".gif")) _contentType = "image/gif";
  else if (path.endsWith(".jpg")) _contentType = "image/jpeg";
  else if (path.endsWith(".ico")) _contentType = "image/x-icon";
  else if (path.endsWith(".svg")) _contentType = "image/svg+xml";
  else if (path.endsWith(".eot")) _contentType = "font/eot";
  else if (path.endsWith(".woff")) _contentType = "font/woff";
  else if (path.endsWith(".woff2")) _contentType = "font/woff2";
  else if (path.endsWith(".ttf")) _contentType = "font/ttf";
  else if (path.endsWith(".xml")) _contentType = "text/xml";
  else if (path.endsWith(".pdf")) _contentType = "application/pdf";
  else if (path.endsWith(".zip")) _contentType = "application/zip";
  else if(path.endsWith(".gz")) {
      // If the file is .gz, we need to check the extension BEFORE .gz to set the correct content type
      String realPath = path.substring(0, path.length() - 3);
      if (realPath.endsWith(".html")) _contentType = "text/html";
      else if (realPath.endsWith(".htm")) _contentType = "text/html";
      else if (realPath.endsWith(".css")) _contentType = "text/css";
      else if (realPath.endsWith(".json")) _contentType = "application/json";
      else if (realPath.endsWith(".js")) _contentType = "application/javascript";
      else if (realPath.endsWith(".png")) _contentType = "image/png";
      else if (realPath.endsWith(".gif")) _contentType = "image/gif";
      else if (realPath.endsWith(".jpg")) _contentType = "image/jpeg";
      else if (realPath.endsWith(".ico")) _contentType = "image/x-icon";
      else if (realPath.endsWith(".svg")) _contentType = "image/svg+xml";
      else _contentType = "application/x-gzip";
  }
  else _contentType = "text/plain";
  
  setContentType(_contentType);
}

esp_err_t PsychicFileResponse::send()
{
  esp_err_t err = ESP_OK;

  // Avoid heap allocations (malloc/free) per request.
  // These responses are hit heavily by the Web UI (JS/CSS/assets) and malloc churn
  // can quickly fragment heap on ESP32-C6.
  static constexpr size_t kStackChunkSize = 1024;

  //just send small files directly
  size_t size = getContentLength();
  if (size <= kStackChunkSize)
  {
    uint8_t buffer[kStackChunkSize];
    const size_t readSize = _content.readBytes((char *)buffer, size);
    this->setContent(buffer, readSize);
    err = PsychicResponse::send();
  }
  else
  {
    // Stream the file in fixed-size stack chunks.
    // Note: use httpd chunked API to avoid needing a full contiguous buffer.
    httpd_resp_set_status(this->_request->request(), "200 OK");
    this->sendHeaders();

    char chunk[kStackChunkSize];

    size_t chunksize;
    do {
        /* Read file in chunks into the scratch buffer */
        chunksize = _content.readBytes(chunk, sizeof(chunk));
        if (chunksize > 0)
        {
          err = this->sendChunk((uint8_t *)chunk, chunksize);
          if (err != ESP_OK)
            break;
        }

        /* Keep looping till the whole file is sent */
    } while (chunksize != 0);

    if (err == ESP_OK)
    {
      ESP_LOGD(PH_TAG, "File sending complete");
      this->finishChunking();
    }
  }

  return err;
}

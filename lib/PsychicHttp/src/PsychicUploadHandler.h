#ifndef PsychicUploadHandler_h
#define PsychicUploadHandler_h

#include "PsychicCore.h"
#include "PsychicHttpServer.h"
#include "PsychicRequest.h"
#include "PsychicWebHandler.h"
#include "PsychicWebParameter.h"

// ESP32-C6 optimized: Limit max form field value size to prevent unbounded String growth
#ifndef MAX_FORM_VALUE_SIZE
#define MAX_FORM_VALUE_SIZE 1024
#endif

//callback definitions
typedef std::function<esp_err_t(PsychicRequest *request, const String& filename, uint64_t index, uint8_t *data, size_t len, bool final)> PsychicUploadCallback;

/*
* HANDLER :: Can be attached to any endpoint or as a generic request handler.
*/

class PsychicUploadHandler : public PsychicWebHandler {
  protected:
    PsychicUploadCallback _uploadCallback;

    struct MultipartState {
        String _temp;
        size_t _parsedLength = 0;
        uint8_t _multiParseState = 0; // EXPECT_BOUNDARY
        String _boundary;
        uint8_t _boundaryPosition = 0;
        size_t _itemStartIndex = 0;
        size_t _itemSize = 0;
        String _itemName;
        String _itemFilename;
        String _itemType;
        String _itemValue;
        uint8_t *_itemBuffer = nullptr;
        size_t _itemBufferIndex = 0;
        bool _itemIsFile = false;
    };

    esp_err_t _basicUploadHandler(PsychicRequest *request);
    esp_err_t _multipartUploadHandler(PsychicRequest *request);

    void _handleUploadByte(MultipartState* state, PsychicRequest* request, uint8_t data, bool last);
    void _parseMultipartPostByte(MultipartState* state, PsychicRequest* request, uint8_t data, bool last);

  public:
    PsychicUploadHandler();
    ~PsychicUploadHandler();

    bool canHandle(PsychicRequest *request) override;
    esp_err_t handleRequest(PsychicRequest *request) override;

    PsychicUploadHandler * onUpload(PsychicUploadCallback fn);
};

enum {
  EXPECT_BOUNDARY,
  PARSE_HEADERS,
  WAIT_FOR_RETURN1,
  EXPECT_FEED1,
  EXPECT_DASH1,
  EXPECT_DASH2,
  BOUNDARY_OR_DATA,
  DASH3_OR_RETURN2,
  EXPECT_FEED2,
  PARSING_FINISHED,
  PARSE_ERROR
};

#endif // PsychicUploadHandler_h
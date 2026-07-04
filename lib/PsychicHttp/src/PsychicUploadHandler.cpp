#include "PsychicUploadHandler.h"
#if defined(ESP_PLATFORM)
#include "esp_heap_caps.h"
#endif

PsychicUploadHandler::PsychicUploadHandler() : PsychicWebHandler()
{
}
PsychicUploadHandler::~PsychicUploadHandler() {}

bool PsychicUploadHandler::canHandle(PsychicRequest *request)
{
    return true;
}

esp_err_t PsychicUploadHandler::handleRequest(PsychicRequest *request)
{
    esp_err_t err = ESP_OK;

    // request is passed to basic/multipart handlers, no need to store it in member variable
    // _request = request; // REMOVED shared state

    /* File cannot be larger than a limit */
    if (request->contentLength() > request->server()->maxUploadSize)
    {
        ESP_LOGE(PH_TAG, "File too large : %d bytes", request->contentLength());

        /* Respond with 400 Bad Request */
        char error[50];
        snprintf(error, sizeof(error), "File size must be less than %lu bytes!", request->server()->maxUploadSize);
        httpd_resp_send_err(request->request(), HTTPD_400_BAD_REQUEST, error);

        /* Return failure to close underlying connection else the incoming file content will keep the socket busy */
        return ESP_FAIL;
    }

    // we might want to access some of these params
    request->loadParams();

    // 2 types of upload requests
    if (request->isMultipart())
        err = _multipartUploadHandler(request);
    else
        err = _basicUploadHandler(request);

    // we can also call onRequest for some final processing and response
    if (err == ESP_OK)
    {
        if (_requestCallback != NULL)
            err = _requestCallback(request);
        else
            err = request->reply("Upload Successful.");
    }
    else
        request->reply(500, "text/html", "Error processing upload.");

    return err;
}

esp_err_t PsychicUploadHandler::_basicUploadHandler(PsychicRequest *request)
{
    esp_err_t err = ESP_OK;

    String filename = request->getFilename();

    /* Retrieve the pointer to scratch buffer for temporary storage */
#if defined(ESP_PLATFORM) && defined(BOARD_HAS_PSRAM)
    char *buf = (char *)heap_caps_malloc(FILE_CHUNK_SIZE, MALLOC_CAP_SPIRAM);
#else
    char *buf = (char *)malloc(FILE_CHUNK_SIZE);
#endif

    if (buf == NULL)
    {
        ESP_LOGE(PH_TAG, "Failed to allocate memory for upload buffer");
        return ESP_ERR_NO_MEM;
    }

    int received;
    unsigned long index = 0;

    /* Content length of the request gives the size of the file being uploaded */
    int remaining = request->contentLength();

    while (remaining > 0)
    {
        // ESP_LOGD(PH_TAG, "Remaining size : %d", remaining);

        /* Receive the file part by part into a buffer */
        if ((received = httpd_req_recv(request->request(), buf, min(remaining, FILE_CHUNK_SIZE))) <= 0)
        {
            /* Retry if timeout occurred */
            if (received == HTTPD_SOCK_ERR_TIMEOUT)
                continue;
            // bail if we got an error
            else if (received == HTTPD_SOCK_ERR_FAIL)
            {
                ESP_LOGE(PH_TAG, "Socket error");
                err = ESP_FAIL;
                break;
            }
        }

        // call our upload callback here.
        if (_uploadCallback != NULL)
        {
            err = _uploadCallback(request, filename, index, (uint8_t *)buf, received, (remaining - received == 0));
            if (err != ESP_OK)
                break;
        }
        else
        {
            ESP_LOGE(PH_TAG, "No upload callback specified!");
            err = ESP_FAIL;
            break;
        }

        /* Keep track of remaining size of the file left to be uploaded */
        remaining -= received;
        index += received;
    }

    // dont forget to free our buffer
    free(buf);

    return err;
}

esp_err_t PsychicUploadHandler::_multipartUploadHandler(PsychicRequest *request)
{
    esp_err_t err = ESP_OK;

    // Allocate state on heap (PSRAM if available) for thread safety
    MultipartState *state = new MultipartState();
    if (state == nullptr) {
         ESP_LOGE(PH_TAG, "Failed to allocate MultipartState");
         return ESP_ERR_NO_MEM;
    }
    
    // Reserve memory for string value to avoid fragmentation
    state->_itemValue.reserve(512);

    String value = request->header("Content-Type");
    if (value.startsWith("multipart/"))
    {
        state->_boundary = value.substring(value.indexOf('=') + 1);
        state->_boundary.replace("\"", "");
    }
    else
    {
        ESP_LOGE(PH_TAG, "No multipart boundary found.");
        delete state;
        return request->reply(400, "text/html", "No multipart boundary found.");
    }

#if defined(ESP_PLATFORM) && defined(BOARD_HAS_PSRAM)
    char *buf = (char *)heap_caps_malloc(FILE_CHUNK_SIZE, MALLOC_CAP_SPIRAM);
#else
    char *buf = (char *)malloc(FILE_CHUNK_SIZE);
#endif

    if (buf == NULL)
    {
        ESP_LOGE(PH_TAG, "Failed to allocate buffer");
        delete state;
        return ESP_ERR_NO_MEM;
    }

    int received;
    unsigned long index = 0;

    /* Content length of the request gives the size of the file being uploaded */
    int remaining = request->contentLength();

    while (remaining > 0)
    {
        // ESP_LOGD(PH_TAG, "Remaining size : %d", remaining);

        /* Receive the file part by part into a buffer */
        if ((received = httpd_req_recv(request->request(), buf, min(remaining, FILE_CHUNK_SIZE))) <= 0)
        {
            /* Retry if timeout occurred */
            if (received == HTTPD_SOCK_ERR_TIMEOUT)
                continue;
            // bail if we got an error
            else if (received == HTTPD_SOCK_ERR_FAIL)
            {
                ESP_LOGE(PH_TAG, "Socket error");
                err = ESP_FAIL;
                break;
            }
        }

        // parse it 1 byte at a time.
        for (int i = 0; i < received; i++)
        {
            /* Keep track of remaining size of the file left to be uploaded */
            remaining--;
            index++;

            // send it to our parser
            _parseMultipartPostByte(state, request, buf[i], !remaining);
            state->_parsedLength++;
        }
    }

    // dont forget to free our buffer
    free(buf);
    
    // Clean up state
    if (state->_itemBuffer) {
        free(state->_itemBuffer);
    }
    delete state;

    return err;
}

PsychicUploadHandler *PsychicUploadHandler::onUpload(PsychicUploadCallback fn)
{
    _uploadCallback = fn;
    return this;
}

void PsychicUploadHandler::_handleUploadByte(MultipartState* state, PsychicRequest* request, uint8_t data, bool last)
{
    state->_itemBuffer[state->_itemBufferIndex++] = data;

    if (last || state->_itemBufferIndex == FILE_CHUNK_SIZE)
    {
        if (_uploadCallback)
            _uploadCallback(request, state->_itemFilename, state->_itemSize - state->_itemBufferIndex, state->_itemBuffer, state->_itemBufferIndex, last);
        state->_itemBufferIndex = 0;
    }
}

// ESP32-C6 optimized: Added size limit for form field values to prevent heap fragmentation
#define itemWriteByte(b)                                          \
    do                                                            \
    {                                                             \
        state->_itemSize++;                                              \
        if (state->_itemIsFile)                                          \
            _handleUploadByte(state, request, b, last);                           \
        else if (state->_itemValue.length() < MAX_FORM_VALUE_SIZE)       \
            state->_itemValue += (char)(b);                              \
    } while (0)

void PsychicUploadHandler::_parseMultipartPostByte(MultipartState* state, PsychicRequest* request, uint8_t data, bool last)
{
    if (state->_multiParseState == PARSE_ERROR)
    {
        // not sure we can end up with an error during buffer fill, but jsut to be safe
        if (state->_itemBuffer != NULL)
        {
            free(state->_itemBuffer);
            state->_itemBuffer = NULL;
        }

        return;
    }

    if (!state->_parsedLength)
    {
        state->_multiParseState = EXPECT_BOUNDARY;
        state->_temp = String();
        state->_itemName = String();
        state->_itemFilename = String();
        state->_itemType = String();
    }

    if (state->_multiParseState == WAIT_FOR_RETURN1)
    {
        if (data != '\r')
        {
            itemWriteByte(data);
        }
        else
        {
            state->_multiParseState = EXPECT_FEED1;
        }
    }
    else if (state->_multiParseState == EXPECT_BOUNDARY)
    {
        if (state->_parsedLength < 2 && data != '-')
        {
            ESP_LOGE(PH_TAG, "Multipart: No boundary");
            state->_multiParseState = PARSE_ERROR;
            return;
        }
        else if (state->_parsedLength - 2 < state->_boundary.length() && state->_boundary.c_str()[state->_parsedLength - 2] != data)
        {
            ESP_LOGE(PH_TAG, "Multipart: Multipart malformed");
            state->_multiParseState = PARSE_ERROR;
            return;
        }
        else if (state->_parsedLength - 2 == state->_boundary.length() && data != '\r')
        {
            ESP_LOGE(PH_TAG, "Multipart: Multipart missing carriage return");
            state->_multiParseState = PARSE_ERROR;
            return;
        }
        else if (state->_parsedLength - 3 == state->_boundary.length())
        {
            if (data != '\n')
            {
                ESP_LOGE(PH_TAG, "Multipart: Multipart missing newline");
                state->_multiParseState = PARSE_ERROR;
                return;
            }
            state->_multiParseState = PARSE_HEADERS;
            state->_itemIsFile = false;
        }
    }
    else if (state->_multiParseState == PARSE_HEADERS)
    {
        if ((char)data != '\r' && (char)data != '\n')
            state->_temp += (char)data;
        if ((char)data == '\n')
        {
            if (state->_temp.length())
            {
                if (state->_temp.length() > 12 && state->_temp.substring(0, 12).equalsIgnoreCase("Content-Type"))
                {
                    state->_itemType = state->_temp.substring(14);
                    state->_itemIsFile = true;
                }
                else if (state->_temp.length() > 19 && state->_temp.substring(0, 19).equalsIgnoreCase("Content-Disposition"))
                {
                    state->_temp = state->_temp.substring(state->_temp.indexOf(';') + 2);
                    while (state->_temp.indexOf(';') > 0)
                    {
                        String name = state->_temp.substring(0, state->_temp.indexOf('='));
                        String nameVal = state->_temp.substring(state->_temp.indexOf('=') + 2, state->_temp.indexOf(';') - 1);
                        if (name == "name")
                        {
                            state->_itemName = nameVal;
                        }
                        else if (name == "filename")
                        {
                            state->_itemFilename = nameVal;
                            state->_itemIsFile = true;
                        }
                        state->_temp = state->_temp.substring(state->_temp.indexOf(';') + 2);
                    }
                    String name = state->_temp.substring(0, state->_temp.indexOf('='));
                    String nameVal = state->_temp.substring(state->_temp.indexOf('=') + 2, state->_temp.length() - 1);
                    if (name == "name")
                    {
                        state->_itemName = nameVal;
                    }
                    else if (name == "filename")
                    {
                        state->_itemFilename = nameVal;
                        state->_itemIsFile = true;
                    }
                }
                state->_temp = String();
            }
            else
            {
                state->_multiParseState = WAIT_FOR_RETURN1;
                // value starts from here
                state->_itemSize = 0;
                state->_itemStartIndex = state->_parsedLength;
                state->_itemValue = String();
                if (state->_itemIsFile)
                {
                    if (state->_itemBuffer)
                        free(state->_itemBuffer);
#if defined(ESP_PLATFORM) && defined(BOARD_HAS_PSRAM)
                    state->_itemBuffer = (uint8_t *)heap_caps_malloc(FILE_CHUNK_SIZE, MALLOC_CAP_SPIRAM);
#else
                    state->_itemBuffer = (uint8_t *)malloc(FILE_CHUNK_SIZE);
#endif
                    if (state->_itemBuffer == NULL)
                    {
                        ESP_LOGE(PH_TAG, "Multipart: Failed to allocate buffer");
                        state->_multiParseState = PARSE_ERROR;
                        return;
                    }
                    state->_itemBufferIndex = 0;
                }
            }
        }
    }
    else if (state->_multiParseState == EXPECT_FEED1)
    {
        if (data != '\n')
        {
            state->_multiParseState = WAIT_FOR_RETURN1;
            itemWriteByte('\r');
            _parseMultipartPostByte(state, request, data, last);
        }
        else
        {
            state->_multiParseState = EXPECT_DASH1;
        }
    }
    else if (state->_multiParseState == EXPECT_DASH1)
    {
        if (data != '-')
        {
            state->_multiParseState = WAIT_FOR_RETURN1;
            itemWriteByte('\r');
            itemWriteByte('\n');
            _parseMultipartPostByte(state, request, data, last);
        }
        else
        {
            state->_multiParseState = EXPECT_DASH2;
        }
    }
    else if (state->_multiParseState == EXPECT_DASH2)
    {
        if (data != '-')
        {
            state->_multiParseState = WAIT_FOR_RETURN1;
            itemWriteByte('\r');
            itemWriteByte('\n');
            itemWriteByte('-');
            _parseMultipartPostByte(state, request, data, last);
        }
        else
        {
            state->_multiParseState = BOUNDARY_OR_DATA;
            state->_boundaryPosition = 0;
        }
    }
    else if (state->_multiParseState == BOUNDARY_OR_DATA)
    {
        if (state->_boundaryPosition < state->_boundary.length() && state->_boundary.c_str()[state->_boundaryPosition] != data)
        {
            state->_multiParseState = WAIT_FOR_RETURN1;
            itemWriteByte('\r');
            itemWriteByte('\n');
            itemWriteByte('-');
            itemWriteByte('-');
            uint8_t i;
            for (i = 0; i < state->_boundaryPosition; i++)
                itemWriteByte(state->_boundary.c_str()[i]);
            _parseMultipartPostByte(state, request, data, last);
        }
        else if (state->_boundaryPosition == state->_boundary.length() - 1)
        {
            state->_multiParseState = DASH3_OR_RETURN2;
            if (!state->_itemIsFile)
            {
                request->addParam(state->_itemName, state->_itemValue);
                //_addParam(new AsyncWebParameter(_itemName, _itemValue, true));
            }
            else
            {
                if (state->_itemSize)
                {
                    if (_uploadCallback)
                        _uploadCallback(request, state->_itemFilename, state->_itemSize - state->_itemBufferIndex, state->_itemBuffer, state->_itemBufferIndex, true);
                    state->_itemBufferIndex = 0;
                    request->addParam(new PsychicWebParameter(state->_itemName, state->_itemFilename, true, true, state->_itemSize));
                }
                free(state->_itemBuffer);
                state->_itemBuffer = NULL;
            }
        }
        else
        {
            state->_boundaryPosition++;
        }
    }
    else if (state->_multiParseState == DASH3_OR_RETURN2)
    {
        if (data == '-' && (request->contentLength() - state->_parsedLength - 4) != 0)
        {
            ESP_LOGE(PH_TAG, "ERROR: The parser got to the end of the POST but is expecting more bytes!");
            state->_multiParseState = PARSE_ERROR;
            return;
        }
        if (data == '\r')
        {
            state->_multiParseState = EXPECT_FEED2;
        }
        else if (data == '-' && request->contentLength() == (state->_parsedLength + 4))
        {
            state->_multiParseState = PARSING_FINISHED;
        }
        else
        {
            state->_multiParseState = WAIT_FOR_RETURN1;
            itemWriteByte('\r');
            itemWriteByte('\n');
            itemWriteByte('-');
            itemWriteByte('-');
            uint8_t i;
            for (i = 0; i < state->_boundary.length(); i++)
                itemWriteByte(state->_boundary.c_str()[i]);
            _parseMultipartPostByte(state, request, data, last);
        }
    }
    else if (state->_multiParseState == EXPECT_FEED2)
    {
        if (data == '\n')
        {
            state->_multiParseState = PARSE_HEADERS;
            state->_itemIsFile = false;
        }
        else
        {
            state->_multiParseState = WAIT_FOR_RETURN1;
            itemWriteByte('\r');
            itemWriteByte('\n');
            itemWriteByte('-');
            itemWriteByte('-');
            uint8_t i;
            for (i = 0; i < state->_boundary.length(); i++)
                itemWriteByte(state->_boundary.c_str()[i]);
            itemWriteByte('\r');
            _parseMultipartPostByte(state, request, data, last);
        }
    }
}

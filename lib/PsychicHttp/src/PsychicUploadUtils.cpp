#include "PsychicUploadUtils.h"

namespace {

String trimCopy(String value)
{
    value.trim();
    return value;
}

Disposition parseDispositionToken(const String& token)
{
    String normalized = trimCopy(token);

    if (normalized.equalsIgnoreCase("form-data"))
        return FORM_DATA;
    if (normalized.equalsIgnoreCase("attachment"))
        return ATTACHMENT;
    if (normalized.equalsIgnoreCase("inline"))
        return INLINE;

    return NONE;
}

String parseParameterValue(const String& token)
{
    String value = trimCopy(token);
    if (value.length() >= 2 && value[0] == '"' && value[value.length() - 1] == '"')
        return value.substring(1, value.length() - 1);

    return value;
}

void applyParameterToken(const String& token, ContentDisposition& cd)
{
    String trimmed = trimCopy(token);
    const int equal = trimmed.indexOf('=');
    if (equal < 0)
        return;

    String key = trimCopy(trimmed.substring(0, equal));
    String value = parseParameterValue(trimmed.substring(equal + 1));

    if (key.equalsIgnoreCase("filename"))
        cd.filename = value;
    else if (key.equalsIgnoreCase("name"))
        cd.name = value;
}

String filenameFromUri(const String& uri)
{
    String path = uri;
    const int queryStart = path.indexOf('?');
    if (queryStart >= 0)
        path = path.substring(0, queryStart);

    const int filenameStart = path.lastIndexOf('/') + 1;
    if (filenameStart < 0 || filenameStart >= (int)path.length())
        return "";

    return path.substring(filenameStart);
}

} // namespace

namespace PsychicUploadUtils {

ContentDisposition parseContentDisposition(const String& header)
{
    ContentDisposition cd;
    String token;
    bool inQuotes = false;
    bool firstToken = true;

    for (size_t i = 0; i < header.length(); ++i)
    {
        const char ch = header.charAt(i);
        if (ch == '"')
            inQuotes = !inQuotes;

        if (ch == ';' && !inQuotes)
        {
            if (firstToken)
            {
                cd.disposition = parseDispositionToken(token);
                firstToken = false;
            }
            else
            {
                applyParameterToken(token, cd);
            }

            token = "";
            continue;
        }

        token += ch;
    }

    if (firstToken)
        cd.disposition = parseDispositionToken(token);
    else
        applyParameterToken(token, cd);

    return cd;
}

String resolveFilename(const String& contentDispositionHeader,
                       const PsychicWebParameter* filenameParam,
                       const String& uri)
{
    const ContentDisposition cd = parseContentDisposition(contentDispositionHeader);
    if (!cd.filename.isEmpty())
        return cd.filename;

    if (filenameParam != nullptr && !filenameParam->value().isEmpty())
        return filenameParam->value();

    return filenameFromUri(uri);
}

} // namespace PsychicUploadUtils

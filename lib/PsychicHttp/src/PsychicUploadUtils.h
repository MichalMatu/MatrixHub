#ifndef PsychicUploadUtils_h
#define PsychicUploadUtils_h

#include "PsychicContentDisposition.h"
#include "PsychicWebParameter.h"

namespace PsychicUploadUtils {

ContentDisposition parseContentDisposition(const String& header);
String resolveFilename(const String& contentDispositionHeader,
                       const PsychicWebParameter* filenameParam,
                       const String& uri);

} // namespace PsychicUploadUtils

#endif // PsychicUploadUtils_h

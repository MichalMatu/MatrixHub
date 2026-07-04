#ifndef PsychicContentDisposition_h
#define PsychicContentDisposition_h

#include <Arduino.h>

enum Disposition { NONE, INLINE, ATTACHMENT, FORM_DATA };

struct ContentDisposition {
  Disposition disposition = NONE;
  String filename;
  String name;
};

#endif // PsychicContentDisposition_h

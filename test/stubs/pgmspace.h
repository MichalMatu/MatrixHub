#pragma once

#ifndef PROGMEM
#define PROGMEM
#endif

#ifndef FPSTR
#define FPSTR(pstr_pointer) (reinterpret_cast<const char*>(pstr_pointer))
#endif

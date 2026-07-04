#pragma once

#include <netinet/ip_icmp.h>

#ifndef ICMPH_TYPE_SET
#define ICMPH_TYPE_SET(hdr, typeValue) ((hdr)->type = (typeValue))
#endif

#ifndef ICMPH_CODE_SET
#define ICMPH_CODE_SET(hdr, codeValue) ((hdr)->code = (codeValue))
#endif


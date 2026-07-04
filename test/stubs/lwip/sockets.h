#pragma once

#include <sys/socket.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

int setsockopt(int sockfd, int level, int optname, const void* optval, socklen_t optlen);

#ifdef __cplusplus
}
#endif

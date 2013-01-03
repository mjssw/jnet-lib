#ifndef _NET_SOCKET_H_
#define _NET_SOCKET_H_

#ifdef _WIN32
#include "iocp_socket.h"
#elif defined __linux__
#include "epoll_socket.h"
#endif

#endif

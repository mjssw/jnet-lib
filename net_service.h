#ifndef _NET_SERVICE_
#define _NET_SERVICE_

#ifdef _MSC_VER
#pragma once
#endif

#ifdef _WIN32
#include "iocp_service.h"
#elif defined __linux__
#include "epoll_service.h"
#endif

#endif

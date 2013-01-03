#ifndef _NET_FILE_H_
#define _NET_FILE_H_

#ifdef _MSC_VER
#pragma once
#endif

#ifdef _WIN32
#include "iocp_file.h"
#elif defined __linux__
#include "epoll_file.h"
#endif

#endif //_NET_FILE_H_

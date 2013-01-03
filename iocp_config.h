// iocp_config.h
#ifndef _IOCP_CONFIG_H_
#define _IOPC_CONFIG_H_

#ifdef _MSC_VER
#pragma once
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <mswsock.h>
#include <windows.h>
#elif defined __linux__
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#include <limits.h>
#include <assert.h>
#else
#error unsupported platform
#endif

#endif

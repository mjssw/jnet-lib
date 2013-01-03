// iocp_error.cpp
#ifndef _IOCP_NO_INCLUDE_
#include "iocp_error.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

namespace iocp {
namespace error {

std::string frame_error_describer(int value)
{
	switch (value)
	{
	case iocp::error::no_error:
		return "no error";
	case iocp::error::eof:
		return "eof";
	case iocp::error::already_open:
		return "handle already open";
	case iocp::error::not_open:
		return "handle has not open";
	case iocp::error::already_exists:
		return "file already exits";
#ifdef __linux__
	case iocp::error::epoll_flag:
		return "some unexpected epoll_wait flag set";
#endif
	default:
		return "unknown error";
	}
	return "unknown error";
}

std::string system_error_despcriber(int value)
{
#ifdef _WIN32
	char* msg = 0;
	DWORD length = ::FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER
		| FORMAT_MESSAGE_FROM_SYSTEM
		| FORMAT_MESSAGE_IGNORE_INSERTS, 0, value,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (char*)&msg, 0, 0);
	local_free_on_block_exit local_free_obj(msg);
	if (length && msg[length - 1] == '\n')
		msg[--length] = '\0';
	if (length && msg[length - 1] == '\r')
		msg[--length] = '\0';
	if (length)
		return msg;
	else
		return "system error";
#elif defined __linux__
	char *msg = 0;
	msg = (char *)malloc(1024);
	if (msg == NULL)
		return "error buffer allocation failed";
	local_free_on_block_exit local_free_obj(msg);
	char *ret = strerror_r(value, msg, 1024);
	return ret;
#endif

}

#pragma warning(push)
#pragma warning(disable:4996)

std::string describe_error(const iocp::error_code &ec)
{
	if (!ec.describer()) {
		char reason[64];
		sprintf(reason, "unknown error: %d", ec.value());
		return reason;
	}
	else {
		return ec.describer()(ec.value());
	}
}

#pragma warning(pop)

}
}

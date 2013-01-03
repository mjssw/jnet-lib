// iocp_error.h
#ifndef _IOCP_ERROR_H_
#define _IOCP_ERROR_H_

#ifdef _MSC_VER
#pragma once
#endif

#ifndef _IOCP_NO_INCLUDE_
#include "iocp_config.h"
#endif

#include <stdlib.h>
#include <string>

namespace iocp {

class error_code;

namespace error {

enum
{
	no_error = 0,
	eof = 1,
	already_open = 2,
	not_open = 3,
	already_exists = 4,
#ifdef __linux__
	epoll_flag = 5
#endif
};

typedef std::string (*describer_type)(int);
std::string frame_error_describer(int value);
std::string system_error_despcriber(int value);
std::string describe_error(const iocp::error_code &ec);

}

class error_code
{
public:
	error_code()
		: value_(iocp::error::no_error),
		describer_(iocp::error::frame_error_describer) {}
	error_code(int v, iocp::error::describer_type d)
		: value_(v), describer_(d) {}

	int value() const { return value_; }
	void set_value(int v) { value_ = v; }
	iocp::error::describer_type describer() const { return describer_; }
	void set_describer(iocp::error::describer_type d) { describer_ = d; }

	operator bool() const { return value_ != 0; }
	operator std::string() const { return iocp::error::describe_error(*this); }
	std::string to_string() const { return *this; }

private:
	int value_;
	iocp::error::describer_type describer_;
};

class local_free_on_block_exit
{
public:
	local_free_on_block_exit(char *buffer): buffer_(buffer) {}
#ifdef _WIN32
	~local_free_on_block_exit() { ::LocalFree(buffer_); }
#elif __linux__
	~local_free_on_block_exit() { ::free(buffer_); }
#endif

private:
	char *buffer_;
};

}

#endif

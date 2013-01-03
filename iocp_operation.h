// iocp_operation.h
#ifndef _IOCP_OPERATION_
#define _IOCP_OPERATION_

#ifdef _MSC_VER
#pragma once
#endif

#ifndef _IOCP_NO_INCLUDE_
#include "iocp_config.h"
#include "iocp_error.h"
#include "iocp_buffer.h"
#endif

namespace iocp {

class service;
#ifdef _WIN32
class operation: public OVERLAPPED
{
public:
	typedef void (*func_type)(iocp::service *,
		iocp::operation *, const iocp::error_code &, size_t);

	operation(func_type func, void *arg): func_(func), arg_(arg) { reset(); }
	virtual ~operation() {}

	void complete(iocp::service &owner,
		const iocp::error_code &ec, size_t bytes_transferred = 0)
	{
		func_(&owner, this, ec, bytes_transferred);
	}

	void reset()
	{
		Internal = 0;
		InternalHigh = 0;
		Offset = 0;
		OffsetHigh = 0;
		hEvent = 0;
		ready_ = 0;
	}

protected:
	void *argument() { return arg_; }

private:
	friend class service;

	func_type func_;
	void *arg_;
	long ready_;
};
#endif

#ifdef __linux__
class operation
{
public:
	typedef void (*func_type)(iocp::service *,
		iocp::operation *, const iocp::error_code &, size_t);

	operation(func_type func, void *arg): func_(func), arg_(arg) { reset(); }
	virtual ~operation() {}

	void complete(iocp::service &owner,
		const iocp::error_code &ec, size_t bytes_transferred = 0)
	{
		func_(&owner, this, ec, bytes_transferred);
	}

	void reset()
	{
		ready_ = 0;
		ec_ = iocp::error_code();
		bytes_transferred_ = 0;
		epoll_flags_ = 0;
	}

protected:
	void *argument() { return arg_; }
	func_type func_;
	
	iocp::error_code ec_;
	unsigned int bytes_transferred_;
	unsigned int epoll_flags_;

private:
	friend class service;

	void *arg_;
	long ready_;
};
#endif

}

#endif

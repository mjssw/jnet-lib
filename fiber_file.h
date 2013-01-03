// fiber_file.h
#ifndef _IOCP_NO_INCLUDE_
#pragma once
#include "fiber_scheduler.h"
#include "iocp_service.h"
#include "iocp_file.h"
#endif

namespace iocp {

class fiber_file: public iocp::file
{
public:
	fiber_file(iocp::service &service);
	fiber_file(iocp::service &service, fiber::scheduler::routine_type, void *arg);
	void invoke(fiber::scheduler::routine_type routine, void *arg);
	fiber::scheduler *scheduler() { return fiber_data_; }

	iocp::error_code write(const char *buf, size_t len, size_t &bytes_transferred);
	iocp::error_code read(char *buf, size_t len, size_t &bytes_transferred);
	iocp::error_code read_line(char *buf, size_t len, size_t &bytes_transferred, const char *eol = "\r\n", bool include_eol = false);

private:
	static void on_post(void *binded, const iocp::error_code &ec);
	static void on_written(void *binded, const iocp::error_code &ec, size_t bytes_trasferred);
	static void on_read(void *binded, const iocp::error_code &ec, size_t bytes_trasferred, char *buffer);

	fiber::scheduler *fiber_data_;

	/// return variables
	iocp::error_code ec_;
	size_t bytes_transferred_;
};

}

// fiber_http.h
#ifndef _IOCP_NO_INCLUDE_
#pragma once
#include "fiber_scheduler.h"
#include "iocp_service.h"
#include "http_socket.h"
#endif

namespace iocp {

class fiber_http_socket: public iocp::http_socket
{
public:
	fiber_http_socket(iocp::service &service, i_stream &stream);
	fiber_http_socket(iocp::service &service, i_stream &stream, fiber::scheduler::routine_type routine, void *arg);
	void invoke(fiber::scheduler::routine_type routine, void *arg);
	fiber::scheduler *scheduler() { return fiber_data_; }

	// TODO: supply more method to distinguish content
	iocp::error_code receive_request(http_socket::result &result, http::request &request, char *buf, size_t len);

private:
	static void on_post(void *binded, const iocp::error_code &ec);
	static void on_received_request(void *binded, const iocp::error_code &ec, const http_socket::result &result);

	iocp::service &service_;
	fiber::scheduler *fiber_data_;

	/// return variables
	iocp::error_code ec_;
	http_socket::result result_;
	size_t bytes_transferred_;
};

}

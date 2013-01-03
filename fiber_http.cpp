// fiber_http.cpp
#ifndef _IOCP_NO_INCLUDE_
#include "fiber_http.h"
#endif

namespace iocp {

fiber_http_socket::fiber_http_socket(iocp::service &service, i_stream &stream)
	: service_(service), iocp::http_socket(stream)
{
	fiber_data_ = fiber::scheduler::create(0, 0);
}

fiber_http_socket::fiber_http_socket(iocp::service &service, i_stream &stream, fiber::scheduler::routine_type routine, void *arg)
	: service_(service), iocp::http_socket(stream)
{
	fiber_data_ = fiber::scheduler::create(routine, arg);
	fiber::scheduler::switch_to(fiber_data_);
}

void fiber_http_socket::invoke(fiber::scheduler::routine_type routine, void *arg)
{
	fiber_data_->set_routine(routine);
	fiber_data_->set_argument(arg);
	fiber::scheduler::switch_to(fiber_data_);
}

void fiber_http_socket::on_post(void *binded, const iocp::error_code &ec)
{
	iocp::fiber_http_socket *socket = static_cast<iocp::fiber_http_socket *>(binded);
	if (!fiber::scheduler::switch_to(socket->fiber_data_))
		socket->service_.post(&fiber_http_socket::on_post, socket, ec);
}

void fiber_http_socket::on_received_request(void *binded,
																						const iocp::error_code &ec,
																						const http_socket::result &result)
{
	iocp::fiber_http_socket *socket = static_cast<iocp::fiber_http_socket *>(binded);
	socket->ec_ = ec;
	socket->result_ = result;
	if (!fiber::scheduler::switch_to(socket->fiber_data_))
		socket->service_.post(&fiber_http_socket::on_post, socket, ec);
}

iocp::error_code fiber_http_socket::receive_request(http_socket::result &result,
																										http::request &request,
																										char *buf, size_t len)
{
	asyn_receive_request(request, buf, len,\
		result.remain_buffer, result.remain_length,\
		false, &fiber_http_socket::on_received_request, this);
	fiber_data_->yield();
	result = result_;
	return ec_;
}

}

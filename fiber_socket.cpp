// fiber_socket.cpp
#ifndef _IOCP_NO_INCLUDE_
#include "fiber_socket.h"
#endif

namespace iocp {

fiber_socket::fiber_socket(iocp::service &service)
: iocp::socket(service)
{
	fiber_data_ = fiber::scheduler::create(0, 0);
}

fiber_socket::fiber_socket(iocp::service &service, fiber::scheduler::routine_type routine, void *arg)
: iocp::socket(service)
{
	fiber_data_ = fiber::scheduler::create(routine, arg);
	fiber::scheduler::switch_to(fiber_data_);
}

void fiber_socket::invoke(fiber::scheduler::routine_type routine, void *arg)
{
	fiber_data_->set_routine(routine);
	fiber_data_->set_argument(arg);
	fiber::scheduler::switch_to(fiber_data_);
}

void fiber_socket::on_post(void *binded, const iocp::error_code &ec)
{
	iocp::fiber_socket *socket = static_cast<iocp::fiber_socket *>(binded);
	if (!fiber::scheduler::switch_to(socket->fiber_data_))
		socket->service().post(&fiber_socket::on_post, socket, ec);
}

void fiber_socket::on_connected(void *binded, const iocp::error_code &ec)
{
	iocp::fiber_socket *socket = static_cast<iocp::fiber_socket *>(binded);
	socket->ec_ = ec;
	if (!fiber::scheduler::switch_to(socket->fiber_data_))
		socket->service().post(&fiber_socket::on_post, socket, ec);
}

iocp::error_code fiber_socket::connect(const iocp::address &addr, unsigned short port)
{
	asyn_connect(addr, port, &fiber_socket::on_connected, this);
	fiber_data_->yield();
	return ec_;
}

void fiber_socket::on_sent(void *binded, const iocp::error_code &ec, size_t bytes_trasferred)
{
	iocp::fiber_socket *socket = static_cast<iocp::fiber_socket *>(binded);
	socket->ec_ = ec;
	socket->bytes_transferred_ = bytes_trasferred;
	if (!fiber::scheduler::switch_to(socket->fiber_data_))
		socket->service().post(&fiber_socket::on_post, socket, ec);
}

iocp::error_code fiber_socket::send(const char *buf, size_t len, size_t &bytes_transferred)
{
	asyn_send(buf, len, &fiber_socket::on_sent, this);
	fiber_data_->yield();
	bytes_transferred = bytes_transferred_;
	return ec_;
}

void fiber_socket::on_received(void *binded, const iocp::error_code &ec, size_t bytes_trasferred, char *)
{
	iocp::fiber_socket *socket = static_cast<iocp::fiber_socket *>(binded);
	socket->ec_ = ec;
	socket->bytes_transferred_ = bytes_trasferred;
	if (!fiber::scheduler::switch_to(socket->fiber_data_))
		socket->service().post(&fiber_socket::on_post, socket, ec);
}

iocp::error_code fiber_socket::recv(char *buf, size_t len, size_t &bytes_transferred)
{
	asyn_recv(buf, len, &fiber_socket::on_received, this);
	fiber_data_->yield();
	bytes_transferred = bytes_transferred_;
	return ec_;
}

iocp::error_code fiber_socket::recv_some(char *buf, size_t len, size_t &bytes_transferred)
{
	asyn_recv_some(buf, len, &fiber_socket::on_received, this);
	fiber_data_->yield();
	bytes_transferred = bytes_transferred_;
	return ec_;
}

fiber_acceptor::fiber_acceptor(iocp::service &service)
: iocp::acceptor(service)
{
	fiber_data_ = fiber::scheduler::create(0, 0);
}

fiber_acceptor::fiber_acceptor(iocp::service &service, fiber::scheduler::routine_type routine, void *arg)
: iocp::acceptor(service)
{
	fiber_data_ = fiber::scheduler::create(routine, arg);
	fiber::scheduler::switch_to(fiber_data_);
}

void fiber_acceptor::invoke(fiber::scheduler::routine_type routine, void *arg)
{
	fiber_data_->set_routine(routine);
	fiber_data_->set_argument(arg);
	fiber::scheduler::switch_to(fiber_data_);
}

void fiber_acceptor::on_post(void *binded, const iocp::error_code &ec)
{
	iocp::fiber_acceptor *acceptor = static_cast<iocp::fiber_acceptor *>(binded);
	//socket->ec_ = ec;	need post with error code
	if (!fiber::scheduler::switch_to(acceptor->fiber_data_))
		acceptor->service().post(&fiber_acceptor::on_post, acceptor, ec);
}

void fiber_acceptor::on_accepted(void *binded, const iocp::error_code &ec, void *)
{
	iocp::fiber_acceptor *acceptor = static_cast<iocp::fiber_acceptor *>(binded);
	acceptor->ec_ = ec;
	if (!fiber::scheduler::switch_to(acceptor->fiber_data_))
		acceptor->service().post(&fiber_acceptor::on_post, acceptor, ec);
}

iocp::error_code fiber_acceptor::accept(iocp::socket &socket)
{
	asyn_accept(socket, 0, &fiber_acceptor::on_accepted, this);
	fiber_data_->yield();
	return ec_;
}

#ifdef _NET_ENABLE_SSL_

iocp::error_code fiber_ssl_socket::s_ec;

fiber_ssl_socket::fiber_ssl_socket(ssl::context &ctx, iocp::service &service, iocp::error_code &ec)
: iocp::ssl_socket(ctx, service, ec)
{
	fiber_data_ = fiber::scheduler::create(0, 0);
}

fiber_ssl_socket::fiber_ssl_socket(ssl::context &ctx, iocp::service &service, fiber::scheduler::routine_type routine, void *arg, iocp::error_code &ec)
: iocp::ssl_socket(ctx, service, ec)
{
	fiber_data_ = fiber::scheduler::create(routine, arg);
	fiber::scheduler::switch_to(fiber_data_);
}

void fiber_ssl_socket::invoke(fiber::scheduler::routine_type routine, void *arg)
{
	fiber_data_->set_routine(routine);
	fiber_data_->set_argument(arg);
	fiber::scheduler::switch_to(fiber_data_);
}

void fiber_ssl_socket::on_post(void *binded, const iocp::error_code &ec)
{
	iocp::fiber_ssl_socket *socket = static_cast<iocp::fiber_ssl_socket *>(binded);
	socket->ec_ = ec;
	if (!fiber::scheduler::switch_to(socket->fiber_data_))
		socket->service().post(&fiber_ssl_socket::on_post, socket, ec);
}

void fiber_ssl_socket::on_connected(void *binded, const iocp::error_code &ec)
{
	iocp::fiber_ssl_socket *socket = static_cast<iocp::fiber_ssl_socket *>(binded);
	socket->ec_ = ec;
	if (!fiber::scheduler::switch_to(socket->fiber_data_))
		socket->service().post(&fiber_ssl_socket::on_post, socket, ec);
}

iocp::error_code fiber_ssl_socket::connect(const iocp::address &addr, unsigned short port)
{
	asyn_connect(addr, port, &fiber_ssl_socket::on_connected, this);
	fiber_data_->yield();
	return ec_;
}

void fiber_ssl_socket::on_handshaked(void *binded, const iocp::error_code &ec)
{
	iocp::fiber_ssl_socket *socket = static_cast<iocp::fiber_ssl_socket *>(binded);
	socket->ec_ = ec;
	if (!fiber::scheduler::switch_to(socket->fiber_data_))
		socket->service().post(&fiber_ssl_socket::on_post, socket, ec);
}

iocp::error_code fiber_ssl_socket::handshake(ssl::handshake_type type)
{
	asyn_handshake(type, &fiber_ssl_socket::on_handshaked, this);
	fiber_data_->yield();
	return ec_;
}

void fiber_ssl_socket::on_sent(void *binded, const iocp::error_code &ec, size_t bytes_trasferred)
{
	iocp::fiber_ssl_socket *socket = static_cast<iocp::fiber_ssl_socket *>(binded);
	socket->ec_ = ec;
	socket->bytes_transferred_ = bytes_trasferred;
	if (!fiber::scheduler::switch_to(socket->fiber_data_))
		socket->service().post(&fiber_ssl_socket::on_post, socket, ec);
}

iocp::error_code fiber_ssl_socket::send(const char *buf, size_t len, size_t &bytes_transferred)
{
	asyn_send(buf, len, &fiber_ssl_socket::on_sent, this);
	fiber_data_->yield();
	bytes_transferred = bytes_transferred_;
	return ec_;
}

void fiber_ssl_socket::on_received(void *binded, const iocp::error_code &ec, size_t bytes_trasferred, char *)
{
	iocp::fiber_ssl_socket *socket = static_cast<iocp::fiber_ssl_socket *>(binded);
	socket->ec_ = ec;
	socket->bytes_transferred_ = bytes_trasferred;
	if (!fiber::scheduler::switch_to(socket->fiber_data_))
		socket->service().post(&fiber_ssl_socket::on_post, socket, ec);
}

iocp::error_code fiber_ssl_socket::recv(char *buf, size_t len, size_t &bytes_transferred)
{
	asyn_recv(buf, len, &fiber_ssl_socket::on_received, this);
	fiber_data_->yield();
	bytes_transferred = bytes_transferred_;
	return ec_;
}

iocp::error_code fiber_ssl_socket::recv_some(char *buf, size_t len, size_t &bytes_transferred)
{
	asyn_recv_some(buf, len, &fiber_ssl_socket::on_received, this);
	fiber_data_->yield();
	bytes_transferred = bytes_transferred_;
	return ec_;
}

void fiber_ssl_socket::on_shutdown(void *binded, const iocp::error_code &ec)
{
	iocp::fiber_ssl_socket *socket = static_cast<iocp::fiber_ssl_socket *>(binded);
	socket->ec_ = ec;
	if (!fiber::scheduler::switch_to(socket->fiber_data_))
		socket->service().post(&fiber_ssl_socket::on_post, socket, ec);
}

iocp::error_code fiber_ssl_socket::shutdown()
{
	asyn_shutdown(&fiber_ssl_socket::on_shutdown, this);
	fiber_data_->yield();
	return ec_;
}

#endif // _NET_ENABLE_SSL_

}

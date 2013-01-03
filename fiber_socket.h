// fiber_socket.h
#ifndef _FIBER_SOCKET_H_
#define _FIBER_SOCKET_H_

#ifdef _MSC_VER
#pragma once
#endif

#ifndef _IOCP_NO_INCLUDE_
#include "fiber_scheduler.h"
#include "net_service.h"
#include "net_socket.h"
#ifdef _NET_ENABLE_SSL_
#include "ssl_socket.h"
#endif // _NET_ENABLE_SSL_
#endif

namespace iocp {

class fiber_stream
{
public:
	virtual iocp::error_code send(const char *buf, size_t len, size_t &bytes_transferred) = 0;
	virtual iocp::error_code recv(char *buf, size_t len, size_t &bytes_transferred) = 0;
	virtual iocp::error_code recv_some(char *buf, size_t len, size_t &bytes_transferred) = 0;
};

class fiber_socket: public iocp::socket, public fiber_stream
{
public:
	fiber_socket(iocp::service &service);
	fiber_socket(iocp::service &service, fiber::scheduler::routine_type routine, void *arg);
	void invoke(fiber::scheduler::routine_type routine, void *arg);
	fiber::scheduler *scheduler() { return fiber_data_; }

	iocp::error_code connect(const iocp::address &addr, unsigned short port);
	iocp::error_code send(const char *buf, size_t len, size_t &bytes_transferred);
	iocp::error_code recv(char *buf, size_t len, size_t &bytes_transferred);
	iocp::error_code recv_some(char *buf, size_t len, size_t &bytes_transferred);

private:
	// TODO: this callback should be put in supper class
	static void on_post(void *binded, const iocp::error_code &ec);

	static void on_connected(void *binded, const iocp::error_code &ec);
	static void on_sent(void *binded, const iocp::error_code &ec, size_t bytes_trasferred);
	static void on_received(void *binded, const iocp::error_code &ec, size_t bytes_trasferred, char *buffer);

	fiber::scheduler *fiber_data_;

	/// return variables
	iocp::error_code ec_;
	size_t bytes_transferred_;
};

class fiber_acceptor: public iocp::acceptor
{
public:
	fiber_acceptor(iocp::service &service);
	fiber_acceptor(iocp::service &service, fiber::scheduler::routine_type routine, void *arg);
	void invoke(fiber::scheduler::routine_type routine, void *arg);

	iocp::error_code accept(iocp::socket &socket);

private:
	static void on_post(void *binded, const iocp::error_code &ec);
	static void on_accepted(void *binded, const iocp::error_code &ec, void *data);

	fiber::scheduler *fiber_data_;

	/// return variables
	iocp::error_code ec_;
};

#ifdef _NET_ENABLE_SSL_

class fiber_ssl_socket: public iocp::ssl_socket, public fiber_stream
{
public:
	fiber_ssl_socket(ssl::context &ctx, iocp::service &service, iocp::error_code &ec = s_ec);
	fiber_ssl_socket(ssl::context &ctx, iocp::service &service, fiber::scheduler::routine_type routine, void *arg, iocp::error_code &ec = s_ec);
	void invoke(fiber::scheduler::routine_type routine, void *arg);
	fiber::scheduler *scheduler() { return fiber_data_; }

	iocp::error_code connect(const iocp::address &addr, unsigned short port);
	iocp::error_code handshake(ssl::handshake_type type);
	iocp::error_code send(const char *buf, size_t len, size_t &bytes_transferred);
	iocp::error_code recv(char *buf, size_t len, size_t &bytes_transferred);
	iocp::error_code recv_some(char *buf, size_t len, size_t &bytes_transferred);
	iocp::error_code shutdown();

private:
	static void on_post(void *binded, const iocp::error_code &ec);
	static void on_connected(void *binded, const iocp::error_code &ec);
	static void on_handshaked(void *binded, const iocp::error_code &ec);
	static void on_sent(void *binded, const iocp::error_code &ec, size_t bytes_trasferred);
	static void on_received(void *binded, const iocp::error_code &ec, size_t bytes_trasferred, char *buffer);
	static void on_shutdown(void *binded, const iocp::error_code &ec);

	fiber::scheduler *fiber_data_;

	/// return variables
	iocp::error_code ec_;
	size_t bytes_transferred_;

	static iocp::error_code s_ec;
};

#endif // _NET_ENABLE_SSL_

}

#endif // _FIBER_SOCKET_H_

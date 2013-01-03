// epoll_socket.h
#ifdef __linux__

#ifndef _EPOLL_SOCKET_H_
#define _EPOLL_SOCKET_H_

#ifndef _IOCP_NO_INCLUDE_
#include "epoll_socket.h"
#include "iocp_error.h"
#include "iocp_address.h"
#include "iocp_operation.h"
#include "iocp_base_type.h"
#include "iocp_lock.h"
#endif

namespace iocp {

class i_stream
{
public:
	typedef void (*send_cb_type)(void *binded, const iocp::error_code &ec, size_t bytes_transferred);
	virtual void asyn_send(const char *buf, size_t len, send_cb_type callback, void *binded) = 0;

	typedef void (*receive_cb_type)(void *binded, const iocp::error_code &ec, size_t bytes_transferred, char *data);
	virtual void asyn_recv(char *buf, size_t len, receive_cb_type callback, void *data) = 0;
	virtual void asyn_recv_some(char *buf, size_t len, receive_cb_type callback, void *binded) = 0;
};

class service;
class socket_ops;
class socket_base: public base_type
{
public:
	socket_base(iocp::service &service);
	~socket_base();

	iocp::error_code open();
	iocp::error_code close();
	//iocp::error_code cancel();

	operator struct epoll_event &() { return event_; }

	//bool set_in_op(void *op) { return ::__sync_bool_compare_and_swap(&in_op_, 0, op); }
	void set_in_op(void *op) { sync_set(&in_op_, op); }
	//bool set_out_op(void *op) { return ::__sync_bool_compare_and_swap(&out_op_, 0, op); }
	void set_out_op(void *op) { sync_set(&out_op_, op); }

protected:
	friend class iocp::socket_ops;
	friend class iocp::service;
	int socket_;

private:
	struct epoll_event event_;;

	void *in_op_;
	void *out_op_;
};

class socket: public socket_base, public i_stream
{
public:
	socket(iocp::service &service): socket_base(service) {}
	~socket() {}

	typedef void (*connect_cb_type)(void *binded, const iocp::error_code &ec);
	void asyn_connect(const iocp::address &addr, unsigned short port, connect_cb_type callback, void *binded);
	// stream interface
	void asyn_send(const char *buf, size_t len, send_cb_type callback, void *binded);
	void asyn_recv(char *buf, size_t len, receive_cb_type callback, void *binded);
	void asyn_recv_some(char *buf, size_t len, receive_cb_type callback, void *binded);
};

class acceptor: public socket_base
{
public:
	acceptor(iocp::service &service): socket_base(service) {}

	iocp::error_code bind_and_listen(const iocp::address &addr, unsigned short port, int backlog);

	typedef void (*accept_cb_type)(void *binded, const iocp::error_code &ec, void *data);
	void asyn_accept(iocp::socket &socket, void *data, accept_cb_type callback, void *binded);
};

class socket_ops
{
public:
	static const int invalid_socket = -1;
	static iocp::error_code socket(iocp::socket_base &socket);
	static iocp::error_code close(iocp::socket_base &socket);
	static iocp::error_code bind_and_listen(iocp::acceptor &acceptor,
		const iocp::address &addr, unsigned short port, int backlog);
	static void begin_accept(iocp::acceptor &acceptor, iocp::operation *op);
	static iocp::error_code accept(iocp::acceptor &acceptor, iocp::socket &socket);
	static void end_accept(iocp::acceptor &acceptor);

	static void begin_connect(iocp::socket &socket, iocp::operation *op);
	static void connect(iocp::socket &socket,
		iocp::operation *op, const iocp::address &addr, unsigned short port);
	static void end_connect(iocp::socket &socket);

	static void begin_send(iocp::socket &socket, iocp::operation *op);
	static iocp::error_code send(iocp::socket &socket, const char *buffer, unsigned int &len);
	static void end_send(iocp::socket &socket);

	static void begin_recv(iocp::socket &socket, iocp::operation *op);
	static iocp::error_code recv(iocp::socket &socket, char *buffer, unsigned int &len);
	static void end_recv(iocp::socket &socket);

	static void post_operation(iocp::service &svr,
		iocp::operation *op, iocp::error_code &ec, size_t bytes_transferred);
};

class accept_op: public operation
{
public:
	accept_op(iocp::acceptor &acceptor,
		iocp::acceptor::accept_cb_type callback, void *binded, void *data, iocp::socket &socket)
		: operation(&accept_op::on_complete, binded),
		acceptor_(acceptor), callback_(callback), data_(data),
		socket_(socket), accepted_(false) {}

	void begin_accept();

private:
	friend class iocp::acceptor;
	static void on_complete(iocp::service *svr, iocp::operation *op,
		const iocp::error_code &ec, size_t bytes_transferred);

	iocp::acceptor &acceptor_;
	iocp::socket &socket_;
	iocp::acceptor::accept_cb_type callback_;
	bool accepted_;

	void *data_;
};

class connect_op: public operation
{
public:
	connect_op(iocp::socket &socket,
		const iocp::address &addr, unsigned short port,
		iocp::socket::connect_cb_type callback, void *binded)
		: operation(&connect_op::on_ready, binded),
		  socket_(socket), callback_(callback),
		  addr_(addr), port_(port) {}

	void begin_connect()
	{
		func_ = &connect_op::on_ready;
		socket_ops::begin_connect(socket_, this);
	}

private:
	static void on_ready(iocp::service *svr, iocp::operation *op,
		const iocp::error_code &ec, size_t bytes_transferred);
	static void on_complete(iocp::service *svr, iocp::operation *op,
		const iocp::error_code &ec, size_t bytes_transferred);

	iocp::address addr_;
	unsigned short port_;

	iocp::socket &socket_;
	iocp::socket::connect_cb_type callback_;
};

class send_op: public operation
{
public:
	send_op(iocp::socket &socket,
		const char *buf, size_t len, iocp::socket::send_cb_type callback, void *binded)
		: operation(&send_op::on_complete, binded),
		socket_(socket), callback_(callback),
		buf_start_pos_(buf), buffer_(buf, len), sent_(false) {}

		void begin_send();

private:
	enum { max_send_buffer_size = 64 * 1024 };
	static void on_complete(iocp::service *svr, iocp::operation *op,
		const iocp::error_code &ec, size_t bytes_transferred);

	iocp::socket &socket_;
	iocp::socket::send_cb_type callback_;

	const char *buf_start_pos_;
	const_buffer buffer_;

	bool sent_;
};

class recv_op: public operation
{
public:
	recv_op(iocp::socket &socket,
		char *buf, size_t len, bool some, iocp::socket::receive_cb_type callback, void *binded)
		: operation(&recv_op::on_complete, binded),
		socket_(socket), callback_(callback), some_(some),
		buf_start_pos_(buf), buffer_(buf, len, 0), recv_(false) {}

		void begin_recv();

private:
	enum { max_recv_buffer_size = 64 * 1024 };
	friend class socket;
	static void on_post(void *binded, const iocp::error_code &ec)
	{
		recv_op *op = static_cast<recv_op *>(binded);
		op->callback_(op->argument(), ec, 0, op->buffer_.raw_pos());
	}
	static void on_complete(iocp::service *svr, iocp::operation *op,
		const iocp::error_code &ec, size_t bytes_transferred);

	iocp::socket &socket_;
	iocp::socket::receive_cb_type callback_;

	bool some_;
	char *buf_start_pos_;
	mutable_buffer buffer_;
	bool recv_;
};

}

#endif

#endif

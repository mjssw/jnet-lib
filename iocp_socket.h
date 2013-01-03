// iocp_socket.h
#ifdef _WIN32

#ifndef _IOCP_NO_INCLUDE_
#pragma once
#include "iocp_error.h"
#include "iocp_address.h"
#include "iocp_operation.h"
#include "iocp_base_type.h"
#endif

namespace iocp {

/**
 * @brief interface for socket support stream API
 */
class i_stream
{
public:
  /**
   * @brief callback type for asyn_send
   * @param[in]   binded the point provided before
   * @param[out]  ec error code for this operation
   * @param[out]  bytes_transferred bytes sent
   */
	typedef void (*send_cb_type)(void *binded, const iocp::error_code &ec, size_t bytes_transferred);
  /**
   * @brief send data asynchronously
   * @param[in]   buf buffer in which data will be sent
   * @param[in]   len buffer length
   * @param[in]   callback callback function when sending finished
   * @param[in]   binded binded data which will pass to callback function
   */
	virtual void asyn_send(const char *buf, size_t len, send_cb_type callback, void *binded) = 0;

  /**
   * @brief callback type for asyn_recv & asyn_recv_some
   * @param[in]   binded the point provided before
   * @param[out]  ec error code for this operation
   * @param[out]  bytes_transferred bytes received
   * @param[in]   data buffer provided before
   */
	typedef void (*receive_cb_type)(void *binded, const iocp::error_code &ec, size_t bytes_transferred, char *data);
  /**
   * @brief send data asynchronously
   * callback when received the specified length of data
   * @param[in]   buf buffer in which data will be stored
   * @param[in]   len the length that MUST receive. provide zero will callback immediately
   * @param[in]   callback callback function when receiving finished
   * @param[in]   binded binded data which will pass to callback function
   */
	virtual void asyn_recv(char *buf, size_t len, receive_cb_type callback, void *data) = 0;
  /**
   * @brief send data asynchronously
   * callback when received any data
   * @param[in]   buf buffer in which data will be stored
   * @param[in]   len the length that MUST receive. provide zero will callback immediately
   * @param[in]   callback callback function when receiving finished
   * @param[in]   binded binded data which will pass to callback function
   */
	virtual void asyn_recv_some(char *buf, size_t len, receive_cb_type callback, void *binded) = 0;
};

class service;
class socket_ops;
/**
 * @brief base class for socket support open & close
 */
class socket_base: public base_type
{
public:
  enum sd_type { sd_read = SD_RECEIVE, sd_write = SD_SEND, sd_both = SD_BOTH };
  /**
   * Constructor
   * @param[in] service a service instance
   */
	socket_base(iocp::service &service);
  /**
   * Destructor
   */
	virtual ~socket_base();

  /**
   * open a socket
   * @retval error code
   */
	iocp::error_code open();
  /**
   * close a socket
   * @retval error code
   */
	iocp::error_code close();
	//iocp::error_code cancel();

  iocp::error_code shutdown(iocp::socket_base::sd_type type);

  iocp::error_code getsockname(iocp::address &addr, unsigned short &port);
  iocp::error_code getpeername(iocp::address &addr, unsigned short &port);

protected:
	friend class iocp::socket_ops;
	SOCKET socket_;
};

/**
 * @brief a tcp socket class
 */
class socket: public socket_base, public i_stream
{
public:
	socket(iocp::service &service): socket_base(service), already_binded_(0) {}

	typedef void (*connect_cb_type)(void *binded, const iocp::error_code &ec);
	void asyn_connect(const iocp::address &addr, unsigned short port, connect_cb_type callback, void *binded);
	// stream interface
	void asyn_send(const char *buf, size_t len, send_cb_type callback, void *binded);
	void asyn_recv(char *buf, size_t len, receive_cb_type callback, void *binded);
	void asyn_recv_some(char *buf, size_t len, receive_cb_type callback, void *binded);
  
private:
  friend class socket_ops;
  long already_binded_;
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
  static const SOCKET invalid_socket = INVALID_SOCKET;
  static iocp::error_code socket(iocp::socket_base &socket);
  static iocp::error_code close(iocp::socket_base &socket);
  static iocp::error_code shutdown(iocp::socket_base &socket, iocp::socket_base::sd_type type);
  static iocp::error_code bind_and_listen(iocp::acceptor &acceptor, const iocp::address &addr, unsigned short port, int backlog);
  static iocp::error_code update_accept_context(iocp::acceptor &acceptor, iocp::socket &socket);
  static iocp::error_code update_connect_context(iocp::socket &socket);
  static iocp::error_code getsockname(iocp::socket_base &socket, iocp::address &addr, unsigned short &port);
  static iocp::error_code getpeername(iocp::socket_base &socket, iocp::address &addr, unsigned short &port);
  static void accept(iocp::acceptor &acceptor,
    iocp::operation *op, iocp::socket &socket, char *buffer);
  static void connect(iocp::socket &socket,
    iocp::operation *op, const iocp::address &addr, unsigned short port);
  static void send(iocp::socket &socket,
    iocp::operation *op, WSABUF *buffers, size_t buffer_count);
  static void recv(iocp::socket &socket,
    iocp::operation *op, WSABUF *buffers, size_t buffer_count);
};

class accept_op: public operation
{
public:
	accept_op(iocp::acceptor &acceptor,
		iocp::acceptor::accept_cb_type callback, void *binded, void *data, iocp::socket &socket)
		: operation(&accept_op::on_complete, binded),
		acceptor_(acceptor), callback_(callback), data_(data), socket_(socket) {}

private:
	friend class iocp::acceptor;
	static void on_complete(iocp::service *svr, iocp::operation *op,
		const iocp::error_code &ec, size_t bytes_transferred);

	iocp::acceptor &acceptor_;
	iocp::socket &socket_;
	iocp::acceptor::accept_cb_type callback_;
	char buffer_[(sizeof(SOCKADDR_IN)+16)*2];

	void *data_;
};

class connect_op: public operation
{
public:
	connect_op(iocp::socket &socket,
		iocp::socket::connect_cb_type callback, void *binded)
		: operation(&connect_op::on_complete, binded),
		socket_(socket), callback_(callback) {}

private:
	static void on_complete(iocp::service *svr, iocp::operation *op,
		const iocp::error_code &ec, size_t bytes_transferred);

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
		buf_start_pos_(buf), buffer_(buf, len), wsa_buffer_(buf, len)
	{ wsa_buffer_.shrink_to(max_send_buffer_size); }

private:
	enum { max_send_buffer_size = 64 * 1024 };
	friend class socket;
	static void on_complete(iocp::service *svr, iocp::operation *op,
		const iocp::error_code &ec, size_t bytes_transferred);

	iocp::socket &socket_;
	iocp::socket::send_cb_type callback_;

	const char *buf_start_pos_;
	const_buffer buffer_;
	wsabuf_wrapper wsa_buffer_;
};

class recv_op: public operation
{
public:
	recv_op(iocp::socket &socket,
		char *buf, size_t len, bool some, iocp::socket::receive_cb_type callback, void *binded)
		: operation(&recv_op::on_complete, binded),
		socket_(socket), callback_(callback), some_(some),
		buf_start_pos_(buf), buffer_(buf, len, 0), wsa_buffer_(buf, len)
	{ wsa_buffer_.shrink_to(max_recv_buffer_size < len ? max_recv_buffer_size : len); }

private:
	enum { max_recv_buffer_size = 64 * 1024 };
	friend class socket;
	static void on_complete(iocp::service *svr, iocp::operation *op,
		const iocp::error_code &ec, size_t bytes_transferred);

	iocp::socket &socket_;
	iocp::socket::receive_cb_type callback_;

	bool some_;
	char *buf_start_pos_;
	mutable_buffer buffer_;
	wsabuf_wrapper wsa_buffer_;
};

}

#endif

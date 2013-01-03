// iocp_socket.cpp
#ifdef _WIN32

#ifndef _IOCP_NO_INCLUDE_
#include "iocp_socket.h"
#include "iocp_service.h"
#endif

namespace iocp {

/// socket_base

socket_base::socket_base(iocp::service &service)
: base_type(service), socket_(socket_ops::invalid_socket)
{

}

socket_base::~socket_base()
{
	close();
}

iocp::error_code socket_base::open()
{
	return socket_ops::socket(*this);
}

iocp::error_code socket_base::close()
{
	return socket_ops::close(*this);
}

iocp::error_code socket_base::shutdown(iocp::socket_base::sd_type type)
{
  return socket_ops::shutdown(*this, type);
}

iocp::error_code socket_base::getsockname(iocp::address &addr, unsigned short &port)
{
  return socket_ops::getsockname(*this, addr, port);
}

iocp::error_code socket_base::getpeername(iocp::address &addr, unsigned short &port)
{
  return socket_ops::getpeername(*this, addr, port);
}

/// socket

void socket::asyn_connect(const iocp::address &addr, unsigned short port, connect_cb_type callback, void *binded)
{
	iocp::connect_op *op = new iocp::connect_op(*this, callback, binded);
	socket_ops::connect(*this, op, addr, port);
}

void socket::asyn_send(const char *buf, size_t len, send_cb_type callback, void *binded)
{
	iocp::send_op *op = new iocp::send_op(*this, buf, len, callback, binded);
	socket_ops::send(*this, op, &op->wsa_buffer_, 1);
}

void socket::asyn_recv(char *buf, size_t len, receive_cb_type callback, void *binded)
{
	iocp::recv_op *op = new iocp::recv_op(*this, buf, len, false, callback, binded);
	socket_ops::recv(*this, op, &op->wsa_buffer_, 1);
}

void socket::asyn_recv_some(char *buf, size_t len, receive_cb_type callback, void *binded)
{
	iocp::recv_op *op = new iocp::recv_op(*this, buf, len, true, callback, binded);
	socket_ops::recv(*this, op, &op->wsa_buffer_, 1);
}

/// acceptor

iocp::error_code acceptor::bind_and_listen(const iocp::address &addr, unsigned short port, int backlog)
{
	return socket_ops::bind_and_listen(*this, addr, port, backlog);
}

void acceptor::asyn_accept(iocp::socket &socket, void *data, accept_cb_type callback, void *binded)
{
	iocp::accept_op *op = new iocp::accept_op(*this, callback, binded, data, socket);
	socket_ops::accept(*this, op, socket, op->buffer_);
}

/// socket_ops

iocp::error_code socket_ops::socket(iocp::socket_base &socket)
{
	if (socket.socket_ != socket_ops::invalid_socket)
		return iocp::error_code(iocp::error::already_open, \
		iocp::error::frame_error_describer);

	iocp::error_code ec;
	ec = socket.service().socket(socket.socket_);
	if (!ec)
		ec = socket.service().register_handle((HANDLE)socket.socket_);
	return ec;
}

iocp::error_code socket_ops::close(iocp::socket_base &socket)
{
	if (socket.socket_ == socket_ops::invalid_socket)
		return iocp::error_code(iocp::error::not_open, \
		iocp::error::frame_error_describer);

	return socket.service().close_socket(socket.socket_);
}

iocp::error_code socket_ops::shutdown(iocp::socket_base &socket, iocp::socket_base::sd_type type)
{
  return socket.service().shutdown(socket.socket_, type);
}

iocp::error_code socket_ops::bind_and_listen(iocp::acceptor &acceptor,
																						 const iocp::address &addr,
																						 unsigned short port, int backlog)
{
	iocp::service &service = acceptor.service();
	iocp::error_code ec = acceptor.open();
	if (!ec || ec.value() == iocp::error::already_open)
		ec = service.bind(acceptor.socket_, addr, port);
	if (!ec)
		ec = service.listen(acceptor.socket_, backlog);

	return ec;
}

iocp::error_code socket_ops::update_accept_context(iocp::acceptor &acceptor,
																									 iocp::socket &socket)
{
	if (setsockopt(socket.socket_, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
		(char *)&acceptor.socket_, sizeof(acceptor.socket_)))
		return iocp::error_code(::WSAGetLastError(), iocp::error::system_error_despcriber);
	return iocp::error_code();
}

iocp::error_code socket_ops::update_connect_context(iocp::socket &socket)
{
	if (setsockopt(socket.socket_, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0))
		return iocp::error_code(::WSAGetLastError(), iocp::error::system_error_despcriber);
	return iocp::error_code();
}

iocp::error_code socket_ops::getsockname(iocp::socket_base &socket, iocp::address &addr, unsigned short &port)
{
  return socket.service().getsockname(socket.socket_, addr, port);
}

iocp::error_code socket_ops::getpeername(iocp::socket_base &socket, iocp::address &addr, unsigned short &port)
{
  return socket.service().getpeername(socket.socket_, addr, port);
}

void socket_ops::accept(iocp::acceptor &acceptor,
												iocp::operation *op,
												iocp::socket &socket,
												char *buffer)
{
	iocp::service &service = acceptor.service();
	service.work_started();
	iocp::error_code ec = socket.open();
	if (ec && ec.value() != iocp::error::already_open)
		service.on_completion(op, ec);
	else
		service.accept(acceptor.socket_, op, socket.socket_, buffer);
}

void socket_ops::connect(iocp::socket &socket,
												 iocp::operation *op,
												 const iocp::address &addr,
												 unsigned short port)
{
	iocp::service &service = socket.service();
	service.work_started();
	iocp::error_code ec = socket.open();
  if (ec.value() == iocp::error::already_open)
    ec.set_value(0);
	// IOCP needs socket to bind an address before calling ConnectEx
	if (!ec && !sync_get(&socket.already_binded_)) {
		ec = service.bind(socket.socket_, iocp::address(0U), 0);
    if (!ec)
      sync_set(&socket.already_binded_, 1);
  }
	if (ec)
		service.on_completion(op, ec);
	else
		service.connect(socket.socket_, op, addr, port);
}

void socket_ops::send(iocp::socket &socket,
											iocp::operation *op,
											WSABUF *buffers,
											size_t buffer_count)
{
	iocp::service &service = socket.service();
	service.work_started();
	service.send(socket.socket_, op, buffers, buffer_count);
}

void socket_ops::recv(iocp::socket &socket,
											iocp::operation *op,
											WSABUF *buffers,
											size_t buffer_count)
{
	iocp::service &service = socket.service();
	service.work_started();
	service.recv(socket.socket_, op, buffers, buffer_count);
}

/// operations

void accept_op::on_complete(iocp::service *svr, iocp::operation *op_,
														const iocp::error_code &ec, size_t bytes_transferred)
{
	accept_op *op = static_cast<accept_op *>(op_);
	iocp::error_code ec_ = ec;
	if (!ec_)
		ec_ = socket_ops::update_accept_context(op->acceptor_, op->socket_);
	op->callback_(op->argument(), ec_, op->data_);

	// commit suicide, where does asio do this?
	delete op;
}

void connect_op::on_complete(iocp::service *svr, iocp::operation *op_,
														 const iocp::error_code &ec, size_t bytes_transferred)
{
	connect_op *op = static_cast<connect_op *>(op_);
	iocp::error_code ec_ = ec;
	if (!ec_)
		ec_ = socket_ops::update_connect_context(op->socket_);
	op->callback_(op->argument(), ec_);

	delete op;
}

void send_op::on_complete(iocp::service *svr, iocp::operation *op_,
													const iocp::error_code &ec, size_t bytes_transferred)
{
	send_op *op = static_cast<send_op *>(op_);

	if (!ec) {
		op->buffer_.consume(bytes_transferred);
		if (op->buffer_.len()) {
			op->wsa_buffer_ = op->buffer_;
			op->wsa_buffer_.shrink_to(max_send_buffer_size);
			op->reset();
			socket_ops::send(op->socket_, op, &op->wsa_buffer_, 1);
		}
	}

	if (ec || op->buffer_.len() == 0) {
		op->callback_(op->argument(), ec, static_cast<size_t>(op->buffer_.raw() - op->buf_start_pos_));
		delete op;
	}
}

void recv_op::on_complete(iocp::service *svr, iocp::operation *op_,
													const iocp::error_code &ec_, size_t bytes_transferred)
{
	recv_op *op = static_cast<recv_op *>(op_);

	iocp::error_code ec = ec_;

	if (!ec && bytes_transferred == 0 && op->buffer_.remain() > 0) {	// already end
		ec.set_value(iocp::error::eof);
		ec.set_describer(iocp::error::frame_error_describer);
	}

	if (!ec) {
		size_t remain;
		op->buffer_.append(bytes_transferred);
		if (!op->some_ && (remain = op->buffer_.remain()) > 0) {
			op->wsa_buffer_ = op->buffer_;
			op->wsa_buffer_.shrink_to(max_recv_buffer_size < remain ? max_recv_buffer_size : remain);
			op->reset();
			socket_ops::recv(op->socket_, op, &op->wsa_buffer_, 1);
			return;
		}
	}

	if (ec || op->some_ || op->buffer_.remain() == 0) {
		op->callback_(op->argument(), ec, op->buffer_.len(), op->buf_start_pos_);
		delete op;
	}
}

}

#endif

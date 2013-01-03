// epoll_socket.cpp
#ifdef __linux__

#ifndef _IOCP_NO_INCLUDE_
#include "epoll_socket.h"
#include "epoll_service.h"
#endif

namespace iocp {

/// socket_base

socket_base::socket_base(iocp::service &service)
: base_type(service), socket_(socket_ops::invalid_socket)
{
	event_.events = EPOLLERR|EPOLLHUP|EPOLLRDHUP|EPOLLET;
	event_.data.ptr = this;
	in_op_ = 0;
	out_op_ = 0;
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

/// socket

void socket::asyn_connect(const iocp::address &addr, unsigned short port, connect_cb_type callback, void *binded)
{
	iocp::connect_op *op = new iocp::connect_op(*this, addr, port, callback, binded);
	op->begin_connect();
}

void socket::asyn_send(const char *buf, size_t len, send_cb_type callback, void *binded)
{
	iocp::send_op *op = new iocp::send_op(*this, buf, len, callback, binded);
	op->begin_send();
}

void socket::asyn_recv(char *buf, size_t len, receive_cb_type callback, void *binded)
{
	iocp::recv_op *op = new iocp::recv_op(*this, buf, len, false, callback, binded);
	op->begin_recv();
}

void socket::asyn_recv_some(char *buf, size_t len, receive_cb_type callback, void *binded)
{
	iocp::recv_op *op = new iocp::recv_op(*this, buf, len, true, callback, binded);
	op->begin_recv();
}

/// acceptor

iocp::error_code acceptor::bind_and_listen(const iocp::address &addr, unsigned short port, int backlog)
{
	return socket_ops::bind_and_listen(*this, addr, port, backlog);
}

void acceptor::asyn_accept(iocp::socket &socket, void *data, accept_cb_type callback, void *binded)
{
	iocp::accept_op *op = new iocp::accept_op(*this, callback, binded, data, socket);
	op->begin_accept();
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
		ec = socket.service().register_event(socket.socket_, socket);
	return ec;
}

iocp::error_code socket_ops::close(iocp::socket_base &socket)
{
	if (socket.socket_ == socket_ops::invalid_socket)
		return iocp::error_code(iocp::error::not_open, \
		  iocp::error::frame_error_describer);

	socket.service().delete_event(socket.socket_, socket);
	return socket.service().close_socket(socket.socket_);
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

void socket_ops::begin_accept(iocp::acceptor &acceptor, iocp::operation *op)
{
	iocp::error_code ec;
	iocp::service &service = acceptor.service();
	acceptor.set_in_op(op);
	service.work_started();
	service.wait_for_epollin(acceptor.socket_, acceptor, op);
}

iocp::error_code socket_ops::accept(iocp::acceptor &acceptor, iocp::socket &socket)
{
	iocp::error_code ec;
	iocp::service &service = acceptor.service();
	ec = service.accept(acceptor.socket_, socket.socket_);
	if (!ec)
		ec = service.register_event(socket.socket_, socket);
	return ec;
}

void socket_ops::end_accept(iocp::acceptor &acceptor)
{
	//acceptor.service().clear_epollin(acceptor.socket_, acceptor);
}

void socket_ops::begin_connect(iocp::socket &socket, iocp::operation *op)
{
  bool ret;
	iocp::error_code ec;
	iocp::service &service = socket.service();
	socket.set_out_op(op);
	service.work_started();

	ec = socket.open();
	if (ec)
		service.on_completion(op, ec);
	else
		ret = service.wait_for_epollout(socket.socket_, socket, op);
  if (!ret)
    socket.set_out_op(0);
}

void socket_ops::connect(iocp::socket &socket,
            						 iocp::operation *op,
            						 const iocp::address &addr,
            						 unsigned short port)
{
  bool ret;
  iocp::error_code ec;
	iocp::service &service = socket.service();
	op->reset();
	socket.set_out_op(op);
	service.work_started();

  ec = service.connect(socket.socket_, addr, port);
  if (ec)
    service.on_completion(op, ec);
  else
    ret = service.wait_for_epollout(socket.socket_, socket, op);
  if (!ret)
    socket.set_out_op(0);
}

void socket_ops::end_connect(iocp::socket &socket)
{
	//socket.service().clear_epollout(socket.socket_, socket);
}

void socket_ops::begin_send(iocp::socket &socket, iocp::operation *op)
{
	iocp::service &service = socket.service();
	socket.set_out_op(op);
	service.work_started();
	service.wait_for_epollout(socket.socket_, socket, op);
}

iocp::error_code socket_ops::send(iocp::socket &socket,
			                            const char *buffer, unsigned int &len)
{
	iocp::service &service = socket.service();
	return service.send(socket.socket_, buffer, len);
}

void socket_ops::end_send(iocp::socket &socket)
{
	//socket.service().clear_epollout(socket.socket_, socket);
}

void socket_ops::begin_recv(iocp::socket &socket, iocp::operation *op)
{
	iocp::service &service = socket.service();
	socket.set_in_op(op);
	service.work_started();
	service.wait_for_epollin(socket.socket_, socket, op);
}

iocp::error_code socket_ops::recv(iocp::socket &socket,
                                  char *buffer, unsigned int &len)
{
	iocp::service &service = socket.service();
	return service.recv(socket.socket_, buffer, len);
}

void socket_ops::end_recv(iocp::socket &socket)
{
	//socket.service().clear_epollin(socket.socket_, socket);
}

void socket_ops::post_operation(iocp::service &svr,
                                iocp::operation *op,
                                iocp::error_code &ec,
                                size_t bytes_transferred)
{
  svr.work_started();
  svr.on_completion(op, ec, bytes_transferred);
}

/// operations

void accept_op::begin_accept()
{
  iocp::error_code ec;
  ec = socket_ops::accept(acceptor_, socket_);
  if (ec.value() == EAGAIN || ec.value() == EINPROGRESS) {
    accepted_ = false;
    socket_ops::begin_accept(acceptor_, this);
  }
  else {
    accepted_ = true;
    socket_ops::post_operation(acceptor_.service(), this, ec, 0);
  }
}

void accept_op::on_complete(iocp::service *svr, iocp::operation *op_,
							const iocp::error_code &ec, size_t bytes_transferred)
{
	accept_op *op = static_cast<accept_op *>(op_);
	op->ec_ = ec;
	if (!op->ec_ && !op->accepted_)
		op->ec_ = socket_ops::accept(op->acceptor_, op->socket_);
	socket_ops::end_accept(op->acceptor_);
	op->callback_(op->argument(), op->ec_, op->data_);

	// commit suicide, where does asio do this?
	delete op;
}

void connect_op::on_ready(iocp::service *svr, iocp::operation *op_,
						  const iocp::error_code &ec, size_t bytes_transferred)
{
	connect_op *op = static_cast<connect_op *>(op_);
  if (ec && ec.value() == iocp::error::epoll_flag && op->epoll_flags_ == EPOLLHUP) {
    op->ec_.set_value(0);
  	op->func_ = &connect_op::on_complete;
  	socket_ops::connect(op->socket_, op, op->addr_, op->port_);
  }
  else {
    socket_ops::end_connect(op->socket_);
    op->callback_(op->argument(), ec);
    delete op;
  }
}

void connect_op::on_complete(iocp::service *svr, iocp::operation *op_,
							 const iocp::error_code &ec, size_t bytes_transferred)
{
	connect_op *op = static_cast<connect_op *>(op_);

	socket_ops::end_connect(op->socket_);
	op->callback_(op->argument(), ec);
	delete op;
}

void send_op::begin_send()
{
  iocp::error_code ec;
  unsigned int len = buffer_.len();
  if (len > max_send_buffer_size)
    len = max_send_buffer_size;
  ec = socket_ops::send(socket_, buffer_.raw(), len);
  if (ec.value() == EAGAIN || ec.value() == EINPROGRESS) {
    sent_ = false;
    socket_ops::begin_send(socket_, this);
  }
  else {
    sent_ = true;
    socket_ops::post_operation(socket_.service(), this, ec, len);
  }
}

void send_op::on_complete(iocp::service *svr, iocp::operation *op_,
						  const iocp::error_code &ec_, size_t bytes_transferred)
{
	send_op *op = static_cast<send_op *>(op_);

	iocp::error_code ec = ec_;

  unsigned int len = op->buffer_.len();
	if (!ec && len && !op->sent_) {
		if (len > max_send_buffer_size)
			len = max_send_buffer_size;
		ec = socket_ops::send(op->socket_, op->buffer_.raw(), len);
		bytes_transferred = len;
	}

	if (!ec) {
		op->buffer_.consume(bytes_transferred);
		if (op->buffer_.len()) {
			op->reset();
			op->begin_send();
      return;
		}
	}
	else if (ec.value() == EAGAIN || ec.value() == EINPROGRESS) {
		op->reset();
		op->begin_send();
		return;
	}

	if (ec || op->buffer_.len() == 0) {
		socket_ops::end_send(op->socket_);
		op->callback_(op->argument(), ec, static_cast<size_t>(op->buffer_.raw() - op->buf_start_pos_));
		delete op;
	}
}

void recv_op::begin_recv()
{
  if (buffer_.remain()) { // we should support recv 0 bytes length, and then callback immediately
    iocp::error_code ec;
    unsigned int len = buffer_.remain();
    ec = socket_ops::recv(socket_, buffer_.raw_pos(), len);
    if (ec.value() == EAGAIN || ec.value() == EINPROGRESS) {
      recv_ = false;
      socket_ops::begin_recv(socket_, this);
    }
    else {
      recv_ = true;
      socket_ops::post_operation(socket_.service(), this, ec, len);
    }
  }
  else {
    socket_.service().post(&recv_op::on_post, this);
  }
}

void recv_op::on_complete(iocp::service *svr, iocp::operation *op_,
						  const iocp::error_code &ec_, size_t bytes_transferred)
{
	recv_op *op = static_cast<recv_op *>(op_);

	iocp::error_code ec = ec_;
  if (ec.value() == iocp::error::epoll_flag && op->epoll_flags_ == EPOLLRDHUP)
    ec.set_value(0);

	unsigned int len = op->buffer_.remain();
	if (!ec && len && !op->recv_) {
		//if (len > max_recv_buffer_size)
			//len = max_recv_buffer_size;
		ec = socket_ops::recv(op->socket_, op->buffer_.raw_pos(), len);
		bytes_transferred = len;	
	}

	if (!ec && bytes_transferred == 0 && op->buffer_.remain() > 0) {	// already end
		ec.set_value(iocp::error::eof);
		ec.set_describer(iocp::error::frame_error_describer);
	}

	if (!ec) {
		op->buffer_.append(bytes_transferred);
		if (!op->some_ && op->buffer_.remain() > 0) {
			op->reset();
			socket_ops::begin_recv(op->socket_, op);
			return;
		}
	}
  else if (ec.value() == EAGAIN || ec.value() == EINPROGRESS) {
    op->reset();
    socket_ops::begin_recv(op->socket_, op);
    return;
  }

	if (ec || op->some_ || op->buffer_.remain() == 0) {
		socket_ops::end_recv(op->socket_);
		op->callback_(op->argument(), ec, op->buffer_.len(), op->buf_start_pos_);
		delete op;
	}
}

}

#endif

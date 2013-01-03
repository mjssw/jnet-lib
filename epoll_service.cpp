#ifdef __linux__

#ifndef _IOCP_NO_INCLUDE_
#include "iocp_config.h"
#include "epoll_service.h"
#include "epoll_socket.h"
#endif
#include <stdio.h>

namespace iocp {

struct work_finished_on_block_exit
{
	~work_finished_on_block_exit()
	{
		iocp::error_code ec;
		io_service_->work_finished(ec);
	}

	iocp::service *io_service_;
};

service::service(): outstanding_work_(0), stopped_(0),
	impl_list_(0), demultiplexing_thread_(0), nthread_(0)
{
	impl_ = epoll_create(128);
}

service::~service()
{
	close(impl_);
}

class post_op: public operation
{
public:
	post_op(iocp::service::post_cb_type callback, void *binded)
		: operation(&post_op::on_complete, binded), callback_(callback) {}

private:
	static void on_complete(iocp::service *svr, iocp::operation *op_,
		const iocp::error_code &ec, size_t bytes_transferred)
	{
		post_op *op = static_cast<post_op *>(op_);
		op->callback_(op->argument(), ec);

		// commit suicide, where does asio do this?
		delete op;
	}

	iocp::service::post_cb_type callback_;
};

void service::post(service::post_cb_type callback, void *binded)
{
	work_started();
	post_op *op = new post_op(callback, binded);
	post_deferred_completion(op);
}

void service::post(service::post_cb_type callback, void *binded, const iocp::error_code &ec)
{
	work_started();
	post_op *op = new post_op(callback, binded);
	on_completion(op, ec, 0);
}

size_t service::run(iocp::error_code &ec)
{
	if (::__sync_fetch_and_add(&outstanding_work_, 0) == 0)
		stop(ec);

	size_t n = 0;
	sync_inc(&nthread_);
	while (do_one(ec))
		++n;
	sync_dec(&nthread_);
	sync_compare_and_swap(&demultiplexing_thread_, pthread_self(), 0);

	return n;
}

void service::stop(iocp::error_code &ec)
{
	if (::__sync_bool_compare_and_swap(&stopped_, 0, 1))
		post_deferred_completion(0);
}

void service::construct(iocp::base_type &impl)
{
	scope_lock sl(impl_list_lock_);
	impl.next_ = impl_list_;
	impl.prev_ = 0;
	if (impl_list_)
		impl_list_->prev_ = &impl;
	impl_list_ = &impl;
}

void service::destory(iocp::base_type &impl)
{
	scope_lock sl(impl_list_lock_);
	// Remove implementation from linked list of all implementations.
	if (impl_list_ == &impl)
		impl_list_ = impl.next_;
	if (impl.prev_)
		impl.prev_->next_ = impl.next_;
	if (impl.next_)
		impl.next_->prev_= impl.prev_;
	impl.next_ = 0;
	impl.prev_ = 0;
}

void service::post_deferred_completion(operation* op)
{
	if (op)
		op->ready_ = 1;

	completed_ops_.append(op);
}

void service::on_pending(operation *op)
{
	if (!sync_compare_and_swap(&op->ready_, 0, 1))
		post_deferred_completion(op);
}

void service::on_completion(operation *op, const iocp::error_code &ec, unsigned int bytes_transferred)
{
	op->ec_ = ec;
	op->bytes_transferred_ = bytes_transferred;
	post_deferred_completion(op);
}

int service::get_epoll_timeout()
{

	if (sync_get(&nthread_) == 1) {
		int ret = completed_ops_.test();
		if (ret == 0)
			return 100;
		else
			return 0;
	}
	return 0;
}

int service::get_op_timeout(bool epoll_thread)
{
	if (epoll_thread)
		return 1;
	else
		return 100;
}

size_t service::do_one(iocp::error_code &ec)
{
	pthread_t thd = pthread_self();

	for (;;)
	{
		int epoll_wait_timeout, op_wait_timeout;
		pthread_t oldthd = ::__sync_val_compare_and_swap(&demultiplexing_thread_, 0, thd);
		bool need_epoll_wait = (oldthd == 0 || oldthd == thd);

		if (need_epoll_wait)
			epoll_wait_timeout = get_epoll_timeout();
		op_wait_timeout = get_op_timeout(need_epoll_wait);

		if (need_epoll_wait) {
			int ret = epoll_wait(impl_, events_, 128, epoll_wait_timeout);
			if (ret > 0) {
				int i;
				for (i = 0; i < ret; ++i) {
					operation *epoll_op = NULL;
					iocp::socket_base *s = (iocp::socket_base *)events_[i].data.ptr;

					if (events_[i].events & EPOLLIN) {
						epoll_op = (iocp::operation *)::__sync_fetch_and_and(&s->in_op_, (void *)0);
						if (events_[i].events & EPOLLERR) {
							unsigned error;
							so_error(s->socket_, error);
							epoll_op->ec_ = iocp::error_code(error, iocp::error::system_error_despcriber);
						}
						clear_epollin(s->socket_, *s);
						if (epoll_op) {
							epoll_op->epoll_flags_ = events_[i].events&(EPOLLERR|EPOLLHUP|EPOLLRDHUP);
							if (epoll_op->epoll_flags_ && !(epoll_op->epoll_flags_&EPOLLERR))
								epoll_op->ec_ = iocp::error_code(iocp::error::epoll_flag, iocp::error::frame_error_describer);
							post_deferred_completion(epoll_op);
							//on_pending(epoll_op);
						}
						else
							puts("in fatal error!!!!!!!!!!!");
					}
					if (events_[i].events & EPOLLOUT) {
						epoll_op = (iocp::operation *)::__sync_fetch_and_and(&s->out_op_, (void *)0);
						if (events_[i].events & EPOLLERR) {
							unsigned error;
							so_error(s->socket_, error);
							epoll_op->ec_ = iocp::error_code(error, iocp::error::system_error_despcriber);
						}
						clear_epollout(s->socket_, *s);
						if (epoll_op) {
							epoll_op->epoll_flags_ = events_[i].events&(EPOLLERR|EPOLLHUP|EPOLLRDHUP);
							if (epoll_op->epoll_flags_ && !(epoll_op->epoll_flags_&EPOLLERR))
								epoll_op->ec_ = iocp::error_code(iocp::error::epoll_flag, iocp::error::frame_error_describer);
							post_deferred_completion(epoll_op);
							//on_pending(epoll_op);
						}
						else
							puts("out fatal error!!!!!!!!!!!");
					}
					if (events_[i].events & EPOLLPRI)
						puts("epollpri event");
					if (!(events_[i].events & EPOLLIN) && !(events_[i].events & EPOLLOUT) && !(events_[i].events & EPOLLPRI)) {
						//puts("events without in & out & pri");
					}
				}
			}
			/*else if (ret == 0) {
				if (op_wait_timeout > 0)
					continue;
			}*/
			else if (ret < 0 && errno != EINTR) {
				// ignore error when epoll_wait was interrupted by a signal
				ec = iocp::error_code(errno, iocp::error::system_error_despcriber);
				return 0;
			}
		}

		int nop = 0;
		operation *op = NULL;
		nop = completed_ops_.wait(op, op_wait_timeout);

		if (op) {
			if (!sync_compare_and_swap(&op->ready_, 0, 1)) {
				// Ensure the count of outstanding work is decremented on block exit.
				work_finished_on_block_exit on_exit = { this };
				(void)on_exit;
				// for exception?

				op->complete(*this, op->ec_, op->bytes_transferred_);
				ec = iocp::error_code();	// no error
				return 1;
			}
		}
		else if (nop == 0) {
			// timeout
			continue;
		}
		else {
			if (nop < 0)
				return 0;
			// The stopped_ flag is always checked to ensure that any leftover
			// interrupts from a previous run invocation are ignored.
			if (sync_get(&stopped_) != 0)
			{
				// Wake up next thread that is blocked on epoll_wait.
				post_deferred_completion(0);
				ec = iocp::error_code();
				return 0;
			}
		}
	}

	return 0;
}

iocp::error_code service::register_event(int fd, struct epoll_event &event)
{
	iocp::error_code ec(0, iocp::error::system_error_despcriber);
	if (::epoll_ctl(impl_, EPOLL_CTL_ADD, fd, &event))
		ec.set_value(errno);
	return ec;
}

iocp::error_code service::modify_event(int fd, struct epoll_event &event)
{
	iocp::error_code ec(0, iocp::error::system_error_despcriber);
	if (::epoll_ctl(impl_, EPOLL_CTL_MOD, fd, &event))
		ec.set_value(errno);
	return ec;
}

iocp::error_code service::delete_event(int fd, struct epoll_event &event)
{
	iocp::error_code ec(0, iocp::error::system_error_despcriber);
	if (::epoll_ctl(impl_, EPOLL_CTL_DEL, fd, &event))
		ec.set_value(errno);
	return ec;
}

bool service::wait_for_epollin(int fd, struct epoll_event &event, iocp::operation *op)
{
	iocp::error_code ec;
	if (!(::__sync_fetch_and_or(&event.events, EPOLLIN) & EPOLLIN))
		ec = modify_event(fd, event);
	else
		puts("already has in flag");
	if (ec)
		on_completion(op, ec);
	//else
		//on_pending(op);
	if (ec)
		return false;
	return true;
}

bool service::wait_for_epollout(int fd, struct epoll_event &event, iocp::operation *op)
{
	iocp::error_code ec;
	if (!(::__sync_fetch_and_or(&event.events, EPOLLOUT) & EPOLLOUT))
		ec = modify_event(fd, event);
	else
		puts("already has out flag");
	if (ec)
		on_completion(op, ec);
	//else
		//on_pending(op);
	if (ec)
		return false;
	return true;
}

void service::clear_epollin(int fd, struct epoll_event &event)
{
	if ((::__sync_fetch_and_and(&event.events, ~EPOLLIN) & EPOLLIN))
		modify_event(fd, event);
}

void service::clear_epollout(int fd, struct epoll_event &event)
{
	if ((::__sync_fetch_and_and(&event.events, ~EPOLLOUT) & EPOLLOUT))
		modify_event(fd, event);
}

// socket service
iocp::error_code service::so_error(int sock, unsigned int &error)
{
	iocp::error_code ec(0, iocp::error::system_error_despcriber);
	error= 0;
	socklen_t len = sizeof(error);
	if (::getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len));
		ec.set_value(errno);
	return ec;
}

iocp::error_code service::socket(int &sock)
{
	iocp::error_code ec(0, iocp::error::system_error_despcriber);
	sock = ::socket(AF_INET, SOCK_STREAM|SOCK_NONBLOCK, IPPROTO_TCP);
	if (sock == -1)
		ec.set_value(errno);
	return ec;
}

iocp::error_code service::close_socket(int &sock)
{
	iocp::error_code ec(0, iocp::error::system_error_despcriber);

	if (::close(sock) < 0)
		ec.set_value(errno);

	if (ec.value() == EWOULDBLOCK)
	{
		// According to UNIX Network Programming Vol. 1, it is possible for
		// close() to fail with EWOULDBLOCK under certain circumstances. What
		// isn't clear is the state of the descriptor after this error. The one
		// current OS where this behaviour is seen, Windows, says that the socket
		// remains open. Therefore we'll put the descriptor back into blocking
		// mode and have another attempt at closing it.
	}
	sock = -1;

	return ec;
}

iocp::error_code service::bind(int socket, const iocp::address &addr, unsigned short port)
{
	iocp::error_code ec(0, iocp::error::system_error_despcriber);
	struct sockaddr_in sockaddr;
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_addr = (struct in_addr)addr;
	sockaddr.sin_port = htons(port);
	if (::bind(socket, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) != 0)
		ec.set_value(errno);

	return ec;
}

iocp::error_code service::listen(int socket, int backlog)
{
	iocp::error_code ec(0, iocp::error::system_error_despcriber);
	if (::listen(socket, backlog) != 0)
		ec.set_value(errno);

	return ec;
}

iocp::error_code service::accept(int acceptor, int &socket)
{
	int accepted;
	struct sockaddr_in svr;
	socklen_t svrlen = sizeof(svr);
	errno = 0;
	accepted = ::accept4(acceptor, (struct sockaddr *)&svr, &svrlen, SOCK_NONBLOCK);
	if (accepted >= 0)
		socket = accepted;
	return iocp::error_code(errno, iocp::error::system_error_despcriber);
}

iocp::error_code service::connect(int fd, const iocp::address &addr, unsigned short port)
{
	int ret;
	struct sockaddr_in svr;
	svr.sin_family = AF_INET;
	svr.sin_addr.s_addr = ((struct in_addr)addr).s_addr;
	svr.sin_port = ::htons(port);

	iocp::error_code ec(0, iocp::error::system_error_despcriber);
	ret = ::connect(fd, (struct sockaddr *)&svr, sizeof(svr));
	if (ret < 0 && errno != EINPROGRESS)
		ec.set_value(errno);
	return ec;
}

iocp::error_code service::send(int socket, const char *buffer, unsigned int &len)
{
	errno = 0;
	ssize_t ret = ::send(socket, buffer, len, 0);
	if (ret >= 0)
		len = ret;
	return iocp::error_code(errno, iocp::error::system_error_despcriber);
}

iocp::error_code service::recv(int socket, char *buffer, unsigned int &len)
{
	errno = 0;
	ssize_t ret = ::recv(socket, buffer, len, 0);
	if (ret >= 0)
		len = ret;
	return iocp::error_code(errno, iocp::error::system_error_despcriber);
}

// file service
iocp::error_code service::file(int &fd,
															 const char *file_name,
															 int access_mode,
															 int share_mode,
															 int creation_mode)
{
	iocp::error_code ec(0, iocp::error::system_error_despcriber);
	fd = ::open(file_name, access_mode|creation_mode, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	if (fd == -1)
		ec.set_value(errno);
	if (ec.value() == EEXIST) {
		ec.set_value(iocp::error::already_exists);
		ec.set_describer(iocp::error::frame_error_describer);
	}
	// TODO: share_mode
	return ec;
}

iocp::error_code service::close_file(int &fd)
{
	iocp::error_code ec(0, iocp::error::system_error_despcriber);
	if (::close(fd) == -1)
		ec.set_value(errno);
	fd = -1;
	return ec;
}

iocp::error_code service::seek(int fd, long long distance,
															 unsigned long long *new_point, int wherece)
{
	long long ret;
	iocp::error_code ec(0, iocp::error::system_error_despcriber);
	if ((ret = ::lseek(fd, distance, wherece)) == -1)
		ec.set_value(errno);
	else if (new_point)
		*new_point = ret;
	return ec;
}

iocp::error_code service::length(int fd, unsigned long long *len)
{
	struct stat st;
	iocp::error_code ec(0, iocp::error::system_error_despcriber);
	if (::fstat(fd, &st) == -1)
		ec.set_value(errno);
	else
		*len = st.st_size;
	return ec;
}

iocp::error_code service::set_eof(int fd)
{
	off_t off;
	iocp::error_code ec(0, iocp::error::system_error_despcriber);
	off = ::lseek(fd, 0, SEEK_CUR);
	if (off == -1)
		ec.set_value(errno);
	if (off != -1) {
		if (ftruncate(fd, off)) {
			ec.set_value(errno);
		}
	}
	return ec;
}

iocp::error_code service::flush(int fd)
{
	iocp::error_code ec(0, iocp::error::system_error_despcriber);
	if (::fsync(fd))
		ec.set_value(errno);
	return ec;
}

iocp::error_code service::write(int fd,
																const char *buffer, size_t &bytes)
{
	ssize_t ret;
	iocp::error_code ec(0, iocp::error::system_error_despcriber);
	if (bytes > SSIZE_MAX)
		bytes = SSIZE_MAX;
	ret = ::write(fd, buffer, bytes);
	if (ret < 0)
		ec.set_value(errno);
	else
		bytes = ret;

	return ec;
}

iocp::error_code service::read(int fd,
															 char *buffer, size_t &bytes)
{
	ssize_t ret;
	iocp::error_code ec(0, iocp::error::system_error_despcriber);
	if (bytes > SSIZE_MAX)
		bytes = SSIZE_MAX;
	ret = ::read(fd, buffer, bytes);
	if (ret < 0)
		ec.set_value(errno);
	else
		bytes = ret;

	return ec;
}

}

#endif
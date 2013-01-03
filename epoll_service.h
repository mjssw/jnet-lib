#ifdef __linux__

#ifndef _EPOLL_SERVICE_H_
#define _EPOLL_SERVICE_H_

#ifndef _IOCP_NO_INCLUDE_
#include "noncopyable.h"
#include "iocp_lock.h"
#include "iocp_operation.h"
#include "iocp_error.h"
#include "iocp_base_type.h"
#include "iocp_address.h"
#endif

#include <list>

namespace iocp {

class my_semaphore
{
public:
	my_semaphore()
	{
    ::pthread_cond_init(&cond_, NULL);
    ::pthread_mutex_init(&mutex_, NULL);
	}

	~my_semaphore()
	{
    ::pthread_cond_destroy(&cond_);
    ::pthread_mutex_destroy(&mutex_);
	}

	int test()
	{
		int ret;
		::pthread_mutex_lock(&mutex_);
		ret = completed_ops_.size();
		::pthread_mutex_unlock(&mutex_);
		return ret;
	}

	int wait(operation *&op, int timeout = -1)
	{
    int ret = 0;
    op = 0;
    ::pthread_mutex_lock(&mutex_);

    if (timeout == -1) {
      while (completed_ops_.size() == 0 && ret == 0)
        ret = ::pthread_cond_wait(&cond_, &mutex_);
    }
    else if (timeout == 0) {
    	ret = completed_ops_.size();
    	if (ret) {
    		op = completed_ops_.front();
    		completed_ops_.pop_front();
    	}
    	::pthread_mutex_unlock(&mutex_);
    	return ret;
    }
    else {
      struct timeval tv;
      struct timespec ts;
      ::gettimeofday(&tv, NULL);
      ts.tv_nsec = tv.tv_usec * 1000UL + (timeout % 1000UL) * 1000000UL;
      ts.tv_sec = tv.tv_sec + timeout / 1000UL + ts.tv_nsec / 1000000000UL;
      ts.tv_nsec = ts.tv_nsec % 1000000000UL;
      while (completed_ops_.size() == 0 && ret == 0)
        ret = ::pthread_cond_timedwait(&cond_, &mutex_, &ts);
    }

    if (ret == 0) {
    	op = completed_ops_.front();
    	completed_ops_.pop_front();
    }

    if (ret == ETIMEDOUT)
      ret = 0;
    else if (ret == 0)
      ret = completed_ops_.size()+1;
    else
      ret = -1;

    ::pthread_mutex_unlock(&mutex_);

    return ret;
	}

	int append(operation *op)
	{
    ::pthread_mutex_lock(&mutex_);
    completed_ops_.push_back(op);
    ::pthread_mutex_unlock(&mutex_);
    ::pthread_cond_broadcast(&cond_);
		return 0;
	}

private:
  pthread_cond_t  cond_;
  pthread_mutex_t mutex_;
  std::list<operation *> completed_ops_;
};

class resolver;
class service: private noncopyable
{
public:
	service();
	~service();

	typedef void (*post_cb_type)(void *, const iocp::error_code &);
	void post(post_cb_type callback, void *binded);
	void post(post_cb_type callback, void *binded, const iocp::error_code &ec);

	// Notify that some work has started.
	void work_started()
	{
		::__sync_fetch_and_add(&outstanding_work_, 1);
	}

	// Notify that some work has finished.
	void work_finished(iocp::error_code &ec)
	{
		if (::__sync_sub_and_fetch(&outstanding_work_, 1) == 0)
			stop(ec);
	}

	void stop(iocp::error_code &ec);
	size_t run(iocp::error_code &ec);

private:
	iocp::error_code register_event(int fd, struct epoll_event &event);
	iocp::error_code modify_event(int fd, struct epoll_event &event);
	iocp::error_code delete_event(int fd, struct epoll_event &event);

	bool wait_for_epollin(int fd, struct epoll_event &event, iocp::operation *op);
	bool wait_for_epollout(int fd, struct epoll_event &event, iocp::operation *op);
	void clear_epollin(int fd, struct epoll_event &event);
	void clear_epollout(int fd, struct epoll_event &event);

	// socket service
	iocp::error_code so_error(int sock, unsigned int &error);
	iocp::error_code socket(int &sock);
	iocp::error_code close_socket(int &sock);
	iocp::error_code bind(int sock, const iocp::address &addr, unsigned short port);
	iocp::error_code listen(int sock, int backlog);
	iocp::error_code accept(int acceptor, int &sock);
	iocp::error_code connect(int sock, const iocp::address &addr, unsigned short port);
	iocp::error_code send(int sock, const char *buffer, unsigned int &len);
	iocp::error_code recv(int sock, char *buffer, unsigned int &len);

	// file service
	iocp::error_code file(int &fd, const char *file_name,\
		int access_mode, int share_mode, int creation_mode);
	iocp::error_code close_file(int &fd);

	iocp::error_code seek(int fd, long long distance,
		unsigned long long *new_point, int wherece);
	iocp::error_code length(int fd, unsigned long long *len);
	iocp::error_code set_eof(int fd);
	iocp::error_code flush(int fd);

	iocp::error_code write(int fd, const char *buffer, size_t &bytes);
	iocp::error_code read(int fd, char *buffer, size_t &bytes);

	// service
	void construct(iocp::base_type &impl);
	void destory(iocp::base_type &impl);

	size_t do_one(iocp::error_code &ec);

	// Request invocation of the given operation and return immediately. Assumes
	// that work_started() was previously called for the operation.
	void post_deferred_completion(operation* op);

	// Called after starting an overlapped I/O operation that did not complete
	// immediately. The caller must have already called work_started() prior to
	// starting the operation.
	void on_pending(operation *op);

	// Called after starting an overlapped I/O operation that completed
	// immediately. The caller must have already called work_started() prior to
	// starting the operation.
	void on_completion(operation *op, const iocp::error_code &ec, unsigned int bytes_transferred = 0);

	friend class base_type;
	friend class socket_ops;
	friend class file_ops;
	friend class resolver;

	int impl_;

	long outstanding_work_;
	long stopped_;
	long shutdown_;

	iocp::mutex impl_list_lock_;
	iocp::base_type *impl_list_;

	my_semaphore completed_ops_;

	struct epoll_event events_[128];

	int get_epoll_timeout();
	int get_op_timeout(bool epoll_thread);
	long nthread_;
	pthread_t demultiplexing_thread_;
};

}

#endif // _EPOLL_SERVICE_H_

#endif

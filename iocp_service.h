// iocp_service.h
#ifdef _WIN32

#ifndef _IOCP_NO_INCLUDE_
#pragma once
#include "iocp_config.h"
#include "noncopyable.h"

#include "iocp_error.h"
#include "iocp_address.h"
#include "iocp_base_type.h"
#include "iocp_operation.h"
#include "iocp_lock.h"
#endif

#include <list>

namespace iocp {

class socket_ops;
// include iocp service according to implementation of asio
class service: private noncopyable
{
public:

	service(unsigned long concurrency_hint = 0UL);
	~service();

	typedef void (*post_cb_type)(void *, const iocp::error_code &);
	void post(post_cb_type callback, void *binded);
	void post(post_cb_type callback, void *binded, const iocp::error_code &ec);

	size_t run(iocp::error_code &ec);

	// Stop the event processing loop.
	void stop(iocp::error_code &ec);

	// Determine whether the io_service is stopped.
	bool stopped() const
	{
		return ::InterlockedExchangeAdd(&stopped_, 0) != 0;
	}

	// Reset in preparation for a subsequent run invocation.
	void reset()
	{
		::InterlockedExchange(&stopped_, 0);
	}

	// Notify that some work has started.
	void work_started()
	{
		::InterlockedIncrement(&outstanding_work_);
	}

	// Notify that some work has finished.
	void work_finished(iocp::error_code &ec)
	{
		if (::InterlockedDecrement(&outstanding_work_) == 0)
			stop(ec);
	}

private:
	enum
	{
		// Timeout to use with GetQueuedCompletionStatus. Some versions of windows
		// have a "bug" where a call to GetQueuedCompletionStatus can appear stuck
		// even though there are events waiting on the queue. Using a timeout helps
		// to work around the issue.
		gqcs_timeout = 500,
		// Completion key value to indicate that an operation has posted with the
		// original last_error and bytes_transferred values stored in the fields of
		// the OVERLAPPED structure.
		overlapped_contains_result = 1,
		// Completion key value used to wake up a thread to dispatch timers or
		// completed operations.
		wake_for_dispatch = 2
	};

	iocp::error_code register_handle(HANDLE handle);

	// socket service
	iocp::error_code socket(SOCKET &sock);
	iocp::error_code close_socket(SOCKET &sock);
  iocp::error_code shutdown(SOCKET &sock, int type);

  iocp::error_code getsockname(SOCKET &sock, iocp::address &addr, unsigned short &port);
  iocp::error_code getpeername(SOCKET &sock, iocp::address &addr, unsigned short &port);

	iocp::error_code bind(SOCKET socket, const iocp::address &addr, unsigned short port);
	iocp::error_code listen(SOCKET socket, int backlog);

	void accept(SOCKET acceptor,
		iocp::operation *op, SOCKET socket, char *buffer);
	void connect(SOCKET socket,
		iocp::operation *op, const iocp::address &addr, unsigned short port);
	void send(SOCKET socket,
		iocp::operation *op, WSABUF *buffers, size_t buffer_count = 1);
	void recv(SOCKET socket,
		iocp::operation *op, WSABUF *buffers, size_t buffer_count = 1);

	// file service
	iocp::error_code file(HANDLE &handle, const char *file_name,\
		DWORD access_mode, DWORD share_mode, DWORD creation_mode);
	iocp::error_code close_file(HANDLE &handle);

	iocp::error_code seek(HANDLE handle, __int64 distance,
		unsigned __int64 *new_point, DWORD move_method);
	iocp::error_code length(HANDLE handle, unsigned __int64 *len);
	iocp::error_code set_eof(HANDLE handle);
	iocp::error_code flush(HANDLE handle);

	void write(HANDLE handle,
		iocp::operation *op, LPCVOID buffer, size_t bytes);
	void read(HANDLE handle,
		iocp::operation *op, LPVOID buffer, size_t bytes);


	// service
	void construct(iocp::base_type &impl);
	void destory(iocp::base_type &impl);

	size_t do_one(iocp::error_code &ec);

	// Request invocation of the given operation and return immediately. Assumes
	// that work_started() was previously called for the operation.
	void post_deferred_completion(operation* op);

	// Request invocation of the given operation and return immediately. Assumes
	// that work_started() was previously called for the operations.
	void post_deferred_completions(std::list<operation *>& ops);

	// Called after starting an overlapped I/O operation that did not complete
	// immediately. The caller must have already called work_started() prior to
	// starting the operation.
	void on_pending(operation *op);

	// Called after starting an overlapped I/O operation that completed
	// immediately. The caller must have already called work_started() prior to
	// starting the operation.
	void on_completion(operation *op, const iocp::error_code &ec, DWORD bytes_transferred = 0);

private:
	friend class base_type;
	friend class socket_ops;
	friend class file_ops;
	friend class resolver;

	// The IO completion port used for queuing operations.
	HANDLE handle_;

	// The count of unfinished work.
	long outstanding_work_;

	// Flag to indicate whether the event loop has been stopped.
	mutable long stopped_;

	// Flag to indicate whether the service has been shut down.
	long shutdown_;

	// all sockets
	iocp::mutex impl_list_cs_;
	iocp::base_type *impl_list_;

	// Non-zero if timers or completed operations need to be dispatched.
	long dispatch_required_;

	// The operations that are ready to dispatch.
	iocp::mutex completed_ops_cs_;
	std::list<operation *> completed_ops_;
};

}

#endif

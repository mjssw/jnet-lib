// iocp_service.cpp
#ifdef _WIN32
#ifndef _IOCP_NO_INCLUDE_
#include "iocp_service.h"
#endif

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mswsock.lib")

namespace iocp {

// not very clearly understand the implementation in asio winsock_init.hpp
// maybe we do not need variable to ensure that winsock only starts-up once
class winsock_init
{
public:
	winsock_init();
	~winsock_init();
};

winsock_init::winsock_init()
{
	WSADATA wsa_data;
	if (::WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
		exit(-1);
}

winsock_init::~winsock_init()
{
	::WSACleanup();
}

static const winsock_init winsock_init_instance;

service::service(unsigned long concurrency_hint): impl_list_(0),
																									outstanding_work_(0),
																								  stopped_(0),
																									shutdown_(0)
{
	handle_ = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, concurrency_hint);
}

service::~service()
{
	::CloseHandle(handle_);
}

// ioservice

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

void service::post(post_cb_type callback, void *binded, const iocp::error_code &ec)
{
	work_started();
	post_op *op = new post_op(callback, binded);
	on_completion(op, ec, 0);
}

size_t service::run(iocp::error_code &ec)
{
	if (::InterlockedExchangeAdd(&outstanding_work_, 0) == 0)
		stop(ec);

// 	call_stack<win_iocp_io_service>::context ctx(this);

	size_t n = 0;
	while (do_one(ec))
		++n;

	return n;
}

void service::stop(iocp::error_code &ec)
{
	if (::InterlockedExchange(&stopped_, 1) == 0)
	{
		if (!::PostQueuedCompletionStatus(handle_, 0, 0, 0))
		{
			ec = iocp::error_code(::GetLastError(),
				iocp::error::system_error_despcriber);
		}
	}
}

void service::construct(iocp::base_type &impl)
{
	scope_lock sl(impl_list_cs_);
	impl.next_ = impl_list_;
	impl.prev_ = 0;
	if (impl_list_)
		impl_list_->prev_ = &impl;
	impl_list_ = &impl;
}

void service::destory(iocp::base_type &impl)
{
	scope_lock sl(impl_list_cs_);
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

struct work_finished_on_block_exit
{
	~work_finished_on_block_exit()
	{
		io_service_->work_finished(iocp::error_code());
	}

	iocp::service* io_service_;
};

size_t service::do_one(iocp::error_code &ec)
{
	for (;;)
	{
		// Try to acquire responsibility for dispatching timers and completed ops.
		if (::InterlockedCompareExchange(&dispatch_required_, 0, 1) == 1)
		{
			iocp::scope_lock lock(completed_ops_cs_);

			// Dispatch pending /*timers and*/ operations.
			std::list<operation *> ops;
			std::list<operation *>::iterator it;
			for (it = completed_ops_.begin(); it != completed_ops_.end(); ++it)
				ops.push_back(*it);
			completed_ops_.clear();
			post_deferred_completions(ops);
		}

		DWORD bytes_transferred = 0;
		DWORD_PTR completion_key = 0;
		LPOVERLAPPED overlapped = 0;
		::SetLastError(0);
		BOOL ok = ::GetQueuedCompletionStatus(handle_, &bytes_transferred,
			&completion_key, &overlapped, gqcs_timeout);
		DWORD last_error = ::GetLastError();

		if (overlapped)
		{
			operation* op = static_cast<operation *>(overlapped);
			iocp::error_code result_ec(last_error, iocp::error::system_error_despcriber);

			// We may have been passed the last_error and bytes_transferred in the
			// OVERLAPPED structure itself.
			if (completion_key == overlapped_contains_result)
			{
				result_ec = iocp::error_code(static_cast<int>(op->Offset),
					reinterpret_cast<iocp::error::describer_type>(op->Internal));
				bytes_transferred = op->OffsetHigh;
			}
			// Otherwise ensure any result has been saved into the OVERLAPPED
			// structure.
			else
			{
				op->Internal = reinterpret_cast<ULONG_PTR>(result_ec.describer());
				op->Offset = result_ec.value();
				op->OffsetHigh = bytes_transferred;
			}

			// Dispatch the operation only if ready. The operation may not be ready
			// if the initiating function (e.g. a call to WSARecv) has not yet
			// returned. This is because the initiating function still wants access
			// to the operation's OVERLAPPED structure.
			if (::InterlockedCompareExchange(&op->ready_, 1, 0) == 1)
			{
				// Ensure the count of outstanding work is decremented on block exit.
				work_finished_on_block_exit on_exit = { this };
				(void)on_exit;
				// for exception?

 				op->complete(*this, result_ec, bytes_transferred);
 				ec = iocp::error_code();	// no error
				return 1;
			}
		}
		else if (!ok)
		{
			if (last_error != WAIT_TIMEOUT)
			{
				ec = iocp::error_code(last_error, iocp::error::system_error_despcriber);
				return 0;
			}

			continue;
		}
		else if (completion_key == wake_for_dispatch)
		{
			// We have been woken up to try to acquire responsibility for dispatching
			// timers and completed operations.
		}
		else
		{
			// The stopped_ flag is always checked to ensure that any leftover
			// interrupts from a previous run invocation are ignored.
			if (::InterlockedExchangeAdd(&stopped_, 0) != 0)
			{
				// Wake up next thread that is blocked on GetQueuedCompletionStatus.
				if (!::PostQueuedCompletionStatus(handle_, 0, 0, 0))
				{
					ec = iocp::error_code(::GetLastError(),
						iocp::error::system_error_despcriber);
					return 0;
				}

				ec = iocp::error_code();
				return 0;
			}
		}
	}

	return 0;
}

void service::post_deferred_completion(operation* op)
{
	// Flag the operation as ready.
	op->ready_ = 1;

	// Enqueue the operation on the I/O completion port.
	if (!::PostQueuedCompletionStatus(handle_,
		0, overlapped_contains_result, op))
	{
		// Out of resources. Put on completed queue instead.
		iocp::scope_lock lock(completed_ops_cs_);
		completed_ops_.push_back(op);
		::InterlockedExchange(&dispatch_required_, 1);
	}
}

void service::post_deferred_completions(std::list<operation *> &ops)
{
	while (!ops.empty())
	{
		operation* op = ops.front();
		ops.pop_front();

		// Flag the operation as ready.
		op->ready_ = 1;

		// Enqueue the operation on the I/O completion port.
		if (!::PostQueuedCompletionStatus(handle_,
			0, overlapped_contains_result, op))
		{
			// Out of resources. Put on completed queue instead.
			iocp::scope_lock lock(completed_ops_cs_);
			completed_ops_.push_back(op);
			std::list<operation *>::iterator it;
			for (it = ops.begin(); it != ops.end(); ++it)
				completed_ops_.push_back(*it);
			ops.clear();
			::InterlockedExchange(&dispatch_required_, 1);
		}
	}
}

void service::on_pending(operation *op)
{
	if (::InterlockedCompareExchange(&op->ready_, 1, 0) == 1)
	{
		// result already stored in do_one()
		// Enqueue the operation on the I/O completion port.
		if (!::PostQueuedCompletionStatus(handle_,
			0, overlapped_contains_result, op))
		{
			// Out of resources. Put on completed queue instead.
			iocp::scope_lock lock(completed_ops_cs_);
			completed_ops_.push_back(op);
			::InterlockedExchange(&dispatch_required_, 1);
		}
	}
}

void service::on_completion(operation *op, const iocp::error_code &ec, DWORD bytes_transferred)
{
	// Flag that the operation is ready for invocation.
	op->ready_ = 1;

	// Store results in the OVERLAPPED structure.
	op->Internal = reinterpret_cast<ULONG_PTR>(ec.describer());
	op->Offset = ec.value();
	op->OffsetHigh = bytes_transferred;

	// Enqueue the operation on the I/O completion port.
	if (!::PostQueuedCompletionStatus(handle_,
		0, overlapped_contains_result, op))
	{
		// Out of resources. Put on completed queue instead.
		iocp::scope_lock lock(completed_ops_cs_);
		completed_ops_.push_back(op);
		::InterlockedExchange(&dispatch_required_, 1);
	}
}

iocp::error_code service::register_handle(HANDLE handle)
{
	error_code ec(0, iocp::error::system_error_despcriber);
	if (::CreateIoCompletionPort(handle, handle_, 0, 0) == 0)
		ec.set_value(::GetLastError());

	return ec;
}

// socket service
iocp::error_code service::socket(SOCKET &sock)
{
	iocp::error_code ec(0, iocp::error::system_error_despcriber);
	sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0,
		WSA_FLAG_OVERLAPPED);
	if (sock == INVALID_SOCKET)
		ec.set_value(::WSAGetLastError());
	return ec;
}

iocp::error_code service::close_socket(SOCKET &sock)
{
	iocp::error_code ec(0, iocp::error::system_error_despcriber);
	::WSASetLastError(0);
	int result = ::closesocket(sock);
	ec.set_value(::WSAGetLastError());

	if (result != 0 && ec.value() == WSAEWOULDBLOCK)
	{
		// According to UNIX Network Programming Vol. 1, it is possible for
		// close() to fail with EWOULDBLOCK under certain circumstances. What
		// isn't clear is the state of the descriptor after this error. The one
		// current OS where this behaviour is seen, Windows, says that the socket
		// remains open. Therefore we'll put the descriptor back into blocking
		// mode and have another attempt at closing it.
		unsigned long arg = 0;
		::ioctlsocket(sock, FIONBIO, &arg);

		::WSASetLastError(0);
		result = ::closesocket(sock);
		ec.set_value(::WSAGetLastError());
	}
	sock = INVALID_SOCKET;

	return ec;
}

iocp::error_code service::shutdown(SOCKET &sock, int type)
{
  iocp::error_code ec;
  if (::shutdown(sock, type)) {
    ec.set_value(::WSAGetLastError());
    ec.set_describer(iocp::error::system_error_despcriber);
  }
  return ec;
}

iocp::error_code service::getsockname(SOCKET &sock, iocp::address &addr, unsigned short &port)
{
  iocp::error_code ec;
  struct sockaddr_in sockaddr;
  int socklen = sizeof(sockaddr);
  if (::getsockname(sock, (struct sockaddr *)&sockaddr, &socklen)) {
    ec.set_value(::WSAGetLastError());
    ec.set_describer(iocp::error::system_error_despcriber);
  }
  else {
    addr = *(unsigned int *)&sockaddr.sin_addr;
    port = ::ntohs(sockaddr.sin_port);
  }

  return ec;
}

iocp::error_code service::getpeername(SOCKET &sock, iocp::address &addr, unsigned short &port)
{
  iocp::error_code ec;
  struct sockaddr_in sockaddr;
  int socklen = sizeof(sockaddr);
  if (::getpeername(sock, (struct sockaddr *)&sockaddr, &socklen)) {
    ec.set_value(::WSAGetLastError());
    ec.set_describer(iocp::error::system_error_despcriber);
  }
  else {
    addr = *(unsigned int *)&sockaddr.sin_addr;
    port = ::ntohs(sockaddr.sin_port);
  }

  return ec;
}

iocp::error_code service::bind(SOCKET socket, const iocp::address &addr, unsigned short port)
{
	iocp::error_code ec(0, iocp::error::system_error_despcriber);
	SOCKADDR_IN sockaddr;
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_addr = (IN_ADDR)addr;
	sockaddr.sin_port = htons(port);
	if (::bind(socket, (SOCKADDR *)&sockaddr, sizeof(sockaddr)) != 0)
		ec.set_value(::WSAGetLastError());

	return ec;
}

iocp::error_code service::listen(SOCKET socket, int backlog)
{
	iocp::error_code ec(0, iocp::error::system_error_despcriber);
	if (::listen(socket, backlog) != 0)
		ec.set_value(::WSAGetLastError());

	return ec;
}

void service::accept(SOCKET acceptor,
										 iocp::operation *op,
										 SOCKET socket, char *buffer)
{
	static LPFN_ACCEPTEX lpfnAcceptEx = 0;
	static GUID GuidAcceptEx = WSAID_ACCEPTEX;

	DWORD dwBytes;
	if (!lpfnAcceptEx && ::WSAIoctl(acceptor,
		SIO_GET_EXTENSION_FUNCTION_POINTER, &GuidAcceptEx, 
		sizeof(GuidAcceptEx), &lpfnAcceptEx, sizeof(lpfnAcceptEx),
		&dwBytes, NULL, NULL) != 0)
	{
		on_completion(op, iocp::error_code(::GetLastError(), iocp::error::system_error_despcriber));
		return;
	}

	BOOL result = lpfnAcceptEx(acceptor, socket, buffer, 0,
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, &dwBytes, op);
	DWORD last_error = ::WSAGetLastError();
	if (result == FALSE && last_error != WSA_IO_PENDING)
		on_completion(op, iocp::error_code(last_error, iocp::error::system_error_despcriber));
	else
		on_pending(op);
}

void service::connect(SOCKET socket,
											iocp::operation *op,
											const iocp::address &addr, unsigned short port)
{
	static LPFN_CONNECTEX lpfnConnectEx = 0;
	static GUID GuidConnectEx = WSAID_CONNECTEX;

	DWORD dwBytes;
	if (!lpfnConnectEx && ::WSAIoctl(socket,
		SIO_GET_EXTENSION_FUNCTION_POINTER, &GuidConnectEx,
		sizeof(GuidConnectEx), &lpfnConnectEx, sizeof(lpfnConnectEx),
		&dwBytes, NULL, NULL) != 0)
	{
		on_completion(op, iocp::error_code(::GetLastError(), iocp::error::system_error_despcriber));
		return;
	}

	SOCKADDR_IN svr;
	svr.sin_family = AF_INET;
	svr.sin_addr = (IN_ADDR)addr;
	svr.sin_port = ::htons(port);

	BOOL result = lpfnConnectEx(socket, (SOCKADDR*)&svr,
		sizeof(svr), 0, 0, 0, op);
	DWORD last_error = ::WSAGetLastError();
	if (result == FALSE && last_error != WSA_IO_PENDING)
		on_completion(op, iocp::error_code(last_error, iocp::error::system_error_despcriber));
	else
		on_pending(op);
}

void service::send(SOCKET socket,
									 iocp::operation *op,
									 WSABUF *buffers, size_t buffer_count)
{
	// no op
	size_t count = 0;
	while (count != buffer_count)
		if (buffers[count++].len != 0)
			break;
	if (!count || buffers[count-1].len == 0) {
		on_completion(op, iocp::error_code(), 0);
		return;
	}

	DWORD bytes_transferred = 0;
	int result = ::WSASend(socket, buffers,
		static_cast<DWORD>(buffer_count), &bytes_transferred, 0, op, 0);
	DWORD last_error = ::WSAGetLastError();
	if (last_error == ERROR_PORT_UNREACHABLE)
		last_error = WSAECONNREFUSED;
	if (result != 0 && last_error != WSA_IO_PENDING)
		on_completion(op, iocp::error_code(last_error, iocp::error::system_error_despcriber), bytes_transferred);
	else
		on_pending(op);
}

void service::recv(SOCKET socket,
									 iocp::operation *op,
									 WSABUF *buffers, size_t buffer_count)
{
	// no op
	size_t count = 0;
	while (count != buffer_count)
		if (buffers[count++].len != 0)
			break;
	if (!count || buffers[count-1].len == 0) {
		on_completion(op, iocp::error_code(), 0);
		return;
	}

	DWORD flags = 0;
	DWORD bytes_transferred = 0;
	int result = ::WSARecv(socket, buffers,
		static_cast<DWORD>(buffer_count), &bytes_transferred, &flags, op, 0);
	DWORD last_error = ::WSAGetLastError();
	if (last_error == ERROR_NETNAME_DELETED)
		last_error = WSAECONNRESET;
	else if (last_error == ERROR_PORT_UNREACHABLE)
		last_error = WSAECONNREFUSED;
	if (result != 0 && last_error != WSA_IO_PENDING)
		on_completion(op, iocp::error_code(last_error, iocp::error::system_error_despcriber), bytes_transferred);
	else
		on_pending(op);
}

// file service
iocp::error_code service::file(HANDLE &handle,
															 const char *file_name,
															 DWORD access_mode,
															 DWORD share_mode,
															 DWORD creation_mode)
{
	iocp::error_code ec(0, iocp::error::system_error_despcriber);
	handle = CreateFileA(file_name,access_mode,
		share_mode, 0, creation_mode, FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OVERLAPPED, 0);
	// we do not check ERROR_ALREADY_EXISTS
	//   when dwCreationDisposition is CREATE_ALWAYS or OPEN_ALWAYS
	if (handle == INVALID_HANDLE_VALUE)
		ec.set_value(::GetLastError());
	if (ec.value() == ERROR_FILE_EXISTS) {
		ec.set_value(iocp::error::already_exists);
		ec.set_describer(iocp::error::frame_error_describer);
	}
	return ec;
}

iocp::error_code service::close_file(HANDLE &handle)
{
	iocp::error_code ec(0, iocp::error::system_error_despcriber);
	if (!::CloseHandle(handle))
		ec.set_value(::GetLastError());
	handle = INVALID_HANDLE_VALUE;
	return ec;
}

iocp::error_code service::seek(HANDLE handle, __int64 distance,
															 unsigned __int64 *new_point, DWORD move_method)
{
	iocp::error_code ec(0, iocp::error::system_error_despcriber);
	LARGE_INTEGER dt; dt.QuadPart = distance;
	if (!::SetFilePointerEx(handle, dt, (PLARGE_INTEGER)new_point, move_method))
		ec.set_value(::GetLastError());
	return ec;
}

iocp::error_code service::length(HANDLE handle, unsigned __int64 *len)
{
	iocp::error_code ec(0, iocp::error::system_error_despcriber);
	if (!::GetFileSizeEx(handle, (PLARGE_INTEGER)len))
		ec.set_value(::GetLastError());
	return ec;
}

iocp::error_code service::set_eof(HANDLE handle)
{
	iocp::error_code ec(0, iocp::error::system_error_despcriber);
	if (!::SetEndOfFile(handle))
		ec.set_value(::GetLastError());
	return ec;
}

iocp::error_code service::flush(HANDLE handle)
{
	iocp::error_code ec(0, iocp::error::system_error_despcriber);
	if (!::FlushFileBuffers(handle))
		ec.set_value(::GetLastError());
	return ec;
}

void service::write(HANDLE handle,
										iocp::operation *op,
										LPCVOID buffer, size_t bytes)
{
	// no op
	if (bytes == 0) {
		on_completion(op, iocp::error_code(), 0);
		return;
	}

	DWORD bytes_transferred = 0;
	int result = ::WriteFile(handle, buffer,
		static_cast<DWORD>(bytes), &bytes_transferred, op);
	DWORD last_error = ::GetLastError();
	if (result == 0 && last_error != ERROR_IO_PENDING)
		on_completion(op, iocp::error_code(last_error, iocp::error::system_error_despcriber), bytes_transferred);
	else
		on_pending(op);
}

void service::read(HANDLE handle,
									 iocp::operation *op,
									 LPVOID buffer, size_t bytes)
{
	// no op
	if (bytes == 0) {
		on_completion(op, iocp::error_code(), 0);
		return;
	}

	DWORD bytes_transferred = 0;
	int result = ::ReadFile(handle, buffer,
		static_cast<DWORD>(bytes), &bytes_transferred, op);
	DWORD last_error = ::GetLastError();
	if (result == 0 && last_error != ERROR_IO_PENDING)
		on_completion(op, iocp::error_code(last_error, iocp::error::system_error_despcriber), bytes_transferred);
	else
		on_pending(op);
}

}

#endif

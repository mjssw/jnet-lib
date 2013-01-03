// iocp_resolver.h
#ifndef _IOCP_RESOLVER_H_
#define _IOCP_RESOLVER_H_

#ifndef _IOCP_NO_INCLUDE_
#include "noncopyable.h"
#include "iocp_config.h"
#include "net_service.h"
#include "iocp_thread.h"
#include "iocp_lock.h"
#endif

namespace iocp {

class resolve_op;
struct resolve_entry
{
	iocp::service *svr;
	resolve_op *op;
	resolve_entry *next;
};

class resolver: private noncopyable
{
public:
	resolver(iocp::service &service): service_(service) {}
	typedef void (*resolve_cb_type)(void *binded, const iocp::error_code &ec, const iocp::address &addr);
	void asyn_resolve(const char *name, resolve_cb_type callback, void *binded);
  static void asyn_resolve(iocp::service &svr, const char *name, resolve_cb_type callback, void *binded);
  static iocp::error_code resolve(const char *name, iocp::address &addr);

  static void init();
	static void stop();

private:
	iocp::service &service_;

	static void resolve_thread_loop(void *arg);

	static long initialized_;
	static long stop_flag_;
	static mutex resolve_mutex_;

  static thread resolve_thread_;
  static event resolve_event_;

	static resolve_entry *head_;
	static resolve_entry *tail_;

public:
	static void push(resolve_entry *entry)
	{
		scope_lock lock(resolver::resolve_mutex_);

		if (resolver::tail_)
			resolver::tail_->next = entry;
		resolver::tail_ = entry;
		if (!resolver::head_)
			resolver::head_ = entry;

    resolver::resolve_event_.set();
	}
	static resolve_entry *pop()
	{
		resolve_entry *entry = 0;

		scope_lock lock(resolver::resolve_mutex_);
		// pop
		entry = resolver::head_;
		if (entry)
			resolver::head_ = entry->next;
		if (resolver::tail_ == entry)
			resolver::tail_ = 0;

		return entry;
	}
};

class resolve_op: public iocp::operation
{
public:
	resolve_op(iocp::resolver::resolve_cb_type callback,
		void *binded, iocp::service &service, const char *name)
		: operation(&resolve_op::on_complete, binded),
		callback_(callback), service_(service), name_(name), addr_(0U) {}

	void start_resolve_op();

private:
	friend class resolver;
	static void on_complete(iocp::service *svr, iocp::operation *op,
		const iocp::error_code &ec, size_t bytes_transferred);

	iocp::resolver::resolve_cb_type callback_;
	void *binded;

	iocp::service &service_;
	const char *name_;
	iocp::address addr_;
};

}

#endif // _IOCP_RESOLVER_H_

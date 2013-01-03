// iocp_resolver.cpp
#ifndef _IOCP_NO_INCLUDE_
#include "iocp_resolver.h"
#endif

namespace iocp {

mutex resolver::resolve_mutex_;
thread resolver::resolve_thread_(&resolver::resolve_thread_loop, 0, false);
event resolver::resolve_event_(event::nonsignaled, event::auto_reset);
resolve_entry *resolver::head_;
resolve_entry *resolver::tail_;
long resolver::initialized_;
long resolver::stop_flag_;

class auto_resolver
{
public:
	~auto_resolver()
	{
		resolver::stop();
	}
};
static auto_resolver auto_resolver_;

void resolver::asyn_resolve(const char *name, resolve_cb_type callback, void *binded)
{
	if (sync_compare_and_swap(&resolver::initialized_, 0, 1))
    resolver::init();

	service_.work_started();
	resolve_op *op = new resolve_op(callback, binded, service_, name);
	op->start_resolve_op();
}

void resolver::asyn_resolve(iocp::service &svr, const char *name, resolve_cb_type callback, void *binded)
{
  if (sync_compare_and_swap(&resolver::initialized_, 0, 1))
    resolver::init();

  svr.work_started();
  resolve_op *op = new resolve_op(callback, binded, svr, name);
  op->start_resolve_op();
}

iocp::error_code resolver::resolve(const char *name, iocp::address &addr)
{
  iocp::error_code ec(0, iocp::error::system_error_despcriber);
	struct hostent *lpHostent = ::gethostbyname(name);
	if (lpHostent == NULL)
    ec.set_value(h_errno);
	else
		addr = *(unsigned int *)(lpHostent->h_addr_list)[0];

  return ec;
}

void resolver::resolve_thread_loop(void *arg)
{
  iocp::error_code ec;
	for (;;)
	{
    resolver::resolve_event_.wait();
		if (sync_get(&resolver::stop_flag_) == 1)
			break;
		resolve_entry *entry = resolver::pop();
		if (entry) {
			if (entry->next)
        resolver::resolve_event_.set();
      ec = resolver::resolve(entry->op->name_, entry->op->addr_);
      entry->svr->on_completion(entry->op, ec);
			delete entry;
		}
	}

	//return 0L;
}

void resolver::init()
{
  resolver::resolve_thread_.start();
}

void resolver::stop()
{
	if (sync_compare_and_swap(&resolver::stop_flag_, 0, 1)) {
    resolver::resolve_event_.set();
    if (resolver::resolve_thread_.join(1000) == 0)
      resolver::resolve_thread_.kill();

		resolve_entry *entry;
		for (entry = resolver::head_; entry ; ) {
			resolve_entry *to_del = entry;
			entry = entry->next;
			delete to_del;
		}
	}
}

void resolve_op::start_resolve_op()
{
	resolve_entry *entry = new resolve_entry;
	entry->svr = &service_;
	entry->op = this;
	entry->next = 0;
	resolver::push(entry);
}

void resolve_op::on_complete(iocp::service *svr, iocp::operation *op_,
														 const iocp::error_code &ec, size_t bytes_transferred)
{
	resolve_op *op = static_cast<resolve_op *>(op_);
	op->callback_(op->argument(), ec, op->addr_);
	delete op;
}

}

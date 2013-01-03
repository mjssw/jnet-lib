// fiber_file.cpp
#ifndef _IOCP_NO_INCLUDE_
#include "fiber_file.h"
#endif

namespace iocp {

fiber_file::fiber_file(iocp::service &service)
	:iocp::file(service)
{
	fiber_data_ = fiber::scheduler::create(0, 0);
}

fiber_file::fiber_file(iocp::service &service, fiber::scheduler::routine_type routine, void *arg)
	:iocp::file(service)
{
	fiber_data_ = fiber::scheduler::create(routine, arg);
	fiber::scheduler::switch_to(fiber_data_);
}

void fiber_file::invoke(fiber::scheduler::routine_type routine, void *arg)
{
	fiber_data_->set_routine(routine);
	fiber_data_->set_argument(arg);
	fiber::scheduler::switch_to(fiber_data_);
}

void fiber_file::on_post(void *binded, const iocp::error_code &ec)
{
	iocp::fiber_file *file = static_cast<iocp::fiber_file *>(binded);
	if (!fiber::scheduler::switch_to(file->fiber_data_))
		file->service().post(&fiber_file::on_post, file, ec);
}

void fiber_file::on_written(void *binded, const iocp::error_code &ec, size_t bytes_trasferred)
{
	iocp::fiber_file *file = static_cast<iocp::fiber_file *>(binded);
	file->ec_ = ec;
	file->bytes_transferred_ = bytes_trasferred;
	if (!fiber::scheduler::switch_to(file->fiber_data_))
		file->service().post(&fiber_file::on_post, file, ec);
}

iocp::error_code fiber_file::write(const char *buf, size_t len, size_t &bytes_transferred)
{
	asyn_write(buf, len, &fiber_file::on_written, this);
	fiber_data_->yield();
	bytes_transferred = bytes_transferred_;
	return ec_;
}

void fiber_file::on_read(void *binded, const iocp::error_code &ec, size_t bytes_trasferred, char *buffer)
{
	iocp::fiber_file *file = static_cast<iocp::fiber_file *>(binded);
	file->ec_ = ec;
	file->bytes_transferred_ = bytes_trasferred;
	if (!fiber::scheduler::switch_to(file->fiber_data_))
		file->service().post(&fiber_file::on_post, file, ec);
}

iocp::error_code fiber_file::read(char *buf, size_t len, size_t &bytes_transferred)
{
	asyn_read(buf, len, &fiber_file::on_read, this);
	fiber_data_->yield();
	bytes_transferred = bytes_transferred_;
	return ec_;
}

iocp::error_code fiber_file::read_line(char *buf, size_t len, size_t &bytes_transferred,
																			 const char *eol /* = "\r\n" */, bool include_eol /* = false */)
{
	asyn_read_line(buf, len, &fiber_file::on_read, this, eol, include_eol);
	fiber_data_->yield();
	bytes_transferred = bytes_transferred_;
	return ec_;
}

}

// epoll_file.cpp
#ifdef __linux__

#ifndef _IOCP_NO_INCLUDE_
#include "epoll_file.h"
#include "epoll_service.h"
#endif

namespace iocp {

file::file(iocp::service &service): base_type(service)
{
	fd_ = file_ops::invalid_handle;
}

iocp::error_code file::advance_open(const char *filename,
																		file::access_mode access_mode,
																		file::share_mode share_mode,
																		file::creation_mode creation_mode)
{
	return file_ops::file(*this, filename, access_mode, share_mode, creation_mode);
}

iocp::error_code file::create(const char *filename,
															bool create_anyway, bool truncate, bool append,
															file::access_mode access_mode /* = generic_write */,
															file::share_mode share_mode /* = share_read */)
{
	if (!create_anyway)
		return file_ops::file(*this, filename, access_mode, share_mode, file::create_new);
	else if (truncate)
		return file_ops::file(*this, filename, access_mode, share_mode, file::create_always);
	iocp::error_code ec;
	ec = file_ops::file(*this, filename, access_mode, share_mode, file::create_new);
	if (ec && ec.value() == iocp::error::already_exists) {
		ec = file_ops::file(*this, filename, access_mode, share_mode, file::open_existing);
		if (append && !ec)
			ec = seek(0, 0, file::by_end);
	}

	return ec;
}

iocp::error_code file::open(const char *filename,
														bool open_anyway, bool truncate, bool append,
														file::access_mode access_mode /* = generic_read */,
														file::share_mode share_mode /* = share_read */)
{
	iocp::error_code ec;
	if (!open_anyway && truncate)
		return file_ops::file(*this, filename, access_mode, share_mode, file::truncate_existing);
	else if (!open_anyway)
		ec = file_ops::file(*this, filename, access_mode, share_mode, file::open_existing);
	else
		ec = file_ops::file(*this, filename, access_mode, share_mode, file::open_always);
	if (ec)
		return ec;
	if (truncate)
		ec = this->truncate();
	else if (append)
		ec = seek(0, 0, file::by_end);
	return ec;
}

iocp::error_code file::close()
{
	return file_ops::close(*this);
}

iocp::error_code file::length(file::point_type &len)
{
	return file_ops::length(*this, len);
}

file::point_type file::length()
{
	file::point_type len = ~(file::point_type)0;
	length(len);
	return len;
}

iocp::error_code file::set_eof()
{
	return file_ops::set_eof(*this);
}

iocp::error_code file::flush()
{
	return file_ops::flush(*this);
}

iocp::error_code file::seek(file::seek_type distance,
														file::point_type *new_point,
														file::seek_method seek_method)
{
	return file_ops::seek(*this, distance, new_point, seek_method);
}

iocp::error_code file::set_pos(file::point_type pos)
{
	return seek((file::seek_type)pos, 0, file::from_beginning);
}

iocp::error_code file::move_pos(file::seek_type offset)
{
	return seek(offset, 0, file::from_current);
}

iocp::error_code file::tell(file::point_type &point)
{
	return seek(0, &point, file::from_current);
}

file::point_type file::tell()
{
	file::point_type len = ~(file::point_type)0;
	tell(len);
	return len;
}

iocp::error_code file::rewind()
{
	return seek(0, 0, file::from_beginning);
}

iocp::error_code file::truncate()
{
	iocp::error_code ec;
	ec = rewind();
	if (!ec)
		ec = set_eof();
	return ec;
}

iocp::error_code file::extend_to(file::point_type size)
{
	iocp::error_code ec;
	ec = seek(size, 0, file::from_beginning);
	if (!ec)
		ec = set_eof();
	return ec;
}

void file::asyn_write(const char *buf, size_t len, write_cb_type callback, void *binded)
{
	write_op *op = new write_op(*this, buf, len, callback, binded);
	op->begin_write();
}

void file::asyn_read(char *buf, size_t len, read_cb_type callback, void *binded)
{
	read_op *op = new read_op(*this, buf, len, callback, binded);
	op->begin_read();
}

void file::asyn_read_line(char *buf, size_t len, read_cb_type callback, void *binded,
													const char *eol /* = "\r\n" */, bool inc_eol /* = false */)
{
	read_line_op *op = new read_line_op(*this, buf, len, eol, inc_eol, callback, binded);
	op->begin_read();
}

/// file_ops

iocp::thread file_ops::file_thread_(&file_ops::file_thread_loop, 0, false);
iocp::event file_ops::file_event_(event::nonsignaled, event::auto_reset);
long file_ops::initialized_ = 0;
long file_ops::stop_flag_ = 0;

iocp::mutex file_ops::file_mutex_;
iocp::file_entry *file_ops::head_;
iocp::file_entry *file_ops::tail_;

void file_ops::push(file_entry *entry)
{
	scope_lock lock(file_ops::file_mutex_);

	if (file_ops::tail_)
		file_ops::tail_->next = entry;
	file_ops::tail_ = entry;
	if (!file_ops::head_)
		file_ops::head_ = entry;

  file_ops::file_event_.set();
}

file_entry *file_ops::pop()
{
	file_entry *entry = 0;

	scope_lock lock(file_ops::file_mutex_);
	// pop
	entry = file_ops::head_;
	if (entry)
		file_ops::head_ = entry->next;
	if (file_ops::tail_ == entry)
		file_ops::tail_ = 0;

	return entry;
}

void file_ops::file_thread_loop(void *arg)
{
  iocp::error_code ec;
  unsigned int bytes_transferred;

	for (;;)
	{
    file_ops::file_event_.wait();
		if (sync_get(&file_ops::stop_flag_) == 1)
			break;
		file_entry *entry = file_ops::pop();
		if (entry) {
			if (entry->next)
        file_ops::file_event_.set();
      if (entry->type == file_entry::read) {
      	ec = entry->rop->do_read(bytes_transferred);
      	entry->svr->on_completion(entry->rop, ec, bytes_transferred);
      }
      else if (entry->type == file_entry::read_line) {
      	ec = entry->rlop->do_read(bytes_transferred);
      	entry->svr->on_completion(entry->rlop, ec, bytes_transferred);
      }
      else {
      	ec = entry->wop->do_write(bytes_transferred);
      	entry->svr->on_completion(entry->wop, ec, bytes_transferred);
      }
			delete entry;
		}
	}
}

iocp::error_code file_ops::file(iocp::file &file,
																const char *filename,
																file::access_mode access_mode,
																file::share_mode share_mode,
																file::creation_mode creation_mode)
{
	if (file.fd_ != file_ops::invalid_handle)
		return iocp::error_code(iocp::error::already_open, \
			iocp::error::frame_error_describer);

	iocp::error_code ec;
	ec = file.service().file(file.fd_,\
		filename, access_mode, share_mode, creation_mode);
	return ec;
}

iocp::error_code file_ops::close(iocp::file &file)
{
	if (file.fd_ == file_ops::invalid_handle)
		return iocp::error_code(iocp::error::not_open, \
			iocp::error::frame_error_describer);

	return file.service().close_file(file.fd_);
}

iocp::error_code file_ops::length(iocp::file &file, file::point_type &len)
{
	iocp::error_code ec;
	file::point_type size = 0;
	ec = file.service().length(file.fd_, &size);
	if (!ec)
		len = size;
	return ec;
}

iocp::error_code file_ops::seek(iocp::file &file,
																file::seek_type distance,
																file::point_type *new_point,
																file::seek_method seek_method)
{
	return file.service().seek(file.fd_, distance, new_point, seek_method);
}

iocp::error_code file_ops::set_eof(iocp::file &file)
{
	return file.service().set_eof(file.fd_);
}

iocp::error_code file_ops::flush(iocp::file &file)
{
	return file.service().flush(file.fd_);
}

void file_ops::begin_write(iocp::file &file)
{
	iocp::service &service = file.service();
	service.work_started();
	if (sync_compare_and_swap(&file_ops::initialized_, 0, 1))
		file_ops::init();
}

iocp::error_code file_ops::write(iocp::file &file,
																 const char *buffer, size_t &bytes)
{
	iocp::service &service = file.service();

	return service.write(file.fd_, buffer, bytes);
}

void file_ops::begin_read(iocp::file &file)
{
	iocp::service &service = file.service();
	service.work_started();
	if (sync_compare_and_swap(&file_ops::initialized_, 0, 1))
		file_ops::init();
}

iocp::error_code file_ops::read(iocp::file &file,
																char *buffer, size_t &bytes)
{
	iocp::service &service = file.service();
	return service.read(file.fd_, buffer, bytes);
}

void file_ops::post_operation(iocp::service &svr,
                              iocp::operation *op,
                              iocp::error_code &ec,
                              size_t bytes_transferred)
{
  svr.work_started();
  svr.on_completion(op, ec, bytes_transferred);
}

/// operations

void write_op::begin_write()
{
	file_ops::begin_write(file_);
	file_entry *entry = new file_entry;
	entry->svr = &file_.service();
	entry->wop = this;
	entry->type = file_entry::write;
	entry->next = 0;
	file_ops::push(entry);
}

iocp::error_code write_op::do_write(unsigned int &bytes_transferred)
{
	iocp::error_code ec;

	size_t len = buffer_.len();
	if (len > max_write_buffer_size)
		len = max_write_buffer_size;
	if (len) {
		ec = file_ops::write(file_, buffer_.raw(), len);
	}
	bytes_transferred = len;

	return ec;
}

void write_op::on_complete(iocp::service *svr, iocp::operation *op_,
													 const iocp::error_code &ec, size_t bytes_transferred)
{
	write_op *op = static_cast<write_op *>(op_);

	if (!ec) {
		op->buffer_.consume(bytes_transferred);
		if (op->buffer_.len()) {
			op->reset();
			op->begin_write();
      return;
		}
	}

	if (ec || op->buffer_.len() == 0) {
		//file_ops::end_write(op->file_);
		op->callback_(op->argument(), ec, static_cast<size_t>(op->buffer_.raw() - op->buf_start_pos_));
		delete op;
	}
}

void read_op::begin_read()
{
	file_ops::begin_read(file_);
	file_entry *entry = new file_entry;
	entry->svr = &file_.service();
	entry->rop = this;
	entry->type = file_entry::read;
	entry->next = 0;
	file_ops::push(entry);
}

iocp::error_code read_op::do_read(unsigned int &bytes_transferred)
{
	iocp::error_code ec;

	size_t len = buffer_.remain();
	if (len > max_read_buffer_size)
		len = max_read_buffer_size;
	if (len) {
		ec = file_ops::read(file_, buffer_.raw_pos(), len);
	}
	bytes_transferred = len;

	return ec;
}

void read_op::on_complete(iocp::service *svr, iocp::operation *op_,
													const iocp::error_code &ec_, size_t bytes_transferred)
{
	read_op *op = static_cast<read_op *>(op_);

	iocp::error_code ec = ec_;

	if (ec.value() == iocp::error::epoll_flag && op->epoll_flags_ == EPOLLRDHUP)
		ec.set_value(0);

	if (!ec && bytes_transferred == 0 && op->buffer_.remain() > 0) {	// already end
		ec.set_value(iocp::error::eof);
		ec.set_describer(iocp::error::frame_error_describer);
	}

	if (!ec) {
		op->buffer_.append(bytes_transferred);
		if (op->buffer_.remain() > 0) {
			op->reset();
			op->begin_read();
			return;
		}
	}

	if (ec || op->buffer_.remain() == 0) {
		//file_ops::end_read(op->file_);
		op->callback_(op->argument(), ec, op->buffer_.len(), op->buf_start_pos_);
		delete op;
	}
}

void read_line_op::begin_read()
{
	file_ops::begin_read(file_);
	file_entry *entry = new file_entry;
	entry->svr = &file_.service();
	entry->rlop = this;
	entry->type = file_entry::read_line;
	entry->next = 0;
	file_ops::push(entry);
}

iocp::error_code read_line_op::do_read(unsigned int &bytes_transferred)
{
	iocp::error_code ec;

	size_t len = remain_length_;
	if (len > max_read_buffer_size)
		len = max_read_buffer_size;
	if (len) {
		ec = file_ops::read(file_, buffer_pos_, len);
	}
	bytes_transferred = len;

	return ec;
}

size_t read_line_op::find_eol(const char *text, size_t text_len,
															const char *word, size_t word_len,
															size_t *last_match_count)
{
	int text_pos = 0, word_pos = 0;
	int *T = read_line_op::build_kmp_table(word, word_len);
	size_t found_pos = text_len;
	while (text_pos+word_pos < (int)text_len) {
		if (word[word_pos] == text[text_pos+word_pos]) {
			if (word_pos == word_len-1) {
				found_pos = text_pos;
				break;
			}
			++word_pos;
		}
		else {
			text_pos = text_pos+word_pos-T[word_pos];
			if ((word_pos = T[word_pos]) < 0)
				word_pos = 0;
		}
	}
	read_line_op::free_kmp_table(T);
	if (found_pos == text_len)
		*last_match_count = word_pos;
	return found_pos;
}

int *read_line_op::build_kmp_table(const char *word, size_t word_len)
{
	int *T = (int *)malloc(word_len * sizeof(int));
	T[0] = -1; T[1] = 0;
	int pos = 2, cnd = 0;

	while (pos < (int)word_len) {
		if (word[pos -1] == word[cnd])
			T[pos++] = ++cnd;
		else if (cnd > 0)
			cnd = T[cnd];
		else
			T[pos++] = 0;
	}

	return T;
}

void read_line_op::free_kmp_table(int *table)
{
	free(table);
}


void read_line_op::on_complete(iocp::service *svr, iocp::operation *op_,
															 const iocp::error_code &ec_, size_t bytes_transferred)
{
	read_line_op *op = static_cast<read_line_op *>(op_);

	iocp::error_code ec = ec_;

	if (ec.value() == iocp::error::epoll_flag && op->epoll_flags_ == EPOLLRDHUP)
		ec.set_value(0);

	if (!ec && bytes_transferred == 0 && op->remain_length_ > 0) {	// already end
		ec.set_value(iocp::error::eof);
		ec.set_describer(iocp::error::frame_error_describer);
	}

	// 1. eof with explicit error
	if (ec || bytes_transferred <= 0) {
		op->callback_(op->argument(), ec, op->buffer_pos_ - op->buffer_, op->buffer_);
		delete op;
		return;
	}

	size_t last = 0;
	size_t found = read_line_op::find_eol(op->buffer_pos_, bytes_transferred, op->eol_, op->eol_len_, &last);
	if (found < bytes_transferred) {	// found!
		file_ops::seek(op->file_, found + op->eol_len_ - bytes_transferred, 0, file::from_current);
		bytes_transferred = found + op->eol_len_;
	}
	else {
		file_ops::seek(op->file_, -last, 0, file::from_current);
		bytes_transferred -= last;
	}
	op->buffer_pos_ += bytes_transferred;
	op->remain_length_ -= bytes_transferred;

	if (found < bytes_transferred) {
		op->callback_(op->argument(), ec, \
			op->buffer_pos_ - op->buffer_ - (op->inc_eol_ ? 0 : op->eol_len_), op->buffer_);
		delete op;
	}	// 2. normal eof, 3. eof with no error
	else if (op->remain_length_ == 0 || \
		bytes_transferred != max_read_buffer_size && op->remain_length_ != 0) {
			iocp::error_code ec_(iocp::error::eof, iocp::error::frame_error_describer);
			op->callback_(op->argument(), ec_, op->buffer_pos_ - op->buffer_, op->buffer_);
			delete op;
	}
	else {
    op->reset();
    op->begin_read();
	}
}

}

#endif

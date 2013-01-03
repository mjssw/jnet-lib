// iocp_file.cpp
#ifdef _WIN32

#ifndef _IOCP_NO_INCLUDE_
#include "iocp_file.h"
#include "iocp_service.h"
#endif

namespace iocp {

/// file

file::file(iocp::service &service): base_type(service)
{
	handle_ = file_ops::invalid_handle;
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
	file_ops::write(*this, op, buf, len);
}

void file::asyn_read(char *buf, size_t len, read_cb_type callback, void *binded)
{
	read_op *op = new read_op(*this, buf, len, callback, binded);
	op->go();
}

void file::asyn_read_line(char *buf, size_t len, read_cb_type callback, void *binded,
													const char *eol /* = "\r\n" */, bool inc_eol /* = false */)
{
	read_line_op *op = new read_line_op(*this, buf, len, eol, inc_eol, callback, binded);
	op->go();
}

/// file_ops

const HANDLE file_ops::invalid_handle = INVALID_HANDLE_VALUE;

iocp::error_code file_ops::file(iocp::file &file,
																const char *filename,
																file::access_mode access_mode,
																file::share_mode share_mode,
																file::creation_mode creation_mode)
{
	if (file.handle_ != file_ops::invalid_handle)
		return iocp::error_code(iocp::error::already_open, \
			iocp::error::frame_error_describer);

	iocp::error_code ec;
	ec = file.service().file(file.handle_,\
		filename, access_mode, share_mode, creation_mode);
	if (!ec)
		ec = file.service().register_handle(file.handle_);
	return ec;
}

iocp::error_code file_ops::close(iocp::file &file)
{
	if (file.handle_ == file_ops::invalid_handle)
		return iocp::error_code(iocp::error::not_open, \
			iocp::error::frame_error_describer);

	return file.service().close_file(file.handle_);
}

iocp::error_code file_ops::length(iocp::file &file, file::point_type &len)
{
	iocp::error_code ec;
	file::point_type size = 0;
	ec = file.service().length(file.handle_, &size);
	if (!ec)
		len = size;
	return ec;
}

iocp::error_code file_ops::seek(iocp::file &file,
																file::seek_type distance,
																file::point_type *new_point,
																file::seek_method seek_method)
{
	return file.service().seek(file.handle_, distance, new_point, seek_method);
}

iocp::error_code file_ops::set_eof(iocp::file &file)
{
	return file.service().set_eof(file.handle_);
}

iocp::error_code file_ops::flush(iocp::file &file)
{
	return file.service().flush(file.handle_);
}

void file_ops::write(iocp::file &file,
										 iocp::operation *op,
										 const char *buffer, size_t bytes)
{
	iocp::service &service = file.service();
	service.work_started();
	// set offset in OVERLAPPED
	file_ops::seek(file, 0, (file::point_type *)&op->Pointer, file::from_current);
	service.write(file.handle_, op, buffer, bytes);
}

void file_ops::read(iocp::file &file,
										iocp::operation *op,
										char *buffer, size_t bytes)
{
	iocp::service &service = file.service();
	service.work_started();
	// set offset in OVERLAPPED
	file_ops::seek(file, 0, (file::point_type *)&op->Pointer, file::from_current);
	service.read(file.handle_, op, buffer, bytes);
}

/// operations

void write_op::on_complete(iocp::service *svr, iocp::operation *op_,
													 const iocp::error_code &ec, size_t bytes_transferred)
{
	write_op *op = static_cast<write_op *>(op_);
	file_ops::seek(op->file_, bytes_transferred, 0, file::from_current);

	op->callback_(op->argument(), ec, bytes_transferred);
	delete op;
}

void read_op::on_complete(iocp::service *svr, iocp::operation *op_,
													const iocp::error_code &ec, size_t bytes_transferred)
{
	read_op *op = static_cast<read_op *>(op_);
	file_ops::seek(op->file_, bytes_transferred, 0, file::from_current);
	iocp::error_code ec_ = ec;
	if (ec_.value() == ERROR_HANDLE_EOF) { // TODO: move these to service
		ec_.set_value(iocp::error::eof);
		ec_.set_describer(iocp::error::frame_error_describer);
	}

	op->callback_(op->argument(), ec_, bytes_transferred, op->buffer_);
	delete op;
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
															 const iocp::error_code &ec, size_t bytes_transferred)
{
	read_line_op *op = static_cast<read_line_op *>(op_);
	// 1. eof with explicit error
	if (ec || bytes_transferred <= 0) {
		iocp::error_code ec_ = ec;
		if (ec_.value() == ERROR_HANDLE_EOF) { // TODO: move these to service
			ec_.set_value(iocp::error::eof);
			ec_.set_describer(iocp::error::frame_error_describer);
		}

		op->callback_(op->argument(), ec_, op->buffer_pos_ - op->buffer_, op->buffer_);
		delete op;
		return;
	}

	size_t last = 0;
	size_t found = read_line_op::find_eol(op->buffer_pos_, bytes_transferred, op->eol_, op->eol_len_, &last);
	if (found < bytes_transferred)	// found!
		bytes_transferred = found + op->eol_len_;
	else
		bytes_transferred -= last;
	op->buffer_pos_ += bytes_transferred;
	op->remain_length_ -= bytes_transferred;
	file_ops::seek(op->file_, bytes_transferred, 0, file::from_current);

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
		file_ops::read(op->file_, op, op->buffer_pos_, \
			op->remain_length_ > max_read_buffer_size ? max_read_buffer_size : op->remain_length_);
	}
}

}

#endif

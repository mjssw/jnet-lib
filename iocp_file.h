// iocp_file.h
#ifdef _WIN32

#ifndef _IOCP_NO_INCLUDE_
#pragma once
#include "iocp_error.h"
#include "iocp_operation.h"
#include "iocp_base_type.h"
#endif

namespace iocp {

class service;
class file_ops;
class file: public base_type
{
public:
	enum access_mode
	{
		generic_execute = GENERIC_EXECUTE,
		generic_read = GENERIC_READ,
		generic_write = GENERIC_WRITE,
		generic_read_write = GENERIC_READ|GENERIC_WRITE
	};

	enum share_mode
	{
		share_none = 0,
		share_delete = FILE_SHARE_DELETE,
		share_read = FILE_SHARE_READ,
		share_write = FILE_SHARE_WRITE,
		share_read_delete = FILE_SHARE_DELETE|FILE_SHARE_READ,
		share_write_delete = FILE_SHARE_DELETE|FILE_SHARE_WRITE,
		share_read_write = FILE_SHARE_READ|FILE_SHARE_WRITE,
		share_all = FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE
	};

	enum creation_mode
	{
		create_always = CREATE_ALWAYS,
		create_new = CREATE_NEW,
		open_always = OPEN_ALWAYS,
		open_existing = OPEN_EXISTING,
		truncate_existing = TRUNCATE_EXISTING
	};

	typedef unsigned __int64 point_type;
	typedef __int64 seek_type;

	enum seek_method
	{
		from_beginning = FILE_BEGIN,
		from_current = FILE_CURRENT,
		by_end = FILE_END
	};

	file(iocp::service &service);

	iocp::error_code advance_open(const char *filename,
		file::access_mode access_mode, file::share_mode share_mode, file::creation_mode creation_mode);
	iocp::error_code create(const char *filename,
		bool create_anyway, bool truncate = false, bool append = true,
		access_mode = generic_write, share_mode = share_read);
	iocp::error_code open(const char *filename,
		bool open_anyway, bool truncate = false, bool append = false,
		access_mode = generic_read, share_mode = share_read);

	iocp::error_code close();
	iocp::error_code length(file::point_type &len);
	file::point_type length();
	iocp::error_code flush();

	iocp::error_code seek(file::seek_type distance,
		file::point_type *new_point, file::seek_method seek_method);
	iocp::error_code set_pos(file::point_type pos);
	iocp::error_code move_pos(file::seek_type offset);
	iocp::error_code tell(file::point_type &point);
	file::point_type tell();
	iocp::error_code rewind();

	iocp::error_code set_eof();
	iocp::error_code truncate();
	iocp::error_code extend_to(file::point_type size);

	typedef void (*write_cb_type)(void *binded, const iocp::error_code &ec, size_t bytes_transferred);
	void asyn_write(const char *buf, size_t len, write_cb_type callback, void *binded);
	typedef void (*read_cb_type)(void *binded, const iocp::error_code &ec, size_t bytes_transferred, char *data);
	void asyn_read(char *buf, size_t len, read_cb_type callback, void *binded);
	// TODO: support regex expression?
	void asyn_read_line(char *buf, size_t len, read_cb_type callback, void *binded, const char *eol = "\r\n", bool inc_eol = false);

private:
	friend class file_ops;
	HANDLE handle_;
};

class file_ops
{
public:
	static const HANDLE invalid_handle;
	static iocp::error_code file(iocp::file &file,
		const char *filename, file::access_mode access_mode,
		file::share_mode share_mode, file::creation_mode creation_mode);
	static iocp::error_code close(iocp::file &file);

	static iocp::error_code length(iocp::file &file, file::point_type &len);
	static iocp::error_code seek(iocp::file &file,
		file::seek_type distance, file::point_type *new_point, file::seek_method seek_method);
	static iocp::error_code set_eof(iocp::file &file);
	static iocp::error_code flush(iocp::file &file);

	static void write(iocp::file &file,
		iocp::operation *op, const char *buffer, size_t bytes);
	static void read(iocp::file &file,
		iocp::operation *op, char *buffer, size_t bytes);
};

class write_op: public operation
{
public:
	write_op(iocp::file &file,
		const char *buf, size_t len, iocp::file::write_cb_type callback, void *binded)
		: operation(&write_op::on_complete, binded),
		file_(file), callback_(callback) {}

private:
	//enum { max_write_buffer_size = 64 * 1024 };
	static void on_complete(iocp::service *svr, iocp::operation *op,
		const iocp::error_code &ec, size_t bytes_transferred);

	iocp::file &file_;
	iocp::file::write_cb_type callback_;
};

class read_op: public operation
{
public:
	read_op(iocp::file &file,
		char *buf, size_t len, iocp::file::read_cb_type callback, void *binded)
		: operation(&read_op::on_complete, binded),
		file_(file), callback_(callback), buffer_(buf), length_(len) {}

	void go()
	{ file_ops::read(file_, this, buffer_, length_); }

private:
	//enum { max_read_buffer_size = 64 * 1024 };
	static void on_complete(iocp::service *svr, iocp::operation *op,
		const iocp::error_code &ec, size_t bytes_transferred);

	iocp::file &file_;
	iocp::file::read_cb_type callback_;

	char *buffer_;
	size_t length_;
};

class read_line_op: public operation
{
public:
	read_line_op(iocp::file &file,
		char *buf, size_t len, const char *eol, bool inc_eol,
		iocp::file::read_cb_type callback, void *binded)
		: operation(&read_line_op::on_complete, binded),
		file_(file), callback_(callback), buffer_(buf),
		buffer_pos_(buf), remain_length_(len),
		eol_(eol), inc_eol_(inc_eol)
	{
		eol_len_ = strlen(eol_);
		char *eol_copy = new char[eol_len_+1];
		memcpy(eol_copy, eol, eol_len_+1);
		eol_ = eol_copy;
	}

	~read_line_op()
	{
		delete[] eol_;
	}

	void go()
	{
		file_ops::read(file_, this, buffer_pos_, \
			remain_length_ > max_read_buffer_size ? max_read_buffer_size : remain_length_);
	}

private:
	enum { max_read_buffer_size = 64 * 1024 };
	static void on_complete(iocp::service *svr, iocp::operation *op,
		const iocp::error_code &ec, size_t bytes_transferred);

	static size_t find_eol(const char *text, size_t text_len, const char *word, size_t word_len, size_t *last_match_count);
	static int *build_kmp_table(const char *word, size_t word_len);
	static void free_kmp_table(int *table);

	iocp::file &file_;
	iocp::file::read_cb_type callback_;

	char *buffer_;
	char *buffer_pos_;
	size_t remain_length_;
	const char *eol_;
	size_t eol_len_;
	bool inc_eol_;
};

}

#endif

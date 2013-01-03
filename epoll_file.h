// epoll_file.h
#ifdef __linux__

#ifndef _EPOLL_FILE_H_
#define _EPOLL_FILE_H_

#ifndef _IOCP_NO_INCLUDE_
#include "iocp_error.h"
#include "iocp_operation.h"
#include "iocp_base_type.h"
#include "iocp_thread.h"
#include "iocp_lock.h"
#endif

#include <string.h>

namespace iocp {

class service;
class file_ops;
class file: public base_type
{
public:
	enum access_mode
	{
		// TODO:
		//generic_execute = O_RDONLY,
		generic_read = O_RDONLY,
		generic_write = O_WRONLY,
		generic_read_write = O_RDWR
	};

	enum share_mode
	{
		share_none = 0,
		share_delete = 1,
		share_read = 2,
		share_write = 4,
		share_read_delete = share_delete|share_read,
		share_write_delete = share_delete|share_write,
		share_read_write = share_read|share_write,
		share_all = share_delete|share_read|share_write
	};

	enum creation_mode
	{
		create_always = O_CREAT|O_TRUNC,
		create_new = O_CREAT|O_EXCL,
		open_always = O_CREAT,
		open_existing = 0,
		truncate_existing = O_TRUNC
	};

	typedef unsigned long long point_type;
	typedef long long seek_type;

	enum seek_method
	{
		from_beginning = SEEK_SET,
		from_current = SEEK_CUR,
		by_end = SEEK_END
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
	int fd_;
};


class write_op;
class read_op;
class read_line_op;
struct file_entry
{
	iocp::service *svr;

	union
	{
		write_op *wop;
		read_op *rop;
		read_line_op *rlop;
	};

	enum 
	{
		write = 0,
		read,
		read_line
	} type;

	file_entry *next;
};

class file_ops
{
public:
	static const int invalid_handle = -1;
	static iocp::error_code file(iocp::file &file,
		const char *filename, file::access_mode access_mode,
		file::share_mode share_mode, file::creation_mode creation_mode);
	static iocp::error_code close(iocp::file &file);

	static iocp::error_code length(iocp::file &file, file::point_type &len);
	static iocp::error_code seek(iocp::file &file,
		file::seek_type distance, file::point_type *new_point, file::seek_method wherece);
	static iocp::error_code set_eof(iocp::file &file);
	static iocp::error_code flush(iocp::file &file);

	static void begin_write(iocp::file &file);
	static iocp::error_code write(iocp::file &file, const char *buffer, size_t &bytes);
	static void begin_read(iocp::file &file);
	static iocp::error_code read(iocp::file &file, char *buffer, size_t &bytes);

	static void post_operation(iocp::service &svr,
		iocp::operation *op, iocp::error_code &ec, size_t bytes_transferred);

private:
	static void file_thread_loop(void *arg);
	static void init() { file_thread_.start(); }
	static iocp::thread file_thread_;
	static iocp::event file_event_;
	static long initialized_;
	static long stop_flag_;

	static iocp::mutex file_mutex_;
	static file_entry *head_;
	static file_entry *tail_;

	friend class write_op;
	friend class read_op;
	friend class read_line_op;
	static void push(file_entry *entry);
	static file_entry *pop();
};

class write_op: public operation
{
public:
	write_op(iocp::file &file,
		const char *buf, size_t len, iocp::file::write_cb_type callback, void *binded)
		: operation(&write_op::on_complete, binded),
		file_(file), callback_(callback),
		buf_start_pos_(buf), buffer_(buf, len) {}

		void begin_write();

private:
	enum { max_write_buffer_size = 64 * 1024 };
	friend class file_ops;
	iocp::error_code do_write(unsigned int &bytes_transferred);

	static void on_complete(iocp::service *svr, iocp::operation *op,
		const iocp::error_code &ec, size_t bytes_transferred);

	iocp::file &file_;
	iocp::file::write_cb_type callback_;

	const char *buf_start_pos_;
	const_buffer buffer_;
};

class read_op: public operation
{
public:
	read_op(iocp::file &file,
		char *buf, size_t len, iocp::file::read_cb_type callback, void *binded)
		: operation(&read_op::on_complete, binded),
		file_(file), callback_(callback),
		buf_start_pos_(buf), buffer_(buf, len, 0) {}

	void begin_read();

private:
	enum { max_read_buffer_size = 64 * 1024 };
	friend class file_ops;
	iocp::error_code do_read(unsigned int &bytes_transferred);

	static void on_complete(iocp::service *svr, iocp::operation *op,
		const iocp::error_code &ec, size_t bytes_transferred);

	iocp::file &file_;
	iocp::file::read_cb_type callback_;

	char *buf_start_pos_;
	mutable_buffer buffer_;
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

	void begin_read();

private:
	enum { max_read_buffer_size = 64 * 1024 };
	friend class file_ops;
	iocp::error_code do_read(unsigned int &bytes_transferred);

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

#endif //_EPOLL_FILE_H_

#endif

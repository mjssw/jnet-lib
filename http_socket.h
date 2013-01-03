//http_socket.h
#ifndef _HTTP_SOCKET_H_
#define _HTTP_SOCKET_H_

#ifdef _MSC_VER
#pragma once
#endif

#ifndef _IOCP_NO_INCLUDE_
#include "net_socket.h"
#include "iocp_operation.h"
#include "http_parser.h"
#endif

#include <string>
#include <vector>

namespace http {
namespace error {

std::string http_error_descripter(int value);

}

struct header
{
	std::string name;
	std::string value;
};

struct request
{
	std::string method;
	std::string uri;
	http_parser_url uri_offset;
	int http_version_major;
	int http_version_minor;
	std::vector<http::header> headers;
};

}

namespace iocp {

class http_socket: public noncopyable
{
public:
	http_socket(i_stream &stream): stream_(stream)
	{ ::http_parser_init(&parser_, HTTP_REQUEST); }

	i_stream &stream() { return stream_; }

	enum type { req, res, dat, fin };

	struct result
	{
		result(http_socket::type type_ = fin, const http::request *request_ = 0,
			const char *buffer_ = 0, size_t length_ = 0)
			: type(type_), request(request_), remain_buffer(0), remain_length(0)
		{ data.buffer = buffer_; data.length = length_; }

		http_socket::type type;
		const http::request *request;
		struct  
		{
			const char *buffer;
			size_t length;
		} data;
		char *remain_buffer;
		size_t remain_length;
	};

	typedef void (*request_cb_type)(void *, const iocp::error_code &, const http_socket::result &);
	void asyn_receive_request(http::request &, char *buf, size_t len,
		char *init_buf, size_t init_len, bool continuous, request_cb_type request_callback, void *binded);
	// void asyn_receive_response

private:
	friend class http_request_op;
	i_stream &stream_;

	::http_parser parser_;
};

class http_request_op: public operation
{
public:
	http_request_op(iocp::http_socket &socket, bool continuous,
		http::request &request, char *buf, size_t len, char *init_buf, size_t init_len,
		iocp::http_socket::request_cb_type request_callback, void *binded)
		: operation(&http_request_op::on_complete, binded), socket_(socket),
			continuous_(continuous), request_callback_(request_callback), message_complete_(false),
			request_(request), buf_(buf), buf_len_(len), init_buf_(init_buf), init_buf_len_(init_len)
	{
		settings_.on_message_begin = &http_request_op::on_message_begin;
		settings_.on_url = &http_request_op::on_url;
		settings_.on_header_field = &http_request_op::on_header_field;
		settings_.on_header_value = &http_request_op::on_header_value;
		settings_.on_headers_complete = &http_request_op::on_headers_complete;
		settings_.on_body = &http_request_op::on_body;
		settings_.on_message_complete = &http_request_op::on_message_complete;

		socket_.parser_.data = this;
	}

	void do_request_operation()
	{
		if (init_buf_len_)
			socket_.stream_.asyn_recv_some(init_buf_, 0, &http_request_op::receive_callback, this);
		else
			socket_.stream_.asyn_recv_some(buf_, buf_len_, &http_request_op::receive_callback, this);
	}

private:
	static void on_complete(iocp::service *svr, iocp::operation *op,
		const iocp::error_code &ec, size_t bytes_transferred) {} // it's empty

	static void receive_callback(void *binded, const iocp::error_code &ec, size_t bytes_transferred, char *buffer);

	static int on_message_begin(http_parser *parser);
	static int on_url(http_parser *parser, const char *at, size_t length);
	static int on_header_field(http_parser *parser, const char *at, size_t length);
	static int on_header_value(http_parser *parser, const char *at, size_t length);
	static int on_headers_complete(http_parser *parser);
	static int on_body(http_parser *parser, const char *at, size_t length);
	static int on_message_complete(http_parser *parser);

	iocp::http_socket::request_cb_type request_callback_;

	http_socket &socket_;

	bool continuous_;
	::http_parser_settings settings_;
	int last_field_or_value_;	// 0 for field, 1 for value
	bool message_complete_;
	http_socket::result result_;

	http::request &request_;
	char *buf_;
	size_t buf_len_;
	char *init_buf_;
	size_t init_buf_len_;
};


}

#endif //_HTTP_SOCKET_H_

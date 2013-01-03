// http_socket.cpp
#ifndef _IOCP_NO_INCLUDE_
#include "http_socket.h"
#include <string.h>
#endif

namespace iocp {
namespace error {

std::string http_error_descripter(int value)
{
	const char* s = ::http_errno_name((enum http_errno)value);
	return s ? s : "http error";
}

}

void
http_socket::asyn_receive_request(http::request &req, char *buf,
																	size_t len, char *init_buf, size_t init_len, bool continuous,
																	request_cb_type request_callback, void *binded)
{
	http_request_op *op = new http_request_op(*this, continuous, req,\
		buf, len, init_buf, init_len, request_callback, binded);
	op->do_request_operation();
}

void
http_request_op::receive_callback(void *binded,
																	const iocp::error_code &ec,
																	size_t bytes_transferred, char *buffer)
{
	http_request_op *op = static_cast<http_request_op *>(binded);

	if (op->init_buf_len_) {
		bytes_transferred = op->init_buf_len_;
		op->init_buf_len_ = 0;
	}

	if (ec) {
		op->request_callback_(op->argument(), ec,
			http_socket::result(http_socket::dat, 0, buffer, bytes_transferred));
		delete op;
		return;
	}

	if (HTTP_PARSER_ERRNO(&op->socket_.parser_) == HPE_PAUSED)
		::http_parser_pause(&op->socket_.parser_, 0);
	size_t nparsed =
		::http_parser_execute(&op->socket_.parser_, &op->settings_, buffer, bytes_transferred);
	enum ::http_errno err = HTTP_PARSER_ERRNO(&op->socket_.parser_);
	if (op->socket_.parser_.upgrade)	// we do not handle new protocol
		err = HPE_UNKNOWN;

	if (err != HPE_OK && err != HPE_PAUSED) {
		// when occur error, we only use data callback type in order to
		// indicate the bytes we already received
		op->result_ = http_socket::result(http_socket::dat, 0, buffer, bytes_transferred);
		op->request_callback_(op->argument(),
			iocp::error_code(err, iocp::error::http_error_descripter),
			op->result_);
		delete op;
	}
	else if (op->message_complete_ || err == HPE_PAUSED) {
		if (op->result_.remain_length = bytes_transferred - nparsed)
			op->result_.remain_buffer = buffer + nparsed;
		else
			op->result_.remain_buffer = 0;
		op->request_callback_(op->argument(), iocp::error_code(), op->result_);
		delete op;
	}
	else {
		// we assume that nparsed == buffer length
		op->socket_.stream_.asyn_recv_some(op->buf_,
			op->buf_len_, &http_request_op::receive_callback, op);
	}
}

int http_request_op::on_message_begin(http_parser *parser)
{
	http_request_op *op = static_cast<http_request_op *>(parser->data);
	op->request_.method.clear();
	op->request_.uri.clear();
	op->request_.http_version_major = 0;
	op->request_.http_version_minor = 0;
	op->request_.headers.clear();
	memset(&op->request_.uri_offset, 0, sizeof(op->request_.uri_offset));

	op->last_field_or_value_ = 1;

	return 0;
}

int http_request_op::on_url(http_parser *parser, const char *at, size_t length)
{
	http_request_op *op = static_cast<http_request_op *>(parser->data);
	op->request_.uri.append(at, length);
	return 0;
}

int http_request_op::on_header_field(http_parser *parser, const char *at, size_t length)
{
	http_request_op *op = static_cast<http_request_op *>(parser->data);
	if (op->last_field_or_value_ == 1)	// new header
		op->request_.headers.push_back(http::header());
	op->request_.headers.back().name.append(at, length);
	op->last_field_or_value_ = 0;
	return 0;
}

int http_request_op::on_header_value(http_parser *parser, const char *at, size_t length)
{
	http_request_op *op = static_cast<http_request_op *>(parser->data);
	op->request_.headers.back().value.append(at, length);
	op->last_field_or_value_ = 1;
	return 0;
}

int http_request_op::on_headers_complete(http_parser *parser)
{
	http_request_op *op = static_cast<http_request_op *>(parser->data);
	op->request_.method = ::http_method_str((enum http_method)parser->method);
	op->request_.http_version_major = parser->http_major;
	op->request_.http_version_minor = parser->http_minor;
	int result =
		::http_parser_parse_url(op->request_.uri.c_str(),
														op->request_.uri.length(),
														parser->method == HTTP_CONNECT,
														&op->request_.uri_offset);
	if (!result) {
		op->result_ = http_socket::result(http_socket::req, &op->request_);
		if (op->continuous_)
			op->request_callback_(op->argument(), iocp::error_code(), op->result_);
		else
			::http_parser_pause(parser, 1);	// no continue
	}
	return result;
}

int http_request_op::on_body(http_parser *parser, const char *at, size_t length)
{
	http_request_op *op = static_cast<http_request_op *>(parser->data);
	op->result_ = http_socket::result(http_socket::dat, &op->request_, at, length);
	if (op->continuous_)
		op->request_callback_(op->argument(), iocp::error_code(), op->result_);
	else
		::http_parser_pause(parser, 1);	// no continue
	return 0;
}

int http_request_op::on_message_complete(http_parser *parser)
{
	http_request_op *op = static_cast<http_request_op *>(parser->data);
	op->result_ = http_socket::result(http_socket::fin);
	::http_parser_pause(parser, 1);	// no continue
	op->message_complete_ = true;
	// we can not call callback function here for missing buffer information
	return 0;
}

}

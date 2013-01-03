// ssl_socket.h
#ifndef _SSL_SOCKET_H_
#define _SSL_SOCKET_H_

#ifdef _NET_ENABLE_SSL_

#ifdef _MSC_VER
#pragma once
#endif

#ifndef _IOCP_NO_INCLUDE_
#include "iocp_operation.h"
#include "iocp_lock.h"
#include "net_socket.h"
#endif

#include <openssl/conf.h>
#include <openssl/ssl.h>
#include <openssl/engine.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>

#include <string>
#ifndef OPENSSL_NO_TLSEXT
#include <map>
#endif

namespace ssl {
namespace error {

std::string ssl_error_descripter(int value);

}

	void init();

	typedef enum
	{
		/// Generic SSL version 2.
		sslv2,

		/// SSL version 2 client.
		sslv2_client,

		/// SSL version 2 server.
		sslv2_server,

		/// Generic SSL version 3.
		sslv3,

		/// SSL version 3 client.
		sslv3_client,

		/// SSL version 3 server.
		sslv3_server,

		/// Generic TLS version 1.
		tlsv1,

		/// TLS version 1 client.
		tlsv1_client,

		/// TLS version 1 server.
		tlsv1_server,

		/// Generic SSL/TLS.
		sslv23,

		/// SSL/TLS client.
		sslv23_client,

		/// SSL/TLS server.
		sslv23_server
	} method;

	typedef int options;
	typedef int verify_mode;
	typedef int (*verify_cb)(int, X509_STORE_CTX *);
	typedef ::pem_password_cb pem_password_cb;

	enum file_format
	{
		/// ASN.1 file.
		asn1,

		/// PEM file.
		pem
	};

	enum handshake_type
	{
		/// Perform handshaking as a client.
		client,

		/// Perform handshaking as a server.
		server
	};

	// make compiler happy
	static iocp::error_code s_ec;

	class context
	{
	public:
		context(ssl::method m, iocp::error_code &ec = s_ec);
		~context();

		void set_options(options o);
		void set_verify_mode(verify_mode v);
		void set_verify_callback(verify_cb callback);
		long load_verify_file(const char *filename);
		long set_default_verify_paths();
		long add_verify_path(const char *path);
		long use_certificate_file(const char *filename, ssl::file_format format);
		long use_certificate_chain_file(const char *filename);
		long use_private_key_file(const char *filename, ssl::file_format format);
		long use_rsa_private_key_file(const char *filename, ssl::file_format format);
		long use_tmp_dh_file(const char *filename);
		void set_password_callback(pem_password_cb callback);
		long set_cipher_list(const char *list);

#ifndef OPENSSL_NO_TLSEXT
		long set_server_context_pair(const char *server_name, context &ctx);
#endif

		// SSL_CTX_get_app_data
		// SSL_CTX_set_app_data

		SSL_CTX *impl() { return impl_; }

	private:

#ifndef OPENSSL_NO_TLSEXT
		static int ssl_servername_cb(SSL *s, int *ad, void *arg);
		std::map<std::string, context *> sni_map_;
#endif
		SSL_CTX *impl_;
	};


	class engine
	{
	public:
		enum want
		{
			// Returned by functions to indicate that the engine wants input. The input
			// buffer should be updated to point to the data. The engine then needs to
			// be called again to retry the operation.
			want_input_and_retry = -2,

			// Returned by functions to indicate that the engine wants to write output.
			// The output buffer points to the data to be written. The engine then
			// needs to be called again to retry the operation.
			want_output_and_retry = -1,

			// Returned by functions to indicate that the engine doesn't need input or
			// output.
			want_nothing = 0,

			// Returned by functions to indicate that the engine wants to write output.
			// The output buffer points to the data to be written. After that the
			// operation is complete, and the engine does not need to be called again.
			want_output = 1,

			want_output_and_input_and_retry = 2
		};

		explicit engine(context &ctx, iocp::error_code &ec = ssl::s_ec);
		~engine();

		void set_verify_mode(verify_mode v);
		void set_verify_callback(verify_cb callback);

		iocp::error_code set_cipher_list(const char *list)
		{
			if (SSL_set_cipher_list(ssl_, list))
				return iocp::error_code();
			else
				return iocp::error_code(::SSL_get_error(ssl_, 0), ssl::error::ssl_error_descripter);
		}

		// Perform an SSL handshake using either SSL_connect (client-side) or
		// SSL_accept (server-side).
		want handshake(ssl::handshake_type type, iocp::error_code &ec);

		// Perform a graceful shutdown of the SSL session.
		want shutdown(iocp::error_code &ec);

		// Write bytes to the SSL session.
		want write(const char *data, size_t len, size_t &bytes_transferred, iocp::error_code &ec);

		// Read bytes from the SSL session.
		want read(char *data, size_t len, size_t &bytes_transferred, iocp::error_code &ec);

		// Get output data to be written to the transport.
		size_t get_output(char *data, size_t len);

		// Put input data that was read from the transport.
		size_t put_input(const char *data, size_t len);

		// Map an error::eof code returned by the underlying transport according to
		// the type and state of the SSL session. Returns a const reference to the
		// error code object, suitable for passing to a completion handler.
		void map_error_code(iocp::error_code& ec);

	private:
		// Disallow copying and assignment.
		engine(const engine&);
		engine& operator=(const engine&);

		// SSL_accept mutex
		static iocp::mutex accept_lock_;

		// Perform one operation. Returns >= 0 on success or error, want_read if the
		// operation needs more input, or want_write if it needs to write some output
		// before the operation can complete.
		want perform(int (engine::* op)(void*, size_t),
			void* data, size_t length, iocp::error_code &ec,
			size_t *bytes_transferred);

		// Adapt the SSL_accept function to the signature needed for perform().
		int do_accept(void*, size_t);

		// Adapt the SSL_connect function to the signature needed for perform().
		int do_connect(void*, size_t);

		// Adapt the SSL_shutdown function to the signature needed for perform().
		int do_shutdown(void*, size_t);

		// Adapt the SSL_read function to the signature needed for perform().
		int do_read(void* data, size_t length);

		// Adapt the SSL_write function to the signature needed for perform().
		int do_write(void* data, size_t length);

		SSL* ssl_;
		BIO* ext_bio_;
	};

}

namespace iocp {

class ssl_socket: private noncopyable, public i_stream
{
public:
	enum { ssl_buffer_size = 16 * 1024 };	// 16KB is the block size of ssl packet
	ssl_socket(ssl::context &ctx, iocp::service &service, iocp::error_code &ec = ssl::s_ec)
		: engine_(ctx, ec), context_(ctx), stream_(service),
	input_buffer_(input_buffer_space_), input_buffer_len_(0),
	output_buffer_(output_buffer_space_), output_buffer_len_(ssl_buffer_size),
	input_flag_(0), output_flag_(0) {}

	iocp::socket &stream() { return stream_; }
	operator iocp::socket &() { return stream_; }
	ssl::context &context() { return context_; }

	iocp::error_code set_cipher_list(const char *list)
	{ return engine_.set_cipher_list(list); }

	// in order to simplified the code
	iocp::service &service() { return stream_.service(); }
	void asyn_connect(const iocp::address &addr, unsigned short port, iocp::socket::connect_cb_type callback, void *binded)
	{ stream_.asyn_connect(addr, port, callback, binded); }
	iocp::error_code close()
	{ return stream_.close(); }

	typedef void (*handshake_cb_type)(void *binded, const iocp::error_code &ec);
	void asyn_handshake(ssl::handshake_type type, handshake_cb_type callback, void *binded);
	// stream interface
	void asyn_send(const char *buf, size_t len, send_cb_type callback, void *binded);
	void asyn_recv(char *buf, size_t len, receive_cb_type callback, void *data);
	void asyn_recv_some(char *buf, size_t len, receive_cb_type callback, void *binded);

	typedef void (*shutdown_cb_type)(void *binded, const iocp::error_code &ec);
	void asyn_shutdown(shutdown_cb_type callback, void *binded);

private:
	friend class ssl_base_op;
	
	ssl::context &context_; //a backup
	ssl::engine engine_;
	iocp::socket stream_;

	//iocp::critical_section cs_;
	long input_flag_;
	char input_buffer_space_[ssl_buffer_size];
	char *input_buffer_;
	size_t input_buffer_len_;
	long output_flag_;
	char output_buffer_space_[ssl_buffer_size];
	char *output_buffer_;
	size_t output_buffer_len_;
};

class ssl_base_op: public operation
{
public:
	typedef ssl::engine::want (*ssl_op_type)(operation *, ssl::engine &, iocp::error_code &, size_t &);

	ssl_base_op(ssl_socket &socket, ssl_op_type op,
		operation::func_type callback_, void *binded)
		:operation(callback_, binded), socket_(socket),
		op_(op), bytes_transferred_(0) {}
	virtual ~ssl_base_op() {}

	void do_ssl_operation()
	{ io_operation(iocp::error_code(), 0, 1); }

private:
	ssl_socket &socket_;
	ssl_op_type op_;
	ssl::engine::want want_;
	iocp::error_code ec_;
	size_t bytes_transferred_;

	void io_operation(const iocp::error_code ec, size_t bytes_transferred, int start = 0);

	// for callback purposal
	static void post_callback(void *binded, const iocp::error_code &);

	static void receive_callback(void *binded, const iocp::error_code &ec, size_t bytes_transferred, char *buffer);
	static void send_callback(void *binded, const iocp::error_code &ec, size_t bytes_transferred);
};

#pragma warning(push)
#pragma warning(disable:4355) // we can deal with 'this' point

class ssl_handshake_op: public ssl_base_op
{
public:
	ssl_handshake_op(iocp::ssl_socket &socket, ssl::handshake_type type,
		iocp::ssl_socket::handshake_cb_type callback, void *binded)
		: ssl_base_op(socket, &ssl_handshake_op::handshake_op,
		&ssl_handshake_op::on_complete, binded),
		type_(type), callback_(callback) {}

private:
	static void on_complete(iocp::service *svr, iocp::operation *op,
		const iocp::error_code &ec, size_t bytes_transferred);

	static ssl::engine::want handshake_op(operation *op_, ssl::engine &engine,
		iocp::error_code &ec, size_t &)
	{
		return engine.handshake(static_cast<ssl_handshake_op *>(op_)->type_, ec);
	}

	ssl::handshake_type type_;
	iocp::ssl_socket::handshake_cb_type callback_;
};

class ssl_send_op: public ssl_base_op
{
public:
	ssl_send_op(iocp::ssl_socket &socket, const char *buf, size_t len, 
		iocp::ssl_socket::send_cb_type callback, void *binded)
		: ssl_base_op(socket, &ssl_send_op::send_op,
		&ssl_send_op::on_complete, binded),
		buf_start_pos_(buf), buffer_(buf, len), callback_(callback) {}

private:
	static void on_complete(iocp::service *svr, iocp::operation *op,
		const iocp::error_code &ec, size_t bytes_transferred);

	static ssl::engine::want send_op(operation *op_, ssl::engine &engine,
		iocp::error_code &ec, size_t &bytes_transferred)
	{
		ssl_send_op *op = static_cast<ssl_send_op *>(op_);
		ssl::engine::want want = engine.write(op->buffer_.raw(), op->buffer_.len(), bytes_transferred, ec);
		if (!ec && bytes_transferred > 0)
			op->buffer_.consume(bytes_transferred);
		return want;
	}

	const char *buf_start_pos_;
	const_buffer buffer_;
	iocp::ssl_socket::send_cb_type callback_;
};

class ssl_receive_op: public ssl_base_op
{
public:
	ssl_receive_op(iocp::ssl_socket &socket, char *buf, size_t len, bool some,
		iocp::ssl_socket::receive_cb_type callback, void *binded)
		: ssl_base_op(socket, &ssl_receive_op::receive_op, 
		&ssl_receive_op::on_complete, binded), some_(some),
		buf_start_pos_(buf), buffer_(buf, len, 0), callback_(callback) {}

private:
	static void on_complete(iocp::service *svr, iocp::operation *op,
		const iocp::error_code &ec, size_t bytes_transferred);

	static ssl::engine::want receive_op(operation *op_, ssl::engine &engine,
		iocp::error_code &ec, size_t &bytes_transferred)
	{
		ssl_receive_op *op = static_cast<ssl_receive_op *>(op_);
		size_t remain = op->buffer_.remain();
		ssl::engine::want want = engine.read(op->buffer_.raw_pos(),
			remain, bytes_transferred, ec);
		if (!ec && bytes_transferred > 0)
			op->buffer_.append(bytes_transferred);
		return want;
	}

	bool some_;
	char *buf_start_pos_;
	mutable_buffer buffer_;

	iocp::ssl_socket::receive_cb_type callback_;
};

class ssl_shutdown_op: public ssl_base_op
{
public:
	ssl_shutdown_op(iocp::ssl_socket &socket,
		iocp::ssl_socket::shutdown_cb_type callback, void *binded)
		: ssl_base_op(socket, &ssl_shutdown_op::shutdown_op,
		&ssl_shutdown_op::on_complete, binded), callback_(callback) {}

private:
	static void on_complete(iocp::service *svr, iocp::operation *op,
		const iocp::error_code &ec, size_t bytes_transferred);

	static ssl::engine::want shutdown_op(operation *op_, ssl::engine &engine,
		iocp::error_code &ec, size_t &)
	{
		return engine.shutdown(ec);
	}


	iocp::ssl_socket::shutdown_cb_type callback_;
};

#pragma warning(pop)

}

#endif // _NET_ENABLE_SSL_

#endif

// ssl_socket.cpp
#ifdef _NET_ENABLE_SSL_

#ifndef _IOCP_NO_INCLUDE_
#include "ssl_socket.h"
#include "net_service.h"
#include <limits.h>
#endif

namespace ssl {
namespace error {

std::string ssl_error_descripter(int value)
{
	const char* s = ::ERR_reason_error_string(value);
	//const char* s = ::ERR_error_string(value, 0);
	return s ? s : "ssl error";
}

}

	iocp::mutex *ssl_mutex;

	void SSL_Locking_CallBack(int mode, int type, const char *file, int line)
	{
		if (mode & CRYPTO_LOCK)
			ssl_mutex[type].lock();
		else if (mode & CRYPTO_UNLOCK)
			ssl_mutex[type].unlock();
	}

	// openssl seems to release resources itself when process terminated
	void init()
	{
		SSL_load_error_strings();
		ERR_load_BIO_strings();
		ERR_load_crypto_strings();
		ERR_load_SSL_strings();
		OpenSSL_add_all_algorithms();

		SSL_library_init();

		// needed for multi-thread
		ssl_mutex = new iocp::mutex[CRYPTO_num_locks()];
		CRYPTO_set_locking_callback(SSL_Locking_CallBack);
	}

	context::context(ssl::method m, iocp::error_code &ec): impl_(0)
	{
		switch (m)
		{
		case sslv2:
			impl_ = ::SSL_CTX_new(::SSLv2_method());
			break;
		case sslv2_client:
			impl_ = ::SSL_CTX_new(::SSLv2_client_method());
			break;
		case sslv2_server:
			impl_ = ::SSL_CTX_new(::SSLv2_server_method());
			break;
		case sslv3:
			impl_ = ::SSL_CTX_new(::SSLv3_method());
			break;
		case sslv3_client:
			impl_ = ::SSL_CTX_new(::SSLv3_client_method());
			break;
		case sslv3_server:
			impl_ = ::SSL_CTX_new(::SSLv3_server_method());
			break;
		case tlsv1:
			impl_ = ::SSL_CTX_new(::TLSv1_method());
			break;
		case tlsv1_client:
			impl_ = ::SSL_CTX_new(::TLSv1_client_method());
			break;
		case tlsv1_server:
			impl_ = ::SSL_CTX_new(::TLSv1_server_method());
			break;
		case sslv23:
			impl_ = ::SSL_CTX_new(::SSLv23_method());
			break;
		case sslv23_client:
			impl_ = ::SSL_CTX_new(::SSLv23_client_method());
			break;
		case sslv23_server:
			impl_ = ::SSL_CTX_new(::SSLv23_server_method());
			break;
		default:
			impl_ = ::SSL_CTX_new(0);
			break;
		}

		if (impl_ == 0) {
			ec.set_value(::ERR_get_error());
			ec.set_describer(ssl::error::ssl_error_descripter);
		}
	}

	context::~context()
	{
		if (impl_) {
			::SSL_CTX_free(impl_);
		}
	}

	void context::set_options(options o)
	{
		::SSL_CTX_set_options(impl_, o);
	}

	void context::set_verify_mode(verify_mode v)
	{
		::SSL_CTX_set_verify(impl_, v, ::SSL_CTX_get_verify_callback(impl_));
	}

	void context::set_verify_callback(verify_cb callback)
	{
		::SSL_CTX_set_verify(impl_, ::SSL_CTX_get_verify_mode(impl_), callback);
	}

	long context::load_verify_file(const char *filename)
	{
		if (::SSL_CTX_load_verify_locations(impl_, filename, 0)  != 1)
			return ::ERR_get_error();
		return 0;
	}

	long context::set_default_verify_paths()
	{
		if (::SSL_CTX_set_default_verify_paths(impl_) != 1)
			return ::ERR_get_error();
		return 0;
	}

	long context::add_verify_path(const char *path)
	{
		if (::SSL_CTX_load_verify_locations(impl_, 0, path) != 1)
			return ::ERR_get_error();
		return 0;
	}

	long context::use_certificate_file(const char *filename, file_format format)
	{
		int file_type;
		switch (format)
		{
		case ssl::asn1:
			file_type = SSL_FILETYPE_ASN1;
			break;
		case ssl::pem:
			file_type = SSL_FILETYPE_PEM;
			break;
		default:
			return -1;
		}

		if (::SSL_CTX_use_certificate_file(impl_, filename, file_type)  != 1)
			return ::ERR_get_error();
		return 0;
	}

	long context::use_certificate_chain_file(const char *filename)
	{
		if (::SSL_CTX_use_certificate_chain_file(impl_, filename) != 1)
			return ::ERR_get_error();
		return 0;
	}

	long context::use_private_key_file(const char *filename, file_format format)
	{
		int file_type;
		switch (format)
		{
		case ssl::asn1:
			file_type = SSL_FILETYPE_ASN1;
			break;
		case ssl::pem:
			file_type = SSL_FILETYPE_PEM;
			break;
		default:
			return -1;
		}

		if (::SSL_CTX_use_PrivateKey_file(impl_, filename, file_type) != 1)
			return ::ERR_get_error();
		return 0;
	}

	long context::use_rsa_private_key_file(const char *filename, file_format format)
	{
		int file_type;
		switch (format)
		{
		case ssl::asn1:
			file_type = SSL_FILETYPE_ASN1;
			break;
		case ssl::pem:
			file_type = SSL_FILETYPE_PEM;
			break;
		default:
			return -1;
		}

		if (::SSL_CTX_use_RSAPrivateKey_file(impl_, filename, file_type) != 1)
			return ::ERR_get_error();
		return 0;
	}

	long context::use_tmp_dh_file(const char *filename)
	{
		::BIO *bio = ::BIO_new_file(filename, "r");
		if (!bio)
		{
			return -1;
		}

		::DH* dh = ::PEM_read_bio_DHparams(bio, 0, 0, 0);
		if (!dh)
		{
			::BIO_free(bio);
			return -1;
		}

		::BIO_free(bio);
		int result = ::SSL_CTX_set_tmp_dh(impl_, dh);
		::DH_free(dh);
		if (result != 1)
		{
			return -1;
		}

		return 0;
	}

	void context::set_password_callback(pem_password_cb callback)
	{
		::SSL_CTX_set_default_passwd_cb(impl_, callback);
	}

	long context::set_cipher_list(const char *list)
	{
		if (::SSL_CTX_set_cipher_list(impl_, list))
			return 0;
		return -1;
	}

#ifndef OPENSSL_NO_TLSEXT
	int context::ssl_servername_cb(SSL *s, int *ad, void *arg)
	{
		context *ctx = static_cast<context *>(arg);
		const char *servername = SSL_get_servername(s, TLSEXT_NAMETYPE_host_name);
		if (!servername) {
			// default context
			SSL_set_SSL_CTX(s, ctx->impl_);
			return SSL_TLSEXT_ERR_OK;
		}

		std::map<std::string, context *>::iterator it;
		if ((it = ctx->sni_map_.find(servername)) != ctx->sni_map_.end())
			SSL_set_SSL_CTX(s, it->second->impl_);
		else	// default context
			SSL_set_SSL_CTX(s, ctx->impl_);

		return SSL_TLSEXT_ERR_OK;
	}

	long context::set_server_context_pair(const char *server_name, context &ctx)
	{
		long err = SSL_ERROR_NONE;
		if (sni_map_.find("default") == sni_map_.end()) {
			// uninitialized
			if (::SSL_CTX_set_tlsext_servername_callback(impl_, &context::ssl_servername_cb) != 1)
				err = ::ERR_get_error();
			else if (::SSL_CTX_set_tlsext_servername_arg(impl_, this) != 1)
				err = ::ERR_get_error();
			else
				sni_map_["default"] = this;
			if (err)
				return err;
		}
		ctx.sni_map_["default"] = this;
		if (::SSL_CTX_set_tlsext_servername_callback(ctx.impl_, &context::ssl_servername_cb) != 1)
			err = ::ERR_get_error();
		else if (::SSL_CTX_set_tlsext_servername_arg(ctx.impl_, this) != 1)
			err = ::ERR_get_error();
		else
			sni_map_[server_name] = &ctx;

		return err;
	}
#endif

	//////////////////////////////////////////////////////////////////////////

	engine::engine(context &ctx, iocp::error_code &ec)
		: ssl_(::SSL_new(ctx.impl()))
	{
		//accept_mutex().init();
		if (ssl_ == 0) {
			ec.set_value(::ERR_get_error());
			ec.set_describer(ssl::error::ssl_error_descripter);
			return;
		}

		::SSL_set_mode(ssl_, SSL_MODE_ENABLE_PARTIAL_WRITE);
		::SSL_set_mode(ssl_, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
		::BIO* int_bio = 0;
		::BIO_new_bio_pair(&int_bio, 0, &ext_bio_, 0);
		::SSL_set_bio(ssl_, int_bio, int_bio);
	}

	engine::~engine()
	{
		// 	if (SSL_get_app_data(ssl_))
		// 	{
		// 		delete static_cast<verify_callback_base*>(SSL_get_app_data(ssl_));
		// 		SSL_set_app_data(ssl_, 0);
		// 	}

		::BIO_free(ext_bio_);
		::SSL_free(ssl_);
	}

	iocp::mutex engine::accept_lock_;

	engine::want engine::handshake(ssl::handshake_type type,
		iocp::error_code &ec)
	{
		return perform((type == ssl::client)
			? &engine::do_connect : &engine::do_accept, 0, 0, ec, 0);
	}

	engine::want engine::shutdown(iocp::error_code &ec)
	{
		// if we need the data that the other side send
		// we should FIXME: take too long recv the return data
		engine::want result = perform(&engine::do_shutdown, 0, 0, ec, 0);
 		if (result == engine::want_input_and_retry)	// we do not care about what other side return
 			return want_nothing;
		return result;
	}

	engine::want engine::write(const char *data, size_t len,
		size_t &bytes_transferred, iocp::error_code &ec)
	{
		return perform(&engine::do_write, (void *)data, len, ec, &bytes_transferred);
	}

	engine::want engine::read(char *data, size_t len,
		size_t &bytes_transferred, iocp::error_code &ec)
	{
		return perform(&engine::do_read, data, len, ec, &bytes_transferred);
	}

	size_t engine::get_output(char *data, size_t len)
	{
		int length = 0;
		if (BIO_pending(ext_bio_) > 0)	// do not return unnecessary negative number
			length = ::BIO_read(ext_bio_, static_cast<void *>(data), static_cast<int>(len));

		return static_cast<size_t>(length);
	}

	size_t engine::put_input(const char *data, size_t len)
	{
		int length = ::BIO_write(ext_bio_, static_cast<const void *>(data), static_cast<int>(len));

		return static_cast<size_t>(length);
	}

	void engine::map_error_code(iocp::error_code& ec)
	{
		// We only want to map the error::eof code.
		if (ec.value() != iocp::error::eof)
			return;

		// If there's data yet to be read, it's an error.
		if (BIO_wpending(ext_bio_))
		{
			ec.set_value(ERR_PACK(ERR_LIB_SSL, 0, SSL_R_SHORT_READ));
			ec.set_describer(ssl::error::ssl_error_descripter);
			return;
		}

		// SSL v2 doesn't provide a protocol-level shutdown, so an eof on the
		// underlying transport is passed through.
		if (ssl_ && ssl_->version == SSL2_VERSION)
			return;

		// Otherwise, the peer should have negotiated a proper shutdown.
		ec.set_value(ERR_PACK(ERR_LIB_SSL, 0, SSL_R_SHORT_READ));
		ec.set_describer(ssl::error::ssl_error_descripter);
		return;
	}

	engine::want engine::perform(int (engine::* op)(void*, size_t),
		void* data, size_t length, iocp::error_code &ec,
		size_t *bytes_transferred)
	{
		size_t pending_output_before = ::BIO_ctrl_pending(ext_bio_);
		//size_t pending_input_before = ::BIO_ctrl_wpending(ext_bio_);
		int result = (this->*op)(data, length);
		int ssl_error = ::SSL_get_error(ssl_, result);
		int sys_error = ::ERR_get_error();
		size_t pending_output_after = ::BIO_ctrl_pending(ext_bio_);
		//size_t pending_input_after = ::BIO_ctrl_wpending(ext_bio_);

		if (ssl_error == SSL_ERROR_SSL)
		{
			ec.set_value(sys_error);
			ec.set_describer(ssl::error::ssl_error_descripter);
			return want_nothing;
		}

		if (ssl_error == SSL_ERROR_SYSCALL)
		{
			if (op == &engine::do_shutdown && sys_error == 0) {
				ssl_error = SSL_ERROR_NONE;
			}
			else {
				ec.set_value(sys_error);
				ec.set_describer(ssl::error::ssl_error_descripter);
				return want_nothing;
			}
		}

		if (result > 0 && bytes_transferred)
			*bytes_transferred = static_cast<size_t>(result);
		else if (bytes_transferred)
			*bytes_transferred = 0;

		if (ssl_error == SSL_ERROR_WANT_WRITE)
		{
			ec.set_value(0);
			ec.set_describer(ssl::error::ssl_error_descripter);
			return want_output_and_retry;
		}
		else if (pending_output_after > pending_output_before && op != &engine::do_read)
		{
			ec.set_value(0);
			ec.set_describer(ssl::error::ssl_error_descripter);
// 			if (ssl_error == SSL_ERROR_WANT_READ)
// 				return want_output_and_input_and_retry;
// 			else
				return result > 0 ? want_output : want_output_and_retry;
		}
		else if (ssl_error == SSL_ERROR_WANT_READ)
		{
			ec.set_value(0);
			ec.set_describer(ssl::error::ssl_error_descripter);
			return want_input_and_retry;
		}
		else if (::SSL_get_shutdown(ssl_) & SSL_RECEIVED_SHUTDOWN)
		{
			ec.set_value(iocp::error::eof);
			ec.set_describer(iocp::error::frame_error_describer);
			return want_nothing;
		}
		else
		{
			ec.set_value(ssl_error);
			ec.set_describer(ssl::error::ssl_error_descripter);
			return want_nothing;
		}
	}

	int engine::do_accept(void*, size_t)
	{
		//iocp::scope_lock lock(engine::accept_lock_);
		return ::SSL_accept(ssl_);
	}

	int engine::do_connect(void*, size_t)
	{
		return ::SSL_connect(ssl_);
	}

	int engine::do_shutdown(void*, size_t)
	{
		int result = ::SSL_shutdown(ssl_);
		if (result == 0)
			result = ::SSL_shutdown(ssl_);
		return result;
	}

	int engine::do_read(void* data, size_t length)
	{
		return ::SSL_read(ssl_, data, length < INT_MAX ? static_cast<int>(length) : INT_MAX);
	}

	int engine::do_write(void* data, size_t length)
	{
		return ::SSL_write(ssl_, data, length < INT_MAX ? static_cast<int>(length) : INT_MAX);
	}
}

namespace iocp {

#include <stdlib.h>	// for memmove
#include <assert.h> // for assert

void ssl_base_op::post_callback(void *binded, const iocp::error_code &)
{
	ssl_base_op *ssl_op = static_cast<ssl_base_op *>(binded);
	ssl_op->complete(ssl_op->socket_.service(), ssl_op->ec_, ssl_op->bytes_transferred_);
}

void ssl_base_op::receive_callback(void *binded, const iocp::error_code &ec, size_t bytes_transferred, char *buffer)
{
	ssl_base_op *ssl_op = static_cast<ssl_base_op *>(binded);
	//iocp::scope_cs sc(ssl_op->socket_.cs_);
	ssl_op->io_operation(ec, bytes_transferred);
}

void ssl_base_op::send_callback(void *binded, const iocp::error_code &ec, size_t bytes_transferred)
{
	ssl_base_op *ssl_op = static_cast<ssl_base_op *>(binded);
	//iocp::scope_cs sc(ssl_op->socket_.cs_);
	ssl_op->io_operation(ec, bytes_transferred);
}

void ssl_base_op::io_operation(const iocp::error_code ec, size_t bytes_transferred, int start)
{
	switch (start) {
	case 1: // Called after at least one async operation.
	do
	{
		switch (want_ = op_(this, socket_.engine_, ec_, bytes_transferred_))
		{
		//case ssl::engine::want_output_and_input_and_retry:
		case ssl::engine::want_input_and_retry:
			{
				// If the input buffer already has data in it we can pass it to the
				// engine and then retry the operation immediately.
				if (socket_.input_buffer_len_ != 0)
				{
					size_t result = socket_.engine_.put_input(socket_.input_buffer_,
						socket_.input_buffer_len_);
					if (result > 0) {
						socket_.input_buffer_len_ -= result;
						memmove(socket_.input_buffer_,\
							socket_.input_buffer_ + result, socket_.input_buffer_len_);
					}
					continue;
				}

				// The engine wants more data to be read from input. However, we
				// cannot allow more than one read operation at a time on the
				// underlying transport.
				if (!sync_compare_and_swap(&socket_.input_flag_, 0, 1))
					assert(!"still receiving");
				
				socket_.stream_.asyn_recv_some(
					socket_.input_buffer_ + socket_.input_buffer_len_,
					ssl_socket::ssl_buffer_size - socket_.input_buffer_len_,
					&ssl_base_op::receive_callback, this);

				// Yield control until asynchronous operation completes. Control
				// resumes at the "default:" label below.
				//if (want_ != ssl::engine::want_output_and_input_and_retry)
					return;
				// continue outputting
			}

		case ssl::engine::want_output_and_retry:
		case ssl::engine::want_output:

			{
				// The engine wants some data to be written to the output. However, we
				// cannot allow more than one write operation at a time on the
				// underlying transport.
				if (!sync_compare_and_swap(&socket_.output_flag_, 0, 1))
					assert(!"still sending");

				size_t result = socket_.engine_.get_output(socket_.output_buffer_,
					socket_.output_buffer_len_);
				if (result > 0) {
					socket_.stream_.asyn_send(socket_.output_buffer_,
						result, &ssl_base_op::send_callback, this);
				}
				else {
					sync_set(&socket_.output_flag_, 0);
				}

				// Yield control until asynchronous operation completes. Control
				// resumes at the "default:" label below.
				return;
			}

		default:

			// The SSL operation is done and we can invoke the handler, but we
			// have to keep in mind that this function might be being called from
			// the async operation's initiating function. In this case we're not
			// allowed to call the handler directly. Instead, issue a zero-sized
			// read so the handler runs "as-if" posted using io_service::post().
			if (start)
			{
				// TODO: FIXME: post callback directly
				//socket_.stream_.asyn_recv_some(socket_.input_buffer_, 0,
					//&ssl_base_op::receive_callback, this);
				socket_.service().post(&ssl_base_op::post_callback, this);

				// Yield control until asynchronous operation completes. Control
				// resumes at the "default:" label below.
				return;
			}
			else
			{
				// Continue on to run handler directly.
				break;
			}
		}

	default:
			if (bytes_transferred != ~size_t(0) && !ec_)
				ec_ = ec;

			switch (want_)
			{
			case ssl::engine::want_input_and_retry:
				{
					// Add received data to the engine's input.
					sync_set(&socket_.input_flag_, 0);
					socket_.input_buffer_len_ += bytes_transferred;
					size_t result = socket_.engine_.put_input(socket_.input_buffer_,
						socket_.input_buffer_len_);
					if (result > 0) {
						socket_.input_buffer_len_ -= result;
						memmove(socket_.input_buffer_,\
							socket_.input_buffer_ + result, socket_.input_buffer_len_);
					}
				}

				// Release any waiting read operations.
				//core_.pending_read_.expires_at(boost::posix_time::neg_infin);

				// Try the operation again.
				continue;

			case ssl::engine::want_output_and_retry:
			case ssl::engine::want_output:
				{
					size_t result = socket_.engine_.get_output(socket_.output_buffer_,
						socket_.output_buffer_len_);
					if (result > 0) {
						socket_.stream_.asyn_send(socket_.output_buffer_,
							result, &ssl_base_op::send_callback, this);
						return;
					}
					sync_set(&socket_.output_flag_, 0);

					// Release any waiting write operations.
					//core_.pending_write_.expires_at(boost::posix_time::neg_infin);

					// Try the operation again.
					if (want_ == ssl::engine::want_output_and_retry)
						continue;
					// Fall through to call handler.
				}
			default:

				// Pass the result to the handler.
				socket_.engine_.map_error_code(ec_);
				this->complete(socket_.stream_.service(), ec_, ec_ ? 0 : bytes_transferred_);
// 				if (ec)
// 					bytes_transferred_ = 0;
// 				socket_.stream().service().post(&ssl_base_op::post_callback, this);

				// Our work here is done.
				return;
			}
		} while (!ec_);

		// Operation failed. Pass the result to the handler.
		socket_.engine_.map_error_code(ec_);
		this->complete(socket_.stream_.service(), ec_, 0);
// 		bytes_transferred_ = 0;
// 		socket_.stream().service().post(&ssl_base_op::post_callback, this);
	}
}

void ssl_handshake_op::on_complete(iocp::service *svr,
																	 iocp::operation *op_,
																	 const iocp::error_code &ec,
																	 size_t bytes_transferred)
{
	iocp::ssl_handshake_op *op = static_cast<iocp::ssl_handshake_op *>(op_);
	op->callback_(op->argument(), ec);
	delete op;
}

void ssl_send_op::on_complete(iocp::service *svr,
															iocp::operation *op_,
															const iocp::error_code &ec,
															size_t bytes_transferred)
{
	iocp::ssl_send_op *op = static_cast<iocp::ssl_send_op *>(op_);
	if (!ec && op->buffer_.len()) {
		op->do_ssl_operation();
	}
	else {
		op->callback_(op->argument(), ec, static_cast<size_t>(op->buffer_.raw() - op->buf_start_pos_));
		delete op;
	}
}

void ssl_receive_op::on_complete(iocp::service *svr,
																 iocp::operation *op_,
																 const iocp::error_code &ec,
																 size_t bytes_transferred)
{
	iocp::ssl_receive_op *op = static_cast<iocp::ssl_receive_op *>(op_);

	if (!ec && !op->some_ && op->buffer_.remain()) {
		op->do_ssl_operation();
	}
	else {
		op->callback_(op->argument(), ec, op->buffer_.len(), op->buf_start_pos_);
		delete op;
	}
}

void ssl_shutdown_op::on_complete(iocp::service *svr,
																	iocp::operation *op_,
																	const iocp::error_code &ec,
																	size_t bytes_transferred)
{
	iocp::ssl_shutdown_op *op = static_cast<iocp::ssl_shutdown_op *>(op_);
	iocp::error_code ec_ = ec;
	if (ec.value() == ERR_PACK(ERR_LIB_SSL, 0, SSL_R_SHORT_READ))
		ec_.set_value(SSL_ERROR_NONE);
	op->callback_(op->argument(), ec_);
	delete op;
}

void ssl_socket::asyn_handshake(ssl::handshake_type type, handshake_cb_type callback, void *binded)
{
	ssl_handshake_op *op = new ssl_handshake_op(*this, type, callback, binded);
	op->do_ssl_operation();
}

void ssl_socket::asyn_send(const char *buf, size_t len, ssl_socket::send_cb_type callback, void *binded)
{
	ssl_send_op *op = new ssl_send_op(*this, buf, len, callback, binded);
	op->do_ssl_operation();
}

void ssl_socket::asyn_recv_some(char *buf, size_t len, ssl_socket::receive_cb_type callback, void *binded)
{
	ssl_receive_op *op = new ssl_receive_op(*this, buf, len, true, callback, binded);
	op->do_ssl_operation();
}

void ssl_socket::asyn_recv(char *buf, size_t len, ssl_socket::receive_cb_type callback, void *binded)
{
	ssl_receive_op *op = new ssl_receive_op(*this, buf, len, false, callback, binded);
	op->do_ssl_operation();
}

void ssl_socket::asyn_shutdown(ssl_socket::shutdown_cb_type callback, void *binded)
{
	ssl_shutdown_op *op = new ssl_shutdown_op(*this, callback, binded);
	op->do_ssl_operation();
}

}

#endif // _NET_ENABLE_SSL_

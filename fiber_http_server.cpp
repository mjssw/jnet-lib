#include "fiber_http_server.h"

FiberWebServer::FiberWebServer()
{
	ssl::init();
	fiber::scheduler::convert_thread(0);
	server_context_ = new ssl::context(ssl::sslv23_server);
	client_context_ = new ssl::context(ssl::sslv23_client);
	threads_.push_back(new iocp::thread(http_thread_proc, &service_, false));
}

FiberWebServer::~FiberWebServer()
{
	delete server_context_;
	delete client_context_;
}

int FiberWebServer::AppendThread(int num)
{
	for (int i = 0; i < num; ++i)
		threads_.push_back(new iocp::thread(http_thread_proc, &service_, false));
	return static_cast<int>(threads_.size());
}

int FiberWebServer::SetCertificate(const char *pszCertChain, const char *pszPrivateKey)
{
	long ec;
	ec = server_context_->use_certificate_chain_file(pszCertChain);
	if (!ec)
		ec = server_context_->use_private_key_file(pszPrivateKey, ssl::pem);
	return ec;
}

int FiberWebServer::SetCipherList(const char *pszCipher)
{
	return server_context_->set_cipher_list(pszCipher);
}

#ifndef OPENSSL_NO_TLSEXT
int FiberWebServer::AddVirtualHost(const char *servername,
																	 const char *pszCertChain,
																	 const char *pszPrivateKey,
																	 const char *pszCipher)
{
	long ec;
	ssl::context *ctx = new ssl::context(ssl::sslv23_server);
	ec = ctx->use_certificate_chain_file(pszCertChain);
	if (!ec)
		ec = ctx->use_private_key_file(pszPrivateKey, ssl::pem);
	if (!ec && pszCipher)
		ec = ctx->set_cipher_list(pszCipher);
	if (ec)
		delete ctx;
	else
		server_context_->set_server_context_pair(servername, *ctx);
	return ec;
}
#endif

int FiberWebServer::startHTTP(const char *pszIP, unsigned short port, int backlog, FiberConContext::fiber_proc proc)
{
	iocp::error_code ec;
	http_acceptor_= new iocp::fiber_acceptor(service_);
	ec = http_acceptor_->bind_and_listen(pszIP, port, backlog);
	if (!ec) {
		http_proc_ = proc;
		service_.post(&FiberWebServer::on_http_need_accept, this);
		run();
	}
	return ec.value();
}

int FiberWebServer::startHTTPS(const char *pszIP, unsigned short port, int backlog, FiberConContext::fiber_proc proc)
{
	iocp::error_code ec;
	https_acceptor_= new iocp::fiber_acceptor(service_);
	ec = https_acceptor_->bind_and_listen(pszIP, port, backlog);
	if (!ec) {
		https_proc_= proc;
		service_.post(&FiberWebServer::on_https_need_accept, this);
		run();
	}
	return ec.value();
}

int FiberWebServer::connect(const char *pszIP, unsigned short port, bool use_ssl, void *ctx, FiberConContext::fiber_proc proc)
{
	iocp::error_code ec;
	FiberConContext *cctx = new FiberConContext(*this, use_ssl ? FiberConContext::ssl : FiberConContext::tcp);
	cctx->proc_ = proc;
	cctx->context_ = ctx;
	cctx->ip_ = pszIP;
	cctx->port_ = port;
	service_.post(&FiberWebServer::on_need_connect, cctx);
	run();
	return ec.value();
}

int FiberWebServer::run()
{
	for (size_t i = 0; i < threads_.size(); ++i)
		threads_[i]->start();
	return static_cast<int>(threads_.size());
}

FiberConContext::FiberConContext(FiberWebServer &svr, stream_type type)
: type_(type), shutdown_flag_(0), close_flag_(0)
{
	server_ = &svr;
	if (type_ & FiberConContext::ssl) {
		if (type_ == FiberConContext::https)
			ssl_stream_ = ssl_stream_ = new iocp::fiber_ssl_socket(*svr.server_context_, svr.service_);
		else
			ssl_stream_ = ssl_stream_ = new iocp::fiber_ssl_socket(*svr.client_context_, svr.service_);
		tcp_stream_ = 0;
	}
	else {
		ssl_stream_ = 0;
		tcp_stream_ = new iocp::fiber_socket(svr.service_);
	}
}

FiberConContext::~FiberConContext()
{
	if (type_ & FiberConContext::ssl)
		delete ssl_stream_;
	else
		delete tcp_stream_;
}

iocp::error_code FiberConContext::Connect(const char *pszIP, unsigned short port)
{
	if (!check_closed()) {
		if (type_ & FiberConContext::ssl)
			return ssl_stream_->connect(pszIP, port);
		else
			return tcp_stream_->connect(pszIP, port);
	}
	return iocp::error_code();
}

void FiberConContext::fiber_acceptor_proc_wrapper(void *c)
{
	FiberConContext *ctx = static_cast<FiberConContext *>(c);
	ctx->proc_(ctx, ctx->accept_ec_);
	delete ctx;
}

void FiberConContext::fiber_connect_proc_wrapper(void *c)
{
	FiberConContext *ctx = static_cast<FiberConContext *>(c);
	ctx->proc_(ctx, ctx->Connect(ctx->ip_.c_str(), ctx->port_));
	delete ctx;
}

iocp::error_code FiberConContext::Handshake()
{
	if (!check_closed() && type_ & FiberConContext::ssl) {
		if (type_ == FiberConContext::https)
			return ssl_stream_->handshake(ssl::server);
		else
			return ssl_stream_->handshake(ssl::client);
	}
	return iocp::error_code();
}

iocp::error_code FiberConContext::Shutdown()
{
	if (!check_closed() && type_ & FiberConContext::ssl) {
		if (sync_compare_and_swap(&shutdown_flag_, 0, 1))
			return ssl_stream_->shutdown();
	}
	return iocp::error_code();
}

iocp::error_code FiberConContext::Recv(char *buf, int len, size_t &bytes_transferred)
{
	if (check_closed())
		return iocp::error_code();
	if (type_ & FiberConContext::ssl)
		return ssl_stream_->recv_some(buf, len, bytes_transferred);
	else
		return tcp_stream_->recv_some(buf, len, bytes_transferred);
}

iocp::error_code FiberConContext::Send(const char *buf, int len, size_t &bytes_transferred)
{
	if (check_closed())
		return iocp::error_code();
	if (type_ & FiberConContext::ssl)
		return ssl_stream_->send(buf, len, bytes_transferred);
	else
		return tcp_stream_->send(buf, len, bytes_transferred);
}

iocp::error_code FiberConContext::Close()
{
	sync_set(&close_flag_, 1);
	if (type_ & FiberConContext::ssl)
		return ssl_stream_->close();
	else
		return tcp_stream_->close();
}

bool FiberConContext::check_closed()
{
	if (sync_get(&close_flag_) == 1)
		return true;
	else if (type_ & FiberConContext::ssl && sync_get(&shutdown_flag_) == 1)
		return true;
	return false;
}

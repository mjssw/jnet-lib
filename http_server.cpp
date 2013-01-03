#include "http_server.h"
#include "iocp_lock.h"

IOCPWebServer::IOCPWebServer()
{
	ssl::init();
	server_context_ = new ssl::context(ssl::sslv23_server);
	client_context_ = new ssl::context(ssl::sslv23_client);
	threads_.push_back(new iocp::thread(http_thread_proc, &service_, false));
}

IOCPWebServer::~IOCPWebServer()
{
	delete server_context_;
	delete client_context_;
}

int IOCPWebServer::AppendThread(int num)
{
	for (int i = 0; i < num; ++i)
		threads_.push_back(new iocp::thread(http_thread_proc, &service_, false));
	return static_cast<int>(threads_.size());
}

int IOCPWebServer::SetCertificate(const char *pszCertChain, const char *pszPrivateKey)
{
	long ec;
	ec = server_context_->use_certificate_chain_file(pszCertChain);
	if (!ec)
		ec = server_context_->use_private_key_file(pszPrivateKey, ssl::pem);
	return ec;
}

int IOCPWebServer::SetCipherList(const char *pszCipher)
{
	return server_context_->set_cipher_list(pszCipher);
}

void IOCPWebServer::on_connect(void *binded, const iocp::error_code &ec)
{
	ConnectionContext *ctx = static_cast<ConnectionContext *>(binded);
	ctx->ec_ = ec;
	int ret = 0;
	if (ctx->server_->cbOnConnected)
		ret = ctx->server_->cbOnConnected(ctx);
	if (ret)
		ctx->Close();
	else if (ctx->type_ == ConnectionContext::ssl)
		ctx->Handshake();
	ctx->server_->CheckAndDelete(ctx);
}

void IOCPWebServer::on_http_accept(void *acceptor_point, const iocp::error_code &ec, void *socket_point)
{
	ConnectionContext *ctx = static_cast<ConnectionContext *>(socket_point);
	if (ec) {
		std::cerr << ec.to_string() << std::endl;
		delete ctx;
	}
	else {
		IOCPWebServer *svr = static_cast<IOCPWebServer *>(acceptor_point);
		int ret = 0;
		if (ctx->server_->cbOnConnected)
			ret = ctx->server_->cbOnConnected(ctx);
		if (ret)
			ctx->Close();
		svr->CheckAndDelete(ctx);

		if (!ret) {
			ConnectionContext *nctx = new ConnectionContext(*svr, ConnectionContext::http);
			nctx->ref_ = 1;
			svr->http_acceptor_->asyn_accept(*nctx->tcp_stream_, nctx, &IOCPWebServer::on_http_accept, svr);
		}
	}
}

void IOCPWebServer::on_https_accept(void *acceptor_point, const iocp::error_code &ec, void *socket_point)
{
	ConnectionContext *ctx = static_cast<ConnectionContext *>(socket_point);
	if (ec) {
		std::cerr << ec.to_string() << std::endl;
		delete ctx;
	}
	else {
		IOCPWebServer *svr = static_cast<IOCPWebServer *>(acceptor_point);
		int ret = 0;
		if (ctx->server_->cbOnConnected)
			ret = ctx->server_->cbOnConnected(ctx);
		if (ret)
			ctx->Close();
		else
			ctx->Handshake();
		svr->CheckAndDelete(ctx);

		if (!ret) {
			ConnectionContext *nctx = new ConnectionContext(*svr, ConnectionContext::https);
			nctx->ref_ = 1;
			svr->https_acceptor_->asyn_accept(*nctx->ssl_stream_, nctx, &IOCPWebServer::on_https_accept, svr);
		}
	}
}

void IOCPWebServer::on_handshake(void *binded, const iocp::error_code &ec)
{
	ConnectionContext *ctx = static_cast<ConnectionContext *>(binded);
	ctx->ec_ = ec;
	int ret = 0;
	if (ctx->server_->cbOnHandshaked)
		ret = ctx->server_->cbOnHandshaked(ctx);
	if (ret)
		ctx->Close();
	ctx->server_->CheckAndDelete(ctx);
}

void IOCPWebServer::on_shutdown(void *binded, const iocp::error_code &ec)
{
	ConnectionContext *ctx = static_cast<ConnectionContext *>(binded);
	ctx->server_->CheckAndDelete(ctx);
}

void IOCPWebServer::on_send(void *binded, const iocp::error_code &ec, size_t bytes_transferred)
{
	ConnectionContext *ctx = static_cast<ConnectionContext *>(binded);
	ctx->bytes_transferred_ = bytes_transferred;
	ctx->ec_ = ec;
	int ret = 0;
	if (ctx->server_->cbOnSentBytes)
		ret = ctx->server_->cbOnSentBytes(ctx);
	if (ret)
		ctx->Close();
	ctx->server_->CheckAndDelete(ctx);
}

void IOCPWebServer::on_receive(void *binded, const iocp::error_code &ec, size_t bytes_transferred, char *buf)
{
	ConnectionContext *ctx = static_cast<ConnectionContext *>(binded);
	ctx->bytes_transferred_ = bytes_transferred;
	ctx->ec_ = ec;
	ctx->bytes_received_ = buf;
	int ret = 0;
	if (ctx->server_->cbOnReceivedBytes)
		ret = ctx->server_->cbOnReceivedBytes(ctx);
	if (ret)
		ctx->Close();
	ctx->server_->CheckAndDelete(ctx);
}

int IOCPWebServer::startHTTP(const char *pszIP, unsigned short port, int backlog)
{
	iocp::error_code ec;
	http_acceptor_= new iocp::acceptor(service_);
	ec = http_acceptor_->bind_and_listen(pszIP, port, backlog);
	if (!ec) {
		ConnectionContext *ctx = new ConnectionContext(*this, ConnectionContext::http);
		ctx->ref_ = 1;
		http_acceptor_->asyn_accept(*ctx->tcp_stream_, ctx, &IOCPWebServer::on_http_accept, this);
		run();
	}
	return ec.value();
}

int IOCPWebServer::startHTTPS(const char *pszIP, unsigned short port, int backlog)
{
	iocp::error_code ec;
	https_acceptor_= new iocp::acceptor(service_);
	ec = https_acceptor_->bind_and_listen(pszIP, port, backlog);
	if (!ec) {
		ConnectionContext *ctx = new ConnectionContext(*this, ConnectionContext::https);
		ctx->ref_ = 1;
		https_acceptor_->asyn_accept(*ctx->ssl_stream_, ctx, &IOCPWebServer::on_https_accept, this);
		run();
	}
	return ec.value();
}

int IOCPWebServer::connect(const char *pszIP, unsigned short port, bool use_ssl, void *ctx)
{
	ConnectionContext *cctx = new ConnectionContext(*this,\
		use_ssl ? ConnectionContext::ssl : ConnectionContext::tcp);
	cctx->context_ = ctx;
	cctx->Connect(pszIP, port);
	run();
	return 0;
}

int IOCPWebServer::run()
{
	for (size_t i = 0; i < threads_.size(); ++i)
		threads_[i]->start();
	return static_cast<int>(threads_.size());
}

#include <assert.h>

void IOCPWebServer::CheckAndDelete(ConnectionContext *ctx)
{
	long ret = sync_dec(&ctx->ref_);
	assert(ret >= 0);
	if (ret != 0)
		return;
	if (ctx->type_ == ConnectionContext::https &&
		sync_get(&ctx->close_flag_) != 1 && sync_get(&ctx->shutdown_flag_) != 1)
			ctx->Shutdown();
	else {
		if (cbOnClosed)
			cbOnClosed(ctx);
		delete ctx;
	}
}

ConnectionContext::ConnectionContext(IOCPWebServer &svr, stream_type type)
	: type_(type), ref_(0), shutdown_flag_(0), close_flag_(0)
{
	server_ = &svr;
	if (type_ & ConnectionContext::ssl) {
		tcp_stream_ = 0;
		if (type_ == ConnectionContext::https)
			ssl_stream_ = new iocp::ssl_socket(*svr.server_context_, svr.service_);
		else
			ssl_stream_ = new iocp::ssl_socket(*svr.client_context_, svr.service_);
	}
	else {
		ssl_stream_ = 0;
		tcp_stream_ = new iocp::socket(svr.service_);
	}
}

ConnectionContext::~ConnectionContext()
{
	if (ssl_stream_)
		delete ssl_stream_;
	else
		delete tcp_stream_;
}

void ConnectionContext::Connect(const char *pszIP, unsigned short port)
{
	if (!check_closed()) {
		sync_inc(&ref_);
		if (ssl_stream_)
			ssl_stream_->asyn_connect(pszIP, port, &IOCPWebServer::on_connect, this);
		else
			tcp_stream_->asyn_connect(pszIP, port, &IOCPWebServer::on_connect, this);
	}
}

void ConnectionContext::Handshake()
{
	if (!check_closed() && type_ & ConnectionContext::ssl) {
		sync_inc(&ref_);
		if (type_ == ConnectionContext::https)
			ssl_stream_->asyn_handshake(ssl::server, &IOCPWebServer::on_handshake, this);
		else
			ssl_stream_->asyn_handshake(ssl::client, &IOCPWebServer::on_handshake, this);
	}
}

void ConnectionContext::Shutdown()
{
	if (!check_closed() && type_ & ConnectionContext::ssl) {
		if (sync_compare_and_swap(&shutdown_flag_, 0, 1))
			sync_inc(&ref_);
			ssl_stream_->asyn_shutdown(&IOCPWebServer::on_shutdown, this);
	}
}

void ConnectionContext::Recv(char *buf, int len)
{
	if (check_closed())
		return;
	sync_inc(&ref_);
	if (ssl_stream_)
		ssl_stream_->asyn_recv_some(buf, len, &IOCPWebServer::on_receive, this);
	else
		tcp_stream_->asyn_recv_some(buf, len, &IOCPWebServer::on_receive, this);
}

void ConnectionContext::Send(const char *buf, int len)
{
	if (check_closed())
		return;
	sync_inc(&ref_);
	if (ssl_stream_)
		ssl_stream_->asyn_send(buf, len, &IOCPWebServer::on_send, this);
	else
		tcp_stream_->asyn_send(buf, len, &IOCPWebServer::on_send, this);
}

void ConnectionContext::Close()
{
	sync_set(&close_flag_, 1);
	if (ssl_stream_)
		ssl_stream_->close();
	else
		tcp_stream_->close();
}

bool ConnectionContext::check_closed()
{
	if (sync_get(&close_flag_) == 1)
		return true;
	else if (type_ & ConnectionContext::ssl && sync_get(&shutdown_flag_) == 1)
		return true;
	return false;
}

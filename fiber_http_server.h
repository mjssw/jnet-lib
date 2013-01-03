#include "net_service.h"
#include "fiber_socket.h"
#include "iocp_thread.h"
#include <vector>
#include <string>
#include <iostream>

class FiberWebServer;
class FiberConContext
{
public:
	enum stream_type { tcp = 0, ssl = 2, http = 1, https = 3 };
	FiberConContext(FiberWebServer &svr, stream_type type);
	~FiberConContext();

	stream_type type() { return type_; }

	iocp::error_code Connect(const char *pszIP, unsigned short port);
	iocp::error_code Handshake();
	iocp::error_code Shutdown();
	iocp::error_code Recv(char *buf, int len, size_t &bytes_transferred);
	iocp::error_code Send(const char *buf, int len, size_t &bytes_transferred);
	iocp::error_code Close();

	/// use these variables when callback is invoked
	void *context_;
 	iocp::error_code accept_ec_;

private:
	/// should not use directly
	stream_type type_;
	typedef void (*fiber_proc)(FiberConContext *, const iocp::error_code &ec);
	fiber_proc proc_;
	static void fiber_acceptor_proc_wrapper(void *c);
	static void fiber_connect_proc_wrapper(void *c);

	friend class FiberWebServer;
	FiberWebServer *server_;

	iocp::fiber_socket *tcp_stream_;
	iocp::fiber_ssl_socket *ssl_stream_;

	long shutdown_flag_;
	long close_flag_;
	bool check_closed();

	/// used by connecting
	std::string ip_;
	unsigned short port_;
};

class FiberWebServer
{
public:
	FiberWebServer();
	~FiberWebServer();

	int AppendThread(int num = 1);
	int SetCertificate(const char *pszCertChain, const char *pszPrivateKey);
	int SetCipherList(const char *pszCipher);
#ifndef OPENSSL_NO_TLSEXT
	int AddVirtualHost(const char *servername,
		const char *pszCertChain, const char *pszPrivateKey, const char *pszCipher);
#endif

	int startHTTP(const char *pszIP, unsigned short port, int backlog, FiberConContext::fiber_proc proc);
	int startHTTPS(const char *pszIP, unsigned short port, int backlog, FiberConContext::fiber_proc proc);
	int connect(const char *pszIP, unsigned short port, bool use_ssl, void *ctx, FiberConContext::fiber_proc proc);
	int run();

private:
	friend class FiberConContext;

	static void http_thread_proc(void *arg)
	{
		fiber::scheduler::convert_thread(0);
		iocp::service *service = static_cast<iocp::service *>(arg);
		iocp::error_code ec;
		size_t result = service->run(ec);
		if (!ec) {
			std::cout << result << " done" << std::endl;;
		}
		else {
			std::cerr << iocp::error::describe_error(ec) << std::endl;
		}
	}

	static void http_acceptor_proc(void *s)
	{
		FiberWebServer *server = static_cast<FiberWebServer *>(s);
		for (;;) {
			iocp::error_code ec;
			FiberConContext *ctx = new FiberConContext(*server, FiberConContext::http);
			ctx->proc_ = server->http_proc_;
			ec = ctx->accept_ec_ = server->http_acceptor_->accept(*ctx->tcp_stream_);
			ctx->tcp_stream_->invoke(&FiberConContext::fiber_acceptor_proc_wrapper, ctx);
			if (ec)
				break;
		}
	}

	static void https_acceptor_proc(void *s)
	{
		FiberWebServer *server = static_cast<FiberWebServer *>(s);
		for (;;) {
			iocp::error_code ec;
			FiberConContext *ctx = new FiberConContext(*server, FiberConContext::https);
			ctx->proc_ = server->https_proc_;
			ec = ctx->accept_ec_ = server->https_acceptor_->accept(*ctx->ssl_stream_);
			ctx->ssl_stream_->invoke(&FiberConContext::fiber_acceptor_proc_wrapper, ctx);
			if (ec)
				break;
		}
	}

	static void on_http_need_accept(void *binded, const iocp::error_code &ec)
	{
		FiberWebServer *server = static_cast<FiberWebServer *>(binded);
		server->http_acceptor_->invoke(&FiberWebServer::http_acceptor_proc, server);
	}

	static void on_https_need_accept(void *binded, const iocp::error_code &ec)
	{
		FiberWebServer *server = static_cast<FiberWebServer *>(binded);
		server->https_acceptor_->invoke(&FiberWebServer::https_acceptor_proc, server);
	}

	static void on_need_connect(void *binded, const iocp::error_code &ec)
	{
		FiberConContext *cctx = static_cast<FiberConContext *>(binded);
		if (cctx->tcp_stream_)
			cctx->tcp_stream_->invoke(&FiberConContext::fiber_connect_proc_wrapper, cctx);
		else
			cctx->ssl_stream_->invoke(&FiberConContext::fiber_connect_proc_wrapper, cctx);
	}

	iocp::service service_;
	ssl::context *server_context_;
	ssl::context *client_context_;

	std::vector<iocp::thread *> threads_;

	iocp::fiber_acceptor *http_acceptor_;
	iocp::fiber_acceptor *https_acceptor_;

	FiberConContext::fiber_proc http_proc_;
	FiberConContext::fiber_proc https_proc_;
	FiberConContext::fiber_proc connect_proc_;
};

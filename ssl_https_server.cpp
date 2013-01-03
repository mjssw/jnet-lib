#include "net_service.h"
#include "ssl_socket.h"
#include <stdio.h>

static ssl::context *server_ctx;

static char response[] = "HTTP/1.1 200 OK\r\nDate: Thu, 08 Mar 2012 08:23:34 GMT\r\nServer: MyIOCP/2.2.21 (Win32) mod_ssl/2.2.21 OpenSSL/0.9.8r\r\nLast-Modified: Sat, 20 Nov 2004 06:16:24 GMT\r\nETag: \"3000000002dcf-2c-3e94a9010be00\"\r\nAccept-Ranges: bytes\r\nContent-Length: 44\r\nConnection: close\r\nContent-Type: text/html\r\n\r\n<html><body><h1>It works!</h1></body></html>";

class ssl_thread_t
{
public:
	ssl_thread_t(iocp::service &svr): socket(*server_ctx, svr) {}

	iocp::ssl_socket socket;
	char recv_buf[1024];
};

static void on_shutdown(void *binded, const iocp::error_code &ec)
{
	ssl_thread_t *socket = static_cast<ssl_thread_t *>(binded);
	if (ec)
		puts(ec.to_string().c_str());
	delete socket;
}

static void on_sent(void *binded, const iocp::error_code &ec, size_t len)
{
	ssl_thread_t *socket = static_cast<ssl_thread_t *>(binded);
	if (ec) {
		puts(ec.to_string().c_str());
		delete socket;
	}
	else {
		socket->socket.asyn_shutdown(on_shutdown, socket);
	}
}

static void on_received(void *binded, const iocp::error_code &ec, size_t len, char *buf)
{
	ssl_thread_t *socket = static_cast<ssl_thread_t *>(binded);
	if (ec) {
		puts(ec.to_string().c_str());
		delete socket;
	}
	else {
		socket->socket.asyn_send(response, sizeof(response)-1, on_sent, socket);
	}
}

static void on_handshaked(void *binded, const iocp::error_code &ec)
{
	ssl_thread_t *socket = static_cast<ssl_thread_t *>(binded);
	if (ec) {
		puts(ec.to_string().c_str());
		delete socket;
	}
	else {
		socket->socket.asyn_recv_some(socket->recv_buf, 1024, on_received, socket);
	}
}

static void on_accepted(void *binded, const iocp::error_code &ec, void *data)
{
	iocp::acceptor *acceptor = static_cast<iocp::acceptor *>(binded);
	ssl_thread_t *accepted = static_cast<ssl_thread_t *>(data);
	if (ec) {
		puts(ec.to_string().c_str());
		delete accepted;
	}
	else {
		accepted->socket.asyn_handshake(ssl::server, on_handshaked, accepted);

		ssl_thread_t *socket = new ssl_thread_t(acceptor->service());
		acceptor->asyn_accept(socket->socket, socket, on_accepted, acceptor);
	}
}

static void on_begin_accepting(void *binded, const iocp::error_code &ec)
{
	iocp::acceptor *acceptor = static_cast<iocp::acceptor *>(binded);
	ssl_thread_t *socket = new ssl_thread_t(acceptor->service());
	acceptor->asyn_accept(socket->socket, socket, on_accepted, acceptor);
}

void ssl_https_server_example()
{
	ssl::init();
	iocp::error_code ec;
	iocp::service svr;
	iocp::acceptor acceptor(svr);

	server_ctx = new ssl::context(ssl::sslv23_server);
	server_ctx->use_certificate_file("chain.pem", ssl::pem);
	server_ctx->use_rsa_private_key_file("privkey.pem", ssl::pem);

	ec = acceptor.bind_and_listen(iocp::address::any, 60443, 100);
	if (ec)
		puts(ec.to_string().c_str());

	svr.post(on_begin_accepting, &acceptor);

	puts("https server start");
	svr.run(ec);
	puts("https server end");
}


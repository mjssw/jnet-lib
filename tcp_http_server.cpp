#include "net_service.h"
#include "net_socket.h"
#include <stdio.h>

static char response[] = "HTTP/1.1 200 OK\r\nDate: Thu, 08 Mar 2012 08:23:34 GMT\r\nServer: MyIOCP/2.2.21 (Win32) mod_ssl/2.2.21 OpenSSL/0.9.8r\r\nLast-Modified: Sat, 20 Nov 2004 06:16:24 GMT\r\nETag: \"3000000002dcf-2c-3e94a9010be00\"\r\nAccept-Ranges: bytes\r\nContent-Length: 44\r\nConnection: close\r\nContent-Type: text/html\r\n\r\n<html><body><h1>It works!</h1></body></html>";

class thread_t
{
public:
	thread_t(iocp::service &svr): socket(svr) {}

	iocp::socket socket;
	char recv_buf[1024];
};

static void on_sent(void *binded, const iocp::error_code &ec, size_t len)
{
	thread_t *socket = static_cast<thread_t *>(binded);
	if (ec)
		puts(ec.to_string().c_str());
	delete socket;
}

static void on_received(void *binded, const iocp::error_code &ec, size_t len, char *buf)
{
	thread_t *socket = static_cast<thread_t *>(binded);
	if (ec) {
		puts(ec.to_string().c_str());
		delete socket;
	}
	else {
		socket->socket.asyn_send(response, sizeof(response)-1, on_sent, socket);
	}
}

static void on_accepted(void *binded, const iocp::error_code &ec, void *data)
{
	iocp::acceptor *acceptor = static_cast<iocp::acceptor *>(binded);
	thread_t *accepted = static_cast<thread_t *>(data);
	if (ec) {
		puts(ec.to_string().c_str());
		delete accepted;
	}
	else {
		accepted->socket.asyn_recv_some(accepted->recv_buf, 1024, on_received, accepted);

		thread_t *socket = new thread_t(acceptor->service());
		acceptor->asyn_accept(socket->socket, socket, on_accepted, acceptor);
	}
}

static void on_begin_accepting(void *binded, const iocp::error_code &ec)
{
	iocp::acceptor *acceptor = static_cast<iocp::acceptor *>(binded);
	thread_t *socket = new thread_t(acceptor->service());
	acceptor->asyn_accept(socket->socket, socket, on_accepted, acceptor);
}

void tcp_http_server_example()
{
	iocp::error_code ec;
	iocp::service svr;
	iocp::acceptor acceptor(svr);

	ec = acceptor.bind_and_listen(iocp::address::any, 60080, 100);
	if (ec)
		puts(ec.to_string().c_str());

	svr.post(on_begin_accepting, &acceptor);

	puts("http server start");
	svr.run(ec);
	puts("http server end");
}


#include "iocp_service.h"
#include "http_socket.h"
#include "fiber_socket.h"
#include "fiber_http.h"
#include <iostream>
#include <stdio.h>


static void fiber_http_proc(void *arg)
{
	iocp::fiber_http_socket *sock = static_cast<iocp::fiber_http_socket *>(arg);

	iocp::error_code ec;
	iocp::http_socket::result result;
	http::request request;
	char buffer[1024];
	ec = sock->receive_request(result, request, buffer, 1024);
	ec = sock->receive_request(result, request, buffer, 1024);

}

static void acceptot_proc(void *arg)
{
	iocp::fiber_acceptor *acceptor = static_cast<iocp::fiber_acceptor *>(arg);
	iocp::error_code ec;
	ec = acceptor->bind_and_listen(iocp::address::any, 60080, 500);
	if (ec)
		std::cerr << "bind & listen: " << ec.to_string() << std::endl;

	iocp::socket *ns;
	for (;;) {
		//iocp::fiber_socket socket(acceptor->service());
		ns = new iocp::socket(acceptor->service());
		ec = acceptor->accept(*ns);
		if (ec) {
			std::cerr << "accept: " << ec.to_string() << std::endl;
			break;
		}
		else {
			iocp::fiber_http_socket *http_sock = new iocp::fiber_http_socket(ns->service(), *ns);
			http_sock->invoke(fiber_http_proc, http_sock);
		}
	}
}

void fiber_http_test()
{
	fiber::scheduler::convert_thread(0);
	iocp::service service;

	iocp::fiber_acceptor http_acceptor(service);
	http_acceptor.invoke(acceptot_proc, &http_acceptor);

	iocp::error_code ec;
	service.run(ec);

	for (;;)
		Sleep(500);
}

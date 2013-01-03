#include "iocp_service.h"
#include "fiber_socket.h"
#include "iocp_thread.h"

#include <iostream>

static char request_string[] = "GET / HTTP/1.1\r\n\r\n";
static char response_string[] = "HTTP/1.1 200 OK\r\nDate: Thu, 08 Mar 2012 08:23:34 GMT\r\nServer: MyIOCP/2.2.21 (Win32) mod_ssl/2.2.21 OpenSSL/0.9.8r\r\nLast-Modified: Sat, 20 Nov 2004 06:16:24 GMT\r\nETag: \"3000000002dcf-2c-3e94a9010be00\"\r\nAccept-Ranges: bytes\r\nContent-Length: 44\r\nConnection: close\r\nContent-Type: text/html\r\n\r\n<html><body><h1>It works!</h1></body></html>";

void request_proc(void *arg)
{
	iocp::fiber_socket *socket = static_cast<iocp::fiber_socket *>(arg);
	iocp::error_code ec;
	ec = socket->connect("127.0.0.1", 80);
	if (ec)
		std::cerr << "connect: " << ec.to_string() << std::endl;
	/// send request
	size_t bytes_transferred;
	ec = socket->send(request_string, sizeof(request_string)-1, bytes_transferred);
	if (ec)
		std::cerr << "send: " << ec.to_string() << std::endl;
	else
		std::cout << "sent " << bytes_transferred << " bytes" << std::endl;
	/// receive response
	char *recv_buf = new char[1024];
	ec = socket->recv_some(recv_buf, 1024, bytes_transferred);
	if (ec)
		std::cerr << "recv: " << ec.to_string() << std::endl;
	else
		std::cout << "received " << bytes_transferred << " bytes" << std::endl;
	delete[] recv_buf;
	/// close
	socket->close();
}

void responser_proc(void *arg)
{
	iocp::fiber_socket *socket = static_cast<iocp::fiber_socket *>(arg);
	iocp::error_code ec;
	size_t bytes_transferred;

	/// receive request
	char *recv_buf = new char[1024];
	ec = socket->recv_some(recv_buf, 1024, bytes_transferred);
	if (ec)
		std::cerr << "recv: " << ec.to_string() << std::endl;
// 	else
// 		std::cout << "received " << bytes_transferred << " bytes" << std::endl;
	delete[] recv_buf;

	/// send response
	ec = socket->send(response_string, sizeof(response_string)-1, bytes_transferred);
	if (ec)
		std::cerr << "send: " << ec.to_string() << std::endl;
// 	else
// 		std::cout << "sent " << bytes_transferred << " bytes" << std::endl;

	/// close
	socket->close();
	delete socket;
}

void acceptot_proc(void *arg)
{
	iocp::fiber_acceptor *acceptor = static_cast<iocp::fiber_acceptor *>(arg);
	iocp::error_code ec;
	ec = acceptor->bind_and_listen(iocp::address::any, 60080, 500);
	if (ec)
		std::cerr << "bind & listen: " << ec.to_string() << std::endl;

	iocp::fiber_socket *ns;
	for (;;) {
		//iocp::fiber_socket socket(acceptor->service());
		ns = new iocp::fiber_socket(acceptor->service());
		ec = acceptor->accept(*ns);
		if (ec) {
			std::cerr << "accept: " << ec.to_string() << std::endl;
			break;
		}
		else {
			ns->invoke(responser_proc, ns);
		}
	}
}

void ssl_proc(void *arg)
{
	iocp::fiber_ssl_socket*socket = static_cast<iocp::fiber_ssl_socket *>(arg);
	iocp::error_code ec;
	size_t bytes_transferred;

	/// handshake
	ec = socket->handshake(ssl::server);
	if (ec)
		std::cerr << "handshake: " << ec.to_string() << std::endl;

	/// receive request
	char *recv_buf = new char[1024];
	ec = socket->recv_some(recv_buf, 1024, bytes_transferred);
	if (ec)
		std::cerr << "recv: " << ec.to_string() << std::endl;
	// 	else
	// 		std::cout << "received " << bytes_transferred << " bytes" << std::endl;
	delete[] recv_buf;

	/// send response
	ec = socket->send(response_string, sizeof(response_string)-1, bytes_transferred);
	if (ec)
		std::cerr << "send: " << ec.to_string() << std::endl;
	// 	else
	// 		std::cout << "sent " << bytes_transferred << " bytes" << std::endl;

	/// shutdown
	ec = socket->shutdown();
	if (ec)
		std::cerr << "shutdown: " << ec.to_string() << std::endl;

	/// close
	socket->close();
	delete socket;
}

void ssl_acceptot_proc(void *arg)
{
	iocp::fiber_acceptor *acceptor = static_cast<iocp::fiber_acceptor *>(arg);
	iocp::error_code ec;
	ec = acceptor->bind_and_listen(iocp::address::any, 60443, 500);
	if (ec)
		std::cerr << "bind & listen: " << ec.to_string() << std::endl;

	ssl::init();
	ssl::context server_ctx(ssl::sslv23_server), client_ctx(ssl::sslv23_client);
	server_ctx.use_certificate_file("chain.pem", ssl::pem);
	server_ctx.use_rsa_private_key_file("privkey.pem", ssl::pem);

	iocp::fiber_ssl_socket *ns;
	for (;;) {
		ns = new iocp::fiber_ssl_socket(server_ctx, acceptor->service());
		ec = acceptor->accept(*ns);
		if (ec) {
			std::cerr << "accept: " << ec.to_string() << std::endl;
			break;
		}
		else {
			ns->invoke(ssl_proc, ns);
		}
	}
}

static void fiber_thread_proc(void *arg)
{
	fiber::scheduler *data = fiber::scheduler::convert_thread(0);
	printf("thread started\n");

	iocp::service *service = static_cast<iocp::service *>(arg);
	iocp::error_code ec;
	size_t result = service->run(ec);
	if (!ec) {
		std::cout << result << " done" << std::endl;;
	}
	else {
		std::cout << iocp::error::describe_error(ec) << std::endl;
	}
}


void fiber_example()
{
	iocp::service svr;
	fiber::scheduler *data = fiber::scheduler::convert_thread(0);
	//iocp::fiber_socket socket(svr);
	//socket.invoke(request_proc, &socket);
	iocp::fiber_acceptor http_acceptor(svr);
	http_acceptor.invoke(acceptot_proc, &http_acceptor);
	iocp::fiber_acceptor https_acceptor(svr);
	https_acceptor.invoke(ssl_acceptot_proc, &https_acceptor);

	iocp::thread t0(fiber_thread_proc, &svr);

	printf("started\n");
	iocp::error_code ec;
	size_t result;
	result = svr.run(ec);
	t0.join();

	std::cout << "finished " << result << " operations" << std::endl;
	std::cout << ec.to_string() << std::endl;
}

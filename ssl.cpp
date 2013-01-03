#include "net_service.h"
#include "ssl_socket.h"
#include <iostream>
#include <stdio.h>

class ssl_client
{
public:
	ssl_client(iocp::service &service, ssl::context &ctx): socket_(ctx, service), addr(0U) {}

	iocp::ssl_socket &socket()  { return socket_; }

	static void connect(void *tp, const iocp::error_code &ec)
	{
		ssl_client *client = static_cast<ssl_client *>(tp);

		client->socket_.asyn_connect(client->addr, client->port, &ssl_client::on_connect, client);
	}

	static void on_connect(void *tp, const iocp::error_code &ec)
	{
		ssl_client *client = static_cast<ssl_client *>(tp);
		puts("connected");
		if (ec) {
			std::cout << ec.to_string() << std::endl;
		}
		else {
			client->socket_.asyn_handshake(ssl::client, &ssl_client::on_handshake, client);
		}
	}

	static void on_handshake(void *tp, const iocp::error_code &ec)
	{
		ssl_client *client = static_cast<ssl_client *>(tp);
		puts("client hanshaked");
		if (ec) {
			std::cout << ec.to_string() << std::endl;
		}
		else {
			client->socket_.asyn_send("hello", 5, &ssl_client::on_send, client);
		}
	}

	static void on_send(void *tp, const iocp::error_code &ec, size_t bytes_transferred)
	{
		ssl_client *client = static_cast<ssl_client *>(tp);
		if (ec) {
			std::cout << ec.to_string() << std::endl;
		}
		else {
			//client->socket_.shutdown(); // TODO:
		}
	}

	iocp::address addr;
	unsigned short port;

private:
	iocp::ssl_socket socket_;
};

class ssl_server
{
public:
	ssl_server(iocp::service &service, ssl::context &ctx): socket_(ctx, service) {}

	iocp::ssl_socket &socket()  { return socket_; }

	void handshake()
	{
		socket_.asyn_handshake(ssl::server, &ssl_server::on_handshake, this);
	}

	static void on_handshake(void *tp, const iocp::error_code &ec)
	{
		ssl_server *server = static_cast<ssl_server *>(tp);
		puts("server handshaked");
		if (ec) {
			std::cout << ec.to_string() << std::endl;
		}
		else {
			server->socket_.asyn_recv(server->buf_, 5, &ssl_server::on_recv, server);
		}
	}

	static void on_recv(void *tp, const iocp::error_code &ec, size_t bytes_transferred, char *buf)
	{
		ssl_server *server = static_cast<ssl_server *>(tp);
		if (ec) {
			std::cout << ec.to_string() << std::endl;
		}
		else {
			printf("%.*s\n", (int)bytes_transferred, buf);
			//server->socket_.shutdown(); // TODO: shutdown
		}
	}

private:
	iocp::ssl_socket socket_;
	char buf_[5];
};

void on_ssl_accept(void *acceptor_point, const iocp::error_code &ec, void *socket_point)
{
	ssl_server *server = static_cast<ssl_server *>(socket_point);
	puts("accepted");
	if (ec) {
		std::cout << ec.to_string() << std::endl;
	}
	else {
		server->handshake();
	}
}

void ssl_example()
{
	ssl::init();	 // don't forget
	ssl::context server_ctx(ssl::sslv23_server), client_ctx(ssl::sslv23_client);
	server_ctx.use_certificate_file("chain.pem", ssl::pem);
	server_ctx.use_rsa_private_key_file("privkey.pem", ssl::pem);
	iocp::service svr;
	iocp::acceptor acceptor(svr);

	// iocp::address(0U) or iocp::address("0.0.0.0") alse work
	iocp::error_code ec = acceptor.bind_and_listen(iocp::address::any, 60000, 10);
	if (ec) {
		std::cout << ec.to_string() << std::endl;
		return;
	}

	ssl_server server(svr, server_ctx);
	acceptor.asyn_accept(server.socket(), &server, on_ssl_accept, &acceptor);

	ssl_client client(svr, client_ctx);
	client.addr = iocp::address::loopback;
	client.port = 60000;
	client.socket().service().post(&ssl_client::connect, &client);

	size_t result = svr.run(ec);

	std::cout << "finished " << result << " operations" << std::endl;
	std::cout << ec.to_string() << std::endl;
}

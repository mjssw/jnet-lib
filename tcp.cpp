#include "net_service.h"
#include "net_socket.h"
#include <iostream>
#include <stdio.h>

class tcp_client
{
public:
	tcp_client(iocp::service &service): socket_(service), addr(0U) {}

	iocp::socket &socket()  { return socket_; }

	static void connect(void *tp, const iocp::error_code &ec)
	{
		tcp_client *client = static_cast<tcp_client *>(tp);

		client->socket_.asyn_connect(client->addr, client->port, &tcp_client::on_connect, client);
	}

	static void on_connect(void *tp, const iocp::error_code &ec)
	{
		tcp_client *client = static_cast<tcp_client *>(tp);
		puts("connected");
		if (ec) {
			std::cout << ec.to_string() << std::endl;
		}
		else {
			client->socket_.asyn_send("hello", 5, &tcp_client::on_send, client);
		}
	}

	static void on_send(void *tp, const iocp::error_code &ec, size_t bytes_transferred)
	{
		tcp_client *client = static_cast<tcp_client *>(tp);
		puts("sent");
		if (ec) {
			std::cout << ec.to_string() << std::endl;
		}
		else {
			client->socket_.close();
		}
	}

	iocp::address addr;
	unsigned short port;

private:
	iocp::socket socket_;
};

class tcp_server
{
public:
	tcp_server(iocp::service &service): socket_(service) {}

	iocp::socket &socket()  { return socket_; }

	void receive()
	{
		socket_.asyn_recv(buf_, 5, &tcp_server::on_recv, this);
	}

	static void on_recv(void *tp, const iocp::error_code &ec, size_t bytes_transferred, char *buf)
	{
		tcp_server *server = static_cast<tcp_server *>(tp);
		if (ec) {
			std::cout << ec.to_string() << std::endl;
		}
		else {
			printf("%.*s\n", (int)bytes_transferred, buf);
			server->socket_.close();
		}
	}

private:
	iocp::socket socket_;
	char buf_[5];
};

void on_tcp_accept(void *acceptor_point, const iocp::error_code &ec, void *socket_point)
{
	tcp_server *server = static_cast<tcp_server *>(socket_point);
	puts("accepted");
	if (ec) {
		std::cout << ec.to_string() << std::endl;
	}
	else {
		server->receive();
	}
}


void tcp_example()
{
	iocp::service svr;
	iocp::acceptor acceptor(svr);

	// iocp::address(0UL) or iocp::address("0.0.0.0") alse work
	iocp::error_code ec = acceptor.bind_and_listen(iocp::address::any, 60000, 10);
	if (ec) {
		std::cout << ec.to_string() << std::endl;
		return;
	}

	tcp_server server(svr);
	acceptor.asyn_accept(server.socket(), &server, on_tcp_accept, &acceptor);

	tcp_client client(svr);
	client.addr = iocp::address::loopback;
	client.port = 60000;
	client.socket().service().post(&tcp_client::connect, &client);

	size_t result = svr.run(ec);

	std::cout << "finished " << result << " operations" << std::endl;
	std::cout << ec.to_string() << std::endl;
}

#include "net_service.h"
#include "ssl_socket.h"
//#include "http_socket.h"
#include "iocp_resolver.h"
#include "iocp_thread.h"
//#include <vld.h>
#include <iostream>

#ifdef _MSC_VER
#pragma comment(lib, "ssleay32.lib")
#pragma comment(lib, "libeay32.lib")
#endif

//#include <vld.h>

/// test 0
void test_run_exit()
{
	std::cout << "wait iocp service exiting...";
	iocp::service service;
	iocp::error_code ec;
	size_t result = service.run(ec);
	if (!result && !ec)
		std::cout << "done" << std::endl;
	else
		std::cout << iocp::error::describe_error(ec) << std::endl;
}
/// test 0 end

/// test 1
static int test1_flag = 0;
void on_post(void *binded, const iocp::error_code &ec)
{
	if (ec) {
		std::cout << iocp::error::describe_error(ec) << std::endl;
	}
	else {
		if (binded == (void *)1)
			test1_flag = 1;
		else 
			std::cout << binded << " is not what we want" << std::endl;
	}
}

void test_run_one_exit()
{
	std::cout << "wait iocp service exiting...";
	iocp::service service;

	service.post(on_post, (void *)1);

	iocp::error_code ec;
	size_t result = service.run(ec);
	if (result == 1 && !ec && test1_flag)
		std::cout << "done" << std::endl;
	else if (test1_flag == 0)
		std::cout << "flag not set" << std::endl;
	else
		std::cout << iocp::error::describe_error(ec) << std::endl;
}
/// test 1 end


/// test 2
class  test2_server
{
public:
	test2_server(iocp::service &service): socket_(service) {}

	iocp::socket &socket() { return socket_; }

private:
	iocp::socket socket_;
};

static test2_server *s_test2_server;

class test2_acceptor
{
public:
	test2_acceptor(iocp::service &service): acceptor_(service)
	{
		service.post(&test2_acceptor::on_post, this);
	}

	static void on_post(void *binded, const iocp::error_code &ec)
	{
		if (ec) {
			std::cout << iocp::error::describe_error(ec) << std::endl;
		}
		else {
			test2_acceptor *acceptor = static_cast<test2_acceptor *>(binded);
			iocp::error_code bl_ec = acceptor->acceptor_.bind_and_listen(iocp::address::any, 60000, 10);
			if (bl_ec) {
				std::cout << iocp::error::describe_error(bl_ec);
			}
			else
			{
				s_test2_server= new test2_server(acceptor->acceptor_.service());
				acceptor->acceptor_.asyn_accept(s_test2_server->socket(), s_test2_server, &test2_acceptor::on_accept, acceptor);
			}
		}
	}

	static void on_accept(void *binded, const iocp::error_code &ec, void *data)
	{
		test2_server *server = static_cast<test2_server *>(data);
		if (ec) {
			std::cout << iocp::error::describe_error(ec) << std::endl;
		}
	}

private:
	iocp::acceptor acceptor_;
};

class test2_client
{
public:
	test2_client(iocp::service &service): socket_(service)
	{ socket_.asyn_connect(iocp::address("127.0.0.1"), 60000, &test2_client::on_connect, this); }

	iocp::socket &socket() { return socket_; }

	static void on_connect(void *binded, const iocp::error_code &ec)
	{
		if (ec) {
			std::cout << iocp::error::describe_error(ec) << std::endl;;
		}
	}

private:
	iocp::socket socket_;
};

void test_acceptor()
{
	iocp::service service;
	test2_acceptor *acceptor = new test2_acceptor(service);
	test2_client *connector = new test2_client(service);
	std::cout << "listening at port 60000..." << std::endl;
	iocp::error_code ec;
	size_t result = service.run(ec);
	if (result == 3 && !ec) {
		std::cout << "done" << std::endl;;
	}
	else {
		std::cout << iocp::error::describe_error(ec) << std::endl;
	}
	delete acceptor;
	delete connector;
	delete s_test2_server;
}
/// test 2 end

/// test 3
void test3_resolver_callback(void *binded, const iocp::error_code &ec, const iocp::address &addr)
{
	if (ec)
		iocp::error::describe_error(ec);
	else
		std::cout << (char *)binded << "-" << ::inet_ntoa(addr) << std::endl;
}

void test_resolver()
{
	iocp::service service;
	iocp::resolver resolver(service);
	std::cout << "resolving..." << std::endl;
	resolver.asyn_resolve("localhost", test3_resolver_callback, (void *)"localhost");
	resolver.asyn_resolve("www.baidu.com", test3_resolver_callback, (void *)"www.baidu.com");
	iocp::error_code ec;
	size_t result = service.run(ec);
	if (result == 2 && !ec) {
		std::cout << "done" << std::endl;;
	}
	else {
		std::cout << iocp::error::describe_error(ec) << std::endl;
	}
}
/// test 3 end

/// test 4

long test4_recv_count ,test4_send_count;

class test4_server
{
public:
	test4_server(iocp::service &service): socket_(service) {}

	iocp::socket &socket() { return socket_; }
	void recv()
	{
		socket_.asyn_recv(buf_, 5, &test4_server::on_recv, this);
	}

	static void on_recv(void *binded, const iocp::error_code &ec, size_t bytes_transferred, char *buf)
	{
		if (ec) {
			std::cout << "recv: " << iocp::error::describe_error(ec) << std::endl;
			return;
		}
		if (bytes_transferred < 5 || memcmp(buf, "hello", 5) != 0)
			printf("%.*s\n", (int)bytes_transferred, buf);
		test4_server *server = static_cast<test4_server *>(binded);
		if (--test4_recv_count)
			server->recv();
		else
			puts("recv done");
		//else
			//delete server;
	}

private:
	iocp::socket socket_;
	char buf_[5];
};

static test4_server *s_server;

class test4_acceptor
{
public:
	test4_acceptor(iocp::service &service): acceptor_(service)
	{
		iocp::error_code ec = acceptor_.bind_and_listen(iocp::address::any, 60000, 10);
		if (ec) {
			std::cout << iocp::error::describe_error(ec);
		}
		else
		{
			s_server = new test4_server(acceptor_.service());
			acceptor_.asyn_accept(s_server->socket(), s_server, &test4_acceptor::on_accept, this);
		}
	}

	static void on_accept(void *binded, const iocp::error_code &ec, void *data)
	{
		if (ec) {
			std::cout << iocp::error::describe_error(ec) << std::endl;
		}
		else {
			test4_acceptor *acceptor = static_cast<test4_acceptor *>(binded);
			test4_server *server = static_cast<test4_server *>(data);
			server->recv();
			acceptor->acceptor_.close();
		}
	}

private:
	iocp::acceptor acceptor_;
};

class test4_client
{
public:
	test4_client(iocp::service &service): socket_(service)
	{
		memcpy(buf_, "hello", 5);
		socket_.asyn_connect(iocp::address("127.0.0.1"), 60000, &test4_client::on_connect, this);
	}

	iocp::socket &socket() { return socket_; }

	static void on_connect(void *binded, const iocp::error_code &ec)
	{
		test4_client *client = static_cast<test4_client *>(binded);
		if (ec) {
			std::cout << iocp::error::describe_error(ec);
		}
		else {
			client->socket_.asyn_send(client->buf_, 5, on_send, client);
		}
	}

	static void on_send(void *binded, const iocp::error_code &ec, size_t bytes_transferred)
	{
		if (ec) {
			std::cout << iocp::error::describe_error(ec);
			return;
		}
		test4_client *client = static_cast<test4_client *>(binded);
		if (--test4_send_count)
			client->socket_.asyn_send(client->buf_, 5, on_send, client);
		else {
			puts("send done");
			client->socket_.close();
		}
	}

private:
	iocp::socket socket_;
	char buf_[5];
};

void test4_thread_proc(void *arg)
{
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

void test_multi_thread()
{
	test4_recv_count = test4_send_count = 10;
	iocp::service service;
	test4_acceptor acceptor(service);
	test4_client connector(service);
	std::cout << "two threads running..." << std::endl;
	iocp::thread t(test4_thread_proc, &service);
	iocp::error_code ec;
	size_t result = service.run(ec);
	t.join();
	if (!ec) {
		std::cout << result << " done" << std::endl;;
	}
	else {
		std::cout << iocp::error::describe_error(ec) << std::endl;
	}

	connector.socket().close();
	delete s_server;
}
/// test 4 end

/// test 5
void test5_on_client_handshake(void *binded, const iocp::error_code &ec)
{
	if (ec) {
		std::cout << ec.to_string() << std::endl;
		iocp::ssl_socket *socket = static_cast<iocp::ssl_socket *>(binded);
		socket->close();
	}
	else
		std::cout << "client handshake successful" << std::endl;
}

void test5_on_server_handshake(void *binded, const iocp::error_code &ec)
{
	if (ec) {
		std::cout << ec.to_string() << std::endl;
		iocp::ssl_socket *socket = static_cast<iocp::ssl_socket *>(binded);
		socket->close();
	}
	else
		std::cout << "server handshake successful" << std::endl;
}

void test5_on_connect(void *binded, const iocp::error_code &ec)
{
	iocp::ssl_socket *socket = static_cast<iocp::ssl_socket *>(binded);
	socket->asyn_handshake(ssl::client, test5_on_client_handshake, socket);
}

void test5_on_accept(void *binded, const iocp::error_code &ec, void *data)
{
	iocp::ssl_socket *socket = static_cast<iocp::ssl_socket *>(data);
	socket->asyn_handshake(ssl::server, test5_on_server_handshake, socket);
}

void test_ssl_single()
{
	iocp::error_code ec;
	ssl::init();
	ssl::context svr_ctx(ssl::sslv23_server), cli_ctx(ssl::sslv23_client);
	svr_ctx.use_certificate_file("chain.pem", ssl::pem);
	svr_ctx.use_rsa_private_key_file("privkey.pem", ssl::pem);
	iocp::service service;
	iocp::acceptor acceptor(service);
	iocp::ssl_socket server(svr_ctx, service), client(cli_ctx, service);
	ec = acceptor.bind_and_listen(iocp::address::loopback, 60000, 10);
	if (ec)
		std::cout << ec.to_string() << std::endl;
	acceptor.asyn_accept(server, &server, test5_on_accept, &acceptor);
	client.asyn_connect(iocp::address::loopback, 60000, test5_on_connect, &client);
	size_t result = service.run(ec);
	if (!ec) {
		std::cout << result << " done" << std::endl;;
	}
	else {
		std::cout << iocp::error::describe_error(ec) << std::endl;
	}
}
/// test 5 end

/// test 6
long test6_recv_count ,test6_send_count;
#define test6_times 100
#define test6_buffer_length 32*1024
char test6_recv_buf[test6_buffer_length];
char test6_send_buf[test6_buffer_length];

bool test6_check(const unsigned char *buf)
{
	for (int i = 0; i < test6_buffer_length; ++i)
		if (buf[i] != i % 256)
			return true;
	return false;
}

void test6_on_recv(void *binded, const iocp::error_code &ec, size_t bytes_transferred, char *buf)
{
	iocp::ssl_socket *socket = static_cast<iocp::ssl_socket *>(binded);
	if (ec) {
		std::cout << "recv: " << ec.to_string() << std::endl;
		socket->close();
	}
	else {
		if (bytes_transferred < test6_buffer_length || test6_check((unsigned char *)buf))
			std::cout << "recv something wrong" << std::endl;
		if (--test6_recv_count)
			socket->asyn_recv(test6_recv_buf, test6_buffer_length, test6_on_recv, socket);
	}
}

void test6_on_send(void *binded, const iocp::error_code &ec, size_t bytes_transferred)
{
	iocp::ssl_socket *socket = static_cast<iocp::ssl_socket *>(binded);
	if (ec) {
		std::cout << "send: " << ec.to_string() << std::endl;
		socket->close();
	}
	else {
		if (bytes_transferred < test6_buffer_length)
			std::cout << "send something wrong" << std::endl;
		if (--test6_send_count)
			socket->asyn_send(test6_send_buf, test6_buffer_length, test6_on_send, socket);
	}
}

void test6_on_client_handshake(void *binded, const iocp::error_code &ec)
{
	iocp::ssl_socket *socket = static_cast<iocp::ssl_socket *>(binded);
	if (ec) {
		std::cout << ec.to_string() << std::endl;
		socket->close();
	}
	else {
		std::cout << "client handshake successful" << std::endl;
		socket->asyn_recv(test6_recv_buf, test6_buffer_length, test6_on_recv, socket);
	}
}

void test6_on_server_handshake(void *binded, const iocp::error_code &ec)
{
	iocp::ssl_socket *socket = static_cast<iocp::ssl_socket *>(binded);
	if (ec) {
		std::cout << ec.to_string() << std::endl;
		socket->close();
	}
	else {
		std::cout << "server handshake successful" << std::endl;
		socket->asyn_send(test6_send_buf, test6_buffer_length, test6_on_send, socket);
	}
}

void test6_on_connect(void *binded, const iocp::error_code &ec)
{
	iocp::ssl_socket *socket = static_cast<iocp::ssl_socket *>(binded);
	socket->asyn_handshake(ssl::client, test6_on_client_handshake, socket);
}

void test6_on_accept(void *binded, const iocp::error_code &ec, void *data)
{
	iocp::ssl_socket *socket = static_cast<iocp::ssl_socket *>(data);
	socket->asyn_handshake(ssl::server, test6_on_server_handshake, socket);
}

void test6_thread_proc(void *arg)
{
	iocp::service *service = static_cast<iocp::service *>(arg);
	iocp::error_code ec;
	size_t result = service->run(ec);
	if (!ec) {
		std::cout << result << " done" << std::endl;;
		std::cout << result << " done" << std::endl;;
		std::cout << result << " done(thread)" << std::endl;;
	}
	else {
		std::cout << iocp::error::describe_error(ec) << std::endl;
	}
}

void test_ssl_multi()
{
	for (int i = 0; i < test6_buffer_length; ++i)
		test6_send_buf[i] = i % 256;

	test6_recv_count = test6_send_count = test6_times;
	iocp::error_code ec;
	ssl::init();
	ssl::context svr_ctx(ssl::sslv23_server), cli_ctx(ssl::sslv23_client);
	svr_ctx.use_certificate_file("chain.pem", ssl::pem);
	svr_ctx.use_rsa_private_key_file("privkey.pem", ssl::pem);
	iocp::service service;
	iocp::acceptor acceptor(service);
	iocp::ssl_socket server(svr_ctx, service), client(cli_ctx, service);
	ec = acceptor.bind_and_listen(iocp::address::loopback, 60000, 10);
	if (ec)
		std::cout << ec.to_string() << std::endl;
	acceptor.asyn_accept(server, &server, test6_on_accept, &acceptor);
	client.asyn_connect(iocp::address::loopback, 60000, test6_on_connect, &client);
	std::cout << "two threads running..." << std::endl;
	iocp::thread t(test6_thread_proc, &service);
	size_t result = service.run(ec);
	t.join();
	if (!ec) {
		std::cout << result << " done(main)" << std::endl;;
	}
	else {
		std::cout << iocp::error::describe_error(ec) << std::endl;
	}
}
/// test 6 end

/// test 7

long test7_recv_count, test7_recv_count_client ,test7_send_count, test7_send_count_client;
#define test7_times 100
#define test7_buffer_length 32*1024
char test7_recv_buf[test7_buffer_length], test7_recv_buf_client[test7_buffer_length];
char test7_send_buf[test7_buffer_length];

bool test7_check(const unsigned char *buf)
{
	for (int i = 0; i < test7_buffer_length; ++i)
		if (buf[i] != i % 256)
			return true;
	return false;
}

void test7_on_recv(void *binded, const iocp::error_code &ec, size_t bytes_transferred, char *buf)
{
	iocp::ssl_socket *socket = static_cast<iocp::ssl_socket *>(binded);
	if (ec) {
		std::cout << ec.to_string() << std::endl;
		socket->close();
	}
	else {
		if (bytes_transferred < test7_buffer_length || test7_check((unsigned char *)buf))
			std::cout << "server recv something wrong" << std::endl;
		if (--test7_recv_count)
			socket->asyn_recv(test7_recv_buf, test7_buffer_length, test7_on_recv, socket);
	}
}
void test7_on_recv_client(void *binded, const iocp::error_code &ec, size_t bytes_transferred, char *buf)
{
	iocp::ssl_socket *socket = static_cast<iocp::ssl_socket *>(binded);
	if (ec) {
		std::cout << ec.to_string() << std::endl;
		socket->close();
	}
	else {
		if (bytes_transferred < test7_buffer_length || test7_check((unsigned char *)buf))
			std::cout << "client something wrong" << std::endl;
		if (--test7_recv_count_client)
			socket->asyn_recv(test7_recv_buf_client, test7_buffer_length, test7_on_recv_client, socket);
	}
}

void test7_on_send(void *binded, const iocp::error_code &ec, size_t bytes_transferred)
{
	iocp::ssl_socket *socket = static_cast<iocp::ssl_socket *>(binded);
	if (ec) {
		std::cout << ec.to_string() << std::endl;
		socket->close();
	}
	else {
		if (bytes_transferred < test7_buffer_length)
			std::cout << "something wrong" << std::endl;
		if (--test7_send_count)
			socket->asyn_send(test7_send_buf, test7_buffer_length, test7_on_send, socket);
	}
}

void test7_on_send_client(void *binded, const iocp::error_code &ec, size_t bytes_transferred)
{
	iocp::ssl_socket *socket = static_cast<iocp::ssl_socket *>(binded);
	if (ec) {
		std::cout << ec.to_string() << std::endl;
		socket->close();
	}
	else {
		if (bytes_transferred < test7_buffer_length)
			std::cout << "something wrong" << std::endl;
		if (--test7_send_count_client)
			socket->asyn_send(test7_send_buf, test7_buffer_length, test7_on_send_client, socket);
	}
}

void test7_on_client_handshake(void *binded, const iocp::error_code &ec)
{
	iocp::ssl_socket *socket = static_cast<iocp::ssl_socket *>(binded);
	if (ec) {
		std::cout << ec.to_string() << std::endl;
		socket->close();
	}
	else {
		std::cout << "client handshake successful" << std::endl;
		socket->asyn_send(test7_send_buf, test7_buffer_length, test7_on_send_client, socket);
		socket->asyn_recv(test7_recv_buf_client, test7_buffer_length, test7_on_recv_client, socket);
	}
}

void test7_on_server_handshake(void *binded, const iocp::error_code &ec)
{
	iocp::ssl_socket *socket = static_cast<iocp::ssl_socket *>(binded);
	if (ec) {
		std::cout << ec.to_string() << std::endl;
		socket->close();
	}
	else {
		std::cout << "server handshake successful" << std::endl;
		socket->asyn_send(test7_send_buf, test7_buffer_length, test7_on_send, socket);
		socket->asyn_recv(test7_recv_buf, test7_buffer_length, test7_on_recv, socket);
	}
}

void test7_on_connect(void *binded, const iocp::error_code &ec)
{
	iocp::ssl_socket *socket = static_cast<iocp::ssl_socket *>(binded);
	socket->asyn_handshake(ssl::client, test7_on_client_handshake, socket);
}

void test7_on_accept(void *binded, const iocp::error_code &ec, void *data)
{
	iocp::ssl_socket *socket = static_cast<iocp::ssl_socket *>(data);
	socket->asyn_handshake(ssl::server, test7_on_server_handshake, socket);
}

void test7_thread_proc(void *arg)
{
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

void test_ssl_single_full_duplex()
{
	for (int i = 0; i < test7_buffer_length; ++i)
		test7_send_buf[i] = i % 256;

	test7_recv_count_client = test7_send_count_client = test7_recv_count = test7_send_count = test7_times;
	iocp::error_code ec;
	ssl::init();
	ssl::context svr_ctx(ssl::sslv23_server), cli_ctx(ssl::sslv23_client);
	svr_ctx.use_certificate_file("chain.pem", ssl::pem);
	svr_ctx.use_rsa_private_key_file("privkey.pem", ssl::pem);
	iocp::service service;
	iocp::acceptor acceptor(service);
	iocp::ssl_socket server(svr_ctx, service), client(cli_ctx, service);
	ec = acceptor.bind_and_listen(iocp::address::loopback, 60000, 10);
	if (ec)
		std::cout << ec.to_string() << std::endl;
	acceptor.asyn_accept(server, &server, test7_on_accept, &acceptor);
	client.asyn_connect(iocp::address::loopback, 60000, test7_on_connect, &client);
	//std::cout << "two threads running..." << std::endl;
	//iocp::thread t(test7_thread_proc, &service);
	size_t result = service.run(ec);
	//t.join();
	if (!ec) {
		std::cout << result << " done" << std::endl;;
	}
	else {
		std::cout << iocp::error::describe_error(ec) << std::endl;
	}
}
/// test 7 end

/// test 8

long test8_recv_count, test8_recv_count_client ,test8_send_count, test8_send_count_client;
#define test8_times 100
#define test8_buffer_length 256/*32*1024*/
char test8_recv_buf[test8_buffer_length], test8_recv_buf_client[test8_buffer_length];
char test8_send_buf[test8_buffer_length];

bool test8_check(const unsigned char *buf)
{
	for (int i = 0; i < test8_buffer_length; ++i)
		if (buf[i] != i % 256)
			return true;
	return false;
}

void test8_on_recv(void *binded, const iocp::error_code &ec, size_t bytes_transferred, char *buf)
{
	iocp::ssl_socket *socket = static_cast<iocp::ssl_socket *>(binded);
	if (ec) {
		std::cout << ec.to_string() << std::endl;
		socket->close();
	}
	else {
		if (bytes_transferred < test8_buffer_length || test8_check((unsigned char *)buf))
			std::cout << "something wrong" << std::endl;
// 		else
// 			std::cout << "server recv " << bytes_transferred << " bytes" << std::endl;
		if (--test8_recv_count)
			socket->asyn_recv(test8_recv_buf, test8_buffer_length, test8_on_recv, socket);
	}
}
void test8_on_recv_client(void *binded, const iocp::error_code &ec, size_t bytes_transferred, char *buf)
{
	iocp::ssl_socket *socket = static_cast<iocp::ssl_socket *>(binded);
	if (ec) {
		std::cout << ec.to_string() << std::endl;
		socket->close();
	}
	else {
		if (bytes_transferred < test8_buffer_length || test8_check((unsigned char *)buf))
			std::cout << "something wrong" << std::endl;
// 		else
// 			std::cout << "client recv " << bytes_transferred << " bytes" << std::endl;
		if (--test8_recv_count_client)
			socket->asyn_recv(test8_recv_buf_client, test8_buffer_length, test8_on_recv_client, socket);
	}
}

void test8_on_send(void *binded, const iocp::error_code &ec, size_t bytes_transferred)
{
	iocp::ssl_socket *socket = static_cast<iocp::ssl_socket *>(binded);
	if (ec) {
		std::cout << ec.to_string() << std::endl;
		socket->close();
	}
	else {
		if (bytes_transferred < test8_buffer_length)
			std::cout << "something wrong" << std::endl;
// 		else
// 			std::cout << "server sent " << bytes_transferred << " bytes" << std::endl;
		if (--test8_send_count)
			socket->asyn_send(test8_send_buf, test8_buffer_length, test8_on_send, socket);
	}
}

void test8_on_send_client(void *binded, const iocp::error_code &ec, size_t bytes_transferred)
{
	iocp::ssl_socket *socket = static_cast<iocp::ssl_socket *>(binded);
	if (ec) {
		std::cout << ec.to_string() << std::endl;
		socket->close();
	}
	else {
		if (bytes_transferred < test8_buffer_length)
			std::cout << "something wrong" << std::endl;
// 		else
// 			std::cout << "client sent " << bytes_transferred << " bytes" << std::endl;
		if (--test8_send_count_client)
			socket->asyn_send(test8_send_buf, test8_buffer_length, test8_on_send_client, socket);
	}
}

void test8_on_client_handshake(void *binded, const iocp::error_code &ec)
{
	iocp::ssl_socket *socket = static_cast<iocp::ssl_socket *>(binded);
	if (ec) {
		std::cout << ec.to_string() << std::endl;
		socket->close();
	}
	else {
		std::cout << "client handshake successful" << std::endl;
		socket->asyn_send(test8_send_buf, test8_buffer_length, test8_on_send_client, socket);
		socket->asyn_recv(test8_recv_buf_client, test8_buffer_length, test8_on_recv_client, socket);
	}
}

void test8_on_server_handshake(void *binded, const iocp::error_code &ec)
{
	iocp::ssl_socket *socket = static_cast<iocp::ssl_socket *>(binded);
	if (ec) {
		std::cout << ec.to_string() << std::endl;
		socket->close();
	}
	else {
		std::cout << "server handshake successful" << std::endl;
		socket->asyn_send(test8_send_buf, test8_buffer_length, test8_on_send, socket);
		socket->asyn_recv(test8_recv_buf, test8_buffer_length, test8_on_recv, socket);
	}
}

void test8_on_connect(void *binded, const iocp::error_code &ec)
{
	iocp::ssl_socket *socket = static_cast<iocp::ssl_socket *>(binded);
	socket->asyn_handshake(ssl::client, test8_on_client_handshake, socket);
}

void test8_on_accept(void *binded, const iocp::error_code &ec, void *data)
{
	iocp::ssl_socket *socket = static_cast<iocp::ssl_socket *>(data);
	socket->asyn_handshake(ssl::server, test8_on_server_handshake, socket);
}

void test8_thread_proc(void *arg)
{
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

void test_ssl_multi_full_duplex()
{
	for (int i = 0; i < test8_buffer_length; ++i)
		test8_send_buf[i] = i % 256;

	test8_recv_count_client = test8_send_count_client = test8_recv_count = test8_send_count = test8_times;
	iocp::error_code ec;
	ssl::init();
	ssl::context svr_ctx(ssl::sslv23_server), cli_ctx(ssl::sslv23_client);
	svr_ctx.use_certificate_file("chain.pem", ssl::pem);
	svr_ctx.use_rsa_private_key_file("privkey.pem", ssl::pem);
	iocp::service service;
	iocp::acceptor acceptor(service);
	iocp::ssl_socket server(svr_ctx, service), client(cli_ctx, service);
	ec = acceptor.bind_and_listen(iocp::address::loopback, 60000, 10);
	if (ec)
		std::cout << ec.to_string() << std::endl;
	acceptor.asyn_accept(server, &server, test8_on_accept, &acceptor);
	client.asyn_connect(iocp::address::loopback, 60000, test8_on_connect, &client);
	std::cout << "two threads running..." << std::endl;
	iocp::thread t(test8_thread_proc, &service);
	size_t result = service.run(ec);
	t.join();
	if (!ec) {
		std::cout << result << " done" << std::endl;;
	}
	else {
		std::cout << iocp::error::describe_error(ec) << std::endl;
	}
}
/// test 8 end

extern void tcp_example();
extern void ssl_example();
extern void http_example();
extern void fiber_example();
extern void file_example();
extern void http_server_test();
extern void client_test();
extern void fiber_http_server_test();
extern void fiber_client_test();
extern void fiber_file_test();
extern void tlsext_sni_test();
extern void fiber_http_test();
extern void http_server_test_0();
extern void test_close_socket();


int main(int argc, char *argv[])
{
	/*test_run_exit();
	test_run_one_exit();
	test_acceptor();
	test_resolver();
	test_multi_thread();
	test_ssl_single();
	test_ssl_multi();
	test_ssl_single_full_duplex();
	test_ssl_multi_full_duplex();*/

// 	tcp_example();
// 	ssl_example();
//	http_example();
//	fiber_example();
//	file_example();
	http_server_test();
// 	client_test();
//	fiber_http_server_test();
//	fiber_client_test();
//	fiber_file_test();
//	tlsext_sni_test();
//	fiber_http_test();
//  http_server_test_0();
//  test_close_socket();

	return 0;
}


#include "iocp_service.h"
#include "ssl_socket.h"
#include "http_socket.h"
#include "iocp_resolver.h"
#include "iocp_thread.h"
#include <iostream>
#include <stdio.h>

char response[] = "HTTP/1.1 200 OK\r\nDate: Thu, 08 Mar 2012 08:23:34 GMT\r\nServer: MyIOCP/2.2.21 (Win32) mod_ssl/2.2.21 OpenSSL/0.9.8r\r\nLast-Modified: Sat, 20 Nov 2004 06:16:24 GMT\r\nETag: \"3000000002dcf-2c-3e94a9010be00\"\r\nAccept-Ranges: bytes\r\nContent-Length: 44\r\nConnection: close\r\nContent-Type: text/html\r\n\r\n<html><body><h1>It works!</h1></body></html>";

struct test_variable 
{
	DWORD index;
	DWORD start_time;
	DWORD cost_of_handshake;
	DWORD cost_of_recv;
	DWORD cost_of_send;
	DWORD cost_of_shutdown;
};

DWORD global_index = 0;
DWORD global_start = 0;


#include <deque>
std::deque<DWORD> req_ticks;
iocp::mutex req_ticks_cs;
long num_requests = 0;
long num_succeed_req = 0;
long num_failed_req = 0;

void WriteToFile(const http::request *request);

class http_server
{
public:
	http_server(iocp::service &service)
		: socket_(service), http_(socket_) {}

	iocp::socket &socket() { return socket_; }

	void run_server()
	{
		http_.asyn_receive_request(request_, buffer, 1024, 0, 0,
			true, &http_server::on_request, this);
	}

	static void on_request(void *binded, const iocp::error_code &ec, const iocp::http_socket::result &result)
	{
		http_server *server = static_cast<http_server *>(binded);
		if (ec) {
			//std::cout << ec.to_string() << std::endl;
			printf("http_on_request: %s\n", ec.to_string().c_str());
			InterlockedIncrement(&num_failed_req);
			delete server;
		}
		else {
			if (result.type == iocp::http_socket::req) {
				WriteToFile(result.request);
			}
			else if (result.type == iocp::http_socket::fin)
				server->socket_.asyn_send(response, sizeof(response)-1, &http_server::on_send, server);
		}
	}

	static void on_send(void *binded, const iocp::error_code &ec, size_t bytes_transferred)
	{
		if (ec) {
			//std::cout << ec.to_string() << std::endl;
			printf("http_on_send: %s\n", ec.to_string().c_str());
			InterlockedIncrement(&num_failed_req);
		}
		else {
			InterlockedIncrement(&num_succeed_req);
		}
		http_server *server = static_cast<http_server *>(binded);		
		delete server;
	}

private:
	iocp::socket socket_;
	iocp::http_socket http_;
	http::request request_;
	char buffer[1024];
};

class https_server
{
public:
	https_server(iocp::service &service, ssl::context &ctx)
		: socket_(ctx, service), http_(socket_) {}

	iocp::ssl_socket &socket() { return socket_; }

	void run_server()
	{
		http_.asyn_receive_request(request_, buffer, 1024, 0, 0,
			true, &https_server::on_request, this);
	}

	static void on_request(void *binded, const iocp::error_code &ec, const iocp::http_socket::result &result)
	{
		https_server *server = static_cast<https_server *>(binded);
		if (ec) {
			//std::cout << ec.to_string() << std::endl;
			printf("https_on_request: %s\n", ec.to_string().c_str());
			InterlockedIncrement(&num_failed_req);
			delete server;
		}
		else {
			if (result.type == iocp::http_socket::req) {
				WriteToFile(result.request);
			}
			else if (result.type == iocp::http_socket::fin) {
				server->variable_.cost_of_recv = GetTickCount() - server->variable_.cost_of_recv;
				server->socket_.asyn_send(response, sizeof(response)-1, &https_server::on_send, server);
				server->variable_.cost_of_send = GetTickCount();
			}
		}
	}

	static void on_send(void *binded, const iocp::error_code &ec, size_t bytes_transferred)
	{
		https_server *server = static_cast<https_server *>(binded);
		if (ec) {
			//std::cout << ec.to_string() << std::endl;
			printf("https_on_send: %s\n", ec.to_string().c_str());
			InterlockedIncrement(&num_failed_req);
			delete server;
		}
		else {
			InterlockedIncrement(&num_succeed_req);
			server->variable_.cost_of_send = GetTickCount() - server->variable_.cost_of_send;
			server->socket_.asyn_shutdown(&https_server::on_shutdown, server);
			server->variable_.cost_of_shutdown = GetTickCount();
		}
	}

	static void on_shutdown(void *binded, const iocp::error_code &ec)
	{
		if (ec) {
			//std::cout << ec.to_string() << std::endl;
			printf("https_on_shutdown: %s\n", ec.to_string().c_str());
		}
		https_server *server = static_cast<https_server *>(binded);
		server->variable_.cost_of_shutdown = GetTickCount() - server->variable_.cost_of_shutdown;
		//printf("%d\t%d\t%d\t%d\t%d\t%d\n", server->variable_.index, server->variable_.start_time, server->variable_.cost_of_handshake, server->variable_.cost_of_recv, server->variable_.cost_of_send, server->variable_.cost_of_shutdown);

		delete server;
	}

	test_variable variable_;

private:
	iocp::ssl_socket socket_;
	iocp::http_socket http_;
	http::request request_;
	char buffer[1024];
};

void on_http_accept(void *acceptor_point, const iocp::error_code &ec, void *socket_point)
{
	http_server *server = static_cast<http_server *>(socket_point);
	if (ec) {
		//std::cout << ec.to_string() << std::endl;
		printf("http_on_accept: %s\n", ec.to_string().c_str());
		delete server;
	}
	else {
		InterlockedIncrement(&num_requests);
		req_ticks_cs.lock();
		req_ticks.push_back(GetTickCount());
		req_ticks_cs.unlock();
		server->run_server();

		iocp::acceptor *acceptor = static_cast<iocp::acceptor *>(acceptor_point);
		http_server *nserver = new http_server(acceptor->service());
		acceptor->asyn_accept(nserver->socket(), nserver, on_http_accept, acceptor);
	}

}

void on_https_handshake(void *binded, const iocp::error_code &ec)
{
	https_server *server = static_cast<https_server *>(binded);
	if (ec) {
		//std::cout << ec.to_string() << std::endl;
		printf("https_on_handshake: %s\n", ec.to_string().c_str());
		InterlockedIncrement(&num_failed_req);
		delete server;
	}
	else {
		server->variable_.cost_of_handshake = GetTickCount() - server->variable_.cost_of_handshake;
		server->run_server();
		server->variable_.cost_of_recv = GetTickCount();
	}
}

void on_https_accept(void *acceptor_point, const iocp::error_code &ec, void *socket_point)
{
	https_server *server = static_cast<https_server *>(socket_point);
	if (ec) {
		//std::cout << ec.to_string() << std::endl;
		printf("https_on_accept: %s\n", ec.to_string().c_str());
		delete server;
	}
	else {
		InterlockedIncrement(&num_requests);
		req_ticks_cs.lock();
		req_ticks.push_back(GetTickCount());
		req_ticks_cs.unlock();
		server->variable_.index = global_index++;
		server->socket().asyn_handshake(ssl::server, on_https_handshake, server);
		server->variable_.cost_of_handshake = GetTickCount();

		server->variable_.start_time = server->variable_.cost_of_handshake;
		if (global_start == 0)
			global_start = server->variable_.start_time;
		server->variable_.start_time -= global_start;

		iocp::acceptor *acceptor = static_cast<iocp::acceptor *>(acceptor_point);
		https_server *nserver = new https_server(acceptor->service(), server->socket().context());
		acceptor->asyn_accept(nserver->socket(), nserver, on_https_accept, acceptor);
	}
}

void http_thread_proc(void *arg)
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

void http_monitor_thread(void *arg)
{
	static DWORD last_tick, tick;
	static long last_num_requests;
	size_t deque_size;
	for (;;)
	{
		tick = GetTickCount();
		req_ticks_cs.lock();
		if (!req_ticks.empty() && tick - req_ticks.front() >= 2000 * 60)	// 2 min
			req_ticks.pop_front();
		deque_size = req_ticks.size();
		req_ticks_cs.unlock();
		printf("req: %d(%d, %d) %.2lf req/s %d reqs in last 2 mins,\n",
			num_requests, num_succeed_req, num_failed_req,
			(double)(num_requests - last_num_requests)*1000.0/(tick - last_tick),
			deque_size);
		last_num_requests = num_requests;
		last_tick = tick;
		Sleep(1000);
	}
}

#include <signal.h>
#include <time.h>
static FILE *fp;

void WriteToFile(const http::request *request)
{
	if (fp == NULL)
		fp = fopen("requests.log", "a+");

#define TIME_BUFFER_LENGTH 80
	time_t rawtime;
	struct tm *timeinfo;
	char timebuf[TIME_BUFFER_LENGTH];

	time(&rawtime);timeinfo=localtime(&rawtime);
	strftime(timebuf,TIME_BUFFER_LENGTH,"%c",timeinfo);

	fprintf(fp, "%s %s %s HTTP/%d.%d\n", timebuf,
		request->method.c_str(), request->uri.c_str(),
		request->http_version_major, request->http_version_minor);

	fflush(fp);
}

void ex_program(int sig)
{
	printf("signal %d received.\n", sig);
	if (fp)
		fclose(fp);
	exit(0);
}

void http_example()
{
	ssl::init();	 // don't forget
	ssl::context server_ctx(ssl::sslv23_server);
	server_ctx.use_certificate_file("chain.pem", ssl::pem);
	server_ctx.use_rsa_private_key_file("privkey.pem", ssl::pem);
	server_ctx.set_cipher_list("HIGH:MEDIUM:!aNULL:+SHA1:+MD5:+HIGH:+MEDIUM:!kEDH");
	//server_ctx.set_cipher_list("ALL:!ADH:!EXPORT56:RC4+RSA:+HIGH:+MEDIUM:+LOW:+SSLv2:+EXP:+eNULL");
	iocp::service svr;
	iocp::acceptor http_acceptor(svr), https_acceptor(svr);

	iocp::error_code ec = http_acceptor.bind_and_listen(iocp::address::any, 60080, 500);
	if (ec) {
		std::cout << ec.to_string() << std::endl;
		return;
	}
	ec = https_acceptor.bind_and_listen(iocp::address::any, 60443, 500);
	if (ec) {
		std::cout << ec.to_string() << std::endl;
		return;
	}

	http_server *http = new http_server(svr);
	http_acceptor.asyn_accept(http->socket(), http, on_http_accept, &http_acceptor);

	https_server *https = new https_server(svr, server_ctx);
	https_acceptor.asyn_accept(https->socket(), https, on_https_accept, &https_acceptor);
	https = new https_server(svr, server_ctx);
	https_acceptor.asyn_accept(https->socket(), https, on_https_accept, &https_acceptor);

 	iocp::thread t0(http_thread_proc, &svr);
// 	iocp::thread t1(http_thread_proc, &svr);
// 	iocp::thread t2(http_thread_proc, &svr);

	printf("started\n");
	iocp::thread monitor(http_monitor_thread, &svr);
	signal(SIGINT, ex_program);

	size_t result = svr.run(ec);
 	t0.join();
// 	t1.join();
// 	t2.join();

	std::cout << "finished " << result << " operations" << std::endl;
	std::cout << ec.to_string() << std::endl;
}

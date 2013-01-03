#include "fiber_http_server.h"

static char request_string[] = "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n";

static void tcp_proc(FiberConContext *ctx, const iocp::error_code &init_ec)
{
	iocp::error_code ec;
	size_t bytes_transferred = 0;
	/// check whether connection is a success
	if (init_ec) {
		std::cerr << "on connection" << init_ec.to_string() << std::endl;
		return;
	}
	/// send
	ec = ctx->Send(request_string, sizeof(request_string)-1, bytes_transferred);
	if (ec) {
		std::cerr << "on sending" << ec.to_string() << std::endl;
		return;
	}
	/// recv
	char *buf = new char[1024];
	
	ec = ctx->Recv(buf, 1024, bytes_transferred);
	if (!ec)
		printf("%.*s", bytes_transferred, buf);
	delete[] buf;
	if (ec) {
		std::cerr << "on receiving" << ec.to_string() << std::endl;
		return;
	}
	ctx->Close();
}

static void ssl_proc(FiberConContext *ctx,  const iocp::error_code &init_ec)
{
	iocp::error_code ec;
	size_t bytes_transferred = 0;
	/// check whether connection is a success
	if (init_ec) {
		std::cerr << "on connection" << init_ec.to_string() << std::endl;
		return;
	}
	/// handshake
	ec = ctx->Handshake();
	if (ec) {
		std::cerr << "on handshaking" << ec.to_string() << std::endl;
		return;
	}
	/// send
	ec = ctx->Send(request_string, sizeof(request_string)-1, bytes_transferred);
	if (ec) {
		std::cerr << "on sending" << ec.to_string() << std::endl;
		return;
	}
	/// recv
	char *buf = new char[1024];
	ec = ctx->Recv(buf, 1024, bytes_transferred);
	if (!ec)
		printf("%.*s", bytes_transferred, buf);
	delete[] buf;
	if (ec) {
		std::cerr << "on receiving" << ec.to_string() << std::endl;
		return;
	}
	/// shutdown
	ec = ctx->Shutdown();
	if (ec) {
		std::cerr << "on receiving" << ec.to_string() << std::endl;
		return;
	}
	ctx->Close();
}

// extern "C" 
// {
// 	void VLDMarkAllLeaksAsReported ();
// };

void fiber_client_test()
{
	FiberWebServer web_server;
	web_server.AppendThread(2);
	web_server.SetCertificate("chain.pem", "privkey.pem");
	web_server.SetCipherList("HIGH:MEDIUM:!aNULL:+SHA1:+MD5:+HIGH:+MEDIUM:!kEDH");


	web_server.connect("127.0.0.1", 80, false, NULL, tcp_proc);
	web_server.connect("127.0.0.1", 443, true, NULL, ssl_proc);

	//VLDMarkAllLeaksAsReported();

	for (;;)
		Sleep(500);
}

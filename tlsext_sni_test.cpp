#include "fiber_http_server.h"

static char response_string[] = "HTTP/1.1 200 OK\r\nDate: Thu, 08 Mar 2012 08:23:34 GMT\r\nServer: MyIOCP/2.2.21 (Win32) mod_ssl/2.2.21 OpenSSL/0.9.8r\r\nLast-Modified: Sat, 20 Nov 2004 06:16:24 GMT\r\nETag: \"3000000002dcf-2c-3e94a9010be00\"\r\nAccept-Ranges: bytes\r\nContent-Length: 44\r\nConnection: close\r\nContent-Type: text/html\r\n\r\n<html><body><h1>It works!</h1></body></html>";

static void http_proc(FiberConContext *ctx, const iocp::error_code &init_ec)
{
	iocp::error_code ec;
	size_t bytes_transferred = 0;
	/// check accept error
	if (init_ec) {
		std::cerr << "on accepting: " << init_ec.to_string() << std::endl;
		return;
	}
	/// recv
	char *buf = new char[1024];
	ec = ctx->Recv(buf, 1024, bytes_transferred);
	delete[] buf;
	if (ec) {
		std::cerr << "on receiving: " << ec.to_string() << std::endl;
		return;
	}
	/// send
	ec = ctx->Send(response_string, sizeof(response_string)-1, bytes_transferred);
	if (ec) {
		std::cerr << "on sending: " << ec.to_string() << std::endl;
		return;
	}
	ctx->Close();
}

static void https_proc(FiberConContext *ctx, const iocp::error_code &init_ec)
{
	iocp::error_code ec;
	size_t bytes_transferred = 0;
	/// check accept error
	if (init_ec) {
		std::cerr << "on accepting: " << init_ec.to_string() << std::endl;
		return;
	}
	/// handshake
	ec = ctx->Handshake();
	if (ec) {
		std::cerr << "on handshaking: " << ec.to_string() << std::endl;
		return;
	}
	/// recv
	char *buf = new char[1024];
	ec = ctx->Recv(buf, 1024, bytes_transferred);
	delete[] buf;
	if (ec) {
		std::cerr << "on receiving: " << ec.to_string() << std::endl;
		return;
	}
	/// send
	ec = ctx->Send(response_string, sizeof(response_string)-1, bytes_transferred);
	if (ec) {
		std::cerr << "on sending: " << ec.to_string() << std::endl;
		return;
	}
	/// shutdown
	ec = ctx->Shutdown();
	if (ec) {
		std::cerr << "on shutdowning: " << ec.to_string() << std::endl;
		return;
	}
	ctx->Close();
}

// extern "C" 
// {
// 	void VLDMarkAllLeaksAsReported ();
// };

void tlsext_sni_test()
{
	FiberWebServer web_server;
	web_server.AppendThread(2);
	web_server.SetCertificate("chain.pem", "privkey.pem");
	web_server.SetCipherList("HIGH:MEDIUM:!aNULL:+SHA1:+MD5:+HIGH:+MEDIUM:!kEDH");
#ifndef OPENSSL_NO_TLSEXT
	web_server.AddVirtualHost("www.example.com", "server_crt_1024.pem", "server_key_1024.pem", NULL);
#endif

	if (web_server.startHTTP("0.0.0.0", 60080, 100, http_proc))
		std::cerr << "can not start http server" << std::endl;
	if (web_server.startHTTPS("0.0.0.0", 60443, 100, https_proc))
		std::cerr << "can not start https server" << std::endl;

	//VLDMarkAllLeaksAsReported();

	for (;;)
		Sleep(500);
}

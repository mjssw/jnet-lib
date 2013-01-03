#include "http_server.h"

static char response_string[] = "HTTP/1.1 200 OK\r\nDate: Thu, 08 Mar 2012 08:23:34 GMT\r\nServer: MyIOCP/2.2.21 (Win32) mod_ssl/2.2.21 OpenSSL/0.9.8r\r\nLast-Modified: Sat, 20 Nov 2004 06:16:24 GMT\r\nETag: \"3000000002dcf-2c-3e94a9010be00\"\r\nAccept-Ranges: bytes\r\nContent-Length: 44\r\nConnection: close\r\nContent-Type: text/html\r\n\r\n<html><body><h1>It works!</h1></body></html>";

static int OnConnected(ConnectionContext *ctx)
{
	if (ctx->ec_) {
		std::cerr << "on connecting" << ctx->ec_.to_string() << std::endl;
		return -1;
	}

	if (ctx->type() == ConnectionContext::http) {
		char *buf = new char[1024];
		ctx->Recv(buf, 1024);
	}	// no need any more
// 	else
// 		ctx->Handshake();
	return 0;
}

static int OnHandshaked(ConnectionContext *ctx)
{
	if (ctx->ec_) {
		std::cerr << "on handshaking" << ctx->ec_.to_string() << std::endl;
		return -1;
	}

	char *buf = new char[1024];
	ctx->Recv(buf, 1024);
	return 0;
}

static int OnReceivedBytes(ConnectionContext *ctx)
{
	if (ctx->ec_) {
		std::cerr << "on receiving" << ctx->ec_.to_string() << std::endl;
		delete[] ctx->bytes_received_;
		return -1;
	}

	delete[] ctx->bytes_received_;
	ctx->Send(response_string, sizeof(response_string)-1);
	return 0;
}

static int OnSentBytes(ConnectionContext *ctx)
{
	if (ctx->ec_) {
		std::cerr << "on sending" << ctx->ec_.to_string() << std::endl;
		return -1;
	}

	return 0;
}

static int OnClosed(ConnectionContext *ctx)
{
	//std::cout << ctx << " closed" << std::endl;
	return 0;
}

// extern "C" 
// {
// 	void VLDMarkAllLeaksAsReported ();
// };

void http_server_test()
{
	IOCPWebServer web_server;
	web_server.AppendThread(1);
	web_server.SetCertificate("chain.pem", "privkey.pem");
	web_server.SetCipherList("HIGH:MEDIUM:!aNULL:+SHA1:+MD5:+HIGH:+MEDIUM:!kEDH");

	web_server.cbOnConnected = OnConnected;
	web_server.cbOnHandshaked = OnHandshaked;
	web_server.cbOnReceivedBytes = OnReceivedBytes;
	web_server.cbOnSentBytes = OnSentBytes;
	web_server.cbOnClosed = OnClosed;

	if (web_server.startHTTP("0.0.0.0", 60080, 100))
		std::cerr << "can not start http server" << std::endl;
 	if (web_server.startHTTPS("0.0.0.0", 60443, 100))
		std::cerr << "can not start https server" << std::endl;

	//VLDMarkAllLeaksAsReported();

#ifdef _WIN32
	for (;;)
		Sleep(500);
#else
	for (;;)
		sleep(1);
#endif
}

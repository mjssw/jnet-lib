#include "http_server.h"

static char request_string[] = "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n";

static int OnConnected(ConnectionContext *ctx)
{
	if (ctx->ec_) {
		std::cerr << "on connecting" << ctx->ec_.to_string() << std::endl;
		return -1;
	}

	if (ctx->type() == ConnectionContext::tcp)
		ctx->Send(request_string, sizeof(request_string)-1);
	// no need any more
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

	ctx->Close();

	ctx->Send(request_string, sizeof(request_string)-1);
	return 0;
}

static int OnReceivedBytes(ConnectionContext *ctx)
{
	if (ctx->ec_) {
		std::cerr << "on receiving" << ctx->ec_.to_string() << std::endl;
		return -1;
	}

	printf("%.*s\n", ctx->bytes_transferred_, ctx->bytes_received_);
	delete[] ctx->bytes_received_;
	return 0;
}

static int OnSentBytes(ConnectionContext *ctx)
{
	if (ctx->ec_) {
		std::cerr << "on sending" << ctx->ec_.to_string() << std::endl;
		return -1;
	}

	char *buf = new char[1024];
	ctx->Recv(buf, 1024);

	return 0;
}

static int OnClosed(ConnectionContext *ctx)
{
	std::cout << ctx << " closed" << std::endl;
	return 0;
}

// extern "C" 
// {
// 	void VLDMarkAllLeaksAsReported ();
// };

void client_test()
{
	IOCPWebServer web_server;
	web_server.AppendThread(2);
	web_server.SetCertificate("chain.pem", "privkey.pem");
	web_server.SetCipherList("HIGH:MEDIUM:!aNULL:+SHA1:+MD5:+HIGH:+MEDIUM:!kEDH");

	web_server.cbOnConnected = OnConnected;
	web_server.cbOnHandshaked = OnHandshaked;
	web_server.cbOnReceivedBytes = OnReceivedBytes;
	web_server.cbOnSentBytes = OnSentBytes;
	web_server.cbOnClosed = OnClosed;

	//web_server.connect("127.0.0.1", 80, false, NULL);
	web_server.connect("127.0.0.1", 443, true, NULL);

	//VLDMarkAllLeaksAsReported();

	for (;;)
		Sleep(500);
}

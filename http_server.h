#include "net_service.h"
#include "ssl_socket.h"
#include "iocp_thread.h"
#include <vector>
#include <iostream>

class IOCPWebServer;
class ConnectionContext
{
public:
	enum stream_type { tcp = 0, ssl = 2, http = 1, https = 3 };
	ConnectionContext(IOCPWebServer &svr, stream_type type);
	~ConnectionContext();

	stream_type type() { return type_; }

	void Connect(const char *pszIP, unsigned short port);
	void Handshake();
	void Shutdown();
	void Recv(char *buf, int len);
	void Send(const char *buf, int len);
	void Close();

	/// use these variables when callback is invoked
	void *context_;
	iocp::error_code ec_;
	size_t bytes_transferred_;
	char *bytes_received_;

private:
	/// should not use directly
	stream_type type_;

	friend class IOCPWebServer;
	IOCPWebServer *server_;

	iocp::socket *tcp_stream_;
	iocp::ssl_socket *ssl_stream_;

	long ref_;
	long shutdown_flag_;
	long close_flag_;
	bool check_closed();
};

class IOCPWebServer
{
public:
	IOCPWebServer();
	~IOCPWebServer();

	int AppendThread(int num = 1);
	int SetCertificate(const char *pszCertChain, const char *pszPrivateKey);
	int SetCipherList(const char *pszCipher);

	int startHTTP(const char *pszIP, unsigned short port, int backlog);
	int startHTTPS(const char *pszIP, unsigned short port, int backlog);
	int connect(const char *pszIP, unsigned short port, bool use_ssl, void *ctx);
	int run();

	typedef int (*fnIOCPCallback)( ConnectionContext* pHC );

	fnIOCPCallback cbOnConnected;
	fnIOCPCallback cbOnHandshaked ;
	fnIOCPCallback cbOnReceivedBytes;
	fnIOCPCallback cbOnSentBytes;
	fnIOCPCallback cbOnClosed;

private:
	friend class ConnectionContext;

	static void http_thread_proc(void *arg)
	{
		iocp::service *service = static_cast<iocp::service *>(arg);
		iocp::error_code ec;
		size_t result = service->run(ec);
		if (!ec) {
			std::cout << result << " done" << std::endl;;
		}
		else {
			std::cerr << iocp::error::describe_error(ec) << std::endl;
		}
	}

	void CheckAndDelete(ConnectionContext *ctx);

	static void on_connect(void *binded, const iocp::error_code &ec);
	static void on_http_accept(void *acceptor_point, const iocp::error_code &ec, void *socket_point);
	static void on_https_accept(void *acceptor_point, const iocp::error_code &ec, void *socket_point);
	static void on_handshake(void *binded, const iocp::error_code &ec);
	static void on_shutdown(void *binded, const iocp::error_code &ec);

	static void on_send(void *binded, const iocp::error_code &ec, size_t bytes_transferred);
	static void on_receive(void *binded, const iocp::error_code &ec, size_t bytes_transferred, char *buf);

	iocp::service service_;
	ssl::context *server_context_;
	ssl::context *client_context_;

	std::vector<iocp::thread *> threads_;

	iocp::acceptor *http_acceptor_;
	iocp::acceptor *https_acceptor_;
};

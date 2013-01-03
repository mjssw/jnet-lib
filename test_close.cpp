#include "net_service.h"
#include "net_socket.h"

#include <stdio.h>

char buffer[1024];

void on_received(void *binded, const iocp::error_code &ec, size_t bytes_transferred, char *buffer)
{
  if (ec)
    printf("recv: %s\n", ec.to_string().c_str());
}

void on_connected(void *binded, const iocp::error_code &ec)
{
  iocp::socket *sock = (iocp::socket *)binded;

  if (ec)
    printf("connect: %s\n", ec.to_string().c_str());

  sock->asyn_recv_some(buffer, 1024, on_received, &sock);

  sock->close();
}

void test_close_socket()
{
  iocp::error_code ec;
  iocp::service svr;
  iocp::socket sock(svr);

  sock.asyn_connect("192.168.0.124", 60080, on_connected, &sock);

  svr.run(ec);
}

#include "epoll_service.h"
#include "epoll_socket.h"
#include "iocp_thread.h"
#include "iocp_resolver.h"
#include <stdio.h>

#define TEST_HTTP_SERVER "192.168.1.172"
#define TEST_HTTP_PORT 80

/* test 0 */
static int test_run_exit()
{
  iocp::service svr;
  iocp::error_code ec;

  if (svr.run(ec) == 0) {
    if (ec) {
      puts(ec.to_string().c_str());
      return -1;
    }
  }
  else {
    puts(ec.to_string().c_str());
    return -1;
  }

  return 0;
}

/* test 1 */
static int test1_post_flag = 0;
static void test1_on_post(void *binded, const iocp::error_code &ec)
{
  if (ec || binded != (void *)1)
    puts(ec.to_string().c_str());
  else
    test1_post_flag = 1;
}

static int test_post()
{
  iocp::service svr;
  iocp::error_code ec;

  svr.post(test1_on_post, (void *)1);
  if (svr.run(ec) == 1) {
    if (ec) {
      puts(ec.to_string().c_str());
      return -1;
    }
    else {
      if (test1_post_flag == 1)
        return 0;
      puts("post callback failed");
      return -1;
    }
  }
  else {
    puts(ec.to_string().c_str());
    return -1;
  }

  return 0;
}

/* test 2 */
static int test2_connect_flag = 0;
static void test2_on_connected(void *binded, const iocp::error_code &ec)
{
  if (ec || binded != (void *)1)
    puts(ec.to_string().c_str());
  else
    test2_connect_flag = 1;
}

static int test_connect()
{
  iocp::service svr;
  iocp::socket s(svr);
  iocp::error_code ec;
  iocp::address addr(TEST_HTTP_SERVER);

  s.asyn_connect(addr, TEST_HTTP_PORT, test2_on_connected, (void *)1);
  if (svr.run(ec) > 0) {
    if (ec) {
      puts(ec.to_string().c_str());
      return -1;
    }
    else {
      if (test2_connect_flag == 1)
        return 0;
      puts("connect failed");
      return -1;
    }
  }
  else {
    puts(ec.to_string().c_str());
    return -1;
  }

  return 0;
}

/* test 3 */
static int test3_everything_ok = 0;
static char test3_request[] = "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n";
static char test3_response[1024];

static void test3_on_recv(void *binded, const iocp::error_code &ec, size_t len, char *buf)
{
  if (ec) {
    puts(ec.to_string().c_str());
  }
  else {
    printf("%.*s\n", (int)len, buf);
    test3_everything_ok = 1;
  }
}

static void test3_on_sent(void *binded, const iocp::error_code &ec, size_t len)
{
  iocp::socket *s = static_cast<iocp::socket *>(binded);
  if (ec)
    puts(ec.to_string().c_str());
  else {
    puts(test3_request);
    s->asyn_recv_some(test3_response, 1024, test3_on_recv, s);
  }
}

static void test3_on_connected(void *binded, const iocp::error_code &ec)
{
  iocp::socket *s = static_cast<iocp::socket *>(binded);
  if (ec)
    puts(ec.to_string().c_str());
  else {
    puts("connected\n");
    s->asyn_send(test3_request, sizeof(test3_request)-1, test3_on_sent, s);
  }
}

static int test_send_recv()
{
  iocp::service svr;
  iocp::socket s(svr);
  iocp::error_code ec;
  iocp::address addr(TEST_HTTP_SERVER);

  puts("3 steps");

  s.asyn_connect(addr, TEST_HTTP_PORT, test3_on_connected, (void *)&s);
  if (svr.run(ec) > 0) {
    if (ec) {
      puts(ec.to_string().c_str());
      return -1;
    }
    else {
      if (test3_everything_ok == 1)
        return 0;
      puts("something error");
      return -1;
    }
  }
  else {
    puts(ec.to_string().c_str());
    return -1;
  }
  
  return 0;
}

/* test 4 */
static void test4_on_accepted(void *binded, const iocp::error_code &ec, void *data)
{
  if (ec)
    puts(ec.to_string().c_str());
  else
    puts("accepted");
}

static void test4_on_connected(void *binded, const iocp::error_code &ec)
{
  iocp::socket *s = static_cast<iocp::socket *>(binded);
  if (ec)
    puts(ec.to_string().c_str());
  else
    puts("connected");
  s->close();
}

static int test_connect_accept()
{
  iocp::service svr;
  iocp::socket server(svr), client(svr);
  iocp::acceptor acpt(svr);
  iocp::error_code ec;

  ec = acpt.bind_and_listen(iocp::address::loopback, 60001, 100);
  if (ec)
    puts(ec.to_string().c_str());

  acpt.asyn_accept(server, &server, test4_on_accepted, &acpt);
  client.asyn_connect(iocp::address::loopback, 60001, test4_on_connected, &client);

  puts("");
  if (svr.run(ec) > 0) {
    if (ec) {
      puts(ec.to_string().c_str());
      return -1;
    }
    else {
      return 0;
    }
  }
  else {
    puts(ec.to_string().c_str());
    return -1;
  }

  return 0;
}

/* test 5 */
static int test5_flag = 0;
static void test5_thread_proc(void *arg)
{
  test5_flag = 1;
  for (;;) {
    sleep(1);
    printf(".....");
  }
}

static int test_thread()
{
  iocp::thread thd(test5_thread_proc, (void *)1, false);
  sleep(2);
  if (test5_flag == 1) {
    puts("thread started too early");
    return -1;
  }
  thd.start();
  sleep(2);
  if (test5_flag == 0) {
    puts("thread started too late");
    return -1;
  }
  if (thd.join(1000) != 0) {
    puts("did not return with timeout as expected");
    return -1;
  }
  thd.kill();
  fflush(stdout);
  puts("you should not see any dot after this line");

  return 0;
}

#define test_case(id, case) {\
  printf("test% 2d: ", id);\
  if (!case())\
    puts("done");\
}

/* test 6 */

static void test6_resolver_callback(void *binded, const iocp::error_code &ec, const iocp::address &addr)
{
  if (ec)
    iocp::error::describe_error(ec);
  else
    printf("%s-%s\n", (char *)binded, ::inet_ntoa(addr));
}

static int test_resolver()
{
  iocp::error_code ec;
  iocp::service service;
  iocp::resolver resolver(service);
  iocp::address addr;
  puts("");
  ec = iocp::resolver::resolve("localhost", addr);
  if (ec)
    puts(ec.to_string().c_str());
  else
    printf("%s-%s\n", "localhost", ::inet_ntoa(addr));
  ec = iocp::resolver::resolve("www.baidu.com", addr);
  if (ec)
    puts(ec.to_string().c_str());
  else
    printf("%s-%s\n", "www.baidu.com", ::inet_ntoa(addr));

  resolver.asyn_resolve(service, "localhost", test6_resolver_callback, (void *)"localhost");
  resolver.asyn_resolve(service, "www.baidu.com", test6_resolver_callback, (void *)"www.baidu.com");
  size_t result = service.run(ec);
  if (result > 0 && !ec)
    return 0;
  else
    puts(ec.to_string().c_str());

  return -1;
}

void tcp_example();
void tcp_http_server_example();
void ssl_example();
void ssl_https_server_example();

int main(int argc, char *argv[])
{
  test_case(0, test_run_exit);
  test_case(1, test_post);
  test_case(2, test_connect);
  test_case(3, test_send_recv);
  test_case(4, test_connect_accept);
  //test_case(5, test_thread);
  //test_case(6, test_resolver);

  //tcp_example();
  //ssl_example();
  //tcp_http_server_example();
  //ssl_https_server_example();

	return 0;
}

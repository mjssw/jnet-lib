#Jeff 2012/4/16
#http://learn.akae.cn/media/ch22s04.html

SSL_INC = /usr/local/ssl64/include
SSL_LIB = /usr/local/ssl64/lib

FIB_INC = ../simple_fiber/
FIB_LIB = ../simple_fiber/

CPP = gcc
CPPFLAGS = -m64 -D_NET_ENABLE_SSL_ -I$(FIB_INC)

CXX = g++
CXXFLAGS = -m64 -I$(SSL_INC) -L$(SSL_LIB) -I$(FIB_INC) -L$(FIB_LIB) -D_NET_ENABLE_SSL_ -g
LIBS = -lpthread -lssl -lcrypto -lfiber64

SRCS = iocp_error.cpp epoll_service.cpp iocp_base_type.cpp\
		epoll_socket.cpp iocp_address.cpp ssl_socket.cpp iocp_resolver.cpp\
		iocp_thread.cpp http_server.cpp fiber_scheduler.cpp fiber_socket.cpp\
		fiber_http_server.cpp http_socket.cpp epoll_file.cpp
OBJS = $(SRCS:.cpp=.o) http_parser.o

TSRCS = epoll_test.cpp tcp.cpp tcp_http_server.cpp ssl.cpp\
		ssl_https_server.cpp test.cpp http_server_test.cpp\
		fiber_http_server_test.cpp file.cpp
TOBJS = epoll_test.o tcp.o tcp_http_server.o ssl.o\
		ssl_https_server.o

OUTPUT = test epoll_test

.PHONY: all
all: $(OUTPUT) $(OBJS)

epoll_test: $(TOBJS) $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

test: test.o http_server_test.o file.o fiber_http_server_test.o $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

.PHONY: clean
clean:
	rm -f $(OUTPUT) *.o *.d

include $(SRCS:.cpp=.d)
include $(TSRCS:.cpp=.d)

%.d: %.cpp
	set -e; rm -f $@; \
	$(CPP) -MM $(CPPFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

http_parser.o: http_parser.c http_parser.h
	gcc -c -m64 $< -o $@

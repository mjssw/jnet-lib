// iocp_address.h
#ifndef _IOCP_ADDRESS_H_
#define _IOCP_ADDRESS_H_

#ifdef _MSC_VER
#pragma once
#endif

#ifndef _IOCP_NO_INCLUDE_
#include "iocp_config.h"
#endif

namespace iocp {

class address {
public:

#ifdef _WIN32
	address(const char *addr) { addr_.S_un.S_addr = inet_addr(addr); }
	address(unsigned int addr = 0U) { addr_.S_un.S_addr = addr; }
  address(const struct in_addr &addr) { addr_.S_un.S_addr = addr.S_un.S_addr; }
#elif defined __linux__
	address(const char *addr) { addr_.s_addr = inet_addr(addr); }
	address(in_addr_t addr = 0) { addr_.s_addr = addr; }
  address(const struct in_addr &addr) {  addr_.s_addr = addr.s_addr; }
#endif

	operator struct in_addr() const { return addr_; }
#ifdef _WIN32
  operator unsigned int() const { return addr_.S_un.S_addr; }
#elif defined __linux__
  operator in_addr_t() const { return addr_.s_addr; }
#endif

	static const iocp::address any;
	static const iocp::address loopback;

private:
	struct in_addr addr_;
};

}

#endif

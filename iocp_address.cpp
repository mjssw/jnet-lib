// iocp_address.cpp
#ifndef _IOCP_NO_INCLUDE_
#include "iocp_address.h"
#endif

namespace iocp {

const iocp::address iocp::address::any(0U);
const iocp::address iocp::address::loopback("127.0.0.1");

}

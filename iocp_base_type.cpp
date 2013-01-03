// iocp_base_type.cpp
#ifndef _IOCP_NO_INCLUDE_
#include "iocp_base_type.h"
#include "net_service.h"
#endif

namespace iocp {

base_type::base_type(iocp::service &service): service_(service)
{
	service_.construct(*this);
}

base_type::~base_type()
{
	service_.destory(*this);
}

}

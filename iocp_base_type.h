// iocp_base_type.h
#ifndef _IOCP_BASE_TYPE_H_
#define _IOCP_BASE_TYPE_H_

#ifdef _MSC_VER
#pragma once
#endif

#ifndef _IOCP_NO_INCLUDE_
#include "noncopyable.h"
#endif

namespace iocp {

class service;
/**
 * @brief base class for all types running on the service
 */
class base_type: private noncopyable
{
public:
  /**
   * Constructor
   * @param[in] service a service instance
   */
	base_type(iocp::service &service);
  /**
   * Destructor
   */
	virtual ~base_type();

   /**
   * Get the service
   * @retval service the stored service reference
   */
	iocp::service &service() { return service_; }

private:
	friend class iocp::service;

	iocp::service &service_;

	base_type *prev_;
	base_type *next_;
};

}

#endif

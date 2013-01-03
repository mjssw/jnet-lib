// iocp_thread.cpp
#ifndef _IOCP_NO_INCLUDE_
#include "iocp_thread.h"
#endif

namespace iocp {

#ifdef _WIN32

// thread::thread(): runner_(0), arg_(0), started_(false)
// {
// 
// }

thread::thread(runner_type runner, void *arg, bool run): runner_(runner), arg_(arg), started_(run)
{
	handle_ = ::CreateThread(0, 0, &thread::runner_wrapper, this, run ? 0 : CREATE_SUSPENDED, 0);
}

thread::~thread()
{
  ::CloseHandle(handle_);
}

int thread::join(int timeout)
{
	if (started_)
    return ::WaitForSingleObject(handle_, timeout==-1?INFINITE:timeout)==WAIT_TIMEOUT?0:1;
  return 1;
}

void thread::kill()
{
  ::TerminateThread(handle_, 1);
}

DWORD thread::runner_wrapper(LPVOID arg)
{
	iocp::thread *t = static_cast<iocp::thread *>(arg);
	t->runner_(t->arg_);

	return 0L;
}

#elif defined __linux__

// thread::thread(): runner_(0), arg_(0), started_(false)
// {
// 
// }

thread::thread(runner_type runner, void *arg, bool run): runner_(runner), arg_(arg), started_(run)
{
  int oldval;
  ::pthread_attr_init(&attr_);
  ::pthread_attr_setdetachstate(&attr_, PTHREAD_CREATE_DETACHED);
  if (run)
    pre_event_.set();
  ::pthread_create(&thread_, &attr_, &thread::runner_wrapper, this);
  ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldval);
  ::pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &oldval);
}

thread::~thread()
{
  ::pthread_attr_destroy(&attr_);
}

int thread::join(int timeout)
{
  if (started_)
    return pro_event_.wait(timeout);
  return 1;
}

void thread::kill()
{
  ::pthread_cancel(thread_);
}

void *thread::runner_wrapper(void *arg)
{
  iocp::thread *t = static_cast<iocp::thread *>(arg);
  t->pre_event_.wait();
  t->runner_(t->arg_);

  t->pro_event_.set();

  return NULL;
}

#endif

}

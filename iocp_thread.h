// iocp_thread.h
#ifndef _IOCP_THREAD_H_
#define _IOCP_THREAD_H_

#ifdef _MSC_VER
#pragma once
#endif

#ifndef _IOCP_NO_INCLUDE_
#include "iocp_config.h"
#include "noncopyable.h"
#include "iocp_lock.h"
#endif

namespace iocp {

#ifdef _WIN32

class thread: private noncopyable
{
public:
	typedef void (__cdecl *runner_type)(void *arg);

  // TODO:
  //thread();
	thread(runner_type runner, void *arg, bool run = true);
  ~thread();

	void start() { if (!started_) ::ResumeThread(handle_); started_ = true; }

	int join(int timeout = -1);
  void kill();

private:
	HANDLE handle_;
	runner_type runner_;
	void *arg_;
	bool started_;

	static DWORD WINAPI runner_wrapper(LPVOID arg);
};

#elif defined __linux__

class thread: private noncopyable
{
public:
  typedef void (*runner_type)(void *arg);

  // TODO:
  //thread();
  thread(runner_type runner, void *arg, bool run = true);
  ~thread();

  void start() { if (!started_) pre_event_.set(); started_ = true; }

  int join(int timeout = -1);
  void kill();

private:
  event pre_event_;
  event pro_event_;
  pthread_t thread_;
  pthread_attr_t attr_;

  runner_type runner_;
  void *arg_;
  bool started_;

  static void *runner_wrapper(void *arg);
};

#endif

}

#endif // _IOCP_THREAD_H_

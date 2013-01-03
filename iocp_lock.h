// iocp_lock.h
#ifndef _IOCP_LOCK_H_
#define _IOCP_LOCK_H_

#ifdef _MSC_VER
#pragma once
#endif

#ifndef _IOCP_NO_INCLUDE_
#include "iocp_config.h"
#endif

namespace iocp {

#ifdef _WIN32

class mutex
{
public:
	mutex() { InitializeCriticalSection(&impl_); }
	~mutex() { DeleteCriticalSection(&impl_); }

	void lock() { EnterCriticalSection(&impl_); }
	void unlock() { LeaveCriticalSection(&impl_); }

private:
	CRITICAL_SECTION impl_;
};

class event
{
public:
  enum state { nonsignaled = 0, signaled };
  enum type { auto_reset = 0, manual_reset };

  event(state init_state = nonsignaled, type reset_type = auto_reset)
  {
    impl_ = ::CreateEvent(NULL, reset_type==auto_reset?FALSE:TRUE, init_state==nonsignaled?FALSE:TRUE, NULL);
  }

  ~event()
  {
    CloseHandle(impl_);
  }

  void set() { ::SetEvent(impl_); }
  void reset() { ::ResetEvent(impl_); }
  // in millisecond
  int wait(int timeout = -1)
  {
    DWORD ret;
    ret = ::WaitForSingleObject(impl_, timeout==-1?INFINITE:timeout);
    if (ret == WAIT_TIMEOUT)
      return 0;
    else if (ret == WAIT_OBJECT_0)
      return 1;
    else
      return -1;
  }

private:
  HANDLE impl_;
};

#define sync_fetch_and_add(ptr, val) InterlockedExchangeAdd((ptr), (val))
#define sync_compare_and_swap(ptr, old, val) (InterlockedCompareExchange((ptr), (val), (old)) == old)
#define sync_set(ptr, val) InterlockedExchange((ptr), (val))
#define sync_get(ptr) InterlockedExchangeAdd((ptr), 0)
#define sync_inc(ptr) InterlockedIncrement((ptr))
#define sync_dec(ptr) InterlockedDecrement((ptr))

#define sync_compare_and_swap64(ptr, old, val) (InterlockedCompareExchange64((volatile LONG64 *)(ptr), (LONG64)(val), (LONG64)(old)) == old)
#define sync_set64(ptr, val) InterlockedExchange64((volatile LONG64 *)(ptr), (LONG64)(val))
#define sync_get64(ptr) InterlockedExchangeAdd64((volatile LONG64 *)(ptr), (LONG64)0)
#define sync_inc64(ptr) InterlockedIncrement64((volatile LONG64 *)(ptr))
#define sync_dec64(ptr) InterlockedDecrement64((volatile LONG64 *)(ptr))

#elif defined __linux__

class mutex
{
public:
	mutex() { ::pthread_mutex_init(&impl_, NULL); }
	~mutex() { ::pthread_mutex_destroy(&impl_); }

	void lock() { ::pthread_mutex_lock(&impl_); }
	void unlock() { ::pthread_mutex_unlock(&impl_); }

private:
	pthread_mutex_t impl_;
};

class event
{
public:
  enum state { nonsignaled = 0, signaled };
  enum type { auto_reset = 0, manual_reset };

  event(state init_state = nonsignaled, type reset_type = auto_reset)
  {
    ::pthread_cond_init(&cond_, NULL);
    ::pthread_mutex_init(&mutex_, NULL);
    type_ = reset_type;
    state_= init_state;
  }

  ~event()
  {
    ::pthread_cond_destroy(&cond_);
    ::pthread_mutex_destroy(&mutex_);
  }

  void set()
  {
    ::pthread_mutex_lock(&mutex_);
    state_ = signaled;
    ::pthread_mutex_unlock(&mutex_);
    ::pthread_cond_broadcast(&cond_);
  }

  void reset()
  {
    ::pthread_mutex_lock(&mutex_);
    state_ = nonsignaled;
    ::pthread_mutex_unlock(&mutex_);
  }

  // in millisecond
  int wait(int timeout = -1)
  {
    int ret = 0;
    ::pthread_mutex_lock(&mutex_);

    if (timeout == -1) {
      while (state_ != signaled && ret == 0)
        ret = ::pthread_cond_wait(&cond_, &mutex_);
    }
    else {
      struct timeval tv;
      struct timespec ts;
      ::gettimeofday(&tv, NULL);
      ts.tv_sec = tv.tv_sec + timeout / 1000UL;
      ts.tv_nsec = tv.tv_usec * 1000UL + (timeout % 1000UL) * 1000000UL;
      ts.tv_sec += ts.tv_nsec / 1000000000UL;
      ts.tv_nsec = ts.tv_nsec % 1000000000UL;
      while (state_ != signaled && ret == 0)
        ret = ::pthread_cond_timedwait(&cond_, &mutex_, &ts);
    }

    if (type_ == auto_reset && ret == 0)
      state_ = nonsignaled;

    ::pthread_mutex_unlock(&mutex_);
    if (ret == ETIMEDOUT)
      ret = 0;
    else if (ret == 0)
      ret = 1;
    else
      ret = -1;
    return ret;
  }

private:
  pthread_cond_t  cond_;
  pthread_mutex_t mutex_;
  type type_;
  state state_;
};

#define sync_fetch_and_add(ptr, val) ::__sync_fetch_and_add((ptr), (val))
#define sync_compare_and_swap(ptr, old, val) ::__sync_bool_compare_and_swap((ptr), (old), (val))
#define sync_set(ptr, val) do { ::__sync_synchronize(); ::__sync_lock_test_and_set((ptr), (val)); } while(0)
#define sync_get(ptr) ::__sync_fetch_and_add((ptr), 0)
#define sync_inc(ptr) ::__sync_add_and_fetch((ptr), 1)
#define sync_dec(ptr) ::__sync_sub_and_fetch((ptr), 1)

#define sync_compare_and_swap64(ptr, old, val) sync_compare_and_swap((ptr), (old), (val))
#define sync_set64(ptr, val) sync_set((ptr), (val))
#define sync_get64(ptr) sync_get((ptr))
#define sync_inc64(ptr) sync_inc((ptr))
#define sync_dec64(ptr) sync_dec((ptr))

#endif

class scope_lock
{
public:
	scope_lock(mutex &cs): mutex_(cs), left_(false) { mutex_.lock(); }
	~scope_lock() { if (!left_) mutex_.unlock(); }

  void lock() { mutex_.lock(); }
  void unlock() { if (!left_) mutex_.unlock(); left_ = true; }

private:
	mutex &mutex_;
  bool left_;
};

}

#endif

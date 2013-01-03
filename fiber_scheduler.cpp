// fiber_scheduler.cpp
//#define _USE_SYSTEM_FIBER_
#ifndef _IOCP_NO_INCLUDE_
	#include "iocp_config.h"
	#include "fiber_scheduler.h"
	#include "iocp_lock.h"
#endif

//#define _USE_SYSTEM_FIBER_
#ifndef _USE_SYSTEM_FIBER_
  #include "simple_fiber.h"
  #ifdef _MSC_VER
    #ifndef _WIN64
      #ifdef _DEBUG
        #pragma comment(lib, "../Debug/simple_fiber.lib")
      #else
        #pragma comment(lib, "../Release/simple_fiber.lib")
      #endif
    #else
      #ifdef _DEBUG
        #pragma comment(lib, "../x64/Debug/simple_fiber.lib")
      #else
        #pragma comment(lib, "../x64/Release/simple_fiber.lib")
      #endif
    #endif
  #endif
#endif

namespace fiber {

scheduler *scheduler::current()
{
#ifdef _USE_SYSTEM_FIBER_
	return static_cast<scheduler *>(::GetFiberData());
#else
	return static_cast<scheduler *>(::get_current_fiber_arg());
#endif
}

scheduler *scheduler::convert_thread(void *arg)
{
	scheduler *ns = scheduler::alloc();
	ns->arg_ = arg;
#ifdef _USE_SYSTEM_FIBER_
	ns->fiber_ = ::ConvertThreadToFiber(ns);
#else
	ns->fiber_ = ::convert_thread(ns);
#endif
	//printf("convert %08x\n", ns->fiber_);
	return ns;
}

scheduler *scheduler::create(routine_type proc, void *arg)
{
	scheduler *ns = scheduler::alloc();
	ns->proc_ = proc;
	ns->arg_ = arg;
#ifdef _USE_SYSTEM_FIBER_
	ns->fiber_ = ::CreateFiberEx(0, 64*1024, 0, (LPFIBER_START_ROUTINE)&scheduler::wrapper, ns);
#else
	ns->fiber_ = ::make_fiber(64*1024, &scheduler::wrapper, ns);
#endif
	//printf("create %08x\n", ns->fiber_);
	return ns;
}

scheduler *scheduler::alloc()
{
	return new scheduler();
}

void scheduler::free()
{
#ifdef _USE_SYSTEM_FIBER_
	::DeleteFiber(fiber_);
#else
	::delete_fiber(fiber_);
#endif
	//printf("remove %08x\n", fiber_);
	delete this;
}

void scheduler::wrapper(void *arg)
{
	scheduler *f = static_cast<scheduler *>(arg);
	f->proc_(f->arg_);
	f->free_flag_ = true;
	f->yield();
}

// #include <stdio.h>
// #include <assert.h>

bool scheduler::switch_to(scheduler *f)
{
	return f->zwitch();
}

bool scheduler::zwitch()
{
	scheduler *to_switch = 0;
	scheduler *current_scheduler = scheduler::current();

	// already invoked, still not yield
	if (!sync_compare_and_swap((long *)&invoker_, 0, (long)current_scheduler))
	//if (InterlockedCompareExchange((long *)&invoker_, (long)current_scheduler, 0l) != 0)
		return false;
	//printf("switch from %08x to %08x\n", invoker_ ? invoker_->fiber_ : 0, fiber_);
#ifdef _USE_SYSTEM_FIBER_
	::SwitchToFiber(fiber_);
#else
	::switch_fiber(fiber_);
#endif
	if (free_flag_) {
		//printf("free fiber %08x\n", fiber_);
		free();
	}
	else {
		sync_set((long *)&invoker_, 0);
		//InterlockedExchange((long *)&invoker_, 0l);
	}
	return true;
}

void scheduler::yield()
{
	//printf("yield to %08x\n", invoker_ ? invoker_->fiber_ : 0);
	if (invoker_)
#ifdef _USE_SYSTEM_FIBER_
		::SwitchToFiber(invoker_->fiber_);
#else
		::switch_fiber(invoker_->fiber_);
#endif
}

}

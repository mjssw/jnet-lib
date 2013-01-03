// fiber_scheduler.h
#ifndef _FIBER_SCHEDULER_H_
#define _FIBER_SCHEDULER_H_

#ifdef _MSC_VER
#pragma once
#endif

namespace fiber {

class scheduler
{
public:
	typedef void (* routine_type)(void *arg);
	scheduler(): invoker_(0), free_flag_(false) {}

	static scheduler *current();

	static scheduler *convert_thread(void *arg);
	static scheduler *create(routine_type proc, void *arg);
	static bool switch_to(scheduler *f);
	void yield();

	void *argument() { return arg_; }
	void set_argument(void *arg) { arg_ = arg; }

	routine_type routine() { return proc_; }
	void set_routine(routine_type proc) { proc_ = proc; }

private:
	static void wrapper(void *arg);
	static scheduler *alloc();
	bool zwitch();
	void free();

	routine_type proc_;
	scheduler *invoker_;
	void *fiber_;
	void *arg_;
	bool free_flag_;
};

}

#endif

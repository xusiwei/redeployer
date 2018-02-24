//
// Created by xu on 18-2-2.
//

#include "FunctionScheduler.h"


using namespace std::chrono;

class RepeatFunc : public std::enable_shared_from_this<RepeatFunc> {
	std::function<void(void)> cb;
	std::chrono::milliseconds delay;
	std::chrono::milliseconds interval;
	int count;
	bool once;

	// maybe more than one function has the same `next run time`
	std::shared_ptr<RepeatFunc> next;

public:
	RepeatFunc(std::function<void()> f, std::chrono::milliseconds d, std::chrono::milliseconds i)
		: cb(f), delay(d), interval(i), count(0) {
		once = (milliseconds(0) == interval);
//		printf("RepeatFunc_%p born!\n", this);
	}

	RepeatFunc(std::function<void()> f, std::chrono::milliseconds d)
		: RepeatFunc(f, d, milliseconds(0)) {
		once = true;
	}

	~RepeatFunc() {
//		printf("RepeatFunc_%p die!\n", this);
	}

	time_point<system_clock, milliseconds> next_run_time() const {
		auto now = system_clock::now();
		if (count == 0) {
			return time_point_cast<milliseconds>(now + delay);
		}
		return time_point_cast<milliseconds>(now + interval);
	}


	void run() {
		if (next) {
			next->run();
		}
		if (cb) {
			cb();

			if (++count == 1 && once) {
				cb = nullptr;
			}
		}
	}

	std::shared_ptr<RepeatFunc> link(std::shared_ptr<RepeatFunc> other) {
		this->next = other;
		return shared_from_this();
	}

	bool valid() const {
		return bool(cb);
	}
};

FunctionScheduler::FunctionScheduler()
	: thread_(std::bind(&FunctionScheduler::run, this))
{
}

FunctionScheduler::~FunctionScheduler()
{
	shutdown();
}

std::shared_ptr<RepeatFunc> FunctionScheduler::call_one()
{
	std::unique_lock<std::mutex> _lock(mutex_);
	while (functions_.empty()) {
		condition_.wait(_lock);
	}

	auto it = functions_.begin();
	std::this_thread::sleep_until(it->first);

	auto prf = it->second;
	if (prf) {
		prf->run();
	}
	functions_.erase(it);
	return prf;
}

void FunctionScheduler::run()
{
	std::shared_ptr<RepeatFunc> prf;
	while (running_) {
		prf = call_one();
		if (!prf) {
			break;
		}
		if (prf->valid()) {
			push_one(prf);
		}
	}
}

void FunctionScheduler::push_one(std::shared_ptr<RepeatFunc> prf)
{
	std::lock_guard<std::mutex> _l(mutex_);
	time_point<system_clock, milliseconds> nrt;  // next run time
	if (prf) {
		nrt = prf->next_run_time();
	}
	auto it = functions_.find(nrt);
	if (it != functions_.end()) { // found it, has same next run time!
		it->second = prf->link(it->second);
	} else {
		functions_.insert(std::make_pair(nrt, prf));
	}
	condition_.notify_one();
}

void FunctionScheduler::schedule(std::function<void(void)> func,
                                 std::chrono::milliseconds delay,
                                 std::chrono::milliseconds interval)
{
	std::shared_ptr<RepeatFunc> prf{new RepeatFunc(func, delay, interval)};

	push_one(prf);
}

void FunctionScheduler::schedule(std::function<void(void)> func, std::chrono::milliseconds delay)
{
	std::shared_ptr<RepeatFunc> prf{new RepeatFunc(func, delay, milliseconds(0))};

	push_one(prf);
}

void FunctionScheduler::shutdown()
{
	running_ = false;
	{
		std::lock_guard<std::mutex> _l(mutex_);
		functions_.clear();
	}
	if (thread_.joinable()) {
		thread_.join();
	}
}


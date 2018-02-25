//
// Created by xu on 18-2-2.
//

#include "FunctionScheduler.h"


using namespace std::chrono;

struct RepeatFunc : public std::enable_shared_from_this<RepeatFunc> {
	std::function<void(void)> cb;
	std::chrono::milliseconds delay;
	std::chrono::milliseconds interval;
	std::string name;
	int count;
	bool once;

	// maybe more than one function has the same `next run time`
	std::shared_ptr<RepeatFunc> next;

public:
	RepeatFunc(std::function<void()> f, std::chrono::milliseconds d, std::chrono::milliseconds i, std::string n)
		: cb(f), delay(d), interval(i), name(n), count(0) {
		once = (milliseconds(0) == interval);
//		printf("RepeatFunc_%p born!\n", this);
	}

	RepeatFunc(std::function<void()> f, std::chrono::milliseconds d, std::string n)
		: RepeatFunc(f, d, milliseconds(0), n) {
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
	: thread_(),
      running_(false),
      functions_(),
      mutex_(),
      condition_()
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

	// blocking wait to the time point
	std::this_thread::sleep_until(it->first);

	auto prf = it->second;
	if (prf) {
		// fire the callback
		prf->run();
	}

	// remove the front element
	functions_.erase(it);
	if (prf && prf->name.size()) {
		name_index_.erase(prf->name);  // unregister a schedule name
	}
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
                                 std::chrono::milliseconds interval,
                                 std::string name)
{
	std::shared_ptr<RepeatFunc> prf{new RepeatFunc(func, delay, interval, name)};

	if (name.size()) {  // register a schedule name
		std::lock_guard<std::mutex> _l(mutex_);
		name_index_.insert(std::make_pair(name, prf));
	}

	push_one(prf);
}

void FunctionScheduler::schedule(std::function<void(void)> func, std::chrono::milliseconds delay, std::string name)
{
	std::shared_ptr<RepeatFunc> prf{new RepeatFunc(func, delay, milliseconds(0), name)};

	if (name.size()) {  // register a schedule name
		std::lock_guard<std::mutex> _l(mutex_);
		name_index_.insert(std::make_pair(name, prf));
	}

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

void FunctionScheduler::start()
{
	running_ = true;
	thread_ = std::thread(&FunctionScheduler::run, this);
}

bool FunctionScheduler::has_schedule(std::string name) const
{
	std::lock_guard<std::mutex> _l(mutex_);
	return name_index_.count(name) > 0;
}


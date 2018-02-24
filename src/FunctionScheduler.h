//
// Created by xu on 18-2-2.
//

#ifndef AUTODEPLOY_FUNCTIONSCHEDULER_H
#define AUTODEPLOY_FUNCTIONSCHEDULER_H

#include <map>
#include <mutex>
#include <memory>
#include <atomic>
#include <chrono>
#include <thread>
#include <functional>
#include <condition_variable>

class RepeatFunc;

class FunctionScheduler
{
public:
	FunctionScheduler();

	~FunctionScheduler();

	void schedule(std::function<void(void)> func,
	              std::chrono::milliseconds delay,
	              std::chrono::milliseconds interval);

	void schedule(std::function<void(void)> func,
	              std::chrono::milliseconds delay);

	void shutdown();
private:
	void run();
	std::shared_ptr<RepeatFunc> call_one();
	void push_one(std::shared_ptr<RepeatFunc> prf);

	using time_point_t = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>;

private:
	std::mutex mutex_;
	std::condition_variable condition_;
	std::thread thread_;
	std::atomic<bool> running_;
	std::map<time_point_t, std::shared_ptr<RepeatFunc>> functions_;
};

#endif //AUTODEPLOY_FUNCTIONSCHEDULER_H

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
#include <string>
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
	              std::chrono::milliseconds interval,
	              std::string name = "");

	void schedule(std::function<void(void)> func,
	              std::chrono::milliseconds delay,
	              std::string name = "");

	void start();

	void shutdown();

	bool has_schedule(std::string name) const;

private:
	void run();
	std::shared_ptr<RepeatFunc> call_one();
	void push_one(std::shared_ptr<RepeatFunc> prf);

	using time_point_t = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>;

private:
	std::thread thread_;
	std::atomic<bool> running_;
	std::map<time_point_t, std::shared_ptr<RepeatFunc>> functions_;
	std::map<std::string, std::shared_ptr<RepeatFunc>> name_index_;
	mutable std::mutex mutex_;
	std::condition_variable condition_;
};

#endif //AUTODEPLOY_FUNCTIONSCHEDULER_H

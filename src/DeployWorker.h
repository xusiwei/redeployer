#ifndef _DEPLOY_WORKER_H_
#define _DEPLOY_WORKER_H_

#include "EpollPoller.h"
#include "BlockingQueue.h"
#include "ProcessWatcher.h"
#include "FileSystemWatcher.h"

#include <thread>
#include <vector>
#include <string>

class DeployWorker
{
public:
	typedef std::function<void(void)> Function;

	DeployWorker();

	~DeployWorker();

	void start();

	pid_t deploy(std::vector<std::string> args, std::string path);

	bool undeloy(pid_t pid);

	void stop();

protected:
	void run();

	void ProcessCallback(pid_t pid, const ProcessWatcher::ProcessInfo& info);
	void on_child_exit(pid_t pid, const ProcessWatcher::ProcessInfo& info);

	void FsEventCallback(std::string path, uint32_t mask);
	void on_fs_event(std::string path, uint32_t mask);

private:
	struct Work {
		pid_t pid;
		std::string path;
		std::vector<std::string> args;

		Work() : pid(0), path(), args() {}
		Work(pid_t pi, const std::string& pa, const std::vector<std::string>& a) : pid(pi), path(pa), args(a) {}
		Work(const Work&) = default;
	};

private:
	BlockingQueue<Function> func_queue_;
	FileSystemWatcher fs_watcher_;
	ProcessWatcher process_watcher_;
	EpollPoller poller_;
	std::thread cb_caller_;
	std::thread event_poller_;

	std::mutex mutex_;
	std::map<pid_t, Work> works_;

	std::atomic<bool> started_{false};
	std::atomic<long> redeploy_interval_{1};
};

#endif  // _DEPLOY_WORKER_H_
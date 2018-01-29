#include "DeployWorker.h"

#include <string.h>
#include <stdexcept>

using namespace fsevent;
using namespace std::placeholders;

const static long kMaxRedeployInterval = 64;

DeployWorker::DeployWorker()
	: func_queue_(1024),
	  fs_watcher_(std::bind(&DeployWorker::FsEventCallback, this, _1, _2)),
	  process_watcher_(std::bind(&DeployWorker::ProcessCallback, this, _1, _2))
{
}

DeployWorker::~DeployWorker()
{
	if (started_) stop();
	if (works_.size()) {
		for (auto& e: works_) {
			undeloy(e.first);
		}
	}
	if (event_poller_.joinable()) {
		event_poller_.join();
	}
	if (cb_caller_.joinable()) {
		cb_caller_.join();
	}
}

pid_t DeployWorker::deploy(std::vector<std::string> args, std::string path)
{
	if (args.size() == 0) {
		throw std::invalid_argument("args.size() must > 0");
	}

	pid_t pid = process_watcher_.spwan_process(args);
	fs_watcher_.add_watch(path, ATTRIB | MODIFY);

	std::lock_guard<std::mutex> _l(mutex_);
	works_[pid] = {pid, path, args};

	return pid;
}

bool DeployWorker::undeloy(pid_t pid)
{
	std::lock_guard<std::mutex> _l(mutex_);
	auto it = works_.find(pid);
	if (it != works_.end()) {
		process_watcher_.kill_process(pid);
		fs_watcher_.remove_watch(it->second.path);
		works_.erase(pid);
	}
}

void DeployWorker::start()
{
	poller_.add_fd(fs_watcher_.get_fd(),
		std::bind(&FileSystemWatcher::on_fd_events, &fs_watcher_, _1, _2));
	poller_.add_fd(process_watcher_.get_fd(),
		std::bind(&ProcessWatcher::on_fd_events, &process_watcher_, _1, _2));
	
	cb_caller_ = std::thread(std::bind(&DeployWorker::run, this));
	event_poller_ = std::thread(std::bind(&EpollPoller::loop, &poller_));
	started_ = true;
}

void DeployWorker::run()
{
	Function func;
	while (func = func_queue_.take()) {
		func();
	}
}

void DeployWorker::stop()
{
	Function nop;
	func_queue_.put(nop); // stop cb_caller_
	poller_.stop(); // stop event_poller_
	started_ = false;
}

void DeployWorker::ProcessCallback(pid_t pid, const ProcessWatcher::ProcessInfo& info)
{
	if (!started_) return;
	int status = info.status;
	printf("EVENT [%d] exit with status %x", pid, status);
	if (WIFEXITED(status)) {
		printf(", exit normally, code: %d", WEXITSTATUS(status));
	}
	if (WIFSIGNALED(status)) {
		printf(", terminated by signal: %s, core: %d",
			strsignal(WTERMSIG(status)), WCOREDUMP(status));
	}
	if (WIFSTOPPED(status)) {
		printf(", stoped by signal: %s", strsignal(WSTOPSIG(status)));
	}
	if (WIFCONTINUED(status)) {
		printf(", resumed by SIGCONT");
	}
	printf("\n");
	func_queue_.put(std::bind(&DeployWorker::on_child_exit, this, pid, info));
	// on_child_exit(pid, info);
}

void DeployWorker::FsEventCallback(std::string path, uint32_t mask)
{
	if (!started_) return;
	printf("EVENT [%x] on %s with %x\n", mask, path.c_str(), mask);
	// func_queue_.put(std::bind(&DeployWorker::on_fs_event, this, path, mask));
	on_fs_event(path, mask);
}

void DeployWorker::on_child_exit(pid_t pid, const ProcessWatcher::ProcessInfo& info)
{
	printf("child %d exited, restart it after %lds...\n", pid, redeploy_interval_.load());

	std::string path;
	std::vector<std::string> args;
	{
		std::lock_guard<std::mutex> _l(mutex_);
		auto it = works_.find(pid);
		if (it != works_.end()) {
			args = it->second.args;
			path = it->second.path;
		 	works_.erase(pid);
		}
	}
	if (args.size()) {
		long seconds = redeploy_interval_;
		long twice = seconds*2;
		sleep(seconds);
		deploy(args, path);
		printf("now redeploy_interval_: %ld\n", redeploy_interval_.load());
		if (redeploy_interval_.compare_exchange_strong(seconds, twice)
		 && redeploy_interval_ > kMaxRedeployInterval) {
			redeploy_interval_ = 1;
		}
	}
}

void DeployWorker::on_fs_event(std::string path, uint32_t mask)
{
	printf("file %s updated, kill related processes...\n", path.c_str());
	printf("works_.size(): %zu\n", works_.size());
	redeploy_interval_ = 1;

	std::vector<pid_t> pids;
	{
		std::lock_guard<std::mutex> _l(mutex_);
		for (auto e: works_) {
			auto work = e.second;
			printf("compare(%s, %s)\n", work.path.c_str(), path.c_str());
			if (work.path == path) {
				pids.push_back(work.pid);
			}
		}
	}
	
	for (auto pid: pids) {
		printf("  %s: kill process %d...\n", path.c_str(), pid);
		process_watcher_.kill_process(pid, SIGTERM);
		printf("set redeploy_interval_: %ld\n", redeploy_interval_.load());
	}
}
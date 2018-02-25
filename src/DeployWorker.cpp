#include "DeployWorker.h"
#include "RuntimeError.h"

#include <string.h>
#include <limits.h>
#include <stdexcept>

using namespace fsevent;
using namespace std::placeholders;

const static long kMaxRedeployInterval = 64;
const static long kDelayUnit = 1000; // ms

DeployWorker::DeployWorker()
	: queue_(1024),
	  scheduler_(),
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
	if (poller_thread_.joinable()) {
		poller_thread_.join();
	}
	if (handler_thread_.joinable()) {
		handler_thread_.join();
	}
	scheduler_.shutdown();
}

std::string abspath(std::string path)
{
	char filename[PATH_MAX] = "";
	if (realpath(path.c_str(), filename) == NULL) {
		throw RuntimeError("realpath " + path + " failed!");
	}
	return 	filename;
}

pid_t DeployWorker::deploy(std::vector<std::string> args, std::string path)
{
	if (args.size() == 0) {
		throw std::invalid_argument("args.size() must > 0");
	}
	path = abspath(path);
	args[0] = abspath(args[0]);

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
	
	handler_thread_ = std::thread(std::bind(&DeployWorker::run, this));
	poller_thread_ = std::thread(std::bind(&EpollPoller::loop, &poller_));
	started_ = true;
	scheduler_.start();
}

void DeployWorker::run()
{
	Function func;
	while (func = queue_.take()) {
		func();
	}
}

void DeployWorker::stop()
{
	Function nop;
	queue_.put(nop); // stop cb_caller_
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
	queue_.put(std::bind(&DeployWorker::on_child_exit, this, pid, info));
}

void DeployWorker::FsEventCallback(std::string path, uint32_t mask)
{
	if (!started_) return;
	printf("EVENT [%x] on %s with %x\n", mask, path.c_str(), mask);
	queue_.put(std::bind(&DeployWorker::on_fs_event, this, path, mask));
}

bool DeployWorker::redeploy(pid_t pid)
{
	std::string path;
	std::vector<std::string> args;

	// clean this process info from the map
	{
		std::lock_guard<std::mutex> _l(mutex_);
		auto it = works_.find(pid);
		if (it != works_.end()) {
			args = it->second.args;
			path = it->second.path;
			works_.erase(pid);
		}
	}

	// schedule a re-deploy work
	std::string task_name = "redeploy " + path;
	if (args.size() && !scheduler_.has_schedule(task_name)) {
		auto ms = std::chrono::milliseconds(next_redeploy_delay() * 1000);
		printf("schedule a re-deploy task %s...\n", task_name.c_str());
		scheduler_.schedule(std::bind(&DeployWorker::deploy, this, args, path), ms, task_name);
	}
}

void DeployWorker::on_child_exit(pid_t pid, const ProcessWatcher::ProcessInfo& info)
{
	printf("child %d exited, restart it after %lds...\n", pid, redeploy_interval_.load());

	redeploy(pid);
}

void DeployWorker::on_fs_event(std::string path, uint32_t mask)
{
	printf("file %s updated, works_.size(): %zu...\n", path.c_str(), works_.size());

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
		printf("  %s: un-deploy process %d...\n", path.c_str(), pid);
		undeloy(pid);
		reset_redeploy_delay();
		printf("redeploy_interval_: %ld\n", redeploy_interval_.load());
		redeploy(pid);
	}
}

long DeployWorker::next_redeploy_delay()
{
	long current = redeploy_interval_;
	long twice = current * 2;
	printf("next redeploy delay: %ld\n", redeploy_interval_.load());
	if (redeploy_interval_.compare_exchange_strong(current, twice)
		&& redeploy_interval_ > kMaxRedeployInterval) {
		redeploy_interval_ = 1;
	}
	return current;
}

void DeployWorker::reset_redeploy_delay()
{
	redeploy_interval_ = 1;
}

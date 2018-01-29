#ifndef _PROCESS_WATCHER_H_
#define _PROCESS_WATCHER_H_

#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <map>
#include <vector>
#include <string>
#include <mutex>
#include <functional>

class ProcessWatcher
{
public:
	struct ProcessInfo {
		int status;
		siginfo_t siginfo;
		struct rusage rusage;
		std::vector<std::string> args;

		ProcessInfo() {}
		ProcessInfo(const ProcessInfo&) = default;
		ProcessInfo& operator=(const ProcessInfo&) = default;
	};

	typedef std::function<void(pid_t, const ProcessInfo&)> Callback;

	ProcessWatcher(Callback cb);

	~ProcessWatcher();

	pid_t spwan_process(std::vector<std::string> args);

	bool kill_process(pid_t pid, int sig = SIGTERM);

	void on_fd_events(int fd, short events);

	int get_fd();

	void run();

private:
	int sigfd_;
	Callback on_exit_;
	std::map<pid_t, ProcessInfo> infos_;
	std::mutex mutex_;
};

#endif  // _PROCESS_WATCHER_H_
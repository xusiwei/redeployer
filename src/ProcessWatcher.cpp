#include "ProcessWatcher.h"
#include "RuntimeError.h"

#include <stdio.h>
#include <limits.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>


ProcessWatcher::ProcessWatcher(Callback cb)
{
	callback_ = cb;

	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);

 	// block default signal handler
	if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) {
		throw RuntimeError("sigprocmask failed");
	}

	sigfd_ = signalfd(-1, &mask, SFD_CLOEXEC | SFD_NONBLOCK);
	if (sigfd_ < 0) {
		throw RuntimeError("signalfd failed");
	}
}

ProcessWatcher::~ProcessWatcher()
{
	if (sigfd_ >= 0) {
		close(sigfd_);
	}
}

pid_t ProcessWatcher::spwan_process(std::vector<std::string> args)
{
	pid_t pid = fork();
	if (pid < 0) {
		perror("fork failed");
		return pid;
	} else if (pid > 0) {  // parent
		std::lock_guard<std::mutex> _l(mutex_);
		ProcessInfo info;
		info.args = args;
		infos_[pid] = info;
		printf("process %d spawned\n", pid);
	} else {  // child
		for (int i = 0; i < args.size(); i++) {
			printf("  args[%d]: %s\n", i, args[i].c_str());
		}

		std::vector<char*> cargs;
		for (int i = 0; i < args.size(); i++) {
			cargs.push_back(const_cast<char*>(args[i].c_str()));
		}

		printf("prepare to exec %s... in %d\n", cargs[0], getpid());
		execve(cargs[0], &cargs[0], environ);
		throw RuntimeError("exec failed! " + args[0]);
		return -1; // NEVER RUNS HERE
	}
	return pid;
}

bool ProcessWatcher::kill_process(pid_t pid, int sig)
{
	std::lock_guard<std::mutex> _l(mutex_);
	auto it = infos_.find(pid);
	if (it != infos_.end()) {
		if (kill(pid, sig) < 0) {
			throw RuntimeError("kill failed");
		}
		return true;
	}
	return false;
}

void ProcessWatcher::run()
{
	int flags = fcntl(sigfd_, F_GETFL);
	if (flags & O_NONBLOCK) {
		flags &= ~O_NONBLOCK;
		fcntl(sigfd_, F_SETFL, flags);
	}
	for (;;) {
		on_fd_events(sigfd_, 0);
	}
}

void ProcessWatcher::on_fd_events(int fd, short events)
{
	struct signalfd_siginfo ssi;
	long nbytes = read(sigfd_, &ssi, sizeof(ssi));
	if (nbytes < 0) {
		throw RuntimeError("read signalfd failed!");
	}
	pid_t pid = ssi.ssi_pid;
	printf("got signal `%s` from %d\n", strsignal(ssi.ssi_signo), pid);

	int status = 0;
	struct rusage rus;
	memset(&rus, 0, sizeof(rus));
	if (wait4(pid, &status, WNOHANG, &rus) < 0) {
		throw RuntimeError("wait failed");
	}

	// make sure call `callback_` without effect of `mutex_`
	auto info = get_info(pid);
	if (info) {
		callback_(pid, *info);
	}
}

int ProcessWatcher::get_fd()
{
	return sigfd_;
}

ProcessWatcher::ProcessInfo* ProcessWatcher::get_info(pid_t pid)
{
	std::lock_guard<std::mutex> _l(mutex_);
	auto it = infos_.find(pid);
	if (it != infos_.end()) {
		return &it->second;
	}
	return nullptr;
}

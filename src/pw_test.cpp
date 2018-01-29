#include "EpollPoller.h"
#include "ProcessWatcher.h"

#include <string.h>
#include <signal.h>
#include <thread>

using namespace std::placeholders;

int main(int argc, char* argv[])
{
	ProcessWatcher watcher([](pid_t pid, const ProcessWatcher::ProcessInfo& info) {
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
	});

	watcher.spwan_process({"./hi.sh"});

	EpollPoller poller;
	poller.add_fd(watcher.get_fd(), std::bind(&ProcessWatcher::on_fd_events, &watcher, _1, _2));

	std::thread worker(std::bind(&EpollPoller::loop, &poller));
	sleep(5);

	poller.stop();
	worker.join();
	return 0;
}
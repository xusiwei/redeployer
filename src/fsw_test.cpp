#include "EpollPoller.h"
#include "FileSystemWatcher.h"

#include <unistd.h>
#include <thread>

using namespace fsevent;
using namespace std::placeholders;

int main(int argc, char* argv[])
{
	FileSystemWatcher fsWatcher([](std::string path, uint32_t mask){
		printf("EVENT [%x] on %s\n", mask, path.c_str());
	});
	fsWatcher.add_watch("./", CREATE | ATTRIB | MODIFY | DELETE | RENAME_FROM | RENAME_TO);

	EpollPoller poller;
	poller.add_fd(fsWatcher.get_fd(), std::bind(&FileSystemWatcher::on_fd_events, &fsWatcher, _1, _2));

	std::thread worker(std::bind(&EpollPoller::loop, &poller));
	sleep(20);

	poller.stop();
	worker.join();

	return 0;
}
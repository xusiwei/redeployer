#include "EpollPoller.h"
#include "RuntimeError.h"

#include <vector>
#include <unistd.h>
#include <sys/epoll.h>

EpollPoller::EpollPoller()
{
	epfd_ = epoll_create1(EPOLL_CLOEXEC);
}

EpollPoller::~EpollPoller()
{
	if (epfd_ >= 0) {
		close(epfd_);
	}
}

void EpollPoller::stop()
{
	stop_ = true;
}

void EpollPoller::loop()
{
	std::vector<struct epoll_event> events(256);
	while (!stop_) {
		// printf("waiting ready events...\n");
		int nready = epoll_wait(epfd_, &events[0], events.size(), 1000);
		// printf("nready: %d\n", nready);
		if (nready < 0) {
			throw RuntimeError("epoll_wait failed");
		}
		for (int i = 0; i < nready; i++) {
			int fd = events[i].data.fd;
			auto cb = get_cb(fd);
			if (cb) {
				cb(fd, events[i].events);
			}
		}
	}
}

void EpollPoller::add_fd(int fd, Callback cb)
{
	struct epoll_event event;
	event.events = EPOLLIN | EPOLLOUT;
	event.data.fd = fd;
	if (epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &event) < 0) {
		throw RuntimeError("EPOLL_CTL_ADD failed");
	}

	std::lock_guard<std::mutex> _l(mutex_);
	cbs_[fd] = cb;
}

void EpollPoller::remove_fd(int fd)
{
	if (epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, NULL) < 0) {
		throw RuntimeError("EPOLL_CTL_DEL failed");
	}
	std::lock_guard<std::mutex> _l(mutex_);
	cbs_.erase(fd);
}

EpollPoller::Callback EpollPoller::get_cb(int fd)
{
	std::lock_guard<std::mutex> _l(mutex_);
	auto it = cbs_.find(fd);
	if (it != cbs_.end()) {
		return it->second;
	}
	return Callback(nullptr);
}
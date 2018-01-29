#ifndef _EPOLL_POLLER_H_
#define _EPOLL_POLLER_H_

#include <atomic>
#include <mutex>
#include <map>
#include <functional>

class EpollPoller
{
public:
	typedef std::function<void(int, short)> Callback;

	EpollPoller();

	~EpollPoller();

	void add_fd(int fd, Callback cb);

	void remove_fd(int fd);

	Callback get_cb(int fd);

	void loop();

	void stop();

private:
	int epfd_;
	std::map<int, Callback> cbs_;
	std::mutex mutex_;
	std::atomic<bool> stop_{false};
};

#endif  // _EPOLL_POLLER_H_
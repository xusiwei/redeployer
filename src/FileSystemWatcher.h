#ifndef _FILE_SYSTEM_WATCHER_H_
#define _FILE_SYSTEM_WATCHER_H_

#include <map>
#include <mutex>
#include <string>
#include <functional>

namespace fsevent {
	enum {
		ACCESS      = 0x01,
		ATTRIB      = 0x02,
		CREATE      = 0x04,
		DELETE      = 0x08,
		MODIFY      = 0x10,
		RENAME_TO   = 0x20,
		RENAME_FROM = 0x40,
		OPEN        = 0x80,
		CLOSE       = 0x100,
		ALL_EVENTS  = 0x1FF
	};
};

class FileSystemWatcher
{
public:
	typedef std::function<void(std::string, uint32_t)> Callback;

	FileSystemWatcher(Callback cb);
	
	~FileSystemWatcher();

	void add_watch(std::string path, uint32_t mask);

	void add_watch(std::string path, uint32_t mask, Callback cb);

	bool remove_watch(std::string path);

	void on_fd_events(int fd, short events);

	int get_fd();

	void run();

private:
	struct WatchInfo
	{
		std::string path;
		uint32_t mask{0};
		Callback cb;
		WatchInfo() : path(), mask(0), cb() {}
		WatchInfo(std::string p, uint32_t m, Callback c) : path(p), mask(m), cb(c) {}
		WatchInfo(const WatchInfo&) = default;
		WatchInfo& operator=(const WatchInfo&) = default;
	};
	int get_wd(std::string path);
	WatchInfo* get_info(int wd);

private:
	int fd_;
	Callback default_cb_;
	std::mutex mutex_;
	std::map<std::string, int> wds_;
	std::map<int, WatchInfo> infos_;
};

#endif  // _FILE_SYSTEM_WATCHER_H_



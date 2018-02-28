#include "FileSystemWatcher.h"
#include "RuntimeError.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include <linux/limits.h>
#include <vector>
#include <stdexcept>

FileSystemWatcher::FileSystemWatcher(Callback cb)
	: default_cb_(cb)
{
	fd_ = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
	if (fd_ < 0) {
		throw RuntimeError("inotify_init failed:");
	}
}

FileSystemWatcher::~FileSystemWatcher()
{
	if (fd_ >= 0) {
		close(fd_);
	}
}

void FileSystemWatcher::add_watch(std::string path, uint32_t mask)
{
	add_watch(path, mask, default_cb_);
}

bool FileSystemWatcher::remove_watch(std::string path)
{
	std::lock_guard<std::mutex> _l(mutex_);
	auto it = wds_.find(path);
	if (it != wds_.end()) { // found
		int wd = it->second;
		infos_.erase(wd);
		if (inotify_rm_watch(fd_, wd) < 0) {
			throw RuntimeError("inotify_rm_watch failed: ");
		}
		return true;
	}
	return false;
}

static std::map<uint32_t, uint32_t> mask_map {
	{fsevent::ACCESS, IN_ACCESS},
	{fsevent::ATTRIB, IN_ATTRIB},
	{fsevent::CREATE, IN_CREATE},
	{fsevent::DELETE, IN_DELETE | IN_DELETE_SELF},
	{fsevent::MODIFY, IN_MODIFY},
	{fsevent::RENAME_FROM, IN_MOVE_SELF | IN_MOVED_FROM},
	{fsevent::RENAME_TO, IN_MOVE_SELF | IN_MOVED_TO},
	{fsevent::OPEN, IN_OPEN},
	{fsevent::CLOSE, IN_CLOSE_WRITE | IN_CLOSE_NOWRITE}
};

// event mask to inotfy mask
static uint32_t emask_to_imask(uint32_t mask)
{
	uint32_t in_mask = 0; // inotify mask
	for (auto& e: mask_map) {
		if (mask & e.first) {
			in_mask |= e.second;
		}
	}
	return in_mask;	
}

// inotify mask to event mask
static uint32_t imask_to_emask(uint32_t imask)
{
	uint32_t emask = 0;
	for (auto& e: mask_map) {
		if (imask & e.second) {
			emask |= e.first;
		}
	}
	return emask;
}

void FileSystemWatcher::add_watch(std::string path, uint32_t mask, Callback cb)
{
	uint32_t in_mask = emask_to_imask(mask);
	
	remove_watch(path);

	int wd = inotify_add_watch(fd_, path.c_str(), in_mask);
	if (wd < 0) {
		throw RuntimeError("inotify_add_watch failed:");
	}

	std::lock_guard<std::mutex> _l(mutex_);
	wds_[path] = wd;
	infos_[wd] = WatchInfo(path, mask, cb);
}

int FileSystemWatcher::get_wd(std::string path)
{
	std::lock_guard<std::mutex> _l(mutex_);
	auto it = wds_.find(path);
	if (it != wds_.end()) {
		return it->second;
	}
	return -1;
}

FileSystemWatcher::WatchInfo* FileSystemWatcher::get_info(int wd)
{
	std::lock_guard<std::mutex> _l(mutex_);
	auto it = infos_.find(wd);
	if (it != infos_.end()) {
		return &it->second;
	}
	return nullptr;
}

std::string path_join(std::string dir, std::string name)
{
	printf("path_join(%s, %s)\n", dir.c_str(), name.c_str());
	if (dir.empty() || name.empty()) {
		return dir + name;
	}
	if (dir.size() && dir.back() == '/') {
		dir.pop_back();
	}
	if (name.size() && name.substr(0, 2) == "./") {
		name = name.substr(2);
	}
	return dir + "/" + name;
}

void FileSystemWatcher::run()
{
	int flags = fcntl(fd_, F_GETFL);
	if (flags & O_NONBLOCK) {
		flags &= ~O_NONBLOCK;
		fcntl(fd_, F_SETFL, flags);
	}
	for (;;) {
		on_fd_events(fd_, 0);
	}
}
	
void FileSystemWatcher::on_fd_events(int fd, short events)
{
	char buffer[(sizeof(struct inotify_event) + PATH_MAX + 1)*4];
	long nbytes = read(fd_, &buffer[0], sizeof(buffer));
	if (nbytes < 0) {  // maybe blocking here...
		throw RuntimeError("read failed: ");
	}
	if (!nbytes) return;
	// printf("nbytes: %ld\n", nbytes);
	for (char* p = &buffer[0]; (p - &buffer[0]) < nbytes; ) {
		auto evt = (struct inotify_event*) p;
		// printf("raw event %x on '%s' with %d %d\n", evt->mask, evt->name, evt->len, evt->cookie);
		auto info = get_info(evt->wd);
		int mask = imask_to_emask(evt->mask);
		if (info && mask) {
			std::string full = path_join(info->path, evt->name);
			info->cb(full, mask);
		}
		p = evt->name + evt->len;  // move to next event
	}
}

int FileSystemWatcher::get_fd()
{
	return fd_;
}
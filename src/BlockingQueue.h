#ifndef _BLOCKING_QUQUE_H_
#define _BLOCKING_QUQUE_H_

#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>

template <typename T>
class BlockingQueue
{
public:
	BlockingQueue(size_t cap)
		: capacity_(cap) {}

	void put(const T& x) {
		std::unique_lock<std::mutex> _lock(mutex_);
		while (queue_.size() > capacity_) {
			slot_cond_.wait(_lock);
		}
		queue_.push(x);
		item_cond_.notify_one();
	}

	T take() {
		std::unique_lock<std::mutex> _lock(mutex_);
		while (queue_.empty()) {
			item_cond_.wait(_lock);
		}
		T x = queue_.front();
		queue_.pop();
		slot_cond_.notify_one();
		return x;
	}

	size_t size() {
		std::unique_lock<std::mutex> _lock(mutex_);
		return queue_.size();
	}

	bool peek(T* px) {
		if (!px) return false;
		std::unique_lock<std::mutex> _lock(mutex_);
		if (queue_.empty()) return false;
		*px = queue_.front();
		queue_.pop();
		slot_cond_.notify_one();
		return true;
	}

	size_t peekTo(std::vector<T>* pv) {
		if (!pv) return 0;
		std::unique_lock<std::mutex> _lock(mutex_);
		if (queue_.empty()) return 0;
		while (queue_.size()) {
			pv->push_back(queue_.front());
			queue_.pop();
		}
		slot_cond_.notify_all();
	}

	size_t peekTo(std::vector<T>* pv, size_t m) {
		if (!pv) return 0;
		std::unique_lock<std::mutex> _lock(mutex_);
		if (queue_.empty()) return 0;
		size_t n = std::min(queue_.size(), m);
		while (n--) {
			pv->push_back(queue_.front());
			queue_.pop();
		}
		slot_cond_.notify_all();
	}	

private:
	size_t capacity_;
	std::queue<T> queue_;
	std::mutex mutex_;
	std::condition_variable item_cond_;
	std::condition_variable slot_cond_;
};



#endif  // _BLOCKING_QUQUE_H_
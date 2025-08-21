#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <iostream>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <vector>
#include <set>
#include <condition_variable>

#include "common/macros.h"

namespace buzzdb {

// Lock Manager definitions
enum class LockMode {
    SHARED,
    EXCLUSIVE
};

struct Lock {
    uint64_t txn_id;
    LockMode mode;
    Lock(uint64_t id, LockMode m) : txn_id(id), mode(m) {}
};

class FrameLockManager {
public:
    explicit FrameLockManager(uint64_t page_id) : page_id_(page_id) {}

    bool can_grant_lock(uint64_t txn_id, LockMode mode);
    bool grant_lock(uint64_t txn_id, LockMode mode, uint64_t timeout_ms);
    void release_lock(uint64_t txn_id);
    bool has_lock(uint64_t txn_id);
    bool has_exclusive_lock(uint64_t txn_id);
    bool has_shared_lock(uint64_t txn_id);
    bool can_upgrade_lock(uint64_t txn_id);

private:
    uint64_t page_id_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<Lock> granted_locks_;
    std::vector<Lock> waiting_locks_;
};

class LockManager {
public:
    FrameLockManager& get_frame_lock_manager(uint64_t page_id);
    bool has_cycle(uint64_t start_txn, uint64_t current_txn, std::set<uint64_t>& visited);
    bool check_deadlock(uint64_t txn_id, uint64_t waiting_for);
    bool acquire_lock(uint64_t txn_id, uint64_t page_id, LockMode mode);
    bool has_exclusive_locks_on_page(uint64_t page_id, uint64_t txn_id);
    void release_lock(uint64_t txn_id, uint64_t page_id);
    void release_all_locks(uint64_t txn_id);
    bool has_lock(uint64_t txn_id, uint64_t page_id);
    std::set<uint64_t> get_page_ids_for_txn(uint64_t txn_id);

private:
    std::mutex mutex_;
    std::unordered_map<uint64_t, std::unique_ptr<FrameLockManager>> page_locks_;
    std::unordered_map<uint64_t, std::set<uint64_t>> txn_locks_;
    std::unordered_map<uint64_t, std::set<uint64_t>> waiting_graph_;
};

// Buffer Manager definitions
class BufferFrame {
 private:
	friend class BufferManager;

	uint64_t page_id;
	uint64_t frame_id;
	std::vector<char> data;

	bool dirty;
	bool exclusive;
	std::thread::id exclusive_thread_id;

 public:
	/// Returns a pointer to this page's data.
	char *get_data();

	BufferFrame();

	BufferFrame(const BufferFrame &other);

	BufferFrame &operator=(BufferFrame other);

	void mark_dirty() {dirty = true;}
};

class buffer_full_error : public std::exception {
 public:
	const char *what() const noexcept override { return "buffer is full"; }
};

class transaction_abort_error : public std::exception {
 public:
	const char *what() const noexcept override { return "transaction aborted"; }
};

class BufferManager {
 public:
	/// Constructor.
	/// @param[in] page_size  Size in bytes that all pages will have.
	/// @param[in] page_count Maximum number of pages that should reside in
	//                        memory at the same time.
	BufferManager(size_t page_size, size_t page_count);

	/// Destructor. Writes all dirty pages to disk.
	~BufferManager();

	BufferFrame &fix_page(uint64_t txn_id, uint64_t page_id, bool exclusive);

	void unfix_page(uint64_t txn_id, BufferFrame& page, bool is_dirty);

	/// Returns the segment id for a given page id which is contained in the 16
	/// most significant bits of the page id.
	static constexpr uint16_t get_segment_id(uint64_t page_id) {
		return page_id >> 48;
	}

	/// Returns the page id within its segment for a given page id. This
	/// corresponds to the 48 least significant bits of the page id.
	static constexpr uint64_t get_segment_page_id(uint64_t page_id) {
		return page_id & ((1ull << 48) - 1);
	}

	/// Returns the overall page id associated with a segment id and
	/// a given segment page id.
	static uint64_t get_overall_page_id(uint16_t segment_id, uint64_t segment_page_id) {
		return (static_cast<uint64_t>(segment_id) << 48) | segment_page_id;
	}

	/// Print page id
	static std::string print_page_id(uint64_t page_id) {
		if (page_id == INVALID_NODE_ID) {
			return "INVALID";
		} else {
			auto segment_id = BufferManager::get_segment_id(page_id);
			auto segment_page_id = BufferManager::get_segment_page_id(page_id);
			return "( " + std::to_string(segment_id) + " " +
						 std::to_string(segment_page_id) + " )";
		}
	}

	size_t get_page_size() { return page_size_; }

	void flush_all_pages();
	void flush_page(uint64_t page_id);
	void discard_page(uint64_t page_id);
	void discard_all_pages();
	void flush_pages(uint64_t txn_id);
	void discard_pages(uint64_t txn_id);
	void transaction_complete(uint64_t txn_id);
	void transaction_abort(uint64_t txn_id);

 private:
	uint64_t capacity_;
	size_t page_size_;
	std::vector<std::unique_ptr<BufferFrame>> pool_;

	mutable std::mutex file_use_mutex_;
	mutable std::mutex pool_mutex_;
	std::deque<size_t> free_frames_;
	std::unordered_map<uint64_t, size_t> page_table_;
	std::unordered_map<uint64_t, std::set<uint64_t>> txn_pages_;
	LockManager lock_manager_;

	size_t get_frame_id(uint64_t page_id);
	size_t get_free_frame();
	void read_frame(uint64_t frame_id);
	void write_frame(uint64_t frame_id);
};

}  // namespace buzzdb

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

namespace buzzdb {

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

} // namespace buzzdb 
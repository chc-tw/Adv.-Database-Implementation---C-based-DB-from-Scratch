class FrameLockManager {
private:
    enum class LockMode { SHARED, EXCLUSIVE };
    
    struct Lock {
        uint64_t txn_id;
        LockMode mode;
        
        Lock(uint64_t id, LockMode m) : txn_id(id), mode(m) {}
    };
    
    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<Lock> granted_locks_;
    std::vector<Lock> waiting_locks_;
    const int timeout_ms = 1000; // 1 second timeout to avoid infinite waiting
    
public:
    bool grant_lock(uint64_t txn_id, bool exclusive);
    void release_lock(uint64_t txn_id);
    bool has_lock(uint64_t txn_id);
    bool is_exclusive(uint64_t txn_id);
}; 
#include <cassert>
#include <iostream>
#include <string>
#include <algorithm>

#include "buffer/buffer_manager.h"
#include "common/macros.h"
#include "storage/file.h"
#include <chrono>
#include <ctime> 

uint64_t wake_timeout_ = 100;
uint64_t timeout_ = 2000; // 2 seconds timeout for deadlock detection

namespace buzzdb {

char* BufferFrame::get_data() { return data.data(); }

BufferFrame::BufferFrame()
    : page_id(INVALID_PAGE_ID),
      frame_id(INVALID_FRAME_ID),
      dirty(false),
      exclusive(false) {}

BufferFrame::BufferFrame(const BufferFrame& other)
    : page_id(other.page_id),
      frame_id(other.frame_id),
      data(other.data),
      dirty(other.dirty),
      exclusive(other.exclusive) {}

BufferFrame& BufferFrame::operator=(BufferFrame other) {
  std::swap(this->page_id, other.page_id);
  std::swap(this->frame_id, other.frame_id);
  std::swap(this->data, other.data);
  std::swap(this->dirty, other.dirty);
  std::swap(this->exclusive, other.exclusive);
  return *this;
}

// FrameLockManager implementation
bool FrameLockManager::can_grant_lock(uint64_t txn_id, LockMode mode) {
    // If transaction already has a lock, check compatibility
    for (const auto& lock : granted_locks_) {
        if (lock.txn_id == txn_id) {
            // Already has the lock with same or higher mode
            if (mode == LockMode::SHARED || lock.mode == LockMode::EXCLUSIVE) {
                return true;
            }
            // Wants to upgrade from shared to exclusive - only if it's the only lock holder
            return granted_locks_.size() == 1;
        }
    }
    
    // No locks granted yet, can grant any lock
    if (granted_locks_.empty()) {
        return true;
    }
    
    // Shared lock can be granted if no exclusive locks exist
    if (mode == LockMode::SHARED) {
        for (const auto& lock : granted_locks_) {
            if (lock.mode == LockMode::EXCLUSIVE) {
                return false;  // Can't get a shared lock if an exclusive lock exists
            }
        }
        return true;  // No exclusive locks, can grant a shared lock
    }
    
    // Exclusive lock can only be granted if no other locks exist
    return false;
}

bool FrameLockManager::grant_lock(uint64_t txn_id, LockMode mode, uint64_t timeout_ms) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // Check if transaction already has the lock
    for (auto& existing_lock : granted_locks_) {
        if (existing_lock.txn_id == txn_id) {
            // Already has the lock in appropriate mode
            if (existing_lock.mode == mode || 
                (existing_lock.mode == LockMode::EXCLUSIVE && mode == LockMode::SHARED)) {
                return true;
            }
            
            // Upgrade from shared to exclusive - only if it's the only lock holder
            if (existing_lock.mode == LockMode::SHARED && mode == LockMode::EXCLUSIVE) {
                if (granted_locks_.size() == 1) {
                    existing_lock.mode = LockMode::EXCLUSIVE;
                    return true;
                }
                
                // Can't upgrade now, will need to wait - add a timeout
                auto pred = [this, txn_id]() {
                    return granted_locks_.size() == 1 && granted_locks_[0].txn_id == txn_id;
                };
                
                auto now = std::chrono::system_clock::now();
                if (!cv_.wait_until(lock, now + std::chrono::milliseconds(timeout_ms), pred)) {
                    // Timeout occurred
                    return false;
                }
                
                // Successfully waited for upgrade
                existing_lock.mode = LockMode::EXCLUSIVE;
                return true;
            }
        }
    }
    
    // Check if the lock can be granted immediately
    bool can_grant = true;
    
    if (mode == LockMode::EXCLUSIVE) {
        // For exclusive lock, there should be no other locks
        can_grant = granted_locks_.empty();
    } else {
        // For shared lock, there should be no exclusive locks
        for (const auto& existing_lock : granted_locks_) {
            if (existing_lock.mode == LockMode::EXCLUSIVE && existing_lock.txn_id != txn_id) {
                can_grant = false;
                break;
            }
        }
    }
    
    if (can_grant) {
        // Make sure the transaction doesn't already have a lock (would be a duplicate)
        for (const auto& existing_lock : granted_locks_) {
            if (existing_lock.txn_id == txn_id) {
                return true;  // Already has a lock, don't add another
            }
        }
        
        granted_locks_.emplace_back(txn_id, mode);
        return true;
    }
    
    // Add to waiting queue with timestamp to implement deadlock prevention
    waiting_locks_.emplace_back(txn_id, mode);
    
    // Wait for the lock with timeout
    auto pred = [this, txn_id, mode]() {
        if (mode == LockMode::EXCLUSIVE) {
            // For exclusive lock, can be granted if no other locks or only this txn's lock
            for (const auto& lock : granted_locks_) {
                if (lock.txn_id != txn_id) {
                    return false;
                }
            }
            return true;
        } else {
            // For shared lock, can be granted if no exclusive locks from other txns
            for (const auto& lock : granted_locks_) {
                if (lock.mode == LockMode::EXCLUSIVE && lock.txn_id != txn_id) {
                    return false;
                }
            }
            return true;
        }
    };
    
    auto now = std::chrono::system_clock::now();
    bool result = cv_.wait_until(lock, now + std::chrono::milliseconds(timeout_ms), pred);
    
    // Always remove from waiting queue
    auto it = std::find_if(waiting_locks_.begin(), waiting_locks_.end(), 
                         [txn_id](const Lock& l) { return l.txn_id == txn_id; });
    if (it != waiting_locks_.end()) {
        waiting_locks_.erase(it);
    }
    
    // If timed out, return false
    if (!result) {
        return false;
    }
    
    // Check again if transaction already has a lock (would be a duplicate)
    for (const auto& existing_lock : granted_locks_) {
        if (existing_lock.txn_id == txn_id) {
            return true;  // Already has a lock, don't add another
        }
    }
    
    // Grant the lock
    granted_locks_.emplace_back(txn_id, mode);
    return true;
}

void FrameLockManager::release_lock(uint64_t txn_id) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // Remove the lock from granted locks
    for (auto it = granted_locks_.begin(); it != granted_locks_.end(); ) {
        if (it->txn_id == txn_id) {
            it = granted_locks_.erase(it);
        } else {
            ++it;
        }
    }
    
    // Notify waiting transactions
    cv_.notify_all();
}

bool FrameLockManager::has_lock(uint64_t txn_id) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    for (const auto& granted_lock : granted_locks_) {
        if (granted_lock.txn_id == txn_id) {
            return true;
        }
    }
    
    return false;
}

bool FrameLockManager::has_exclusive_lock(uint64_t txn_id) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    for (const auto& granted_lock : granted_locks_) {
        if (granted_lock.txn_id == txn_id && granted_lock.mode == LockMode::EXCLUSIVE) {
            return true;
        }
    }
    
    return false;
}

bool FrameLockManager::has_shared_lock(uint64_t txn_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& granted_lock : granted_locks_) {
        if (granted_lock.txn_id == txn_id && granted_lock.mode == LockMode::SHARED) {
            return true;
        }
    }
    
    return false;
}

bool FrameLockManager::can_upgrade_lock(uint64_t txn_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if the transaction has a shared lock
    bool has_shared = false;
    for (const auto& granted_lock : granted_locks_) {
        if (granted_lock.txn_id == txn_id && granted_lock.mode == LockMode::SHARED) {
            has_shared = true;
            break;
        }
    }
    
    if (!has_shared) {
        return false;  // No shared lock to upgrade
    }
    
    // Can upgrade only if it's the only lock holder
    return granted_locks_.size() == 1;
}

// LockManager implementation
FrameLockManager& LockManager::get_frame_lock_manager(uint64_t page_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = page_locks_.find(page_id);
    if (it == page_locks_.end()) {
        // Create new frame lock manager for this page
        auto result = page_locks_.emplace(page_id, std::make_unique<FrameLockManager>(page_id));
        return *(result.first->second);
    }
    
    return *(it->second);
}

bool LockManager::has_cycle(uint64_t start_txn, uint64_t current_txn, std::set<uint64_t>& visited) {
    // If we've seen this transaction before in this path (but not the start of our path), we found a cycle
    if (current_txn == start_txn && !visited.empty()) {
        return true;
    }
    
    // If we've already visited this transaction in this path, skip it to avoid false cycles
    if (visited.find(current_txn) != visited.end()) {
        return false;
    }
    
    // Mark this transaction as visited in this path
    visited.insert(current_txn);
    
    // Check all transactions this one is waiting for
    auto it = waiting_graph_.find(current_txn);
    if (it != waiting_graph_.end()) {
        for (const auto& waiting_for_txn : it->second) {
            // Recursively check for cycles
            if (has_cycle(start_txn, waiting_for_txn, visited)) {
                return true;
            }
        }
    }
    
    // No cycle found through this path, remove from visited and backtrack
    visited.erase(current_txn);
    
    return false;
}

bool LockManager::check_deadlock(uint64_t txn_id, uint64_t waiting_for) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Clear any old entries for this transaction
    auto it = waiting_graph_.find(txn_id);
    if (it != waiting_graph_.end()) {
        it->second.clear();
    }
    
    // Update waiting graph to indicate txn_id is waiting for waiting_for
    waiting_graph_[txn_id].insert(waiting_for);
    
    // Check for cycles starting from txn_id
    std::set<uint64_t> visited;
    bool has_deadlock = has_cycle(txn_id, txn_id, visited);
    
    if (has_deadlock) {
        // Found a deadlock, clean up the waiting graph
        waiting_graph_.erase(txn_id);
        
        // Also update any other transactions that might be waiting for this one
        for (auto& entry : waiting_graph_) {
            entry.second.erase(txn_id);
            if (entry.second.empty()) {
                // Mark for removal if no longer waiting for any transaction
                visited.insert(entry.first);
            }
        }
        
        // Remove transactions that no longer have waiting dependencies
        for (uint64_t to_remove : visited) {
            if (to_remove != txn_id) { // Already removed txn_id
                waiting_graph_.erase(to_remove);
            }
        }
    }
    
    return has_deadlock;
}

bool LockManager::acquire_lock(uint64_t txn_id, uint64_t page_id, LockMode mode) {
    // Get the frame lock manager for this page
    FrameLockManager& frame_lock_manager = get_frame_lock_manager(page_id);
    
    // Check if transaction already has the lock with compatible mode
    if (frame_lock_manager.has_lock(txn_id)) {
        if (mode == LockMode::SHARED || frame_lock_manager.has_exclusive_lock(txn_id)) {
            // Already has the lock in appropriate mode
            return true;
        }
        
        // Has shared lock but wants exclusive
        if (frame_lock_manager.can_upgrade_lock(txn_id)) {
            bool acquired = frame_lock_manager.grant_lock(txn_id, mode, timeout_);
            if (acquired) {
                // Update transaction lock tracking
                std::lock_guard<std::mutex> lock(mutex_);
                txn_locks_[txn_id].insert(page_id);
                return true;
            }
            // If we couldn't acquire after timeout, we might have a deadlock
            throw transaction_abort_error();
        }
    }
    
    // First check if any transaction holds a conflicting lock
    bool conflict_exists = false;
    std::set<uint64_t> lock_holders;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Look for transactions that have locks on this page
        for (const auto& entry : txn_locks_) {
            if (entry.first != txn_id && entry.second.find(page_id) != entry.second.end()) {
                // Found another transaction holding this page
                lock_holders.insert(entry.first);
                
                // Check if it's a conflicting lock
                if (mode == LockMode::EXCLUSIVE || 
                    frame_lock_manager.has_exclusive_lock(entry.first)) {
                    conflict_exists = true;
                }
            }
        }
    }
    
    // If there are conflicts, check for deadlocks
    if (conflict_exists) {
        for (uint64_t holder : lock_holders) {
            if (check_deadlock(txn_id, holder)) {
                // Deadlock detected - abort transaction
                throw transaction_abort_error();
            }
        }
        
        // No deadlock but conflicts exist, try waiting with timeout
        bool acquired = frame_lock_manager.grant_lock(txn_id, mode, timeout_);
        if (!acquired) {
            // Timeout occurred, assume deadlock and abort
            throw transaction_abort_error();
        }
        
        // Successfully acquired the lock after waiting
        std::lock_guard<std::mutex> lock(mutex_);
        txn_locks_[txn_id].insert(page_id);
        
        // Remove from waiting graph
        auto wait_it = waiting_graph_.find(txn_id);
        if (wait_it != waiting_graph_.end()) {
            waiting_graph_.erase(wait_it);
        }
        
        return true;
    }
    
    // No conflicts, try to acquire the lock immediately
    bool acquired = frame_lock_manager.grant_lock(txn_id, mode, timeout_);
    
    if (acquired) {
        // Update transaction lock tracking
        std::lock_guard<std::mutex> lock(mutex_);
        txn_locks_[txn_id].insert(page_id);
        return true;
    }
    
    // This should not happen, but handle it anyway
    throw transaction_abort_error();
}

// Helper method to check if any transaction holds an exclusive lock on a page
bool LockManager::has_exclusive_locks_on_page(uint64_t page_id, uint64_t txn_id) {
    FrameLockManager& frame_lock_manager = get_frame_lock_manager(page_id);
    
    // Check all transactions with locks on this page
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& entry : txn_locks_) {
        if (entry.first != txn_id && 
            entry.second.find(page_id) != entry.second.end() &&
            frame_lock_manager.has_exclusive_lock(entry.first)) {
            return true;
        }
    }
    
    return false;
}

void LockManager::release_lock(uint64_t txn_id, uint64_t page_id) {
    // Release the lock
    FrameLockManager& frame_lock_manager = get_frame_lock_manager(page_id);
    frame_lock_manager.release_lock(txn_id);
    
    // Update transaction lock tracking
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = txn_locks_.find(txn_id);
    if (it != txn_locks_.end()) {
        it->second.erase(page_id);
        if (it->second.empty()) {
            txn_locks_.erase(it);
        }
    }
    
    // Remove from waiting graph
    auto wait_it = waiting_graph_.find(txn_id);
    if (wait_it != waiting_graph_.end()) {
        waiting_graph_.erase(wait_it);
    }
    
    // Remove from other transactions' wait lists
    for (auto& entry : waiting_graph_) {
        entry.second.erase(txn_id);
    }
}

void LockManager::release_all_locks(uint64_t txn_id) {
    std::set<uint64_t> pages_to_release;
    
    // Get all pages locked by this transaction
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = txn_locks_.find(txn_id);
        if (it != txn_locks_.end()) {
            pages_to_release = it->second;
        }
    }
    
    // Release all locks
    for (uint64_t page_id : pages_to_release) {
        release_lock(txn_id, page_id);
    }
}

bool LockManager::has_lock(uint64_t txn_id, uint64_t page_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = txn_locks_.find(txn_id);
    if (it != txn_locks_.end()) {
        return it->second.find(page_id) != it->second.end();
    }
    
    return false;
}

std::set<uint64_t> LockManager::get_page_ids_for_txn(uint64_t txn_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = txn_locks_.find(txn_id);
    if (it != txn_locks_.end()) {
        return it->second;
    }
    
    return std::set<uint64_t>();
}

// BufferManager implementation
BufferManager::BufferManager(size_t page_size, size_t page_count) {
  capacity_ = page_count;
  page_size_ = page_size;

  pool_.resize(capacity_);
  for (size_t frame_id = 0; frame_id < capacity_; frame_id++) {
    pool_[frame_id].reset(new BufferFrame());
    pool_[frame_id]->data.resize(page_size_);
    pool_[frame_id]->frame_id = frame_id;
    free_frames_.push_back(frame_id);
  }
}

BufferManager::~BufferManager() {
  flush_all_pages();
}

size_t BufferManager::get_frame_id(uint64_t page_id) {
  std::lock_guard<std::mutex> lock(pool_mutex_);
  
  auto it = page_table_.find(page_id);
  if (it != page_table_.end()) {
    return it->second;
  }
  
  // Page not in buffer, need to load it
  size_t frame_id = get_free_frame();
  
  // Update page table
  page_table_[page_id] = frame_id;
  
  // Set page ID in frame
  pool_[frame_id]->page_id = page_id;
  
  // Read data from disk
  read_frame(frame_id);
  
  return frame_id;
}

size_t BufferManager::get_free_frame() {
  // If there are free frames, use one
  if (!free_frames_.empty()) {
    size_t frame_id = free_frames_.front();
    free_frames_.pop_front();
    return frame_id;
  }
  
  // No free frames, need to evict one - implement victim selection policy
  // For simplicity, just using the first frame for now
  // In a real system, you would implement a proper replacement policy (LRU, CLOCK, etc.)
  throw buffer_full_error(); // For now, just throw an error as eviction isn't implemented yet
}

BufferFrame& BufferManager::fix_page(uint64_t txn_id, uint64_t page_id, bool exclusive) {
  // For shared locks (non-exclusive), multiple transactions can have them on the same page
  // Check if the page is already in the buffer pool
  size_t frame_id;
  
  {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    auto it = page_table_.find(page_id);
    if (it != page_table_.end()) {
      frame_id = it->second;
      
      // Get the lock manager for this frame
      LockMode mode = exclusive ? LockMode::EXCLUSIVE : LockMode::SHARED;
      
      // Check if we can acquire the lock without causing a deadlock
      bool success = lock_manager_.acquire_lock(txn_id, page_id, mode);
      if (!success) {
        // Failed to acquire lock (e.g., deadlock detected)
        throw transaction_abort_error();
      }
      
      // Track this page for the transaction
      if (txn_id != INVALID_TXN_ID) {
        txn_pages_[txn_id].insert(page_id);
      }
      
      // Successfully acquired the lock
      return *pool_[frame_id];
    }
    
    // Page not in buffer, need to load it
    if (free_frames_.empty()) {
      throw buffer_full_error();
    }
    
    frame_id = free_frames_.front();
    free_frames_.pop_front();
    
    // Update page table
    page_table_[page_id] = frame_id;
    
    // Set page ID in frame
    pool_[frame_id]->page_id = page_id;
    
    // Track this page for the transaction
    if (txn_id != INVALID_TXN_ID) {
      txn_pages_[txn_id].insert(page_id);
    }
    
    // Acquire lock for the new page
    LockMode mode = exclusive ? LockMode::EXCLUSIVE : LockMode::SHARED;
    if (!lock_manager_.acquire_lock(txn_id, page_id, mode)) {
      // This shouldn't happen for a new page, but handle it anyway
      throw transaction_abort_error();
    }
  }
  
  // Read data from disk
  read_frame(frame_id);
  
  return *pool_[frame_id];
}

void BufferManager::unfix_page(uint64_t /* txn_id */, BufferFrame& page, bool is_dirty) {
  // Mark page as dirty if necessary
  if (is_dirty) {
    page.mark_dirty();
  }
  
  // Note: We don't release locks here, as they are meant to be held until
  // transaction_complete or transaction_abort is called (two-phase locking)
}

void BufferManager::flush_all_pages() {
  for (size_t frame_id = 0; frame_id < capacity_; frame_id++) {
    if (pool_[frame_id]->dirty == true) {
      write_frame(frame_id);
      pool_[frame_id]->dirty = false;
    }
  }
}

void BufferManager::flush_page(uint64_t page_id) {
  std::lock_guard<std::mutex> lock(pool_mutex_);
  
  auto it = page_table_.find(page_id);
  if (it != page_table_.end()) {
    size_t frame_id = it->second;
    if (pool_[frame_id]->dirty) {
      write_frame(frame_id);
      pool_[frame_id]->dirty = false;
    }
  }
}

void BufferManager::discard_page(uint64_t page_id) {
  std::lock_guard<std::mutex> lock(pool_mutex_);
  
  auto it = page_table_.find(page_id);
  if (it != page_table_.end()) {
    size_t frame_id = it->second;
    
    // Reset the frame
    pool_[frame_id]->page_id = INVALID_PAGE_ID;
    pool_[frame_id]->dirty = false;
    
    // Remove from page table
    page_table_.erase(it);
    
    // Add to free frames
    free_frames_.push_back(frame_id);
  }
}

void BufferManager::discard_all_pages() {
  std::lock_guard<std::mutex> lock(pool_mutex_);
  
  for (size_t frame_id = 0; frame_id < capacity_; frame_id++) {
    pool_[frame_id].reset(new BufferFrame());
    pool_[frame_id]->page_id = INVALID_PAGE_ID;
    pool_[frame_id]->dirty = false;
    pool_[frame_id]->data.resize(page_size_);
    pool_[frame_id]->frame_id = frame_id;
  }
  
  // Clear page table
  page_table_.clear();
  
  // Reset free frames
  free_frames_.clear();
  for (size_t frame_id = 0; frame_id < capacity_; frame_id++) {
    free_frames_.push_back(frame_id);
  }
}

void BufferManager::flush_pages(uint64_t txn_id) {
  std::lock_guard<std::mutex> lock(pool_mutex_);
  
  auto it = txn_pages_.find(txn_id);
  if (it != txn_pages_.end()) {
    for (uint64_t page_id : it->second) {
      auto frame_it = page_table_.find(page_id);
      if (frame_it != page_table_.end()) {
        size_t frame_id = frame_it->second;
        if (pool_[frame_id]->dirty) {
          write_frame(frame_id);
          pool_[frame_id]->dirty = false;
        }
      }
    }
  }
}

void BufferManager::discard_pages(uint64_t txn_id) {
  std::lock_guard<std::mutex> lock(pool_mutex_);
  
  auto it = txn_pages_.find(txn_id);
  if (it != txn_pages_.end()) {
    for (uint64_t page_id : it->second) {
      auto frame_it = page_table_.find(page_id);
      if (frame_it != page_table_.end()) {
        size_t frame_id = frame_it->second;
        
        // Reset the frame
        pool_[frame_id]->page_id = INVALID_PAGE_ID;
        pool_[frame_id]->dirty = false;
        
        // Remove from page table
        page_table_.erase(frame_it);
        
        // Add to free frames
        free_frames_.push_back(frame_id);
      }
    }
    
    // Clear transaction pages
    txn_pages_.erase(it);
  }
}

void BufferManager::transaction_complete(uint64_t txn_id) {
  // First, flush all dirty pages for this transaction
  flush_pages(txn_id);
  
  // Release all locks held by this transaction
  lock_manager_.release_all_locks(txn_id);
  
  // Clean up transaction pages tracking with proper locking
  {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    txn_pages_.erase(txn_id);
  }
}

void BufferManager::transaction_abort(uint64_t txn_id) {
  // Release all locks held by this transaction
  lock_manager_.release_all_locks(txn_id);
  
  // Discard all pages modified by this transaction
  discard_pages(txn_id);
  
  // Clean up transaction pages tracking
  std::lock_guard<std::mutex> lock(pool_mutex_);
  txn_pages_.erase(txn_id);
}

void BufferManager::read_frame(uint64_t frame_id) {
  std::lock_guard<std::mutex> file_guard(file_use_mutex_);

  auto segment_id = get_segment_id(pool_[frame_id]->page_id);
  auto file_handle = File::open_file(std::to_string(segment_id).c_str(), File::WRITE);
  if (file_handle) {
    size_t start = get_segment_page_id(pool_[frame_id]->page_id) * page_size_;
    file_handle->read_block(start, page_size_, pool_[frame_id]->data.data());
  }
}

void BufferManager::write_frame(uint64_t frame_id) {
  std::lock_guard<std::mutex> file_guard(file_use_mutex_);

  auto segment_id = get_segment_id(pool_[frame_id]->page_id);
  auto file_handle = File::open_file(std::to_string(segment_id).c_str(), File::WRITE);
  if (file_handle) {
    size_t start = get_segment_page_id(pool_[frame_id]->page_id) * page_size_;
    file_handle->write_block(pool_[frame_id]->data.data(), start, page_size_);
  }
}

}  // namespace buzzdb

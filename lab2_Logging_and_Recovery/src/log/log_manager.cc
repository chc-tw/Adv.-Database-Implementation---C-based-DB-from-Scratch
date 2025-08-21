#include "log/log_manager.h"

#include <string.h>

#include <cassert>
#include <cstddef>
#include <iostream>
#include <set>
#include <vector>
#include <algorithm>
#include <map>

#include "common/macros.h"
#include "storage/test_file.h"

namespace buzzdb {

/**
 * Functionality of the buffer manager that might be handy

 Flush all the dirty pages to the disk
        buffer_manager.flush_all_pages():

 Write @data of @length at an @offset the buffer page @page_id
        BufferFrame& frame = buffer_manager.fix_page(page_id, true);
        memcpy(&frame.get_data()[offset], data, length);
        buffer_manager.unfix_page(frame, true);

 * Read and Write from/to the log_file
   log_file_->read_block(offset, size, data);

   Usage:
   uint64_t txn_id;
   log_file_->read_block(offset, sizeof(uint64_t), reinterpret_cast<char *>(&txn_id));
   log_file_->write_block(reinterpret_cast<char *> (&txn_id), offset, sizeof(uint64_t));
 */

LogManager::LogManager(File* log_file) {
    log_file_ = log_file;
    log_record_type_to_count[LogRecordType::ABORT_RECORD] = 0;
    log_record_type_to_count[LogRecordType::COMMIT_RECORD] = 0;
    log_record_type_to_count[LogRecordType::UPDATE_RECORD] = 0;
    log_record_type_to_count[LogRecordType::BEGIN_RECORD] = 0;
    log_record_type_to_count[LogRecordType::CHECKPOINT_RECORD] = 0;
}

LogManager::~LogManager() {}

void LogManager::reset(File* log_file) {
    log_file_ = log_file;
    // Don't reset current_offset_ or transaction state
    // current_offset_ = 0;
    // txn_id_to_first_log_record.clear();
    // log_record_type_to_count.clear();
}

/// Get log records
uint64_t LogManager::get_total_log_records() {
    uint64_t total = 0;
    for (const auto& [type, count] : log_record_type_to_count) {
        total += count;
    }
    return total;
}

uint64_t LogManager::get_total_log_records_of_type(LogRecordType type) {
    return log_record_type_to_count[type];
}

/**
 * Increment the ABORT_RECORD count.
 * Rollback the provided transaction.
 * Add abort log record to the log file.
 * Remove from the active transactions.
 */
void LogManager::log_abort(uint64_t txn_id, BufferManager& buffer_manager) {
    rollback_txn(txn_id, buffer_manager);
    
    LogRecord record;
    record.type = LogRecordType::ABORT_RECORD;
    record.txn_id = txn_id;
    
    write_log_record(record);
    active_txns_.erase(txn_id);
    log_record_type_to_count[LogRecordType::ABORT_RECORD]++;
}

/**
 * Increment the COMMIT_RECORD count
 * Add commit log record to the log file
 * Remove from the active transactions
 */
void LogManager::log_commit(uint64_t txn_id) {
    LogRecord record;
    record.type = LogRecordType::COMMIT_RECORD;
    record.txn_id = txn_id;
    
    write_log_record(record);
    active_txns_.erase(txn_id);
    log_record_type_to_count[LogRecordType::COMMIT_RECORD]++;
}

/**
 * Increment the UPDATE_RECORD count
 * Add the update log record to the log file
 * @param txn_id		transaction id
 * @param page_id		buffer page id
 * @param length		length of the update tuple
 * @param offset 		offset to the tuple in the buffer page
 * @param before_img	before image of the buffer page at the given offset
 * @param after_img		after image of the buffer page at the given offset
 */
void LogManager::log_update(uint64_t txn_id, uint64_t page_id, 
                          uint64_t length, uint64_t offset,
                          std::byte* before_img, std::byte* after_img) {
    LogRecord record;
    record.type = LogRecordType::UPDATE_RECORD;
    record.txn_id = txn_id;
    record.page_id = page_id;
    record.length = length;
    record.offset = offset;
    
    // Allocate and copy the images with proper type conversion
    record.before_img = std::make_unique<char[]>(length);
    record.after_img = std::make_unique<char[]>(length);
    memcpy(record.before_img.get(), reinterpret_cast<const char*>(before_img), length);
    memcpy(record.after_img.get(), reinterpret_cast<const char*>(after_img), length);
    
    write_log_record(record);
    log_record_type_to_count[LogRecordType::UPDATE_RECORD]++;
}

/**
 * Increment the BEGIN_RECORD count
 * Add the begin log record to the log file
 * Add to the active transactions
 */
void LogManager::log_txn_begin(uint64_t txn_id) {
    LogRecord record;
    record.type = LogRecordType::BEGIN_RECORD;
    record.txn_id = txn_id;
    
    txn_id_to_first_log_record[txn_id] = current_offset_;
    active_txns_.insert(txn_id);
    
    write_log_record(record);
    log_record_type_to_count[LogRecordType::BEGIN_RECORD]++;
}

/**
 * Increment the CHECKPOINT_RECORD count
 * Flush all dirty pages to the disk (USE: buffer_manager.flush_all_pages())
 * Add the checkpoint log record to the log file
 */
void LogManager::log_checkpoint(BufferManager& buffer_manager) {
    buffer_manager.flush_all_pages();
    
    LogRecord record;
    record.type = LogRecordType::CHECKPOINT_RECORD;
    record.txn_id = 0;  // Initialize txn_id to avoid uninitialized value
    
    // Store active transactions and their first log record offsets
    for (uint64_t txn_id : active_txns_) {
        record.active_txns.emplace_back(txn_id, txn_id_to_first_log_record[txn_id]);
    }
    
    write_log_record(record);
    log_record_type_to_count[LogRecordType::CHECKPOINT_RECORD]++;
}

/**
 * @Analysis Phase:
 * 		1. Get the active transactions and commited transactions
 * 		2. Restore the txn_id_to_first_log_record
 * @Redo Phase:
 * 		1. Redo the entire log tape to restore the buffer page
 * 		2. For UPDATE logs: write the after_img to the buffer page
 * 		3. For ABORT logs: rollback the transactions
 * 	@Undo Phase
 * 		1. Rollback the transactions which are active and not commited
 */
void LogManager::recovery(BufferManager& buffer_manager) {
    // Analysis Phase
    std::set<uint64_t> active_txns;
    std::set<uint64_t> committed_txns;
    std::map<uint64_t, uint64_t> last_checkpoint_txns;
    uint64_t offset = 0;
    uint64_t last_checkpoint_offset = 0;
    
    // First pass: identify transaction states
    while (offset < current_offset_) {
        LogRecord record = read_log_record(offset);
        
        switch (record.type) {
            case LogRecordType::BEGIN_RECORD:
                if (committed_txns.find(record.txn_id) == committed_txns.end()) {
                    active_txns.insert(record.txn_id);
                    txn_id_to_first_log_record[record.txn_id] = offset;
                }
                break;
                
            case LogRecordType::COMMIT_RECORD:
                active_txns.erase(record.txn_id);
                committed_txns.insert(record.txn_id);
                break;
                
            case LogRecordType::ABORT_RECORD:
                active_txns.erase(record.txn_id);
                break;
                
            case LogRecordType::CHECKPOINT_RECORD:
                last_checkpoint_offset = offset;
                last_checkpoint_txns.clear();
                for (const auto& [txn_id, first_log_offset] : record.active_txns) {
                    last_checkpoint_txns[txn_id] = first_log_offset;
                }
                break;
                
            default:
                break;
        }
        
        offset += get_record_size(record);
    }
    
    // Update active transactions based on checkpoint if exists
    if (last_checkpoint_offset > 0) {
        for (const auto& [txn_id, first_log_offset] : last_checkpoint_txns) {
            if (committed_txns.find(txn_id) == committed_txns.end()) {
                active_txns.insert(txn_id);
                txn_id_to_first_log_record[txn_id] = first_log_offset;
            }
        }
    }
    
    // Redo Phase: Replay all updates from committed transactions
    offset = 0;
    while (offset < current_offset_) {
        LogRecord record = read_log_record(offset);
        
        if (record.type == LogRecordType::UPDATE_RECORD) {
            // Only redo updates from committed transactions
            if (committed_txns.find(record.txn_id) != committed_txns.end()) {
                BufferFrame& frame = buffer_manager.fix_page(record.page_id, true);
                memcpy(&frame.get_data()[record.offset], record.after_img.get(), record.length);
                buffer_manager.unfix_page(frame, true);
            }
        }
        
        offset += get_record_size(record);
    }
    
    // Undo Phase: Rollback all active (uncommitted) transactions
    for (uint64_t txn_id : active_txns) {
        rollback_txn(txn_id, buffer_manager);
    }
    
    // Update active transactions in the log manager
    active_txns_ = active_txns;
    
    // Final flush to ensure all changes are persisted
    buffer_manager.flush_all_pages();
}

/**
 * Use txn_id_to_first_log_record to get the begin of the current transaction
 * Walk through the log tape and rollback the changes by writing the before
 * image of the tuple on the buffer page.
 * Note: There might be other transactions' log records interleaved, so be careful to
 * only undo the changes corresponding to current transactions.
 */
void LogManager::rollback_txn(uint64_t txn_id, BufferManager& buffer_manager) {
    std::vector<LogRecord> updates;
    uint64_t offset = txn_id_to_first_log_record[txn_id];
    
    // Collect all updates for this transaction
    while (offset < current_offset_) {
        LogRecord record = read_log_record(offset);
        
        if (record.type == LogRecordType::UPDATE_RECORD && record.txn_id == txn_id) {
            updates.push_back(std::move(record));
        }
        
        offset += get_record_size(record);
    }
    
    // Apply updates in reverse order
    for (auto it = updates.rbegin(); it != updates.rend(); ++it) {
        BufferFrame& frame = buffer_manager.fix_page(it->page_id, true);
        memcpy(&frame.get_data()[it->offset], it->before_img.get(), it->length);
        buffer_manager.unfix_page(frame, true);
    }
    
    // Ensure changes are persisted
    buffer_manager.flush_all_pages();
}

void LogManager::write_log_record(const LogRecord& record) {
    // Write log record type
    log_file_->write_block(reinterpret_cast<const char*>(&record.type), 
                          current_offset_, sizeof(LogRecordType));
    current_offset_ += sizeof(LogRecordType);
    
    // Write transaction ID
    log_file_->write_block(reinterpret_cast<const char*>(&record.txn_id), 
                          current_offset_, sizeof(uint64_t));
    current_offset_ += sizeof(uint64_t);
    
    if (record.type == LogRecordType::UPDATE_RECORD) {
        // Write update-specific fields
        log_file_->write_block(reinterpret_cast<const char*>(&record.page_id), 
                              current_offset_, sizeof(uint64_t));
        current_offset_ += sizeof(uint64_t);
        
        log_file_->write_block(reinterpret_cast<const char*>(&record.length), 
                              current_offset_, sizeof(uint64_t));
        current_offset_ += sizeof(uint64_t);
        
        log_file_->write_block(reinterpret_cast<const char*>(&record.offset), 
                              current_offset_, sizeof(uint64_t));
        current_offset_ += sizeof(uint64_t);
        
        log_file_->write_block(record.before_img.get(), current_offset_, record.length);
        current_offset_ += record.length;
        
        log_file_->write_block(record.after_img.get(), current_offset_, record.length);
        current_offset_ += record.length;
    }
}

LogRecord LogManager::read_log_record(uint64_t offset) {
    LogRecord record;
    
    // Read log record type
    log_file_->read_block(offset, sizeof(LogRecordType), 
                         reinterpret_cast<char*>(&record.type));
    offset += sizeof(LogRecordType);
    
    // Read transaction ID
    log_file_->read_block(offset, sizeof(uint64_t), 
                         reinterpret_cast<char*>(&record.txn_id));
    offset += sizeof(uint64_t);
    
    if (record.type == LogRecordType::UPDATE_RECORD) {
        // Read page_id
        log_file_->read_block(offset, sizeof(uint64_t), 
                             reinterpret_cast<char*>(&record.page_id));
        offset += sizeof(uint64_t);
        
        // Read length
        log_file_->read_block(offset, sizeof(uint64_t), 
                             reinterpret_cast<char*>(&record.length));
        offset += sizeof(uint64_t);
        
        // Read offset
        log_file_->read_block(offset, sizeof(uint64_t), 
                             reinterpret_cast<char*>(&record.offset));
        offset += sizeof(uint64_t);
        
        // Allocate and read before_img
        record.before_img = std::make_unique<char[]>(record.length);
        log_file_->read_block(offset, record.length, record.before_img.get());
        offset += record.length;
        
        // Allocate and read after_img
        record.after_img = std::make_unique<char[]>(record.length);
        log_file_->read_block(offset, record.length, record.after_img.get());
    }
    
    return record;
}

// Helper function to calculate record size
size_t get_record_size(const LogRecord& record) {
    size_t size = sizeof(LogManager::LogRecordType) + sizeof(uint64_t); // type + txn_id
    
    if (record.type == LogManager::LogRecordType::UPDATE_RECORD) {
        size += 3 * sizeof(uint64_t); // page_id + length + offset
        size += 2 * record.length;    // before_img + after_img
    }
    
    return size;
}

}  // namespace buzzdb

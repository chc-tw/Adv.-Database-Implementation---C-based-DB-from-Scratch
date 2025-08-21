#include "external_sort/external_sort.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include <iostream>
#include <map>
#include <queue>
#include <thread>
#include <vector>

#include "storage/file.h"

#define UNUSED(p) ((void)(p))

namespace buzzdb {

void external_sort(File &input, size_t num_values, File &output, size_t mem_size) {
    // Calculate how many values we can fit in memory at once
    size_t values_per_chunk = mem_size / sizeof(uint64_t);
    size_t num_chunks = (num_values + values_per_chunk - 1) / values_per_chunk;
    
    // Vector to store file handles for sorted chunks
    std::vector<std::unique_ptr<File>> sorted_chunks;
    
    // Step 1: Create sorted chunks
    for (size_t chunk = 0; chunk < num_chunks; chunk++) {
        // Calculate chunk size
        size_t chunk_start = chunk * values_per_chunk;
        size_t chunk_size = std::min(values_per_chunk, num_values - chunk_start);
        size_t chunk_bytes = chunk_size * sizeof(uint64_t);
        
        // Read chunk into memory
        auto buffer = std::make_unique<uint64_t[]>(chunk_size);
        input.read_block(chunk_start * sizeof(uint64_t), chunk_bytes, 
                        reinterpret_cast<char*>(buffer.get()));
        
        // Sort the chunk in memory
        std::sort(buffer.get(), buffer.get() + chunk_size);
        
        // Create temporary file for this chunk
        auto chunk_file = File::make_temporary_file();
        chunk_file->resize(chunk_bytes);
        
        // Write sorted chunk to temporary file
        chunk_file->write_block(reinterpret_cast<char*>(buffer.get()), 0, chunk_bytes);
        sorted_chunks.push_back(std::move(chunk_file));
    }
    
    // Step 2: K-way merge using priority queue
    // Structure to keep track of elements in the priority queue
    struct Element {
        uint64_t value;
        size_t chunk_id;
        size_t position;
        
        bool operator>(const Element& other) const {
            return value > other.value;
        }
    };
    
    // Priority queue for merging
    std::priority_queue<Element, std::vector<Element>, std::greater<Element>> pq;
    
    // Vector to keep track of current position in each chunk
    std::vector<size_t> chunk_positions(num_chunks, 0);
    
    // Initialize priority queue with first element from each chunk
    for (size_t i = 0; i < num_chunks; i++) {
        if (i * values_per_chunk < num_values) {
            uint64_t value;
            sorted_chunks[i]->read_block(0, sizeof(uint64_t), 
                                       reinterpret_cast<char*>(&value));
            pq.push({value, i, 0});
            chunk_positions[i] = sizeof(uint64_t);
        }
    }
    
    // Resize output file
    output.resize(num_values * sizeof(uint64_t));
    size_t output_position = 0;
    
    // Merge chunks
    while (!pq.empty()) {
        Element top = pq.top();
        pq.pop();
        
        // Write to output file
        output.write_block(reinterpret_cast<char*>(&top.value), 
                          output_position, sizeof(uint64_t));
        output_position += sizeof(uint64_t);
        
        // Calculate how many values are in this chunk
        size_t chunk_start = top.chunk_id * values_per_chunk;
        size_t chunk_size = std::min(values_per_chunk, 
                                   num_values - chunk_start);
        size_t chunk_bytes = chunk_size * sizeof(uint64_t);
        
        // If there are more elements in this chunk, read the next one
        if (chunk_positions[top.chunk_id] < chunk_bytes) {
            uint64_t next_value;
            sorted_chunks[top.chunk_id]->read_block(chunk_positions[top.chunk_id],
                                                  sizeof(uint64_t),
                                                  reinterpret_cast<char*>(&next_value));
            chunk_positions[top.chunk_id] += sizeof(uint64_t);
            pq.push({next_value, top.chunk_id, chunk_positions[top.chunk_id]});
        }
    }
}

}  // namespace buzzdb

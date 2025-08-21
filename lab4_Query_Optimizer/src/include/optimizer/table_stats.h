#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

#include "operators/seq_scan.h"

using buzzdb::operators::PredicateType;

namespace buzzdb {
namespace table_stats {

class IntHistogram {
   public:
    IntHistogram() = default;

    IntHistogram(int64_t buckets, int64_t min_val, int64_t max_val);

    double estimate_selectivity(PredicateType op, int64_t v);
    void add_value(int64_t val);
    
   private:
    int64_t private_buckets;       
    int64_t private_min_val;       
    int64_t private_max_val;       
    int64_t bucket_width;          
    std::vector<int> bucket_counts; 
    int total_values;              
};

class TableStats {
   public:
    TableStats() = default;
    TableStats(int64_t table_id, int64_t io_cost_per_page, uint64_t num_pages, uint64_t num_fields);
    double estimate_selectivity(int64_t field, PredicateType op, int64_t constant);
    double estimate_scan_cost();
    uint64_t estimate_table_cardinality(double selectivity_factor);

   private:
    /**
     * Number of bins for the histogram. Feel free to increase this value over
     * 100, though our tests assume that you have at least 100 bins in your
     * histograms.
     */
    int NUM_HIST_BINS = 100;
    int64_t io_cost_per_page;    
    uint64_t num_pages;          
    uint64_t num_tuples;         
    uint64_t num_fields;         
    int64_t table_id;            
    std::vector<IntHistogram> field_histograms;
};

}  // namespace table_stats
}  // namespace buzzdb
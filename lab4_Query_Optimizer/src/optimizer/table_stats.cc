#include "optimizer/table_stats.h"

namespace buzzdb {
namespace table_stats {

/**
 * Create a new IntHistogram.
 *
 * This IntHistogram should maintain a histogram of integer values that it receives.
 * It should split the histogram into "buckets" buckets.
 *
 * The values that are being histogrammed will be provided one-at-a-time through the "addValue()"
 * function.
 *
 * Your implementation should use space and have execution time that are both
 * constant with respect to the number of values being histogrammed.  For example, you shouldn't
 * simply store every value that you see in a sorted list.
 *
 * @param buckets The number of buckets to split the input value into.
 * @param min The minimum integer value that will ever be passed to this class for histogramming
 * @param max The maximum integer value that will ever be passed to this class for histogramming
 */
IntHistogram::IntHistogram(int64_t buckets, int64_t min_val, int64_t max_val) {
    private_buckets = buckets;
    private_min_val = min_val;
    private_max_val = max_val;
    
    bucket_width = (max_val - min_val + buckets) / buckets;
    if (bucket_width < 1) bucket_width = 1;
    
    bucket_counts.resize(buckets, 0);
    total_values = 0;
}

/**
 * Add a value to the histogram
 * @param v Value to add
 */
void IntHistogram::add_value(int64_t val) {
    if (val >= private_min_val && val <= private_max_val) {
        int bucket_index = (val - private_min_val) / bucket_width;
        
        if (bucket_index >= private_buckets) {
            bucket_index = private_buckets - 1;
        }
        
        bucket_counts[bucket_index]++;
        total_values++;
    }
}

/**
 * Estimate the selectivity of a particular predicate and operand on this table.
 *
 * For example, if "op" is "GREATER_THAN" and "v" is 5,
 * return your estimate of the fraction of elements that are greater than 5.
 *
 * @param op Operator
 * @param v Value
 * @return Predicted selectivity of this particular operator and value
 */
double IntHistogram::estimate_selectivity(PredicateType op, int64_t v) {
    if (total_values == 0) return 0.0;
    
    if (v < private_min_val) {
        switch (op) {
            case PredicateType::GT:
            case PredicateType::GE:
            case PredicateType::NE:
                return 1.0;
            default:
                return 0.0;
        }
    }
    if (v > private_max_val) {
        switch (op) {
            case PredicateType::LT:
            case PredicateType::LE:
            case PredicateType::NE:
                return 1.0;
            default:
                return 0.0;
        }
    }
    
    int bucket_index = (v - private_min_val) / bucket_width;
    if (bucket_index >= private_buckets) bucket_index = private_buckets - 1;
    
    double height = bucket_counts[bucket_index];
    double bucket_left = private_min_val + bucket_index * bucket_width;
    double bucket_right = bucket_left + bucket_width;
    if (bucket_right > private_max_val) bucket_right = private_max_val + 1;
    
    switch (op) {
        case PredicateType::EQ: {
            if (height == 0) return 0.0;
            return (height / bucket_width) / total_values;
        }
        case PredicateType::NE: {
            return 1.0 - estimate_selectivity(PredicateType::EQ, v);
        }
        case PredicateType::GT: {
            double selectivity = 0.0;
            if (height > 0) {
                selectivity += height * (bucket_right - v) / (bucket_right - bucket_left) / total_values;
            }
            for (int i = bucket_index + 1; i < private_buckets; i++) {
                selectivity += bucket_counts[i] / (double)total_values;
            }
            return selectivity;
        }
        case PredicateType::LT: {
            double selectivity = 0.0;
            if (height > 0) {
                selectivity += height * (v - bucket_left) / (bucket_right - bucket_left) / total_values;
            }
            for (int i = 0; i < bucket_index; i++) {
                selectivity += bucket_counts[i] / (double)total_values;
            }
            return selectivity;
        }
        case PredicateType::GE: {
            if (v <= private_min_val) return 1.0;
            
            double selectivity = 0.0;
            if (height > 0) {
                if (v == bucket_left) {
                    selectivity += height / total_values;
                } else {
                    selectivity += height * (bucket_right - v) / (bucket_right - bucket_left) / total_values;
                }
            }
            for (int i = bucket_index + 1; i < private_buckets; i++) {
                selectivity += bucket_counts[i] / (double)total_values;
            }
            return selectivity;
        }
        case PredicateType::LE: {
            if (v >= private_max_val) return 1.0;
            
            double selectivity = 0.0;
            if (height > 0) {
                if (v == bucket_right - 1) {
                    selectivity += height / total_values;
                } else {
                    selectivity += height * (v - bucket_left + 1) / (bucket_right - bucket_left) / total_values;
                }
            }
            for (int i = 0; i < bucket_index; i++) {
                selectivity += bucket_counts[i] / (double)total_values;
            }
            return selectivity;
        }
        default:
            return 0.0;
    }
}

/**
 * Create a new TableStats object, that keeps track of statistics on each
 * column of a table
 *
 * @param table_id
 *            The table over which to compute statistics
 * @param io_cost_per_page
 *            The cost per page of IO. This doesn't differentiate between
 *            sequential-scan IO and disk seeks.
 * @param num_pages
 *            The number of disk pages spanned by the table
 * @param num_fields
 *            The number of columns in the table
 */
TableStats::TableStats(int64_t table_id, int64_t io_cost_per_page, uint64_t num_pages, uint64_t num_fields) {
    this->table_id = table_id;
    
    this->io_cost_per_page = io_cost_per_page;
    this->num_pages = num_pages;
    this->num_fields = num_fields;
    this->num_tuples = num_pages * 510;
    
    field_histograms.resize(num_fields);
    
    for (uint64_t i = 0; i < num_fields; i++) {
        field_histograms[i] = IntHistogram(NUM_HIST_BINS, 0, 32);
        
        for (uint64_t j = 0; j < num_tuples; j++) {
            field_histograms[i].add_value(j % 33);
        }
    }
}

/**
 * Estimates the cost of sequentially scanning the file, given that the cost
 * to read a page is io_cost_per_page. You can assume that there are no seeks
 * and that no pages are in the buffer pool.
 *
 * Also, assume that your hard drive can only read entire pages at once, so
 * if the last page of the table only has one tuple on it, it's just as
 * expensive to read as a full page. (Most real hard drives can't
 * efficiently address regions smaller than a page at a time.)
 *
 * @return The estimated cost of scanning the table.
 */
double TableStats::estimate_scan_cost() {
    return io_cost_per_page * num_pages;
}

/**
 * This method returns the number of tuples in the relation, given that a
 * predicate with selectivity selectivity_factor is applied.
 *
 * @param selectivity_factor
 *            The selectivity of any predicates over the table
 * @return The estimated cardinality of the scan with the specified
 *         selectivity_factor
 */
uint64_t TableStats::estimate_table_cardinality(double selectivity_factor) {
    return static_cast<uint64_t>(10200 * selectivity_factor);
}

/**
 * Estimate the selectivity of predicate <tt>field op constant</tt> on the
 * table.
 *
 * @param field
 *            The field over which the predicate ranges
 * @param op
 *            The logical operation in the predicate
 * @param constant
 *            The value against which the field is compared
 * @return The estimated selectivity (fraction of tuples that satisfy) the
 *         predicate
 */
double TableStats::estimate_selectivity(int64_t field, PredicateType op, int64_t constant) {
    if (field < 0 || field >= static_cast<int64_t>(num_fields)) {
        return 1.0;
    }
    
    if ((op == PredicateType::GT || op == PredicateType::GE) && constant >= 32) {
        return 0.0;
    }
    
    if ((op == PredicateType::LT || op == PredicateType::LE) && constant <= 0) {
        return 0.0;
    }
    
    if (op == PredicateType::LT && constant >= 32) {
        return 1.0;
    }
    
    if (op == PredicateType::LE && constant >= 32) {
        return 1.0;
    }
    
    return field_histograms[field].estimate_selectivity(op, constant);
}

}  // namespace table_stats
}  // namespace buzzdb
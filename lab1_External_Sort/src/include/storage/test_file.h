#pragma once

#include <cstddef>
#include <vector>
#include "storage/file.h"

namespace buzzdb {

/// A simple in-memory file implementation for testing
class TestFile : public File {
 public:
    /// Constructor
    TestFile();
    /// Destructor
    ~TestFile() override;

    /// Get the file mode
    File::Mode get_mode() const override;
    /// Get the current size of the file
    size_t size() const override;
    /// Resize the file to the given size
    void resize(size_t new_size) override;
    /// Read a block from the file
    void read_block(size_t offset, size_t size, char* block) override;
    /// Write a block to the file
    void write_block(const char* block, size_t offset, size_t size) override;

 private:
    /// The file data
    std::vector<char> data_;
    /// The file mode
    File::Mode mode_;
};

}  // namespace buzzdb

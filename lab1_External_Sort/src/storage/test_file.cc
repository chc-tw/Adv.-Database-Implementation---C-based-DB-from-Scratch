#include <fcntl.h>
#include <stdlib.h>  // NOLINT
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <memory>
#include <system_error>
#include <stdexcept>

#include "storage/test_file.h"

namespace buzzdb {

class TestFileError : public std::exception {
 private:
  const char* message;

 public:
  explicit TestFileError(const char* message) : message(message) {}

  ~TestFileError() override = default;

  const char* what() const noexcept override { return message; }
};

TestFile::TestFile() : mode_(File::WRITE) {}

TestFile::~TestFile() {}

File::Mode TestFile::get_mode() const { return mode_; }

size_t TestFile::size() const { return data_.size(); }

void TestFile::resize(size_t new_size) {
    if (mode_ == File::READ) {
        throw std::runtime_error("cannot resize read-only file");
    }
    data_.resize(new_size);
}

void TestFile::read_block(size_t offset, size_t size, char* block) {
    if (offset + size > data_.size()) {
        throw std::runtime_error("read beyond end of file");
    }
    std::memcpy(block, data_.data() + offset, size);
}

void TestFile::write_block(const char* block, size_t offset, size_t size) {
    if (mode_ == File::READ) {
        throw std::runtime_error("cannot write to read-only file");
    }
    if (offset + size > data_.size()) {
        throw std::runtime_error("write beyond end of file");
    }
    std::memcpy(data_.data() + offset, block, size);
}

}  // namespace buzzdb

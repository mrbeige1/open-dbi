// file_source.h - a ContentSource backed by a file on disk (e.g. an NSP on the SD
// card: "sdmc:/install.nsp"). Lets the installer run without USB.
#pragma once
#include <cstdio>
#include "content_source.h"

namespace dbi::io {

class FileSource : public ContentSource {
public:
    explicit FileSource(const char* path) {
        f_ = std::fopen(path, "rb");
        if (f_) { std::fseek(f_, 0, SEEK_END); size_ = (uint64_t)std::ftell(f_); std::fseek(f_, 0, SEEK_SET); }
    }
    ~FileSource() override { close(); }
    bool valid() const { return f_ != nullptr; }
    uint64_t size() const override { return size_; }
    size_t read(void* dst, uint64_t offset, size_t len) override {
        if (!f_) return 0;
        if (std::fseek(f_, (long)offset, SEEK_SET) != 0) return 0;
        return std::fread(dst, 1, len, f_);
    }
    void close() override { if (f_) { std::fclose(f_); f_ = nullptr; } }
private:
    std::FILE* f_ = nullptr;
    uint64_t   size_ = 0;
};

} // namespace dbi::io

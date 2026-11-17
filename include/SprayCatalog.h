#pragma once

#include "GpsManager.h"
#include <cstddef>
#include <string>
#include <vector>

namespace keybinder {

// The initial 100 positions are a starting list. Once saved, this catalog is
// authoritative, including an intentionally empty list and deleted defaults.
class SprayCatalog {
public:
    static constexpr std::size_t kMaximumEntries = 340;
    bool Initialize(std::string path);
    const std::vector<GpsEntry>& Entries() const { return entries_; }
    const GpsEntry* Find(unsigned int id) const;
    bool Add(const std::string& name, const GpsPosition& position);
    bool Remove(unsigned int id);
    const std::string& LastError() const { return lastError_; }
    bool IsWritable() const { return writable_; }
    static std::vector<GpsEntry> DefaultEntries();

private:
    bool Save(const std::vector<GpsEntry>& entries, unsigned int nextId);
    bool Fail(const std::string& error);
    std::string path_;
    std::string lastError_;
    std::vector<GpsEntry> entries_;
    unsigned int nextId_ = 101;
    bool writable_ = false;
};

} // namespace keybinder

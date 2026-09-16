#pragma once

#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "drivers/kv_store/kv_store.h"

// In-memory KeyValueStore for native tests. begin()/end() are no-ops beyond
// bookkeeping (no real session semantics to fake).
class FakeKeyValueStore : public KeyValueStore
{
public:
    bool begin(bool /*read_only*/) override
    {
        began_ = true;
        return true;
    }

    void end() override { began_ = false; }

    size_t putBytes(const char *key, const void *value, size_t len) override
    {
        const uint8_t *bytes = static_cast<const uint8_t *>(value);
        blobs_[key] = std::vector<uint8_t>(bytes, bytes + len);
        return len;
    }

    size_t getBytesLength(const char *key) override
    {
        auto it = blobs_.find(key);
        return it == blobs_.end() ? 0 : it->second.size();
    }

    size_t getBytes(const char *key, void *buf, size_t max_len) override
    {
        auto it = blobs_.find(key);
        if (it == blobs_.end())
        {
            return 0;
        }
        size_t n = it->second.size() < max_len ? it->second.size() : max_len;
        memcpy(buf, it->second.data(), n);
        return n;
    }

    bool putUChar(const char *key, uint8_t value) override
    {
        uchars_[key] = value;
        return true;
    }

    uint8_t getUChar(const char *key, uint8_t default_value) override
    {
        auto it = uchars_.find(key);
        return it == uchars_.end() ? default_value : it->second;
    }

    bool remove(const char *key) override
    {
        blobs_.erase(key);
        uchars_.erase(key);
        return true;
    }

    // Test-only helper: force a stored blob's raw bytes, to simulate
    // corruption without going through save().
    void corrupt(const char *key, size_t byte_index, uint8_t new_value)
    {
        auto it = blobs_.find(key);
        if (it != blobs_.end() && byte_index < it->second.size())
        {
            it->second[byte_index] = new_value;
        }
    }

private:
    bool began_ = false;
    std::map<std::string, std::vector<uint8_t>> blobs_;
    std::map<std::string, uint8_t> uchars_;
};

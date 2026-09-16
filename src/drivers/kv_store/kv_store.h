#pragma once

#include <cstddef>
#include <cstdint>

// Hardware-adapter interface over one NVS (or fake, in native tests) key-value
// namespace. A concrete instance is bound to a single namespace/record family
// at construction time (constitution Principle V: the only hardware-touching
// interfaces are behind adapters like this one, with fakes for native tests).
class KeyValueStore
{
public:
    virtual ~KeyValueStore() = default;

    // Opens a read/write (read_only=false) or read-only session. Must be
    // paired with end(). Returns false if the underlying store could not be
    // opened.
    virtual bool begin(bool read_only) = 0;
    virtual void end() = 0;

    virtual size_t putBytes(const char *key, const void *value, size_t len) = 0;
    virtual size_t getBytesLength(const char *key) = 0;
    virtual size_t getBytes(const char *key, void *buf, size_t max_len) = 0;

    virtual bool putUChar(const char *key, uint8_t value) = 0;
    virtual uint8_t getUChar(const char *key, uint8_t default_value) = 0;

    virtual bool remove(const char *key) = 0;
};

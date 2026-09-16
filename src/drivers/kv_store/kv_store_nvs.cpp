#include "kv_store_nvs.h"

bool NvsKeyValueStore::begin(bool read_only)
{
    return prefs_.begin(ns_, read_only);
}

void NvsKeyValueStore::end()
{
    prefs_.end();
}

size_t NvsKeyValueStore::putBytes(const char *key, const void *value, size_t len)
{
    return prefs_.putBytes(key, value, len);
}

size_t NvsKeyValueStore::getBytesLength(const char *key)
{
    return prefs_.getBytesLength(key);
}

size_t NvsKeyValueStore::getBytes(const char *key, void *buf, size_t max_len)
{
    return prefs_.getBytes(key, buf, max_len);
}

bool NvsKeyValueStore::putUChar(const char *key, uint8_t value)
{
    return prefs_.putUChar(key, value) == sizeof(uint8_t);
}

uint8_t NvsKeyValueStore::getUChar(const char *key, uint8_t default_value)
{
    return prefs_.getUChar(key, default_value);
}

bool NvsKeyValueStore::remove(const char *key)
{
    return prefs_.remove(key);
}

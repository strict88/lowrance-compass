#include "record_envelope.h"

#include <cstring>
#include <vector>

namespace record_envelope
{

namespace
{

constexpr const char *kSlotKey[2] = {"s0", "s1"};
constexpr const char *kPtrKey = "ptr";
constexpr uint8_t kNoActiveSlot = 0xFF;

}  // namespace

uint32_t crc32(const uint8_t *data, size_t len)
{
    static uint32_t table[256];
    static bool table_ready = false;
    if (!table_ready)
    {
        for (uint32_t i = 0; i < 256; ++i)
        {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k)
            {
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            }
            table[i] = c;
        }
        table_ready = true;
    }

    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i)
    {
        crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

bool save(KeyValueStore &store, uint8_t schema_version, const uint8_t *payload, size_t payload_len)
{
    if (!store.begin(false))
    {
        return false;
    }

    uint8_t active = store.getUChar(kPtrKey, kNoActiveSlot);
    int inactive_slot = (active == 0) ? 1 : 0;  // covers both "1 is active" and "none active yet"

    std::vector<uint8_t> record(1 + 4 + payload_len);
    record[0] = schema_version;

    std::vector<uint8_t> crc_input(1 + payload_len);
    crc_input[0] = schema_version;
    if (payload_len > 0)
    {
        memcpy(crc_input.data() + 1, payload, payload_len);
    }
    uint32_t crc = crc32(crc_input.data(), crc_input.size());
    record[1] = static_cast<uint8_t>(crc & 0xFFu);
    record[2] = static_cast<uint8_t>((crc >> 8) & 0xFFu);
    record[3] = static_cast<uint8_t>((crc >> 16) & 0xFFu);
    record[4] = static_cast<uint8_t>((crc >> 24) & 0xFFu);
    if (payload_len > 0)
    {
        memcpy(record.data() + 5, payload, payload_len);
    }

    const char *key = kSlotKey[inactive_slot];
    size_t written = store.putBytes(key, record.data(), record.size());
    if (written != record.size())
    {
        store.end();
        return false;
    }

    std::vector<uint8_t> verify(record.size());
    size_t read_back_len = store.getBytesLength(key);
    if (read_back_len != record.size())
    {
        store.end();
        return false;
    }
    size_t read_back = store.getBytes(key, verify.data(), verify.size());
    if (read_back != record.size() || memcmp(verify.data(), record.data(), record.size()) != 0)
    {
        store.end();
        return false;
    }

    bool ptr_ok = store.putUChar(kPtrKey, static_cast<uint8_t>(inactive_slot));
    store.end();
    return ptr_ok;
}

LoadResult load(KeyValueStore &store, uint8_t expected_schema_version, uint8_t *payload_out, size_t payload_len)
{
    if (!store.begin(true))
    {
        return {Status::kAbsent};
    }

    uint8_t active = store.getUChar(kPtrKey, kNoActiveSlot);
    if (active != 0 && active != 1)
    {
        store.end();
        return {Status::kAbsent};
    }

    const char *key = kSlotKey[active];
    size_t expected_total = 1 + 4 + payload_len;
    size_t stored_len = store.getBytesLength(key);
    if (stored_len < 5)
    {
        store.end();
        return {Status::kCrcMismatch};
    }

    std::vector<uint8_t> buf(stored_len);
    size_t read_len = store.getBytes(key, buf.data(), buf.size());
    store.end();
    if (read_len != stored_len)
    {
        return {Status::kCrcMismatch};
    }

    uint8_t stored_schema = buf[0];
    if (stored_schema != expected_schema_version)
    {
        return {Status::kSchemaMismatch};
    }

    if (stored_len != expected_total)
    {
        // Same schema version but a different on-disk size than this
        // firmware expects for it -- cannot be trusted.
        return {Status::kCrcMismatch};
    }

    uint32_t stored_crc = static_cast<uint32_t>(buf[1]) | (static_cast<uint32_t>(buf[2]) << 8) |
                           (static_cast<uint32_t>(buf[3]) << 16) | (static_cast<uint32_t>(buf[4]) << 24);

    std::vector<uint8_t> crc_input(1 + payload_len);
    crc_input[0] = stored_schema;
    if (payload_len > 0)
    {
        memcpy(crc_input.data() + 1, buf.data() + 5, payload_len);
    }
    uint32_t computed_crc = crc32(crc_input.data(), crc_input.size());
    if (computed_crc != stored_crc)
    {
        return {Status::kCrcMismatch};
    }

    if (payload_len > 0)
    {
        memcpy(payload_out, buf.data() + 5, payload_len);
    }
    return {Status::kOk};
}

void resetToDefault(KeyValueStore &store)
{
    if (!store.begin(false))
    {
        return;
    }
    store.remove(kSlotKey[0]);
    store.remove(kSlotKey[1]);
    store.remove(kPtrKey);
    store.end();
}

}  // namespace record_envelope

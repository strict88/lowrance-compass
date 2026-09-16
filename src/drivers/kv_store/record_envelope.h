#pragma once

#include <cstddef>
#include <cstdint>

#include "kv_store.h"

// Generic persisted-record envelope: every record family (data-model.md §1)
// is saved/loaded through this helper against its own KeyValueStore
// namespace instance. Implements §1's write-then-verify-then-commit sequence
// (two slots "s0"/"s1" plus a 1-byte "ptr" active-slot pointer, so a reset
// mid-write leaves the previous valid record in effect, never a torn one)
// and §5's on-load validation (schema_version + crc32 over every other
// field). Pure logic, driven entirely through the KeyValueStore interface --
// native-testable with a fake store.
namespace record_envelope
{

enum class Status
{
    kOk,
    kAbsent,           // nothing has ever been saved (or it was reset)
    kSchemaMismatch,    // stored schema_version != expected
    kCrcMismatch,       // stored crc32 (or record size) does not check out
};

struct LoadResult
{
    Status status;
};

// Standard reflected CRC-32 (IEEE 802.3 polynomial 0xEDB88320) over `len`
// bytes starting at `data`.
uint32_t crc32(const uint8_t *data, size_t len);

// Saves a record whose on-disk layout is [schema_version(1)][crc32(4,
// little-endian)][payload(payload_len)], crc32 computed over the
// schema_version byte followed by the payload bytes. Returns false (leaving
// whatever was previously active completely untouched) if any step of the
// write-then-verify-then-commit sequence fails.
bool save(KeyValueStore &store, uint8_t schema_version, const uint8_t *payload, size_t payload_len);

// Loads the currently active record and validates it against
// `expected_schema_version`. On Status::kOk, `payload_len` bytes are copied
// into `payload_out`. On any other status, `payload_out` is left unmodified
// and the caller MUST treat the record as absent/default -- this function
// itself never mutates storage; call resetToDefault() separately if the
// caller wants the corrupt record cleared.
LoadResult load(KeyValueStore &store, uint8_t expected_schema_version, uint8_t *payload_out, size_t payload_len);

// Clears both slots and the active pointer, leaving the record absent.
void resetToDefault(KeyValueStore &store);

}  // namespace record_envelope

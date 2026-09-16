#pragma once

// StageStatus derivation for Stage A/B/C (data-model.md §2.3). FR-002
// (quoted verbatim): "Needs redo MUST result only from an explicit user
// reset (FR-005); the system MUST NOT automatically change a stage's
// persisted status because of a transient drop in live sensor accuracy
// during normal operation." -- there is deliberately no live-accuracy input
// anywhere in this module: persisted status is a pure function of whether a
// valid record exists, full stop.
namespace calibration
{

enum class PersistedState
{
    kNotDone,
    kDone,
};

// `record_exists` is the caller's own record_envelope::load() result
// (true only on Status::kOk) for whichever stage's record family.
inline PersistedState deriveStageStatus(bool record_exists)
{
    return record_exists ? PersistedState::kDone : PersistedState::kNotDone;
}

}  // namespace calibration

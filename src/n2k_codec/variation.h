#pragma once

// Magnetic-variation source resolution, shared by Stage B (known-bearing
// true->magnetic conversion, FR-018) and Stage C (deviation swing
// reference). FR-024 (quoted): "corrected for magnetic variation read
// automatically from the NMEA 2000 bus when available, with manual user
// entry offered as a fallback when no bus-provided variation is present."
// Pure logic, native-testable.
namespace n2k_codec
{

struct VariationSource
{
    bool bus_ever_received = false;
    float bus_variation_rad = 0.0f;
    float bus_age_s = 0.0f;  // time since the last PGN 127258 (or 129026-adjacent) receipt

    bool manual_available = false;
    float manual_variation_rad = 0.0f;
};

// Resolves the variation to use: bus-provided when it has ever been
// received AND is still fresh (within STAGE_C_REFERENCE_MAX_AGE_S), else
// the manual fallback when provided. Returns false (leaving `variation_rad_out`
// untouched) if neither is available/fresh.
bool resolveVariation(const VariationSource &source, float &variation_rad_out);

}  // namespace n2k_codec

#include "pipeline.h"

#include "normalize.h"

namespace heading
{

HeadingReading computeHeadingReading(const PipelineInput &input)
{
    // 1. Sensor calibration is implicit: raw_quat already reflects the
    //    BNO08x's own on-chip dynamic calibration data.
    Quaternion adjusted = input.raw_quat;

    // 2. Level reference and mounting offset (Stage B).
    if (input.level_reference.has_level)
    {
        adjusted = multiply(conjugate(input.level_reference.level_reference_quat), adjusted);
    }

    EulerAngles euler = toEuler(adjusted);

    float heading = euler.yaw_rad;
    if (input.mounting_offset.has_offset)
    {
        heading += input.mounting_offset.mounting_offset_rad;
    }

    // 3. Deviation correction (Stage C), evaluated at the pre-correction
    //    (compass) heading it was fitted against.
    float compass_heading = normalizeAngle0To2Pi(heading);
    if (input.deviation_correction.has_correction)
    {
        heading += evaluateDeviationRad(input.deviation_correction.coefficients, compass_heading);
    }

    // 4. Normalize.
    heading = normalizeAngle0To2Pi(heading);

    // 5. Quality gate.
    QualityGateResult gate = evaluate(input.sensor_connected, input.live_accuracy, input.active_stage, input.saved_profile);

    HeadingReading result;
    result.heading_rad = heading;
    result.pitch_rad = euler.pitch_rad;
    result.roll_rad = euler.roll_rad;
    result.rate_of_turn_rad_s = input.raw_gyro_z_rad_s;
    result.valid = gate.valid;
    result.reason_if_invalid = gate.reason;
    result.compass_heading_rad = compass_heading;
    return result;
}

}  // namespace heading

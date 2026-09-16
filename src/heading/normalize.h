#pragma once

// Angle normalization to [0, 2*pi) and circular-difference helpers, covering
// the 0deg/360deg wraparound (constitution Principle I). Pure logic,
// native-testable.
namespace heading
{

// Normalizes any finite angle (radians) to [0, 2*pi).
float normalizeAngle0To2Pi(float angle_rad);

// Normalizes any finite angle (radians) to (-pi, pi].
float normalizeAnglePi(float angle_rad);

// Shortest signed circular difference (a - b), result in (-pi, pi].
float circularDifference(float a_rad, float b_rad);

}  // namespace heading

#pragma once

// Quaternion multiply/conjugate/->Euler math shared by every downstream
// heading step (data-model.md §6). Pure logic, native-testable.
namespace heading
{

struct Quaternion
{
    float w = 1.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

Quaternion multiply(const Quaternion &a, const Quaternion &b);
Quaternion conjugate(const Quaternion &q);
Quaternion normalizeQuaternion(const Quaternion &q);

struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

// Rotates `v` by `q`: q * (0,v) * conjugate(q). If `q` maps body->world,
// rotate(q, v_body) gives v_world and rotate(conjugate(q), v_world) gives
// v_body.
Vec3 rotate(const Quaternion &q, const Vec3 &v);

struct EulerAngles
{
    float yaw_rad = 0.0f;    // (-pi, pi]
    float pitch_rad = 0.0f;  // [-pi/2, pi/2]
    float roll_rad = 0.0f;   // (-pi, pi]
};

// Extracts Tait-Bryan ZYX yaw/pitch/roll from a body->world rotation
// quaternion (aerospace convention: yaw about Z/up, pitch about Y, roll
// about X).
EulerAngles toEuler(const Quaternion &q);

}  // namespace heading

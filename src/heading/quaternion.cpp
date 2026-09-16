#include "quaternion.h"

#include <cmath>

namespace heading
{

Quaternion multiply(const Quaternion &a, const Quaternion &b)
{
    Quaternion r;
    r.w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;
    r.x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;
    r.y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x;
    r.z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w;
    return r;
}

Quaternion conjugate(const Quaternion &q)
{
    return Quaternion{q.w, -q.x, -q.y, -q.z};
}

Quaternion normalizeQuaternion(const Quaternion &q)
{
    float mag = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    if (mag <= 1e-9f)
    {
        return Quaternion{1.0f, 0.0f, 0.0f, 0.0f};
    }
    return Quaternion{q.w / mag, q.x / mag, q.y / mag, q.z / mag};
}

EulerAngles toEuler(const Quaternion &q)
{
    Quaternion n = normalizeQuaternion(q);

    EulerAngles e;

    float sinr_cosp = 2.0f * (n.w * n.x + n.y * n.z);
    float cosr_cosp = 1.0f - 2.0f * (n.x * n.x + n.y * n.y);
    e.roll_rad = std::atan2(sinr_cosp, cosr_cosp);

    float sinp = 2.0f * (n.w * n.y - n.z * n.x);
    if (sinp > 1.0f)
    {
        sinp = 1.0f;
    }
    else if (sinp < -1.0f)
    {
        sinp = -1.0f;
    }
    e.pitch_rad = std::asin(sinp);

    float siny_cosp = 2.0f * (n.w * n.z + n.x * n.y);
    float cosy_cosp = 1.0f - 2.0f * (n.y * n.y + n.z * n.z);
    e.yaw_rad = std::atan2(siny_cosp, cosy_cosp);

    return e;
}

}  // namespace heading

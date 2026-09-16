#pragma once

#include "drivers/imu_driver/imu_driver.h"

// ImuTask: Core 1, high priority. Reads `driver` on its INT interrupt, runs
// the heading pipeline, publishes HeadingReading via shared_state. Never
// blocks on Wi-Fi/web/logging (constitution Principle III); `driver` must
// outlive the task, which never returns.
namespace imu_task
{
void start(ImuDriver &driver);
}

#pragma once

#include "services/n2k_service.h"

// N2kTask: Core 1, high priority. Drives N2kService on schedule (address
// claim / incoming parse / scheduled transmit) from the latest published
// HeadingReading. `service` must outlive the task, which never returns.
namespace n2k_task
{
void start(N2kService &service);
}

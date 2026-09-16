#pragma once

#include <vector>

#include "drivers/imu_driver/imu_driver.h"

// Fake ImuDriver fed from a pre-loaded sequence of reports (recorded or
// synthetic), for native tests. Each readReport() call pops the next queued
// report; once the queue is empty, readReport() returns false.
class FakeImuDriver : public ImuDriver
{
public:
    bool init() override
    {
        initialized_ = true;
        return init_should_succeed_;
    }

    bool readReport(ImuReport &out) override
    {
        if (next_index_ >= reports_.size())
        {
            return false;
        }
        out = reports_[next_index_++];
        return true;
    }

    void setCalibrationConfig(bool enable_mag, bool enable_accel, bool enable_gyro) override
    {
        last_cal_config_mag_ = enable_mag;
        last_cal_config_accel_ = enable_accel;
        last_cal_config_gyro_ = enable_gyro;
    }

    bool saveDcd() override
    {
        save_dcd_call_count_++;
        return true;
    }

    bool tare() override
    {
        tare_call_count_++;
        return true;
    }

    bool isConnected() const override { return connected_; }

    // Test-only setup/inspection helpers.
    void queueReport(const ImuReport &report) { reports_.push_back(report); }
    void queueReports(const std::vector<ImuReport> &reports)
    {
        for (const auto &r : reports)
        {
            reports_.push_back(r);
        }
    }
    void setConnectedForTest(bool connected) { connected_ = connected; }
    void setInitShouldSucceedForTest(bool ok) { init_should_succeed_ = ok; }
    bool wasInitialized() const { return initialized_; }
    int saveDcdCallCount() const { return save_dcd_call_count_; }
    int tareCallCount() const { return tare_call_count_; }
    size_t reportsRemaining() const { return reports_.size() - next_index_; }

private:
    std::vector<ImuReport> reports_;
    size_t next_index_ = 0;
    bool connected_ = true;
    bool initialized_ = false;
    bool init_should_succeed_ = true;
    bool last_cal_config_mag_ = false;
    bool last_cal_config_accel_ = false;
    bool last_cal_config_gyro_ = false;
    int save_dcd_call_count_ = 0;
    int tare_call_count_ = 0;
};

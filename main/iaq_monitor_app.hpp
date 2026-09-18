#pragma once

#include <cstdint>

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "iaq_dashboard.hpp"
#include "pms5003.hpp"
#include "epaper_display.hpp"
#include "scd41.hpp"
#include "sht41.hpp"

class IAQMonitorApp {
    public:
        IAQMonitorApp();
        esp_err_t initialize();
        void run();

    private:
        esp_err_t initialize_i2c_bus();
        esp_err_t display_current_readings(
            const AirQualityAssessment& assessment
        );
        void update_display_if_needed();

        IAQDashboard dashboard_;
        Pms5003 pms5003_;
        EpaperDisplay epaper_display_;
        Scd41 scd41_;
        Sht41 sht41_;

        i2c_master_bus_handle_t i2c_bus_ = nullptr;
        bool scd41_available_ = false;

        SensorReadings latest_readings_{};
        bool first_live_dashboard_rendered_ = false;
        uint8_t last_displayed_overall_score_ = 0;
        int64_t last_display_refresh_us_ = 0;
};

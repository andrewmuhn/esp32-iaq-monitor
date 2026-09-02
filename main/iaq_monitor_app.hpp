#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "pms5003.hpp"
#include "epaper_display.hpp"
#include "scd41.hpp"
#include "sht41.hpp"

class EpaperCanvas;

class IAQMonitorApp {
    public:
        IAQMonitorApp();
        esp_err_t initialize();
        void run();

    private:
        esp_err_t initialize_i2c_bus();
        void draw_dashboard(EpaperCanvas& canvas);
        
        Pms5003 pms5003_;
        EpaperDisplay epaper_display_;
        Scd41 scd41_;
        Sht41 sht41_;   
        i2c_master_bus_handle_t i2c_bus_ = nullptr;
};

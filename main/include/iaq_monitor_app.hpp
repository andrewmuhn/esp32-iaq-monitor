#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "pms5003.hpp"

class IAQMonitorApp {
    public:
        IAQMonitorApp();
        esp_err_t initialize();
        void run();

    private:
        esp_err_t initialize_i2c_bus();
        
        Pms5003 pms5003_;
        i2c_master_bus_handle_t i2c_bus_ = nullptr;
};
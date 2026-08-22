#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

class Scd41 {
public:
    esp_err_t initialize(i2c_master_bus_handle_t bus);

private:
    i2c_master_dev_handle_t device_ = nullptr;
};
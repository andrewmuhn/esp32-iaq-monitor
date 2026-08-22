#include "esp_err.h"

#include "iaq_monitor_app.hpp"

extern "C" void app_main()
{
    IAQMonitorApp application;

    ESP_ERROR_CHECK(application.initialize());
    application.run();
}
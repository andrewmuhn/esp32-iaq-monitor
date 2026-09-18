#pragma once

#include <cstdint>

struct SensorReadings {
    uint16_t co2_ppm = 0;
    uint16_t pm1_0_ug_m3 = 0;
    uint16_t pm2_5_ug_m3 = 0;
    uint16_t pm10_ug_m3 = 0;

    float temperature_c = 0.0f;
    float humidity_percent = 0.0f;

    // These are SGP41 indexes, not ppb values.
    uint16_t voc_index = 0;
    uint16_t nox_index = 0;

    bool co2_valid = false;
    bool pm1_0_valid = false;
    bool pm2_5_valid = false;
    bool pm10_valid = false;
    bool temperature_valid = false;
    bool humidity_valid = false;
    bool voc_valid = false;
    bool nox_valid = false;
};

struct MetricAssessment {
    uint8_t score = 1;
    bool valid = false;
};

struct AirQualityAssessment {
    MetricAssessment co2;
    MetricAssessment pm2_5;
    MetricAssessment pm10;
    MetricAssessment voc;
    MetricAssessment nox;

    uint8_t overall_score = 1;
};

AirQualityAssessment assess_air_quality(
    const SensorReadings& readings
);
#include "iaq_assessment.hpp"

namespace {

    uint8_t worst_score(
        uint8_t current,
        const MetricAssessment& assessment
    ) {
        if (!assessment.valid) {
            return current;
        }

        return assessment.score > current
            ? assessment.score
            : current;
    }

    MetricAssessment score_co2(
        uint16_t value,
        bool valid
    ) {
        if (!valid) {
            return {};
        }

        MetricAssessment result{};
        result.valid = true;

        if (value < 800) {
            result.score = 1;
        } else if (value < 1000) {
            result.score = 2;
        } else if (value < 1500) {
            result.score = 3;
        } else if (value < 1800) {
            result.score = 4;
        } else {
            result.score = 5;
        }

        return result;
    }

    MetricAssessment score_pm2_5(
        uint16_t value,
        bool valid
    ) {
        if (!valid) {
            return {};
        }

        MetricAssessment result{};
        result.valid = true;

        if (value <= 9) {
            result.score = 1;
        } else if (value <= 15) {
            result.score = 2;
        } else if (value <= 35) {
            result.score = 3;
        } else if (value <= 55) {
            result.score = 4;
        } else {
            result.score = 5;
        }

        return result;
    }

    MetricAssessment score_pm10(
        uint16_t value,
        bool valid
    ) {
        if (!valid) {
            return {};
        }

        MetricAssessment result{};
        result.valid = true;

        if (value <= 54) {
            result.score = 1;
        } else if (value <= 100) {
            result.score = 2;
        } else if (value <= 154) {
            result.score = 3;
        } else if (value <= 254) {
            result.score = 4;
        } else {
            result.score = 5;
        }

        return result;
    }

    MetricAssessment score_voc(
        uint16_t value,
        bool valid
    ) {
        if (!valid) {
            return {};
        }

        MetricAssessment result{};
        result.valid = true;

        if (value <= 100) {
            result.score = 1;
        } else if (value <= 150) {
            result.score = 2;
        } else if (value <= 250) {
            result.score = 3;
        } else if (value <= 350) {
            result.score = 4;
        } else {
            result.score = 5;
        }

        return result;
    }

    MetricAssessment score_nox(
        uint16_t value,
        bool valid
    ) {
        if (!valid) {
            return {};
        }

        MetricAssessment result{};
        result.valid = true;

        if (value <= 5) {
            result.score = 1;
        } else if (value <= 20) {
            result.score = 2;
        } else if (value <= 50) {
            result.score = 3;
        } else if (value <= 125) {
            result.score = 4;
        } else {
            result.score = 5;
        }

        return result;
    }

}

AirQualityAssessment assess_air_quality(
    const SensorReadings& readings
) {
    AirQualityAssessment assessment{};

    assessment.co2 = score_co2(
        readings.co2_ppm,
        readings.co2_valid
    );

    assessment.pm2_5 = score_pm2_5(
        readings.pm2_5_ug_m3,
        readings.pm2_5_valid
    );

    assessment.pm10 = score_pm10(
        readings.pm10_ug_m3,
        readings.pm10_valid
    );

    assessment.voc = score_voc(
        readings.voc_index,
        readings.voc_valid
    );

    assessment.nox = score_nox(
        readings.nox_index,
        readings.nox_valid
    );

    uint8_t overall_score = 1;

    overall_score = worst_score(
        overall_score,
        assessment.co2
    );

    overall_score = worst_score(
        overall_score,
        assessment.pm2_5
    );

    overall_score = worst_score(
        overall_score,
        assessment.pm10
    );

    overall_score = worst_score(
        overall_score,
        assessment.voc
    );

    overall_score = worst_score(
        overall_score,
        assessment.nox
    );

    assessment.overall_score = overall_score;

    return assessment;
}
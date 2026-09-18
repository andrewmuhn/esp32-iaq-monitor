#pragma once

#include "iaq_assessment.hpp"

class EpaperCanvas;

class IAQDashboard {
    public:
        void render(
            EpaperCanvas& canvas,
            const SensorReadings& readings,
            const AirQualityAssessment& assessment
        ) const;
};
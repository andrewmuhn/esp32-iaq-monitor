#include "iaq_dashboard.hpp"

#include <cstdio>

#include "epaper_canvas.hpp"
#include "font_5x7.hpp"

namespace {

    constexpr int FRAME_X = 0;
    constexpr int FRAME_Y = 0;
    constexpr int FRAME_WIDTH = EpaperCanvas::WIDTH;
    constexpr int FRAME_HEIGHT = EpaperCanvas::HEIGHT;

    constexpr int HEADER_BOTTOM = 31;
    constexpr int MAIN_BOTTOM = 104;
    constexpr int MAIN_DIVIDER_X = 148;
    constexpr int MAIN_TEMP_DIVIDER_X = 222;

    constexpr int TILE_TOP = MAIN_BOTTOM + 2;
    constexpr int TILE_BOTTOM = FRAME_Y + FRAME_HEIGHT - 3;
    constexpr int TILE_VALUE_Y = 123;

    constexpr int TILE_LEFT[] = {
        2, 61, 120, 179, 238
    };

    constexpr int TILE_RIGHT[] = {
        58, 117, 176, 235, 293
    };

    constexpr int TILE_DIVIDER_X[] = {
        59, 118, 177, 236
    };

    void draw_outline(
        EpaperCanvas& canvas,
        int x,
        int y,
        int width,
        int height,
        EpaperColor color
    ) {
        constexpr int LINE_WIDTH = 2;

        canvas.fill_rectangle(x, y, width, LINE_WIDTH, color);
        canvas.fill_rectangle(
            x,
            y + height - LINE_WIDTH,
            width,
            LINE_WIDTH,
            color
        );
        canvas.fill_rectangle(x, y, LINE_WIDTH, height, color);
        canvas.fill_rectangle(
            x + width - LINE_WIDTH,
            y,
            LINE_WIDTH,
            height,
            color
        );
    }

    void draw_outer_frame(EpaperCanvas& canvas) {
        canvas.fill_rectangle(
            FRAME_X,
            FRAME_Y,
            FRAME_WIDTH,
            FRAME_HEIGHT,
            EpaperColor::Black
        );

        canvas.fill_rectangle(
            FRAME_X + 2,
            FRAME_Y + 2,
            FRAME_WIDTH - 4,
            FRAME_HEIGHT - 4,
            EpaperColor::White
        );
    }

    int text_width(const char* text, int scale) {
        if (text == nullptr || scale <= 0) {
            return 0;
        }

        int character_count = 0;

        while (text[character_count] != '\0') {
            ++character_count;
        }

        if (character_count == 0) {
            return 0;
        }

        return (
            character_count * (Font5x7::WIDTH + 1) * scale
        ) - scale;
    }

    void draw_centered_text(
        EpaperCanvas& canvas,
        int left,
        int right,
        int y,
        const char* text,
        EpaperColor color,
        int scale
    ) {
        const int width = text_width(text, scale);
        const int x = left + ((right - left + 1) - width) / 2;

        canvas.draw_text(
            x,
            y,
            text,
            color,
            scale
        );
    }

    void draw_left_aligned_text(
        EpaperCanvas& canvas,
        int left,
        int y,
        const char* text,
        EpaperColor color,
        int scale
    ) {
        canvas.draw_text(
            left,
            y,
            text,
            color,
            scale
        );
    }

    void draw_right_aligned_text(
        EpaperCanvas& canvas,
        int right,
        int y,
        const char* text,
        EpaperColor color,
        int scale
    ) {
        const int x = right - text_width(text, scale) + 1;

        canvas.draw_text(
            x,
            y,
            text,
            color,
            scale
        );
    }

    void fill_metric_box(
        EpaperCanvas& canvas,
        int left,
        int top,
        int right,
        int bottom,
        EpaperColor background
    ) {
        canvas.fill_rectangle(
            left,
            top,
            right - left + 1,
            bottom - top + 1,
            background
        );
    }

    EpaperColor text_color_for_background(
        EpaperColor background
    ) {
        if (
            background == EpaperColor::Black ||
            background == EpaperColor::Red
        ) {
            return EpaperColor::White;
        }

        return EpaperColor::Black;
    }

    EpaperColor background_for_score(uint8_t score) {
        if (score >= 4) {
            return EpaperColor::Red;
        }

        if (score >= 2) {
            return EpaperColor::Yellow;
        }

        return EpaperColor::White;
    }

    EpaperColor background_for_overall_score(uint8_t score) {
        if (score == 1) {
            return EpaperColor::Black;
        }

        return background_for_score(score);
    }

    EpaperColor background_for_metric(
        const MetricAssessment& metric
    ) {
        if (!metric.valid) {
            return EpaperColor::White;
        }

        return background_for_score(metric.score);
    }

    const char* label_for_score(uint8_t score) {
        if (score >= 4) {
            return "BAD";
        }

        if (score >= 2) {
            return "MODERATE";
        }

        return "GOOD";
    }

    void draw_metric_box(
        EpaperCanvas& canvas,
        int left,
        int right,
        int top,
        int bottom,
        const char* label,
        const char* value,
        const char* unit,
        int label_y,
        int label_scale,
        int value_y,
        int value_scale,
        int unit_y,
        int unit_scale,
        EpaperColor background
    ) {
        fill_metric_box(
            canvas,
            left,
            top,
            right,
            bottom,
            background
        );

        const EpaperColor foreground =
            text_color_for_background(background);

        draw_left_aligned_text(
            canvas,
            left + 4,
            label_y,
            label,
            foreground,
            label_scale
        );

        draw_centered_text(
            canvas,
            left,
            right,
            value_y,
            value,
            foreground,
            value_scale
        );

        draw_right_aligned_text(
            canvas,
            right - 3,
            unit_y,
            unit,
            foreground,
            unit_scale
        );
    }

    void draw_metric_tile(
        EpaperCanvas& canvas,
        int left,
        int right,
        int top,
        int bottom,
        const char* label,
        const char* value,
        const char* unit,
        int value_y,
        EpaperColor background
    ) {
        draw_metric_box(
            canvas,
            left,
            right,
            top,
            bottom,
            label,
            value,
            unit,
            top + 6,
            1,
            value_y,
            3,
            bottom - 9,
            1,
            background
        );
    }

    void draw_metric_panel(
        EpaperCanvas& canvas,
        int left,
        int right,
        int top,
        int bottom,
        const char* label,
        const char* value,
        const char* unit,
        int value_y,
        EpaperColor background
    ) {
        draw_metric_box(
            canvas,
            left,
            right,
            top,
            bottom,
            label,
            value,
            unit,
            top + 4,
            2,
            value_y,
            5,
            bottom - 9,
            1,
            background
        );
    }

}

void IAQDashboard::render(
    EpaperCanvas& canvas,
    const SensorReadings& readings,
    const AirQualityAssessment& assessment
) const {
    canvas.clear(EpaperColor::White);
    draw_outer_frame(canvas);

    char co2_value[8];
    char pm2_5_value[8];
    char pm10_value[8];
    char pm1_0_value[8];
    char temperature_value[8];
    char humidity_value[8];
    char voc_value[8];
    char nox_value[8];

    std::snprintf(
        co2_value,
        sizeof(co2_value),
        "%u",
        static_cast<unsigned>(readings.co2_ppm)
    );

    std::snprintf(
        pm2_5_value,
        sizeof(pm2_5_value),
        "%u",
        static_cast<unsigned>(readings.pm2_5_ug_m3)
    );

    std::snprintf(
        pm10_value,
        sizeof(pm10_value),
        "%u",
        static_cast<unsigned>(readings.pm10_ug_m3)
    );

    std::snprintf(
        pm1_0_value,
        sizeof(pm1_0_value),
        "%u",
        static_cast<unsigned>(readings.pm1_0_ug_m3)
    );

    const float temperature_f =
        readings.temperature_c * 9.0f / 5.0f + 32.0f;

    std::snprintf(
        temperature_value,
        sizeof(temperature_value),
        "%.0f",
        temperature_f
    );

    std::snprintf(
        humidity_value,
        sizeof(humidity_value),
        "%.0f",
        readings.humidity_percent
    );

    if (readings.voc_valid) {
        std::snprintf(
            voc_value,
            sizeof(voc_value),
            "%u",
            static_cast<unsigned>(readings.voc_index)
        );
    } else {
        std::snprintf(
            voc_value,
            sizeof(voc_value),
            "--"
        );
    }

    if (readings.nox_valid) {
        std::snprintf(
            nox_value,
            sizeof(nox_value),
            "%u",
            static_cast<unsigned>(readings.nox_index)
        );
    } else {
        std::snprintf(
            nox_value,
            sizeof(nox_value),
            "--"
        );
    }

    // Header: title, status badge, and five-step overall quality scale.
    draw_left_aligned_text(
        canvas,
        8,
        10,
        "AIR QUALITY",
        EpaperColor::Black,
        1
    );

    draw_outline(
        canvas,
        80,
        6,
        62,
        18,
        EpaperColor::Black
    );

    const EpaperColor status_background =
        background_for_overall_score(assessment.overall_score);

    const EpaperColor status_foreground =
        text_color_for_background(status_background);

    canvas.fill_rectangle(
        82,
        8,
        58,
        14,
        status_background
    );
    canvas.draw_text(
        87,
        11,
        label_for_score(assessment.overall_score),
        status_foreground,
        1
    );

    constexpr int SCALE_BOX_Y = 7;
    constexpr int SCALE_BOX_WIDTH = 22;
    constexpr int SCALE_BOX_HEIGHT = 10;
    constexpr int SCALE_STEP = 24;
    constexpr int SCALE_START_X = 145;

    for (int step = 0; step < 5; ++step) {
        const int x = SCALE_START_X + step * SCALE_STEP;

        draw_outline(
            canvas,
            x,
            SCALE_BOX_Y,
            SCALE_BOX_WIDTH,
            SCALE_BOX_HEIGHT,
            EpaperColor::Black
        );

        if (step + 1 == assessment.overall_score) {
            canvas.fill_rectangle(
                x + 2,
                SCALE_BOX_Y + 2,
                SCALE_BOX_WIDTH - 4,
                SCALE_BOX_HEIGHT - 4,
                status_background
            );
        }

        canvas.draw_text(
            x + 8,
            19,
            step == 0 ? "1" :
            step == 1 ? "2" :
            step == 2 ? "3" :
            step == 3 ? "4" : "5",
            EpaperColor::Black,
            1
        );
    }

    // Main-panel divider and bottom-row divider.
    canvas.fill_rectangle(
        FRAME_X + 1,
        HEADER_BOTTOM,
        FRAME_WIDTH - 2,
        2,
        EpaperColor::Black
    );
    canvas.fill_rectangle(
        MAIN_DIVIDER_X,
        HEADER_BOTTOM,
        2,
        MAIN_BOTTOM - HEADER_BOTTOM,
        EpaperColor::Black
    );
    canvas.fill_rectangle(
        MAIN_TEMP_DIVIDER_X,
        HEADER_BOTTOM,
        2,
        MAIN_BOTTOM - HEADER_BOTTOM,
        EpaperColor::Black
    );
    canvas.fill_rectangle(
        FRAME_X + 1,
        MAIN_BOTTOM,
        FRAME_WIDTH - 2,
        2,
        EpaperColor::Black
    );

    draw_metric_panel(
        canvas,
        2,
        147,
        HEADER_BOTTOM + 2,
        MAIN_BOTTOM - 1,
        "CO2",
        co2_value,
        "PPM",
        55,
        background_for_metric(assessment.co2)
    );

    draw_metric_panel(
        canvas,
        150,
        220,
        HEADER_BOTTOM + 2,
        MAIN_BOTTOM - 1,
        "TEMP",
        temperature_value,
        "F",
        55,
        EpaperColor::White
    );

    draw_metric_panel(
        canvas,
        224,
        293,
        HEADER_BOTTOM + 2,
        MAIN_BOTTOM - 1,
        "PM2.5",
        pm2_5_value,
        "UG/M3",
        55,
        background_for_metric(assessment.pm2_5)
    );

    // Bottom-row dividers.
    for (int divider = 0; divider < 4; ++divider) {
        canvas.fill_rectangle(
            TILE_DIVIDER_X[divider],
            MAIN_BOTTOM,
            2,
            TILE_BOTTOM - MAIN_BOTTOM + 1,
            EpaperColor::Black
        );
    }

    // Bottom tiles. The red/yellow colors are temporary visual tests.
    draw_metric_tile(
        canvas,
        TILE_LEFT[0],
        TILE_RIGHT[0],
        TILE_TOP,
        TILE_BOTTOM,
        "VOC",
        voc_value,
        "PPB",
        TILE_VALUE_Y,
        background_for_metric(assessment.voc)
    );

    draw_metric_tile(
        canvas,
        TILE_LEFT[1],
        TILE_RIGHT[1],
        TILE_TOP,
        TILE_BOTTOM,
        "NOX",
        nox_value,
        "PPB",
        TILE_VALUE_Y,
        background_for_metric(assessment.nox)
    );

    draw_metric_tile(
        canvas,
        TILE_LEFT[2],
        TILE_RIGHT[2],
        TILE_TOP,
        TILE_BOTTOM,
        "RH",
        humidity_value,
        "%",
        TILE_VALUE_Y,
        EpaperColor::White
    );

    draw_metric_tile(
        canvas,
        TILE_LEFT[3],
        TILE_RIGHT[3],
        TILE_TOP,
        TILE_BOTTOM,
        "PM1",
        pm1_0_value,
        "UG/M3",
        TILE_VALUE_Y,
        EpaperColor::White
    );

    draw_metric_tile(
        canvas,
        TILE_LEFT[4],
        TILE_RIGHT[4],
        TILE_TOP,
        TILE_BOTTOM,
        "PM10",
        pm10_value,
        "UG/M3",
        TILE_VALUE_Y,
        background_for_metric(assessment.pm10)
    );
}

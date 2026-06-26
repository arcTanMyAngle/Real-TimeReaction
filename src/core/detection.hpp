#pragma once
#include <opencv2/opencv.hpp>
#include <deque>
#include <vector>

struct DetectionResult {
    bool  movement_detected = false;
    float movement_area     = 0.0f;
    float threshold         = 0.0f;
    std::vector<std::vector<cv::Point>> contours; // raw; caller draws if desired
};

class DetectionEngine {
public:
    explicit DetectionEngine(float threshold = 1000.0f, int buffer_size = 3);

    DetectionResult update(const cv::Mat& bgr_frame);
    void set_threshold(float value) noexcept;
    void reset() noexcept;

    float threshold() const noexcept { return threshold_; }

private:
    float              threshold_;
    int                buffer_size_;
    std::deque<cv::Mat> prev_frames_;
};

#include "detection.hpp"

#include <opencv2/imgproc.hpp>

DetectionEngine::DetectionEngine(float threshold, int buffer_size)
    : threshold_(threshold), buffer_size_(buffer_size < 1 ? 1 : buffer_size) {}

void DetectionEngine::set_threshold(float value) noexcept {
    threshold_ = value;
}

void DetectionEngine::reset() noexcept {
    prev_frames_.clear();
}

DetectionResult DetectionEngine::update(const cv::Mat& bgr_frame) {
    DetectionResult result;
    result.threshold = threshold_;

    if (bgr_frame.empty())
        return result;

    // 1. grayscale → blur (the comparison representation)
    cv::Mat gray;
    cv::cvtColor(bgr_frame, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, cv::Size(21, 21), 0.0);

    // Need at least one prior frame to diff against.
    if (prev_frames_.empty()) {
        prev_frames_.push_back(gray);
        return result;
    }

    // 2. absdiff against the oldest buffered frame (smooths transient noise)
    cv::Mat diff;
    cv::absdiff(prev_frames_.front(), gray, diff);

    // 3. threshold → 4. dilate
    cv::Mat thresh;
    cv::threshold(diff, thresh, 25.0, 255.0, cv::THRESH_BINARY);
    cv::dilate(thresh, thresh, cv::Mat(), cv::Point(-1, -1), 2);

    // 5. findContours (caller may render these; engine never draws)
    cv::findContours(thresh, result.contours, cv::RETR_EXTERNAL,
                     cv::CHAIN_APPROX_SIMPLE);

    double total_area = 0.0;
    for (const auto& c : result.contours)
        total_area += cv::contourArea(c);

    result.movement_area     = static_cast<float>(total_area);
    result.movement_detected = result.movement_area >= threshold_;

    // Advance the rolling buffer.
    prev_frames_.push_back(gray);
    while (static_cast<int>(prev_frames_.size()) > buffer_size_)
        prev_frames_.pop_front();

    return result;
}

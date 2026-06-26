#pragma once
#include <GL/glew.h>
#include <opencv2/opencv.hpp>

class CameraTexture {
public:
    CameraTexture(int width, int height, int camera_index = 0);
    ~CameraTexture();

    bool open();
    void update();   // call once per frame; uploads latest frame to GL
    void close();

    GLuint texture_id()  const noexcept { return tex_id_; }
    bool   is_open()     const noexcept { return cap_.isOpened(); }
    int    width()       const noexcept { return width_; }
    int    height()      const noexcept { return height_; }

    // Latest frame for DetectionEngine (BGR)
    const cv::Mat& latest_frame() const noexcept { return latest_bgr_; }

private:
    int           width_, height_, cam_index_;
    cv::VideoCapture cap_;
    cv::Mat          latest_bgr_;
    GLuint           tex_id_ = 0;

    void init_texture();
};

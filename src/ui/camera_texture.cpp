#include "camera_texture.hpp"

#include <opencv2/imgproc.hpp>

CameraTexture::CameraTexture(int width, int height, int camera_index)
    : width_(width), height_(height), cam_index_(camera_index) {}

CameraTexture::~CameraTexture() {
    close();
}

void CameraTexture::init_texture() {
    if (tex_id_ == 0)
        glGenTextures(1, &tex_id_);
    glBindTexture(GL_TEXTURE_2D, tex_id_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Seed with a 1x1 black pixel so the panel renders black until (and if) a
    // camera frame arrives — no camera must never mean a crash.
    const unsigned char black[3] = {0, 0, 0};
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, black);
    glBindTexture(GL_TEXTURE_2D, 0);
}

bool CameraTexture::open() {
    init_texture();  // requires a current GL context (created by Renderer)

    cap_.open(cam_index_);
    if (cap_.isOpened()) {
        cap_.set(cv::CAP_PROP_FRAME_WIDTH, width_);
        cap_.set(cv::CAP_PROP_FRAME_HEIGHT, height_);
    }
    return cap_.isOpened();
}

void CameraTexture::update() {
    if (!cap_.isOpened())
        return;  // texture remains the 1x1 black panel

    cap_ >> latest_bgr_;
    if (latest_bgr_.empty())
        return;

    cv::Mat rgb;
    cv::cvtColor(latest_bgr_, rgb, cv::COLOR_BGR2RGB);
    if (rgb.cols != width_ || rgb.rows != height_)
        cv::resize(rgb, rgb, cv::Size(width_, height_));

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, tex_id_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width_, height_, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, rgb.data);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void CameraTexture::close() {
    if (cap_.isOpened())
        cap_.release();
    if (tex_id_ != 0) {
        glDeleteTextures(1, &tex_id_);
        tex_id_ = 0;
    }
}

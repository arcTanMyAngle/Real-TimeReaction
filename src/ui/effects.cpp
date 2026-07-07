#include "effects.hpp"

#include <algorithm>
#include <cmath>

// ── ScreenFlash ──────────────────────────────────────────────────────────────
void ScreenFlash::trigger(ImVec4 c) {
    color = c;
    timer = duration;
}

void ScreenFlash::update(float dt) {
    if (timer > 0.0f) timer -= dt;
}

void ScreenFlash::render(int w, int h) {
    if (timer <= 0.0f) return;
    const float frac  = std::clamp(timer / duration, 0.0f, 1.0f);
    const float alpha = frac * max_alpha;
    ImVec4 c = color;
    c.w = alpha;
    ImGui::GetForegroundDrawList()->AddRectFilled(
        ImVec2(0, 0), ImVec2(static_cast<float>(w), static_cast<float>(h)),
        ImGui::GetColorU32(c));
}

// ── PerformanceLabel ─────────────────────────────────────────────────────────
const char* PerformanceLabel::text_for(float rt_ms, bool false_start) {
    if (false_start)    return "EARLY!";
    if (rt_ms < 0.0f)   return "TOO SLOW";
    if (rt_ms < 150.0f) return "GODLIKE";
    if (rt_ms < 200.0f) return "ELITE";
    if (rt_ms < 250.0f) return "FAST";
    if (rt_ms < 300.0f) return "GOOD";
    if (rt_ms < 400.0f) return "AVERAGE";
    return "SLOW";
}

ImVec4 PerformanceLabel::color_for(float rt_ms, bool false_start) {
    if (false_start)    return Colors::WARNING;
    if (rt_ms < 0.0f)   return Colors::DANGER;
    if (rt_ms < 150.0f) return ImVec4(Colors::SUCCESS.x, Colors::SUCCESS.y,
                                      Colors::SUCCESS.z, 1.0f);  // full bright
    if (rt_ms < 200.0f) return Colors::SUCCESS;
    if (rt_ms < 250.0f) return Colors::ACCENT;
    if (rt_ms < 300.0f) return Colors::WARNING;
    if (rt_ms < 400.0f) return Colors::MUTED;
    return Colors::DANGER;
}

void PerformanceLabel::trigger(float rt_ms, ImVec2 pos, bool is_false_start) {
    text   = text_for(rt_ms, is_false_start);
    color  = color_for(rt_ms, is_false_start);
    origin = pos;
    timer  = duration;
}

void PerformanceLabel::update(float dt) {
    if (timer > 0.0f) timer -= dt;
}

void PerformanceLabel::render() {
    if (!is_active()) return;

    float progress = 1.0f - (timer / duration);        // 0→1 over lifetime
    float rise     = progress * 80.0f;                 // rises 80px
    float alpha    = (progress < 0.65f) ? 1.0f
                   : 1.0f - (progress - 0.65f) / 0.35f; // fade last 35%

    ImVec4 col = color;
    col.w *= alpha;

    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::Begin("##perf_label", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    float scale = 2.4f - progress * 0.6f;  // 2.4x → 1.8x over lifetime
    ImGui::SetWindowFontScale(scale);
    ImGui::PushStyleColor(ImGuiCol_Text, col);

    // Position: origin.x centered, origin.y - rise
    ImVec2 text_size = ImGui::CalcTextSize(text.c_str());
    ImGui::SetCursorPos({origin.x - text_size.x * 0.5f,
                         origin.y - rise - text_size.y * 0.5f});
    ImGui::TextUnformatted(text.c_str());

    ImGui::PopStyleColor();
    ImGui::SetWindowFontScale(1.0f);
    ImGui::End();
}

// ── NewRecordFlash ───────────────────────────────────────────────────────────
void NewRecordFlash::update(float dt) {
    if (timer > 0.0f) timer -= dt;
}

void NewRecordFlash::render(ImVec2 center_pos) {
    if (timer <= 0.0f) return;

    // Pulse the scale and fade out over the final stretch.
    const float pulse = (std::sin(timer * 8.0f) + 1.0f) * 0.5f;  // 0..1
    const float scale = 1.0f + pulse * 0.25f;
    const float alpha = std::clamp(timer / 0.6f, 0.0f, 1.0f);

    const char* msg = "* NEW RECORD *";
    ImVec4 gold(1.0f, 0.84f, 0.0f, alpha);

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImFont* font   = ImGui::GetFont();
    const float fs = font->FontSize * 1.8f * scale;
    const ImVec2 tsz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, msg);
    dl->AddText(font, fs,
                ImVec2(center_pos.x - tsz.x * 0.5f, center_pos.y - tsz.y * 0.5f),
                ImGui::GetColorU32(gold), msg);
}

// ── ScanlineOverlay ──────────────────────────────────────────────────────────
void ScanlineOverlay::render(int w, int h) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImU32 line_col = IM_COL32(0, 0, 0, static_cast<int>(opacity * 255));
    for (int y = 0; y < h; y += spacing)
        dl->AddLine({0.0f, static_cast<float>(y)},
                    {static_cast<float>(w), static_cast<float>(y)}, line_col);
}

// ── CameraBorderPulse ────────────────────────────────────────────────────────
void CameraBorderPulse::update(float dt) {
    phase += dt * 3.0f;  // ~0.5 Hz breathing
    if (phase > 2.0f * 3.14159265358979323846f)
        phase -= 2.0f * 3.14159265358979323846f;
}

ImVec4 CameraBorderPulse::border_color(SessionState state, int player) const {
    ImVec4 base = (player == 1) ? Colors::P1 : Colors::P2;
    if (state == SessionState::StimulusActive) {
        base.w = 1.0f;  // solid bright on stimulus
        return base;
    }
    if (state == SessionState::Waiting) {
        // breathe: 0.35 → 0.95
        float pulse = (std::sin(phase) + 1.0f) * 0.5f;
        base.w = 0.35f + pulse * 0.60f;
        return base;
    }
    base.w = 0.4f;
    return base;
}

float CameraBorderPulse::border_thickness(SessionState state) const {
    return (state == SessionState::StimulusActive) ? 4.0f : 2.0f;
}

// ── GameEffects ──────────────────────────────────────────────────────────────
void GameEffects::update(float dt) {
    flash.update(dt);
    label.update(dt);
    record.update(dt);
    border.update(dt);
}

void GameEffects::render_foreground(int w, int h) {
    flash.render(w, h);
    scanlines.render(w, h);
    record.render(ImVec2(w * 0.5f, h * 0.28f));
}

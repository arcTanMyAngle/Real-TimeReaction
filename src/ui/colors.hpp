#pragma once
#include "imgui.h"

namespace Colors {
    inline constexpr ImVec4 BG      {0.059f, 0.059f, 0.078f, 1.0f}; // #0F0F14
    inline constexpr ImVec4 PANEL   {0.110f, 0.110f, 0.149f, 1.0f}; // #1C1C26
    inline constexpr ImVec4 ACCENT  {0.369f, 0.918f, 0.824f, 1.0f}; // #5EEACC
    inline constexpr ImVec4 P1      {0.388f, 0.400f, 0.945f, 1.0f}; // #6366F1
    inline constexpr ImVec4 P2      {0.976f, 0.451f, 0.086f, 1.0f}; // #F97316
    inline constexpr ImVec4 TEXT    {0.945f, 0.957f, 0.976f, 1.0f}; // #F1F5F9
    inline constexpr ImVec4 MUTED   {0.392f, 0.455f, 0.545f, 1.0f}; // #64748B
    inline constexpr ImVec4 SUCCESS {0.133f, 0.773f, 0.369f, 1.0f}; // #22C55E
    inline constexpr ImVec4 WARNING {0.918f, 0.702f, 0.031f, 1.0f}; // #EAB308
    inline constexpr ImVec4 DANGER  {0.937f, 0.267f, 0.267f, 1.0f}; // #EF4444
}

namespace Display {
    constexpr int   W          = 1280;
    constexpr int   H          = 720;
    constexpr int   CAMERA_W   = 640;
    constexpr int   CAMERA_H   = 480;
    constexpr float TARGET_FPS = 60.0f;
}
